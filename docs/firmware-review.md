# Firmware review findings

Review of the stock firmware at commit a577e09, done 2026-10-18 by reading the
source, the TinyUSB and TLx493D library code it depends on, and the upstream
issue tracker. Not compiled-tested against hardware at the time of writing.

Status legend: `[ ]` open, `[x]` fixed on local branch, `[-]` won't fix.

## Bugs

- [x] **B1. Button presses can be lost.** (fixed on `firmware-hardening`) `HIDController::sendReports` sends the
  axes report then the button report in the same loop tick. TinyUSB's
  `tud_hid_n_report` returns false if the IN endpoint is still busy from the
  previous transfer; it does not queue. The return value is ignored and
  `buttonBitsSent_` is updated anyway, so a press that coincides with knob
  motion is never retried and the host can see a stuck button.
  Fix: check each send's return, update the "last sent" copy only on success,
  send buttons before axes. `firmware/src/controllers/HIDController.cpp`.

- [ ] **B2. Wake-from-sleep press leaks to the host.** Sleep wakes on a button
  press; Idle then reports it because `buttonBitsSent_` still says released.
  With 3DxWare that fires whatever the button is mapped to.
  Fix: sync `buttonBitsSent_` to current bits on wake, or suppress button
  reports until both are released.

- [x] **B3. No error handling on the I2C sensor path.** (fixed on `firmware-hardening`: checked init, checked reads, fault counter, red `ErrorState`) `begin()`,
  `setIICAddress()` and `getMagneticFieldAndTemperature()` all return bool and
  every return is discarded. A failed address reassignment makes sensors 1 and
  2 collide; a failed read emits zeros that the motion code treats as a large
  deflection. Likely cause of upstream "garbage motion" reports on
  self-assembled boards.
  Fix: propagate failures, skip the frame on read error, show a red LED if
  init fails. Upstream PR 44 has the same shape.

- [ ] **B4. Telemetry on by default.** `Config::ENABLE_TELEMETRY = true` formats
  eight floats over USB CDC every fifth tick on a core with no FPU, and
  enumerates a CDC interface alongside HID. Fine for bring-up, wrong for daily
  use. Fix: default off, expose as a build flag.

## Motion pipeline design

- [ ] **M1. Deadzone is a step, not a ramp.** `MotionController::compute`
  zeroes the filter state inside the deadzone and applies `hardZero` again on
  output, so each axis jumps from 0 straight to >= 16/20 units and the
  low-pass restarts from zero every re-entry. Produces the cliff-edge feel and
  the "press hard then it shoots" symptom in upstream issue 43.
  Fix: `sign(v) * (|v| - dead) * limit / (limit - dead)`, filter continuously.

- [ ] **M2. Axis bleed is inherent to the linear model.** Known and documented
  by the author. Candidates: calibration matrix (upstream PR 44) or dipole
  model + EKF (upstream PR 56, RP2350 preferred).

- [ ] **M3. Calibration has no sanity check.** Averages 200 samples and accepts
  anything. Reject on high variance; signal with LED colour.

- [ ] **M4. No baseline drift compensation.** Temperature is already read and
  discarded. Re-zero slowly while in Sleep or when all axes are quiet.

## Smaller items

- [ ] Sleep only turns off LEDs; sensors stay powered and polled. Matters once
  there is a battery (wireless phase).
- [ ] Double-precision math (`sqrt(3.0)`, `fabs`, `3.0` literals) on a core
  with no FPU. Use `sqrtf`, `fabsf`, float literals, `constexpr` geometry.
- [ ] HID descriptor hard-codes LOGICAL_MIN/MAX = +/-350; must track
  `Config::AXIS_LIMIT`. Add a `static_assert` or cross-comment.
- [ ] `MotionController` is pure and testable; add a PlatformIO `[env:native]`
  unit test feeding synthetic sensor vectors. `firmware/test/` is empty.
- [x] `getMagneticFieldAndTemperature` fetches unused temperature bytes each
  loop; switch to field-only read unless M4 is implemented. (done with B3;
  switch back to the temperature read if M4 is implemented)

## Suggested order

1. B3 (makes a bad board obvious instead of mysterious) - do before first run.
2. B1 (ten-line change, no behaviour change otherwise).
3. B4 off for daily-use builds, on for bring-up.
4. M1, then native tests, then evaluate M2.
5. B2, M3, M4, smaller items as time allows.
