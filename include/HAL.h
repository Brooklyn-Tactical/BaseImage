#ifndef UNTITLED_HAL_H
#define UNTITLED_HAL_H


// ----------- FIRMWARE REVISION -----------
# define FIRMWARE_VERSION_MAJOR 2
# define FIRMWARE_VERSION_MINOR 2




// Conditional compilation for pin definitions
// ---------------------------------------------------------------------------------------------------------------------
#ifdef ESP32
    #define BATTERY_PIN  34
#elif defined(ESP8266)
    #define BATTERY_PIN  A0 // The WeMos D1 Mini has a built-in voltage divider for the battery on A0 (0v-3.3v)
#endif


// --------------------------------------------------- //
// Arm/Disarm configuration
// ARM_DISARM_SECONDS is now a runtime-configurable member of GameFSM (_armDisarmSeconds, default 10, range 1-99).
#define ARM_DISARM_TONE_LOW   1700  // Hz — starting tone for arming / ending tone for disarming
#define ARM_DISARM_TONE_HIGH  3000  // Hz — ending tone for arming / starting tone for disarming

// Input pins
// =================================================== //
// HARDWARE PIN CONFIGURATION (do no change)
#define PIN_TMR_CLK         16           // LED Timer clock
#define PIN_TMR_DATA        14           // LED Timer data
#define PIN_ARMING          12           // Arm/Disarm pin
#define PIN_BATTERY_SENSE   BATTERY_PIN  // Battery voltage sense pin
#define PIN_PIEZO_A         4            // Piezo H-Bridge pin
#define PIN_PIEZO_B         0            // Piezo H-Bridge pin
#define PIN_LIGHT           5            // LED strip pin

// for existing BaseImg code
#define PIN_MODE_SELECT PIN_ARMING


// Universal pin definitions
// ---------------------------------------------------------------------------------------------------------------------
#ifdef LED_BUILTIN
    #define LED_PIN  LED_BUILTIN
#else
    #define LED_PIN  2  // The WeMos D1 Mini has a built-in LED on pin 2 that is active low
#endif

#endif //UNTITLED_HAL_H
