/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#include <avr/interrupt.h>

#include "adc.h"
#include "button.h"
#include "charlieplex.h"
#include "timer.h"
#include "touch.h"
#include "sequence.h"
#include "tests.h"

int main(void)
{
    charlieplex_init();
    adc_init();
    button_init();
    touch_init();
    timer_init();
    sei();

#if defined(TEST_LED_WALK)
    test_led_walk();
#elif defined(TEST_BUTTON_TOGGLE)
    test_button_toggle_last_led();
#elif defined(TEST_BUTTON_HOLD)
    test_button_hold_last_led();
#elif defined(TEST_ADC_RANGE)
    test_adc_touch_range_calibration();
#elif defined(TEST_TOUCH_DETECT)
    test_touch_detect_last_led();
#elif defined(TEST_TOUCH_LONG)
    test_touch_long();
#elif defined(TEST_ADC_BAND_FINDER)
    test_adc_band_finder();
#elif defined(TEST_ATTUNEMENT)
    test_attunement_button_store();
#elif defined(TEST_SEQUENCE)
    test_sequence_mode();
#else
# error "No test selected"
#endif

    return 0;
}
