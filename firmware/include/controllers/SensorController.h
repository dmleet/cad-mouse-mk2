#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "TLx493D_inc.hpp"

class SensorController {
 public:
  SensorController();

  // Returns false if any sensor failed to initialise or take its I2C address.
  bool begin();

  // Returns false and leaves `out` untouched if any sensor read failed.
  bool readRaw(float out[9]);

  // True once consecutive read failures reach Config::SENSOR_MAX_READ_FAILURES.
  bool faulted() const;

  void beginCalibration();
  void updateCalibration();
  bool calibrationDone() const;

  const float* baseline() const;

 private:
  static void powerOff(int pin);
  static void powerOn(int pin);
  bool initSensor(ifx::tlx493d::TLx493D_A2B6& sensor, int powerPin,
                  const TLx493D_IICAddressType_t* newAddress);

  ifx::tlx493d::TLx493D_A2B6 mag1Sensor_;
  ifx::tlx493d::TLx493D_A2B6 mag2Sensor_;
  ifx::tlx493d::TLx493D_A2B6 mag3Sensor_;

  int readFailures_ = 0;

  bool calibrationActive_ = false;
  bool calibrationDone_ = false;
  int calibrationSamples_ = 0;
  unsigned long lastCalibrationSampleMs_ = 0;
  float calibrationSum_[9] = {};
  float baseline_[9] = {};
};
