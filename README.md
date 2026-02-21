# Brain CV Utils

A multi-mode CV utility firmware for the [Brain](https://github.com/shmoergh/brain-sdk) Eurorack module.

## Modes

The firmware has 6 modes:

### 1. Attenuverter (default)
Dual-channel attenuverter with DC offset.

| Control | Function |
|---------|----------|
| Pot 1 | CH1 attenuation — center = mute, left = invert, right = pass |
| Pot 2 | CH2 attenuation — same as above |
| Pot 3 | DC offset — adds/subtracts voltage to both channels |
| CV In A/B | Input signals |
| CV Out A/B | Processed signals |
| LEDs 1–3 | CH1 output magnitude VU (distance from 5V center) |
| LEDs 4–6 | CH2 output magnitude VU (distance from 5V center) |

### 2. Precision Adder
Add precise voltage offsets for octave transposition.

| Control | Function |
|---------|----------|
| Pot 1 | CH1 octave offset — quantized −4V to +4V in 1V steps |
| Pot 2 | CH2 octave offset — same as above |
| Pot 3 | Fine tune — ±5 semitones, applied to both channels |
| CV In A/B | Input signals (1V/oct) |
| CV Out A/B | Input + offset (with calibration applied) |
| LEDs 1–3 | CH1 output magnitude VU |
| LEDs 4–6 | CH2 output magnitude VU |

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

### 4. AD Envelope
Dual attack-decay envelope generator with gate/manual/pulse triggering.

| Control | Function |
|---------|----------|
| Pot 1 | Attack time (1ms–~5s, logarithmic) |
| Pot 2 | Decay time (1ms–~5s, logarithmic) |
| Pot 3 | Shape — linear (left) to exponential (right) |
| Button B | Manual trigger |
| Pulse In | Trigger input (rising edge, triggers both channels) |
| Pulse Out | End-of-cycle trigger (fires when either channel decay completes) |
| CV In A/B | Gate inputs (rising edge triggers channel A/B) |
| CV Out A/B | Envelope outputs (independent per channel) |
| LEDs 1–3 | CH1 output VU |
| LEDs 4–6 | CH2 output VU |

### 5. CV Mixer
Two-input mixer with per-channel level and master output control.

| Control | Function |
|---------|----------|
| Pot 1 | Input A level (0–100%) |
| Pot 2 | Input B level (0–100%) |
| Pot 3 | Main output level (0–100%) |
| CV In A/B | Input signals (AC coupled) |
| CV Out A/B | Mixed output (same signal on both) |
| LEDs 1–3 | CH1 output magnitude VU |
| LEDs 4–6 | CH2 output magnitude VU |

### 6. Noise
Clocked random CV generator with optional quantization scales.

| Control | Function |
|---------|----------|
| Pot 1 | Channel A rate (or external clock when maxed) |
| Pot 2 | Channel B rate (or external clock when maxed) |
| Pot 3 | Random range around center voltage |
| Button B (hold) | Scale select on Pot 3 (Unquantized, Chromatic, Major, Minor, Pentatonic, Whole Tone) |
| Pulse In | External clock source for channels with speed pot maxed |
| Pulse Out | Short trigger when Channel A value changes |
| CV Out A/B | Independent random voltages |
| LEDs | Random step indicator in normal mode, scale index in scale-select mode |

## Controls

### Switching Modes

**Tap Button A** — cycles through modes. LEDs briefly blink the active mode index (1–6).

### Button B

Mode-specific actions:
- **Slew Limiter**: tap to toggle linked rise/fall mode.
- **AD Envelope**: press to manually trigger both channels.
- **Noise**: hold to enter scale-select (Pot 3 chooses scale).

### Calibration Mode

**Hold A + B (long press, ~1.5s)** — enters calibration mode. All LEDs blink.

In calibration mode:
- **Pot 1** — Output A scale (gain trim)
- **Pot 2** — Output B scale (gain trim)
- **Hold Button A + Pot 3** — Output A offset
- **Hold Button B + Pot 3** — Output B offset

**Tap A + B together** — exits calibration mode (saves to flash).

Calibration values persist across power cycles and are currently applied by **Precision Adder** and **Slew Limiter**.

## Firmwares

- [Pico (RP2040) firmware](./brain-cv-utils-pico.uf2)
- [Pico 2 (RP2350) firmware](./brain-cv-utils-pico-2.uf2)

## Development

### Build

```bash
git submodule update --init --recursive
mkdir -p build && cd build
cmake ..
cmake --build .
```

Targets Pico 2 (RP2350) by default. To build for Pico 1 (RP2040), swap the `PICO_BOARD`/`PICO_PLATFORM` lines in `CMakeLists.txt`.

### Tests

Host-side math tests live in `tests/` and can be run without the Pico toolchain:

```bash
mkdir -p /tmp/brain-cv-utils-tests
c++ -std=c++17 tests/voltage_smoother_test.cpp -o /tmp/brain-cv-utils-tests/voltage_smoother_test
/tmp/brain-cv-utils-tests/voltage_smoother_test
c++ -std=c++17 tests/slew_limiter_math_test.cpp -o /tmp/brain-cv-utils-tests/slew_limiter_math_test
/tmp/brain-cv-utils-tests/slew_limiter_math_test
```

### Flash

Hold BOOTSEL while connecting the Brain module via USB, then copy `build/brain-cv-utils.uf2` to the mounted drive.

### Debug

Uses OpenOCD + CMSIS-DAP. VSCode launch configs included for both RP2040 and RP2350.

### SDK

Brain SDK is included as a git submodule. After cloning:

```bash
git submodule update --init --recursive
```
