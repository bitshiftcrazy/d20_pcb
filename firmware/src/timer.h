/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Initialize Timer0 as a 1 ms system tick source. */
void timer_init(void);

/* Monotonic millisecond counter since startup. */
uint32_t millis(void);

#endif /* TIMER_H */
