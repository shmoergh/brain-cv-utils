# Brain CV Utils

A multi-mode CV utility firmware for the [Brain](https://github.com/shmoergh/brain-sdk) Eurorack module.

## Modes

The firmware has 5 modes:

### 1. Attenuverter (default)
Dual-channel attenuverter with DC offset.

| Control | Function |
|---------|----------|
| Pot 1 | CH1 attenuation — center = mute, left = invert, right = pass |
| Pot 2 | CH2 attenuation — same as above |
| Pot 3 | DC offset — adds/subtracts voltage to both channels |
| CV In A/B | Input signals |
| CV Out A/B | Processed signals |
| LEDs 1–3 | CH1 VU meter (positive fills left→right, negative fills right→left) |
| LEDs 4–6 | CH2 VU meter (same as above) |

### 2. Precision Adder
Add precise voltage offsets for octave transposition.

| Control | Function |
|---------|----------|
| Pot 1 | CH1 octave offset — quantized −4V to +4V in 1V steps |
| Pot 2 | CH2 octave offset — same as above |
| Pot 3 | Fine tune — ±5 semitones, applied to both channels |
| CV In A/B | Input signals (1V/oct) |
| CV Out A/B | Input + offset (with calibration applied) |
| LEDs 1–3 | CH1 offset bar (direction = +/−, brightness = magnitude) |
| LEDs 4–6 | CH2 offset bar (same as above) |

### 3. Slew Limiter
Smooth out CV transitions with independent rise and fall times.

| Control | Function |
|---------|----------|
| Pot 1 | Rise time (0ms–~2s, logarithmic) |
| Pot 2 | Fall time (0ms–~2s, logarithmic) |
| Pot 3 | Shape — linear (left) to exponential (right) |
| Button B | Toggle linked mode (Pot 1 controls both rise and fall) |
| CV In A/B | Input signals |
| CV Out A/B | Slewed signals |
| LEDs 1–3 | CH1 output VU |
| LEDs 4–6 | CH2 output VU |
### 4. AD Envelope *(coming soon)*

### 5. CV Mixer
Two-input mixer with per-channel level and master output control.

| Control | Function |
|---------|----------|
| Pot 1 | Input A level (0–100%) |
| Pot 2 | Input B level (0–100%) |
| Pot 3 | Main output level (0–100%) |
| CV In A/B | Input signals (AC coupled) |
| CV Out A/B | Mixed output (same signal on both) |
| LEDs 1–3 | Input A contribution VU |
| LEDs 4–6 | Input B contribution VU |

## Controls

### Switching Modes

**Tap Button A** — cycles through modes. LED briefly shows current mode (1–5).

### Button B

Reserved for per-mode features (e.g. DC/AC coupling toggle, linked mode).

### Calibration Mode

**Hold A + B (long press, ~1.5s)** — enters calibration mode. All LEDs blink.

In calibration mode:
- **Pot 1** — Output A scale (gain trim)
- **Pot 2** — Output B scale (gain trim)
- **Hold Button A + Pot 3** — Output A offset
- **Hold Button B + Pot 3** — Output B offset

**Tap A + B together** — exits calibration mode (saves to flash).

Calibration values persist across power cycles and apply to all modes that use CV passthrough (Precision Adder, Slew Limiter).

## Build

```bash
mkdir build && cd build
cmake .. -G "Unix Makefiles"
make
```

Targets Pico 2 (RP2350) by default. To build for Pico 1 (RP2040), swap the `PICO_BOARD`/`PICO_PLATFORM` lines in `CMakeLists.txt`.

## Flash

Hold BOOTSEL while connecting the Brain module via USB, then copy `build/brain-cv-utils.uf2` to the mounted drive.

## Debug

Uses OpenOCD + CMSIS-DAP. VSCode launch configs included for both RP2040 and RP2350.

## SDK

Brain SDK is included as a git submodule. After cloning:

```bash
git submodule update --init --recursive
```
