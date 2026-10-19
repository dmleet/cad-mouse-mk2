#pragma once

#include "State.h"

// Terminal state: a sensor failed to initialise or stopped responding.
// Shows a solid red ring and keeps USB alive so the serial message is readable.
// Recover by power-cycling the device.
class ErrorState : public State {
 public:
  void enter() override;
  void update() override;
  void exit() override;
};
