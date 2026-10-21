/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* MIDNIGHT ROUTE's engine (engine.h). One compilation unit, -O2. */
#include "engine.h"

/* ---------------------------------------------------------------------------------------------- parameters --- */
static const char *const SHAPE_NAME[SH_N] = {"Arch", "Slow burn", "Collapse", "Waves", "Midnight", "Dawn", "Flat"};
static const char *const CITY_NAME[EN_NCITY] = {"South Ldn", "Estuary", "Night bus", "Tower", "Harbour", "Underpass",
                                                "Last train", "Fog lane"};
const char *const EN_PART_NAME[PT_N] = {"Sub", "Pad", "Voice", "Rain"};
static const char *const CELL_NAME[CELL_N] = {"Held", "Pulse", "Half", "2-step", "Swung", "Sparse", "Any"};
static const char *const CHANGE_NAME[4] = {"2 bars", "4 bars", "8 bars", "16 bars"};
static const char *const BASS_NAME[3] = {"Root", "Pedal", "Fifths"};
static const char *const END_NAME[2] = {"Loop", "Rest"};
const char *const EN_TAG_NAME[TAG_N] = {"Normal", "Dwell", "Skip", "Peak"};
const char *const EN_FUNC_NAME[EN_NFUNC] = {"i", "iv", "v", "VI", "III", "bII", "ii dim", "VII", "iv add9", "v sus", "5ths"};
#define PP(nm, def) {nm, 0, 100, def, 0, ""}, {nm, 0, 100, def, 0, ""}, {nm, 0, 100, def, 0, ""}, {nm, 0, 100, def, 0, ""}
#define PB(nm, def) {nm, -50, 50, def, 0, ""}, {nm, -50, 50, def, 0, ""}, {nm, -50, 50, def, 0, ""}, {nm, -50, 50, def, 0, ""}
const hp_t EN_PARAMS[P_N] = {
    {"Shape", 0, SH_N - 1, SH_MIDNIGHT, SHAPE_NAME, 0}, {"City", 0, EN_NCITY - 1, 0, CITY_NAME, 0}, {"Station", 0, 15, 0, 0, ""},
    {"Minutes", 4, 30, 12, 0, ""}, {"Ceiling", 0, 100, 80, 0, "%"}, {"Contrast", 0, 100, 65, 0, "%"}, {"Fog", 0, 100, 55, 0, ""},
    {"Part", 0, PT_N - 1, 1, EN_PART_NAME, 0},
    {"Drift", 0, 100, 30, 0, ""}, {"Rate", 0, 100, 40, 0, ""}, {"Timing", 0, 100, 25, 0, ""}, {"Dynamics", 0, 100, 40, 0, ""},
    {"Sub", 0, CELL_N - 1, CELL_HALF, CELL_NAME, 0}, {"Pad", 0, CELL_N - 1, CELL_HELD, CELL_NAME, 0},
    {"Voice", 0, CELL_N - 1, CELL_SPARSE, CELL_NAME, 0}, {"Rain", 0, CELL_N - 1, CELL_2STEP, CELL_NAME, 0},
    {"Range", 0, 100, 45, 0, ""}, {"Step", 0, 100, 35, 0, ""}, {"Repeat", 0, 100, 30, 0, "%"}, {"Rest", 0, 100, 30, 0, "%"},
    {"Filter", 0, 100, 60, 0, ""}, {"Tail", 0, 100, 50, 0, ""}, {"Swell", 0, 100, 50, 0, ""}, {"Echo", 0, 100, 35, 0, ""},
    {"Change", 0, 3, 1, CHANGE_NAME, 0}, {"Smooth", 0, 100, 70, 0, ""}, {"Bass", 0, 2, BASS_ROOT, BASS_NAME, 0}, {"End", 0, 1, END_LOOP, END_NAME, 0},
    {"Root", 0, 11, 9, H_NOTE_NAME, 0}, {"Scale", 0, H_SC_N - 1, 0, H_SCALE_NAME, 0}, {"Bed", 0, H_BED_N - 1, H_BED_RAIN, H_BED_NAME, 0},
    {"Tempo", 60, 140, 72, 0, ""},
    PB("Level", 0), PB("Slope", 0), PP("Jitter", 20),
    PP("Bright", 50), PP("Decay", 50), PP("Width", 50),
    {"Transpose", -12, 12, 0, 0, ""},
};
#undef PP
#undef PB

/* the shapes: the master density curve through the 16 stations, 0..100 */
static const uint8_t SHAPE_CURVE[SH_N][EN_NSTATION] = {
    {10, 20, 35, 50, 65, 80, 90, 100, 100, 90, 80, 65, 50, 35, 20, 10},          /* arch */
    {5, 8, 12, 16, 22, 28, 35, 42, 50, 58, 66, 75, 84, 92, 100, 100},            /* slow burn */
    {100, 100, 95, 88, 80, 70, 60, 50, 40, 32, 24, 16, 10, 6, 3, 2},             /* collapse */
    {20, 60, 90, 50, 20, 65, 95, 55, 25, 70, 100, 60, 25, 60, 85, 30},           /* waves */
    {10, 12, 15, 40, 55, 100, 70, 55, 45, 38, 30, 24, 18, 12, 8, 5},             /* midnight: long intro, brief peak, long tail */
    {0, 0, 0, 5, 10, 20, 90, 100, 100, 90, 30, 10, 5, 0, 0, 0},                  /* dawn: nothing, everything, gone */
    {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60},            /* flat */
};

/* the cities: a sound world each. The pad's FM recipe, the sub's, the voice's throat (shift: 1 a man, 1.17 a woman,
 * 1.35 a child, 0.85 a man slowed down) and breath, the percussion's colour, the echo (in beats) and its feedback,
 * the plate's damping, the swing */
typedef struct {
    float pad_ratio, pad_idx, pad_fb, pad_det, pad_a_ms, pad_r_ms;
    float sub_ratio, sub_idx, sub_fb;
    float v_shift, v_breath;
    float th_f0, hat_hz;
    float echo_beats, echo_fb, damp, swing;
} city_t;
static const city_t CITIES[EN_NCITY] = {
    {1.0f, 0.30f, 0.30f, 0.0035f, 900.0f, 2200.0f, 1.0f, 0.55f, 0.15f, 1.17f, 0.18f, 52.0f, 7000.0f, 0.75f, 0.42f, 0.55f, 0.16f},
    {2.0f, 0.22f, 0.45f, 0.0045f, 1400.0f, 3000.0f, 0.5f, 0.40f, 0.10f, 1.00f, 0.22f, 48.0f, 5500.0f, 1.5f, 0.35f, 0.65f, 0.10f},
    {1.0f, 0.42f, 0.20f, 0.0025f, 500.0f, 1600.0f, 1.0f, 0.70f, 0.20f, 1.35f, 0.14f, 58.0f, 8500.0f, 0.75f, 0.50f, 0.45f, 0.20f},
    {3.0f, 0.18f, 0.50f, 0.0060f, 1800.0f, 3500.0f, 2.0f, 0.35f, 0.25f, 0.85f, 0.26f, 44.0f, 4500.0f, 1.5f, 0.30f, 0.75f, 0.08f},
    {1.0f, 0.26f, 0.35f, 0.0040f, 1100.0f, 2600.0f, 1.0f, 0.50f, 0.12f, 1.17f, 0.20f, 50.0f, 6500.0f, 1.0f, 0.40f, 0.60f, 0.14f},
    {2.0f, 0.36f, 0.25f, 0.0030f, 700.0f, 1800.0f, 1.0f, 0.65f, 0.30f, 1.00f, 0.16f, 46.0f, 6000.0f, 0.75f, 0.45f, 0.50f, 0.18f},
    {1.0f, 0.20f, 0.40f, 0.0050f, 1600.0f, 3200.0f, 0.5f, 0.45f, 0.10f, 1.17f, 0.24f, 54.0f, 7500.0f, 1.5f, 0.38f, 0.70f, 0.12f},
    {2.0f, 0.28f, 0.30f, 0.0055f, 2200.0f, 4000.0f, 1.0f, 0.30f, 0.15f, 0.85f, 0.30f, 42.0f, 5000.0f, 1.0f, 0.32f, 0.80f, 0.06f},
};
const en_city_t EN_CITY[EN_NCITY] = {
    {"South Ldn", 9, H_SC_AEOLIAN, H_BED_RAIN, 72}, {"Estuary", 2, H_SC_DORIAN, H_BED_CRACKLE, 68},
    {"Night bus", 5, H_SC_AEOLIAN, H_BED_ROOM, 76}, {"Tower", 0, H_SC_PHRYGIAN, H_BED_WIND, 64},
    {"Harbour", 4, H_SC_PENTMIN, H_BED_RAIN, 70}, {"Underpass", 7, H_SC_HARMMIN, H_BED_HISS, 66},
    {"Last train", 10, H_SC_AEOLIAN, H_BED_CRACKLE, 74}, {"Fog lane", 3, H_SC_DORIAN, H_BED_ROOM, 60},
};

/* the rhythm cells: a bar of 16 steps as a mask; the families each SEQ knob chooses from */
#define M(...) (0u __VA_ARGS__)
#define S(n) | 1u << (n)
static const uint16_t CELL_MASK[16] = {
    M(S(0)),                                                   /* 0 held */
    M(S(0) S(4) S(8) S(12)),                                   /* 1 pulse */
    M(S(0) S(8)),                                              /* 2 half-time */
    M(S(0) S(3) S(8) S(11) S(14)),                             /* 3 2-step */
    M(S(0) S(3) S(4) S(7) S(8) S(11) S(12) S(15)),             /* 4 swung 16ths */
    M(S(2) S(6) S(10) S(14)),                                  /* 5 offbeat */
    M(S(0) S(5) S(11)),                                        /* 6 triplet-ish */
    M(S(0) S(10)),                                             /* 7 sparse */
    M(S(0) S(6) S(12)),                                        /* 8 dotted */
    M(S(0) S(2) S(4) S(6) S(8) S(10) S(12) S(14)),             /* 9 rolling */
    M(S(1) S(4) S(7) S(10) S(13)),                             /* 10 syncopated */
    M(S(2) S(8) S(9) S(14)),                                   /* 11 late */
    M(S(0) S(1)),                                              /* 12 a pair */
    M(S(12) S(14) S(15)),                                      /* 13 the end of the bar */
    M(S(0) S(4) S(10)),                                        /* 14 three */
    0u,                                                        /* 15 nothing */
};
#undef M
#undef S
static const uint8_t FAMILY[CELL_N][16] = {
    {0},                                                       /* held */
    {1, 8, 14},                                                /* pulse */
    {2, 7, 11},                                                /* half-time */
    {3, 10, 4},                                                /* 2-step */
    {4, 9, 10},                                                /* swung */
    {7, 12, 13, 15, 6},                                        /* sparse */
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},    /* any */
};
static const uint8_t FAMILY_N[CELL_N] = {1, 3, 3, 3, 3, 5, 16};

/* the chord functions: a scale degree and a kind */
enum { K_TRIAD, K_ADD9, K_SUS4, K_OPEN5, K_BII };
typedef struct { int8_t deg; uint8_t kind; } func_t;
static const func_t FUNC[EN_NFUNC] = {
    {0, K_TRIAD}, {3, K_TRIAD}, {4, K_TRIAD}, {5, K_TRIAD}, {2, K_TRIAD}, {0, K_BII}, {1, K_TRIAD}, {6, K_TRIAD},
    {3, K_ADD9}, {4, K_SUS4}, {0, K_OPEN5},
};

/* ------------------------------------------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_RESTART, C_TAG, C_PALETTE, C_BOOKMARK, C_BKCLEAR, C_ROUTE, C_PANIC, C_CLOCK };
static h_cmdq_t cq;
void en_set(int p, int v) { h_post(&cq, C_SET, p, v); }
void en_play(int on) { h_post(&cq, C_PLAY, on, 0); }
void en_restart(void) { h_post(&cq, C_RESTART, 0, 0); }
void en_tag(int s, int tag) { h_post(&cq, C_TAG, s, tag); }
void en_palette(uint32_t bits) { h_post(&cq, C_PALETTE, 0, (int32_t)(bits & 0x7FFu)); }
void en_bookmark(int s) { h_post(&cq, C_BOOKMARK, s, 0); }
void en_bookmarks_clear(void) { h_post(&cq, C_BKCLEAR, 0, 0); }
void en_panic(void) { h_post(&cq, C_PANIC, 0, 0); }
void en_clock(int msg) { h_post(&cq, C_CLOCK, msg, 0); }

static uint8_t shadow_tags[EN_NSTATION];              /* a route handed over by the UI */
static uint32_t shadow_bk[EN_NSTATION];
static volatile uint32_t shadow_palette, shadow_seed;
void en_route_load(const uint8_t *tags, uint32_t palette, const uint32_t *bookmarks, uint32_t seed)
{
    int i;
    for (i = 0; i < EN_NSTATION; i++) {
        shadow_tags[i] = tags[i] < TAG_N ? tags[i] : 0;
        shadow_bk[i] = bookmarks[i];
    }
    shadow_palette = palette & 0x7FFu;
    shadow_seed = seed;
    H_BARRIER();
    h_post(&cq, C_ROUTE, 0, 0);
}

/* --------------------------------------------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_resting, en_station, en_bar, en_bars = 1, en_tags[EN_NSTATION], en_nbookmarks, en_chord,
    en_chord_root, en_ext;
volatile uint8_t en_dens[PT_N][EN_NSTATION], en_master[EN_NSTATION];
volatile float en_pos, en_dnow[PT_N], en_level;
volatile uint32_t en_palette_bits = 0x7FFu, en_bookmarks[EN_NSTATION], en_pass, en_bars_total, en_fired, en_secs_left,
    en_seed_now = 0x51A7E5u;
volatile int8_t en_voice_note = -1;

static int16_t par[P_N];
static h_rng_t rng;                                     /* the render's free dice: drift, timing */
static float bar_len = 147000.0f, step16 = 9187.5f;     /* samples a bar, a sixteenth */
static float t_bar;                                     /* samples into the bar */
static uint32_t now_s;                                  /* samples since the start */
static int station, bar_in_st, bars_in_st = 1, bars_since_change = 1000, chord_f, first_bar = 1;
static uint32_t pass, station_seed;
static float dcur[PT_N], drift[PT_N], drift_t[PT_N];     /* each part's density: glided, and its drift */
static float mnow;                                      /* the master density now (the effects read it) */
static float fade = 1.0f, fade_t = 1.0f;
static float swell;                                     /* the bed's swell over a skipped station */
static int ext_ticks;
static uint32_t ext_age;
static uint32_t fired_at[PT_N];
static float xpose = 1.0f, xpose_t = 1.0f;              /* the transposition as a pitch factor, glided */

/* the events of the bar */
typedef struct { uint32_t t; uint8_t part, note, vel, len; } ev_t;
static ev_t ev[EN_MAX_EV];
static int nev, iev;

/* the chord: four pitch classes for the pad, the real tones for the voice */
static int chord_pcs[4], chord_n = 3, chord_root_pc;

/* the voices */
static h_fm_t sub;
static h_lp1_t sub_lp;
static uint32_t sub_until;
static int sub_note = -1;
static float sub_freq = 55.0f;
/* a pad voice: one modulator (with feedback) into two carriers a detune apart, one amplitude envelope */
typedef struct {
    float phm, phc1, phc2, last, f_cur, f_tgt, pan, ratio, idx, fb, det, vel;
    h_env_t amp, mod;
    int note;
} padv_t;
static float pad_tick(padv_t *v, float f)
{
    float m, mi = v->idx * h_env(&v->mod);
    m = h_sin(v->phm + v->fb * v->last);
    v->last = m;
    v->phm = h_wrap(v->phm + f * v->ratio * H_ISR);
    v->phc1 = h_wrap(v->phc1 + f * (1.0f - v->det) * H_ISR);
    v->phc2 = h_wrap(v->phc2 + f * (1.0f + v->det) * H_ISR);
    return (h_sin(v->phc1 + mi * m) + h_sin(v->phc2 + mi * m)) * 0.5f * h_env(&v->amp) * v->vel;
}
static padv_t pad[EN_NPAD];
static uint32_t pad_until;
static int pad_active = 2;
static float pad_glide_k = 0.001f;
static h_voice_t fv;
static h_env_t v_amp;
static float v_f_cur = 220.0f, v_f_tgt = 220.0f, v_vowel_t = 1.0f;
static uint32_t v_until;
static int v_last = 57;
static h_lp1_t v_lp;
static float th_ph, th_env, th_f0 = 50.0f, hat_env;
static h_svf_t hat_f;
static h_lp1_t hat_lp;
static float th_vel, hat_vel;

/* the effects */
static h_plate_t plate;
H_PLATE_BUFFERS(pl);
#define ECHO_N 35280u                                    /* 0.8 s */
static int16_t echo_buf[ECHO_N] H_POOL;
static h_echo_t echo;
static h_bed_t bed;
static h_svf_t flt_l, flt_r;
static float cut = 16000.0f, cut_t = 16000.0f, rev_send, rev_send_t, echo_send;
static h_lp1_t lvl_lp;
static h_lim_t lim = {0.0f, 1.0f};
static h_dc_t dcl, dcr;
static float master_trim = 0.6f;

/* --------------------------------------------------------------------------------------------------- tools --- */
static uint32_t hash32(uint32_t a, uint32_t b)
{
    uint32_t h = a ^ 0x9E3779B9u;
    h ^= b + 0x7F4A7C15u + (h << 6) + (h >> 2);
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
static float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static uint32_t pool(void) { return (uint32_t)H_SCALE[par[P_SCALE]]; }
static float pct(int p) { return (float)par[p] * 0.01f; }

/* an FM voice struck again without a phase reset (no click): the frequency, the velocity, the envelopes */
static void fm_retrig(h_fm_t *v, float freq, float vel)
{
    v->freq = freq;
    v->vel = vel;
    h_env_on(&v->amp);
    h_env_on(&v->mod);
}

/* ------------------------------------------------------------------------------------------------- the route --- */
static int station_len(int s)                           /* bars, by the tag; 0 skipped */
{
    float bar_sec = 240.0f / (float)par[P_TEMPO];
    int b = (int)((float)par[P_LENGTH] * 60.0f / 16.0f / bar_sec + 0.5f);
    if (b < 1)
        b = 1;
    if (en_tags[s] == TAG_SKIP)
        return 0;
    return en_tags[s] == TAG_DWELL ? 2 * b : b;
}

static int any_open(void)                               /* at least one station is not skipped */
{
    int s;
    for (s = 0; s < EN_NSTATION; s++)
        if (en_tags[s] != TAG_SKIP)
            return 1;
    return 0;
}

/* the density tables: the master curve after the ceiling and the contrast, each part's role on it, the ENV page's
 * offsets, the tags */
static void route_rebuild(void)
{
    int s, p;
    float ceil = pct(P_CEILING), c = pct(P_CONTRAST), floor;
    for (s = 0; s < EN_NSTATION; s++) {
        float m = (float)SHAPE_CURVE[par[P_SHAPE]][s] * 0.01f, mp, role[PT_N];
        floor = fm_powf(1.0f - c, 1.5f);                 /* contrast 0: every station at the ceiling */
        mp = ceil * (floor + (1.0f - floor) * fm_powf(m, 0.5f + 1.5f * c));
        if (en_tags[s] == TAG_PEAK)
            mp = ceil;
        mp = clamp01(mp);
        en_master[s] = (uint8_t)(mp * 100.0f + 0.5f);
        role[PT_SUB] = fm_powf(clamp01(mp * 3.0f), 0.6f);
        role[PT_RAIN] = clamp01(mp * 2.0f);
        role[PT_PAD] = clamp01((mp - 0.25f) * (1.0f / 0.5f));
        role[PT_VOICE] = clamp01((mp - 0.55f) * (1.0f / 0.35f));
        for (p = 0; p < PT_N; p++) {
            float d = role[p];
            uint32_t j = hash32(en_seed_now ^ (uint32_t)p * 7919u, (uint32_t)s);
            d += (float)par[P_LEVEL0 + p] * 0.01f;
            d += (float)par[P_SLOPE0 + p] * 0.01f * ((float)s * (1.0f / 7.5f) - 1.0f);
            d += ((float)(j >> 8) * (1.0f / 16777216.0f) - 0.5f) * (float)par[P_JITTER0 + p] * 0.01f * 0.6f;
            if (en_tags[s] == TAG_PEAK && d < ceil)
                d = ceil;
            en_dens[p][s] = (uint8_t)(clamp01(d) * 100.0f + 0.5f);
        }
    }
}

static uint32_t seed_of(int s, uint32_t ps) { return en_bookmarks[s] ? en_bookmarks[s] : hash32(hash32(en_seed_now, ps), (uint32_t)s + 1u) | 1u; }

static void secs_left(void)
{
    int s;
    uint32_t bars = (uint32_t)(bars_in_st - bar_in_st);
    for (s = station + 1; s < EN_NSTATION; s++)
        bars += (uint32_t)station_len(s);
    en_secs_left = (uint32_t)((float)bars * 240.0f / (float)par[P_TEMPO]);
}

static void rebars(void)                                 /* the station's bars again: the length or the tempo changed */
{
    bars_in_st = station_len(station);
    if (bars_in_st < 1)
        bars_in_st = 1;
    if (bar_in_st >= bars_in_st)
        bar_in_st = bars_in_st - 1;
    en_bars = (uint8_t)(bars_in_st > 255 ? 255 : bars_in_st);
}

static void tempo_set(void)
{
    bar_len = H_SR * 240.0f / (float)par[P_TEMPO];
    step16 = bar_len * (1.0f / 16.0f);
    h_echo_time(&echo, 60000.0f / (float)par[P_TEMPO] * CITIES[par[P_CITY]].echo_beats);
}

static void enter_station(int s)                        /* the station begins (or is jumped to) */
{
    station = s;
    bar_in_st = 0;
    bars_in_st = station_len(s);
    if (bars_in_st < 1)
        bars_in_st = 1;
    station_seed = seed_of(s, pass);
    en_station = (uint8_t)s;
    en_bar = 0;
    en_bars = (uint8_t)(bars_in_st > 255 ? 255 : bars_in_st);
    par[P_STATION] = (int16_t)s;
}

/* ------------------------------------------------------------------------------------------------- harmony --- */
static void chord_build(int f)                          /* the chord function f as four pitch classes */
{
    const func_t *fn = &FUNC[f];
    int st[12], m = h_scale_steps(pool(), st), root = par[P_ROOT], pcs[4], i;
    switch (fn->kind) {
    case K_BII:
        pcs[0] = (root + 1) % 12;
        pcs[1] = (root + 5) % 12;
        pcs[2] = (root + 8) % 12;
        pcs[3] = pcs[0];
        chord_n = 3;
        chord_root_pc = pcs[0];
        break;
    case K_SUS4:
        for (i = 0; i < 3; i++)
            pcs[i] = (root + st[((fn->deg + (i == 1 ? 3 : i == 2 ? 4 : 0)) % m + m) % m]) % 12;
        pcs[3] = pcs[0];
        chord_n = 3;
        chord_root_pc = pcs[0];
        break;
    case K_OPEN5:
        pcs[0] = (root + st[0]) % 12;
        pcs[1] = (root + st[4 % m]) % 12;
        pcs[2] = pcs[0];
        pcs[3] = pcs[1];
        chord_n = 2;
        chord_root_pc = pcs[0];
        break;
    case K_ADD9:
        h_chord(pool(), root, fn->deg, 3, pcs);
        pcs[3] = (root + st[((fn->deg + 1) % m + m) % m]) % 12;
        chord_n = 4;
        chord_root_pc = pcs[0];
        break;
    default:
        h_chord(pool(), root, fn->deg, 3, pcs);
        pcs[3] = pcs[0];
        chord_n = 3;
        chord_root_pc = pcs[0];
        break;
    }
    for (i = 0; i < 4; i++)
        chord_pcs[i] = pcs[i];
    chord_f = f;
    en_chord = (uint8_t)f;
    en_chord_root = (uint8_t)chord_root_pc;
}

static void pad_lead(void)                              /* the pad's voices to the chord, moving as little as they can */
{
    int from[EN_NPAD], to[EN_NPAD], i, smooth = par[P_SMOOTH];
    float ms = smooth < 50 ? 5.0f + (float)smooth : 55.0f + (float)(smooth - 50) * 40.0f;
    for (i = 0; i < EN_NPAD; i++)
        from[i] = pad[i].note;
    h_voice_lead(from, chord_pcs, EN_NPAD, to, 50, 74);
    pad_glide_k = h_k_ms(ms);
    for (i = 0; i < EN_NPAD; i++) {
        if (to[i] != pad[i].note && smooth < 50 && h_env_busy(&pad[i].amp))
            h_env_on(&pad[i].amp);
        pad[i].note = to[i];
        pad[i].f_tgt = h_mtof((float)to[i]);
    }
}

static void chord_choose(h_rng_t *r, int station_start)
{
    uint32_t allowed = en_palette_bits ? en_palette_bits : 1u;
    float w[EN_NFUNC], total = 0.0f, x;
    int f;
    for (f = 0; f < EN_NFUNC; f++) {
        w[f] = 0.0f;
        if (!(allowed >> f & 1u))
            continue;
        w[f] = 1.0f;
        if (f == chord_f && bars_since_change < 1000)
            w[f] = 0.15f;                                    /* not the same again, unless it is all there is */
        if (f == 0 && (station_start || mnow < 0.25f))
            w[f] *= 2.5f;                                    /* home at a station's door, and in the quiet */
        total += w[f];
    }
    x = h_frand(r) * total;
    for (f = 0; f < EN_NFUNC; f++) {
        x -= w[f];
        if (w[f] > 0.0f && x <= 0.0f)
            break;
    }
    if (f >= EN_NFUNC)
        f = 0;
    chord_build(f);
    pad_lead();
    bars_since_change = 0;
}

static int bass_note(int k)                             /* the sub's note for its k-th event in the bar */
{
    int pc = chord_root_pc, n;
    if (par[P_BASS] == BASS_PEDAL)
        pc = par[P_ROOT];
    else if (par[P_BASS] == BASS_FIFTHS && (k & 1))
        pc = (chord_root_pc + 7) % 12;
    n = 33 + ((pc - 9 + 24) % 12);                       /* A1 .. Ab2 */
    return n;
}

static int nearest_chord_tone(int m)
{
    int best = m, bd = 100, i;
    for (i = 0; i < chord_n; i++) {
        int c = h_nearest(m, chord_pcs[i], m - 11, m + 11), d = c > m ? c - m : m - c;
        if (d < bd) {
            bd = d;
            best = c;
        }
    }
    return best;
}

/* the voice's motif: a walk on the scale around its centre, chord tones on the strong steps */
static int motif_note(h_rng_t *r, int step)
{
    int centre = 57, range = 3 + par[P_RANGE] * 16 / 100, n = v_last, dir, sz;
    if (h_chance(r, pct(P_REPEAT)))
        return v_last;
    sz = 1 + h_irand(r, 1 + par[P_STEP] * 5 / 100);         /* 1 .. 6 semitones before the snap */
    dir = h_chance(r, 0.5f) ? 1 : -1;
    if (n > centre + range / 2 && h_chance(r, 0.7f))
        dir = -1;
    else if (n < centre - range / 2 && h_chance(r, 0.7f))
        dir = 1;
    n += dir * sz;
    if (n > centre + range)
        n = centre + range - h_irand(r, 3);
    if (n < centre - range)
        n = centre - range + h_irand(r, 3);
    n = (step & 3) == 0 ? nearest_chord_tone(n) : h_snap(pool(), par[P_ROOT], n);
    return n;
}

/* --------------------------------------------------------------------------------------------- the events --- */
static void ev_add(float t, int part, int note, int vel, int len)
{
    int i;
    uint32_t ti;
    if (nev >= EN_MAX_EV)
        return;
    if (t < 0.0f)
        t = 0.0f;
    if (t > bar_len - 2.0f)
        t = bar_len - 2.0f;
    ti = (uint32_t)t;
    for (i = nev; i > 0 && ev[i - 1].t > ti; i--)
        ev[i] = ev[i - 1];
    ev[i].t = ti;
    ev[i].part = (uint8_t)part;
    ev[i].note = (uint8_t)note;
    ev[i].vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    ev[i].len = (uint8_t)(len < 1 ? 1 : len);
    nev++;
}

/* the time of a step: the swing on the odd steps, the LFO page's timing spread */
static float step_time(h_rng_t *r, int step)
{
    float t = (float)step * step16;
    if (step & 1)
        t += CITIES[par[P_CITY]].swing * step16;
    t += h_grand(r) * (float)par[P_TIMING] * 0.4f * 44.1f;    /* up to 40 ms */
    return t;
}

static int pick_steps(h_rng_t *r, uint32_t mask, int n, int *steps)   /* n distinct steps of the mask, in time order */
{
    int all[16], m = 0, i, j, k;
    for (i = 0; i < 16; i++)
        if (mask >> i & 1u)
            all[m++] = i;
    if (n > m)
        n = m;
    for (i = 0; i < n; i++) {                                 /* a partial shuffle */
        j = i + h_irand(r, m - i);
        k = all[i];
        all[i] = all[j];
        all[j] = k;
    }
    for (i = 1; i < n; i++) {                                 /* sorted */
        k = all[i];
        for (j = i; j > 0 && all[j - 1] > k; j--)
            all[j] = all[j - 1];
        all[j] = k;
    }
    for (i = 0; i < n; i++)
        steps[i] = all[i];
    return n;
}

static int vel_of(h_rng_t *r, float base)
{
    float dyn = pct(P_DYNAMICS);
    return (int)(127.0f * clamp01(base * (1.0f - dyn * 0.7f * h_frand(r))));
}

/* one part's events for the bar, from its density */
static void gen_part(int p, h_rng_t *r)
{
    static const int MAXEV[PT_N] = {4, 2, 6, 8};
    float d = en_dnow[p];
    int fam = par[P_CELL_SUB + p], cell, steps[16], n = 0, i, gesture = 0;
    uint32_t mask;
    if (d <= 0.01f)
        return;
    cell = FAMILY[fam][h_irand(r, FAMILY_N[fam])];
    mask = CELL_MASK[cell];
    if (d < 0.2f) {                                           /* gesture: one phrase, somewhere, if at all */
        if (!h_chance(r, d * 4.0f))
            return;
        gesture = 1;
        n = p == PT_VOICE ? 2 + h_irand(r, 2) : p == PT_RAIN ? 2 : 1;
        if (!mask)
            mask = 1u;
        n = pick_steps(r, mask | (uint32_t)(1u << h_irand(r, 16)), n, steps);
    } else {
        if (!mask)
            return;                                           /* a rest bar */
        n = 1 + (int)((d - 0.2f) * (1.0f / 0.8f) * (float)(MAXEV[p] - 1) + 0.5f);
        n = pick_steps(r, mask, n, steps);
    }
    for (i = 0; i < n; i++) {
        int st = steps[i], gap = (i + 1 < n ? steps[i + 1] : 16) - st;
        float t = step_time(r, st);
        if (gesture)
            t += h_brand(r) * 2.0f * step16;                  /* +-1/8 */
        switch (p) {
        case PT_SUB:
            ev_add(t, p, bass_note(i), vel_of(r, 0.9f), cell == 0 ? 16 : gap > 8 ? 8 : gap);
            break;
        case PT_PAD:
            ev_add(t, p, 0, vel_of(r, 0.5f + 0.5f * d), cell == 0 ? 16 : gap);
            break;
        case PT_VOICE:
            if (h_chance(r, pct(P_REST)) && !gesture)
                break;
            v_last = motif_note(r, st);
            ev_add(t, p, v_last, vel_of(r, 0.75f), gap > 6 ? 6 : gap < 2 ? 2 : gap);
            break;
        default: {
            int thump = (st & 7) == 0 ? h_chance(r, 0.85f) : (st & 3) == 0 ? h_chance(r, 0.2f) : 0;
            ev_add(t, p, thump ? 0 : 1, vel_of(r, thump ? 0.9f : 0.55f), 1);
            break;
        }
        }
    }
}

/* the bar begins: the densities glide, the chord may change, every part places its events */
static void bar_gen(void)
{
    h_rng_t r, rc;
    int p, station_start = bar_in_st == 0;
    float m = (float)en_master[station] * 0.01f;
    r.s = hash32(station_seed, (uint32_t)bar_in_st + 17u) | 1u;
    rc.s = hash32(station_seed, 777u + (uint32_t)bar_in_st) | 1u;
    for (p = 0; p < PT_N; p++) {
        float tgt = (float)en_dens[p][station] * 0.01f;
        dcur[p] = first_bar ? tgt : dcur[p] + (tgt - dcur[p]) * 0.5f;      /* two bars to arrive */
        en_dnow[p] = clamp01(dcur[p] + drift[p]);
    }
    mnow = first_bar ? m : mnow + (m - mnow) * 0.5f;
    first_bar = 0;
    if (bars_since_change >= (2 << par[P_CHANGE]) || (station_start && en_dnow[PT_PAD] > 0.0f && bars_since_change >= 2))
        chord_choose(&rc, station_start);
    bars_since_change++;
    nev = iev = 0;
    for (p = 0; p < PT_N; p++)
        gen_part(p, &r);
}

/* ------------------------------------------------------------------------------------------------ voices --- */
static void pad_gate(int vel, int len)
{
    int i;
    float d = en_dnow[PT_PAD];
    pad_active = 1 + (int)(d * 3.0f + 0.5f);
    if (pad_active > EN_NPAD)
        pad_active = EN_NPAD;
    pad_until = now_s + (uint32_t)((float)len * step16);
    for (i = 0; i < EN_NPAD; i++) {
        padv_t *v = &pad[i];
        float lvl = (0.35f + 0.65f * d) * (float)vel * (1.0f / 127.0f) * 0.15f;
        if (i < pad_active) {
            v->vel = lvl;
            if (!h_env_busy(&v->amp) || v->amp.st == H_ENV_R) {
                h_env_on(&v->amp);
                h_env_on(&v->mod);
                v->f_cur = v->f_tgt;
            }
        } else {
            h_env_off(&v->amp);
        }
    }
}

static void fire(const ev_t *e)
{
    const city_t *c = &CITIES[par[P_CITY]];
    float vel = (float)e->vel * (1.0f / 127.0f);
    en_fired |= 1u << e->part;
    fired_at[e->part] = now_s;
    switch (e->part) {
    case PT_SUB:
        sub_until = now_s + (uint32_t)((float)e->len * step16) - (uint32_t)(step16 * 0.3f);
        sub_freq = h_mtof((float)e->note);
        if (sub_note != e->note || !h_env_busy(&sub.amp))
            fm_retrig(&sub, sub_freq * xpose, vel * 0.9f);
        else
            h_env_on(&sub.amp);
        sub_note = e->note;
        break;
    case PT_PAD:
        pad_gate(e->vel, e->len);
        break;
    case PT_VOICE:
        v_f_tgt = h_mtof((float)e->note);
        if (!h_env_busy(&v_amp) || v_amp.st == H_ENV_R)
            v_f_cur = v_f_tgt * (0.97f + 0.02f * h_frand(&rng));   /* a scoop into the note */
        fv.breath = c->v_breath;
        v_vowel_t = (float)h_irand(&rng, 5) * 0.999f;
        v_amp.v = v_amp.v < 0.0f ? 0.0f : v_amp.v;
        h_env_on(&v_amp);
        v_until = now_s + (uint32_t)((float)e->len * step16) - (uint32_t)(step16 * 0.25f);
        en_voice_note = (int8_t)e->note;
        break;
    default:
        if (e->note == 0) {
            th_env = 1.0f;
            th_vel = vel;
            th_f0 = c->th_f0;
        } else {
            hat_env = 1.0f;
            hat_vel = vel;
        }
        break;
    }
}

static void notes_off(void)
{
    int i;
    h_env_off(&sub.amp);
    for (i = 0; i < EN_NPAD; i++)
        h_env_off(&pad[i].amp);
    h_env_off(&v_amp);
    pad_until = sub_until = v_until = 0;
}

static void quiet(void)
{
    int i;
    sub.amp.st = H_ENV_OFF;
    sub.amp.v = 0.0f;
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].amp.st = H_ENV_OFF;
        pad[i].amp.v = 0.0f;
    }
    v_amp.st = H_ENV_OFF;
    v_amp.v = 0.0f;
    th_env = hat_env = 0.0f;
    H_PLATE_INIT(&plate, pl);
    h_echo_init(&echo, echo_buf, ECHO_N, 0x3C3u);
    tempo_set();
}

/* the timbres, from the city and the EDIT page */
static void timbres(void)
{
    const city_t *c = &CITIES[par[P_CITY]];
    float b, d, w;
    int i;
    b = pct(P_BRIGHT0);
    d = pct(P_DECAY0);
    sub.ratio = c->sub_ratio;
    sub.idx = c->sub_idx * (0.3f + 1.4f * b);
    sub.fb = c->sub_fb;
    sub.det = 1.0f;
    h_env_set(&sub.amp, 4.0f, 300.0f + 2500.0f * d, 0.35f + 0.5f * d, 120.0f + 400.0f * d);
    h_env_set(&sub.mod, 1.0f, 60.0f + 400.0f * b, 0.15f, 200.0f);
    b = pct(P_BRIGHT1);
    d = pct(P_DECAY1);
    w = pct(P_WIDTH1);
    for (i = 0; i < EN_NPAD; i++) {
        padv_t *v = &pad[i];
        v->ratio = c->pad_ratio;
        v->idx = c->pad_idx * (0.4f + 1.4f * b);
        v->fb = c->pad_fb;
        v->det = c->pad_det * (0.3f + w) * (1.0f + 0.15f * (float)i);
        h_env_set(&v->amp, c->pad_a_ms * (0.4f + 1.2f * d), 2000.0f, 0.85f, c->pad_r_ms * (0.4f + 1.6f * d));
        h_env_set(&v->mod, 2.0f, 1500.0f + 3000.0f * d, 0.5f + 0.4f * b, 2000.0f);
        v->pan = w * ((i & 1) ? 0.9f : -0.9f) * (i < 2 ? 1.0f : 0.5f);
    }
    b = pct(P_BRIGHT2);
    d = pct(P_DECAY2);
    fv.shift = c->v_shift;
    h_env_set(&v_amp, 30.0f + 120.0f * d, 800.0f, 0.8f, 120.0f + 900.0f * d);
    h_svf_set(&hat_f, c->hat_hz * (0.6f + 0.8f * pct(P_BRIGHT3)), 1.2f);
    (void)b;
}

/* ----------------------------------------------------------------------------------------------- the clock --- */
static void bar_end(void)
{
    int s, hops = 0;
    bar_in_st++;
    en_bars_total++;
    if (bar_in_st >= bars_in_st) {                           /* the next station, over the skipped ones */
        if (!any_open()) {
            s = (station + 1) & 15;
        } else {
            s = station;
            do {
                s++;
                if (s >= EN_NSTATION) {
                    s = 0;
                    pass++;
                    en_pass = pass;
                    if (par[P_END] == END_REST) {                /* the route ends: the bed alone */
                        en_resting = 1;
                        en_playing = 0;
                        fade_t = 0.0f;
                        notes_off();
                    }
                }
                hops++;
            } while (en_tags[s] == TAG_SKIP && hops < 32);
        }
        if (hops > 1)
            swell = 1.0f;                                        /* over a skipped station the bed swells */
        enter_station(s);
    }
    en_bar = (uint8_t)bar_in_st;
    bar_gen();
}

static void advance(float ns)
{
    t_bar += ns;
    while (iev < nev && (float)ev[iev].t <= t_bar)
        fire(&ev[iev++]);
    if (t_bar >= bar_len) {
        t_bar -= bar_len;
        if (t_bar >= bar_len)
            t_bar = 0.0f;
        bar_end();
        while (iev < nev && (float)ev[iev].t <= t_bar)
            fire(&ev[iev++]);
    }
    en_pos = ((float)bar_in_st + t_bar / bar_len) / (float)bars_in_st;
}

static void depart(int from_station)
{
    int s = from_station;
    if (any_open())
        while (en_tags[s] == TAG_SKIP)
            s = (s + 1) & 15;
    en_resting = 0;
    en_playing = 1;
    fade_t = 1.0f;
    t_bar = 0.0f;
    first_bar = 1;
    enter_station(s);
    bar_gen();
}

static void jump(int s)                                     /* SELECT: another station, now */
{
    if (!en_playing) {
        enter_station(s);
        return;
    }
    enter_station(s);
    fade = fade < 0.35f ? fade : 0.35f;
    nev = iev = 0;
    bar_gen();
}

static void pause_route(void)
{
    en_playing = 0;
    fade_t = 0.0f;
    notes_off();
}

/* -------------------------------------------------------------------------------------------------- apply --- */
static void fx_set(void)
{
    const city_t *c = &CITIES[par[P_CITY]];
    float fog = pct(P_FOG);
    h_plate_set(&plate, 0.55f + 0.42f * fog, c->damp * (0.5f + 0.6f * fog), 0.75f - 0.35f * fog, 8.0f + 36.0f * fog);
    echo.fb = c->echo_fb;
    echo.tone = 0.3f - 0.15f * fog;
    echo_send = pct(P_ECHO) * 0.8f;
    bed.type = (uint8_t)par[P_BED];
}

static void apply(int p, int v)
{
    par[p] = (int16_t)v;
    switch (p) {
    case P_TEMPO: tempo_set(); rebars(); break;
    case P_CITY: timbres(); fx_set(); tempo_set(); break;
    case P_SHAPE: case P_CEILING: case P_CONTRAST:
    case P_LEVEL0: case P_LEVEL1: case P_LEVEL2: case P_LEVEL3:
    case P_SLOPE0: case P_SLOPE1: case P_SLOPE2: case P_SLOPE3:
    case P_JITTER0: case P_JITTER1: case P_JITTER2: case P_JITTER3:
        route_rebuild();
        break;
    case P_LENGTH: rebars(); break;
    case P_STATION:
        if (v != station)
            jump(v);
        break;
    case P_TRANSPOSE: xpose_t = h_semis((float)v); break;
    case P_FOG: case P_ECHO: case P_BED: fx_set(); break;
    case P_ROOT: case P_SCALE:
        chord_build(chord_f);
        pad_lead();
        break;
    case P_SMOOTH: pad_lead(); break;
    default:
        if (p >= P_BRIGHT0 && p <= P_WIDTH3)
            timbres();
        break;
    }
}

static void drain(void)
{
    h_cmd_t c;
    while (h_take(&cq, &c)) {
        switch (c.c) {
        case C_SET: apply(c.a, (int)c.b); break;
        case C_PLAY:
            if (c.a && !en_playing)
                depart(en_resting ? 0 : station);
            else if (!c.a && en_playing)
                pause_route();
            break;
        case C_RESTART:
            notes_off();
            pass = 0;
            en_pass = 0;
            fade = 0.0f;
            depart(0);
            break;
        case C_TAG:
            if (c.a >= 0 && c.a < EN_NSTATION && c.b >= 0 && c.b < TAG_N)
                en_tags[c.a] = (uint8_t)c.b;
            route_rebuild();
            rebars();
            break;
        case C_PALETTE:
            en_palette_bits = (uint32_t)c.b;
            break;
        case C_BOOKMARK:
            if (c.a >= 0 && c.a < EN_NSTATION) {
                int i, n = 0;
                en_bookmarks[c.a] = c.a == station ? station_seed : seed_of(c.a, pass);
                for (i = 0; i < EN_NSTATION; i++)
                    n += en_bookmarks[i] != 0;
                en_nbookmarks = (uint8_t)n;
            }
            break;
        case C_BKCLEAR: {
            int i;
            for (i = 0; i < EN_NSTATION; i++)
                en_bookmarks[i] = 0;
            en_nbookmarks = 0;
            break;
        }
        case C_ROUTE: {
            int i, n = 0;
            for (i = 0; i < EN_NSTATION; i++) {
                en_tags[i] = shadow_tags[i];
                en_bookmarks[i] = shadow_bk[i];
                n += shadow_bk[i] != 0;
            }
            en_nbookmarks = (uint8_t)n;
            en_palette_bits = shadow_palette;
            en_seed_now = shadow_seed ? shadow_seed : 0x51A7E5u;
            rng.s = en_seed_now * 2654435761u;
            route_rebuild();
            enter_station(par[P_STATION]);
            chord_build(chord_f);
            pad_lead();
            break;
        }
        case C_PANIC:
            pause_route();
            quiet();
            break;
        case C_CLOCK:
            if (c.a == EN_CLK_TICK) {
                en_ext = 1;
                ext_age = 0;
                ext_ticks++;
                if (en_playing)
                    advance(bar_len * (1.0f / 96.0f));
            } else if (c.a == EN_CLK_START) {
                if (!en_playing)
                    depart(0);
            } else if (c.a == EN_CLK_STOP) {
                pause_route();
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
    cq.w = cq.r = 0;                                      /* the runtime state from the top (a reboot too) */
    en_playing = en_resting = en_ext = 0;
    en_fired = 0;
    pass = en_pass = en_bars_total = 0;
    t_bar = 0.0f;
    now_s = 0;
    first_bar = 1;
    bars_since_change = 1000;
    chord_f = 0;
    fade = fade_t = 1.0f;
    swell = 0.0f;
    nev = iev = 0;
    sub_note = -1;
    sub_until = pad_until = v_until = 0;
    v_last = 57;
    th_env = hat_env = 0.0f;
    for (i = 0; i < PT_N; i++)
        dcur[i] = drift[i] = drift_t[i] = 0.0f;
    for (i = 0; i < EN_NSTATION; i++) {
        en_tags[i] = 0;
        en_bookmarks[i] = 0;
    }
    en_nbookmarks = 0;
    en_seed_now = 0x51A7E5u;
    {
        const h_fm_t fm0 = {0};
        const padv_t pv0 = {0};
        const h_env_t e0 = {0};
        sub = fm0;
        for (i = 0; i < EN_NPAD; i++)
            pad[i] = pv0;
        v_amp = e0;
    }
    rng.s = 0xA5A5A5A5u;
    h_bed_init(&bed, 0x777u);
    H_PLATE_INIT(&plate, pl);
    h_echo_init(&echo, echo_buf, ECHO_N, 0x3C3u);
    h_voice_init(&fv, 0x9191u);
    h_svf_set(&flt_l, 16000.0f, 0.6f);
    h_svf_set(&flt_r, 16000.0f, 0.6f);
    h_svf_set(&hat_f, 7000.0f, 1.2f);
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = 55 + 4 * i;
        pad[i].f_cur = pad[i].f_tgt = h_mtof((float)pad[i].note);
    }
    en_palette_bits = 0x7FFu;
    for (i = 0; i < P_N; i++)
        apply(i, par[i]);
    route_rebuild();
    chord_build(0);
    pad_lead();
    enter_station(0);
}

static void control(uint32_t n)                         /* once a block: the slow things */
{
    float fn = (float)n, fog = pct(P_FOG), q = mnow;
    const city_t *c = &CITIES[par[P_CITY]];
    int p, dwell = en_tags[station] == TAG_DWELL;
    /* the drift: each part's density walks within the station */
    for (p = 0; p < PT_N; p++) {
        float k = fn * (0.00001f + 0.0002f * pct(P_DRIFTRATE)) * (1.0f / 44.1f);
        drift[p] += (drift_t[p] - drift[p]) * (k > 0.5f ? 0.5f : k);
        if (h_chance(&rng, fn * (0.02f + 0.3f * pct(P_DRIFTRATE)) / H_SR))
            drift_t[p] = h_brand(&rng) * 0.3f * pct(P_DRIFT);
        if (en_fired >> p & 1u && now_s - fired_at[p] > 5000u)
            en_fired &= ~(1u << p);
    }
    /* the gates */
    if (sub_until && now_s >= sub_until) {
        h_env_off(&sub.amp);
        sub_until = 0;
    }
    if (pad_until && now_s >= pad_until) {
        for (p = 0; p < EN_NPAD; p++)
            h_env_off(&pad[p].amp);
        pad_until = 0;
    }
    if (v_until && now_s >= v_until) {
        h_env_off(&v_amp);
        v_until = 0;
    }
    if (!h_env_busy(&v_amp))
        en_voice_note = -1;
    /* the station-aware effects: the reverb opens in the quiet, the filter closes as the density falls, the tail
     * doubles on a DWELL station, the bed swells over a SKIP */
    rev_send_t = fog * (0.25f + 0.55f * (1.0f - q)) * 0.9f;
    cut_t = h_lerp(16000.0f, 400.0f, pct(P_FILTER) * (1.0f - q) * (1.0f - q));
    h_plate_set(&plate, 0.55f + 0.42f * fog + (dwell ? (0.985f - 0.55f - 0.42f * fog) * 0.6f * pct(P_TAIL) : 0.0f),
                c->damp * (0.5f + 0.6f * fog), 0.75f - 0.35f * fog, 8.0f + 36.0f * fog);
    swell -= fn / (bar_len * 2.0f);
    if (swell < 0.0f)
        swell = 0.0f;
    bed.level = 0.07f + 0.5f * fog * fog + swell * pct(P_SWELL) * 0.5f + (1.0f - q) * 0.1f * fog;
    bed.density = 0.4f + 0.6f * (1.0f - q);
    if (en_resting)
        bed.level += 0.1f;
    fade += (fade_t - fade) * (fade_t > fade ? 0.03f : 0.08f);
    xpose += (xpose_t - xpose) * 0.1f;
    cut += (cut_t - cut) * 0.1f;
    rev_send += (rev_send_t - rev_send) * 0.05f;
    h_svf_set(&flt_l, cut, 0.6f);
    flt_r.g = flt_l.g;
    flt_r.k = flt_l.k;
    flt_r.a1 = flt_l.a1;
    flt_r.a2 = flt_l.a2;
    flt_r.a3 = flt_l.a3;
    fv.vowel += (v_vowel_t - fv.vowel) * 0.08f;
    secs_left();
    if (en_ext && ++ext_age > 60u)
        en_ext = 0;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float g = h_gain(gain_q12, master_trim);
    float peak = 0.0f, sub_k = h_lp1_k(220.0f), v_k = h_lp1_k(5000.0f), vg = 0.0025f, sub_lvl, v_lvl, th_k, hat_k;
    uint32_t i;
    int v, flt_on;
    drain();
    control(n);
    sub_lvl = 0.34f;
    v_lvl = 0.3f;
    sub.freq = sub_freq * xpose;
    flt_on = cut < 14000.0f;
    th_k = 1.0f - 1.0f / (H_SR * (0.08f + 0.25f * pct(P_DECAY3)));
    hat_k = 1.0f - 1.0f / (H_SR * (0.02f + 0.08f * pct(P_DECAY3)));
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, s, x, rl, rr, bl, br, e, send = 0.0f, lp, bp, hp, w;
        if (en_playing && !en_ext)
            advance(1.0f);
        now_s++;
        /* the sub */
        if (h_env_busy(&sub.amp)) {
            s = h_lp1(&sub_lp, h_fm(&sub), sub_k) * 1.6f;
            s = fm_clampf(s, -1.0f, 1.0f);
            s = s * (1.5f - 0.5f * fm_fabsf(s)) * sub_lvl;      /* a soft knee */
            l += s;
            r += s;
        }
        /* the pad: four voices in pairs, gliding to their notes */
        for (v = 0; v < EN_NPAD; v++) {
            padv_t *pv = &pad[v];
            if (!h_env_busy(&pv->amp))
                continue;
            pv->f_cur += (pv->f_tgt - pv->f_cur) * pad_glide_k;
            s = pad_tick(pv, pv->f_cur * xpose);
            l += s * (1.0f - pv->pan * 0.5f);
            r += s * (1.0f + pv->pan * 0.5f);
            send += s * 0.25f;
        }
        /* the voice: a throat that stays where it is while the pitch moves */
        if (h_env_busy(&v_amp)) {
            v_f_cur += (v_f_tgt - v_f_cur) * vg;
            fv.freq = v_f_cur * xpose;
            s = h_lp1(&v_lp, h_voice(&fv), v_k) * h_env(&v_amp) * v_lvl;
            l += s * 0.9f;
            r += s * 1.1f;
            send += s * 0.8f;
        }
        /* the rain: a thump and a hat */
        if (th_env > 0.001f) {
            th_env *= th_k;
            th_ph = h_wrap(th_ph + th_f0 * (1.0f + 2.5f * th_env * th_env) * H_ISR);
            s = h_sin(th_ph) * th_env * th_vel * 0.36f;
            l += s;
            r += s;
        }
        if (hat_env > 0.001f) {
            hat_env *= hat_k;
            w = h_brand(&rng);
            h_svf(&hat_f, w, &lp, &bp, &hp);
            s = h_lp1(&hat_lp, bp + hp * 0.3f, 0.6f) * hat_env * hat_env * hat_vel * 0.15f;
            l += s * 1.15f;
            r += s * 0.85f;
            send += s * 0.5f;
        }
        /* the master: the fade, the filter that closes as the route empties */
        if (flt_on) {
            l = h_svf_lp(&flt_l, l);
            r = h_svf_lp(&flt_r, r);
        }
        l *= fade;
        r *= fade;
        send *= fade;
        /* the echo on the voice and the hats, the plate on it all, the bed under */
        e = h_echo(&echo, send * echo_send);
        x = (l + r) * 0.5f + e * 0.6f;
        h_plate(&plate, x * rev_send, &rl, &rr);
        h_bed(&bed, &bl, &br);
        l += rl + e * 0.4f + bl;
        r += rr + e * 0.5f + br;
        l = h_dc(&dcl, l);
        r = h_dc(&dcr, r);
        h_lim(&lim, &l, &r);
        if (fm_fabsf(l) > peak)
            peak = fm_fabsf(l);
        h_out(out + 2 * i, l, r, g);
    }
    en_level = h_lp1(&lvl_lp, peak, 0.3f);
}
