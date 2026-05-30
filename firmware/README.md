# d20 PCB Firmware

Firmware for the d20 PCB project.

This firmware runs on an **ATtiny84** and controls the charlieplexed LEDs,
button input, capacitive touch input, EEPROM-backed attunement state, dice
roll logic, animations, and some easter eggs.

The following instructions assume firmware is built and flashed from Linux.

## Hardware Requirements

To program the board, you need:

- d20 PCB with ATtiny84 (see [hardware/](../hardware))
- ISP programmer, for example USBasp
- ISP connection to the board
- Linux machine with AVR toolchain installed

The Makefile currently assumes a USBasp programmer.

## Software Requirements

Install the AVR toolchain.

### Arch Linux

```sh
sudo pacman -S avr-gcc avr-libc avrdude make
```

### Debian / Ubuntu

```sh
sudo apt install gcc-avr avr-libc avrdude make
```

## Building

From the repository root:

```sh
cd firmware
make
```

This builds the default firmware and prints AVR memory usage.

Build output is written to:

```text
build/
```

The generated firmware image is:

```text
build/d20_firmware_4.hex
```

The default character ID is currently `4`.

## Character Selection

Since the project originates from a gift for my DnD players, the firmware
supports different character-specific builds through `CHARACTER_ID`.

```sh
make CHARACTER_ID=4
```

Known character IDs:

```text
1 = ASHTAN
2 = BARTHOLOMEW
3 = FAWN
4 = SALEM
5 = VII
```

Example:

```sh
make flash CHARACTER_ID=2
```

## Fuse Bits

Before flashing the firmware for the first time, program the ATtiny84 fuse
bits:

```sh
make fuses
```

This configures the microcontroller to use the 8 MHz clock settings expected by the firmware.

Fuse bits only need to be programmed once per microcontroller.

## Flashing

To build and flash the firmware with a USBasp programmer:

```sh
make flash
```

This uses:

```text
avrdude -c usbasp -p t84 -B 10
```

To flash a different character firmware:

```sh
make flash CHARACTER_ID=5
```

## Diagnostic Test Firmware

The firmware includes small functional and diagnostic test builds.

These are not full unit tests. They are intended to flash special test
firmware onto the board to verify individual hardware or firmware features.

Build a test firmware without flashing:

```sh
make test TEST_CASE=attunement
```

Build and flash a selected test firmware:

```sh
make flash-test TEST_CASE=adc_band_finder
```

Available test cases:

```text
led_walk
button_toggle
button_hold
adc_range
touch_detect
touch_long
adc_band_finder
attunement
sequence
```

Shortcut targets are also available:

```sh
make test-led-walk
make test-button-toggle
make test-button-hold
make test-adc-range
make test-touch-detect
make test-touch-long
make test-adc-band-finder
make test-attunement
make test-sequence
```

## EEPROM Helpers

Read the ATtiny84 EEPROM into `build/eeprom.hex`:

```sh
make eeprom-read
```

Read and print EEPROM contents:

```sh
make eeprom-view
```

This is useful for checking attunement state and debugging persistent firmware
state.

## Cleaning

Remove build output:

```sh
make clean
```

## Makefile Overview

Useful targets:

```text
make all          Build firmware hex and show size
make flash        Build and flash normal firmware
make test         Build diagnostic test firmware
make flash-test   Build and flash diagnostic test firmware
make size         Show AVR memory usage
make eeprom-read  Read EEPROM into build/eeprom.hex
make eeprom-view  Read and print EEPROM contents
make clean        Remove build output
make help         Show Makefile help
```

Useful options:

```text
CHARACTER_ID=N    Select character firmware variant
TEST=1            Build test firmware instead of normal firmware
TEST_CASE=name    Select diagnostic test case
```

## Firmware Configuration and Tuning

The main configuration is done in `src/config.h`.

Touch sensing can be adjusted in `src/touch.c`.

You can use the diagnostic test firmware targets:

- `test-adc-range`
- `test-adc-band-finder`

to inspect and tune touch detection thresholds for your hardware.

## Hardware Compatibility

This firmware is intended for the d20 PCB hardware revisions developed in
this repository.

If using modified hardware revisions, verify:

- LED pin mappings
- touchpad routing
- ISP wiring
- EEPROM layout assumptions

before flashing the firmware.

## License

This firmware is licensed under GPL-3.0-only.

See `LICENSE` for details.