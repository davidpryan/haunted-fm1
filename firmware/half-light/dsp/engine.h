/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* HALF-LIGHT's engine: harmonic weather.
 *
 * No grid: time is in seconds. Up to eight pad voices hold a chord. Every BREATH seconds some of them (MOVEMENT)
 * move to the nearest tone of the next chord, which a Markov chain over the scale's degrees draws, weighted by
 * PRESSURE: how far from the tonic the chain may wander and how dissonant the chord may be (the VOCABULARY says
 * which qualities exist at all: triads, sevenths, add9, sus, quartal stacks, clusters). The CLIMATE is the chain's
 * transition preferences and a pressure curve: Still (it barely moves), Front (it builds over minutes and breaks:
 * a dense passage, then the voices thin to a fifth over the bass and rest), Storm (fast builds, big breaks),
 * Clearing (one long resolution), Fog (it wanders and never resolves), Manual (SELECT is the pressure). In the
 * others SELECT offsets the curve.
 *   pins     a white key pins its note: one voice holds it and never moves
 *   bells    a black key toggles a scale degree: a high FM bell on it fires now and then, timed in seconds
 *            (a Poisson-ish process), and may go into the pitched echo
 *   bass     a sub voice follows the chord's root, holds the tonic, or walks by fifths, LATE seconds after a change
 *   swell    a voice that moves is a crossfade: the old note fades as the new one rises, from a cut to a smear
 *   beat     each voice drifts in cents on a slow random walk, so the chord shimmers
 *   depth    the plate's size and feedback, and an echo whose feedback falls an octave each repeat
 *   freeze   the chord held, the pressure stopped
 *
 * Threads: en_render runs in the audio interrupt; the UI posts commands (en_set and the rest), which the render
 * takes at the start of a block. The state marked (UI) is written by the render and read by the screen. */
#pragma once
#include <stdint.h>

#define EN_NVOICES 8                   /* pad voices, pins among them */
#define EN_NBELLS 4
#define EN_NPINS 16                    /* the white keys */
#define EN_NBELLDEG 11                 /* the black keys: scale degrees 0..10 */
#include "hdsp.h"
extern const hp_t EN_PARAMS[];           /* every parameter: name, range, default, value names */

enum {
    P_CLIMATE, P_VOICE, P_PRESSURE,                     /* ALGORITHM, PRESETS, SELECT */
    P_BREATH, P_VOICES, P_MOVEMENT, P_DEPTH,            /* HOME */
    P_SWELL, P_ATTACK, P_RELEASE, P_PAD,                /* ENV */
    P_BEAT, P_WALK, P_SHIMMER, P_WIDTH,                 /* LFO */
    P_BELLEVERY, P_BELLSPREAD, P_BELLMAX, P_BELLECHO,   /* SEQ */
    P_BASS, P_LATE, P_BASSLEVEL, P_SUB,                 /* ARP */
    P_BED, P_AIR, P_HIPASS, P_DAMP,                     /* FX */
    P_SEVENTHS, P_ADD9, P_SUS, P_DENSE,                 /* GLO */
    P_ROOT, P_SCALE, P_RANGE, P_LIGHTS,                 /* SEL */
    P_BRIGHT, P_BODY, P_GRAIN, P_TONE,                  /* EDIT: the pad's timbre */
    P_OCTAVE,                                           /* OCT-/OCT+: the field's register, -2..2 */
    P_N
};
enum { CL_STILL, CL_FRONT, CL_STORM, CL_CLEARING, CL_FOG, CL_MANUAL, CL_N };
enum { VO_GLASS, VO_SAW, VO_BOWED, VO_ORGAN, VO_CHOIR, VO_STRINGS, VO_N };
enum { Q_TRIAD, Q_SEVENTH, Q_ADD9, Q_SUS, Q_QUARTAL, Q_CLUSTER, Q_N };
enum { BS_OFF, BS_ROOT, BS_TONIC, BS_FIFTHS };
enum { DN_OFF, DN_QUARTAL, DN_CLUSTER, DN_BOTH };
extern const char *const EN_QUAL_SUFFIX[Q_N];          /* "", "7", "add9", "sus", "4ths", "clus" */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);                  /* stop: the voices release over BREATH seconds */
void en_freeze(int on);                /* the chord held, the pressure stopped */
void en_pin(int key, int note);        /* white key 0..15 pins MIDI note (0: released) */
void en_bell(int deg, int on);         /* black key 0..10: a bell on that degree, or not */
void en_pins_load(const uint8_t *notes, uint32_t bells);   /* a saved set: 16 notes (0 none), the bells' mask */
void en_panic(void);
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_STOP };
void en_clock(int msg);                /* MIDI clock: while ticks come, BREATH is rounded to whole bars of it */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state (UI) */
extern volatile uint8_t en_playing, en_frozen, en_rest, en_ext;
extern volatile float en_pressure;                     /* the pressure now, 0..1 (the curve and SELECT) */
extern volatile float en_next_s;                       /* seconds to the next change */
extern volatile float en_breath_s;                     /* the interval in use (BREATH, or bars of MIDI clock) */
extern volatile uint32_t en_changes;                   /* chord changes since the start */
extern volatile uint32_t en_breaks;                    /* fronts that broke */
extern volatile uint8_t en_deg, en_qual, en_root_pc;   /* the chord: scale degree, quality, root pitch class */
extern volatile uint8_t en_nvoices;                    /* free (unpinned) voices sounding */
extern volatile uint8_t en_vnote[EN_NVOICES];          /* each voice's note (0: silent) */
extern volatile uint8_t en_vpin[EN_NVOICES];           /* each voice's pin (key + 1), 0 free */
extern volatile uint8_t en_vgen[EN_NVOICES];           /* bumps when the voice moves */
extern volatile uint8_t en_pins[EN_NPINS];             /* the pinned note of each white key (0 none) */
extern volatile uint32_t en_bells;                     /* bit d: degree d is a bell */
extern volatile uint32_t en_bells_fired;               /* bells since the start */
extern volatile uint8_t en_bell_note, en_bell_deg;     /* the last bell */
extern volatile uint8_t en_bass_note;                  /* the bass's note (0 silent) */
extern volatile float en_level;                        /* the output's level, 0..1 */
extern volatile int16_t en_lo, en_hi;                  /* the field's range, MIDI */
