/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* UNTRUE STEP's engine (engine.h). One compilation unit, -O2. */
#include "engine.h"

/* ---------------------------------------------------------------------------------------------- parameters --- */
static const char *const GROOVE_NAME[GR_N] = {"2-Step", "Half-time", "Broken", "Dub step", "Ghost 4x4", "Pulse", "None"};
static const char *const KIT_NAME[KIT_N] = {"Lighter", "Vinyl", "Bottle", "Keys", "Metal", "Paper", "Wood", "Garage"};
static const char *const PROG_NAME[8] = {"1637", "1451", "1645", "6415", "1166", "1364", "4165", "1764"};
static const char *const CBARS_NAME[4] = {"1 bar", "2 bar", "4 bar", "8 bar"};
static const char *const RETURN_NAME[4] = {"All", "Kick", "Hats", "New"};
static const char *const BED_NAME[BED_N] = {"None", "Vinyl", "Rain", "Both", "Hiss", "Room"};
static const char *const SUBOCT_NAME[3] = {"Low", "Mid", "High"};
const hp_t EN_PARAMS[P_N] = {
    {"Groove", 0, GR_N - 1, 0, GROOVE_NAME, 0}, {"Kit", 0, KIT_N - 1, 0, KIT_NAME, 0}, {"Shuffle", -50, 50, 0, 0, ""},
    {"Loose", 0, 100, 30, 0, ""}, {"Ghosts", 0, 100, 30, 0, ""}, {"Duck", 0, 100, 45, 0, ""}, {"Dust", 0, 100, 40, 0, ""},
    {"Kick", 0, 100, 50, 0, ""}, {"Snare", 0, 100, 50, 0, ""}, {"Hat", 0, 100, 50, 0, ""}, {"Foley", 0, 100, 50, 0, ""},
    {"Drag", 0, 100, 30, 0, ""}, {"Over", 8, 32, 16, 0, " bar"}, {"Wander", 0, 100, 15, 0, ""}, {"Flam", 0, 100, 15, 0, "%"},
    {"Event", 0, EN_MAX_EV - 1, 0, 0, ""}, {"Move ms", -120, 120, 0, 0, ""}, {"Velocity", 1, 127, 100, 0, ""}, {"Chance", 0, 100, 100, 0, "%"},
    {"Chords", 0, 7, 0, PROG_NAME, 0}, {"Change", 0, 3, 1, CBARS_NAME, 0}, {"Detune", 0, 100, 30, 0, ""}, {"Pad", 0, 100, 55, 0, ""},
    {"Sub", 0, 100, 70, 0, ""}, {"Sub decay", 0, 100, 50, 0, ""}, {"Delay", 0, 100, 35, 0, ""}, {"Feedback", 0, 100, 45, 0, ""},
    {"Every", 4, 64, 24, 0, " bar"}, {"Away", 1, 16, 6, 0, " bar"}, {"Chance", 0, 100, 70, 0, "%"}, {"Return", 0, 3, RET_CHANGED, RETURN_NAME, 0},
    {"Root", 0, 11, 9, H_NOTE_NAME, 0}, {"Scale", 0, H_SC_N - 1, 0, H_SCALE_NAME, 0}, {"Tempo", 80, 160, 132, 0, ""}, {"Bed", 0, BED_N - 1, BED_BOTH, BED_NAME, 0},
    {"Kick tune", -50, 50, 0, 0, ""}, {"Snare tone", 0, 100, 50, 0, ""}, {"Hat colour", 0, 100, 50, 0, ""}, {"Room", 0, 100, 45, 0, ""},
    {"Sub octave", -1, 1, 0, SUBOCT_NAME, 0},
};

/* the grooves: a template of events on the cycle's 32 sixteenths, each a lane, a slot, a velocity, a chance and a
 * micro offset in ms (a late snare, a dragged hat) */
typedef struct { uint8_t lane, slot, vel, prob; int8_t ms; } tmpl_t;
static const tmpl_t T_2STEP[] = {
    {0, 0, 118, 100, 0}, {0, 10, 104, 100, 0}, {0, 16, 118, 100, 0}, {0, 26, 100, 100, 0}, {0, 29, 80, 45, 0},
    {1, 4, 110, 100, 8}, {1, 12, 112, 100, 10}, {1, 20, 110, 100, 8}, {1, 28, 112, 100, 10}, {1, 31, 54, 40, 0},
    {2, 2, 84, 100, 6}, {2, 6, 78, 100, 6}, {2, 10, 84, 100, 6}, {2, 14, 80, 100, 6}, {2, 15, 46, 60, 0},
    {2, 18, 84, 100, 6}, {2, 22, 78, 100, 6}, {2, 26, 84, 100, 6}, {2, 30, 80, 100, 6},
    {3, 7, 70, 55, 0}, {3, 13, 62, 40, 0}, {3, 23, 74, 60, 0}};
static const tmpl_t T_HALF[] = {
    {0, 0, 120, 100, 0}, {0, 11, 96, 45, 0}, {0, 16, 120, 100, 0}, {0, 22, 90, 60, 0},
    {1, 8, 112, 100, 12}, {1, 24, 112, 100, 12}, {1, 30, 50, 35, 0},
    {2, 2, 76, 100, 4}, {2, 6, 70, 100, 4}, {2, 10, 76, 100, 4}, {2, 14, 70, 100, 4}, {2, 18, 76, 100, 4},
    {2, 22, 70, 100, 4}, {2, 26, 76, 100, 4}, {2, 30, 70, 100, 4}, {2, 13, 44, 50, 0}, {2, 29, 44, 50, 0},
    {3, 5, 66, 50, 0}, {3, 21, 70, 55, 0}, {3, 27, 58, 35, 0}};
static const tmpl_t T_BROKEN[] = {
    {0, 0, 118, 100, 0}, {0, 6, 100, 100, 0}, {0, 11, 96, 80, 0}, {0, 16, 118, 100, 0}, {0, 22, 102, 100, 0}, {0, 27, 88, 50, 0},
    {1, 4, 108, 100, 10}, {1, 13, 104, 100, -8}, {1, 20, 108, 100, 10}, {1, 29, 104, 100, -8}, {1, 31, 56, 50, 0},
    {2, 2, 80, 100, 0}, {2, 5, 70, 100, 0}, {2, 9, 82, 100, 0}, {2, 14, 74, 100, 0}, {2, 18, 80, 100, 0},
    {2, 21, 70, 100, 0}, {2, 25, 82, 100, 0}, {2, 30, 74, 100, 0},
    {3, 7, 72, 60, 0}, {3, 15, 66, 60, 0}, {3, 23, 74, 60, 0}};
static const tmpl_t T_DUB[] = {
    {0, 0, 120, 100, 0}, {0, 10, 108, 100, 0}, {0, 16, 120, 100, 0}, {0, 22, 104, 100, 0}, {0, 29, 92, 50, 0},
    {2, 6, 52, 50, 0}, {2, 14, 52, 50, 0}, {2, 22, 52, 50, 0}, {2, 30, 52, 50, 0},
    {3, 12, 70, 50, 0}, {3, 28, 64, 45, 0}};
static const tmpl_t T_GHOST44[] = {
    {0, 0, 54, 80, 0}, {0, 4, 48, 70, 0}, {0, 8, 52, 70, 0}, {0, 12, 46, 70, 0}, {0, 16, 54, 80, 0}, {0, 20, 48, 70, 0},
    {0, 24, 52, 70, 0}, {0, 28, 46, 70, 0},
    {1, 4, 64, 100, 8}, {1, 12, 66, 100, 8}, {1, 20, 64, 100, 8}, {1, 28, 66, 100, 8},
    {2, 2, 72, 100, 6}, {2, 6, 72, 100, 6}, {2, 10, 72, 100, 6}, {2, 14, 72, 100, 6}, {2, 18, 72, 100, 6},
    {2, 22, 72, 100, 6}, {2, 26, 72, 100, 6}, {2, 30, 72, 100, 6},
    {3, 9, 60, 50, 0}, {3, 25, 62, 50, 0}};
static const tmpl_t T_PULSE[] = {{0, 0, 112, 100, 0}, {0, 16, 112, 100, 0}, {2, 8, 60, 50, 0}, {2, 24, 60, 50, 0}};
static const struct { const tmpl_t *ev; uint8_t n; } TEMPLATES[GR_N] = {
    {T_2STEP, sizeof T_2STEP / sizeof T_2STEP[0]}, {T_HALF, sizeof T_HALF / sizeof T_HALF[0]},
    {T_BROKEN, sizeof T_BROKEN / sizeof T_BROKEN[0]}, {T_DUB, sizeof T_DUB / sizeof T_DUB[0]},
    {T_GHOST44, sizeof T_GHOST44 / sizeof T_GHOST44[0]}, {T_PULSE, sizeof T_PULSE / sizeof T_PULSE[0]}, {0, 0}};

/* the kits: how each lane's model is coloured. kick: its pitch from k_f0 to k_f1, the click; snare: the noise band
 * (s_hz, s_q), the tone under it (s_tone, s_mix), clap (three bursts); hat: the high-pass (h_hz), the metallic tick;
 * foley: a type (0 click, 1 bottle, 2 jangle, 3 paper, 4 wood), its resonance */
enum { FO_CLICK, FO_BOTTLE, FO_JANGLE, FO_PAPER, FO_WOOD };
typedef struct {
    float k_f0, k_f1, k_click;
    float s_hz, s_q, s_tone, s_mix;
    uint8_t s_clap, f_type;
    float h_hz, h_metal;
    float f_hz, f_q;
} kit_t;
static const kit_t KITS[KIT_N] = {
    {140.0f, 48.0f, 0.6f, 2200.0f, 1.2f, 190.0f, 0.5f, 0, FO_CLICK, 7000.0f, 0.3f, 4000.0f, 3.0f},     /* lighter */
    {120.0f, 42.0f, 0.3f, 1500.0f, 0.8f, 170.0f, 0.4f, 1, FO_PAPER, 6000.0f, 0.1f, 3000.0f, 1.5f},     /* vinyl */
    {160.0f, 50.0f, 0.4f, 1800.0f, 2.0f, 240.0f, 0.6f, 0, FO_BOTTLE, 8000.0f, 0.5f, 1250.0f, 14.0f},   /* bottle */
    {130.0f, 45.0f, 0.5f, 2600.0f, 1.0f, 200.0f, 0.3f, 0, FO_JANGLE, 9000.0f, 0.8f, 2100.0f, 4.0f},    /* keys */
    {150.0f, 55.0f, 0.7f, 3000.0f, 3.0f, 330.0f, 0.7f, 0, FO_JANGLE, 8500.0f, 1.0f, 900.0f, 9.0f},     /* metal */
    {110.0f, 40.0f, 0.2f, 1200.0f, 0.6f, 150.0f, 0.3f, 1, FO_PAPER, 5000.0f, 0.0f, 1800.0f, 0.7f},     /* paper */
    {170.0f, 60.0f, 0.5f, 1600.0f, 1.5f, 420.0f, 0.8f, 0, FO_WOOD, 6500.0f, 0.2f, 1900.0f, 9.0f},      /* wood */
    {135.0f, 45.0f, 0.5f, 2000.0f, 1.1f, 185.0f, 0.55f, 1, FO_CLICK, 7500.0f, 0.4f, 2600.0f, 3.0f},    /* garage */
};
static const int8_t PROGS[8][4] = {{0, 5, 2, 6}, {0, 3, 4, 0}, {0, 5, 3, 4}, {5, 3, 0, 4}, {0, 0, 5, 5}, {0, 2, 5, 3}, {3, 0, 5, 4}, {0, 6, 5, 3}};

/* ------------------------------------------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_HIT, C_NOTE, C_REC, C_ADD, C_SELECT, C_EDIT, C_REMOVE, C_NUDGE, C_MUTE, C_LEAVE, C_CLEAR,
       C_PATTERN, C_PANIC, C_CLOCK };
static h_cmdq_t cq;
void en_set(int p, int v) { h_post(&cq, C_SET, p, v); }
void en_play(int on) { h_post(&cq, C_PLAY, on, 0); }
void en_hit(int lane, int vel) { h_post(&cq, C_HIT, lane, vel); }
void en_note(int lane, int deg, int on) { h_post(&cq, C_NOTE, lane, deg * 2 + (on ? 1 : 0)); }
void en_rec(int on) { h_post(&cq, C_REC, on, 0); }
void en_add(int lane, int anchor) { h_post(&cq, C_ADD, lane, anchor); }
void en_select(int i) { h_post(&cq, C_SELECT, i, 0); }
void en_edit(int field, int delta) { h_post(&cq, C_EDIT, field, delta); }
void en_remove(void) { h_post(&cq, C_REMOVE, 0, 0); }
void en_nudge(int ms) { h_post(&cq, C_NUDGE, ms, 0); }
void en_mute(int lane, int on) { h_post(&cq, C_MUTE, lane, on); }
void en_leave(void) { h_post(&cq, C_LEAVE, 0, 0); }
void en_clear_edits(void) { h_post(&cq, C_CLEAR, 0, 0); }
void en_panic(void) { h_post(&cq, C_PANIC, 0, 0); }
void en_clock(int msg) { h_post(&cq, C_CLOCK, msg, 0); }

static en_mod_t shadow_mods[EN_NLANES][EN_SLOTS];       /* a pattern handed over by the UI */
static en_user_t shadow_user[EN_MAX_USER];
static volatile uint8_t shadow_n;
static volatile uint32_t shadow_seed;
void en_pattern_load(const en_mod_t *mods, const en_user_t *user, int n, uint32_t seed)
{
    int i, l, k;
    if (n > EN_MAX_USER)
        n = EN_MAX_USER;
    for (l = 0; l < EN_NLANES; l++)
        for (k = 0; k < EN_SLOTS; k++)
            shadow_mods[l][k] = mods[l * EN_SLOTS + k];
    for (i = 0; i < n; i++)
        shadow_user[i] = user[i];
    shadow_n = (uint8_t)n;
    shadow_seed = seed;
    H_BARRIER();
    h_post(&cq, C_PATTERN, 0, 0);
}

/* --------------------------------------------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_recording, en_ext, en_lane, en_mutes, en_away, en_returning, en_nev, en_sel,
    en_ghost_lit, en_chord, en_sub_lit, en_kick_lit, en_nuser;
volatile uint32_t en_bar, en_cycle, en_leaves, en_pat_ver, en_seed_now = 0x5EED5u;
volatile float en_pos, en_tempo_now = 132.0f, en_level, en_duck = 1.0f;
volatile en_ev_t en_ev[EN_MAX_EV];
volatile int16_t en_eff[EN_MAX_EV];
volatile en_mod_t en_mods[EN_NLANES][EN_SLOTS];
volatile en_user_t en_user[EN_MAX_USER];

static int16_t par[P_N];
static h_rng_t rng;                                     /* the render's dice: the roll of each cycle */
static float pos;                                       /* the playhead, ticks */
static float tick_rate;                                 /* ticks a sample at the tempo */
static float tpm;                                       /* ticks a millisecond */
static float drag_ph, drag_f = 1.0f;                    /* the tempo's breathing */
static uint32_t bar_in_cycle;                           /* 0 or 1: the bar the playhead is in */
static float loose_var, ghost_var;                      /* a return's change: added to LOOSE and GHOSTS */

/* the roll of this cycle */
static int16_t jit[EN_MAX_EV];                          /* each event's spread, ticks */
static int8_t veljit[EN_MAX_EV];
static uint8_t active[EN_MAX_EV];
static float wander[EN_MAX_EV];                         /* a slow drift of each event's mean, ms */
typedef struct { uint16_t t; uint8_t lane, vel; } ghost_t;
static ghost_t ghosts[EN_MAX_GHOST];
static uint8_t nghost;
typedef struct { uint16_t t; uint8_t idx, vel; } sched_t;   /* idx: an event, or 128 + a ghost */
static sched_t sched[EN_MAX_EV + EN_MAX_GHOST];
static uint32_t nsched, cursor;
typedef struct { uint16_t off; uint8_t idx, vel; } fire_t;
static fire_t fire[24];
static uint32_t nfire;
static uint32_t sample_clk;                             /* samples since the start (MIDI clock) */

/* leave */
static uint32_t bars_since_leave, away_left, kick_in_bar;
static uint8_t away_state;                              /* 0 playing, 1 away, 2 returning */

/* MIDI clock */
static uint32_t ext_ticks, ext_last_clk, ext_age;
static float ext_interval, ext_adj = 1.0f;

/* the drums */
typedef struct {
    uint8_t on, lane, ghost, ftype;
    float amp, amp_k, gain, pan;
    float pitch, pitch_k, pitch_end, ph, tone_hz;
    float tone_amp, tone_k, click, click_k, metal;
    int32_t clap_n, clap_t, clap_len;
    float amp0;
    h_svf_t svf;
    h_fm_t fm;
    h_rng_t rng;
} pv_t;
static pv_t pv[EN_NPERC];
static uint32_t pv_n;
static float lane_dec[EN_NLANES];                       /* each lane's decay, ms */

/* the sub, the pad */
static float sub_ph, sub_f, sub_amp, sub_k, sub_tgt_amp, sub_lvl;
static h_lp1_t sub_lp;
static h_fm_t pad[EN_NPAD];
static float pad_freq_t[EN_NPAD];
static int pad_note[EN_NPAD];
static uint8_t pad_live;                                /* a key holds a pad voice */
static int pad_live_note;
static float pad_lvl, pad_g;
static uint32_t chord_ver;

/* duck, space, bed */
static float duck_env, duck_k, duck_depth;
static h_plate_t plate;
H_PLATE_BUFFERS(pl);
#define ECHO_N 25000u
static int16_t echo_buf[ECHO_N] H_POOL;
typedef struct { h_dl_t dl; float time, time_t, fb; h_lp1_t lp, hp; } echo_t;   /* a tempo delay, darker each repeat */
static echo_t echo;
static h_bed_t bedA, bedB;
static h_lp1_t lvl_lp;
static h_lim_t lim = {0.0f, 1.0f};
static h_dc_t dcl, dcr;
static float room_send, delay_send, pad_rev;
static float master_trim = 0.6f;

/* ----------------------------------------------------------------------------------------------- the clock --- */
static float tempo_now(void) { return en_ext ? en_tempo_now : (float)par[P_TEMPO]; }

static void rate_set(void)                              /* ticks a sample, from the tempo this moment */
{
    float t = tempo_now() * drag_f * (en_ext ? ext_adj : 1.0f);
    tick_rate = (float)EN_BEAT * t / (60.0f * H_SR);    /* a beat is 960 ticks and 60/t seconds */
    tpm = (float)EN_BEAT * t / 60000.0f;
    if (!en_ext)
        en_tempo_now = t;
    echo.time_t = fm_clampf(45000.0f / tempo_now() * 44.1f, 40.0f, (float)(ECHO_N - 4u));   /* a dotted eighth */
}
static void echo_init(void)
{
    h_dl_init(&echo.dl, echo_buf, ECHO_N);
    echo.time = echo.time_t = 0.25f * H_SR;
    echo.lp.z = echo.hp.z = 0.0f;
}
static inline float echo_run(float in)
{
    float y;
    echo.time += (echo.time_t - echo.time) * 0.0008f;   /* the time glides: a tempo change bends the repeats */
    y = h_dl_tap(&echo.dl, echo.time);
    y = h_lp1(&echo.lp, y, 0.22f);
    y = h_hp1(&echo.hp, y, 0.004f);
    h_dl_push(&echo.dl, fm_clampf(in + y * echo.fb, -1.0f, 1.0f));
    return y;
}

static uint32_t pool(void) { return (uint32_t)H_SCALE[par[P_SCALE]]; }

/* the shuffle: a warp inside each beat. s -1..1: the half-beat lands at 1/3 (push) .. 2/3 (swing) */
static float warp(float t)
{
    float s = (float)par[P_SHUFFLE] * (1.0f / 50.0f), p = 0.5f + s * (1.0f / 6.0f);
    float b = fm_floorf(t * (1.0f / (float)EN_BEAT)), u = t * (1.0f / (float)EN_BEAT) - b;
    u = u < 0.5f ? u * 2.0f * p : p + (u - 0.5f) * 2.0f * (1.0f - p);
    return (b + u) * (float)EN_BEAT;
}
static float unwarp(float t)                            /* a played moment back to the written grid */
{
    float s = (float)par[P_SHUFFLE] * (1.0f / 50.0f), p = 0.5f + s * (1.0f / 6.0f);
    float b = fm_floorf(t * (1.0f / (float)EN_BEAT)), u = t * (1.0f / (float)EN_BEAT) - b;
    u = u < p ? u * 0.5f / p : 0.5f + (u - p) * 0.5f / (1.0f - p);
    return (b + u) * (float)EN_BEAT;
}
static int wrap_t(int t)
{
    while (t < 0)
        t += EN_CYCLE;
    while (t >= EN_CYCLE)
        t -= EN_CYCLE;
    return t;
}

/* --------------------------------------------------------------------------------------------- the pattern --- */
static float loose_ms(void) { return fm_clampf((float)par[P_LOOSE] + loose_var, 0.0f, 100.0f) * 0.4f; }
static float ghosts_n(void) { return fm_clampf((float)par[P_GHOSTS] + ghost_var, 0.0f, 100.0f) * 0.01f; }

static void sched_insert(uint16_t t, uint8_t idx, uint8_t vel)
{
    uint32_t i = nsched;
    if (nsched >= EN_MAX_EV + EN_MAX_GHOST)
        return;
    while (i > 0 && sched[i - 1].t > t) {
        sched[i] = sched[i - 1];
        i--;
    }
    sched[i].t = t;
    sched[i].idx = idx;
    sched[i].vel = vel;
    nsched++;
}

/* the schedule of this cycle from the roll: every active event's warped, spread time; the ghosts; sorted */
static void schedule_build(void)
{
    uint32_t i;
    nsched = 0;
    for (i = 0; i < en_nev; i++) {
        int t;
        if (!active[i]) {
            en_eff[i] = -1;
            continue;
        }
        t = wrap_t((int)(warp((float)en_ev[i].t + wander[i] * tpm) + 0.5f) + jit[i]);
        en_eff[i] = (int16_t)t;
        sched_insert((uint16_t)t, (uint8_t)i, (uint8_t)fm_clampf((float)en_ev[i].vel + (float)veljit[i], 1.0f, 127.0f));
    }
    for (i = 0; i < nghost; i++)
        sched_insert(ghosts[i].t, (uint8_t)(128u + i), ghosts[i].vel);
}

static void cursor_from_pos(void)
{
    cursor = 0;
    while (cursor < nsched && (float)sched[cursor].t < pos)
        cursor++;
}

/* the roll: a new cycle's chances, spreads, ghosts */
static void roll_cycle(void)
{
    uint32_t i, n;
    float lm = loose_ms(), gn = ghosts_n(), wn = (float)par[P_WANDER] * 0.01f, flam = (float)par[P_FLAM] * 0.01f * 0.5f;
    for (i = 0; i < en_nev; i++) {
        active[i] = (uint8_t)(h_frand(&rng) * 100.0f < (float)en_ev[i].prob);
        jit[i] = (int16_t)(h_grand(&rng) * lm * tpm);
        veljit[i] = (int8_t)(h_grand(&rng) * lm * 0.5f);
        wander[i] += h_grand(&rng) * wn * 4.0f;
        wander[i] = fm_clampf(wander[i] * 0.97f, -wn * 30.0f, wn * 30.0f);
    }
    nghost = 0;
    n = (uint32_t)(gn * 22.0f + h_frand(&rng) * 2.0f);
    for (i = 0; i < n && nghost < EN_MAX_GHOST; i++) {         /* a quiet hit between the events */
        float r = h_frand(&rng);
        int lane = r < 0.5f ? L_HAT : r < 0.8f ? L_SNARE : L_FOLEY, slot = h_irand(&rng, EN_SLOTS), t, k, near = 0;
        if (en_away >> lane & 1u)
            continue;
        t = wrap_t((int)(warp((float)(slot * (EN_CYCLE / EN_SLOTS))) + h_grand(&rng) * (20.0f + lm) * tpm));
        for (k = 0; k < en_nev; k++)                              /* not on top of a hit of its lane */
            if (en_ev[k].lane == lane && active[k]) {
                int d = en_eff[k] >= 0 ? en_eff[k] - t : wrap_t(en_ev[k].t) - t;
                if (d < 0)
                    d = -d;
                if (d > EN_CYCLE / 2)
                    d = EN_CYCLE - d;
                if (d < 100)
                    near = 1;
            }
        if (near)
            continue;
        ghosts[nghost].t = (uint16_t)t;
        ghosts[nghost].lane = (uint8_t)lane;
        ghosts[nghost].vel = (uint8_t)(14 + h_irand(&rng, 14));
        nghost++;
    }
    if (flam > 0.0f)                                              /* a flam: a quieter hit just before a snare or foley */
        for (i = 0; i < en_nev && nghost < EN_MAX_GHOST; i++)
            if (active[i] && (en_ev[i].lane == L_SNARE || en_ev[i].lane == L_FOLEY) && h_chance(&rng, flam)) {
                ghosts[nghost].t = (uint16_t)wrap_t((int)warp((float)en_ev[i].t) + jit[i] - (int)((18.0f + 16.0f * h_frand(&rng)) * tpm));
                ghosts[nghost].lane = en_ev[i].lane;
                ghosts[nghost].vel = (uint8_t)(en_ev[i].vel / 2);
                nghost++;
            }
    schedule_build();
}

/* the built pattern: the groove's template with the mods over it, then the player's events */
static void rebuild(void)
{
    const tmpl_t *tp = TEMPLATES[par[P_GROOVE]].ev;
    int n = TEMPLATES[par[P_GROOVE]].n, i, k = 0;
    for (i = 0; i < n && k < EN_MAX_EV; i++) {
        const volatile en_mod_t *m = &en_mods[tp[i].lane][tp[i].slot];
        int v, p;
        if (m->off)
            continue;
        v = tp[i].vel + m->vel;
        p = tp[i].prob + m->prob;
        en_ev[k].t = (uint16_t)wrap_t(tp[i].slot * (EN_CYCLE / EN_SLOTS) + (int)(((float)tp[i].ms + (float)m->nudge) * tpm));
        en_ev[k].lane = tp[i].lane;
        en_ev[k].vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        en_ev[k].prob = (uint8_t)(p < 0 ? 0 : p > 100 ? 100 : p);
        en_ev[k].user = 0;
        en_ev[k].src = (uint8_t)(tp[i].lane * EN_SLOTS + tp[i].slot);
        en_ev[k].lit = 0;
        k++;
    }
    for (i = 0; i < en_nuser && k < EN_MAX_EV; i++) {
        en_ev[k].t = en_user[i].t;
        en_ev[k].lane = en_user[i].lane;
        en_ev[k].vel = en_user[i].vel;
        en_ev[k].prob = en_user[i].prob;
        en_ev[k].user = 1;
        en_ev[k].src = (uint8_t)i;
        en_ev[k].lit = 0;
        k++;
    }
    for (i = 0; i < k; i++)
        wander[i] = 0.0f;
    H_BARRIER();
    en_nev = (uint8_t)k;
    if (en_sel >= en_nev)
        en_sel = (uint8_t)(k ? k - 1 : 0);
    roll_cycle();
    cursor_from_pos();
}

static void edits_changed(void)
{
    en_pat_ver++;
    rebuild();
}

static void user_add(int t, int lane, int vel)
{
    if (en_nuser >= EN_MAX_USER || lane < 0 || lane >= EN_NLANES)
        return;
    en_user[en_nuser].t = (uint16_t)wrap_t(t);
    en_user[en_nuser].lane = (uint8_t)lane;
    en_user[en_nuser].vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    en_user[en_nuser].prob = 100;
    en_user[en_nuser].rsv = 0;
    en_nuser++;
    edits_changed();
    en_sel = (uint8_t)(en_nev - 1);                     /* the new one is selected */
}

static void edit(int field, int delta)
{
    volatile en_ev_t *e;
    if (en_sel >= en_nev)
        return;
    e = &en_ev[en_sel];
    if (e->user) {
        volatile en_user_t *u = &en_user[e->src];
        if (field == EV_MOVE)
            u->t = (uint16_t)wrap_t((int)u->t + (int)((float)delta * tpm));
        else if (field == EV_VEL)
            u->vel = (uint8_t)fm_clampf((float)u->vel + (float)delta, 1.0f, 127.0f);
        else
            u->prob = (uint8_t)fm_clampf((float)u->prob + (float)delta, 0.0f, 100.0f);
    } else {
        volatile en_mod_t *m = &en_mods[e->src / EN_SLOTS][e->src % EN_SLOTS];
        if (field == EV_MOVE)
            m->nudge = (int8_t)fm_clampf((float)m->nudge + (float)delta, -120.0f, 120.0f);
        else if (field == EV_VEL)
            m->vel = (int8_t)fm_clampf((float)m->vel + (float)delta, -120.0f, 120.0f);
        else
            m->prob = (int8_t)fm_clampf((float)m->prob + (float)delta, -100.0f, 100.0f);
    }
    edits_changed();
}

static void remove_sel(void)
{
    volatile en_ev_t *e;
    if (en_sel >= en_nev)
        return;
    e = &en_ev[en_sel];
    if (e->user) {
        int i;
        for (i = e->src; i + 1 < en_nuser; i++)
            en_user[i] = en_user[i + 1];
        en_nuser--;
    } else {
        en_mods[e->src / EN_SLOTS][e->src % EN_SLOTS].off = 1;
    }
    edits_changed();
}

static void mods_clear(void)
{
    int l, k;
    for (l = 0; l < EN_NLANES; l++)
        for (k = 0; k < EN_SLOTS; k++) {
            volatile en_mod_t *m = &en_mods[l][k];
            m->nudge = m->vel = m->prob = 0;
            m->off = 0;
        }
}

static void clear_edits(void)
{
    mods_clear();
    en_nuser = 0;
    edits_changed();
}

/* --------------------------------------------------------------------------------------------- the drums --- */
static float ms_curve(int v, float lo, float hi) { return lo * fm_exp2f((float)v * 0.01f * fm_log2f(hi / lo)); }
static float decay_mul(float ms) { return 1.0f - h_k_ms(ms); }

static pv_t *pv_alloc(int lane)
{
    int i, best = 0;
    float quiet = 1e9f;
    if (lane == L_KICK || lane == L_HAT)               /* one kick, one hat: a new one chokes the last */
        for (i = 0; i < EN_NPERC; i++)
            if (pv[i].on && pv[i].lane == lane)
                return &pv[i];
    for (i = 0; i < EN_NPERC; i++)
        if (!pv[i].on)
            return &pv[i];
    for (i = 0; i < EN_NPERC; i++) {                    /* the same lane first (a choke), else the quietest */
        float a = pv[i].amp + (pv[i].lane == lane ? -1.0f : 0.0f);
        if (a < quiet) {
            quiet = a;
            best = i;
        }
    }
    return &pv[best];
}

static void drum_on(int lane, int vel, int ghost)
{
    const kit_t *k = &KITS[par[P_KIT]];
    pv_t *v = pv_alloc(lane);
    float vn = (float)vel * (1.0f / 127.0f), dec = lane_dec[lane] * (ghost ? 0.6f : 1.0f);
    float dust = (float)par[P_DUST] * 0.01f;
    v->on = 1;
    v->lane = (uint8_t)lane;
    v->ghost = (uint8_t)ghost;
    v->gain = vn * (0.3f + 0.7f * vn) * (ghost ? 0.35f : 1.0f);
    v->rng.s = (uint32_t)(++pv_n * 2654435761u) | 1u;
    v->amp = v->amp0 = 1.0f;
    v->amp_k = decay_mul(dec);
    v->ph = 0.0f;
    v->tone_amp = v->click = v->metal = 0.0f;
    v->clap_n = 0;
    v->fm.amp.st = v->fm.mod.st = H_ENV_OFF;
    v->fm.amp.v = v->fm.mod.v = 0.0f;
    v->fm.last = 0.0f;
    v->pan = ghost ? h_brand(&v->rng) * 0.6f : lane == L_HAT ? -0.3f : lane == L_FOLEY ? 0.35f : lane == L_SNARE ? 0.08f : 0.0f;
    switch (lane) {
    case L_KICK: {
        float tune = h_semis((float)par[P_KTUNE] * 0.24f);
        v->pitch = k->k_f0 * tune * (1.0f + 0.6f * vn);
        v->pitch_end = k->k_f1 * tune;
        v->pitch_k = h_k_ms(28.0f);
        v->click = k->k_click * 0.8f;
        v->click_k = decay_mul(2.5f);
        v->amp_k = decay_mul(dec * 0.5f + 60.0f);
        break;
    }
    case L_SNARE: {
        float tone = (float)par[P_STONE] * 0.01f;
        h_svf_set(&v->svf, k->s_hz * (0.6f + tone), k->s_q);
        v->tone_hz = k->s_tone * (0.8f + 0.4f * tone);
        v->tone_amp = k->s_mix * (0.5f + 0.5f * tone) * 0.8f;
        v->tone_k = decay_mul(dec * 0.25f + 10.0f);
        v->svf.ic1 = v->svf.ic2 = 0.0f;
        if (k->s_clap && !ghost) {
            v->clap_n = 2;
            v->clap_len = v->clap_t = (int32_t)(H_SR * 0.011f);
        }
        break;
    }
    case L_HAT: {
        float col = (float)par[P_HCOLOUR] * 0.01f;
        h_svf_set(&v->svf, k->h_hz * (0.6f + col) * (1.0f + 0.8f * dust) * (ghost ? 1.3f : 1.0f), 0.8f);
        v->svf.ic1 = v->svf.ic2 = 0.0f;
        v->metal = k->h_metal * col * 2.0f * 0.5f;
        if (v->metal > 0.0f) {
            v->fm.ratio = 3.73f;
            v->fm.idx = 2.2f;
            v->fm.fb = 0.2f;
            v->fm.det = 1.0f;
            h_env_set(&v->fm.amp, 0.3f, dec * 0.6f, 0.0f, 20.0f);
            h_env_set(&v->fm.mod, 0.3f, dec * 0.3f, 0.0f, 20.0f);
            h_fm_on(&v->fm, k->h_hz * 0.31f * (0.8f + 0.4f * col), 1.0f);
        }
        break;
    }
    default: {                                            /* foley */
        v->ftype = k->f_type;
        v->svf.ic1 = v->svf.ic2 = 0.0f;
        switch (k->f_type) {
        case FO_CLICK:
            h_svf_set(&v->svf, k->f_hz, k->f_q);
            v->amp_k = decay_mul(dec * 0.08f + 4.0f);
            v->metal = 0.5f;
            break;
        case FO_BOTTLE:
            h_svf_set(&v->svf, k->f_hz * (0.9f + 0.2f * vn), k->f_q);
            v->amp_k = decay_mul(dec * 0.5f);
            v->metal = 0.25f;
            break;
        case FO_JANGLE:
            h_svf_set(&v->svf, 3200.0f, 1.5f);
            v->amp_k = decay_mul(dec * 0.3f);
            v->metal = 0.9f;
            break;
        case FO_PAPER:
            h_svf_set(&v->svf, k->f_hz, k->f_q);
            v->amp_k = decay_mul(dec * 0.35f + 20.0f);
            break;
        default:                                          /* wood */
            h_svf_set(&v->svf, k->f_hz, k->f_q);
            v->amp_k = decay_mul(dec * 0.2f + 15.0f);
            v->tone_hz = k->f_hz * 0.5f;
            v->tone_amp = 0.4f;
            v->tone_k = decay_mul(25.0f);
            break;
        }
        if (v->metal > 0.0f) {
            v->fm.ratio = k->f_type == FO_JANGLE ? 2.76f : 5.1f;
            v->fm.idx = k->f_type == FO_JANGLE ? 3.0f : 2.0f;
            v->fm.fb = k->f_type == FO_JANGLE ? 0.5f : 0.0f;
            v->fm.det = 1.0f;
            h_env_set(&v->fm.amp, 0.3f, k->f_type == FO_CLICK ? 18.0f : dec * 0.5f, 0.0f, 30.0f);
            h_env_set(&v->fm.mod, 0.3f, k->f_type == FO_CLICK ? 10.0f : dec * 0.25f, 0.0f, 30.0f);
            h_fm_on(&v->fm, k->f_hz * (k->f_type == FO_JANGLE ? 1.0f : 1.5f) * (0.9f + 0.2f * vn), 1.0f);
        }
        break;
    }
    }
}

static inline float pv_run(pv_t *v)
{
    float n = h_brand(&v->rng), out, lp, bp, hp;
    switch (v->lane) {
    case L_KICK:
        v->pitch += (v->pitch_end - v->pitch) * v->pitch_k;
        v->ph = h_wrap(v->ph + v->pitch * H_ISR);
        out = h_sin(v->ph) * v->amp + n * v->click;
        v->click *= v->click_k;
        break;
    case L_SNARE:
        h_svf(&v->svf, n, &lp, &bp, &hp);
        v->ph = h_wrap(v->ph + v->tone_hz * H_ISR);
        out = bp * v->amp * 1.4f + h_sin(v->ph) * v->tone_amp;
        v->tone_amp *= v->tone_k;
        if (v->clap_n && --v->clap_t <= 0) {
            v->amp = v->amp0 * 0.85f;
            v->clap_t = v->clap_len;
            v->clap_n--;
        }
        break;
    case L_HAT:
        h_svf(&v->svf, n, &lp, &bp, &hp);
        out = hp * v->amp * 0.9f;
        if (v->metal > 0.0f && h_env_busy(&v->fm.amp))
            out += h_fm(&v->fm) * v->metal * 0.4f;
        break;
    default:
        h_svf(&v->svf, n, &lp, &bp, &hp);
        out = (v->ftype == FO_CLICK ? hp : bp) * v->amp * (v->ftype == FO_BOTTLE || v->ftype == FO_WOOD ? 2.2f : 1.0f);
        if (v->metal > 0.0f && h_env_busy(&v->fm.amp))
            out += h_fm(&v->fm) * v->metal * 0.35f;
        if (v->tone_amp > 0.0f) {
            v->ph = h_wrap(v->ph + v->tone_hz * H_ISR);
            out += h_sin(v->ph) * v->tone_amp;
            v->tone_amp *= v->tone_k;
        }
        break;
    }
    v->amp *= v->amp_k;
    if (v->amp < 2.5e-3f && v->tone_amp < 2.5e-3f && !h_env_busy(&v->fm.amp))   /* -52 dB: gone */
        v->on = 0;
    return out * v->gain;
}

/* ------------------------------------------------------------------------------------------ sub and pad --- */
static int chord_root_note(void)                        /* the chord's root in the sub's octave */
{
    int n = h_deg_note(pool(), 36 + par[P_ROOT], (int)en_chord) + 12 * par[P_SUBOCT];
    while (n < 26)
        n += 12;
    while (n > 50)
        n -= 12;
    return n;
}

static void sub_on(int note, float dec_mul)
{
    sub_f = h_mtof((float)note);
    sub_tgt_amp = 1.0f;
    sub_k = decay_mul(ms_curve(par[P_SUBDECAY], 120.0f, 2400.0f) * dec_mul);
    en_sub_lit = 20;
}

static void chord_set(uint32_t bar)
{
    int bars = 1 << par[P_CHORDBARS], deg = PROGS[par[P_PROG]][(bar / (uint32_t)bars) & 3u], pcs[4], from[4], to[4], i;
    en_chord = (uint8_t)deg;
    h_chord(pool(), par[P_ROOT], deg, 4, pcs);
    for (i = 0; i < EN_NPAD; i++)
        from[i] = pad_note[i];
    h_voice_lead(from, pcs, EN_NPAD, to, 53, 79);
    for (i = 0; i < EN_NPAD; i++) {
        pad_note[i] = to[i];
        if (!(pad_live && i == EN_NPAD - 1))
            pad_freq_t[i] = h_mtof((float)to[i]);
    }
    chord_ver++;
}

static void pad_start(void)
{
    int i;
    float det = (float)par[P_DETUNE] * 0.01f;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].ratio = 2.0f;
        pad[i].idx = 0.28f;
        pad[i].fb = 0.3f;
        pad[i].det = 1.0f + ((i & 1) ? 1.0f : -1.0f) * det * 0.007f * (1.0f + 0.5f * (float)(i >> 1));
        h_env_set(&pad[i].amp, 900.0f, 3000.0f, 0.8f, 1800.0f);
        h_env_set(&pad[i].mod, 1500.0f, 4000.0f, 0.5f, 1800.0f);
        h_fm_on(&pad[i], pad_freq_t[i], 0.55f);
    }
}

static void pad_stop(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++)
        h_env_off(&pad[i].amp);
}

/* ----------------------------------------------------------------------------------------------- the form --- */
static void return_lane(int lane)
{
    en_away &= (uint8_t)~(1u << lane);
}

static void leave_now(void)
{
    away_state = 1;
    en_away = 0x0F;
    en_returning = 0;
    away_left = (uint32_t)par[P_AWAY];
    bars_since_leave = 0;
    en_leaves++;
}

static void bar_start(void)                             /* every bar: leave and return, the chord, the sub's floor */
{
    en_bar++;
    if (away_state == 0) {
        if (par[P_EVERY] > 0 && ++bars_since_leave >= (uint32_t)par[P_EVERY]) {
            bars_since_leave = 0;
            if (par[P_GROOVE] != GR_NONE && h_frand(&rng) * 100.0f < (float)par[P_CHANCE])
                leave_now();
        }
    } else if (away_state == 1) {
        if (away_left > 0)
            away_left--;
        if (away_left == 0) {
            away_state = 2;
            en_returning = 1;
            if (par[P_RETURN] == RET_CHANGED) {         /* they come back changed */
                loose_var = h_grand(&rng) * 30.0f;
                ghost_var = h_grand(&rng) * 35.0f;
            }
        }
    }
    if (away_state == 2) {
        static const uint8_t ORDER[3][4] = {{L_KICK, L_SNARE, L_HAT, L_FOLEY}, {L_HAT, L_FOLEY, L_SNARE, L_KICK},
                                            {L_HAT, L_KICK, L_FOLEY, L_SNARE}};
        if (par[P_RETURN] == RET_TOGETHER) {
            en_away = 0;
        } else {
            const uint8_t *o = ORDER[par[P_RETURN] == RET_KICK ? 0 : par[P_RETURN] == RET_HATS ? 1 : 2];
            int i;
            for (i = 0; i < 4; i++)
                if (en_away >> o[i] & 1u) {
                    return_lane(o[i]);
                    break;
                }
        }
        if (!en_away) {
            away_state = 0;
            en_returning = 0;
        }
    }
    if ((en_bar - 1u) % (uint32_t)(1 << par[P_CHORDBARS]) == 0u)
        chord_set(en_bar - 1u);
    if (!kick_in_bar || (en_away & 1u))                 /* no kick to carry the sub: it sounds on the bar */
        sub_on(chord_root_note(), 2.5f);
    kick_in_bar = 0;
}

static void cycle_wrap(void)
{
    en_cycle++;
    roll_cycle();
    cursor = 0;
}

/* ------------------------------------------------------------------------------------------- the playhead --- */
static void trigger(uint8_t idx, int vel)
{
    int lane, ghost = idx >= 128u;
    if (ghost) {
        lane = ghosts[idx - 128u].lane;
        en_ghost_lit = (uint8_t)(lane + 1);
    } else {
        lane = en_ev[idx].lane;
        en_ev[idx].lit = 20;
    }
    if (lane >= EN_NLANES || (en_mutes >> lane & 1u) || (en_away >> lane & 1u))
        return;
    drum_on(lane, vel, ghost);
    if (lane == L_KICK && !ghost) {
        duck_env = 1.0f;
        kick_in_bar = 1;
        en_kick_lit = 20;
        sub_on(chord_root_note(), 1.0f);
    }
}

static void fire_add(float t_from_pos, uint8_t idx, uint8_t vel, uint32_t n)
{
    float off = t_from_pos / tick_rate;
    if (nfire >= 24u)
        return;
    fire[nfire].off = (uint16_t)(off < 0.0f ? 0.0f : off >= (float)n ? (float)(n - 1u) : off);
    fire[nfire].idx = idx;
    fire[nfire].vel = vel;
    nfire++;
}

/* the block's events: those whose time falls inside it get a sample offset; the cycle and the bar turn inside it */
static void schedule_block(uint32_t n)
{
    float end = pos + tick_rate * (float)n;
    nfire = 0;
    while (cursor < nsched && (float)sched[cursor].t < end) {
        fire_add((float)sched[cursor].t - pos, sched[cursor].idx, sched[cursor].vel, n);
        cursor++;
    }
    if (bar_in_cycle == 0u && end >= (float)EN_BAR) {
        bar_in_cycle = 1;
        bar_start();
    }
    if (end >= (float)EN_CYCLE) {
        float rem = end - (float)EN_CYCLE;
        cycle_wrap();
        bar_in_cycle = 0;
        bar_start();
        while (cursor < nsched && (float)sched[cursor].t < rem) {
            fire_add((float)sched[cursor].t + (float)EN_CYCLE - pos, sched[cursor].idx, sched[cursor].vel, n);
            cursor++;
        }
        end = rem;
    }
    pos = end;
    en_pos = pos;
}

static void play_start(void)
{
    pos = 0.0f;
    en_pos = 0.0f;
    bar_in_cycle = 0;
    en_bar = 0;
    en_cycle = 0;
    kick_in_bar = 0;
    drag_ph = 0.0f;
    roll_cycle();
    cursor = 0;
    en_playing = 1;
    chord_set(0);
    pad_start();
    bar_start();
}

static void play_stop(void)
{
    en_playing = 0;
    en_recording = 0;
    nfire = 0;
    pad_stop();
    sub_tgt_amp = 0.0f;
}

static void quiet(void)
{
    int i;
    for (i = 0; i < EN_NPERC; i++)
        pv[i].on = 0;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].amp.st = pad[i].mod.st = H_ENV_OFF;
        pad[i].amp.v = pad[i].mod.v = 0.0f;
    }
    sub_amp = sub_tgt_amp = 0.0f;
    H_PLATE_INIT(&plate, pl);
    echo_init();
    rate_set();
}

/* a white key: the lane's hit now; while recording, kept at this very moment (unwarped: it comes back here) */
static void hit(int lane, int vel)
{
    if (lane < 0 || lane >= EN_NLANES || vel <= 0)
        return;
    drum_on(lane, vel, 0);
    if (lane == L_KICK) {
        duck_env = 1.0f;
        kick_in_bar = 1;
        en_kick_lit = 20;
        sub_on(chord_root_note(), 1.0f);
    }
    if (en_recording && en_playing)
        user_add((int)(unwarp(pos) + 0.5f), lane, vel);
}

static void note(int lane, int deg, int on)
{
    int n;
    if (lane == L_SUB) {
        if (on)
            sub_on(h_deg_note(pool(), 36 + par[P_ROOT], deg) + 12 * par[P_SUBOCT], 1.5f);
    } else if (lane == L_PAD) {
        if (on) {
            n = h_deg_note(pool(), 60 + par[P_ROOT], deg);
            pad_live = 1;
            pad_live_note = n;
            pad_freq_t[EN_NPAD - 1] = h_mtof((float)n);
            pad[EN_NPAD - 1].freq = pad_freq_t[EN_NPAD - 1];
            if (!h_env_busy(&pad[EN_NPAD - 1].amp))
                h_fm_on(&pad[EN_NPAD - 1], pad_freq_t[EN_NPAD - 1], 0.55f);
            else
                h_env_on(&pad[EN_NPAD - 1].mod);
        } else {
            pad_live = 0;
            pad_freq_t[EN_NPAD - 1] = h_mtof((float)pad_note[EN_NPAD - 1]);
            if (!en_playing)
                h_env_off(&pad[EN_NPAD - 1].amp);
        }
    }
}

/* ------------------------------------------------------------------------------------------------- apply --- */
static void bed_set(void)
{
    float d = (float)par[P_DUST] * 0.01f;
    int b = par[P_BED];
    bedA.type = b == BED_CRACKLE || b == BED_BOTH ? H_BED_CRACKLE : b == BED_RAIN ? H_BED_RAIN : b == BED_HISS ? H_BED_HISS
             : b == BED_ROOM ? H_BED_ROOM : H_BED_NONE;
    bedB.type = b == BED_BOTH ? H_BED_RAIN : H_BED_NONE;
    bedA.level = d * (bedA.type == H_BED_CRACKLE ? 0.55f : bedA.type == H_BED_RAIN ? 0.4f : 0.3f);
    bedB.level = d * 0.3f;
    bedA.density = 0.3f + 0.7f * d;
    bedB.density = 0.3f + 0.7f * d;
}

static void apply(int p, int v)
{
    par[p] = (int16_t)v;
    switch (p) {
    case P_TEMPO: rate_set(); rebuild(); break;
    case P_GROOVE: rebuild(); break;
    case P_SHUFFLE:
        schedule_build();                                /* the rest of this cycle moves with it */
        cursor_from_pos();
        break;
    case P_LOOSE: case P_GHOSTS: case P_WANDER: case P_FLAM:
        break;                                          /* the next roll */
    case P_DEC_KICK: lane_dec[L_KICK] = ms_curve(v, 80.0f, 700.0f); break;
    case P_DEC_SNARE: lane_dec[L_SNARE] = ms_curve(v, 60.0f, 700.0f); break;
    case P_DEC_HAT: lane_dec[L_HAT] = ms_curve(v, 20.0f, 350.0f); break;
    case P_DEC_FOLEY: lane_dec[L_FOLEY] = ms_curve(v, 40.0f, 1400.0f); break;
    case P_EVSEL:
        en_sel = (uint8_t)(en_nev ? (v >= en_nev ? en_nev - 1 : v) : 0);
        break;
    case P_DUCK:
        duck_depth = (float)v * 0.01f * 0.85f;
        duck_k = decay_mul((0.25f + 1.25f * (float)v * 0.01f) * 60000.0f / tempo_now() * 0.35f);
        break;
    case P_DUST: case P_BED: bed_set(); break;
    case P_PROG: case P_CHORDBARS: case P_ROOT: case P_SCALE:
        if (en_playing)
            chord_set(en_bar ? en_bar - 1u : 0u);
        break;
    case P_DETUNE: {
        int i;
        float det = (float)v * 0.01f;
        for (i = 0; i < EN_NPAD; i++)
            pad[i].det = 1.0f + ((i & 1) ? 1.0f : -1.0f) * det * 0.007f * (1.0f + 0.5f * (float)(i >> 1));
        break;
    }
    case P_PAD: pad_lvl = (float)v * 0.01f; pad_lvl *= pad_lvl * 0.3f; break;
    case P_SUB: sub_lvl = (float)v * 0.01f * 0.55f; break;
    case P_DELAY: delay_send = (float)v * 0.01f; delay_send *= delay_send; break;
    case P_FEEDBACK: echo.fb = (float)v * 0.01f * 0.82f; break;
    case P_ROOM:
        room_send = (float)v * 0.01f;
        room_send *= room_send * 0.9f;
        pad_rev = room_send * 0.4f;
        h_plate_set(&plate, 0.72f + 0.2f * (float)v * 0.01f, 0.45f, 0.5f, 10.0f);
        break;
    default: break;
    }
}

/* --------------------------------------------------------------------------------------------- commands --- */
static void drain(void)
{
    h_cmd_t c;
    while (h_take(&cq, &c)) {
        switch (c.c) {
        case C_SET: apply(c.a, (int)c.b); break;
        case C_PLAY:
            if (c.a && !en_playing)
                play_start();
            else if (!c.a && en_playing)
                play_stop();
            break;
        case C_HIT: hit(c.a, (int)c.b); break;
        case C_NOTE: note(c.a, (int)(c.b >> 1), (int)(c.b & 1)); break;
        case C_REC: en_recording = (uint8_t)(c.a != 0); break;
        case C_ADD:
            if (c.a >= 0 && c.a < EN_NLANES)
                user_add((int)c.b * EN_EIGHTH + (int)(h_grand(&rng) * loose_ms() * tpm), c.a, 100);
            break;
        case C_SELECT: apply(P_EVSEL, c.a); break;
        case C_EDIT: edit(c.a, (int)c.b); break;
        case C_REMOVE: remove_sel(); break;
        case C_NUDGE: edit(EV_MOVE, c.a); break;
        case C_MUTE:
            if (c.b)
                en_mutes |= (uint8_t)(1u << c.a);
            else
                en_mutes &= (uint8_t)~(1u << c.a);
            break;
        case C_LEAVE: leave_now(); break;
        case C_CLEAR: clear_edits(); break;
        case C_PATTERN: {
            int i, l, k;
            for (l = 0; l < EN_NLANES; l++)
                for (k = 0; k < EN_SLOTS; k++)
                    en_mods[l][k] = shadow_mods[l][k];
            for (i = 0; i < shadow_n; i++)
                en_user[i] = shadow_user[i];
            en_nuser = shadow_n;
            en_seed_now = shadow_seed ? shadow_seed : 0x5EED5u;
            rng.s = en_seed_now * 2654435761u | 1u;
            edits_changed();
            break;
        }
        case C_PANIC:
            play_stop();
            quiet();
            break;
        case C_CLOCK:
            if (c.a == EN_CLK_TICK) {
                uint32_t dt = sample_clk - ext_last_clk;
                ext_last_clk = sample_clk;
                ext_age = 0;
                if (!en_ext) {
                    en_ext = 1;
                    ext_interval = 60.0f * H_SR / (24.0f * (float)par[P_TEMPO]);
                    ext_ticks = (uint32_t)(pos / 40.0f);
                } else if (dt > 100u && dt < 20000u) {
                    ext_interval += ((float)dt - ext_interval) * 0.15f;
                    en_tempo_now = fm_clampf(60.0f * H_SR / (24.0f * ext_interval), 40.0f, 240.0f);
                }
                ext_ticks++;
                if (en_playing) {                        /* the phase: the tick's place against the playhead */
                    float expect = (float)((ext_ticks % 192u) * 40u), err = expect - pos;
                    if (err > (float)(EN_CYCLE / 2))
                        err -= (float)EN_CYCLE;
                    if (err < -(float)(EN_CYCLE / 2))
                        err += (float)EN_CYCLE;
                    ext_adj = 1.0f + fm_clampf(err * (1.0f / 400.0f), -0.08f, 0.08f);
                }
                rate_set();
            } else if (c.a == EN_CLK_START) {
                ext_ticks = 0;
                play_start();
            } else if (c.a == EN_CLK_STOP) {
                play_stop();
            }
            break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ render --- */
void en_init(void)
{
    int i;
    for (i = 0; i < P_N; i++)
        par[i] = EN_PARAMS[i].def;
    en_playing = en_recording = en_ext = en_lane = en_mutes = en_away = en_returning = en_sel = 0;
    en_bar = en_cycle = en_leaves = 0;
    away_state = 0;
    bars_since_leave = away_left = 0;
    loose_var = ghost_var = 0.0f;
    pos = 0.0f;
    en_pos = 0.0f;
    nfire = 0;
    ext_adj = drag_f = 1.0f;
    sub_amp = sub_tgt_amp = 0.0f;
    pad_live = 0;
    for (i = 0; i < EN_NPERC; i++)
        pv[i].on = 0;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].amp.st = pad[i].mod.st = H_ENV_OFF;
        pad[i].amp.v = pad[i].mod.v = 0.0f;
    }
    rng.s = 0xA5A5A5A5u;
    h_bed_init(&bedA, 0x777u);
    h_bed_init(&bedB, 0x999u);
    H_PLATE_INIT(&plate, pl);
    echo_init();
    for (i = 0; i < EN_NPAD; i++) {
        pad_note[i] = 57 + 4 * i;
        pad_freq_t[i] = h_mtof((float)pad_note[i]);
        pad[i].freq = pad_freq_t[i];
    }
    mods_clear();
    en_nuser = 0;
    rate_set();
    for (i = 0; i < P_N; i++)
        apply(i, par[i]);
    chord_set(0);
    rebuild();
}

static void control(uint32_t n)                         /* once a block: the slow things */
{
    uint32_t i;
    float fn = (float)n, k = 1.0f - fm_expf(-fn * (1.0f / (0.06f * H_SR)));
    /* drag: the tempo breathes over DRAGBARS bars */
    if (en_playing && par[P_DRAG]) {
        drag_ph = h_wrap(drag_ph + tick_rate * fn / ((float)EN_BAR * (float)par[P_DRAGBARS]));
        drag_f = 1.0f + 0.02f * (float)par[P_DRAG] * 0.01f * h_sin(drag_ph);
    } else {
        drag_f += (1.0f - drag_f) * 0.01f;
    }
    rate_set();
    /* the pad glides to its chord */
    for (i = 0; i < EN_NPAD; i++)
        pad[i].freq += (pad_freq_t[i] - pad[i].freq) * k;
    pad_g += (pad_lvl - pad_g) * 0.05f;
    sample_clk += n;
    if (en_ext && ++ext_age > 80u) {                    /* the clock stopped coming */
        en_ext = 0;
        ext_adj = 1.0f;
        rate_set();
    }
    for (i = 0; i < en_nev; i++)
        if (en_ev[i].lit)
            en_ev[i].lit--;
    if (en_sub_lit)
        en_sub_lit--;
    if (en_kick_lit)
        en_kick_lit--;
    if (en_ghost_lit && (sample_clk & 2048u))
        en_ghost_lit = 0;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float g = h_gain(gain_q12, master_trim);
    const int dub = par[P_GROOVE] == GR_DUB;
    float peak = 0.0f;
    uint32_t i, fi = 0;
    int v;
    drain();
    control(n);
    if (en_playing)
        schedule_block(n);
    else
        nfire = 0;
    for (i = 0; i < n; i++) {
        float dl = 0.0f, dr = 0.0f, s, p = 0.0f, sb, rv = 0.0f, dly = 0.0f, rl, rr, e, bl, br, bl2, br2, dg, l, r;
        while (fi < nfire && fire[fi].off == i) {
            trigger(fire[fi].idx, fire[fi].vel);
            fi++;
        }
        /* the drums */
        for (v = 0; v < EN_NPERC; v++) {
            pv_t *d = &pv[v];
            if (!d->on)
                continue;
            s = pv_run(d);
            dl += s * (1.0f - d->pan * 0.5f);
            dr += s * (1.0f + d->pan * 0.5f);
            if (d->lane == L_SNARE || d->lane == L_FOLEY) {
                rv += s * room_send;
                dly += s * delay_send;
            } else if (d->lane == L_HAT) {
                dly += s * delay_send * 0.5f;
            } else if (dub) {
                dly += s * 0.45f;
            }
        }
        /* the pad, the sub */
        for (v = 0; v < EN_NPAD; v++)
            if (h_env_busy(&pad[v].amp))
                p += h_fm(&pad[v]);
        sub_amp += (sub_tgt_amp - sub_amp) * 0.004f;
        sub_tgt_amp *= sub_k;
        sub_ph = h_wrap(sub_ph + sub_f * H_ISR);
        sb = h_sin(sub_ph);
        sb = sb * (1.15f - 0.25f * sb * sb) * sub_amp * sub_lvl;   /* a little of the second harmonic */
        sb = h_lp1(&sub_lp, sb, 0.03f);
        /* the duck, the space, the bed */
        duck_env *= duck_k;
        dg = 1.0f - duck_depth * duck_env;
        p *= pad_g * dg;
        rv += p * pad_rev;
        h_plate(&plate, rv, &rl, &rr);
        e = echo_run(dly);
        h_bed(&bedA, &bl, &br);
        h_bed(&bedB, &bl2, &br2);
        l = dl * 0.55f + sb + p + rl + e * 0.7f + (bl + bl2) * dg;
        r = dr * 0.55f + sb + p * 0.98f + rr + e * 0.7f + (br + br2) * dg;
        l = h_dc(&dcl, l);
        r = h_dc(&dcr, r);
        h_lim(&lim, &l, &r);
        if (fm_fabsf(l) > peak)
            peak = fm_fabsf(l);
        h_out(out + 2 * i, l, r, g);
    }
    en_duck = 1.0f - duck_depth * duck_env;
    en_level = h_lp1(&lvl_lp, peak, 0.3f);
}
