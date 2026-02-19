# Brain CV Utils

A multi-mode CV utility firmware for the [Brain](https://github.com/shmoergh/brain-sdk) Eurorack module.

## Modes

The firmware has 4 modes:

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
| Pot 3 | Fine tune — ±1 semitone, applied to both channels |
| CV In A/B | Input signals (1V/oct) |
| CV Out A/B | Input + offset |
| LEDs 1–3 | CH1 offset bar (direction = +/−, brightness = magnitude) |
| LEDs 4–6 | CH2 offset bar (same as above) |
### 3. Slew Limiter *(coming soon)*
### 4. AD Envelope *(coming soon)*

## Switching Modes

1. **Hold both buttons** — enters mode select (LEDs 1–4 show current mode)
2. **Turn Pot 1** — selects mode (pot range divided into 4 zones)
3. **Release both buttons** — confirms selection, LEDs return to VU meters

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
