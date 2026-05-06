/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize button GPIO and internal debounce state. */
void button_init(void);

/* Update button state, call regularly from the main loop. */
void button_update(uint32_t now_ms);

/* Return the current debounced button state. */
bool button_down(void);

/* Edge events: valid until the next button_update() call. */
bool button_pressed(void);
bool button_released(void);

/* Long-press event: latched until consumed by button_long(). */
bool button_long(void);

#endif /* BUTTON_H */
