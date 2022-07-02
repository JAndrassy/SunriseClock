#include <ESP8266WiFi.h>
#include <WiFiManager.h> // author tzapu
#include <TZ.h>  // in esp8266 core
#include <coredecls.h> // for settimeofday_cb()
#include <TM1637Display.h> // author Avishay Orpaz
#include <Encoder.h> // patched with https://github.com/PaulStoffregen/Encoder/pull/49
#include <Bounce2.h> // maintainer Thomas O Fredericks
#include <ESP_EEPROM.h> //author j-watson
#include <Average.h> // https://github.com/MajenkoLibraries/Average
#include "consts.h"

TM1637Display display(DISPLAY_SCLK_PIN, DISPLAY_DATA_PIN);
byte displayData[4];

Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
Button button;

short nextAlarmMinute = -1;
bool alarmIsNextDay = false;
short nextAlarmIndicationMinute = -1;

byte setAlarmHour = 0;
byte setAlarmMinute = 0;
byte lightLevel = 0;

struct {
  AlarmMode alarmMode = AlarmMode::OFF;
  byte alarmHour15 = 6;
  byte alarmMinute15 = 45;
  byte alarmHour67 = 8;
  byte alarmMinute67 = 00;
  AlarmDisplay alarmDisplay = AlarmDisplay::BRIGHT;
  AlarmLEDs alarmLEDs = AlarmLEDs::SUNRISE;
  SleepLEDs sleepLEDs = SleepLEDs::FIRE;
  byte alarmDuration = 60;
  byte sunriseDuration = 10;
  byte sleepDuration = 60;
} config;

WiFiManager wm;

char alarmLEDsComboBoxHtml[htmlAlarmLEDsComboBoxLength];
char alarmDisplayComboBoxHtml[htmlAlarmDisplayComboBoxLength];
char sleepLEDsComboBoxHtml[htmlSleepLEDsComboBoxLength];
WiFiManagerParameter wmParamAlarmLEDs(alarmLEDsComboBoxHtml);
WiFiManagerParameter wmParamAlarmDisplay(alarmDisplayComboBoxHtml);
WiFiManagerParameter wmParamSleepLEDs(sleepLEDsComboBoxHtml);
WiFiManagerParameter wmParamAlarmDuration("alarmduration", "Alarm duration",
    "60", 3, htmlInputTypeNumber);
WiFiManagerParameter wmParamSunriseDuration("sunriseduration", "Sunrise duration",
    "10", 3, htmlInputTypeNumber);
WiFiManagerParameter wmParamSleepDuration("sleepduration", "Fall asleep duration",
    "60", 3, htmlInputTypeNumber);

void setup() {

  pinMode(LEDS_CENTRAL_PIN, OUTPUT);
  pinMode(LEDS_LEFT_PIN, OUTPUT);
  pinMode(LEDS_RIGHT_PIN, OUTPUT);
  ledsLight(0);

  pinMode(INDICATOR_LED_PIN, OUTPUT);
  digitalWrite(INDICATOR_LED_PIN, LOW); // on (inverted circuit)

  button.attach(BUTTON_PIN, INPUT_PULLDOWN_16);
  button.setPressedState(HIGH);

  display.setBrightness(7, true);
  display.setSegments(SEG_JA);

  EEPROM.begin(sizeof(config));
  if (EEPROM.percentUsed() >= 0) {
    EEPROM.get(0, config);
  }

  sprintf_P(alarmDisplayComboBoxHtml, htmlAlarmDisplayComboBox, (int) config.alarmDisplay);
  sprintf_P(alarmLEDsComboBoxHtml, htmlAlarmLEDsComboBox, (int) config.alarmLEDs);
  sprintf_P(sleepLEDsComboBoxHtml, htmlSleepLEDsComboBox, (int) config.sleepLEDs);
  itoa(config.alarmDuration, (char*) wmParamAlarmDuration.getValue(), 10);
  itoa(config.sunriseDuration, (char*) wmParamSunriseDuration.getValue(), 10);
  itoa(config.sleepDuration, (char*) wmParamSleepDuration.getValue(), 10);

  wm.setMenu(wmMenuItems);
  wm.setCustomHeadElement(htmlStyles);
  wm.addParameter(&wmParamAlarmLEDs);
  wm.addParameter(&wmParamAlarmDisplay);
  wm.addParameter(&wmParamAlarmDuration);
  wm.addParameter(&wmParamSunriseDuration);
  wm.addParameter(&wmParamSleepLEDs);
  wm.addParameter(&wmParamSleepDuration);
  wm.setSaveParamsCallback(saveParamsCallback);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    display.setSegments(SEG_CNAP);
    wm.autoConnect("NTPClock");
    saveConfig();
  }
  WiFi.persistent(false); // to not store mode changes

  memcpy(displayData, SEG_JA, 4);

  settimeofday_cb([]() {
    WiFi.mode(WIFI_OFF);
    digitalWrite(INDICATOR_LED_PIN, HIGH); // off (inverted circuit)
    calculateNextAlarmMinutes();
  });
  configTime(TIME_ZONE, "pool.ntp.org");
}

void loop() {

  static unsigned long previousMillis;
  static unsigned long ntpPrevMillis;
  static unsigned long clockPrevMillis;
  static unsigned long displayRefreshMillis;
  static unsigned long menuTimeoutMillis;
  static byte blink;
  static byte state = CLOCK;
  static ClockState clockState = ClockState::IDLE;
  static AlarmSetState alarmSetState = AlarmSetState::NONE;
  static byte lastDisplayedMinute = FORCE_SHOW_CLOCK;
  static short minuteNow;
  static bool alarmExecuted;  // flag for the first minute of alarm
  static short alarmStartMinute;
  static short sleepStartMinute;

  unsigned long currentMillis = millis();

  // WiFi for NTP query
  if (state == CLOCK && clockState == ClockState::IDLE
      && (currentMillis - ntpPrevMillis > (1000L * 60 * 60 * 24) || !ntpPrevMillis)) {
    ntpPrevMillis = currentMillis;
    WiFi.begin();
  }
  if (WiFi.isConnected()) {
    digitalWrite(INDICATOR_LED_PIN, blink);
    if (currentMillis - ntpPrevMillis > 30000) {
      WiFi.mode(WIFI_OFF);
      calculateNextAlarmMinutes(); // to restore indicator LED state
    }
  }

  if (state == CLOCK && currentMillis - clockPrevMillis > 1000 && !button.isPressed()) {
    clockPrevMillis = currentMillis;
    blink = !blink;
    time_t t = time(nullptr);
    struct tm *tm = localtime(&t);
    if (tm->tm_year >= 120) {
      byte minute = tm->tm_min;
      byte hour = tm->tm_hour;

      // show the time
      if (lastDisplayedMinute != minute) {
        lastDisplayedMinute = minute;
        displayData[0] = (hour < 10) ? 0 : display.encodeDigit(hour / 10);
        displayData[1] = display.encodeDigit(hour % 10) | 0x80; // with colon
        displayData[2] = display.encodeDigit(minute / 10);
        displayData[3] = display.encodeDigit(minute % 10);
      }

      // handle alarm
      minuteNow = hour * 60 + minute;
      if (minuteNow == 0) { // a new day at midnight
        calculateNextAlarmMinutes();
      }
      if (nextAlarmIndicationMinute != -1 && minuteNow >= nextAlarmIndicationMinute) {
        digitalWrite(INDICATOR_LED_PIN, LOW); // inverted circuit
        nextAlarmIndicationMinute = -1; // done
      }
      if (alarmExecuted) {
        if (minuteNow == alarmStartMinute + 1) {
          alarmExecuted = false;
          calculateNextAlarmMinutes();
        }
      } else if (!alarmIsNextDay && minuteNow == nextAlarmMinute && clockState != ClockState::ALARM) {
        clockState = ClockState::ALARM;
        alarmExecuted = true;
        alarmStartMinute = minuteNow;
      }
    }
  }

  // alarm time setting display
  if (alarmSetState != AlarmSetState::NONE) {
    if (currentMillis - previousMillis >= SET_TIME_BLINK_MILLIS) {
      previousMillis = currentMillis;
      blink = !blink;
      if (blink && alarmSetState == AlarmSetState::HOUR) {
        displayData[0] = 0;
        displayData[1] = 0;
      } else {
        displayData[0] = setAlarmHour < 10 ? 0 : display.encodeDigit(setAlarmHour / 10);
        displayData[1] = display.encodeDigit(setAlarmHour % 10);
      }
      displayData[1] |= 0x80;
      if (blink && alarmSetState == AlarmSetState::MINUTE) {
        displayData[2] = 0;
        displayData[3] = 0;
      } else {
        displayData[2] = display.encodeDigit(setAlarmMinute / 10);
        displayData[3] = display.encodeDigit(setAlarmMinute % 10);
      }
    }
  }

  // encoder
  int dir = encoder.read();
  if (abs(dir) >= ENCODER_PULSES_PER_STEP) {
    if (state == CLOCK) {
      if (clockState == ClockState::IDLE) {
        rotateValue(lightLevel, dir, 0, 5);
        ledsLight(lightLevel);
      }
    } else { // in menu
      menuTimeoutMillis = 0;
      if (alarmSetState == AlarmSetState::HOUR) {
        rotateValue(setAlarmHour, dir, 0, 23);
      } else if (alarmSetState == AlarmSetState::MINUTE) {
        rotateValue(setAlarmMinute, dir, 0, 59);
      } else {
        rotateValue(state, dir, MENU_END, MENU_CONF_AP);
        updateMenu(state);
      }
    }
    encoder.write(0);
  }

  // button
  static unsigned long buttonPressedMillis;
  button.update();
  if (button.pressed()) {
    buttonPressedMillis = currentMillis;
    if (state == CLOCK) {
      clockState = ClockState::IDLE;
      resetEffects();
      if (nextAlarmMinute == -1) {
        memcpy_P(displayData, SEG_NOTIME, 4);
      } else {
        short h = nextAlarmMinute / 60;
        short m = nextAlarmMinute - (h * 60);
        displayData[0] = (h < 10) ? 0 : display.encodeDigit(h / 10);
        displayData[1] = display.encodeDigit(h % 10) | 0x80; // with colon
        displayData[2] = display.encodeDigit(m / 10);
        displayData[3] = display.encodeDigit(m % 10);
      }
      lastDisplayedMinute = FORCE_SHOW_CLOCK;
    }
  }
  if (buttonPressedMillis && currentMillis - buttonPressedMillis > LONG_PUSH_INTERVAL) {
    buttonPressedMillis = 0;
    menuTimeoutMillis = 0;
    if (state == CLOCK) {
      lastDisplayedMinute = FORCE_SHOW_CLOCK;
      state = MENU_ALARM_MODE;
      updateMenu(state);
    }
  }
  if (button.released() && buttonPressedMillis) { // released before long push time
    buttonPressedMillis = 0;
    menuTimeoutMillis = 0;
    switch (state) {
      case MENU_END:
        state = CLOCK;
        saveConfig();
        break;
      case MENU_START_SLEEP:
        resetEffects();
        sleepStartMinute = minuteNow;
        clockState = ClockState::SLEEP;
        state = CLOCK;
        saveConfig();
        break;
      case MENU_ALARM_MODE:
        rotateValue((byte&) config.alarmMode, 1, (byte) AlarmMode::OFF, (byte) AlarmMode::WORK_DAYS);
        updateMenu(state);
        calculateNextAlarmMinutes(); // to update the indicator immediately
        break;
      case MENU_ALARM_D1_D5:
        switch (alarmSetState) {
          case AlarmSetState::NONE:
            setAlarmHour = config.alarmHour15;
            setAlarmMinute = config.alarmMinute15;
            alarmSetState = AlarmSetState::HOUR;
            break;
          case AlarmSetState::HOUR:
            alarmSetState = AlarmSetState::MINUTE;
            break;
          case AlarmSetState::MINUTE:
            alarmSetState = AlarmSetState::NONE;
            config.alarmHour15 = setAlarmHour;
            config.alarmMinute15 = setAlarmMinute;
            updateMenu(state);
            calculateNextAlarmMinutes(); // to update the indicator immediately
            break;
        }
        break;
      case MENU_ALARM_D6_D7:
        switch (alarmSetState) {
          case AlarmSetState::NONE:
            setAlarmHour = config.alarmHour67;
            setAlarmMinute = config.alarmMinute67;
            alarmSetState = AlarmSetState::HOUR;
            break;
          case AlarmSetState::HOUR:
            alarmSetState = AlarmSetState::MINUTE;
            break;
          case AlarmSetState::MINUTE:
            alarmSetState = AlarmSetState::NONE;
            config.alarmHour67 = setAlarmHour;
            config.alarmMinute67 = setAlarmMinute;
            updateMenu(state);
            calculateNextAlarmMinutes(); // to update the indicator immediately
            break;
        }
        break;
      case MENU_CONF_AP: {
        memcpy(displayData, SEG_CNAP, 4);
        display.setSegments(displayData);
        wm.setConfigPortalTimeout(120);
        wm.startConfigPortal("NTPClock"); // returns after Exit in browser or timeout
        state = CLOCK;
        saveConfig();
        break;
      }
    }
  }

  // menu timeout
  if (state != CLOCK) { // in menu
    if (menuTimeoutMillis == 0) {
      menuTimeoutMillis = currentMillis;
    }
    if (currentMillis - menuTimeoutMillis > MENU_DISPLAY_TIMEOUT) {
      menuTimeoutMillis = 0;
      alarmSetState = AlarmSetState::NONE;
      state = CLOCK;
      alarmExecuted = false;
      saveConfig();
    }
  }

  // alarm handling
  if (clockState == ClockState::ALARM) {
    switch (config.alarmLEDs) {
      case AlarmLEDs::NONE:
        break;
      case AlarmLEDs::SUNRISE:
        sunriseEffect(false);
        break;
      case AlarmLEDs::BLINK:
        if (config.alarmDisplay != AlarmDisplay::BLINK) { // if display blinks, then it is there
          ledsLight(blink ? LIGHT_MAX_LEVEL : 0);
        }
        break;
      case AlarmLEDs::CYCLE:
        cycleEffect();
        break;
      case AlarmLEDs::PULSE:
        pulseEffect();
        break;
    }
    if (alarmStartMinute + config.alarmDuration > 24 * 60) { // alarm running over midnight. why not?
      minuteNow += 24 * 60;
    }
    if (minuteNow >= alarmStartMinute + config.alarmDuration) {
      clockState = ClockState::IDLE;
      resetEffects();
    }
  }

  // fall asleep handling
  if (clockState == ClockState::SLEEP) {
    bool effectFinished = true;
    switch (config.sleepLEDs) {
      case SleepLEDs::FADE:
        effectFinished = !fadeEffect(false);
        break;
      case SleepLEDs::FIRE:
        effectFinished = !fireEffect(false);
        break;
      case SleepLEDs::CANDLES:
        candlesEffect();
        break;
    }
    if (sleepStartMinute + config.sleepDuration > 24 * 60) {
      minuteNow += 24 * 60;
    }
    if (effectFinished && minuteNow >= sleepStartMinute + config.sleepDuration) {
      clockState = ClockState::IDLE;
      resetEffects();
    }
  }

  // display refresh
  if (currentMillis - displayRefreshMillis > DISPLAY_REFRESH_INTERVAL) {
    displayRefreshMillis = currentMillis;
    static Average<short> ave(20);
    static int lastMean = 1300; // init out of range
    if (clockState == ClockState::ALARM && config.alarmDisplay != AlarmDisplay::NONE) {
      switch (config.alarmDisplay) {
        case AlarmDisplay::NONE: // to satisfy compiler warning
          break;
        case AlarmDisplay::BRIGHT:
          display.setBrightness(7, true);
          break;
        case AlarmDisplay::BLINK:
          display.setBrightness(7, blink);
          if (config.alarmLEDs == AlarmLEDs::BLINK) { // to blink in sync with the display
            ledsLight(blink ? LIGHT_MAX_LEVEL : 0);
          }
          break;
      }
      lastMean = 1300;
    } else {
      short a = analogRead(PIN_A0);
      float mean = ave.rolling(a);
      if (abs(mean - lastMean) > 5) {
        lastMean = mean;
        byte brightness;
        if (a > LDR_MAX_LIGHT) {
          brightness = 7;
        } else {
          brightness = map(mean, 0, LDR_MAX_LIGHT, 0, 7);
        }
        display.setBrightness(brightness, true);
      }
    }
    display.setSegments(displayData);
  }
}

void calculateNextAlarmMinutes() {
  time_t t = time(nullptr);
  struct tm *tm = localtime(&t);
  nextAlarmMinute = -1;
  alarmIsNextDay = false;
  nextAlarmIndicationMinute = -1;
  if (tm->tm_year < 120) // 2020
    return;
  byte minute = tm->tm_min;
  byte hour = tm->tm_hour;
  byte day = tm->tm_wday;
  short minuteNow = hour * 60 + minute;
  for (int i = 0; ; i++) {
    bool workday = (day > 0 && day < 6);
    if (config.alarmMode == AlarmMode::ON || (workday && config.alarmMode == AlarmMode::WORK_DAYS)) {
      nextAlarmMinute = workday ? config.alarmHour15 * 60 + config.alarmMinute15 //
          : config.alarmHour67 * 60 + config.alarmMinute67;
      if (nextAlarmMinute > minuteNow)
        break;
    }
    if (i == 1)
      break;
    day = (day == 6) ? 0 : day + 1;
    nextAlarmMinute = -1;
    alarmIsNextDay = true;
  }
  bool alarmInNext12Hours = false;
  if (nextAlarmMinute != -1) {
    nextAlarmIndicationMinute = nextAlarmMinute + (alarmIsNextDay ? (+24 - 12) * 60 : -12 * 60);
    alarmInNext12Hours = minuteNow >= nextAlarmIndicationMinute;
  }
  digitalWrite(INDICATOR_LED_PIN, !alarmInNext12Hours); // inverted circuit
  if (alarmInNext12Hours || nextAlarmIndicationMinute > 24 * 60) {
    nextAlarmIndicationMinute = -1; // done for today
  }
}

void updateMenu(byte state) {
  switch (state) {
    case MENU_END:
      memcpy(displayData, SEG_MENU_END, 4);
      break;
    case MENU_START_SLEEP:
      memcpy(displayData, SEG_MENU_SLEEP, 4);
      break;
    case MENU_ALARM_MODE:
      memcpy(displayData, SEG_ALARM_MODE_SET_AL_NO, 4);
      switch (config.alarmMode) {
        case AlarmMode::OFF:
          break;
        case AlarmMode::ON:
          displayData[2] = SEG_ALARM_MODE_SET_AL_NO[3]; // o
          displayData[3] = SEG_ALARM_MODE_SET_AL_NO[2]; // n
          break;
        case AlarmMode::WORK_DAYS:
          displayData[2] = display.encodeDigit(1);
          displayData[3] = display.encodeDigit(5);
          break;
      }
      break;
    case MENU_ALARM_D1_D5:
      memcpy(displayData, SEG_MENU_D1_D5, 4);
      break;
    case MENU_ALARM_D6_D7:
      memcpy(displayData, SEG_MENU_D6_D7, 4);
      break;
    case MENU_CONF_AP:
      memcpy(displayData, SEG_CONF, 4);
      break;
  }
}

void saveParamsCallback() {
  config.alarmDuration = atoi(wmParamAlarmDuration.getValue());
  config.sunriseDuration = atoi(wmParamSunriseDuration.getValue());
  config.sleepDuration = atoi(wmParamSleepDuration.getValue());
  String s = wm.server->arg("ledalarmefect");
  config.alarmLEDs = (AlarmLEDs) s.toInt();
  s = wm.server->arg("displayalarmefect");
  config.alarmDisplay = (AlarmDisplay) s.toInt();
  s = wm.server->arg("ledsleepefect");
  config.sleepLEDs = (SleepLEDs) s.toInt();
  sprintf_P(alarmDisplayComboBoxHtml, htmlAlarmDisplayComboBox, (int) config.alarmDisplay);
  sprintf_P(alarmLEDsComboBoxHtml, htmlAlarmLEDsComboBox, (int) config.alarmLEDs);
  sprintf_P(sleepLEDsComboBoxHtml, htmlSleepLEDsComboBox, (int) config.sleepLEDs);
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void rotateValue(byte &val, const int dir, const byte min, const byte max) {
  if (dir > 0) {
    val = (val < max) ? val + 1 : min;
  } else {
    val = (val > min) ? val - 1 : max;
  }
}

void resetEffects() {
  sunriseEffect(true);
  fireEffect(true);
  fadeEffect(true);
  ledsLight(0);
  lightLevel = 0;
}

/**
 * intensity without PWM
 * level 0 is all LEDs off
 * level 1 are 3 left LEDs
 * level 2 are 4 central LEDs
 * level 3 are 6 LEDs (left and right block)
 * level 4 are 7 LEDs (left and central block)
 * level 5 are all 10 LEDs on
 */
void ledsLight(byte level) {
  digitalWrite(LEDS_CENTRAL_PIN, level == 2 || level == 4 || level == 5);
  digitalWrite(LEDS_LEFT_PIN, level != 0 && level != 2);
  digitalWrite(LEDS_RIGHT_PIN, level == 3 || level == 5);
}

void cycleEffect() {

  static unsigned long previousMillis;
  static uint8_t cycleCounter;

  if (millis() - previousMillis > 100) {
    previousMillis = millis();
    digitalWrite(LEDS_CENTRAL_PIN, cycleCounter == 0);
    digitalWrite(LEDS_LEFT_PIN, cycleCounter == 1);
    digitalWrite(LEDS_RIGHT_PIN, cycleCounter == 2);
    cycleCounter++;
    if (cycleCounter == 3) {
      cycleCounter = 0;
    }
  }
}

void pulseEffect() {

  static short pwm;
  static int8_t dir = 1;
  static unsigned long previousMillis;

  if (millis() - previousMillis > 5) {
    previousMillis = millis();
    analogWrite(LEDS_CENTRAL_PIN, pwm);
    analogWrite(LEDS_LEFT_PIN, pwm);
    analogWrite(LEDS_RIGHT_PIN, pwm);
    if (pwm == LED_MAX_PWM || pwm == 0) {
      dir *= -1;
    }
    pwm += dir;
  }
}

void sunriseEffect(bool reset) {

  static short pwm1;
  static short pwm2;
  static unsigned long step;
  static unsigned long previousMillis;
  static bool fin;

  if (reset) {
    pwm1 = 0;
    pwm2 = 0;
    step = (60000L * config.sunriseDuration) / (2 * LED_MAX_PWM);
    fin = false;
  }
  if (fin)
    return;
  if (millis() - previousMillis > step) {
    previousMillis = millis();
    if (pwm1 < LED_MAX_PWM) {
      pwm1++;
      analogWrite(LEDS_CENTRAL_PIN, pwm1);
    } else if (pwm2 < LED_MAX_PWM) {
      if (pwm2 == 0) {
        digitalWrite(LEDS_CENTRAL_PIN, HIGH);
      }
      pwm2++;
      analogWrite(LEDS_LEFT_PIN, pwm2);
      analogWrite(LEDS_RIGHT_PIN, pwm2);
    } else {
      ledsLight(LIGHT_MAX_LEVEL);
      fin = true;
    }
  }
}

bool fadeEffect(bool reset) {

  static short pwm;
  static unsigned long step;
  static unsigned long previousMillis;

  if (reset) {
    pwm = LED_MAX_PWM;
    step = (60000L * config.sleepDuration) / LED_MAX_PWM;
  }
  if (pwm == 0)
    return false;

  if (millis() - previousMillis > step) {
    previousMillis = millis();
    pwm--;
    analogWrite(LEDS_CENTRAL_PIN, pwm);
    analogWrite(LEDS_LEFT_PIN, pwm);
    analogWrite(LEDS_RIGHT_PIN, pwm);
  }
  return true;
}

bool fireEffect(bool reset) {

  const short BASE_PWM_RANGE = LED_MAX_PWM /  4;
  const short FLAME_PWM_RANGE = LED_MAX_PWM /  2;
  const byte pins[] = {LEDS_CENTRAL_PIN, LEDS_LEFT_PIN, LEDS_RIGHT_PIN};
  const short speedStep[sizeof(pins)] = {500, 100, 100}; // micros

  static short targetPwm[sizeof(pins)];
  static short currentPwm[sizeof(pins)];
  static unsigned long prevMicros[sizeof(pins)];

  static short topPwm; // fade
  static unsigned long step;
  static unsigned long previousMillis;

  if (reset) {
    topPwm = LED_MAX_PWM;
    step = (60000L * config.sleepDuration) / LED_MAX_PWM;
    for (int i = 0; i < sizeof(pins); i++) {
      currentPwm[i] = 0;
    }
  }
  if (topPwm == 0)
    return false;
  for (int i = 0; i < sizeof(pins); i++) {
    if (micros() - prevMicros[i] > speedStep[i]) {
      prevMicros[i] = micros();
      if (currentPwm[i] == targetPwm[i]) {
        targetPwm[i] = random(topPwm - ((i == 0) ? (topPwm / 4) : (2 * topPwm / 3)), topPwm);
      }
      currentPwm[i] += targetPwm[i] < currentPwm[i] ? -1 : 1;
      analogWrite(pins[i], currentPwm[i]);
    }
  }
  // fade fire
  if (millis() - previousMillis > step) {
    previousMillis = millis();
    topPwm--;
  }
  return true;
}

void candlesEffect() {

  const short FLAME_PWM_RANGE = 2 * LED_MAX_PWM / 3;
  const short FLAME_SPEED = 400;
  const byte pins[] = {LEDS_LEFT_PIN, LEDS_RIGHT_PIN};

  static short targetPwm[sizeof(pins)];
  static short currentPwm[sizeof(pins)];
  static unsigned long prevMicros[sizeof(pins)];

  for (int i = 0; i < sizeof(pins); i++) {
    if (micros() - prevMicros[i] > FLAME_SPEED) {
      prevMicros[i] = micros();
      if (currentPwm[i] == targetPwm[i]) {
        targetPwm[i] = random(LED_MAX_PWM - FLAME_PWM_RANGE, LED_MAX_PWM);
      }
      currentPwm[i] += targetPwm[i] < currentPwm[i] ? -1 : 1;
      analogWrite(pins[i], currentPwm[i]);
    }
  }
}
