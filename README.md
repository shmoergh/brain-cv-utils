# Brain CV Utils

A multi-mode CV utility firmware for the [Brain](https://github.com/shmoergh/brain-sdk) Eurorack module.

## Modes

The firmware has 4 modes, indicated by LEDs 1–4:

### 1. Attenuverter (default)
Dual-channel attenuverter with DC offset.

| Control | Function |
|---------|----------|
| Pot 1 | CH1 attenuation — center = mute, left = invert, right = pass |
| Pot 2 | CH2 attenuation — same as above |
| Pot 3 | DC offset — adds/subtracts voltage to both channels |
| CV In A/B | Input signals |
| CV Out A/B | Processed signals |
| LEDs 5–6 | Output level for CH1/CH2 |

### 2. Precision Adder *(coming soon)*
### 3. Slew Limiter *(coming soon)*
### 4. AD Envelope *(coming soon)*

## Switching Modes

1. **Hold Button B** — enters mode select (current mode LED lit)
2. **Tap Button A** — cycles through modes (LED follows)
3. **Release Button B** — confirms selection

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
