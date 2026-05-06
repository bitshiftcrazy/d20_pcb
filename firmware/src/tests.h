/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef TESTS_H
#define TESTS_H

/*
 * Hardware bring-up and functional diagnostics.
 *
 * These are not unit tests. Each test firmware exercises one specific board
 * feature directly on target hardware and usually runs forever.
 */

/* Cycle through all LEDs sequentially. */
void test_led_walk(void);

/* Toggle last LED on button press (edge). */
void test_button_toggle_last_led(void);

/* Light last LED while button is held (level). */
void test_button_hold_last_led(void);

/* Map ADC value into fixed LED ranges. */
void test_adc_touch_range_calibration(void);

/* Light last LED on touch detection event. */
void test_touch_detect_last_led(void);

/* Hold the touch pad long enough to light LED20. */
void test_touch_long(void);

/* Map raw ADC value directly to LED index. */
void test_adc_band_finder(void);

/* Load, display, and update attunement via EEPROM. */
void test_attunement_button_store(void);

void test_sequence_mode(void);

#endif /* TESTS_H */
