/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* WORN TAPE's engine: the loop that decays.
 *
 * You record a short loop of notes (REC, then the keys; LOOP bars long on a 1/8 grid). That is the MASTER. The
 * machine plays it on a tape: every GEN BARS passes is a new generation, and the tape's AGE goes up by one. The
 * notes you hear at age N are a function of (master, seed, N), computed again whenever N changes, so SELECT can
 * run the age both ways:
 *   erosion    each generation, each living note drops with chance EROSION (a dropped note of the master is a
 *              ghost: it may come back)
 *   regrowth   each generation, each empty step may grow a quiet new note from the scale, near its neighbours
 *   memory     every EVERY generations, each ghost of the master comes back with chance MEMORY (the master
 *              reasserting itself; with MEMORY 0 the loop never returns)
 * The tape itself ages with N too: the highs go (HF LOSS, by MEDIUM), dropouts come more often, the hiss rises,
 * saturation thickens. WOBBLE is wow and flutter, SAG a drift in the motor's speed over minutes. OCT-/OCT+ run the
 * tape at half or double speed: pitch and tempo together, as a tape machine does. A second, slower tape (TAPE 2)
 * plays a thinned copy of the same notes an octave down, so the two phase against each other. SPLICES (SEQ held, a
 * black key) are points on the loop: where the playhead crosses one, the tape repeats, skips or reverses a beat.
 *
 * Threads: en_render runs in the audio interrupt; the UI posts commands (en_set and the rest), which the render
 * takes at the start of a block. The state marked (UI) is written by the render and read by the screen. The master
 * (en_master) is the render's; the UI reads it to save (while not recording) and hands a loaded one over with
 * en_master_load. */
#pragma once
#include <stdint.h>

#define EN_MAX_NOTES 96                /* the master's notes, and the aged set's */
#define EN_MAX_STEPS 64                /* 8 bars of 1/8 */
#define EN_NVOICES 8
#include "hdsp.h"
extern const hp_t EN_PARAMS[];           /* every parameter: name, range, default, value names */

enum {
    P_MEDIUM, P_INSTR, P_AGE,                           /* ALGORITHM, PRESETS, SELECT */
    P_EROSION, P_REGROWTH, P_WOBBLE, P_DUST,            /* HOME */
    P_ATTACK, P_GHOST, P_RELEASE, P_TONE,               /* ENV */
    P_SAG, P_SAGRATE, P_SPEEDUP, P_FLUTTER,             /* LFO */
    P_TEMPO, P_LOOPBARS, P_QUANT, P_GENPASS,            /* SEQ */
    P_TAPE2, P_RATIO, P_T2LEVEL, P_T2OCT,               /* ARP */
    P_SAT, P_HFLOSS, P_DROPOUT, P_REVERB,               /* FX */
    P_MEMORY, P_EVERY, P_RETURN, P_SPLICE,              /* GLO */
    P_ROOT, P_SCALE, P_BED, P_LIGHTS,                   /* SEL */
    P_BRIGHT, P_DECAY, P_DETUNE, P_WIDTH,               /* EDIT: the instrument */
    P_SPEED,                                            /* OCT-/OCT+: tape speed in octaves, -2..1 */
    P_N
};
enum { MED_CASSETTE, MED_REEL, MED_DICTAPHONE, MED_VHS, MED_CYLINDER, MED_CLEAN, MED_N };
enum { INS_FELT, INS_MUSICBOX, INS_PAD, INS_CHOIR, INS_GUITAR, INS_ORGAN, INS_N };
enum { SPL_REPEAT, SPL_SKIP, SPL_REVERSE };

typedef struct {
    uint8_t step;                      /* 0 .. loop_steps - 1 (1/8 notes) */
    uint8_t len;                       /* in steps, 1 .. */
    uint8_t note;                      /* MIDI */
    uint8_t vel;                       /* 1 .. 127 */
} en_note_t;

/* the aged set, for the screen: each note's state */
enum { NS_MASTER, NS_GHOST, NS_GROWN };
typedef struct {
    en_note_t n;
    uint8_t state;                     /* NS_* */
    uint8_t gen;                       /* the generation it grew in (grown) */
} en_aged_t;

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);
void en_arm(int on);                   /* REC: the next key starts the recording; it stops after LOOP bars */
void en_key(int note, int vel, int on);   /* a key: played (and recorded while recording) */
void en_commit(void);                  /* the aged set becomes the master (age 0) */
void en_clear(void);                   /* an empty master */
void en_splice(int i, int on);         /* black key i (0..10): a splice point, or not */
void en_master_load(const en_note_t *notes, int n, int loop_steps, uint32_t seed, uint32_t splices);
void en_seed(uint32_t s);
void en_panic(void);
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_STOP };
void en_clock(int msg);                /* MIDI clock: 24 a beat; while ticks come, they drive the steps */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state (UI) */
extern volatile uint8_t en_playing, en_armed, en_recording;
extern volatile uint8_t en_age;                       /* the tape's age now (follows P_AGE, +1 a generation) */
extern volatile uint8_t en_loop_steps;
extern volatile float en_pos;                         /* the playhead, in steps */
extern volatile float en_pos2;                        /* the second tape's */
extern volatile uint8_t en_dropout;                   /* a dropout is on */
extern volatile float en_level;                       /* the output's level, 0..1 */
extern volatile float en_speed;                       /* the tape's speed factor now (wow, sag, octaves) */
extern volatile en_aged_t en_aged[EN_MAX_NOTES];      /* the aged set, as the screen draws it */
extern volatile uint8_t en_naged;
extern volatile uint8_t en_nalive;                    /* how many of them sound */
extern volatile en_note_t en_master[EN_MAX_NOTES];    /* the master: read while not recording (to save) */
extern volatile uint8_t en_nmaster;
extern volatile uint32_t en_master_ver;               /* changes when the master does (recorded, committed, cleared) */
extern volatile uint32_t en_splices;                  /* bit i: black key i is a splice */
extern volatile uint32_t en_seed_now;
extern volatile uint8_t en_splice_glow;               /* a splice just fired: its index + 1 */
extern volatile uint8_t en_ext;                       /* MIDI clock is driving */
extern volatile uint32_t en_steps_total;              /* steps played since the start (tests) */
