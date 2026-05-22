#ifndef DNSServer_h
#define DNSServer_h

#include <memory>
#include <WiFiUdp.h>
#include <pgmspace.h>

#ifndef IANA_DNS_PORT
#define IANA_DNS_PORT 53
#endif

#define DNS_QR_QUERY    0
#define DNS_QR_RESPONSE 1
#define DNS_OPCODE_QUERY 0

#define DNS_QCLASS_IN  1
#define DNS_QCLASS_ANY 255

#define DNS_QTYPE_A   1
#define DNS_QTYPE_ANY 255

#define MAX_DNSNAME_LENGTH  253
#define MAX_DNS_PACKETSIZE  512

enum class DNSReplyCode
{
  NoError          = 0,
  FormError        = 1,
  ServerFailure    = 2,
  NonExistentDomain = 3,
  NotImplemented   = 4,
  Refused          = 5,
  YXDomain         = 6,
  YXRRSet          = 7,
  NXRRSet          = 8
};

struct DNSHeader
{
  uint16_t ID;
  unsigned char RD     : 1;
  unsigned char TC     : 1;
  unsigned char AA     : 1;
  unsigned char OPCode : 4;
  unsigned char QR     : 1;
  unsigned char RCode  : 4;
  unsigned char Z      : 3;
  unsigned char RA     : 1;
  uint16_t QDCount;
  uint16_t ANCount;
  uint16_t NSCount;
  uint16_t ARCount;
};

constexpr inline size_t kDNSSQueSizeAddrBits = 3;
constexpr inline size_t kDNSSQueSize = BIT(kDNSSQueSizeAddrBits);

struct DNSS_REQUESTER {
  uint32_t ip;
  uint16_t port;
  uint16_t id;
};

class DNSServer
{
public:
  DNSServer();
  ~DNSServer() { stop(); }

  bool enableForwarder(const String &domainName = emptyString, const IPAddress &dns = (uint32_t)0);
  void disableForwarder(const String &domainName = emptyString, bool freeResources = false);
  bool isForwarding() { return _forwarder && _dns.isSet(); }
  void setDNS(const IPAddress& dns) { _dns = dns; }
  IPAddress getDNS() { return _dns; }
  bool isDNSSet() { return _dns.isSet(); }

  void processNextRequest();
  void setErrorReplyCode(const DNSReplyCode &replyCode);
  void setTTL(const uint32_t &ttl);
  uint32_t getTTL();
  String getDomainName() { return _domainName; }

  bool start(const uint16_t &port,
             const String   &domainName,
             const IPAddress &resolvedIP,
             const IPAddress &dns = (uint32_t)0);
  void stop();

  /// Pass a PROGMEM array of domain substrings that should be blocked.
  /// Queries whose decoded name contains any entry are answered with 0.0.0.0.
  void setBlockedDomains(const char* const* domains, size_t count);

private:
  WiFiUDP  _udp;
  String   _domainName;
  IPAddress _dns;
  std::unique_ptr<DNSS_REQUESTER[]> _que;
  uint32_t _ttl;
  DNSReplyCode _errorReplyCode;
  bool     _forwarder;
  unsigned char _resolvedIP[4];
  uint16_t _port;

  // Blocklist
  const char* const* _blockedDomains;
  size_t             _blockedCount;

  void   downcaseAndRemoveWwwPrefix(String &domainName);
  String extractDomain(uint8_t *query);
  bool   isBlocked(const String &domain);
  void   replyWithIP(DNSHeader *dnsHeader, unsigned char *query, size_t queryLength);
  void   replyWithError(DNSHeader *dnsHeader, DNSReplyCode rcode, unsigned char *query, size_t queryLength);
  void   replyWithError(DNSHeader *dnsHeader, DNSReplyCode rcode);
  bool   respondToRequest(uint8_t *buffer, size_t length);
  void   forwardRequest(uint8_t *buffer, size_t length);
  void   forwardReply(uint8_t *buffer, size_t length);
  void   writeNBOShort(uint16_t value);
};

#endif // DNSServer_h

