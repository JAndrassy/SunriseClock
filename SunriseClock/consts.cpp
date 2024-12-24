#include <TM1637Display.h>

#include <Arduino.h>
#include "consts.h"

//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D

const uint8_t SEG_JA[] = {
  SEG_A | SEG_D | SEG_E | SEG_F,   // [
  SEG_B | SEG_C | SEG_D | SEG_E,   // J
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,   // A
  SEG_A | SEG_B | SEG_C | SEG_D    // ]
  };

const uint8_t SEG_NOTIME[] = {SEG_G, SEG_G | 0x80, SEG_G, SEG_G}; // --:--

const uint8_t SEG_CONF[] = {
  SEG_A | SEG_D | SEG_E | SEG_F,   // C
  SEG_C | SEG_D | SEG_E | SEG_G,   // o
  SEG_C | SEG_E | SEG_G,           // n
  SEG_A | SEG_E | SEG_F | SEG_G    // f
  };

const uint8_t SEG_CNAP[] = {
  SEG_A | SEG_D | SEG_E | SEG_F,                 // C
  SEG_C | SEG_E | SEG_G,                         // n
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G          // P
  };

const uint8_t SEG_ALARM_MODE_SET_AL_NO[] = {
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
  SEG_D | SEG_E | SEG_F | 0x80,                  // L:
  SEG_C | SEG_E | SEG_G,           // n
  SEG_C | SEG_D | SEG_E | SEG_G    // o
  };

const uint8_t SEG_ALARM_EFFECT_NONE[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_A | SEG_E | SEG_F | SEG_G | 0x80,  // F:
  SEG_C | SEG_E | SEG_G,           // n
  SEG_C | SEG_D | SEG_E | SEG_G    // o
  };

const uint8_t SEG_ALARM_EFFECT_SUNRISE[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_A | SEG_E | SEG_F | SEG_G | 0x80,  // F:
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G, // S
  SEG_E | SEG_G                          // r
  };

const uint8_t SEG_ALARM_EFFECT_BLINK[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
  SEG_A | SEG_E | SEG_F | SEG_G | 0x80,   // F:
  SEG_C | SEG_D  | SEG_E | SEG_F | SEG_G, // b
  SEG_D | SEG_E | SEG_F                   // L
  };

const uint8_t SEG_ALARM_EFFECT_CYCLE[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_A | SEG_E | SEG_F | SEG_G | 0x80,  // F:
  SEG_A | SEG_D | SEG_E | SEG_F,         // C
  SEG_D | SEG_E | SEG_G                  // c
  };

const uint8_t SEG_ALARM_EFFECT_PULSE[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_A | SEG_E | SEG_F | SEG_G | 0x80,  // F:
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G, // P
  SEG_C | SEG_D | SEG_E                  // u
  };

const uint8_t SEG_MENU_D1_D5[] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
    SEG_E | SEG_F | 0x80,                  // 1 :
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G, // 5
  };

const uint8_t SEG_MENU_D6_D7[] = {
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, // 6
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
    SEG_A | SEG_B | SEG_C,                 // 7
  };

const uint8_t SEG_MENU_SLEEP[] = {
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G, // S
    SEG_D | SEG_E | SEG_F,                 // L
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  };

const uint8_t SEG_MENU_END[] = {
    0,
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
    SEG_C | SEG_E | SEG_G,                 // n
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G, // d
  };

std::vector<const char*> wmMenuItems = { "wifi", "info", "param", "erase", "exit" };
const char htmlAlarmLEDsComboBox[htmlAlarmLEDsComboBoxLength] PROGMEM = "<label for='ledalarmefect'>LEDs alarm effect</label><br/><select name='ledalarmefect' id='ledalarmefect'><option value='0'>none</option><option value='1'>Sunrise</option><option value='2'>Blink</option><option value='3'>Cycle</option><option value='4'>Pulse</option></select><script>document.getElementById('ledalarmefect').selectedIndex = %d;</script>";
const char htmlAlarmDisplayComboBox[] PROGMEM = "<label for='displayalarmefect'>Display's alarm effect</label><br/><select name='displayalarmefect' id='displayalarmefect'><option value='0'>none</option><option value='1'>Full brightness</option><option value='2'>Blink</option></select><script>document.getElementById('displayalarmefect').selectedIndex = %d;</script>";
const char htmlSleepLEDsComboBox[htmlSleepLEDsComboBoxLength] PROGMEM = "<label for='ledsllepefect'>LEDs fall asleep effect</label><br/><select name='ledsleepefect' id='ledsleepefect'><option value='0'>Fade</option><option value='1'>Fireplace</option><option value='2'>Candles</option></select><script>document.getElementById('ledsleepefect').selectedIndex = %d;</script>";
const char* htmlInputTypeNumber = "type='number' min='1' max='240'";
const char* htmlStyles = "<style>select{padding:5px;font-size:1em;margin:5px 0;box-sizing:border-box;border-radius:.3rem;width: 100%;}</style>";
