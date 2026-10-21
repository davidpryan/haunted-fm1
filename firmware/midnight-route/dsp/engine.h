/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* MIDNIGHT ROUTE's engine: the arrangement engine.
 *
 * The song has a shape before it has notes. A ROUTE is 16 STATIONS (sections), LENGTH minutes long in all. The
 * SHAPE (ALGORITHM) draws a master density curve through the stations (an arch, a slow burn, a collapse...); the
 * CEILING caps it and CONTRAST pulls the quiet stations down. From that curve each of four PARTS gets its own density
 * per station: SUB (an FM bass) and RAIN (percussion over a bed) come first, PAD (four voice-led FM voices) when the
 * curve rises, VOICE (a formant voice singing a motif) only near the top. The ENV page offsets, tilts and jitters a
 * part's curve; white keys tag a station (DWELL: twice as long, SKIP: left out, PEAK: at the ceiling).
 *
 * Every bar, every part asks its density how many events to place, draws a rhythm cell (SEQ) and puts notes on it:
 * the bass on the chord's root (or a pedal, or fifths), the pad a gate, the voice a walk on the scale (ARP: range,
 * step, repeats, rests), the rain thumps and hats. Below 20 % density a part is in GESTURE mode: one phrase placed
 * somewhere in the bar with a jitter, so a sparse station sounds played, not programmed. The harmony is a chord every
 * CHANGE bars, drawn from the 11 functions the black keys allow (the PALETTE), voice-led. The material is a function
 * of (seed, pass, station, bar), so REC can BOOKMARK a station: its seed is kept and the station comes back the same
 * on the next pass. The effects read the route: the reverb opens in quiet stations, the filter closes as the density
 * falls, the tail doubles on a DWELL station, the bed swells over a SKIP. OCT-/OCT+ transpose the whole route; the
 * voice's formants stay where they are, so down is the pitched-down ghost.
 *
 * Threads: en_render runs in the audio interrupt; the UI posts commands (en_set and the rest), which the render
 * takes at the start of a block. The state marked (UI) is written by the render and read by the screen. */
#pragma once
#include <stdint.h>

#define EN_NSTATION 16
#define EN_NPAD 4                        /* the pad's voices */
#define EN_NFUNC 11                      /* the chord functions, one a black key */
#define EN_MAX_EV 32                     /* events a bar, every part */
#include "hdsp.h"
extern const hp_t EN_PARAMS[];           /* every parameter: name, range, default, value names */

enum {
    P_SHAPE, P_CITY, P_STATION,                         /* ALGORITHM, PRESETS, SELECT */
    P_LENGTH, P_CEILING, P_CONTRAST, P_FOG,             /* HOME */
    P_PART,                                             /* ENV and EDIT knob 1: the part the page edits */
    P_DRIFT, P_DRIFTRATE, P_TIMING, P_DYNAMICS,         /* LFO */
    P_CELL_SUB, P_CELL_PAD, P_CELL_VOICE, P_CELL_RAIN,  /* SEQ */
    P_RANGE, P_STEP, P_REPEAT, P_REST,                  /* ARP: the voice's motif */
    P_FILTER, P_TAIL, P_SWELL, P_ECHO,                  /* FX: the station effects */
    P_CHANGE, P_SMOOTH, P_BASS, P_END,                  /* GLO: the harmony rules */
    P_ROOT, P_SCALE, P_BED, P_TEMPO,                    /* SEL */
    P_LEVEL0, P_LEVEL1, P_LEVEL2, P_LEVEL3,             /* ENV, per part: level, slope, jitter */
    P_SLOPE0, P_SLOPE1, P_SLOPE2, P_SLOPE3,
    P_JITTER0, P_JITTER1, P_JITTER2, P_JITTER3,
    P_BRIGHT0, P_BRIGHT1, P_BRIGHT2, P_BRIGHT3,         /* EDIT, per part: bright, decay, width */
    P_DECAY0, P_DECAY1, P_DECAY2, P_DECAY3,
    P_WIDTH0, P_WIDTH1, P_WIDTH2, P_WIDTH3,
    P_TRANSPOSE,                                        /* OCT-/OCT+: semitones, -12..12 */
    P_N
};
enum { SH_ARCH, SH_BURN, SH_COLLAPSE, SH_WAVES, SH_MIDNIGHT, SH_DAWN, SH_FLAT, SH_N };
enum { PT_SUB, PT_PAD, PT_VOICE, PT_RAIN, PT_N };
enum { TAG_NORMAL, TAG_DWELL, TAG_SKIP, TAG_PEAK, TAG_N };
enum { CELL_HELD, CELL_PULSE, CELL_HALF, CELL_2STEP, CELL_SWUNG, CELL_SPARSE, CELL_ANY, CELL_N };
enum { BASS_ROOT, BASS_PEDAL, BASS_FIFTHS };
enum { END_LOOP, END_REST };
#define EN_NCITY 8

/* a city: a sound world. The UI copies root, scale, bed and tempo into the parameters when one is chosen */
typedef struct {
    const char *name;
    uint8_t root, scale, bed, tempo;
} en_city_t;
extern const en_city_t EN_CITY[EN_NCITY];
extern const char *const EN_TAG_NAME[TAG_N];
extern const char *const EN_FUNC_NAME[EN_NFUNC];
extern const char *const EN_PART_NAME[PT_N];

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);                  /* depart / pause */
void en_restart(void);                 /* from station 1, with a fade */
void en_tag(int station, int tag);     /* a white key: the station's tag */
void en_palette(uint32_t bits);        /* bit f: chord function f allowed */
void en_bookmark(int station);         /* REC: the station's material returns the same on the next pass */
void en_bookmarks_clear(void);
void en_route_load(const uint8_t *tags, uint32_t palette, const uint32_t *bookmarks, uint32_t seed);
void en_panic(void);
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_STOP };
void en_clock(int msg);                /* MIDI clock: 24 a beat; while ticks come, they drive the bars */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state (UI) */
extern volatile uint8_t en_playing, en_resting;        /* resting: the route ended (END Rest), the bed alone */
extern volatile uint8_t en_station;                    /* 0..15 */
extern volatile uint8_t en_bar, en_bars;               /* the bar in the station, and how many it has */
extern volatile float en_pos;                          /* 0..1 through the station */
extern volatile uint8_t en_dens[PT_N][EN_NSTATION];    /* each part's density per station, 0..100 */
extern volatile uint8_t en_master[EN_NSTATION];        /* the master curve (after ceiling and contrast), 0..100 */
extern volatile float en_dnow[PT_N];                   /* each part's density now (drift in), 0..1 */
extern volatile uint8_t en_tags[EN_NSTATION];
extern volatile uint32_t en_palette_bits;
extern volatile uint32_t en_bookmarks[EN_NSTATION];    /* 0: none */
extern volatile uint8_t en_nbookmarks;
extern volatile uint8_t en_chord;                      /* the chord function sounding, 0..10 */
extern volatile uint8_t en_chord_root;                 /* its root, a pitch class */
extern volatile uint32_t en_pass;                      /* passes of the route since departure */
extern volatile uint32_t en_bars_total;                /* bars played since departure (tests) */
extern volatile uint32_t en_fired;                     /* bit p: part p fired an event in the last moment */
extern volatile float en_level;                        /* the output's level, 0..1 */
extern volatile uint8_t en_ext;                        /* MIDI clock is driving */
extern volatile uint32_t en_secs_left;                 /* seconds left on the route, as the tags make it */
extern volatile uint32_t en_seed_now;
extern volatile int8_t en_voice_note;                  /* the voice's note now, -1 silent (the screen) */
