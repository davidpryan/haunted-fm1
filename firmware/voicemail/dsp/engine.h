/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* VOICEMAIL's engine: the ghost-vocal fragment machine.
 *
 * A formant singer (hdsp.h's h_voice_t: a glottal pulse and breath through four resonances) sings PHRASES, not
 * steps. A phrase is a contour (sigh, call, question, mantra, fall, held) from the BOOK (PRESETS), rendered against
 * the scale and the pad's chord of the moment; or the MELODY you wrote with REC. SELECT is the PITCH, -24..+12
 * semitones with the formants held, so the voice drops without its throat changing: the ghost. The SINGER
 * (ALGORITHM) is who sings: the formants' shift, the breath, a radio's band.
 *
 * The form is a conversation, round and round:
 *   CALL      voice 1 sings the phrase REPEAT times (each start subject to DENSITY, bar by bar)
 *   ANSWER    with CHANCE, voice 2 sings it back DELAY bars later (from the call's start when the MODE is overlap,
 *             from its end when it waits), transposed (unison, up a 4th, down a 5th, inverted)
 *   CHORUS    both sing, half a bar apart, and the WASH opens
 *   SILENCE   the bed and the pad only, SILENCE bars (give or take); then a new phrase, a new call
 * The pad (four FM voices, voice-led through a four-chord progression, BARS a chord) runs underneath on its own
 * clock, so the voice comes back over a different chord each time.
 *
 * The cuts: FRAGMENT decides at each phrase how much of it is heard (the whole, a half, a syllable, a consonant).
 * The voice is heard through a ring buffer a fixed 163 ms behind: with STUTTER, a syllable's first slice (a 1/16
 * or a 1/32) is read again 2..4 times; with REVERSE, the syllable is read backwards (above 70 %, into a bigger
 * reverb: the pre-swell). WASH is the plate and a 1/8-dot tape echo, darker as it rises, as one macro.
 *
 * Threads: en_render runs in the audio interrupt; the UI posts commands (en_set and the rest), which the render
 * takes at the start of a block. The state marked (UI) is written by the render and read by the screen. The melody
 * (en_melody) is the render's; the UI reads it to save (while not writing) and hands a loaded one over with
 * en_melody_load. */
#pragma once
#include <stdint.h>

#define EN_MAX_MELODY 16               /* notes of the written melody */
#define EN_MAX_SYL 16                  /* syllables of a rendered phrase */
#define EN_MAX_STEPS 64                /* 4 bars of 1/16 */
#define EN_NPAD 4
#include "hdsp.h"
extern const hp_t EN_PARAMS[];           /* every parameter: name, range, default, value names */

enum {
    P_SINGER, P_BOOK, P_PITCH,                          /* ALGORITHM, PRESETS, SELECT */
    P_FRAGMENT, P_STUTTER, P_REVERSE, P_WASH,           /* HOME */
    P_ATTACK, P_DECAY, P_BREATH, P_CONSONANT,           /* ENV: breath */
    P_WOW, P_WOWRATE, P_FLUTTER, P_DROPOUT,             /* LFO: tape */
    P_TEMPO, P_DENSITY, P_SILENCE, P_REPEAT,            /* SEQ: placement */
    P_CHORD1, P_CHORD2, P_CHORD3, P_CHORD4,             /* ARP: the pad's progression */
    P_BED, P_BEDLEVEL, P_HIPASS, P_PADLEVEL,            /* FX: room */
    P_ANSDELAY, P_ANSTRANS, P_ANSCHANCE, P_ANSMODE,     /* GLO: answer */
    P_ROOT, P_SCALE, P_PADBARS, P_LIGHTS,               /* SEL: key */
    P_THROAT, P_TONE, P_RASP, P_VIBRATO,                /* EDIT: formant */
    P_PADOCT,                                           /* OCT-/OCT+: the pad's register, -2..1 */
    P_N
};
enum { SG_CHILD, SG_WOMAN, SG_MAN, SG_CHOIR, SG_WHISPER, SG_SLOWED, SG_RADIO, SG_N };
enum { BK_HYMN, BK_SOUL, BK_LULLABY, BK_SIREN, BK_PLEA, BK_STATIC, BK_N };
enum { CT_SIGH, CT_CALL, CT_QUESTION, CT_MANTRA, CT_FALL, CT_HELD, CT_N };
enum { TR_UNISON, TR_UP5, TR_DOWN7, TR_INVERT };
enum { PH_CALL, PH_ANSWER, PH_CHORUS, PH_SILENCE };
enum { EN_VOW_N = 11 };                /* vowels 0..4 a e i o u; 5..7 breathy a o u; 8..10 whispered a i u */

typedef struct {
    uint8_t step;                      /* 0 .. 63 (1/16 notes) from the melody's start */
    uint8_t len;                       /* in 1/16, 1 .. */
    uint8_t note;                      /* MIDI */
    uint8_t vowel;                     /* 0 .. EN_VOW_N - 1 */
} en_note_t;

/* a rendered phrase, as a voice sings it and the screen draws it */
typedef struct {
    uint8_t step, len, note, vowel, on;   /* on: kept by FRAGMENT */
} en_syl_t;
typedef struct {
    en_syl_t s[EN_MAX_SYL];
    uint8_t n, steps, root, contour;      /* steps: its length in 1/16; root: the note it is built on */
    uint8_t gate;                         /* 0: each syllable sounds its length; else this many 1/256 of a 1/16 */
} en_phrase_t;

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);
void en_arm(int on);                   /* REC: the keys write the melody (the first key starts it afresh) */
void en_key(int note, int vel, int on);   /* a white key or MIDI: sing a phrase there (and write it, armed) */
void en_vowel(int v);                  /* a black key held: the vowel for what the keys sing (-1: the book's) */
void en_keep(void);                    /* REC held: the phrase being sung (or last sung) becomes the melody */
void en_clear(void);                   /* no melody: the book's phrases again */
void en_melody_load(const en_note_t *notes, int n, int steps, uint32_t seed);
void en_seed(uint32_t s);
void en_panic(void);
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_STOP };
void en_clock(int msg);                /* MIDI clock: 24 a beat; while ticks come, they drive the steps */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state (UI) */
extern volatile uint8_t en_playing, en_armed, en_writing;   /* writing: a melody is being written (a key came) */
extern volatile uint8_t en_phase;                     /* PH_* */
extern volatile uint8_t en_phases;                    /* bit PH_*: the phases seen since PLAY (tests) */
extern volatile uint32_t en_cycles;                   /* conversations completed since PLAY */
extern volatile float en_bar_pos;                     /* the playhead within the bar, 0..16 (as heard) */
extern volatile float en_next_frac;                   /* what is left until the next event, 1 .. 0 */
extern volatile int32_t en_next_bars;                 /* bars until it (ceiling) */
extern volatile uint8_t en_chord_slot;                /* the pad's slot now, 0..3 */
extern volatile int32_t en_chord_root;                /* its root's pitch class, -1 at rest */
extern volatile en_phrase_t en_phrase[2];             /* each voice's phrase */
extern volatile float en_vpos[2];                     /* each voice's place in its phrase, in 1/16 (as heard); <0 idle */
extern volatile int8_t en_vsyl[2];                    /* the syllable sounding, -1 none */
extern volatile uint8_t en_vnote[2];                  /* its note (the keys light) */
extern volatile uint8_t en_stutter, en_reverse;       /* a stutter / a reverse is playing (the screen) */
extern volatile uint8_t en_dropout;
extern volatile float en_level;                       /* the output's level, 0..1 */
extern volatile en_note_t en_melody[EN_MAX_MELODY];   /* the melody: read while not writing (to save) */
extern volatile uint8_t en_nmelody, en_melody_steps;
extern volatile uint32_t en_melody_ver;               /* changes when the melody does (written, kept, cleared) */
extern volatile uint32_t en_seed_now;
extern volatile uint8_t en_ext;                       /* MIDI clock is driving */
extern volatile uint32_t en_steps_total;              /* 1/16 steps played since the start (tests) */
