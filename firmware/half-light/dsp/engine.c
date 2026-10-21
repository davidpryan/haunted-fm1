/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* HALF-LIGHT's engine (engine.h). One compilation unit, -O2. */
#include "engine.h"

/* ---------------------------------------------------------------------------------------------- parameters --- */
static const char *const CLIMATE_NAME[CL_N] = {"Still", "Front", "Storm", "Clearing", "Fog", "Manual"};
static const char *const VOICE_NAME[VO_N] = {"Glass", "Saw", "Bowed", "Organ", "Choir", "Strings"};
static const char *const MOVE_NAME[8] = {"1", "2", "3", "4", "5", "6", "7", "All"};
static const char *const BASS_NAME[4] = {"Off", "Root", "Tonic", "5ths"};
static const char *const DENSE_NAME[4] = {"Off", "4ths", "Clus", "Both"};
static const char *const RANGE_NAME[3] = {"2 oct", "3 oct", "4 oct"};
static const char *const BED_NAME[H_BED_N] = {"None", "Rain", "Vinyl", "Hiss", "Wind", "Room"};   /* (short: a knob card) */
static const char *const MODE_NAME[H_SC_N] = {"Aeol", "Dor", "Phryg", "Harm", "Pent", "Major", "Lyd", "Whole"};
static const char *const LIGHTS_NAME[3] = {"Off", "On", "Keys"};
const char *const EN_QUAL_SUFFIX[Q_N] = {"", "7", "add9", "sus", "4ths", "clus"};
const hp_t EN_PARAMS[P_N] = {
    {"Climate", 0, CL_N - 1, CL_FRONT, CLIMATE_NAME, 0}, {"Voice", 0, VO_N - 1, 0, VOICE_NAME, 0},
    {"Pressure", -50, 50, 0, 0, ""},
    {"Breath", 3, 120, 20, 0, " s"}, {"Voices", 2, 8, 5, 0, ""}, {"Movement", 1, 8, 2, MOVE_NAME, 0}, {"Depth", 0, 100, 55, 0, ""},
    {"Swell", 0, 100, 50, 0, ""}, {"Attack", 0, 100, 50, 0, ""}, {"Release", 0, 100, 50, 0, ""}, {"Pad", 0, 100, 70, 0, ""},
    {"Beat", 0, 30, 8, 0, " c"}, {"Walk", 0, 100, 40, 0, ""}, {"Shimmer", 0, 100, 25, 0, ""}, {"Width", 0, 100, 60, 0, ""},
    {"Every", 2, 60, 12, 0, " s"}, {"Spread", 0, 100, 50, 0, "%"}, {"At once", 1, EN_NBELLS, 2, 0, ""}, {"Echo", 0, 100, 50, 0, "%"},
    {"Bass", 0, 3, BS_ROOT, BASS_NAME, 0}, {"Late", 0, 30, 4, 0, " s"}, {"Level", 0, 100, 60, 0, ""}, {"Sub", 0, 100, 50, 0, ""},
    {"Bed", 0, H_BED_N - 1, H_BED_RAIN, BED_NAME, 0}, {"Air", 0, 100, 35, 0, ""}, {"Hi-pass", 0, 100, 20, 0, ""}, {"Damping", 0, 100, 45, 0, ""},
    {"Sevenths", 0, 1, 1, H_ONOFF, 0}, {"Add 9", 0, 1, 1, H_ONOFF, 0}, {"Sus", 0, 1, 1, H_ONOFF, 0}, {"Dense", 0, 3, DN_CLUSTER, DENSE_NAME, 0},
    {"Root", 0, 11, 9, H_NOTE_NAME, 0}, {"Mode", 0, H_SC_N - 1, 0, MODE_NAME, 0}, {"Range", 0, 2, 1, RANGE_NAME, 0}, {"Lights", 0, 2, 1, LIGHTS_NAME, 0},
    {"Bright", 0, 100, 50, 0, ""}, {"Body", -50, 50, 0, 0, ""}, {"Grain", 0, 100, 20, 0, ""}, {"Tone", 0, 100, 55, 0, ""},
    {"Octave", -2, 2, 0, 0, ""},
};

/* the pad's timbres: a two-operator FM recipe. ms: the modulator's sustain (how bright it stays), md: its decay,
 * tone: the voice's lowpass at Tone 50 */
typedef struct { float ratio, idx, fb, ms, md_ms, tone; } timbre_t;
static const timbre_t TIMBRE[VO_N] = {
    {2.0f, 0.55f, 0.0f, 1.0f, 1000.0f, 9000.0f},     /* glass */
    {1.0f, 1.3f, 0.65f, 0.9f, 2500.0f, 6000.0f},     /* saw-ish */
    {1.0f, 0.8f, 0.25f, 0.8f, 3000.0f, 4500.0f},     /* bowed */
    {3.0f, 0.3f, 0.0f, 1.0f, 100.0f, 7000.0f},       /* organ */
    {2.0f, 0.22f, 0.45f, 0.9f, 2500.0f, 3200.0f},    /* choir */
    {1.0f, 0.5f, 0.35f, 0.85f, 4000.0f, 5000.0f},    /* tape strings */
};
/* the climates: the chain's preference by interval class (same, step, third, fifth, other); the pressure curve's
 * build and rest, in seconds (lo..hi, drawn each time) */
typedef struct { float w[5]; float build_lo, build_hi, rest_lo, rest_hi; } climate_t;
static const climate_t CLIMATE[CL_N] = {
    {{1.6f, 0.3f, 1.0f, 1.0f, 0.4f}, 0.0f, 0.0f, 0.0f, 0.0f},           /* still */
    {{0.5f, 0.6f, 1.0f, 1.2f, 0.6f}, 120.0f, 240.0f, 45.0f, 75.0f},     /* front */
    {{0.2f, 1.0f, 0.8f, 0.8f, 1.0f}, 40.0f, 90.0f, 15.0f, 30.0f},       /* storm */
    {{0.6f, 0.5f, 0.8f, 1.5f, 0.5f}, 0.0f, 0.0f, 0.0f, 0.0f},           /* clearing */
    {{0.3f, 1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 0.0f, 0.0f},           /* fog */
    {{0.5f, 0.6f, 1.0f, 1.2f, 0.6f}, 0.0f, 0.0f, 0.0f, 0.0f},           /* manual */
};
/* how far from home each degree of a seven-note scale feels, and each quality */
static const float T_DEG[7] = {0.0f, 0.75f, 0.4f, 0.3f, 0.25f, 0.5f, 0.9f};
static const float T_QUAL[Q_N] = {0.0f, 0.3f, 0.4f, 0.2f, 0.6f, 1.0f};
static const float SPREAD[EN_NVOICES] = {-0.8f, 0.6f, -0.4f, 0.8f, 0.2f, -0.6f, 0.4f, -0.2f};

/* ------------------------------------------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_FREEZE, C_PIN, C_BELL, C_PINS, C_PANIC, C_CLOCK };
static h_cmdq_t cq;
void en_set(int p, int v) { h_post(&cq, C_SET, p, v); }
void en_play(int on) { h_post(&cq, C_PLAY, on, 0); }
void en_freeze(int on) { h_post(&cq, C_FREEZE, on, 0); }
void en_pin(int key, int note) { h_post(&cq, C_PIN, key, note); }
void en_bell(int deg, int on) { h_post(&cq, C_BELL, deg, on); }
void en_panic(void) { h_post(&cq, C_PANIC, 0, 0); }
void en_clock(int msg) { h_post(&cq, C_CLOCK, msg, 0); }

static uint8_t shadow_pins[EN_NPINS];                   /* a saved set handed over by the UI */
static volatile uint32_t shadow_bells;
void en_pins_load(const uint8_t *notes, uint32_t bells)
{
    int i;
    for (i = 0; i < EN_NPINS; i++)
        shadow_pins[i] = notes[i];
    shadow_bells = bells;
    H_BARRIER();
    h_post(&cq, C_PINS, 0, 0);
}

/* --------------------------------------------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_rest, en_ext, en_deg, en_qual, en_root_pc, en_nvoices, en_bell_note,
    en_bell_deg, en_bass_note;
volatile float en_pressure, en_next_s, en_breath_s = 20.0f, en_level;
volatile uint32_t en_changes, en_breaks, en_bells, en_bells_fired;
volatile uint8_t en_vnote[EN_NVOICES], en_vpin[EN_NVOICES], en_vgen[EN_NVOICES], en_pins[EN_NPINS];
volatile int16_t en_lo = 42, en_hi = 78;

static int16_t par[P_N];
static h_rng_t rng;

typedef struct {
    h_fm_t fm[2];                                       /* the sounding one (cur) and the one fading */
    uint8_t cur, on, pin, note;
    float det, det_t;                                   /* the detune walk, cents */
    float shim_ph, shim_rate;
    float gain, gain_t, pan;
    h_lp1_t lp;
} voice_t;
static voice_t vc[EN_NVOICES];
static h_fm_t bass[2];
static uint8_t bass_cur;
static h_lp1_t bass_lp;
static int bass_note, bass_target;
static float bass_late;                                 /* seconds until the bass moves (-1: no move pending) */
static h_fm_t bell[EN_NBELLS];
static float bell_echo[EN_NBELLS];                      /* each bell's send to the echo */
static float bell_next;                                 /* seconds to the next bell */

/* the weather */
static int cur_deg, cur_qual;
static float p_auto, p_target, p_eff;
static int phase;                                       /* 0 building, 1 resting (after a break) */
static float phase_s, build_s, next_s, breath_jit = 1.0f, breath_used = 20.0f, bell_mean_used = 12.0f;
static int thin;                                        /* voices still to come back after a break */
static int break_pending;
static float ext_bar_s;                                 /* seconds a bar at the MIDI clock's tempo */
static uint32_t ext_ticks, ext_age;
static float ext_t;

/* the effects */
static h_plate_t plate;
H_PLATE_BUFFERS(pl);
#define ECHO_N 24000u                                   /* 0.54 s */
#define SHIFT_N 2048u
static int16_t echo_buf[ECHO_N] H_POOL, shift_buf[SHIFT_N] H_POOL;
typedef struct {
    h_dl_t dl, sh;
    float pos, fb;
    h_lp1_t lp, hp;
} pecho_t;
static pecho_t echo;
static h_bed_t bed;
static h_lp1_t hp_l, hp_r, lvl_lp;
static h_lim_t lim = {0.0f, 1.0f};
static h_dc_t dcl, dcr;
static float rev_send, echo_send, echo_pad, hp_k, pad_gain, bass_gain, bass_lp_k, lp_k;
static float swell_s, attack_s, release_s;
static float master_trim = 0.6f;

/* ----------------------------------------------------------------------------------------------- the field --- */
static uint32_t pool(void) { return (uint32_t)H_SCALE[par[P_SCALE]]; }
static int centre(void) { return 60 + 12 * par[P_OCTAVE]; }
static void range_set(void)
{
    int half = 12 + 6 * par[P_RANGE], c = centre();
    en_lo = (int16_t)(c - half);
    en_hi = (int16_t)(c + half - 1);
}
static int in_range(int n) { return n >= en_lo && n <= en_hi; }
static int pc_of(int n) { return (n % 12 + 12) % 12; }

/* the chord on degree deg with quality q: its pitch classes; the count */
static int chord_pcs(int deg, int q, int *pcs)
{
    static const int8_t OFF[Q_N][4] = {{0, 2, 4, -1}, {0, 2, 4, 6}, {0, 2, 4, 8}, {0, 3, 4, -1}, {0, 3, 6, 9}, {0, 1, 2, 3}};
    int st[12], nd = h_scale_steps(pool(), st), i, n = 0, k;
    for (i = 0; i < 4; i++) {
        int pc;
        if (OFF[q][i] < 0)
            break;
        pc = (par[P_ROOT] + st[((deg + OFF[q][i]) % nd + nd) % nd]) % 12;
        for (k = 0; k < n; k++)
            if (pcs[k] == pc)
                break;
        if (k == n)
            pcs[n++] = pc;
    }
    return n;
}
static int qual_ok(int q)
{
    switch (q) {
    case Q_SEVENTH: return par[P_SEVENTHS];
    case Q_ADD9: return par[P_ADD9];
    case Q_SUS: return par[P_SUS];
    case Q_QUARTAL: return par[P_DENSE] == DN_QUARTAL || par[P_DENSE] == DN_BOTH;
    case Q_CLUSTER: return par[P_DENSE] == DN_CLUSTER || par[P_DENSE] == DN_BOTH;
    default: return 1;
    }
}

/* the chain: the next chord, drawn by the climate's preferences and the pressure */
static void next_chord(float p)
{
    const climate_t *cl = &CLIMATE[par[P_CLIMATE]];
    float w[7 * Q_N], sum = 0.0f, r;
    int st[12], nd = h_scale_steps(pool(), st), d, q, i = 0, n = 0;
    if (nd > 7)
        nd = 7;
    for (d = 0; d < nd; d++) {
        int delta = ((d - cur_deg) % nd + nd) % nd, m = delta < nd - delta ? delta : nd - delta;
        int cls = m == 0 ? 0 : m == 1 ? 1 : m == 2 ? 2 : (nd == 7 && m == 3) ? 3 : 4;
        float td = T_DEG[d * 7 / nd], wd = cl->w[cls];
        if (par[P_CLIMATE] == CL_CLEARING && nd == 7 && delta == 4)
            wd *= 1.5f;                                 /* down a fifth: the resolving step */
        for (q = 0; q < Q_N; q++) {
            float t, x;
            if (!qual_ok(q)) {
                w[i++] = 0.0f;
                continue;
            }
            t = 0.6f * td + 0.4f * T_QUAL[q];
            x = (t - p) * (1.0f / 0.2f);
            x = wd * fm_expf(-0.5f * x * x);
            if (d == cur_deg && q == cur_qual)
                x *= 0.6f;
            w[i++] = x;
            sum += x;
        }
    }
    n = i;
    if (sum <= 0.0f) {
        cur_deg = 0;
        cur_qual = Q_TRIAD;
        return;
    }
    r = h_frand(&rng) * sum;
    for (i = 0; i < n; i++) {
        r -= w[i];
        if (r <= 0.0f)
            break;
    }
    if (i >= n)
        i = n - 1;
    cur_deg = i / Q_N;
    cur_qual = i % Q_N;
}

/* ---------------------------------------------------------------------------------------------- voices --- */
static float s_curve(int v, float lo, float hi) { return lo * fm_exp2f((float)v * 0.01f * fm_log2f(hi / lo)); }

static void timbre_set(h_fm_t *f, float a_s)
{
    const timbre_t *t = &TIMBRE[par[P_VOICE]];
    f->ratio = t->ratio * fm_exp2f((float)par[P_BODY] * 0.02f);
    f->idx = t->idx * (0.3f + 1.4f * (float)par[P_BRIGHT] * 0.01f);
    f->fb = t->fb + 0.5f * (float)par[P_GRAIN] * 0.01f;
    h_env_set(&f->amp, a_s * 1000.0f, 1000.0f, 1.0f, release_s * 1000.0f);
    h_env_set(&f->mod, 2.0f, t->md_ms, t->ms, release_s * 1000.0f);
}
static void fm_release(h_fm_t *f, float s)
{
    f->amp.r = h_k_ms(s * 1000.0f);
    f->mod.r = f->amp.r;
    h_env_off(&f->amp);
    h_env_off(&f->mod);
}
static int voice_busy(const voice_t *v) { return h_env_busy(&v->fm[0].amp) || h_env_busy(&v->fm[1].amp); }

/* voice i to note: the old note fades over fall seconds as the new rises over rise */
static void voice_move(int i, int note, float rise, float fall)
{
    voice_t *v = &vc[i];
    fm_release(&v->fm[v->cur], fall);
    v->cur ^= 1u;
    timbre_set(&v->fm[v->cur], rise);
    v->fm[v->cur].det = h_semis(v->det * 0.01f);
    v->fm[v->cur].last = 0.0f;
    h_fm_on(&v->fm[v->cur], h_mtof((float)note), 1.0f);
    v->note = (uint8_t)note;
    v->on = 1;
    en_vnote[i] = (uint8_t)note;
    en_vgen[i]++;
}
static void voice_release(int i, float fall)
{
    voice_t *v = &vc[i];
    fm_release(&v->fm[0], fall);
    fm_release(&v->fm[1], fall);
    v->on = 0;
    v->note = 0;
    en_vnote[i] = 0;
}
static void voice_quiet(int i)
{
    voice_t *v = &vc[i];
    int k;
    for (k = 0; k < 2; k++) {
        v->fm[k].amp.st = H_ENV_OFF;
        v->fm[k].amp.v = 0.0f;
        v->fm[k].mod.st = H_ENV_OFF;
    }
    v->on = 0;
    v->note = 0;
    en_vnote[i] = 0;
}
static int note_taken(int n, int except)
{
    int i;
    for (i = 0; i < EN_NVOICES; i++)
        if (i != except && vc[i].on && vc[i].note == n)
            return 1;
    return 0;
}
/* the note of class pc nearest from, in the field, not one another voice holds (an octave away then) */
static int nearest_free(int from, int pc, int except)
{
    int n = h_nearest(from, pc, en_lo, en_hi);
    if (note_taken(n, except)) {
        if (in_range(n + 12) && !note_taken(n + 12, except))
            return n + 12;
        if (in_range(n - 12) && !note_taken(n - 12, except))
            return n - 12;
    }
    return n;
}
static int idist(int a, int b) { return a > b ? a - b : b - a; }

/* the free voices to the chord: up to moves of them, the ones off the chord first, to the tones not yet covered */
static void lead(const int *pcs, int npc, int moves, float rise, float fall)
{
    int covered[4], in[EN_NVOICES], i, k, m;
    for (m = 0; m < moves; m++) {
        int best_i = -1, best_n = 0, best_d = 1000, any_unc = 0;
        for (k = 0; k < npc; k++)
            covered[k] = 0;
        for (i = 0; i < EN_NVOICES; i++) {
            in[i] = -1;
            if (!vc[i].on || vc[i].pin)
                continue;
            for (k = 0; k < npc; k++)
                if (pc_of(vc[i].note) == pcs[k] && in_range(vc[i].note)) {
                    in[i] = k;
                    covered[k]++;
                }
        }
        for (k = 0; k < npc; k++)
            if (!covered[k])
                any_unc = 1;
        /* a voice off the chord, to an uncovered tone (or any tone) nearest */
        for (i = 0; i < EN_NVOICES; i++) {
            if (!vc[i].on || vc[i].pin || in[i] >= 0)
                continue;
            for (k = 0; k < npc; k++) {
                int n, d;
                if (any_unc && covered[k])
                    continue;
                n = nearest_free(vc[i].note, pcs[k], i);
                d = idist(n, vc[i].note);
                if (d < best_d) {
                    best_d = d;
                    best_i = i;
                    best_n = n;
                }
            }
        }
        /* everyone is on the chord: a doubled tone to an uncovered one */
        if (best_i < 0 && any_unc)
            for (i = 0; i < EN_NVOICES; i++) {
                if (!vc[i].on || vc[i].pin || in[i] < 0 || covered[in[i]] < 2)
                    continue;
                for (k = 0; k < npc; k++) {
                    int n, d;
                    if (covered[k])
                        continue;
                    n = nearest_free(vc[i].note, pcs[k], i);
                    d = idist(n, vc[i].note);
                    if (d < best_d) {
                        best_d = d;
                        best_i = i;
                        best_n = n;
                    }
                }
            }
        if (best_i < 0)
            return;
        if (best_n == vc[best_i].note)
            return;
        voice_move(best_i, best_n, rise, fall);
    }
}

static int nfree_now(void)
{
    int i, n = 0;
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].on && !vc[i].pin)
            n++;
    return n;
}
static int npins(void)
{
    int i, n = 0;
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].pin)
            n++;
    return n;
}
/* the free voices at their count: extras release, new ones start on the chord */
static void count_voices(int want, const int *pcs, int npc)
{
    int i, have = nfree_now(), k = 0;
    while (have > want) {                                /* the highest voice leaves first */
        int hi = -1, hn = -1;
        for (i = 0; i < EN_NVOICES; i++)
            if (vc[i].on && !vc[i].pin && (int)vc[i].note > hn) {
                hn = vc[i].note;
                hi = i;
            }
        if (hi < 0)
            break;
        voice_release(hi, release_s);
        have--;
    }
    while (have < want) {
        int slot = -1, n, from;
        for (i = 0; i < EN_NVOICES; i++)                 /* a silent slot, else a released one still fading */
            if (!vc[i].on && !vc[i].pin && !voice_busy(&vc[i])) {
                slot = i;
                break;
            }
        if (slot < 0)
            for (i = 0; i < EN_NVOICES; i++)
                if (!vc[i].on && !vc[i].pin) {
                    slot = i;
                    break;
                }
        if (slot < 0)
            break;
        from = en_lo + (en_hi - en_lo) * (have * 2 + 1) / (want * 2);   /* spread over the field */
        n = nearest_free(from, pcs[k % npc], slot);
        k++;
        voice_move(slot, n, attack_s, release_s);
        have++;
    }
}

/* --------------------------------------------------------------------------------------------- the weather --- */
static float breath_now(void)
{
    float b = (float)par[P_BREATH];
    if (en_ext && ext_bar_s > 0.5f) {                    /* MIDI clock: whole bars */
        float bars = fm_floorf(b / ext_bar_s + 0.5f);
        b = (bars < 1.0f ? 1.0f : bars) * ext_bar_s;
    }
    return b;
}
static void schedule(void)
{
    breath_jit = 0.9f + 0.2f * h_frand(&rng);
    breath_used = breath_now();
    next_s = breath_used * breath_jit;
}
static void bass_follow(int immediate)
{
    int pcs[4], n, pc, c = centre() - 24, lo = c - 7, hi = c + 6, i, cand;
    if (par[P_BASS] == BS_OFF)
        return;
    n = chord_pcs(cur_deg, cur_qual, pcs);
    pc = pcs[0];
    if (par[P_BASS] == BS_TONIC) {
        pc = par[P_ROOT];
    } else if (par[P_BASS] == BS_FIFTHS && bass_note > 0) {   /* the root, or a chord tone a fifth from here */
        int bpc = pc_of(bass_note);
        for (i = 0; i < n; i++) {
            int d = (pcs[i] - bpc + 12) % 12;
            if (d == 5 || d == 7) {
                pc = pcs[i];
                break;
            }
        }
    }
    cand = h_nearest(bass_note > 0 ? bass_note : c, pc, lo, hi);
    if (cand == bass_note)
        return;
    bass_target = cand;
    bass_late = immediate ? 0.0f : (float)par[P_LATE];
}
static void bass_set(void)
{
    float tone = (float)par[P_SUB] * 0.01f;
    h_fm_t *f;
    if (bass_target <= 0)
        return;
    fm_release(&bass[bass_cur], 1.5f);
    bass_cur ^= 1u;
    f = &bass[bass_cur];
    f->ratio = 1.0f;
    f->idx = 0.15f + 0.6f * tone;
    f->fb = 0.1f;
    f->det = 1.0f;
    f->last = 0.0f;
    h_env_set(&f->amp, 1500.0f, 1000.0f, 1.0f, 1500.0f);
    h_env_set(&f->mod, 2.0f, 2000.0f, 0.7f, 1500.0f);
    h_fm_on(f, h_mtof((float)bass_target), 1.0f);
    bass_note = bass_target;
    en_bass_note = (uint8_t)bass_note;
    bass_target = 0;
    bass_late = -1.0f;
}
static void bass_off(float s)
{
    fm_release(&bass[0], s);
    fm_release(&bass[1], s);
    bass_note = 0;
    bass_target = 0;
    bass_late = -1.0f;
    en_bass_note = 0;
}

/* a change: the next chord (or the resolution after a break), the voices to it */
static void step(int resolve)
{
    int pcs[4], n, want, moves, nf;
    float rise = swell_s, fall = swell_s;
    if (resolve) {
        cur_deg = 0;
        cur_qual = Q_TRIAD;
        n = chord_pcs(0, Q_TRIAD, pcs);
        pcs[1] = pcs[n - 1];                             /* the root and the fifth */
        n = 2;
        want = 2;
        moves = EN_NVOICES;
        thin = par[P_VOICES] - 2;
    } else {
        next_chord(p_eff);
        n = chord_pcs(cur_deg, cur_qual, pcs);
        if (thin > 0)
            thin--;
        want = par[P_VOICES] - thin;
        moves = par[P_MOVEMENT] >= 8 ? EN_NVOICES : par[P_MOVEMENT];
        if (p_eff > 0.5f)                                /* under pressure more of them move */
            moves += (int)((p_eff - 0.5f) * 2.0f * (float)(EN_NVOICES - moves) + 0.5f);
    }
    nf = EN_NVOICES - npins();
    if (want > nf)
        want = nf;
    if (want < 1)
        want = nf > 0 ? 1 : 0;
    count_voices(want, pcs, n);
    lead(pcs, n, moves, rise, fall);
    en_deg = (uint8_t)cur_deg;
    en_qual = (uint8_t)cur_qual;
    en_root_pc = (uint8_t)pcs[0];
    en_nvoices = (uint8_t)nfree_now();
    bass_follow(resolve);
    en_changes++;
    schedule();
}

static void phase_build(void)
{
    const climate_t *cl = &CLIMATE[par[P_CLIMATE]];
    phase = 0;
    build_s = h_lerp(cl->build_lo, cl->build_hi, h_frand(&rng));
}
static void climate_start(void)                         /* the pressure curve from its beginning */
{
    switch (par[P_CLIMATE]) {
    case CL_STILL: p_auto = 0.15f; p_target = 0.2f; phase_s = 30.0f; break;
    case CL_FRONT: case CL_STORM: p_auto = 0.05f; phase_build(); break;
    case CL_CLEARING: p_auto = 0.9f; break;
    case CL_FOG: p_auto = 0.5f; p_target = 0.5f; phase_s = 20.0f; break;
    default: break;
    }
    phase = 0;
    thin = 0;
    en_rest = 0;
    break_pending = 0;
    if (en_playing)
        schedule();
}
/* once a block: the pressure curve */
static void weather(float dt)
{
    const climate_t *cl = &CLIMATE[par[P_CLIMATE]];
    int c = par[P_CLIMATE];
    if (c == CL_MANUAL) {
        p_auto = ((float)par[P_PRESSURE] + 50.0f) * 0.01f;
        p_eff = p_auto;
        return;
    }
    if (en_playing && !en_frozen) {
        switch (c) {
        case CL_STILL:
            phase_s -= dt;
            if (phase_s <= 0.0f) {
                phase_s = 60.0f + 60.0f * h_frand(&rng);
                p_target = 0.08f + 0.32f * h_frand(&rng);
            }
            p_auto += (p_target - p_auto) * dt * (1.0f / 30.0f);
            break;
        case CL_FRONT: case CL_STORM:
            if (phase == 0) {
                p_auto += dt / build_s;
                if (p_auto >= 1.0f) {                    /* the break */
                    p_auto = 0.02f;
                    phase = 1;
                    phase_s = h_lerp(cl->rest_lo, cl->rest_hi, h_frand(&rng));
                    break_pending = 1;
                    en_breaks++;
                }
            } else {
                phase_s -= dt;
                p_auto += (0.05f - p_auto) * dt * 0.2f;
                if (phase_s <= 0.0f) {
                    phase_build();
                    en_rest = 0;
                    schedule();
                }
            }
            break;
        case CL_CLEARING:
            p_auto -= dt * (1.0f / 300.0f);
            if (p_auto < 0.0f)
                p_auto = 0.0f;
            break;
        default:                                         /* fog */
            phase_s -= dt;
            if (phase_s <= 0.0f) {
                phase_s = 30.0f + 30.0f * h_frand(&rng);
                p_target = 0.3f + 0.55f * h_frand(&rng);
            }
            p_auto += (p_target - p_auto) * dt * (1.0f / 20.0f);
            break;
        }
    }
    p_eff = fm_clampf(p_auto + (float)par[P_PRESSURE] * 0.01f, 0.0f, 1.0f);
}

/* ------------------------------------------------------------------------------------------------- bells --- */
static void bell_schedule(void)
{
    float mean = (float)par[P_BELLEVERY], sp = (float)par[P_BELLSPREAD] * 0.01f;
    float t = mean * (1.0f + sp * h_grand(&rng)) / (0.5f + p_eff);
    bell_mean_used = mean;
    bell_next = t < 0.4f ? 0.4f : t;
}
static void bell_fire(void)
{
    int degs[EN_NBELLDEG], nd = 0, i, deg, note, c = centre() + 12, broot, slot = -1, busy = 0;
    for (i = 0; i < EN_NBELLDEG; i++)
        if (en_bells >> i & 1u)
            degs[nd++] = i;
    if (!nd)
        return;
    for (i = 0; i < EN_NBELLS; i++) {
        if (h_env_busy(&bell[i].amp))
            busy++;
        else if (slot < 0)
            slot = i;
    }
    if (slot < 0 || busy >= par[P_BELLMAX])
        return;
    deg = degs[h_irand(&rng, nd)];
    broot = c - ((c - par[P_ROOT]) % 12 + 12) % 12;      /* the root at or below an octave over the field's centre */
    note = h_deg_note(pool(), broot, deg);
    {
        h_fm_t *f = &bell[slot];
        float vel = 0.35f + 0.4f * h_frand(&rng);
        f->ratio = h_chance(&rng, 0.5f) ? 2.76f : 3.5f;
        f->idx = 0.9f + 0.5f * h_frand(&rng);
        f->fb = 0.0f;
        f->det = 1.0f;
        f->last = 0.0f;
        h_env_set(&f->amp, 2.0f, 2200.0f + 1800.0f * h_frand(&rng), 0.0f, 800.0f);
        h_env_set(&f->mod, 1.0f, 500.0f, 0.15f, 800.0f);
        h_fm_on(f, h_mtof((float)note), vel);
        bell_echo[slot] = h_chance(&rng, (float)par[P_BELLECHO] * 0.01f) ? 1.0f : 0.0f;
    }
    en_bell_note = (uint8_t)note;
    en_bell_deg = (uint8_t)deg;
    en_bells_fired++;
}

/* ---------------------------------------------------------------------------------------------- the echo --- */
static float pecho(pecho_t *e, float in)
{
    const float W = (float)(SHIFT_N - 4u);
    float y = h_dl_tapi(&e->dl, ECHO_N - 2u), d1, d2, g1, g2, s;
    /* the repeat an octave down: two heads reading at half speed over a window, crossfaded */
    h_dl_push(&e->sh, y);
    e->pos += 0.5f;
    if (e->pos >= W)
        e->pos -= W;
    d1 = e->pos;
    d2 = d1 + W * 0.5f;
    if (d2 >= W)
        d2 -= W;
    g1 = 1.0f - fm_fabsf(2.0f * d1 / W - 1.0f);
    g2 = 1.0f - fm_fabsf(2.0f * d2 / W - 1.0f);
    s = h_dl_tap(&e->sh, d1 + 1.0f) * g1 + h_dl_tap(&e->sh, d2 + 1.0f) * g2;
    s = h_lp1(&e->lp, s, 0.12f);
    s = h_hp1(&e->hp, s, 0.006f);
    h_dl_push(&e->dl, fm_clampf(in + s * e->fb, -1.0f, 1.0f));
    return y;
}

/* ---------------------------------------------------------------------------------------------- settings --- */
static void depth_set(void)
{
    float d = (float)par[P_DEPTH] * 0.01f, damp = (float)par[P_DAMP] * 0.01f;
    rev_send = d * d * 0.9f;
    h_plate_set(&plate, h_lerp(0.55f, 0.985f, d > 0.001f ? fm_powf(d, 0.6f) : 0.0f), 0.15f + 0.8f * damp, 0.7f, 20.0f);
    echo_send = d > 0.4f ? (d - 0.4f) * (1.0f / 0.6f) : 0.0f;
    echo_pad = echo_send * 0.12f;
    echo.fb = 0.25f + 0.45f * echo_send;
}
static void apply(int p, int v)
{
    const timbre_t *t;
    int i;
    par[p] = (int16_t)v;
    switch (p) {
    case P_CLIMATE:
        climate_start();
        break;
    case P_DEPTH: case P_DAMP:
        depth_set();
        break;
    case P_BREATH:                                       /* the pending change keeps its fraction of the breath */
        if (breath_used > 0.0f && !en_ext)
            next_s = next_s * (float)v / breath_used;
        breath_used = (float)v;
        break;
    case P_BELLEVERY:
        if (bell_mean_used > 0.0f)
            bell_next = bell_next * (float)v / bell_mean_used;
        bell_mean_used = (float)v;
        break;
    case P_SWELL: swell_s = s_curve(v, 0.02f, 10.0f); break;
    case P_ATTACK: attack_s = s_curve(v, 0.05f, 10.0f); break;
    case P_RELEASE: release_s = s_curve(v, 0.05f, 20.0f); break;
    case P_PAD: pad_gain = (float)v * 0.01f * 0.34f; break;
    case P_WIDTH:
        for (i = 0; i < EN_NVOICES; i++)
            vc[i].pan = SPREAD[i] * (float)v * 0.01f;
        break;
    case P_BASSLEVEL: bass_gain = (float)v * 0.01f * 0.3f; break;
    case P_SUB: bass_lp_k = h_lp1_k(h_lerp(120.0f, 1200.0f, (float)v * 0.01f)); break;
    case P_BED: bed.type = (uint8_t)v; break;
    case P_AIR:
        bed.level = (float)v * 0.01f * 0.5f;
        bed.density = 0.3f + 0.7f * (float)v * 0.01f;
        break;
    case P_HIPASS: hp_k = h_lp1_k(h_lerp(20.0f, 500.0f, h_sq((float)v * 0.01f))); break;
    case P_OCTAVE: case P_RANGE:
        range_set();
        break;
    case P_VOICE: case P_TONE:
        t = &TIMBRE[par[P_VOICE]];
        lp_k = h_lp1_k(fm_clampf(t->tone * s_curve(par[P_TONE], 0.12f, 2.5f), 300.0f, 16000.0f));
        break;
    case P_BASS:
        if (v == BS_OFF)
            bass_off(2.0f);
        else if (en_playing)
            bass_follow(1);
        break;
    default: break;
    }
}

/* --------------------------------------------------------------------------------------------- transport --- */
static void play_start(void)
{
    int i, pcs[4], n;
    en_playing = 1;
    en_frozen = 0;
    climate_start();
    weather(0.0f);
    cur_deg = 0;
    cur_qual = Q_TRIAD;
    for (i = 0; i < EN_NPINS; i++)                       /* the pins sound again */
        if (en_pins[i]) {
            int k;
            for (k = 0; k < EN_NVOICES; k++)
                if (vc[k].pin == i + 1 && !vc[k].on)
                    voice_move(k, en_pins[i], attack_s, release_s);
        }
    n = chord_pcs(0, Q_TRIAD, pcs);
    {
        int want = par[P_VOICES], nf = EN_NVOICES - npins();
        if (want > nf)
            want = nf;
        count_voices(want, pcs, n);
        lead(pcs, n, EN_NVOICES, attack_s, release_s);
    }
    en_deg = 0;
    en_qual = Q_TRIAD;
    en_root_pc = (uint8_t)pcs[0];
    en_nvoices = (uint8_t)nfree_now();
    bass_follow(1);
    schedule();
    bell_schedule();
}
static void play_stop(void)                             /* the voices release over BREATH seconds */
{
    int i;
    float s = breath_now();
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].on)
            voice_release(i, s);
    bass_off(s);
    en_playing = 0;
    en_frozen = 0;
    en_rest = 0;
    en_nvoices = 0;
}
static void quiet(void)
{
    int i;
    for (i = 0; i < EN_NVOICES; i++)
        voice_quiet(i);
    for (i = 0; i < 2; i++) {
        bass[i].amp.st = H_ENV_OFF;
        bass[i].amp.v = 0.0f;
    }
    for (i = 0; i < EN_NBELLS; i++) {
        bell[i].amp.st = H_ENV_OFF;
        bell[i].amp.v = 0.0f;
    }
    H_PLATE_INIT(&plate, pl);
    h_dl_init(&echo.dl, echo_buf, ECHO_N);
    h_dl_init(&echo.sh, shift_buf, SHIFT_N);
    depth_set();
}

static void pin(int key, int note)
{
    int i, slot = -1;
    if (key < 0 || key >= EN_NPINS)
        return;
    for (i = 0; i < EN_NVOICES; i++)                     /* this key's voice, if it has one */
        if (vc[i].pin == key + 1)
            slot = i;
    if (!note) {
        en_pins[key] = 0;
        if (slot >= 0) {
            voice_release(slot, release_s);
            vc[slot].pin = 0;
            en_vpin[slot] = 0;
        }
        return;
    }
    if (note < 24 || note > 108)
        return;
    en_pins[key] = (uint8_t)note;
    if (slot < 0) {                                      /* a silent slot, else the highest free voice */
        for (i = 0; i < EN_NVOICES; i++)
            if (!vc[i].on && !vc[i].pin && (slot < 0 || !voice_busy(&vc[i])))
                slot = i;
        if (slot < 0) {
            int hn = -1;
            for (i = 0; i < EN_NVOICES; i++)
                if (!vc[i].pin && (int)vc[i].note > hn) {
                    hn = vc[i].note;
                    slot = i;
                }
        }
    }
    if (slot < 0)
        return;
    vc[slot].pin = (uint8_t)(key + 1);
    en_vpin[slot] = (uint8_t)(key + 1);
    if (en_playing)
        voice_move(slot, note, attack_s, release_s);
    en_nvoices = (uint8_t)nfree_now();
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
        case C_FREEZE:
            en_frozen = (uint8_t)(c.a != 0 && en_playing);
            break;
        case C_PIN: pin(c.a, (int)c.b); break;
        case C_BELL:
            if (c.a >= 0 && c.a < EN_NBELLDEG) {
                if (c.b)
                    en_bells |= 1u << c.a;
                else
                    en_bells &= ~(1u << c.a);
            }
            break;
        case C_PINS: {
            int i;
            for (i = 0; i < EN_NPINS; i++)
                pin(i, shadow_pins[i]);
            en_bells = shadow_bells & ((1u << EN_NBELLDEG) - 1u);
            break;
        }
        case C_PANIC:
            play_stop();
            quiet();
            break;
        case C_CLOCK:
            if (c.a == EN_CLK_TICK) {
                en_ext = 1;
                ext_age = 0;
                ext_ticks++;
                if (ext_ticks >= 96u) {                  /* a bar: its length */
                    ext_bar_s = ext_t;
                    ext_t = 0.0f;
                    ext_ticks = 0;
                }
            } else if (c.a == EN_CLK_START) {
                ext_ticks = 0;
                ext_t = 0.0f;
                if (!en_playing)
                    play_start();
            } else if (c.a == EN_CLK_STOP) {
                if (en_playing)
                    play_stop();
            }
            break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ render --- */
static void zero(void *p, uint32_t n)                   /* (no libc in this unit) */
{
    uint8_t *b = p;
    while (n--)
        *b++ = 0;
}
void en_init(void)
{
    int i;
    for (i = 0; i < P_N; i++)
        par[i] = EN_PARAMS[i].def;
    zero(vc, sizeof vc);                                 /* a boot (or a reboot): nothing sounds, nothing is pinned */
    zero(bass, sizeof bass);
    zero(bell, sizeof bell);
    zero((void *)en_vnote, sizeof en_vnote);
    zero((void *)en_vpin, sizeof en_vpin);
    zero((void *)en_pins, sizeof en_pins);
    en_playing = en_frozen = en_rest = en_ext = en_nvoices = en_bass_note = 0;
    en_changes = en_breaks = en_bells = en_bells_fired = 0;
    bass_note = bass_target = 0;
    break_pending = 0;
    thin = 0;
    rng.s = 0xA5A5A5A5u;
    h_bed_init(&bed, 0x777u);
    H_PLATE_INIT(&plate, pl);
    h_dl_init(&echo.dl, echo_buf, ECHO_N);
    h_dl_init(&echo.sh, shift_buf, SHIFT_N);
    echo.pos = 0.0f;
    echo.fb = 0.4f;
    for (i = 0; i < EN_NVOICES; i++) {
        vc[i].shim_rate = 0.04f + 0.03f * (float)i;
        vc[i].shim_ph = (float)i * 0.13f;
        vc[i].gain = vc[i].gain_t = 0.0f;
    }
    bass_late = -1.0f;
    range_set();
    for (i = 0; i < P_N; i++)
        apply(i, par[i]);
    weather(0.0f);
}

static void tail_end(h_fm_t *f)                         /* a release gone below -50 dB is over */
{
    if (f->amp.st == H_ENV_R && f->amp.v < 0.003f) {
        f->amp.st = H_ENV_OFF;
        f->amp.v = 0.0f;
        f->mod.st = H_ENV_OFF;
    }
}
static void control(uint32_t n)                         /* once a block: the slow things */
{
    float dt = (float)n * H_ISR;
    int i, nv = 0;
    for (i = 0; i < EN_NVOICES; i++) {
        tail_end(&vc[i].fm[0]);
        tail_end(&vc[i].fm[1]);
    }
    tail_end(&bass[0]);
    tail_end(&bass[1]);
    for (i = 0; i < EN_NBELLS; i++)
        tail_end(&bell[i]);
    float beat = (float)par[P_BEAT], walk = (float)par[P_WALK] * 0.01f, shim = (float)par[P_SHIMMER] * 0.01f;
    weather(dt);
    en_pressure = p_eff;
    if (en_ext) {
        ext_t += dt;
        if (++ext_age > 400u) {                          /* no clock for two seconds: our own time again */
            en_ext = 0;
            ext_bar_s = 0.0f;
        }
    }
    en_breath_s = breath_now();
    if (en_playing && !en_frozen) {
        if (break_pending) {                             /* the front breaks: the resolution, then rest */
            break_pending = 0;
            en_rest = 1;
            step(1);
            next_s = phase_s + breath_now();
        } else if (!en_rest) {
            next_s -= dt;
            if (next_s <= 0.0f)
                step(0);
        }
        if (bass_late >= 0.0f) {
            bass_late -= dt;
            if (bass_late <= 0.0f)
                bass_set();
        }
    }
    if (en_playing) {                                    /* the bells ring on, frozen or not */
        bell_next -= dt;
        if (bell_next <= 0.0f) {
            bell_fire();
            bell_schedule();
        }
    }
    en_next_s = en_rest ? phase_s : next_s;
    /* each voice: the detune walk, the shimmer, its gain */
    for (i = 0; i < EN_NVOICES; i++) {
        voice_t *v = &vc[i];
        float d;
        if (h_chance(&rng, dt * (0.02f + 0.3f * walk)))
            v->det_t = h_brand(&rng) * beat;
        v->det += (v->det_t - v->det) * dt * (0.05f + 0.6f * walk);
        d = h_semis(v->det * 0.01f);
        v->fm[0].det = d;
        v->fm[1].det = d;
        v->shim_ph = h_wrap(v->shim_ph + v->shim_rate * dt);
        if (v->on || voice_busy(v))
            nv++;
    }
    {
        float g = pad_gain / fm_sqrtf((float)(nv < 2 ? 2 : nv));
        for (i = 0; i < EN_NVOICES; i++)
            vc[i].gain_t = g * (1.0f - shim * 0.35f * (1.0f + h_sin(vc[i].shim_ph)));
    }
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float g = h_gain(gain_q12, master_trim);
    float peak = 0.0f;
    uint32_t i;
    int v;
    drain();
    control(n);
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, x, rl, rr, bl, br, e, ein = 0.0f, b = 0.0f, bells_out = 0.0f;
        for (v = 0; v < EN_NVOICES; v++) {
            voice_t *vo = &vc[v];
            float s = 0.0f;
            int busy = 0;
            if (h_env_busy(&vo->fm[0].amp)) {
                s += h_fm(&vo->fm[0]);
                busy = 1;
            }
            if (h_env_busy(&vo->fm[1].amp)) {
                s += h_fm(&vo->fm[1]);
                busy = 1;
            }
            if (!busy)
                continue;
            vo->gain += (vo->gain_t - vo->gain) * 0.0005f;
            s = h_lp1(&vo->lp, s, lp_k) * vo->gain;
            l += s * (1.0f - vo->pan * 0.6f);
            r += s * (1.0f + vo->pan * 0.6f);
        }
        if (h_env_busy(&bass[0].amp))
            b += h_fm(&bass[0]);
        if (h_env_busy(&bass[1].amp))
            b += h_fm(&bass[1]);
        b = h_lp1(&bass_lp, b, bass_lp_k) * bass_gain;
        for (v = 0; v < EN_NBELLS; v++)
            if (h_env_busy(&bell[v].amp)) {
                float s = h_fm(&bell[v]) * 0.22f;
                bells_out += s;
                ein += s * bell_echo[v];
            }
        l = h_hp1(&hp_l, l, hp_k) + bells_out * 0.9f + b;
        r = h_hp1(&hp_r, r, hp_k) + bells_out * 1.1f + b;
        x = (l + r) * 0.5f;
        e = pecho(&echo, ein * echo_send + x * echo_pad);
        h_plate(&plate, (x + e * 0.5f) * rev_send, &rl, &rr);
        h_bed(&bed, &bl, &br);
        l += rl + e * 0.4f + bl;
        r += rr + e * 0.4f + br;
        l = h_dc(&dcl, l);
        r = h_dc(&dcr, r);
        h_lim(&lim, &l, &r);
        if (fm_fabsf(l) > peak)
            peak = fm_fabsf(l);
        h_out(out + 2 * i, l, r, g);
    }
    en_level = h_lp1(&lvl_lp, peak, 0.3f);
}
