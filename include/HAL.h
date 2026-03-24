#ifndef UNTITLED_HAL_H
#define UNTITLED_HAL_H


// Conditional compilation for pin definitions
// ---------------------------------------------------------------------------------------------------------------------
#ifdef ESP32
    #define BATTERY_PIN  34
#elif defined(ESP8266)
    #define BATTERY_PIN  A0 // The WeMos D1 Mini has a built-in voltage divider for the battery on A0 (0v-3.3v)
#endif

// Universal pin definitions
// ---------------------------------------------------------------------------------------------------------------------
#ifdef LED_BUILTIN
    #define LED_PIN  LED_BUILTIN
#else
    #define LED_PIN  2  // The WeMos D1 Mini has a built-in LED on pin 2 that is active low
#endif

#endif //UNTITLED_HAL_H