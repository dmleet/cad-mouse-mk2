#include "controllers/SensorController.h"

#include "Config.h"

using namespace ifx::tlx493d;

SensorController::SensorController()
    : mag1Sensor_(Wire, TLx493D_IIC_ADDR_A0_e),
      mag2Sensor_(Wire, TLx493D_IIC_ADDR_A0_e),
      mag3Sensor_(Wire, TLx493D_IIC_ADDR_A0_e) {}

void SensorController::powerOff(int pin) { digitalWrite(pin, LOW); }

void SensorController::powerOn(int pin) {
  digitalWrite(pin, HIGH);
  delay(5);
}

// Powers one sensor, brings it up at the default address, and optionally moves
// it to `newAddress`. Every library call is checked: a silent failure here
// leaves two sensors on the same address and produces garbage motion that looks
// like a mechanical problem.
bool SensorController::initSensor(TLx493D_A2B6& sensor, int powerPin,
                                  const TLx493D_IICAddressType_t* newAddress) {
  powerOn(powerPin);
  if (!sensor.begin(true, false, false, true)) {
    return false;
  }
  if (newAddress != nullptr && !sensor.setIICAddress(*newAddress)) {
    return false;
  }
  if (!sensor.setSensitivity(TLx493D_EXTRA_SHORT_RANGE_e)) {
    return false;
  }
  delay(10);
  return true;
}

bool SensorController::begin() {
  pinMode(Config::PIN_MAG1_LS, OUTPUT);
  pinMode(Config::PIN_MAG2_LS, OUTPUT);
  pinMode(Config::PIN_MAG3_LS, OUTPUT);

  // All three rails are pulled high in hardware, so force them all off first
  // before bringing sensors up one-by-one for address assignment. Each sensor
  // reverts to the default address (A0) when power-cycled.
  powerOff(Config::PIN_MAG1_LS);
  powerOff(Config::PIN_MAG2_LS);
  powerOff(Config::PIN_MAG3_LS);
  delay(5);

  Wire.begin();
  Wire.setClock(400000);

  readFailures_ = 0;

  const TLx493D_IICAddressType_t mag1Addr = TLx493D_IIC_ADDR_A2_e;
  const TLx493D_IICAddressType_t mag2Addr = TLx493D_IIC_ADDR_A1_e;

  // Order matters: each sensor must be moved off A0 before the next is powered.
  if (!initSensor(mag1Sensor_, Config::PIN_MAG1_LS, &mag1Addr)) return false;
  if (!initSensor(mag2Sensor_, Config::PIN_MAG2_LS, &mag2Addr)) return false;
  if (!initSensor(mag3Sensor_, Config::PIN_MAG3_LS, nullptr)) return false;

  return true;
}

bool SensorController::readRaw(float out[9]) {
  double mag1x = 0, mag1y = 0, mag1z = 0;
  double mag2x = 0, mag2y = 0, mag2z = 0;
  double mag3x = 0, mag3y = 0, mag3z = 0;

  const bool ok = mag1Sensor_.getMagneticField(&mag1x, &mag1y, &mag1z) &&
                  mag2Sensor_.getMagneticField(&mag2x, &mag2y, &mag2z) &&
                  mag3Sensor_.getMagneticField(&mag3x, &mag3y, &mag3z);

  if (!ok) {
    if (readFailures_ < Config::SENSOR_MAX_READ_FAILURES) {
      readFailures_++;
    }
    return false;
  }
  readFailures_ = 0;

  // MAG1 = bottom, MAG2 = top left, MAG3 = top right.
  out[0] = mag1x;
  out[1] = mag1y;
  out[2] = mag1z;
  out[3] = mag2x;
  out[4] = mag2y;
  out[5] = mag2z;
  out[6] = mag3x;
  out[7] = mag3y;
  out[8] = mag3z;
  return true;
}

bool SensorController::faulted() const {
  return readFailures_ >= Config::SENSOR_MAX_READ_FAILURES;
}

void SensorController::beginCalibration() {
  calibrationActive_ = true;
  calibrationDone_ = false;
  calibrationSamples_ = 0;
  lastCalibrationSampleMs_ = 0;
  for (int i = 0; i < 9; i++) {
    calibrationSum_[i] = 0.0;
  }
}

void SensorController::updateCalibration() {
  if (!calibrationActive_) {
    return;
  }

  const unsigned long now = millis();
  if (lastCalibrationSampleMs_ != 0 &&
      (now - lastCalibrationSampleMs_) < 10) {
    return;
  }
  lastCalibrationSampleMs_ = now;

  float raw[9] = {};
  if (!readRaw(raw)) {
    return;  // Skip the sample; a failed read must not pollute the baseline.
  }

  for (int i = 0; i < 9; i++) {
    calibrationSum_[i] += raw[i];
  }

  calibrationSamples_++;
  if (calibrationSamples_ < Config::ZERO_SAMPLES) {
    return;
  }

  for (int i = 0; i < 9; i++) {
    baseline_[i] = calibrationSum_[i] / Config::ZERO_SAMPLES;
  }

  calibrationActive_ = false;
  calibrationDone_ = true;
}

bool SensorController::calibrationDone() const { return calibrationDone_; }

const float* SensorController::baseline() const { return baseline_; }
