#ifndef CONSTS_H
#define CONSTS_H

#define TIME_ZONE TZ_Europe_Bratislava

const byte BUTTON_PIN = D0;
const byte LEDS_CENTRAL_PIN = D1;
const byte DISPLAY_SCLK_PIN = D2;
const byte DISPLAY_DATA_PIN = D3;
const byte INDICATOR_LED_PIN = D4; // built-in LED of Wemos D1
const byte ENCODER_PIN_A = D5;
const byte ENCODER_PIN_B = D6;
const byte LEDS_LEFT_PIN = D7;
const byte LEDS_RIGHT_PIN = D8;

const byte ENCODER_PULSES_PER_STEP = 4;
const byte FORCE_SHOW_CLOCK = 60;
const unsigned short DISPLAY_REFRESH_INTERVAL = 100; // milliseconds
const unsigned short LONG_PUSH_INTERVAL = 2000; // miliseconds
const unsigned short SET_TIME_BLINK_MILLIS = 250;
const unsigned short ALARM_BLINK_MILLIS = 700;
const unsigned MENU_DISPLAY_TIMEOUT = 30000; // miliseconds
const short LDR_MAX_LIGHT = 100; // max for max display brightness
const short LED_MAX_PWM = PWMRANGE;
const byte LIGHT_MAX_LEVEL = 5;

enum {
  CLOCK,
  MENU_END,
  MENU_START_SLEEP,
  MENU_ALARM_MODE,
  MENU_ALARM_D1_D5,
  MENU_ALARM_D6_D7,
  MENU_CONF_AP
};

enum struct ClockState {
  IDLE,
  ALARM,
  SLEEP
};

enum struct AlarmSetState {
  NONE,
  HOUR,
  MINUTE
};

enum struct AlarmMode {
  OFF,
  ON,
  WORK_DAYS
};

enum struct AlarmDisplay {
  NONE,
  BRIGHT,
  BLINK
};

enum struct AlarmLEDs {
  NONE,
  SUNRISE,
  BLINK,
  CYCLE,
  PULSE
};

enum struct SleepLEDs {
  FADE,
  FIRE,
  CANDLES
};

extern const uint8_t SEG_JA[];
extern const uint8_t SEG_NOTIME[];
extern const uint8_t SEG_CONF[];
extern const uint8_t SEG_CNAP[];
extern const uint8_t SEG_ALARM_MODE_SET_AL_NO[];
extern const uint8_t SEG_MENU_D1_D5[];
extern const uint8_t SEG_MENU_D6_D7[];
extern const uint8_t SEG_MENU_SLEEP[];
extern const uint8_t SEG_MENU_END[];

const size_t htmlAlarmLEDsComboBoxLength = 360; // to have the size checked by the compiler
const size_t htmlAlarmDisplayComboBoxLength = 320;
const size_t htmlSleepLEDsComboBoxLength = 299;

extern std::vector<const char*> wmMenuItems;
extern const char htmlAlarmLEDsComboBox[htmlAlarmLEDsComboBoxLength];
extern const char htmlAlarmDisplayComboBox[htmlAlarmLEDsComboBoxLength];
extern const char htmlSleepLEDsComboBox[htmlSleepLEDsComboBoxLength];
extern const char* htmlInputTypeNumber;
extern const char* htmlStyles;

#endif
