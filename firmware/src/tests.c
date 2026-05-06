/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "adc.h"
#include "button.h"
#include "charlieplex.h"
#include "config.h"
#include "pins.h"
#include "timer.h"
#include "touch.h"
#include "attunement.h"
#include "sequence.h"
#include "tests.h"

#define TEST_LED_ON_MS             200
#define TEST_LED_OFF_MS             60
#define TEST_LOOP_PAUSE_MS         400

#define TEST_BLINK_MS              150
#define TEST_SUCCESS_BLINKS         5U
#define TEST_STATUS_PAUSE_MS       800

#define TOUCH_RANGE1_MIN             0U
#define TOUCH_RANGE1_MAX           299U

#define TOUCH_RANGE2_MIN           300U
#define TOUCH_RANGE2_MAX           599U

#define TOUCH_RANGE3_MIN           600U
#define TOUCH_RANGE3_MAX          1023U

#define TOUCH_EVENT_HOLD_MS        200UL

#define ADC_TEST_SAMPLES             8U
#define ADC_MAX_VALUE             1023U

static uint8_t test_last_led_index(void)
{
    return (uint8_t)(LED_COUNT - 1U);
}

static bool test_in_range(uint16_t value, uint16_t min, uint16_t max)
{
    return (value >= min) && (value <= max);
}

static void test_scan_led_span(uint8_t first_led, uint8_t last_led,
                               uint32_t now_ms)
{
    uint8_t count;
    uint8_t offset;
    uint8_t led;

    if (last_led < first_led) {
        charlieplex_all_off();
        return;
    }

    count = (uint8_t)(last_led - first_led + 1U);
    offset = (uint8_t)(now_ms % count);
    led = (uint8_t)(first_led + offset);

    charlieplex_led_on(led);
}

static uint8_t test_adc_value_to_led(uint16_t adc_value)
{
    uint32_t scaled;

    /*
     * Map 0..1023 to 0..(LED_COUNT - 1).
     * Use integer math with multiply first to keep resolution.
     */
    scaled = (uint32_t)adc_value * (uint32_t)LED_COUNT;
    scaled /= (uint32_t)(ADC_MAX_VALUE + 1U);

    if (scaled >= LED_COUNT) {
        scaled = LED_COUNT - 1U;
    }

    return (uint8_t)scaled;
}

static void test_blink_led(uint8_t led, uint8_t count)
{
    uint8_t i;

    for (i = 0U; i < count; i++) {
        charlieplex_led_on(led);
        _delay_ms(TEST_BLINK_MS);

        charlieplex_all_off();
        _delay_ms(TEST_BLINK_MS);
    }
}

static uint8_t test_attunement_value_to_led(uint8_t value)
{
    uint32_t scaled;

    if (value <= ATTUNEMENT_MIN_VALUE) {
        return 0U;
    }

    if (value >= ATTUNEMENT_MAX_VALUE) {
        return test_last_led_index();
    }

    scaled = (uint32_t)(value - ATTUNEMENT_MIN_VALUE) * LED_COUNT;
    scaled /= (uint32_t)(ATTUNEMENT_MAX_VALUE - ATTUNEMENT_MIN_VALUE + 1U);

    if (scaled >= LED_COUNT) {
        return test_last_led_index();
    }

    return (uint8_t)scaled;
}

void test_led_walk(void)
{
    uint8_t led;

    while (1) {
        for (led = 0U; led < LED_COUNT; led++) {
            charlieplex_led_on(led);
            _delay_ms(TEST_LED_ON_MS);

            charlieplex_all_off();
            _delay_ms(TEST_LED_OFF_MS);
        }

        _delay_ms(TEST_LOOP_PAUSE_MS);
    }
}

void test_button_toggle_last_led(void)
{
    uint8_t led_on = 0U;
    uint8_t led = test_last_led_index();

    charlieplex_all_off();

    while (1) {
        button_update(millis());

        if (button_pressed()) {
            led_on ^= 1U;
        }

        if (led_on) {
            charlieplex_led_on(led);
        } else {
            charlieplex_all_off();
        }
    }
}

void test_button_hold_last_led(void)
{
    uint8_t led = test_last_led_index();

    charlieplex_all_off();

    while (1) {
        button_update(millis());

        if (button_down()) {
            charlieplex_led_on(led);
        } else {
            charlieplex_all_off();
        }
    }
}

void test_adc_touch_range_calibration(void)
{
    uint32_t now_ms;
    uint16_t adc_value;

    while (1) {
        now_ms = millis();
        adc_value = adc_read_avg(TOUCH_ADC_CHANNEL, ADC_TEST_SAMPLES);

        if (test_in_range(adc_value, TOUCH_RANGE1_MIN, TOUCH_RANGE1_MAX)) {
            /* LEDs 2..7 -> indices 1..6 */
            test_scan_led_span(1U, 6U, now_ms);
        } else if (test_in_range(adc_value, TOUCH_RANGE2_MIN, TOUCH_RANGE2_MAX)) {
            /* LEDs 11..16 -> indices 10..15 */
            test_scan_led_span(10U, 15U, now_ms);
        } else if (test_in_range(adc_value, TOUCH_RANGE3_MIN, TOUCH_RANGE3_MAX)) {
            /* LEDs 8..17 -> indices 7..16 */
            test_scan_led_span(7U, 16U, now_ms);
        } else {
            charlieplex_all_off();
        }
    }
}

void test_touch_detect_last_led(void)
{
    uint32_t now_ms;
    uint32_t lit_until_ms = 0UL;
    uint8_t led = test_last_led_index();

    charlieplex_all_off();

    while (1) {
        now_ms = millis();

        touch_update();

        if (touch_triggered()) {
            lit_until_ms = now_ms + TOUCH_EVENT_HOLD_MS;
        }

        if ((int32_t)(lit_until_ms - now_ms) > 0) {
            charlieplex_led_on(led);
        } else {
            charlieplex_all_off();
        }
    }
}

void test_adc_band_finder(void)
{
    uint16_t adc_value;
    uint8_t led;

    while (1) {
        adc_value = adc_read_avg(TOUCH_ADC_CHANNEL, ADC_TEST_SAMPLES);
        led = test_adc_value_to_led(adc_value);
        charlieplex_led_on(led);
    }
}

void test_attunement_button_store(void)
{
    uint8_t value;
    uint8_t value_led;
    uint8_t success_led = test_last_led_index();

    charlieplex_all_off();

    attunement_init();

    while (1) {
        button_update(millis());

        if (button_pressed()) {
            /*
             * Button triggers re-attunement.
             * Do NOT clear EEPROM first. The store path should overwrite/update it.
             */
            value = attunement_measure_and_store();

            /*
             * Sanity reload from EEPROM immediately after writing.
             */
            if (!attunement_load()) {
                while (1) {
                    test_blink_led(0U, 1U);
                    _delay_ms(TEST_STATUS_PAUSE_MS);
                }
            }

            if (!attunement_has_value()) {
                while (1) {
                    test_blink_led(0U, 2U);
                    _delay_ms(TEST_STATUS_PAUSE_MS);
                }
            }

            if (attunement_get_value() != value) {
                while (1) {
                    test_blink_led(0U, 3U);
                    _delay_ms(TEST_STATUS_PAUSE_MS);
                }
            }

            /*
             * Success: blink LED 20.
             */
            test_blink_led(success_led, TEST_SUCCESS_BLINKS);
            charlieplex_all_off();
        }

        /*
         * Idle status display:
         *
         * - If attunement exists, map its stored value to an LED.
         * - If no attunement exists, stay dark.
         */
        if (attunement_has_value()) {
            value = attunement_get_value();
            value_led = test_attunement_value_to_led(value);
            charlieplex_led_on(value_led);
        } else {
            charlieplex_all_off();
        }
    }
}

void test_sequence_mode(void)
{
    sequence_init();
    sequence_enter();

    while (1) {
        button_update(millis());
        sequence_update(millis());
    }
}

void test_touch_long(void)
{
    touch_init();
    charlieplex_init();

    while (1) {
        touch_update();

        if (touch_long()) {
			charlieplex_led_on(19U);

            while (1) {
			}
        }
    }
}
