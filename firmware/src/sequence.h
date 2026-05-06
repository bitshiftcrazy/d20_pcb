/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <stdbool.h>
#include <stdint.h>

void sequence_init(void);

void sequence_enter(void);

void sequence_update(uint32_t now_ms);

bool sequence_active(void);

#endif /* SEQUENCE_H */
