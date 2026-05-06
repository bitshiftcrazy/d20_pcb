/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef CHARLIEPLEX_H
#define CHARLIEPLEX_H

#include <stdint.h>

/* Initialize the charlieplex GPIOs and leave all lines in the all-off state. */
void charlieplex_init(void);

/* Turn off all charlieplex outputs. */
void charlieplex_all_off(void);

/*
 * Drive one logical LED by index.
 *
 * led_index must be in the range 0..19.
 * Out-of-range indices are ignored.
 *
 * This function only sets the current output state, it does not hold the LED
 * on for any duration.
 */
void charlieplex_led_on(uint8_t led_index);

#endif /* CHARLIEPLEX_H */
