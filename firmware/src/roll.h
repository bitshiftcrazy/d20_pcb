/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2025 miss.molerat <bitshiftcrazy@posteo.de>
 */

#ifndef ROLL_H
#define ROLL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t value;          /* Final roll result */
    bool is_nat1;
    bool is_nat20;
    bool used_godmode;
    uint8_t nat1_count;
    uint8_t nat20_count;
} roll_result_t;

/* Initialize PRNG state and reset session counters / godmode state. */
void roll_init(uint16_t seed);

/* Mix additional entropy into the current PRNG state. */
void roll_seed_entropy(uint16_t entropy);

/* Track one button press for godmode activation timing. */
void roll_note_button_press(uint32_t now_ms);

/* Return whether godmode is currently active. */
bool roll_godmode_active(void);

/* Disable godmode and clear the current tap sequence. */
void roll_disable_godmode(void);

/* Perform one roll and return the result plus updated session counters. */
roll_result_t roll_perform(void);

/* The dice has a serious issue with you. */
bool roll_upset_active(void);

/* Random silent treatment. */
bool roll_upset_should_ignore_input(void);

/* Random mini tantrums. */
bool roll_upset_should_glitch_animation(void);

#endif /* ROLL_H */
