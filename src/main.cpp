#include <Arduino.h>
#include "TM1637.h"

#define ONETOUCH
//#define INVERT

#ifdef INVERT
#define TOLERANCE(a)  ((a) & 0xFFF8)
#else
#define TOLERANCE(a)  ((a) * 5) // 500%
#endif

#ifdef ONETOUCH
constexpr uint8_t LED_PIN = 8;
constexpr bool LED_LEVEL = LOW;

constexpr uint8_t TOUCH_PIN = 1;
#else
constexpr uint8_t TOUCH_COUNT = 4;

static const uint8_t TOUCH_PINS[TOUCH_COUNT] = { 1, 2, 3, 4 };
#endif
constexpr uint8_t VDD_PIN = 0;

constexpr uint8_t TICK_TIME = 25; // 25 ms.
#ifndef ONETOUCH
constexpr uint8_t PRESS_TIME = 50 / TICK_TIME; // 50 ms.
constexpr uint8_t HOLD_TIME = 500 / TICK_TIME; // 500 ms.
constexpr uint8_t REPEAT_TIME = 100 / TICK_TIME; // 100 ms.

constexpr uint16_t COUNTER_MIN = 0;
constexpr uint16_t COUNTER_MAX = 9999;
#endif

TM1637<6, 7> display;
#ifndef ONETOUCH
uint16_t counter = COUNTER_MIN;
#ifndef INVERT
uint16_t touch_off[TOUCH_COUNT];
#endif
uint8_t touch_pressed[TOUCH_COUNT];
#else
#ifndef INVERT
uint16_t touch_off;
#endif
#endif

static uint16_t touchGet(uint8_t touch_pin, uint8_t vdd_pin, uint16_t measures = 100) {
  uint32_t sum = 0;

  for (uint16_t i = 0; i < measures; ++i) {
#ifdef INVERT
//    pinMode(touch_pin, OUTPUT);
//    digitalWrite(touch_pin, HIGH);
//    pinMode(vdd_pin, OUTPUT);
//    digitalWrite(vdd_pin, LOW);
    pinMode(touch_pin, INPUT_PULLUP);
    pinMode(vdd_pin, INPUT_PULLDOWN);
#else
//    pinMode(vdd_pin, OUTPUT);
//    digitalWrite(vdd_pin, HIGH);
//    pinMode(touch_pin, OUTPUT);
//    digitalWrite(touch_pin, LOW);
    pinMode(vdd_pin, INPUT_PULLUP);
    pinMode(touch_pin, INPUT_PULLDOWN);
#endif
//    pinMode(vdd_pin, INPUT);
    analogRead(vdd_pin);
    analogRead(vdd_pin);
//    pinMode(touch_pin, INPUT);
    sum += analogRead(touch_pin);
  }
  return sum / measures;
}

#ifndef INVERT
static uint16_t touchFiltered(uint8_t touch_pin, uint8_t vdd_pin) {
  constexpr uint8_t MEDIAN = 5;

  uint16_t measures[MEDIAN];

  for (uint8_t i = 0; i < MEDIAN; ++i) {
    measures[i] = touchGet(touch_pin, vdd_pin);
    yield();
  }
  for (uint8_t i = 0; i < MEDIAN - 1; ++i) {
    for (uint8_t j = i + i; j < MEDIAN; ++j) {
      if (measures[j] < measures[i]) {
        uint16_t t = measures[i];

        measures[i] = measures[j];
        measures[j] = t;
      }
    }
  }
  return measures[MEDIAN / 2];
}
#endif

#ifndef ONETOUCH
static void doTouch(uint8_t index, bool hold) {
  switch (index) {
    case 0: // INC
      if (hold) {
        if (counter < COUNTER_MAX - 10)
          counter += 10;
        else
          counter = COUNTER_MAX;
      } else {
        if (counter < COUNTER_MAX)
          ++counter;
      }
      break;
    case 1: // DEC
      if (hold) {
        if (counter > COUNTER_MIN + 10)
          counter -= 10;
        else
          counter = COUNTER_MIN;
      } else {
        if (counter > COUNTER_MIN)
          --counter;
      }
      break;
    case 2: // INC5
      if (hold) {
        if (counter < COUNTER_MAX - 50)
          counter += 50;
        else
          counter = COUNTER_MAX;
      } else {
        if (counter < COUNTER_MAX - 5)
          counter += 5;
        else
          counter = COUNTER_MAX;
      }
      break;
    case 3: // DEC5
      if (hold) {
        if (counter > COUNTER_MIN + 50)
          counter -= 50;
        else
          counter = COUNTER_MIN;
      } else {
        if (counter > COUNTER_MIN + 5)
          counter -= 5;
        else
          counter = COUNTER_MIN;
      }
      break;
  }
}

static void processTouch(uint8_t index) {
#ifdef INVERT
  if (touchGet(TOUCH_PINS[index], VDD_PIN) < TOLERANCE(4095)) {
#else
  if (touchGet(TOUCH_PINS[index], VDD_PIN) > TOLERANCE(touch_off[index])) {
#endif
    if (touch_pressed[index] < 255)
      ++touch_pressed[index];
    if (touch_pressed[index] > HOLD_TIME) {
      if ((touch_pressed[index] - HOLD_TIME) % REPEAT_TIME == 1)
        doTouch(index, true);
      if (touch_pressed[index] >= HOLD_TIME + REPEAT_TIME)
        touch_pressed[index] -= REPEAT_TIME;
    } else if (touch_pressed[index] == PRESS_TIME + 1)
      doTouch(index, false);
  } else {
    touch_pressed[index] = 0;
  }
}
#endif

void setup() {
  Serial.begin(115200);
  display.clear();

#ifdef ONETOUCH
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ! LED_LEVEL);
#ifndef INVERT
  touch_off = touchFiltered(TOUCH_PIN, VDD_PIN);
#endif
#else
  for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
#ifndef INVERT
    touch_off[i] = touchFiltered(TOUCH_PINS[i], VDD_PIN);
#endif
    touch_pressed[i] = 0;
  }
#endif
}

void loop() {
#ifdef ONETOUCH
  uint16_t t = touchGet(TOUCH_PIN, VDD_PIN);

  Serial.print(t);
  Serial.print("   \r");
  display = t;
#ifdef INVERT
  digitalWrite(LED_PIN, (t < TOLERANCE(4095)) == LED_LEVEL);
#else
  digitalWrite(LED_PIN, (t > TOLERANCE(touch_off)) == LED_LEVEL);
#endif
  delay(TICK_TIME);
#else
  static uint32_t time = 0;

  if ((! time) || (millis() - time >= TICK_TIME)) {
    for (uint8_t i = 0; i < TOUCH_COUNT; ++i) {
      processTouch(i);
      yield();
    }
    Serial.print(counter);
    Serial.print("   \r");
    display = counter;
    time = millis();
  }
#endif
}
