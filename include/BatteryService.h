#ifndef BATTERY_SERVICE_H
#define BATTERY_SERVICE_H

#include <Arduino.h>
#include "HFSMEvents.h"

/// Maximum number of FSMs that can be registered for battery event dispatch.
#define BATTERY_MAX_FSM_LISTENERS 4

/// BatteryService manages battery level monitoring via ADC reading and
/// percentage calculation with configurable calibration points.
///
/// Usage:
///   BatteryService battery;
///   IFsmDispatcher* fsms[] = { &myFSM };
///   battery.init(A0, 250, fsms, 1, EVT_CORE_LOW_BATTERY);
///   // In loop():
///   battery.update();
///   int pct = battery.currentLevel();    // 0-100%
///   int raw = battery.currentLevelRaw(); // 0-1024
class BatteryService {
public:
  BatteryService();

  /// Initialize the battery service.
  /// This call configures the ADC pin as INPUT, performs the first read,
  /// and loads calibration points from EEPROM (addresses 4-20).
  /// @param adcPin          The ADC pin to read (e.g., BATTERY_PIN, A0, 34)
  /// @param msInterval      Polling interval in milliseconds (e.g., 250)
  /// @param fsms            Array of FSM dispatcher pointers to notify (optional)
  /// @param fsmCount        Number of entries in fsms[] (optional)
  /// @param lowBatteryEvent Event ID dispatched to all FSMs when level hits 0% (optional)
  void init(uint8_t adcPin, uint32_t msInterval,
            IFsmDispatcher** fsms = nullptr, uint8_t fsmCount = 0,
            uint16_t lowBatteryEvent = 0);

  /// Update calibration ranges from EEPROM.
  /// Reads the five calibration points (B0-B4) from EEPROM addresses 4-20.
  /// EEPROM layout: addr 0=HW version (float), addr 4-20=B0-B4 (int values).
  void updateLevelRanges();

  /// Poll for battery level updates.
  /// Call from loop() frequently. Internally checks msInterval before reading ADC.
  void update();

  /// Get the current cached raw ADC level (0-1024).
  /// Does not perform an ADC read — uses cached value from last update().
  uint16_t currentLevelRaw() const;

  /// Get the current battery level as a percentage (0-100).
  /// Calculated via linear interpolation between calibration points.
  /// Does not perform an ADC read — uses cached value from last update().
  int currentLevel() const;

private:
  uint8_t   _adcPin;
  uint32_t  _msInterval;
  uint32_t  _lastUpdateMs;
  uint16_t  _cachedRawLevel;

  // Calibration points for percentage calculation
  uint16_t _level0;
  uint16_t _level25;
  uint16_t _level50;
  uint16_t _level75;
  uint16_t _level100;

  // FSM event dispatch on low battery
  IFsmDispatcher* _fsms[BATTERY_MAX_FSM_LISTENERS];
  uint8_t         _fsmCount;
  uint16_t        _lowBatteryEvent;

};

#endif // BATTERY_SERVICE_H

