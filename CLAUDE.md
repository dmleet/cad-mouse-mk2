# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

Open-hardware DIY 6DoF "space mouse" (CAD Mouse MK2). The repo holds firmware, PCB sources/fab outputs, enclosure CAD, and BOMs. Only `firmware/` is code; everything else (`pcbs/`, `enclosure/`, `bom/`) is binary CAD/Gerber/CSV data and should not be edited by hand. Licensed CC BY-NC-SA 4.0. Build instructions live on Instructables, not in this repo.

## Build / flash

PlatformIO project; `platformio.ini` at the repo root points `src_dir`/`include_dir`/`lib_dir`/`test_dir` into `firmware/`. Single environment: `seeed_xiao_rp2040` (RP2040, Arduino framework, earlephilhower core, TinyUSB via `-DUSE_TINYUSB`).

```sh
pio run                      # compile
pio run -t upload            # flash XIAO RP2040 (UF2 bootloader; hold BOOT if needed)
pio device monitor -b 115200 # serial telemetry (Teleplot-style ">X:val" lines)
pio run -t clean
```

There are no tests (`firmware/test/` is empty) and no linter configured. The `platform` is the Seeed fork (`platform-seeedboards.git`), not the stock `raspberrypi` platform; this was a deliberate fix for `UnknownBoard: seeed_xiao_rp2040` on fresh installs (#12), so don't "simplify" it back.

Commented-out `usb_vid`/`usb_pid`/`usb_manufacturer` lines in `platformio.ini` are intentional: setting them to 3Dconnexion values enables vendor driver support (see README video at 6:23). Leave them commented by default.

## Firmware architecture

Runtime is a cooperative single loop: `main.cpp` calls `hidController.task()` (TinyUSB pump) then `stateMachine.update()` every iteration. No RTOS, no interrupts beyond what libraries use.

**Controllers** (`firmware/include/controllers/`, `firmware/src/controllers/`) are plain classes instantiated once as globals in `main.cpp` and exposed via `extern` in `Controllers.h`. States reach them through those globals, not injection.

- `SensorController` — three Infineon TLx493D (A2B6) 3D magnetometers on one I2C bus. All three ship with the same I2C address, so `begin()` powers them up one at a time via per-sensor load-switch pins and reassigns addresses (mag1→A2, mag2→A1, mag3 keeps A0). Order matters. Also owns calibration: averages `ZERO_SAMPLES` raw readings into a 9-float `baseline_`.
- `MotionController` — pure math: raw[9] minus baseline → 6 axes (Tx,Ty,Tz,Rx,Ry,Rz) → sign/gain → deadzone → low-pass → clamp to `AXIS_LIMIT`. The formulas and sensor geometry (mag1 bottom, mag2 top-left, mag3 top-right) are documented in `firmware/README.md`. This is the known-weak part of the project (axis bleed, assumes linear sensor response); the author explicitly invites replacement of this module.
- `HIDController` — custom HID report descriptor: report ID 1 = six int16 axes (±350, must match `AXIS_LIMIT`), report ID 3 = 2 buttons. Only sends when values change. If you change `AXIS_LIMIT`, update the `LOGICAL_MIN/MAX` bytes in the descriptor too.
- `InputController` — two buttons via AceButton; tracks pressed state, an "activity" flag (consumed with `takeActivity()`), and a calibration request fired by holding both buttons 3 s (`takeCalibrationRequest()`). Both are one-shot "take" semantics.
- `LEDController` — 8-pixel NeoPixel ring behind a load switch; modes Off/Solid/Spinner.
- `TelemetryController` — serial prints every 5th frame when `Config::ENABLE_TELEMETRY` is true.

**State machine** (`StateMachine.h/.cpp`, `firmware/include/states/`): classic enter/update/exit `State` interface with static singleton instances on `StateMachine`.

- `Calibrating` (boot entry, blue spinner) → `Idle` when `calibrationDone()`.
- `Idle` (green solid) runs the full pipeline each tick: read raw → compute motion → send HID → telemetry. Transitions to `Calibrating` on button-hold request, to `Sleep` after `IDLE_SLEEP_TIMEOUT_MS` with no motion or button activity.
- `Sleep` (LEDs off) → `Idle` on any button activity. Note: sleep does not stop sensor power or USB; it only turns off LEDs and stops the motion pipeline.

**Tuning** lives entirely in `firmware/include/Config.h` (pins, gains, signs, deadzones, smoothing tau, LED colours, sleep timeout). Prefer changing constants there over hard-coding in controllers.

## Local working notes

`docs/firmware-review.md` holds the findings of a full firmware review with a status checklist, and `docs/roadmap.md` the phased build plan (wired first, 3DxWare driver in SolidWorks/Fusion, then wireless via nRF52840 dongle). Update the checkboxes there when a finding is fixed or a phase gate is passed. Neither file exists upstream.
