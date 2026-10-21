/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* VOICEMAIL's engine (engine.h). One compilation unit, -O2. */
#include "engine.h"

/* ---------------------------------------------------------------------------------------------- parameters --- */
static const char *const SINGER_NAME[SG_N] = {"Child", "Woman", "Man", "Choir", "Whisper", "Slowed", "Radio"};
static const char *const BOOK_NAME[BK_N] = {"Hymn", "Soul", "Lullaby", "Siren", "Plea", "Static"};
static const char *const CHORD_NAME[8] = {"Rest", "I", "II", "III", "IV", "V", "VI", "VII"};
enum { BED_NONE, BED_RAIN, BED_CRACKLE, BED_BOTH, BED_HISS, BED_WIND, BED_ROOM, BED_N };
static const char *const BED_NAME[BED_N] = {"None", "Rain", "Crackle", "Both", "Hiss", "Wind", "Room"};
static const char *const TRANS_NAME[4] = {"Unison", "Up 4th", "Down 5th", "Invert"};
static const char *const MODE_NAME[2] = {"Overlap", "Wait"};
static const char *const LIGHTS_NAME[3] = {"Off", "On", "Keys"};
static const char *const PADOCT_NAME[4] = {"-2", "-1", "0", "+1"};
const hp_t EN_PARAMS[P_N] = {
    {"Singer", 0, SG_N - 1, SG_MAN, SINGER_NAME, 0}, {"Book", 0, BK_N - 1, 0, BOOK_NAME, 0}, {"Pitch", -24, 12, 0, 0, ""},
    {"Fragment", 0, 100, 20, 0, "%"}, {"Stutter", 0, 100, 25, 0, "%"}, {"Reverse", 0, 100, 20, 0, "%"}, {"Wash", 0, 100, 50, 0, ""},
    {"Attack", 0, 100, 35, 0, ""}, {"Decay", 0, 100, 40, 0, ""}, {"Breath", 0, 100, 40, 0, ""}, {"Consonant", 0, 100, 40, 0, ""},
    {"Wow", 0, 100, 30, 0, ""}, {"Wow rate", 0, 100, 35, 0, ""}, {"Flutter", 0, 100, 15, 0, ""}, {"Dropout", 0, 100, 20, 0, "%"},
    {"Tempo", 50, 140, 70, 0, ""}, {"Density", 10, 100, 70, 0, "%"}, {"Silence", 1, 8, 3, 0, " bar"}, {"Repeat", 1, 4, 2, 0, ""},
    {"Chord 1", 0, 7, 1, CHORD_NAME, 0}, {"Chord 2", 0, 7, 6, CHORD_NAME, 0}, {"Chord 3", 0, 7, 3, CHORD_NAME, 0},
    {"Chord 4", 0, 7, 7, CHORD_NAME, 0},
    {"Bed", 0, BED_N - 1, BED_RAIN, BED_NAME, 0}, {"Bed level", 0, 100, 45, 0, ""}, {"High-pass", 0, 100, 30, 0, ""},
    {"Pad", 0, 100, 55, 0, ""},
    {"Delay", 1, 8, 2, 0, " bar"}, {"Transpose", 0, 3, TR_UP5, TRANS_NAME, 0}, {"Chance", 0, 100, 75, 0, "%"},
    {"Mode", 0, 1, 1, MODE_NAME, 0},
    {"Root", 0, 11, 9, H_NOTE_NAME, 0}, {"Scale", 0, H_SC_N - 1, 0, H_SCALE_NAME, 0}, {"Bars", 1, 8, 2, 0, " bar"},
    {"Lights", 0, 2, 1, LIGHTS_NAME, 0},
    {"Throat", -50, 50, 0, 0, ""}, {"Tone", 0, 100, 60, 0, ""}, {"Rasp", 0, 100, 25, 0, ""}, {"Vibrato", 0, 100, 25, 0, ""},
    {"Pad oct", -2, 1, -1, PADOCT_NAME, 0},
};

/* the singers: the formants' shift (1 a man), the breath, where the phrases sit (MIDI), the choir's detune in
 * semitones, a radio's band (0 none) */
typedef struct { float shift, breath, centre, detune, band, trim; } singer_t;
static const singer_t SINGER[SG_N] = {
    {1.35f, 0.14f, 72.0f, 0.0f, 0.0f, 0.6f},        /* child */
    {1.17f, 0.16f, 67.0f, 0.0f, 0.0f, 0.8f},        /* woman */
    {1.0f, 0.16f, 55.0f, 0.0f, 0.0f, 1.0f},         /* man */
    {1.08f, 0.2f, 60.0f, 0.12f, 0.0f, 0.8f},        /* choir: two, detuned */
    {1.1f, 0.88f, 62.0f, 0.0f, 0.0f, 2.0f},         /* whisper */
    {0.76f, 0.22f, 48.0f, 0.0f, 0.0f, 1.3f},        /* slowed: a man's formants at half speed */
    {1.05f, 0.25f, 60.0f, 0.0f, 1500.0f, 2.2f},     /* radio */
};
/* the contours: syllables as (scale degrees from the root, 1/16 notes) */
typedef struct { int8_t deg; uint8_t dur; } syl_t;
static const syl_t CONTOUR[CT_N][8] = {
    {{2, 2}, {1, 2}, {0, 4}, {-1, 8}},                                /* sigh: down, slowly */
    {{0, 4}, {4, 8}},                                                 /* call: up to the fifth, held */
    {{0, 2}, {0, 2}, {-1, 2}, {2, 8}},                                /* question: rising at the end */
    {{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 2}, {0, 2}},                 /* mantra: one note, said again */
    {{4, 1}, {3, 1}, {2, 1}, {1, 1}, {0, 4}},                         /* fall: a run down */
    {{0, 16}},                                                        /* held: one long note */
};
static const uint8_t CONTOUR_N[CT_N] = {4, 2, 4, 6, 5, 1};
/* the books: three contours each, how long (durations x mul / 2), how wide (degrees x spread / 2), the vowels */
typedef struct { uint8_t ct[3], mul, spread, vow[4]; } book_t;
static const book_t BOOK[BK_N] = {
    {{CT_HELD, CT_CALL, CT_SIGH}, 4, 2, {3, 0, 4, 3}},                /* hymn: long, o a u */
    {{CT_FALL, CT_QUESTION, CT_SIGH}, 1, 2, {1, 2, 0, 1}},            /* soul ad-lib: quick runs, e i a */
    {{CT_SIGH, CT_CALL, CT_MANTRA}, 2, 1, {4, 3, 0, 4}},              /* lullaby: small steps, u o a */
    {{CT_CALL, CT_HELD, CT_CALL}, 4, 4, {2, 1, 2, 1}},                /* siren: wide calls, i e */
    {{CT_QUESTION, CT_SIGH, CT_QUESTION}, 2, 2, {0, 5, 1, 6}},        /* plea: a e, breathy */
    {{CT_MANTRA, CT_HELD, CT_MANTRA}, 2, 0, {8, 9, 10, 8}},           /* static: one whispered note */
};
static const float VOW_POS[EN_VOW_N] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 0.0f, 3.0f, 4.0f, 0.0f, 2.0f, 4.0f};
static const float VOW_BREATH[EN_VOW_N] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.35f, 0.35f, 0.35f, 0.8f, 0.8f, 0.8f};

/* ------------------------------------------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_ARM, C_KEY, C_VOWEL, C_KEEP, C_CLEAR, C_MELODY, C_SEED, C_PANIC, C_CLOCK };
static h_cmdq_t cq;
void en_set(int p, int v) { h_post(&cq, C_SET, p, v); }
void en_play(int on) { h_post(&cq, C_PLAY, on, 0); }
void en_arm(int on) { h_post(&cq, C_ARM, on, 0); }
void en_key(int note, int vel, int on) { h_post(&cq, C_KEY, note, on ? vel : 0); }
void en_vowel(int v) { h_post(&cq, C_VOWEL, v, 0); }
void en_keep(void) { h_post(&cq, C_KEEP, 0, 0); }
void en_clear(void) { h_post(&cq, C_CLEAR, 0, 0); }
void en_seed(uint32_t s) { h_post(&cq, C_SEED, 0, (int32_t)(s & 0x7FFFFFFFu)); }
void en_panic(void) { h_post(&cq, C_PANIC, 0, 0); }
void en_clock(int msg) { h_post(&cq, C_CLOCK, msg, 0); }

static en_note_t shadow[EN_MAX_MELODY];               /* a melody handed over by the UI */
static volatile uint8_t shadow_n, shadow_steps;
static volatile uint32_t shadow_seed;
void en_melody_load(const en_note_t *notes, int n, int steps, uint32_t seed)
{
    int i;
    if (n > EN_MAX_MELODY)
        n = EN_MAX_MELODY;
    for (i = 0; i < n; i++)
        shadow[i] = notes[i];
    shadow_n = (uint8_t)n;
    shadow_steps = (uint8_t)steps;
    shadow_seed = seed;
    H_BARRIER();
    h_post(&cq, C_MELODY, 0, 0);
}

/* --------------------------------------------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_armed, en_writing, en_phase = PH_SILENCE, en_phases, en_chord_slot, en_vnote[2],
    en_stutter, en_reverse, en_dropout, en_nmelody, en_melody_steps, en_ext;
volatile uint32_t en_cycles, en_melody_ver, en_seed_now = 0x5EED5u, en_steps_total;
volatile float en_bar_pos, en_next_frac, en_level, en_vpos[2];
volatile int32_t en_next_bars, en_chord_root = -1;
volatile int8_t en_vsyl[2] = {-1, -1};
volatile en_phrase_t en_phrase[2];
volatile en_note_t en_melody[EN_MAX_MELODY];

static int16_t par[P_N];
static h_rng_t rng;
static float step16 = 9450.0f;                          /* samples per 1/16 at the tempo */
static float pos;                                       /* the playhead, in 1/16 within the bar (0..16) */
static int32_t abs16 = -1;                              /* 1/16 steps since the start */
static int last_sub = -1;
static int ext_ticks;
static uint32_t ext_age;

/* the ring the voice is heard through: D samples late, so a syllable can be stuttered or reversed as it comes */
#define RING_N 32768u
#define RING_D 6144u
static int16_t ring_buf[RING_N] H_POOL;
static h_dl_t ring;
enum { RM_NORMAL, RM_STUTTER, RM_REVERSE };
static struct {
    uint8_t mode, old_mode, swell;
    uint32_t t, old_t, len, reps, xf, pend_in, pend_mode, pend_len, pend_reps;
} rd;

/* the two voices */
typedef struct {
    h_voice_t fv;
    h_env_t amp;
    float freq, freq_t, vow_t, det, vib_ph, cons_left, gate_left;
    int32_t ph_start;                                   /* abs16 the phrase started at, -1 idle */
    int32_t off_at;                                     /* abs16 the syllable ends at, -1 none */
    uint8_t ph_i, own, live, cur, trans_mode;
    en_phrase_t phr;
} vox_t;
static vox_t vox[2];
static int vowel_hold = -1;                             /* a black key held: the keys' vowel */
static int interrupted;                                 /* a key cut a call phrase short: the form goes on when it ends */

/* the form */
static int32_t call_start = -1, answer_at = -1, chorus2_at = -1, silence_until = -1, next_from, next_at;
static int reps_left;
static float wash_boost;                                /* the chorus opens the wash */

/* the melody being written */
static int32_t mel_start = -1;
static uint8_t key_step[128];                           /* a held key's step + 1 while writing */
static uint8_t key_live[128];                           /* a held key sings (its voice is 0) */
static uint32_t last_sung_ver;                          /* the phrase voice 1 sang last, for KEEP */
static en_phrase_t last_sung;

/* the pad */
typedef struct { h_fm_t fm; float freq, freq_t; int note; } pad_t;
static pad_t pad[EN_NPAD];
static h_lp1_t pad_lp;
static int32_t chord_bar = -1;
static int chord_pcs[3], chord_n;

/* the effects */
static h_plate_t plate;
H_PLATE_BUFFERS(pl);
#define ECHO_N 32768u
static int16_t echo_buf[ECHO_N] H_POOL;
static h_echo_t echo;
static h_bed_t bed, bed2;
static h_svf_t radio_l;
static h_lp1_t hp_pre, lvl_lp;
static h_lim_t lim = {0.0f, 1.0f};
static h_dc_t dcl, dcr;
static float wash, wash_t, send_v, send_p, echo_send, drop_g = 1.0f, drop_t = 1.0f, drop_left, hp_k;
static float wow_ph, flut_ph, wow_f = 1.0f, pitch_f = 1.0f, pad_lvl, bed_lvl, tone_k = 0.5f, rasp = 1.0f;
static h_lp1_t tone_lp;
static float master_trim = 0.5f;

/* ------------------------------------------------------------------------------------------------ harmony --- */
static uint32_t pool_abs(void) { return (uint32_t)H_SCALE[par[P_SCALE]]; }
static int snap(int m) { return h_snap(pool_abs(), par[P_ROOT], m < 24 ? 24 : m > 100 ? 100 : m); }

/* note m (in the scale) moved deg scale degrees */
static int deg_from(int m, int deg)
{
    int st[12], n = h_scale_steps(pool_abs(), st), i, pc, idx = 0;
    m = snap(m);
    pc = ((m - par[P_ROOT]) % 12 + 12) % 12;
    for (i = 0; i < n; i++)
        if (st[i] == pc)
            idx = i;
    return h_deg_note(pool_abs(), m - st[idx], idx + deg);
}

/* the pad's chord in slot s: its tones as pitch classes; 0 at rest */
static int chord_of(int s, int *pcs)
{
    int d = par[P_CHORD1 + s];
    if (d <= 0)
        return 0;
    return h_chord(pool_abs(), par[P_ROOT], d - 1, 3, pcs);
}

/* the chord tone nearest the singer's centre: a phrase's root */
static int phrase_root(float centre)
{
    int c = (int)centre, best = snap(c), bd = 100, i;
    for (i = 0; i < chord_n; i++) {
        int m = h_nearest(c, chord_pcs[i], c - 7, c + 7), d = m > c ? m - c : c - m;
        if (d < bd) {
            bd = d;
            best = m;
        }
    }
    return best;
}

/* ------------------------------------------------------------------------------------------------ phrases --- */
/* a phrase from the book, on root (MIDI), contour ct */
static void render_contour(en_phrase_t *p, int ct, int root, int book, int vowel)
{
    const book_t *b = &BOOK[book];
    int i, n = CONTOUR_N[ct], step = 0;
    for (i = 0; i < n; i++) {
        const syl_t *s = &CONTOUR[ct][i];
        int len = (int)s->dur * (int)b->mul / 2, deg = (int)s->deg * (int)b->spread / 2;
        if (len < 1)
            len = 1;
        if (step + len > EN_MAX_STEPS)
            len = EN_MAX_STEPS - step;
        p->s[i].step = (uint8_t)step;
        p->s[i].len = (uint8_t)len;
        p->s[i].note = (uint8_t)snap(deg_from(root, deg));
        p->s[i].vowel = (uint8_t)(vowel >= 0 ? vowel : b->vow[i & 3]);
        p->s[i].on = 1;
        step += len;
    }
    p->n = (uint8_t)n;
    p->steps = (uint8_t)((step + 15) / 16 * 16);
    if (p->steps == 0)
        p->steps = 16;
    p->root = (uint8_t)root;
    p->contour = (uint8_t)ct;
}

/* the melody as a phrase */
static void render_melody(en_phrase_t *p)
{
    int i, n = en_nmelody;
    for (i = 0; i < n && i < EN_MAX_SYL; i++) {
        p->s[i].step = en_melody[i].step;
        p->s[i].len = en_melody[i].len;
        p->s[i].note = en_melody[i].note;
        p->s[i].vowel = en_melody[i].vowel;
        p->s[i].on = 1;
    }
    p->n = (uint8_t)n;
    p->steps = en_melody_steps ? en_melody_steps : 16;
    p->root = n ? en_melody[0].note : 60;
    p->contour = CT_N;
}

/* FRAGMENT, at the phrase's start: how much of it is heard. f 0..1: the whole; a half; a syllable or two; a
 * consonant (the onset alone) */
static void fragment(en_phrase_t *p, float f)
{
    int i, n = p->n, keep, from;
    if (n <= 0)
        return;
    for (i = 0; i < n; i++)
        p->s[i].on = 1;
    p->gate = 0;
    if (f < 0.2f)
        return;
    if (f < 0.5f) {                                      /* a part of it, contiguous: all down to a half */
        keep = (int)((float)n * (1.0f - (f - 0.2f) * (0.5f / 0.3f)) + 0.5f);
        if (keep < 1)
            keep = 1;
    } else if (f < 0.8f) {                               /* a syllable or two */
        keep = h_chance(&rng, 0.5f) ? 2 : 1;
    } else {
        keep = 1;
    }
    if (keep > n)
        keep = n;
    from = h_irand(&rng, n - keep + 1);
    for (i = 0; i < n; i++)
        p->s[i].on = (uint8_t)(i >= from && i < from + keep);
    if (f >= 0.5f) {                                     /* and cut short: a syllable, then a consonant */
        float g = f < 0.8f ? 1.0f - (f - 0.5f) * (0.5f / 0.3f) : 0.5f - (f - 0.8f) * (0.4f / 0.2f);
        for (i = 0; i < n; i++)
            if (p->s[i].on) {
                int len = (int)((float)p->s[i].len * g + 0.5f);
                if (f >= 0.8f && len > 1)
                    len = 1;
                p->s[i].len = (uint8_t)(len < 1 ? 1 : len);
            }
        if (f >= 0.8f)                                   /* the consonant: a 1/32 down to a 1/256 */
            p->gate = (uint8_t)(128.0f - (f - 0.8f) * (112.0f / 0.2f));
    }
}

static void phrase_new(en_phrase_t *p, int root, int vowel)   /* the call's: the melody, or the book's */
{
    if (en_nmelody && !en_writing) {
        render_melody(p);
    } else {
        const book_t *b = &BOOK[par[P_BOOK]];
        render_contour(p, b->ct[h_irand(&rng, 3)], root, par[P_BOOK], vowel);
    }
    fragment(p, (float)par[P_FRAGMENT] * 0.01f);
}

/* ------------------------------------------------------------------------------------------------- voices --- */
static float ms_curve(int v, float lo, float hi) { return lo * fm_exp2f((float)v * 0.01f * fm_log2f(hi / lo)); }

static void vox_tune(vox_t *v)                          /* the singer's throat, once a block */
{
    const singer_t *sg = &SINGER[par[P_SINGER]];
    float throat = h_semis((float)par[P_THROAT] * 0.1f);
    v->fv.shift = sg->shift * throat;
}

static void vox_vowel(vox_t *v, int code)
{
    const singer_t *sg = &SINGER[par[P_SINGER]];
    if (code < 0 || code >= EN_VOW_N)
        code = 0;
    v->vow_t = VOW_POS[code];
    v->fv.breath = fm_clampf(sg->breath * (0.3f + 1.4f * (float)par[P_BREATH] * 0.01f) + VOW_BREATH[code], 0.02f, 0.95f);
}

/* the note the answer voice sings for the call's: transposed, or inverted around the root */
static int transposed(int note, int root, int mode)
{
    switch (mode) {
    case TR_UP5: return snap(note + 5);
    case TR_DOWN7: return snap(note - 7);
    case TR_INVERT: return snap(2 * root - note);
    default: return note;
    }
}

static void syl_on(int vi, int note, int vowel, int len16, int shadow)
{
    vox_t *v = &vox[vi];
    const singer_t *sg = &SINGER[par[P_SINGER]];
    float a_ms = ms_curve(par[P_ATTACK], 4.0f, 1500.0f), dec = (float)par[P_DECAY] * 0.01f;
    float f;
    if (note < 24)
        note = 24;
    if (note > 108)
        note = 108;
    f = h_mtof((float)note + 0.08f * h_brand(&rng) + (shadow ? sg->detune : 0.0f));
    h_env_set(&v->amp, a_ms, 300.0f + 1500.0f * dec, 1.0f - 0.65f * dec, 60.0f + a_ms * 0.4f);
    h_env_on(&v->amp);
    v->freq_t = f;
    if (!h_env_busy(&v->amp) || v->freq <= 0.0f || v->off_at < 0)
        v->freq = f * 0.94f;                             /* a scoop into the note */
    v->gate_left = v->phr.gate ? step16 * (float)v->phr.gate * (1.0f / 256.0f) : -1.0f;
    v->cons_left = (float)par[P_CONSONANT] * 0.01f * 0.06f * H_SR;   /* the consonant: breath, up to 60 ms */
    v->vib_ph = 0.0f;
    v->off_at = abs16 + (len16 < 1 ? 1 : len16);
    vox_vowel(v, vowel);
    en_vnote[vi] = (uint8_t)note;
    if (!shadow && par[P_SINGER] == SG_CHOIR && !vox[1 - vi].own && vox[1 - vi].ph_start < 0)
        syl_on(1 - vi, note, vowel, len16, 1);
    /* a cut as it comes: a stutter, or backwards */
    if (!shadow && !rd.pend_in && rd.mode == RM_NORMAL) {
        float lenf = (float)len16 * step16;
        if (h_chance(&rng, (float)par[P_STUTTER] * 0.01f * 0.6f)) {
            float slice = step16 * (h_chance(&rng, 0.5f) ? 1.0f : 0.5f);   /* a 1/16 or a 1/32 */
            if (slice > (float)RING_D)
                slice = (float)RING_D;
            rd.pend_mode = RM_STUTTER;
            rd.pend_len = (uint32_t)slice;
            rd.pend_reps = 2u + (uint32_t)h_irand(&rng, 3);
            rd.pend_in = RING_D;
        } else if (h_chance(&rng, (float)par[P_REVERSE] * 0.01f * 0.7f)) {
            if (lenf > (float)RING_D)
                lenf = (float)RING_D;
            if (lenf < 1024.0f)
                lenf = 1024.0f;
            rd.pend_mode = RM_REVERSE;
            rd.pend_len = (uint32_t)lenf;
            rd.pend_reps = 1;
            rd.pend_in = RING_D;
            rd.swell = par[P_REVERSE] > 70;
        }
    }
}

static void syl_off(int vi)
{
    h_env_off(&vox[vi].amp);
    vox[vi].off_at = -1;
    vox[vi].gate_left = -1.0f;
    en_vsyl[vi] = -1;
}

static void vox_start(int vi, const en_phrase_t *p, int32_t start, int own, int live, int trans_mode)
{
    vox_t *v = &vox[vi];
    if (p != &v->phr)
        v->phr = *p;
    v->ph_start = start;
    v->ph_i = 0;
    v->own = (uint8_t)own;
    v->live = (uint8_t)live;
    v->trans_mode = (uint8_t)trans_mode;
    en_phrase[vi] = v->phr;
    if (vi == 0) {
        last_sung = v->phr;
        last_sung_ver++;
    }
}

static void vox_stop(int vi)
{
    vox_t *v = &vox[vi];
    if (v->off_at >= 0)
        syl_off(vi);
    v->ph_start = -1;
    v->own = 0;
    v->live = 0;
    en_vpos[vi] = -1.0f;
}

static void quiet(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        vox_stop(i);
        vox[i].amp.st = H_ENV_OFF;
        vox[i].amp.v = 0.0f;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].fm.amp.st = H_ENV_OFF;
        pad[i].fm.amp.v = 0.0f;
    }
    H_PLATE_INIT(&plate, pl);
    h_dl_init(&ring, ring_buf, RING_N);
    h_dl_init(&echo.dl, echo_buf, ECHO_N);
    rd.mode = rd.old_mode = RM_NORMAL;
    rd.pend_in = 0;
    rd.xf = 0;
}

/* ---------------------------------------------------------------------------------------------------- pad --- */
static void pad_voice_set(int i, int note, int on)
{
    pad_t *p = &pad[i];
    float f = h_mtof((float)note) * (1.0f + 0.0018f * (float)(i - 1));
    if (on) {
        p->freq_t = f;
        if (!h_env_busy(&p->fm.amp)) {
            p->freq = f;
            h_fm_on(&p->fm, f, 1.0f);
        }
        p->note = note;
    } else {
        h_env_off(&p->fm.amp);
    }
}

static void pad_chord(int slot)                         /* the slot's chord, voice-led from the last */
{
    int pcs[3], n = chord_of(slot, pcs), i, from[3], to[3], lo, hi;
    en_chord_slot = (uint8_t)slot;
    if (!n) {
        for (i = 0; i < EN_NPAD; i++)
            pad_voice_set(i, pad[i].note, 0);
        en_chord_root = -1;
        chord_n = 0;
        return;
    }
    for (i = 0; i < 3; i++)
        chord_pcs[i] = pcs[i];
    chord_n = n;
    en_chord_root = pcs[0];
    lo = 60 + 12 * par[P_PADOCT];
    hi = lo + 19;
    for (i = 0; i < 3; i++)
        from[i] = pad[i + 1].note > 0 ? pad[i + 1].note : lo + 4 + 5 * i;
    h_voice_lead(from, pcs, 3, to, lo, hi);
    for (i = 0; i < 3; i++)
        pad_voice_set(i + 1, to[i], 1);
    pad_voice_set(0, h_nearest(lo - 12, pcs[0], lo - 17, lo - 6), 1);   /* the root under it */
}

/* --------------------------------------------------------------------------------------------- the form --- */
static void next_event(int32_t at)
{
    next_from = abs16;
    next_at = at;
}

static void phase_set(int ph)
{
    en_phase = (uint8_t)ph;
    en_phases |= (uint8_t)(1u << ph);
}

static int32_t next_bar(void) { return (abs16 + 16) / 16 * 16; }

static void call_begin(void)                            /* a new phrase, a new call */
{
    const singer_t *sg = &SINGER[par[P_SINGER]];
    phrase_new(&vox[0].phr, phrase_root(sg->centre), -1);
    phase_set(PH_CALL);
    reps_left = par[P_REPEAT];
    call_start = -1;
    answer_at = -1;
    chorus2_at = -1;
    next_event(next_bar());
}

static void silence_begin(void)
{
    int bars = par[P_SILENCE] + h_irand(&rng, par[P_SILENCE] / 2 + 1) - par[P_SILENCE] / 4;
    if (bars < 1)
        bars = 1;
    phase_set(PH_SILENCE);
    silence_until = next_bar() + 16 * (bars - 1);
    if (silence_until <= abs16)
        silence_until = next_bar();
    next_event(silence_until);
    answer_at = -1;
}

static void answer_plan(void)                           /* decided as the call ends (or, overlapping, as it starts) */
{
    if (answer_at >= 0)
        return;
    if (!h_chance(&rng, (float)par[P_ANSCHANCE] * 0.01f)) {
        answer_at = -2;                                  /* no answer this time */
        return;
    }
    answer_at = (par[P_ANSMODE] == 0 && call_start >= 0 ? call_start : abs16) + 16 * par[P_ANSDELAY];
    if (answer_at <= abs16)
        answer_at = next_bar();
    next_event(answer_at);
}

static void form_bar(void)                              /* at each bar line */
{
    if (en_phase == PH_CALL && reps_left > 0 && vox[0].ph_start < 0 && !vox[0].live) {
        if (h_chance(&rng, (float)par[P_DENSITY] * 0.01f)) {
            vox_start(0, &vox[0].phr, abs16, 1, 0, TR_UNISON);
            reps_left--;
            if (call_start < 0) {
                call_start = abs16 + 1;
                if (par[P_ANSMODE] == 0)
                    answer_plan();
            }
            if (answer_at < 0)                       /* the end of this repeat (an answer planned shows instead) */
                next_event(abs16 + vox[0].phr.steps);
        } else {
            next_event(next_bar());
        }
    }
}

static void form_step(void)                             /* at each 1/16 */
{
    if (answer_at >= 0 && abs16 == answer_at && (en_phase == PH_CALL || en_phase == PH_ANSWER)) {
        phase_set(PH_ANSWER);
        vox_start(1, &vox[0].phr, abs16, 1, 0, par[P_ANSTRANS]);
        next_event(abs16 + vox[1].phr.steps);
        answer_at = -3;                                  /* sung */
    }
    if (chorus2_at >= 0 && abs16 == chorus2_at) {
        vox_start(1, &vox[0].phr, abs16, 1, 0, par[P_ANSTRANS]);
        chorus2_at = -1;
    }
    if (en_phase == PH_SILENCE && silence_until >= 0 && abs16 >= silence_until) {
        silence_until = -1;
        call_begin();
    }
}

static void vox_done(int vi)                            /* a phrase sung to its end */
{
    if (vox[vi].live) {
        vox[vi].live = 0;
        if (!interrupted)
            return;
        interrupted = 0;
    }
    if (en_phase == PH_CALL) {
        if (reps_left > 0)
            return;
        if (answer_at == -1)
            answer_plan();
        if (answer_at == -2) {
            en_cycles++;
            silence_begin();
        } else if (answer_at == -3 && vox[1].ph_start < 0) {
            phase_set(PH_CHORUS);
            vox_start(0, &vox[0].phr, abs16 + 1, 1, 0, TR_UNISON);
            chorus2_at = abs16 + 9;
            next_event(abs16 + 1 + vox[0].phr.steps + 8);
        } else {
            phase_set(PH_ANSWER);
        }
    } else if (en_phase == PH_ANSWER) {
        if (vox[0].ph_start < 0 && vox[1].ph_start < 0) {
            phase_set(PH_CHORUS);
            vox_start(0, &vox[0].phr, abs16 + 1, 1, 0, TR_UNISON);
            chorus2_at = abs16 + 9;
            next_event(abs16 + 1 + vox[0].phr.steps + 8);
        }
    } else if (en_phase == PH_CHORUS) {
        if (vox[0].ph_start < 0 && vox[1].ph_start < 0 && chorus2_at < 0) {
            en_cycles++;
            silence_begin();
        }
    }
}

/* ------------------------------------------------------------------------------------------- the playhead --- */
static void tempo_set(void)
{
    step16 = H_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f);
    h_echo_time(&echo, par[P_TEMPO] >= 61 ? 60000.0f * 0.75f / (float)par[P_TEMPO] : 60000.0f * 0.5f / (float)par[P_TEMPO]);
}

static void step(void)                                  /* a new 1/16 */
{
    int vi;
    abs16++;
    en_steps_total++;
    if ((abs16 & 15) == 0) {                             /* a bar line: the pad's chord, the form */
        int32_t bar = abs16 / 16;
        if (chord_bar < 0 || bar - chord_bar >= par[P_PADBARS]) {
            int slot = chord_bar < 0 ? 0 : (en_chord_slot + 1) & 3;
            chord_bar = bar;
            pad_chord(slot);
        }
        form_bar();
    }
    form_step();
    for (vi = 0; vi < 2; vi++) {
        vox_t *v = &vox[vi];
        int32_t rel;
        if (v->off_at >= 0 && abs16 >= v->off_at)
            syl_off(vi);
        if (v->ph_start < 0)
            continue;
        rel = abs16 - v->ph_start;
        while (v->ph_i < v->phr.n && (int32_t)v->phr.s[v->ph_i].step <= rel) {
            const en_syl_t *s = &v->phr.s[v->ph_i];
            if ((int32_t)s->step == rel && s->on) {
                int note = v->trans_mode ? transposed(s->note, v->phr.root, v->trans_mode) : s->note;
                syl_on(vi, note, s->vowel, s->len, 0);
                en_vsyl[vi] = (int8_t)v->ph_i;
                v->cur = v->ph_i;
            }
            v->ph_i++;
        }
        if (v->ph_i >= v->phr.n && rel >= (int32_t)v->phr.steps - 1) {
            v->ph_start = -1;
            v->own = 0;
            vox_done(vi);
        }
    }
}

static void advance(float d16)
{
    int sub;
    pos += d16;
    while (pos >= 16.0f)
        pos -= 16.0f;
    sub = (int)pos;
    if (sub != last_sub) {
        last_sub = sub;
        step();
    }
}

static void play_start(void)
{
    pos = 0.0f;
    abs16 = -1;
    last_sub = -1;
    chord_bar = -1;
    en_phases = 0;
    en_cycles = 0;
    en_playing = 1;
    vox_stop(0);
    vox_stop(1);
    silence_until = -1;
    interrupted = 0;
    call_begin();
}

static void play_stop(void)
{
    int i;
    en_playing = 0;
    vox_stop(0);
    vox_stop(1);
    for (i = 0; i < EN_NPAD; i++)
        h_env_off(&pad[i].fm.amp);
    en_writing = 0;
    mel_start = -1;
}

/* a key: the ghost sings a phrase at that pitch from the next 1/8; writing, the note goes into the melody */
static void key(int note, int vel)
{
    if (note < 0 || note > 127)
        return;
    if (vel) {
        int q;
        if (!en_playing)
            play_start();
        q = (abs16 + 2) & ~1;                            /* the next 1/8 */
        if (en_armed) {
            if (!en_writing) {                           /* the first key: a fresh melody */
                en_writing = 1;
                en_nmelody = 0;
                mel_start = q;
            }
            if (en_nmelody < EN_MAX_MELODY && q - mel_start < EN_MAX_STEPS) {
                en_melody[en_nmelody].step = (uint8_t)(q - mel_start);
                en_melody[en_nmelody].len = 2;
                en_melody[en_nmelody].note = (uint8_t)note;
                en_melody[en_nmelody].vowel = (uint8_t)(vowel_hold >= 0 ? vowel_hold : BOOK[par[P_BOOK]].vow[en_nmelody & 3]);
                key_step[note] = (uint8_t)(en_nmelody + 1);
                en_nmelody++;
            }
        }
        {                                                /* and it sings: a phrase of the book on that note */
            const book_t *b = &BOOK[par[P_BOOK]];
            en_phrase_t p;
            render_contour(&p, en_armed ? CT_HELD : b->ct[h_irand(&rng, 3)], note, par[P_BOOK], vowel_hold);
            if (!en_armed)
                fragment(&p, (float)par[P_FRAGMENT] * 0.01f);
            if (vox[0].own)
                interrupted = 1;
            vox_stop(0);
            vox_start(0, &p, q, 0, 1, TR_UNISON);
            key_live[note] = 1;
        }
    } else {
        if (en_writing && key_step[note]) {              /* the key's length, in 1/16 */
            int i = key_step[note] - 1, len = abs16 + 1 - (mel_start + (int32_t)en_melody[i].step);
            if (len < 1)
                len = 1;
            if (len > 32)
                len = 32;
            en_melody[i].len = (uint8_t)len;
            key_step[note] = 0;
        }
        if (key_live[note]) {
            key_live[note] = 0;
            if (vox[0].live && vox[0].off_at >= 0 && en_armed)
                syl_off(0);
        }
    }
}

static void melody_done(void)                           /* REC again: the melody is written */
{
    int i, end = 0;
    en_writing = 0;
    for (i = 0; i < en_nmelody; i++)
        if (en_melody[i].step + en_melody[i].len > end)
            end = en_melody[i].step + en_melody[i].len;
    en_melody_steps = (uint8_t)((end + 15) / 16 * 16);
    if (en_melody_steps > EN_MAX_STEPS)
        en_melody_steps = EN_MAX_STEPS;
    if (!en_melody_steps)
        en_melody_steps = 16;
    mel_start = -1;
    en_melody_ver++;
}

static void keep(void)                                  /* the phrase voice 1 sang last becomes the melody */
{
    int i, n = 0;
    const en_phrase_t *p = vox[0].ph_start >= 0 ? &vox[0].phr : &last_sung;
    for (i = 0; i < p->n && n < EN_MAX_MELODY; i++) {
        if (!p->s[i].on)
            continue;
        en_melody[n].step = p->s[i].step;
        en_melody[n].len = p->s[i].len;
        en_melody[n].note = p->s[i].note;
        en_melody[n].vowel = p->s[i].vowel;
        n++;
    }
    en_nmelody = (uint8_t)n;
    en_melody_steps = p->steps;
    en_writing = 0;
    en_armed = 0;
    en_melody_ver++;
}

/* ----------------------------------------------------------------------------------------------- settings --- */
static void bed_set(void)
{
    int t = par[P_BED];
    bed.type = (uint8_t)(t == BED_NONE ? H_BED_NONE : t == BED_RAIN || t == BED_BOTH ? H_BED_RAIN : t == BED_CRACKLE ? H_BED_CRACKLE
                                                   : t == BED_HISS ? H_BED_HISS : t == BED_WIND ? H_BED_WIND : H_BED_ROOM);
    bed2.type = (uint8_t)(t == BED_BOTH ? H_BED_CRACKLE : H_BED_NONE);
    bed_lvl = (float)par[P_BEDLEVEL] * 0.01f;
    bed.level = bed_lvl * 0.7f;
    bed2.level = bed_lvl * 0.6f;
}

static void apply(int p, int v)
{
    par[p] = (int16_t)v;
    switch (p) {
    case P_TEMPO: tempo_set(); break;
    case P_PITCH: pitch_f = h_semis((float)v); break;
    case P_PADOCT:
        if (en_playing)
            pad_chord(en_chord_slot);
        break;
    case P_TONE: tone_k = h_lp1_k(h_lerp(1200.0f, 14000.0f, h_sq((float)v * 0.01f))); break;
    case P_RASP: rasp = 1.0f + 3.0f * (float)v * 0.01f; break;
    case P_PADLEVEL: pad_lvl = (float)v * 0.01f * 0.32f; break;
    case P_BED: case P_BEDLEVEL: bed_set(); break;
    case P_HIPASS: hp_k = h_lp1_k(h_lerp(30.0f, 900.0f, h_sq((float)v * 0.01f))); break;
    case P_WASH: wash_t = (float)v * 0.01f; break;
    case P_SINGER:
        if (v == SG_RADIO)
            h_svf_set(&radio_l, SINGER[SG_RADIO].band, 0.9f);
        vox_tune(&vox[0]);
        vox_tune(&vox[1]);
        break;
    case P_THROAT:
        vox_tune(&vox[0]);
        vox_tune(&vox[1]);
        break;
    case P_ROOT: case P_SCALE: case P_CHORD1: case P_CHORD2: case P_CHORD3: case P_CHORD4:
        if (en_playing)
            pad_chord(en_chord_slot);
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
        case C_ARM:
            if (c.a) {
                en_armed = 1;
            } else {
                if (en_writing)
                    melody_done();
                en_armed = 0;
            }
            break;
        case C_KEY: key(c.a, (int)c.b); break;
        case C_VOWEL: vowel_hold = c.a; break;
        case C_KEEP: keep(); break;
        case C_CLEAR:
            en_nmelody = 0;
            en_melody_steps = 0;
            en_writing = 0;
            mel_start = -1;
            en_melody_ver++;
            break;
        case C_MELODY: {
            int i;
            for (i = 0; i < shadow_n; i++)
                en_melody[i] = shadow[i];
            en_nmelody = shadow_n;
            en_melody_steps = shadow_steps <= EN_MAX_STEPS ? shadow_steps : EN_MAX_STEPS;
            en_seed_now = shadow_seed ? shadow_seed : 0x5EED5u;
            rng.s = en_seed_now * 2654435761u;
            en_melody_ver++;
            break;
        }
        case C_SEED:
            en_seed_now = (uint32_t)c.b | 1u;
            rng.s = en_seed_now * 2654435761u;
            break;
        case C_PANIC:
            play_stop();
            quiet();
            break;
        case C_CLOCK:
            if (c.a == EN_CLK_TICK) {
                en_ext = 1;
                ext_age = 0;
                ext_ticks++;
                if (en_playing)
                    advance(1.0f / 6.0f);
            } else if (c.a == EN_CLK_START) {
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
    rng.s = 0xA5A5A5A5u;
    en_playing = en_armed = en_writing = en_ext = 0;
    en_nmelody = en_melody_steps = 0;
    en_phase = PH_SILENCE;
    abs16 = -1;
    last_sub = -1;
    pos = 0.0f;
    mel_start = -1;
    vowel_hold = -1;
    for (i = 0; i < 2; i++) {
        h_voice_init(&vox[i].fv, 0x1357u + 977u * (uint32_t)i);
        vox[i].ph_start = -1;
        vox[i].off_at = -1;
        vox[i].freq = vox[i].freq_t = 110.0f;
        vox[i].det = 1.0f;
        en_vpos[i] = -1.0f;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].fm.ratio = i == 0 ? 1.0f : 2.0f;
        pad[i].fm.idx = i == 0 ? 0.12f : 0.22f;
        pad[i].fm.fb = 0.2f;
        pad[i].fm.det = 1.0f;
        pad[i].note = 0;
        h_env_set(&pad[i].fm.amp, 1400.0f, 2500.0f, 0.8f, 3000.0f);
        h_env_set(&pad[i].fm.mod, 600.0f, 4000.0f, 0.5f, 3000.0f);
    }
    h_bed_init(&bed, 0x777u);
    h_bed_init(&bed2, 0x999u);
    H_PLATE_INIT(&plate, pl);
    h_plate_set(&plate, 0.84f, 0.45f, 0.5f, 18.0f);
    h_echo_init(&echo, echo_buf, ECHO_N, 0x2468u);
    h_dl_init(&ring, ring_buf, RING_N);
    h_svf_set(&radio_l, 1500.0f, 0.9f);
    for (i = 0; i < P_N; i++)
        apply(i, par[i]);
    wash = wash_t;
}

static void control(uint32_t n)                         /* once a block: the slow things */
{
    float fn = (float)n, w, fl;
    int i;
    /* the tape: wow and flutter as one pitch factor */
    wow_ph = h_wrap(wow_ph + fn * h_lerp(0.3f, 2.0f, (float)par[P_WOWRATE] * 0.01f) * H_ISR);
    flut_ph = h_wrap(flut_ph + fn * 6.7f * H_ISR);
    w = (float)par[P_WOW] * 0.01f;
    fl = (float)par[P_FLUTTER] * 0.01f;
    wow_f = 1.0f + 0.025f * w * w * h_sin(wow_ph) + 0.004f * fl * h_sin(flut_ph);
    /* dropouts */
    if (drop_left > 0.0f) {
        drop_left -= fn;
        if (drop_left <= 0.0f)
            drop_t = 1.0f;
    } else if (par[P_DROPOUT] && h_chance(&rng, fn * (float)par[P_DROPOUT] * 0.01f * 0.25f / H_SR)) {
        drop_t = 0.1f * h_frand(&rng);
        drop_left = H_SR * (0.04f + 0.25f * h_frand(&rng));
    }
    en_dropout = drop_t < 0.9f;
    /* the wash: plate and echo as one macro, the chorus opening it */
    wash_boost += ((en_phase == PH_CHORUS ? 0.22f : 0.0f) - wash_boost) * 0.02f;
    wash += (fm_clampf(wash_t + wash_boost, 0.0f, 1.0f) - wash) * 0.05f;
    send_v = wash * 0.55f + (rd.mode == RM_REVERSE && rd.swell ? 0.5f : 0.0f);
    send_p = 0.12f + 0.2f * wash;
    echo_send = wash * 0.6f;
    echo.fb = 0.15f + 0.6f * wash;
    echo.tone = 0.3f - 0.22f * wash;
    h_plate_set(&plate, 0.55f + 0.4f * wash, 0.3f + 0.35f * wash, 0.5f, 18.0f);
    /* the voices: the vowel slides, the vibrato, the throat */
    for (i = 0; i < 2; i++) {
        vox_t *v = &vox[i];
        float vib = 0.0f;
        if (h_env_busy(&v->amp)) {
            v->vib_ph += fn * 5.3f * H_ISR;
            if (v->vib_ph > 0.3f)                        /* the vibrato grows in */
                vib = h_sin(v->vib_ph) * (float)par[P_VIBRATO] * 0.01f * 0.35f * fm_minf(1.0f, (v->vib_ph - 0.3f) * 0.5f);
        }
        v->det = h_semis(vib);
        v->fv.vowel += (v->vow_t - v->fv.vowel) * 0.15f;
        v->cons_left -= fn;
        if (v->gate_left > 0.0f) {                       /* a consonant: cut before its step is over */
            v->gate_left -= fn;
            if (v->gate_left <= 0.0f && v->off_at >= 0)
                syl_off(i);
        }
    }
    /* the screen */
    en_bar_pos = pos - (float)RING_D / step16 + (pos - (float)RING_D / step16 < 0.0f ? 16.0f : 0.0f);
    for (i = 0; i < 2; i++)
        en_vpos[i] = vox[i].ph_start >= 0 ? (float)(abs16 - vox[i].ph_start) + (pos - (float)last_sub) - (float)RING_D / step16 : -1.0f;
    if (next_at > next_from) {
        float left = (float)(next_at - abs16) - (pos - (float)last_sub);
        en_next_frac = fm_clampf(left / (float)(next_at - next_from), 0.0f, 1.0f);
        en_next_bars = (next_at - abs16 + 15) / 16;
    } else {
        en_next_frac = 0.0f;
        en_next_bars = 0;
    }
    en_stutter = rd.mode == RM_STUTTER;
    en_reverse = rd.mode == RM_REVERSE;
    if (en_ext && ++ext_age > 60u)
        en_ext = 0;
}

static inline float ring_read(uint32_t mode, uint32_t t)   /* what is heard: the ring, D back, cut as the mode says */
{
    uint32_t back;
    switch (mode) {
    case RM_STUTTER:
        back = RING_D + t - t % rd.len;
        break;
    case RM_REVERSE:
        back = RING_D - rd.len + 1u + 2u * t;
        break;
    default:
        back = RING_D;
        break;
    }
    if (back >= RING_N - 1u)
        back = RING_N - 2u;
    return h_dl_tapi(&ring, back);
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float g = h_gain(gain_q12, master_trim);
    const singer_t *sg;
    float d16, peak = 0.0f, vtrim, pad_k = h_lp1_k(2200.0f), glide = 0.0008f, pf;
    uint32_t i;
    int v;
    drain();
    control(n);
    sg = &SINGER[par[P_SINGER]];
    vtrim = 0.5f * sg->trim;
    d16 = 1.0f / step16;
    pf = pitch_f * wow_f;
    for (i = 0; i < n; i++) {
        float vb = 0.0f, pb = 0.0f, l, r, rl, rr, bl, br, b2l, b2r, e, x, heard;
        if (en_playing && !en_ext)
            advance(d16);
        /* the singers */
        for (v = 0; v < 2; v++) {
            vox_t *vo = &vox[v];
            float a, s;
            if (!h_env_busy(&vo->amp))
                continue;
            vo->freq += (vo->freq_t - vo->freq) * glide;
            vo->fv.freq = vo->freq * pf * vo->det;
            a = h_env(&vo->amp);
            if (vo->cons_left > 0.0f) {                  /* the consonant: a breath before the tone */
                s = h_voice(&vo->fv) * 0.35f + h_brand(&vo->fv.rng) * 0.25f;
            } else {
                s = h_voice(&vo->fv);
            }
            vb += s * a * vtrim;
        }
        /* the pad */
        for (v = 0; v < EN_NPAD; v++) {
            pad_t *p = &pad[v];
            if (!h_env_busy(&p->fm.amp))
                continue;
            p->freq += (p->freq_t - p->freq) * 0.00008f;
            p->fm.freq = p->freq * (v == 0 ? 1.0f : wow_f);
            pb += h_fm(&p->fm) * (v == 0 ? 0.5f : 0.3f);
        }
        pb = h_lp1(&pad_lp, pb, pad_k) * pad_lvl;
        /* the voice is heard through the ring: late, stuttered or backwards */
        h_dl_push(&ring, vb);
        if (rd.pend_in) {
            if (--rd.pend_in == 0u) {
                rd.old_mode = rd.mode;
                rd.old_t = rd.t;
                rd.mode = (uint8_t)rd.pend_mode;
                rd.len = rd.pend_len;
                rd.reps = rd.pend_reps;
                rd.t = 0;
                rd.xf = 256;
            }
        }
        heard = ring_read(rd.mode, rd.t);
        if (rd.xf) {
            float k = (float)rd.xf * (1.0f / 256.0f);
            heard = heard * (1.0f - k) + ring_read(rd.old_mode, rd.old_t) * k;
            rd.old_t++;
            rd.xf--;
        }
        rd.t++;
        if (rd.mode != RM_NORMAL && rd.t >= rd.len * rd.reps) {
            rd.old_mode = rd.mode;
            rd.old_t = rd.t;
            rd.mode = RM_NORMAL;
            rd.t = 0;
            rd.xf = 256;
            rd.swell = 0;
        }
        heard = h_lp1(&tone_lp, h_sat(heard, rasp), tone_k);   /* the throat's rasp, its tone */
        if (sg->band > 0.0f)                             /* the radio: a band, and a little crush */
            heard = h_sat(h_svf_bp(&radio_l, heard) * 2.0f, 2.0f) * 0.8f;
        drop_g += (drop_t - drop_g) * 0.003f;
        heard *= drop_g;
        /* the wash: echo into the plate */
        e = h_echo(&echo, heard * echo_send);
        x = h_hp1(&hp_pre, (heard + e * 0.7f) * send_v + pb * send_p, hp_k);
        h_plate(&plate, x, &rl, &rr);
        h_bed(&bed, &bl, &br);
        h_bed(&bed2, &b2l, &b2r);
        l = heard * 0.95f + e * 0.6f + pb * 0.9f + rl + bl + b2l;
        r = heard * 0.95f + e * 0.55f + pb * 0.9f + rr + br + b2r;
        l = h_dc(&dcl, l);
        r = h_dc(&dcr, r);
        h_lim(&lim, &l, &r);
        if (fm_fabsf(l) > peak)
            peak = fm_fabsf(l);
        h_out(out + 2 * i, l, r, g);
    }
    en_level = h_lp1(&lvl_lp, peak, 0.3f);
}
