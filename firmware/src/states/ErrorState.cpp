#include "states/ErrorState.h"

#include <Arduino.h>

#include "Config.h"
#include "Controllers.h"

void ErrorState::enter() {
  ledController.setSolid(Config::LED_ERROR_COLOR);
  if (Config::ENABLE_TELEMETRY) {
    Serial.println(">error:sensor");
  }
}

void ErrorState::update() {}

void ErrorState::exit() {}
