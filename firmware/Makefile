# SPDX-License-Identifier: GPL-3.0-only
MCU = attiny84
F_CPU = 8000000UL
CHARACTER_ID ?= 4
TEST ?= 0
TEST_CASE ?= attunement

CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
AVRDUDE = avrdude

PROGRAMMER = usbasp

COMMON_SRC = \
	src/charlieplex.c \
	src/button.c \
	src/timer.c \
	src/adc.c \
	src/touch.c \
	src/eeprom_store.c \
	src/roll.c \
	src/attunement.c \
	src/animations.c \
	src/sequence.c

APP_SRC = \
	src/main.c \
	$(COMMON_SRC)

TEST_SRC = \
	src/test_main.c \
	src/tests.c \
	$(COMMON_SRC)

ifeq ($(TEST),1)
SRC = $(TEST_SRC)
TARGET = build/d20_test_$(TEST_CASE)
CPPFLAGS = -DF_CPU=$(F_CPU) -DCHARACTER_ID=$(CHARACTER_ID) -DTEST

ifeq ($(TEST_CASE),led_walk)
CPPFLAGS += -DTEST_LED_WALK
else ifeq ($(TEST_CASE),button_toggle)
CPPFLAGS += -DTEST_BUTTON_TOGGLE
else ifeq ($(TEST_CASE),button_hold)
CPPFLAGS += -DTEST_BUTTON_HOLD
else ifeq ($(TEST_CASE),adc_range)
CPPFLAGS += -DTEST_ADC_RANGE
else ifeq ($(TEST_CASE),touch_detect)
CPPFLAGS += -DTEST_TOUCH_DETECT
else ifeq ($(TEST_CASE),touch_long)
CPPFLAGS += -DTEST_TOUCH_LONG
else ifeq ($(TEST_CASE),adc_band_finder)
CPPFLAGS += -DTEST_ADC_BAND_FINDER
else ifeq ($(TEST_CASE),attunement)
CPPFLAGS += -DTEST_ATTUNEMENT
else ifeq ($(TEST_CASE),sequence)
CPPFLAGS += -DTEST_SEQUENCE
else
$(error Unknown TEST_CASE '$(TEST_CASE)')
endif

else
SRC = $(APP_SRC)
TARGET = build/d20_firmware_$(CHARACTER_ID)
CPPFLAGS = -DF_CPU=$(F_CPU) -DCHARACTER_ID=$(CHARACTER_ID)
endif

ELF = $(TARGET).elf
HEX = $(TARGET).hex
EEPROM_HEX = build/eeprom.hex

CFLAGS = -mmcu=$(MCU) -Os -std=gnu99 -Wall -Wextra -MMD -MP
LDFLAGS = -mmcu=$(MCU)

.PHONY: all help build elf hex size flash test flash-test \
        test-led-walk test-button-toggle test-button-hold \
        test-adc-range test-touch-detect test-touch-long test-adc-band-finder \
        test-attunement \
        test-sequence \
        eeprom-read eeprom-view clean

all: hex size

help:
	@echo "Targets:"
	@echo "	 make all                         Build hex and show size"
	@echo "	 make flash CHARACTER_ID=N        Build and flash selected character firmware"
	@echo "	 make test                        Build test firmware using TEST_CASE"
	@echo "	 make flash-test                  Build and flash test firmware using TEST_CASE"
	@echo "	 make size                        Show AVR memory usage"
	@echo "	 make eeprom-read                 Read EEPROM into build/eeprom.hex"
	@echo "	 make eeprom-view                 Read and print EEPROM contents"
	@echo "	 make clean                       Remove build output"
	@echo ""
	@echo "Test shortcut targets:"
	@echo "	 make test-led-walk               Flash LED walk test"
	@echo "	 make test-button-toggle          Flash button toggle test"
	@echo "	 make test-button-hold            Flash button hold test"
	@echo "	 make test-adc-range              Flash ADC range calibration test"
	@echo "	 make test-touch-detect           Flash touch detection test"
	@echo "	 make test-touch-long             Flash long touch test"
	@echo "	 make test-adc-band-finder        Flash ADC band finder test"
	@echo "	 make test-attunement             Flash EEPROM attunement test"
	@echo "	 make test-sequence               Flash sequence mode test"
	@echo ""
	@echo "Options:"
	@echo "	 TEST=0                           Normal firmware build (default)"
	@echo "	 TEST=1                           Test build using src/test_main.c"
	@echo "	 TEST_CASE=attunement             Selected test case"
	@echo ""
	@echo "Valid TEST_CASE values:"
	@echo "	 led_walk"
	@echo "	 button_toggle"
	@echo "	 button_hold"
	@echo "	 adc_range"
	@echo "	 touch_detect"
	@echo "	 touch_long"
	@echo "	 adc_band_finder"
	@echo "	 attunement"
	@echo "	 sequence"
	@echo ""
	@echo "Examples:"
	@echo "	 make flash"
	@echo "	 make flash-test TEST_CASE=adc_band_finder"
	@echo "	 make test-attunement"
	@echo ""
	@echo "Character IDs:"
	@echo "	 1 = ASHTAN"
	@echo "	 2 = BARTHOLOMEW"
	@echo "	 3 = FAWN"
	@echo "	 4 = SALEM"
	@echo "	 5 = VII"

build:
	mkdir -p build

elf: build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(ELF)

hex: elf
	$(OBJCOPY) -O ihex -R .eeprom $(ELF) $(HEX)

size: elf
	$(SIZE) -C --mcu=$(MCU) $(ELF)

flash: hex
	@echo "Flashing $(HEX)"
	$(AVRDUDE) -c $(PROGRAMMER) -p t84 -B 10 -U flash:w:$(HEX):i

test:
	$(MAKE) TEST=1 TEST_CASE=$(TEST_CASE) all

flash-test:
	$(MAKE) TEST=1 TEST_CASE=$(TEST_CASE) flash

test-led-walk:
	$(MAKE) TEST=1 TEST_CASE=led_walk flash

test-button-toggle:
	$(MAKE) TEST=1 TEST_CASE=button_toggle flash

test-button-hold:
	$(MAKE) TEST=1 TEST_CASE=button_hold flash

test-adc-range:
	$(MAKE) TEST=1 TEST_CASE=adc_range flash

test-touch-detect:
	$(MAKE) TEST=1 TEST_CASE=touch_detect flash

test-touch-long:
	$(MAKE) TEST=1 TEST_CASE=touch_long flash

test-adc-band-finder:
	$(MAKE) TEST=1 TEST_CASE=adc_band_finder flash

test-attunement:
	$(MAKE) TEST=1 TEST_CASE=attunement flash

test-sequence:
	$(MAKE) TEST=1 TEST_CASE=sequence flash

eeprom-read: build
	$(AVRDUDE) -c $(PROGRAMMER) -p t84 -B 10 -U eeprom:r:$(EEPROM_HEX):i

eeprom-view: eeprom-read
	cat $(EEPROM_HEX)

clean:
	rm -rf build

-include build/*.d
