#include "BatteryService.h"
#include <EEPROM.h>

BatteryService::BatteryService()
  : _adcPin(0)
  , _msInterval(0)
  , _lastUpdateMs(0)
  , _cachedRawLevel(0)
  , _level0(0)
  , _level25(0)
  , _level50(0)
  , _level75(0)
  , _level100(0)
  , _fsmCount(0)
  , _lowBatteryEvent(0)
{
  for (uint8_t i = 0; i < BATTERY_MAX_FSM_LISTENERS; i++) {
    _fsms[i] = nullptr;
  }
}

void BatteryService::init(uint8_t adcPin, uint32_t msInterval,
                          IFsmDispatcher** fsms, uint8_t fsmCount,
                          uint16_t lowBatteryEvent)
{
  _adcPin          = adcPin;
  _msInterval      = msInterval;
  _lowBatteryEvent = lowBatteryEvent;
  _lastUpdateMs    = millis();

  // Copy FSM pointers up to the array maximum
  _fsmCount = (fsmCount < BATTERY_MAX_FSM_LISTENERS) ? fsmCount : BATTERY_MAX_FSM_LISTENERS;
  for (uint8_t i = 0; i < _fsmCount; i++) {
    _fsms[i] = fsms[i];
  }

  // Configure ADC pin as input
  pinMode(_adcPin, INPUT);

  // Perform first read immediately
  _cachedRawLevel = analogRead(_adcPin);

  // Load calibration values from EEPROM
  updateLevelRanges();
}

void BatteryService::updateLevelRanges()
{
  // Read battery calibration points from EEPROM
  // EEPROM layout: addr 0=HW version (float), addr 4-20=B0-B4 (int values)
  // B0 @ addr 4 (0%)  B1 @ addr 8 (25%)  B2 @ addr 12 (50%)
  // B3 @ addr 16 (75%)  B4 @ addr 20 (100%)

  int addr_b0 = sizeof(float) + 0 * sizeof(int);  // addr 4
  int addr_b1 = sizeof(float) + 1 * sizeof(int);  // addr 8
  int addr_b2 = sizeof(float) + 2 * sizeof(int);  // addr 12
  int addr_b3 = sizeof(float) + 3 * sizeof(int);  // addr 16
  int addr_b4 = sizeof(float) + 4 * sizeof(int);  // addr 20

  EEPROM.begin(addr_b4 + sizeof(int));

  uint16_t val0 = 0, val25 = 0, val50 = 0, val75 = 0, val100 = 0;
  EEPROM.get(addr_b0, val0);
  EEPROM.get(addr_b1, val25);
  EEPROM.get(addr_b2, val50);
  EEPROM.get(addr_b3, val75);
  EEPROM.get(addr_b4, val100);

  EEPROM.end();

  // Only update if we have valid, non-zero calibration points
  // (EEPROM defaults to 0xFF, so uninitialized values will be 0xFFFF)
  if (val0 > 0 && val25 > 0 && val50 > 0 && val75 > 0 && val100 > 0) {
    _level0 = val0;
    _level25 = val25;
    _level50 = val50;
    _level75 = val75;
    _level100 = val100;

    Serial.print(F("[BAT] Calibration loaded from EEPROM: "));
    Serial.print(val0); Serial.print(F(" "));
    Serial.print(val25); Serial.print(F(" "));
    Serial.print(val50); Serial.print(F(" "));
    Serial.print(val75); Serial.print(F(" "));
    Serial.println(val100);
  }
}

void BatteryService::update()
{
  unsigned long now = millis();
  if (now - _lastUpdateMs >= _msInterval) {
    _lastUpdateMs   = now;
    _cachedRawLevel = analogRead(_adcPin);

    // Dispatch low-battery event to all registered FSMs on every poll cycle at 0%.
    if (_fsmCount > 0 && _lowBatteryEvent > 0 && currentLevel() == 0) {
      for (uint8_t i = 0; i < _fsmCount; i++) {
        _fsms[i]->dispatch(_lowBatteryEvent);
      }
    }
  }
}

uint16_t BatteryService::currentLevelRaw() const
{
  return _cachedRawLevel;
}


int BatteryService::currentLevel() const
{
  uint16_t raw = _cachedRawLevel;

  // Clamp raw value within the full range
  if (raw <= _level0) {
    return 0;
  }
  if (raw >= _level100) {
    return 100;
  }

  // Find which range the raw value falls into and interpolate
  if (raw <= _level25) {
    // Between 0% and 25%
    return map(raw, _level0, _level25, 0, 25);
  }
  if (raw <= _level50) {
    // Between 25% and 50%
    return map(raw, _level25, _level50, 25, 50);
  }
  if (raw <= _level75) {
    // Between 50% and 75%
    return map(raw, _level50, _level75, 50, 75);
  }

  // Between 75% and 100%
  return map(raw, _level75, _level100, 75, 100);
}

