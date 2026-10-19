# Development roadmap

Goal: build the wired CAD Mouse MK2, make it work through the official 3DxWare
driver in SolidWorks and Fusion, then add wireless. Findings referenced as
B1/M1/etc. are in [firmware-review.md](firmware-review.md).

## Phase 0: Parts and tooling

- [ ] Order sensor board (assembled if available). If fabricating: use the
  active-high **AP22816A** load switch, not the B variant still named in the
  schematic/BOM (upstream issues 5, 31).
- [ ] Filament: PETG Basic (spring), clear PETG (diffuser), PLA (rest).
- [x] PlatformIO Core installed, stock firmware builds (2026-10-18).
- [ ] Install 3DxWare; confirm SolidWorks add-in and Fusion panel appear.

Gate: firmware compiles locally, 3DxWare installed.

## Phase 1: Mechanical

1. Print knob sub-assembly (spring 1.0 mm first, stem, spacer, magnet holders).
   Judge stiffness; reprint 0.8 mm or thicken parametric model if needed.
2. Print remaining parts, install 16 heat-set inserts, weight the base (~250 g).
3. Dry-fit board, diffuser, buttons before closing shells.

Gate: knob returns to centre, both buttons click.

## Phase 2: Electronics bring-up

Flash with telemetry on, MCU cover off.

- Blue spinner then solid green = sensors up, calibration done.
- Serial monitor: every axis moves, correct sign, returns to zero.
- Hold both buttons 3 s: recalibration.

Gate: six clean axes on telemetry, both buttons report.

## Phase 3: Firmware hardening (local branch)

1. [x] B1 button report loss (branch `firmware-hardening`).
2. [x] B3 sensor bus error handling (branch `firmware-hardening`).
3. M1 deadzone ramp.
4. B2 wake press suppression.
5. Native unit tests for `MotionController`.
6. B4 telemetry off by default, build flag.

Gate: tests pass, no stuck buttons, no deadzone cliff.

## Phase 4: Driver integration

1. Uncomment USB identity lines in `platformio.ini`: VID 0x256F, PID from the
   build video at 6:23 (0xC631 is the usual DIY choice). Local branch only.
2. Device appears in 3DxWare settings panel.
3. SolidWorks: enable 3Dconnexion add-in, map buttons.
4. Fusion: per-app speeds in the 3Dconnexion panel.
5. Tune `Config.h` against real modelling sessions.

Gate: a full modelling session in each app without keyboard orbiting.

## Phase 5: Motion quality (only if Phase 4 feel is not good enough)

1. Calibration matrix (upstream PR 44).
2. Dipole model + EKF (upstream PR 56), RP2350 preferred.

Gate: pure translation gives no rotation on telemetry, and vice versa.

## Phase 6: Wireless

Decision: two XIAO nRF52840 radios with a USB dongle, so the Phase 4 driver
path is unchanged. No off-the-shelf dongle exists; the 3Dconnexion Universal
Receiver protocol is proprietary and undocumented.

- 6a. Transport interface in firmware; USB `HIDController` implements it.
  Do on the wired build, verify, commit.
- 6b. Real sleep: cut sensor power via existing load-switch pins, wake on
  button interrupt. Measure current on the bench first.
- 6c. Hardware: XIAO nRF52840, LiPo, charger > 100 mA, 5 V boost for LEDs.
  Parts in `bom/wireless_bom.csv`. Upstream issue 22 author has a schematic.
- 6d. Radio link (Enhanced ShockBurst or BLE), rate-limited to link interval.
- 6e. Dongle firmware on the Adafruit nRF52840 USB Key (product 5199, in
  `bom/wireless_bom.csv`; cased, TinyUF2, PlatformIO board `raytac_mdbt50q_rx`). Re-emits USB
  HID with the Phase 4 descriptor and VID/PID. Reuses the 6a USB transport.

Gate: 3DxWare sees the dongle as it saw the wired mouse; a charge lasts a
working week.
