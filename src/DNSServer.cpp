// DNSServer implementation — adapted from ESP8266 Arduino core.
// Adds PROGMEM blocklist: matching queries are answered with 0.0.0.0
// instead of the captive-portal IP.

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  extern struct rst_info resetInfo;
  static uint32_t _ids __attribute__((section(".noinit")));
#else
  #include <WiFi.h>
  static uint32_t _ids = 0;
#endif

#include "DNSServer.h"
#include <lwip/def.h>
#include <Arduino.h>

#define DNS_HEADER_SIZE sizeof(DNSHeader)

DNSServer::DNSServer()
  : _forwarder(false)
  , _blockedDomains(nullptr)
  , _blockedCount(0)
{
  _ttl = lwip_htonl(60);
  _errorReplyCode = DNSReplyCode::NonExistentDomain;

#ifdef ESP8266
  if (REASON_DEFAULT_RST      == resetInfo.reason ||
      REASON_DEEP_SLEEP_AWAKE <= resetInfo.reason) {
    _ids = random(0, BIT(16) - 1);
  }
  _ids += kDNSSQueSize;
#endif
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void DNSServer::setBlockedDomains(const char* const* domains, size_t count)
{
  _blockedDomains = domains;
  _blockedCount   = count;
}

bool DNSServer::start(const uint16_t &port, const String &domainName,
                      const IPAddress &resolvedIP, const IPAddress &dns)
{
  _port = port ? port : IANA_DNS_PORT;

  _resolvedIP[0] = resolvedIP[0];
  _resolvedIP[1] = resolvedIP[1];
  _resolvedIP[2] = resolvedIP[2];
  _resolvedIP[3] = resolvedIP[3];

  if (!enableForwarder(domainName, dns) && (dns.isSet() || _dns.isSet()))
    return false;

  return _udp.begin(_port) == 1;
}

void DNSServer::stop()
{
  _udp.stop();
  disableForwarder(emptyString, true);
}

void DNSServer::setErrorReplyCode(const DNSReplyCode &replyCode) { _errorReplyCode = replyCode; }
void DNSServer::setTTL(const uint32_t &ttl)                      { _ttl = lwip_htonl(ttl); }
uint32_t DNSServer::getTTL()                                      { return lwip_ntohl(_ttl); }

void DNSServer::disableForwarder(const String &domainName, bool freeResources)
{
  _forwarder = false;
  if (!domainName.isEmpty()) {
    _domainName = domainName;
    downcaseAndRemoveWwwPrefix(_domainName);
  }
  if (freeResources) {
    _dns = (uint32_t)0;
    _que = nullptr;
  }
}

bool DNSServer::enableForwarder(const String &domainName, const IPAddress &dns)
{
  disableForwarder(domainName, false);
  if (dns.isSet()) _dns = dns;
  if (_dns.isSet()) {
    if (!_que)
      _que = std::unique_ptr<DNSS_REQUESTER[]>(new (std::nothrow) DNSS_REQUESTER[kDNSSQueSize]);
    if (_que) {
      for (size_t i = 0; i < kDNSSQueSize; i++) _que[i].ip = 0;
      _forwarder = true;
    }
  }
  return _forwarder;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void DNSServer::downcaseAndRemoveWwwPrefix(String &domainName)
{
  domainName.toLowerCase();
  if (domainName.startsWith("www."))
    domainName.remove(0, 4);
}

/// Decode the DNS label-encoded name starting at query into a dotted String.
String DNSServer::extractDomain(uint8_t *query)
{
  String domain;
  uint8_t *p = query;
  while (*p != 0) {
    uint8_t len = *p++;
    if (domain.length() > 0) domain += '.';
    for (uint8_t i = 0; i < len; i++)
      domain += (char)tolower(*p++);
  }
  return domain;
}

/// Return true if domain contains any entry in the PROGMEM blocklist.
bool DNSServer::isBlocked(const String &domain)
{
  if (!_blockedDomains || _blockedCount == 0) return false;
  char buf[64];
  for (size_t i = 0; i < _blockedCount; i++) {
    const char *entry = (const char*)pgm_read_ptr(&_blockedDomains[i]);
    strncpy_P(buf, entry, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (domain.indexOf(buf) > -1)
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Core request/response logic
// ---------------------------------------------------------------------------

bool DNSServer::respondToRequest(uint8_t *buffer, size_t length)
{
  DNSHeader *dnsHeader = (DNSHeader *)buffer;

  if (dnsHeader->QR != DNS_QR_QUERY)                   return false;
  if (dnsHeader->OPCode != DNS_OPCODE_QUERY) {
    replyWithError(dnsHeader, DNSReplyCode::NotImplemented); return false;
  }
  if (dnsHeader->QDCount != lwip_htons(1)) {
    replyWithError(dnsHeader, DNSReplyCode::FormError);      return false;
  }
  if (dnsHeader->ANCount != 0 || dnsHeader->NSCount != 0 || dnsHeader->ARCount != 0) {
    replyWithError(dnsHeader, DNSReplyCode::FormError);      return false;
  }

  uint8_t *query = buffer + DNS_HEADER_SIZE;
  uint8_t *start = query;
  size_t remaining = length - DNS_HEADER_SIZE;

  while (remaining != 0 && *start != 0) {
    size_t labelLength = *start;
    if (labelLength + 1 > remaining) {
      replyWithError(dnsHeader, DNSReplyCode::FormError); return false;
    }
    remaining -= (labelLength + 1);
    start     += (labelLength + 1);
  }

  if (remaining < 5) {
    replyWithError(dnsHeader, DNSReplyCode::FormError); return false;
  }

  start += 1; // skip root zero label
  uint16_t qtype, qclass;
  memcpy(&qtype,  start, sizeof(qtype));  start += 2;
  memcpy(&qclass, start, sizeof(qclass)); start += 2;
  size_t queryLength = start - query;

  if (qclass != lwip_htons(DNS_QCLASS_ANY) && qclass != lwip_htons(DNS_QCLASS_IN)) {
    replyWithError(dnsHeader, DNSReplyCode::NonExistentDomain, query, queryLength); return false;
  }
  if (qtype != lwip_htons(DNS_QTYPE_A) && qtype != lwip_htons(DNS_QTYPE_ANY)) {
    replyWithError(dnsHeader, DNSReplyCode::NonExistentDomain, query, queryLength); return false;
  }

  if (_domainName.isEmpty()) {
    if (_forwarder) return true;
    replyWithError(dnsHeader, _errorReplyCode, query, queryLength);
    return false;
  }

  // Wildcard branch — check blocklist before replying
  if (_domainName == "*") {
    if (_blockedDomains && _blockedCount > 0) {
      String requested = extractDomain(query);
      if (isBlocked(requested)) {
        // Reply with 0.0.0.0 for blocked domains
        unsigned char saved[4];
        memcpy(saved, _resolvedIP, 4);
        memset(_resolvedIP, 0, 4);
        replyWithIP(dnsHeader, query, queryLength);
        memcpy(_resolvedIP, saved, 4);
        return false;
      }
    }
    replyWithIP(dnsHeader, query, queryLength);
    return false;
  }

  // Specific domain matching
  const char *matchString = _domainName.c_str();
  start = query;
  if (*start == 3 && strncasecmp("www", (char *)start + 1, 3) == 0)
    start += 4;

  while (*start != 0) {
    size_t labelLength = *start++;
    while (labelLength > 0) {
      if (tolower(*start) != *matchString) {
        if (_forwarder) return true;
        replyWithError(dnsHeader, _errorReplyCode, query, queryLength);
        return false;
      }
      ++start; ++matchString; --labelLength;
    }
    if (*start == 0 && *matchString == '\0') {
      replyWithIP(dnsHeader, query, queryLength);
      return false;
    }
    if (*matchString != '.') {
      replyWithError(dnsHeader, _errorReplyCode, query, queryLength);
      return false;
    }
    ++matchString;
  }
  replyWithError(dnsHeader, _errorReplyCode, query, queryLength);
  return false;
}

void DNSServer::processNextRequest()
{
  size_t currentPacketSize = _udp.parsePacket();
  if (currentPacketSize == 0 ||
      currentPacketSize > MAX_DNS_PACKETSIZE ||
      currentPacketSize < DNS_HEADER_SIZE) return;

  std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[currentPacketSize]);
  if (!buffer) return;

  _udp.read(buffer.get(), currentPacketSize);
  if (_dns.isSet() && _udp.remoteIP() == _dns)
    forwardReply(buffer.get(), currentPacketSize);
  else if (respondToRequest(buffer.get(), currentPacketSize))
    forwardRequest(buffer.get(), currentPacketSize);
}

void DNSServer::writeNBOShort(uint16_t value)
{
  _udp.write((unsigned char *)&value, 2);
}

void DNSServer::replyWithIP(DNSHeader *dnsHeader, unsigned char *query, size_t queryLength)
{
  dnsHeader->QR      = DNS_QR_RESPONSE;
  dnsHeader->QDCount = lwip_htons(1);
  dnsHeader->ANCount = lwip_htons(1);
  dnsHeader->NSCount = 0;
  dnsHeader->ARCount = 0;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write((unsigned char *)dnsHeader, sizeof(DNSHeader));
  _udp.write(query, queryLength);

  uint16_t ptr = 0xC000 | DNS_HEADER_SIZE;
  writeNBOShort(lwip_htons(ptr));
  writeNBOShort(lwip_htons(DNS_QTYPE_A));
  writeNBOShort(lwip_htons(DNS_QCLASS_IN));
  _udp.write((unsigned char *)&_ttl, 4);
  writeNBOShort(lwip_htons(sizeof(_resolvedIP)));
  _udp.write(_resolvedIP, sizeof(_resolvedIP));
  _udp.endPacket();
}

void DNSServer::replyWithError(DNSHeader *dnsHeader, DNSReplyCode rcode,
                               unsigned char *query, size_t queryLength)
{
  dnsHeader->QR      = DNS_QR_RESPONSE;
  dnsHeader->RCode   = (unsigned char)rcode;
  dnsHeader->QDCount = query ? lwip_htons(1) : 0;
  dnsHeader->ANCount = 0;
  dnsHeader->NSCount = 0;
  dnsHeader->ARCount = 0;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write((unsigned char *)dnsHeader, sizeof(DNSHeader));
  if (query) _udp.write(query, queryLength);
  _udp.endPacket();
}

void DNSServer::replyWithError(DNSHeader *dnsHeader, DNSReplyCode rcode)
{
  replyWithError(dnsHeader, rcode, nullptr, 0);
}

void DNSServer::forwardRequest(uint8_t *buffer, size_t length)
{
  if (!_forwarder || !_dns.isSet() || !_que) return;
  DNSHeader *dnsHeader = (DNSHeader *)buffer;
  ++_ids;
  size_t i = _ids & (kDNSSQueSize - 1);
  _que[i].ip   = _udp.remoteIP();
  _que[i].port = _udp.remotePort();
  _que[i].id   = dnsHeader->ID;
  dnsHeader->ID = (uint16_t)_ids;
  _udp.beginPacket(_dns, IANA_DNS_PORT);
  _udp.write(buffer, length);
  _udp.endPacket();
}

void DNSServer::forwardReply(uint8_t *buffer, size_t length)
{
  if (!_forwarder || !_que) return;
  DNSHeader *dnsHeader = (DNSHeader *)buffer;
  uint16_t id = dnsHeader->ID;
  if ((uint16_t)kDNSSQueSize <= (uint16_t)_ids - id) return;
  size_t i = id & (kDNSSQueSize - 1);
  if (0 == _que[i].ip) return;
  dnsHeader->ID = _que[i].id;
  _udp.beginPacket(_que[i].ip, _que[i].port);
  _udp.write(buffer, length);
  _udp.endPacket();
  _que[i].ip = 0;
}

