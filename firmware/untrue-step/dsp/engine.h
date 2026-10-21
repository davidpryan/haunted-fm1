/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* UNTRUE STEP's engine: ghost garage without a grid.
 *
 * There is no step grid for the drums. The pattern is a list of EVENTS in a two-bar cycle, each with a time (in
 * ticks: 7680 a cycle, 480 an eighth, so about half a millisecond at 130 bpm), a lane (KICK, SNARE, HAT, FOLEY), a
 * velocity and a probability. Each cycle every event is rolled again: whether it plays (its probability), where it
 * lands (a gaussian spread of up to 40 ms: LOOSE) and how hard (LOOSE widens the velocity too). SHUFFLE is a warp
 * applied to the times inside each beat at playback: right, the off-beats come late (up to the two-thirds of a
 * triplet feel); left, early. GHOSTS are quiet extra hits scattered between the events, new ones every cycle.
 *   the pattern   = the GROOVE's template (2-Step, Half-time, Broken, Dub step, Ghost 4x4, Pulse, None) with the
 *                   player's edits over it: a mod per template slot (a nudge in ms, a velocity and probability
 *                   change, removed) and a list of events the player added (SEQ + a white key, near one of the 16
 *                   anchors; or REC + a white key, at the exact moment it was played). The edits are kept apart from
 *                   the template, so turning GROOVE keeps them.
 *   the drums     six voices, synthesised: a kick (a sine with a pitch envelope and a click), a snare or clap (noise
 *                   through a band and a tone), a hat or shaker (noise through a high-pass, a metallic tick), foley
 *                   (a short burst through a resonance, a bottle, a jangle). Eight KITS colour them.
 *   under them    a SUB that follows the kick on the chord's root, four FM PAD voices holding a progression of four
 *                   chords, voice-led; both DUCK to the kick. A bed (crackle, rain) is always there: DUST.
 *   LEAVE         every EVERY bars, with a chance, the drums go away for AWAY bars (the pad, the sub, the dust stay);
 *                   they come back lane by lane, and changed: the spread and the ghosts rolled again.
 *   DRAG          the tempo breathes, up to two percent, over 8 to 32 bars, as a tape machine would.
 * The clock is the engine's own, sample-exact (an event fires at its sample inside the block), or MIDI clock's: the
 * ticks set the tempo and pull the phase, the warp stays inside the beat.
 *
 * Threads: en_render runs in the audio interrupt; the UI posts commands (en_set and the rest), which the render takes
 * at the start of a block. The state marked (UI) is written by the render and read by the screen. The edits (en_mods,
 * en_user) are the render's; the UI reads them to save, and hands loaded ones over with en_pattern_load. */
#pragma once
#include <stdint.h>
#include "hdsp.h"

#define EN_CYCLE 7680                  /* ticks in the two-bar cycle */
#define EN_BAR 3840
#define EN_BEAT 960
#define EN_EIGHTH 480
#define EN_SLOTS 32                    /* a template's sixteenth slots in the cycle */
#define EN_MAX_USER 48                 /* events the player adds */
#define EN_MAX_EV 96                   /* the built pattern: the template's and the player's */
#define EN_MAX_GHOST 32                /* ghosts a cycle */
#define EN_NPERC 6
#define EN_NPAD 4
#define EN_NLANES 4
extern const hp_t EN_PARAMS[];           /* every parameter: name, range, default, value names */

enum {
    P_GROOVE, P_KIT, P_SHUFFLE,                          /* ALGORITHM, PRESETS, SELECT */
    P_LOOSE, P_GHOSTS, P_DUCK, P_DUST,                   /* HOME */
    P_DEC_KICK, P_DEC_SNARE, P_DEC_HAT, P_DEC_FOLEY,     /* ENV: decay per lane */
    P_DRAG, P_DRAGBARS, P_WANDER, P_FLAM,                /* LFO: drag */
    P_EVSEL, P_EVMOVE, P_EVVEL, P_EVPROB,                /* SEQ: the event editor (the last three mirror the event) */
    P_PROG, P_CHORDBARS, P_DETUNE, P_PAD,                /* ARP: the pad's progression */
    P_SUB, P_SUBDECAY, P_DELAY, P_FEEDBACK,              /* FX: sub and space */
    P_EVERY, P_AWAY, P_CHANCE, P_RETURN,                 /* GLO: leave */
    P_ROOT, P_SCALE, P_TEMPO, P_BED,                     /* SEL */
    P_KTUNE, P_STONE, P_HCOLOUR, P_ROOM,                 /* EDIT: the kit */
    P_SUBOCT,                                            /* OCT-/OCT+ */
    P_N
};
enum { GR_2STEP, GR_HALF, GR_BROKEN, GR_DUB, GR_GHOST44, GR_PULSE, GR_NONE, GR_N };
enum { KIT_N = 8 };
enum { L_KICK, L_SNARE, L_HAT, L_FOLEY, L_SUB, L_PAD, L_N };   /* the lanes (the last two for the keys and the screen) */
enum { RET_TOGETHER, RET_KICK, RET_HATS, RET_CHANGED };
enum { BED_NONE, BED_CRACKLE, BED_RAIN, BED_BOTH, BED_HISS, BED_ROOM, BED_N };
enum { EV_MOVE, EV_VEL, EV_PROB };                       /* en_edit's fields */

/* an event the player added (saved): its time in ticks, exact */
typedef struct {
    uint16_t t;
    uint8_t lane, vel, prob, rsv;
} en_user_t;
/* a mod on a template slot (saved): lane x sixteenth */
typedef struct {
    int8_t nudge;                      /* ms */
    int8_t vel, prob;                  /* added to the template's */
    uint8_t off;                       /* removed */
} en_mod_t;
/* the built pattern, for the screen */
typedef struct {
    uint16_t t;                        /* ticks, as written (before the warp and the spread) */
    uint8_t lane, vel, prob;
    uint8_t user;                      /* 1: the player's (src its index), 0: the template's (src lane * 32 + slot) */
    uint8_t src;
    uint8_t lit;                       /* just fired: blocks left bright */
} en_ev_t;

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);
void en_hit(int lane, int vel);        /* a white key: a hit on the lane now (recorded while REC: en_rec) */
void en_note(int lane, int deg, int on);   /* a white key on the SUB or PAD lane: a scale degree */
void en_rec(int on);                   /* REC: the hits are kept, at the exact time they were played */
void en_add(int lane, int anchor);     /* SEQ + white key: an event near anchor (0..15, eighths), LOOSE away */
void en_select(int i);                 /* the event editor's event */
void en_edit(int field, int delta);    /* the selected event: EV_MOVE (ms), EV_VEL, EV_PROB */
void en_remove(void);                  /* the selected event goes (a template event: marked off) */
void en_nudge(int ms);                 /* the selected event, ms */
void en_mute(int lane, int on);
void en_leave(void);                   /* the drums leave now */
void en_clear_edits(void);             /* the template alone */
void en_pattern_load(const en_mod_t *mods, const en_user_t *user, int nuser, uint32_t seed);
void en_panic(void);
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_STOP };
void en_clock(int msg);                /* MIDI clock: 24 a beat; while ticks come, they set the tempo and the phase */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state (UI) */
extern volatile uint8_t en_playing, en_recording, en_ext;
extern volatile uint8_t en_lane;                      /* the lane the keys play (0..5) */
extern volatile uint8_t en_mutes;                     /* bit: lane muted by the player */
extern volatile uint8_t en_away;                      /* bit: lane away (LEAVE) */
extern volatile uint8_t en_returning;                 /* the drums are coming back */
extern volatile uint32_t en_bar, en_cycle, en_leaves; /* bars and cycles since the start; leave sections so far */
extern volatile float en_pos;                         /* the playhead, ticks in the cycle */
extern volatile float en_tempo_now;                   /* the tempo this moment (drag, MIDI clock) */
extern volatile float en_level;
extern volatile float en_duck;                        /* the duck's gain, 0..1 (the screen's pump) */
extern volatile en_ev_t en_ev[EN_MAX_EV];             /* the built pattern */
extern volatile uint8_t en_nev;
extern volatile int16_t en_eff[EN_MAX_EV];            /* each event's time this cycle, ticks (warped, spread); -1 skipped */
extern volatile uint8_t en_sel;                       /* the event editor's event */
extern volatile uint8_t en_ghost_lit;                 /* a ghost just fired: its lane + 1 */
extern volatile uint8_t en_chord;                     /* the chord's degree now */
extern volatile uint8_t en_sub_lit, en_kick_lit;      /* blocks left bright */
extern volatile en_mod_t en_mods[EN_NLANES][EN_SLOTS];   /* the edits: read to save */
extern volatile en_user_t en_user[EN_MAX_USER];
extern volatile uint8_t en_nuser;
extern volatile uint32_t en_pat_ver;                  /* changes when the edits do */
extern volatile uint32_t en_seed_now;
