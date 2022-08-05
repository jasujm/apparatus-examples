#include <assert.h>
#include <math.h>

#include "HX711.h"
#include "LowPower.h"

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;
const int LED_PIN = 7;

const long LOADCELL_OFFSET = 159512;
const long LOADCELL_DIVIDER = 206737;
const float RAZOR_OFF_THRESHOLD = 1.1f;
const int CHANGE_BLADE_THRESHOLD = 6;

HX711 loadcell {};

int bladeCounter = 0;

const size_t N_MEASUREMENTS = 2;
float measurements[N_MEASUREMENTS] {};
size_t measurementIndex = 0;

enum class State {
  RAZOR_ON,
  RAZOR_OFF_CHANGE_BLADE,
  RAZOR_OFF_IDLE
};

State currentState = State::RAZOR_ON;

void setLed(bool isOn = true) {
  digitalWrite(LED_PIN, isOn ? HIGH : LOW);
}

void resetLed() {
  setLed(false);
}

void transitRazorOff() {
  assert(currentState == State::RAZOR_ON);
  ++bladeCounter;
  for (auto i = 0; i < 2 * bladeCounter; ++i) {
    setLed(i % 2 == 0);
    delay(500);
  }
  if (bladeCounter >= CHANGE_BLADE_THRESHOLD) {
    currentState = State::RAZOR_OFF_CHANGE_BLADE;
    setLed();
  } else {
    currentState = State::RAZOR_OFF_IDLE;
  }
}

void transitRazorOn() {
  assert(currentState != State::RAZOR_ON);
  if (currentState == State::RAZOR_OFF_CHANGE_BLADE) {
    bladeCounter = 0;
  }
  currentState = State::RAZOR_ON;
  resetLed();
}

void wakeUp() {
  // just some dummy stuff, the interrupt is used for waking up from sleep
}

void setup() {
  Serial.begin(9600);
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.set_offset(LOADCELL_OFFSET);
  loadcell.set_scale(LOADCELL_DIVIDER);
  pinMode(LED_PIN, OUTPUT);
  resetLed();
  for (auto& measurement : measurements) {
    measurement = INFINITY;
  }
}

void loop() {
  attachInterrupt(digitalPinToInterrupt(2), wakeUp, LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(digitalPinToInterrupt(2));

  // add measurement to ring buffer if we can, ignore if we can't
  measurements[measurementIndex] = loadcell.get_units();
  measurementIndex = (measurementIndex + 1) % N_MEASUREMENTS;

  // weight is above or below threshold only if it definitely is
  auto weightBelowThreshold = true;
  auto weightAboveThreshold = true;
  for (auto measurement : measurements) {
    if (measurement < RAZOR_OFF_THRESHOLD) {
      weightAboveThreshold = false;
    } else if (measurement > RAZOR_OFF_THRESHOLD) {
      weightBelowThreshold = false;
    }
  }

  // the state machine and transitions
  switch (currentState) {
  case State::RAZOR_ON:
    if (weightBelowThreshold) {
      transitRazorOff();
    }
    break;
  case State::RAZOR_OFF_CHANGE_BLADE:
  case State::RAZOR_OFF_IDLE:
    if (weightAboveThreshold) {
      transitRazorOn();
    }
  }

  loadcell.power_down();
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  loadcell.power_up();
}
