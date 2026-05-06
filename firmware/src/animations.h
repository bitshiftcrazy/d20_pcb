/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <stdint.h>

/* Initialize animation runtime state and seed animation-local RNG. */
void animations_init(void);

/*
 * Background god mode pulse.
 * Call regularly from the main loop while idle.
 * Non-blocking.
 */
void animations_godmode_pulse_step(uint32_t now_ms);

/*
 * Flash all LEDs on/off a number of times.
 * Blocking.
 */
void animations_flash_all(uint8_t times, uint16_t on_ms, uint16_t off_ms);

typedef enum {
    ANIM_ROLL_COMPLETED = 0,
    ANIM_ROLL_ABORT_SHORT,
    ANIM_ROLL_ABORT_LONG,
    ANIM_ROLL_INVALID_INPUT
} anim_roll_status_t;

/*
 * General roll animation:
 * - startup loop (abortable)
 * - roll-up sequence (abortable)
 * - hold result (non-abortable)
 *
 * result must be in the user-facing range 1..20.
 * Internally this is mapped to LED indices 0..19.
 *
 * Returns:
 *   ANIM_ROLL_COMPLETED    if the result was committed and shown
 *   ANIM_ROLL_ABORT_SHORT  if interrupted by a short button press
 *   ANIM_ROLL_ABORT_LONG   if interrupted by a long button press
 *   ANIM_ROLL_INVALID_INPUT if result not in range 1..20
 */
anim_roll_status_t animations_play_roll(uint8_t result);

/*
 * Attunement feedback animation:
 * - 3 clockwise rounds
 * - brief pause
 * - nat20 spectacle
 *
 * Blocking.
 */
void animations_play_attunement(void);

/*
 * Character-specific nat20 spectacle for the selected CHARACTER_ID build.
 * Blocking.
 */
void animations_play_nat20(void);

#endif /* ANIMATIONS_H */
