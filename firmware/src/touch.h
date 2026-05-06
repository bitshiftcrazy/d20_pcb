/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>

/* Initialize touch detection state. */
void touch_init(void);

/* Update touch detection, call regularly from the main loop. */
void touch_update(void);

/* Return and consume a pending touch event. */
bool touch_triggered(void);

/* Return and consume a pending long touch event. */
bool touch_long(void);

#endif /* TOUCH_H */
