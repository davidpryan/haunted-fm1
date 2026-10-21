/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan
 *
 * hdsp.h: the DSP the five haunted firmwares share. Float only, no libm (fm1-base's fastmath.h), single precision
 * (the FM-1's FPU has no double). Everything is static: each firmware's engine includes this once, in its own -O2
 * unit, and the linker drops what it does not use.
 *
 *   random        h_rng_t: xorshift, floats in 0..1 and -1..1, chances
 *   pitch         h_mtof, h_sin (turns), glides and time constants
 *   filters       h_lp1 (one pole), h_svf (Zavalishin's TPT state variable: lp, bp, hp), h_dc (DC blocker)
 *   envelopes     h_env_t: exponential attack, decay, sustain, release
 *   voices        h_fm_t: a two-operator FM voice with feedback (pads, bells, bass, piano)
 *                 h_voice_t: a formant voice (a glottal pulse and breath through four resonances, vowels a e i o u,
 *                 shifted for a man, a woman, a child; pitch and formants apart, so a voice slowed down keeps its
 *                 throat: the Burial trick)
 *   delays        h_dl_t: a 16-bit delay line with a fractional tap (echoes, tape); h_fdl_t: a float one (the plate)
 *   reverb        h_plate_t: Dattorro's plate ("Effect Design, Part 1", JAES 1997) at 44.1 kHz, the tank's long
 *                 delays in 16 bits, its allpasses in float: 90 KB of pool
 *   echo          h_echo_t: a tape echo, up to a second, darker on each repeat, with wow
 *   tape          h_wow_t: wow and flutter as a pitch factor; h_sat (tape saturation)
 *   beds          h_bed_t: rain, vinyl crackle, hiss, wind, room tone: a quiet floor that is always there
 *   harmony       scales as 12-bit pools, degrees to notes, stacked-third chords, nearest voice leading
 *   output        h_out: float stereo to the FM-1's 24-bit frames, with the MASTER gain
 */
#pragma once
#include <stdint.h>
#include "fastmath.h"

#define H_SR 44100.0f
#define H_ISR (1.0f / 44100.0f)

#if defined PLAT_HOST || defined EN_HOST || defined H_HOST
#define H_POOL
#else
#define H_POOL __attribute__((section(".pool")))
#endif
#define H_BARRIER() __asm__ volatile("" ::: "memory")

/* ------------------------------------------------------------------------------------------------- random --- */
typedef struct { uint32_t s; } h_rng_t;
static inline uint32_t h_rnd(h_rng_t *r)
{
    uint32_t x = r->s ? r->s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}
static inline float h_frand(h_rng_t *r) { return (float)(h_rnd(r) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */
static inline float h_brand(h_rng_t *r) { return h_frand(r) * 2.0f - 1.0f; }                        /* -1..1 */
static inline int h_irand(h_rng_t *r, int n) { return n > 0 ? (int)((h_rnd(r) >> 8) % (uint32_t)n) : 0; }
static inline int h_chance(h_rng_t *r, float p) { return h_frand(r) < p; }
/* a gaussian-ish value (three uniforms), -1.5..1.5 mostly, for timing spreads */
static inline float h_grand(h_rng_t *r) { return (h_brand(r) + h_brand(r) + h_brand(r)) * 0.5f; }

/* -------------------------------------------------------------------------------------------------- pitch --- */
static inline float h_mtof(float n) { return 440.0f * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static inline float h_semis(float s) { return fm_exp2f(s * (1.0f / 12.0f)); }     /* a pitch factor from semitones */
static inline float h_sin(float ph) { return fm_sin_turns(ph); }                  /* ph in turns */
static inline float h_wrap(float ph) { return ph - fm_floorf(ph); }
static inline float h_glide(float cur, float target, float k) { return cur + (target - cur) * k; }
static inline float h_k_ms(float ms) { return ms <= 0.0f ? 1.0f : 1.0f - fm_expf(-1000.0f / (ms * H_SR)); }
static inline float h_lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float h_sq(float x) { return x * x; }
static inline float h_db(float db) { return fm_db2lin(db); }

/* ------------------------------------------------------------------------------------------------ filters --- */
typedef struct { float z; } h_lp1_t;
static inline float h_lp1_k(float hz)
{
    float k = FM_TWO_PI * hz * H_ISR;
    return k > 1.0f ? 1.0f : k;
}
static inline float h_lp1(h_lp1_t *f, float x, float k)
{
    f->z += (x - f->z) * k;
    return f->z;
}
static inline float h_hp1(h_lp1_t *f, float x, float k) { return x - h_lp1(f, x, k); }

typedef struct { float ic1, ic2, g, k, a1, a2, a3; } h_svf_t;
static inline void h_svf_set(h_svf_t *s, float hz, float q)
{
    hz = fm_clampf(hz, 15.0f, 18000.0f);
    s->g = fm_tanf(FM_PI * hz * H_ISR);
    s->k = 1.0f / fm_maxf(q, 0.1f);
    s->a1 = 1.0f / (1.0f + s->g * (s->g + s->k));
    s->a2 = s->g * s->a1;
    s->a3 = s->g * s->a2;
}
static inline void h_svf(h_svf_t *s, float x, float *lp, float *bp, float *hp)
{
    float v3 = x - s->ic2, v1 = s->a1 * s->ic1 + s->a2 * v3, v2 = s->ic2 + s->a2 * s->ic1 + s->a3 * v3;
    s->ic1 = 2.0f * v1 - s->ic1;
    s->ic2 = 2.0f * v2 - s->ic2;
    *lp = v2;
    *bp = v1;
    *hp = x - s->k * v1 - v2;
}
static inline float h_svf_lp(h_svf_t *s, float x) { float l, b, h; h_svf(s, x, &l, &b, &h); return l; }
static inline float h_svf_bp(h_svf_t *s, float x) { float l, b, h; h_svf(s, x, &l, &b, &h); return b; }
static inline float h_svf_hp(h_svf_t *s, float x) { float l, b, h; h_svf(s, x, &l, &b, &h); return h; }

typedef struct { float x1, y1; } h_dc_t;
static inline float h_dc(h_dc_t *d, float x)
{
    float y = x - d->x1 + 0.9975f * d->y1;
    d->x1 = x;
    d->y1 = y;
    return y;
}

/* --------------------------------------------------------------------------------------------- envelopes --- */
enum { H_ENV_OFF, H_ENV_A, H_ENV_D, H_ENV_S, H_ENV_R };
typedef struct { float v, a, d, s, r; uint8_t st; } h_env_t;
static inline void h_env_set(h_env_t *e, float a_ms, float d_ms, float s, float r_ms)
{
    e->a = h_k_ms(a_ms);
    e->d = h_k_ms(d_ms);
    e->s = s;
    e->r = h_k_ms(r_ms);
}
static inline void h_env_on(h_env_t *e) { e->st = H_ENV_A; }
static inline void h_env_off(h_env_t *e) { if (e->st) e->st = H_ENV_R; }
static inline int h_env_busy(const h_env_t *e) { return e->st != H_ENV_OFF; }
static inline float h_env(h_env_t *e)
{
    switch (e->st) {
    case H_ENV_A:
        e->v += (1.25f - e->v) * e->a;
        if (e->v >= 1.0f) {
            e->v = 1.0f;
            e->st = H_ENV_D;
        }
        break;
    case H_ENV_D:
        e->v += (e->s - e->v) * e->d;
        if (e->v - e->s < 0.001f)
            e->st = H_ENV_S;
        break;
    case H_ENV_S:
        e->v = e->s;
        break;
    case H_ENV_R:
        e->v += (0.0f - e->v) * e->r;
        if (e->v < 0.0002f) {
            e->v = 0.0f;
            e->st = H_ENV_OFF;
        }
        break;
    default:
        e->v = 0.0f;
    }
    return e->v;
}

/* ------------------------------------------------------------------------------------------------ FM voice --- */
/* two operators: a carrier modulated by a modulator that feeds back on itself. ratio: the modulator's frequency
 * over the carrier's; index: modulation depth in turns (0.5..3 bells, 0.1..0.6 soft pads); fb: the modulator's
 * feedback. The modulator has its own envelope, so a note can start bright and darken (a struck string). */
typedef struct {
    float phc, phm, last, freq, ratio, idx, fb, det;
    h_env_t amp, mod;
    uint8_t note;
    float vel;
} h_fm_t;
static inline void h_fm_on(h_fm_t *v, float freq, float vel)
{
    v->freq = freq;
    v->vel = vel;
    v->phc = v->phm = 0.0f;
    h_env_on(&v->amp);
    h_env_on(&v->mod);
}
static inline float h_fm(h_fm_t *v)
{
    float m, c, mi = v->idx * h_env(&v->mod), f = v->freq * v->det;
    m = h_sin(v->phm + v->fb * v->last);
    v->last = m;
    c = h_sin(v->phc + mi * m);
    v->phm = h_wrap(v->phm + f * v->ratio * H_ISR);
    v->phc = h_wrap(v->phc + f * H_ISR);
    return c * h_env(&v->amp) * v->vel;
}

/* -------------------------------------------------------------------------------------------- formant voice --- */
/* a sung vowel: a glottal pulse (a soft sawtooth, darker than a saw) with breath noise, through four parallel
 * resonances. Formants from the classic tables (Peterson and Barney / Csound's), for a man; the shift scales them
 * (1.17 a woman, 1.35 a child; 0.85 a slowed-down man, the ghost). Vowel is a float 0..4 over a e i o u: in
 * between, the formants slide. */
typedef struct {
    float ph, freq, vowel, shift, breath, f[4], g[4], q[4];
    h_svf_t r[4];
    h_lp1_t lp, br;
    h_rng_t rng;
    uint32_t coef_n;
} h_voice_t;
static const float H_VOWEL_F[5][4] = {{650.0f, 1080.0f, 2650.0f, 2900.0f},     /* a */
                                      {400.0f, 1700.0f, 2600.0f, 3200.0f},     /* e */
                                      {290.0f, 1870.0f, 2800.0f, 3250.0f},     /* i */
                                      {400.0f, 800.0f, 2600.0f, 2800.0f},      /* o */
                                      {350.0f, 600.0f, 2700.0f, 2900.0f}};     /* u */
static const float H_VOWEL_G[5][4] = {{1.0f, 0.5f, 0.3f, 0.2f}, {1.0f, 0.35f, 0.3f, 0.2f}, {1.0f, 0.2f, 0.25f, 0.2f},
                                      {1.0f, 0.3f, 0.12f, 0.1f}, {1.0f, 0.25f, 0.1f, 0.08f}};
static inline void h_voice_coefs(h_voice_t *v)
{
    float vw = fm_clampf(v->vowel, 0.0f, 3.999f);
    int a = (int)vw, b = a + 1, i;
    float t = vw - (float)a;
    for (i = 0; i < 4; i++) {
        float f = h_lerp(H_VOWEL_F[a][i], H_VOWEL_F[b][i], t) * v->shift;
        v->f[i] = f;
        v->g[i] = h_lerp(H_VOWEL_G[a][i], H_VOWEL_G[b][i], t);
        h_svf_set(&v->r[i], f, 8.0f + 2.0f * (float)i);
    }
}
static inline void h_voice_init(h_voice_t *v, uint32_t seed)
{
    int i;
    for (i = 0; i < 4; i++)
        v->r[i].ic1 = v->r[i].ic2 = 0.0f;
    v->ph = 0.0f;
    v->lp.z = v->br.z = 0.0f;
    v->coef_n = 0;
    v->shift = 1.0f;
    v->vowel = 0.0f;
    v->breath = 0.15f;
    v->freq = 110.0f;
    v->rng.s = seed | 1u;
    h_voice_coefs(v);
}
static inline float h_voice(h_voice_t *v)
{
    float x, n, out = 0.0f, l, b, h;
    int i;
    if ((++v->coef_n & 63u) == 0u)
        h_voice_coefs(v);
    v->ph = h_wrap(v->ph + v->freq * H_ISR);
    x = v->ph * v->ph * (3.0f - 2.0f * v->ph) * 2.0f - 1.0f;      /* a rounded saw: the glottal pulse */
    x = h_lp1(&v->lp, x, 0.35f);
    n = h_lp1(&v->br, h_brand(&v->rng), 0.4f);
    x = x * (1.0f - v->breath) + n * v->breath * (0.6f + 0.4f * v->ph);   /* breath, more as the glottis opens */
    for (i = 0; i < 4; i++) {
        h_svf(&v->r[i], x, &l, &b, &h);
        out += b * v->g[i];
    }
    return out * 0.35f;
}

/* ----------------------------------------------------------------------------------------------- commands --- */
/* main loop -> render: the UI posts, the render takes at the start of a block (one writer, one reader) */
typedef struct { uint8_t c; int16_t a; int32_t b; } h_cmd_t;
typedef struct { h_cmd_t q[64]; volatile uint32_t w, r; } h_cmdq_t;
static inline void h_post(h_cmdq_t *q, uint32_t c, int a, int32_t b)
{
    uint32_t w = q->w;
    if (w - q->r >= 64u)
        return;                                                      /* full: the command is lost, never a crash */
    q->q[w & 63u].c = (uint8_t)c;
    q->q[w & 63u].a = (int16_t)a;
    q->q[w & 63u].b = b;
    H_BARRIER();
    q->w = w + 1u;
}
static inline int h_take(h_cmdq_t *q, h_cmd_t *out)
{
    uint32_t r = q->r;
    if (r == q->w)
        return 0;
    *out = q->q[r & 63u];
    H_BARRIER();
    q->r = r + 1u;
    return 1;
}

/* ------------------------------------------------------------------------------------------------- delays --- */
typedef struct { int16_t *b; uint32_t n, w; } h_dl_t;
static inline void h_dl_init(h_dl_t *d, int16_t *buf, uint32_t n)
{
    uint32_t i;
    d->b = buf;
    d->n = n;
    d->w = 0;
    for (i = 0; i < n; i++)
        buf[i] = 0;
}
static inline int16_t h_s16(float x)
{
    x *= 32767.0f;
    return (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : (int32_t)x);
}
static inline void h_dl_push(h_dl_t *d, float x)
{
    d->b[d->w] = h_s16(x);
    if (++d->w >= d->n)
        d->w = 0;
}
static inline float h_dl_tapi(const h_dl_t *d, uint32_t back)   /* back samples ago, 1 .. n */
{
    uint32_t i = d->w + d->n - (back >= d->n ? d->n - 1u : back);
    return (float)d->b[i >= d->n ? i - d->n : i] * (1.0f / 32768.0f);
}
static inline float h_dl_tap(const h_dl_t *d, float back)       /* fractional */
{
    uint32_t i;
    float f;
    if (back < 1.0f)
        back = 1.0f;
    if (back > (float)(d->n - 2u))
        back = (float)(d->n - 2u);
    i = (uint32_t)back;
    f = back - (float)i;
    return h_dl_tapi(d, i) * (1.0f - f) + h_dl_tapi(d, i + 1u) * f;
}

typedef struct { float *b; uint32_t n, w; } h_fdl_t;
static inline void h_fdl_init(h_fdl_t *d, float *buf, uint32_t n)
{
    uint32_t i;
    d->b = buf;
    d->n = n;
    d->w = 0;
    for (i = 0; i < n; i++)
        buf[i] = 0.0f;
}
static inline float h_fdl_tap(const h_fdl_t *d, uint32_t back)
{
    uint32_t i = d->w + d->n - back;
    return d->b[i >= d->n ? i - d->n : i];
}
static inline void h_fdl_push(h_fdl_t *d, float x)
{
    d->b[d->w] = x;
    if (++d->w >= d->n)
        d->w = 0;
}
/* an allpass of length len inside a float line (len <= n), Dattorro's sign convention */
static inline float h_ap(h_fdl_t *d, uint32_t len, float x, float g)
{
    float z = h_fdl_tap(d, len), y = x - g * z;
    h_fdl_push(d, y);
    return z + g * y;
}

/* ------------------------------------------------------------------------------------------------- plate --- */
/* Dattorro's plate, the paper's lengths at 29.761 kHz scaled to 44.1 kHz. A predelay, a bandwidth lowpass, four
 * input diffusers, then a figure-eight tank: each half an allpass whose length wobbles (an excursion of 16), a long
 * delay, a damping lowpass times decay, a second allpass, a second long delay, feeding the other half. Stereo out from
 * seven taps a side. The four long delays (16 bits) and the six allpasses (float) are the caller's buffers:
 * H_PLATE_BUFFERS(name) declares them in the pool, H_PLATE_INIT(p, name) hands them over. */
#define H_DS(x) ((uint32_t)(((x) * 14818u + 5000u) / 10000u))   /* x at 29.761 kHz, at 44.1 kHz (integer: array sizes) */
#define H_PL_EXC 16u
#define H_PL_PRE 2048u
#define H_PL_IN1 H_DS(142)
#define H_PL_IN2 H_DS(107)
#define H_PL_IN3 H_DS(379)
#define H_PL_IN4 H_DS(277)
#define H_PL_APL (H_DS(672) + 2u * H_PL_EXC + 2u)
#define H_PL_APR (H_DS(908) + 2u * H_PL_EXC + 2u)
#define H_PL_AP2L H_DS(1800)
#define H_PL_AP2R H_DS(2656)
#define H_PL_DL1 H_DS(4453)
#define H_PL_DL2 H_DS(3720)
#define H_PL_DR1 H_DS(4217)
#define H_PL_DR2 H_DS(3163)
#define H_PLATE_BUFFERS(nm) \
    static float nm##_in1[H_PL_IN1] H_POOL, nm##_in2[H_PL_IN2] H_POOL, nm##_in3[H_PL_IN3] H_POOL, nm##_in4[H_PL_IN4] H_POOL, \
        nm##_apl[H_PL_APL] H_POOL, nm##_apr[H_PL_APR] H_POOL, nm##_ap2l[H_PL_AP2L] H_POOL, nm##_ap2r[H_PL_AP2R] H_POOL; \
    static int16_t nm##_pre[H_PL_PRE] H_POOL, nm##_dl1[H_PL_DL1] H_POOL, nm##_dl2[H_PL_DL2] H_POOL, \
        nm##_dr1[H_PL_DR1] H_POOL, nm##_dr2[H_PL_DR2] H_POOL
#define H_PLATE_INIT(p, nm) \
    h_plate_init(p, nm##_in1, nm##_in2, nm##_in3, nm##_in4, nm##_apl, nm##_apr, nm##_ap2l, nm##_ap2r, nm##_pre, \
                 nm##_dl1, nm##_dl2, nm##_dr1, nm##_dr2)
typedef struct {
    h_fdl_t in1, in2, in3, in4, apl, apr, ap2l, ap2r;
    h_dl_t pre, dl1, dl2, dr1, dr2;
    float decay, damp, bw, lfo, pre_n;
    h_lp1_t bwf, dampl, dampr;
    float fbl, fbr;
    uint32_t n;
} h_plate_t;
static inline void h_plate_init(h_plate_t *p, float *in1, float *in2, float *in3, float *in4, float *apl, float *apr,
                                float *ap2l, float *ap2r, int16_t *pre, int16_t *dl1, int16_t *dl2, int16_t *dr1,
                                int16_t *dr2)
{
    h_fdl_init(&p->in1, in1, H_PL_IN1);
    h_fdl_init(&p->in2, in2, H_PL_IN2);
    h_fdl_init(&p->in3, in3, H_PL_IN3);
    h_fdl_init(&p->in4, in4, H_PL_IN4);
    h_fdl_init(&p->apl, apl, H_PL_APL);
    h_fdl_init(&p->apr, apr, H_PL_APR);
    h_fdl_init(&p->ap2l, ap2l, H_PL_AP2L);
    h_fdl_init(&p->ap2r, ap2r, H_PL_AP2R);
    h_dl_init(&p->pre, pre, H_PL_PRE);
    h_dl_init(&p->dl1, dl1, H_PL_DL1);
    h_dl_init(&p->dl2, dl2, H_PL_DL2);
    h_dl_init(&p->dr1, dr1, H_PL_DR1);
    h_dl_init(&p->dr2, dr2, H_PL_DR2);
    p->decay = 0.7f;
    p->damp = 0.3f;
    p->bw = 0.7f;
    p->pre_n = 1.0f;
    p->fbl = p->fbr = 0.0f;
    p->lfo = 0.0f;
    p->n = 0;
    p->bwf.z = p->dampl.z = p->dampr.z = 0.0f;
}
/* decay 0..0.98 (0.5 a room, 0.85 a hall, 0.95 a cathedral, 0.98 forever); damp 0..1 (how fast the highs go);
 * bw 0..1 (the input's brightness); pre_ms up to 46 */
static inline void h_plate_set(h_plate_t *p, float decay, float damp, float bw, float pre_ms)
{
    p->decay = fm_clampf(decay, 0.0f, 0.985f);
    p->damp = fm_clampf(damp, 0.0f, 0.98f);
    p->bw = fm_clampf(bw, 0.02f, 0.999f);
    p->pre_n = fm_clampf(pre_ms * 44.1f, 1.0f, (float)(H_PL_PRE - 2u));
}
static inline void h_plate(h_plate_t *p, float in, float *ol, float *orr)
{
    float x, a, b, s, c, l, r;
    h_dl_push(&p->pre, in);
    x = h_dl_tapi(&p->pre, (uint32_t)p->pre_n);
    x = h_lp1(&p->bwf, x, p->bw);
    x = h_ap(&p->in1, H_PL_IN1 - 1u, x, 0.75f);
    x = h_ap(&p->in2, H_PL_IN2 - 1u, x, 0.75f);
    x = h_ap(&p->in3, H_PL_IN3 - 1u, x, 0.625f);
    x = h_ap(&p->in4, H_PL_IN4 - 1u, x, 0.625f);
    if ((++p->n & 7u) == 0u)
        p->lfo = h_wrap(p->lfo + 8.0f * 0.9f * H_ISR);   /* a slow wobble: no metal in the tank */
    s = h_sin(p->lfo);
    c = h_sin(p->lfo + 0.25f);
    /* left half */
    a = h_ap(&p->apl, H_DS(672) + H_PL_EXC + (uint32_t)((float)H_PL_EXC * s), x + p->decay * p->fbr, -0.7f);
    h_dl_push(&p->dl1, a);
    b = h_dl_tapi(&p->dl1, H_PL_DL1 - 1u);
    b = h_lp1(&p->dampl, b, 1.0f - p->damp) * p->decay;
    b = h_ap(&p->ap2l, H_PL_AP2L - 1u, b, 0.5f);
    h_dl_push(&p->dl2, b);
    p->fbl = h_dl_tapi(&p->dl2, H_PL_DL2 - 1u);
    /* right half */
    a = h_ap(&p->apr, H_DS(908) + H_PL_EXC + (uint32_t)((float)H_PL_EXC * c), x + p->decay * p->fbl, -0.7f);
    h_dl_push(&p->dr1, a);
    b = h_dl_tapi(&p->dr1, H_PL_DR1 - 1u);
    b = h_lp1(&p->dampr, b, 1.0f - p->damp) * p->decay;
    b = h_ap(&p->ap2r, H_PL_AP2R - 1u, b, 0.5f);
    h_dl_push(&p->dr2, b);
    p->fbr = h_dl_tapi(&p->dr2, H_PL_DR2 - 1u);
    /* the paper's output taps */
    l = h_dl_tapi(&p->dr1, H_DS(266)) + h_dl_tapi(&p->dr1, H_DS(2974)) - h_fdl_tap(&p->ap2r, H_DS(1913))
        + h_dl_tapi(&p->dr2, H_DS(1996)) - h_dl_tapi(&p->dl1, H_DS(1990)) - h_fdl_tap(&p->ap2l, H_DS(187))
        - h_dl_tapi(&p->dl2, H_DS(1066));
    r = h_dl_tapi(&p->dl1, H_DS(353)) + h_dl_tapi(&p->dl1, H_DS(3627)) - h_fdl_tap(&p->ap2l, H_DS(1228))
        + h_dl_tapi(&p->dl2, H_DS(2673)) - h_dl_tapi(&p->dr1, H_DS(2111)) - h_fdl_tap(&p->ap2r, H_DS(335))
        - h_dl_tapi(&p->dr2, H_DS(121));
    *ol = l * 0.6f;
    *orr = r * 0.6f;
}

/* -------------------------------------------------------------------------------------------- wow, tape --- */
/* wow (0.3..1.5 Hz, slow) and flutter (5..9 Hz, small) as a factor around 1, plus a random walk that never
 * settles (a motor). depth: 0 none .. 1 seasick (about 3 % at 1) */
typedef struct { float ph1, ph2, walk, walk_t, depth; h_rng_t rng; uint32_t n; } h_wow_t;
static inline float h_wow(h_wow_t *w)
{
    if ((++w->n & 255u) == 0u) {
        w->ph1 = h_wrap(w->ph1 + 256.0f * 0.7f * H_ISR);
        w->ph2 = h_wrap(w->ph2 + 256.0f * 6.3f * H_ISR);
        if ((w->n & 32767u) == 0u)
            w->walk_t = h_brand(&w->rng);
        w->walk += (w->walk_t - w->walk) * 0.004f;
    }
    return 1.0f + w->depth * (0.02f * h_sin(w->ph1) + 0.004f * h_sin(w->ph2) + 0.012f * w->walk);
}
static inline float h_sat(float x, float drive)                   /* tape: soft, a little asymmetric */
{
    x *= drive;
    return fm_tanhf(x + 0.08f * x * x) / (0.2f + 0.8f * drive);
}

/* -------------------------------------------------------------------------------------------------- echo --- */
/* a tape echo: one head, feedback through a lowpass (each repeat darker) and a touch of saturation, the read
 * point wobbling with wow; the time glides, so changing it bends the pitch as a tape machine does */
typedef struct {
    h_dl_t dl;
    float time, time_t, fb, tone, wobble;
    h_lp1_t lp, hp;
    h_wow_t wow;
    float z;
} h_echo_t;
static inline void h_echo_init(h_echo_t *e, int16_t *buf, uint32_t n, uint32_t seed)
{
    h_dl_init(&e->dl, buf, n);
    e->time = e->time_t = 0.25f * H_SR;
    e->fb = 0.45f;
    e->tone = 0.25f;
    e->wobble = 0.3f;
    e->wow.rng.s = seed | 1u;
    e->wow.depth = 0.3f;
    e->lp.z = e->hp.z = 0.0f;
    e->z = 0.0f;
}
static inline void h_echo_time(h_echo_t *e, float ms)
{
    e->time_t = fm_clampf(ms * 44.1f, 40.0f, (float)(e->dl.n - 4u));
}
static inline float h_echo(h_echo_t *e, float in)
{
    float y;
    e->time += (e->time_t - e->time) * 0.0008f;
    y = h_dl_tap(&e->dl, e->time * h_wow(&e->wow));
    y = h_lp1(&e->lp, y, e->tone);
    y = h_hp1(&e->hp, y, 0.004f);
    e->z = y;
    h_dl_push(&e->dl, h_sat(in + y * e->fb, 1.0f));
    return y;
}

/* --------------------------------------------------------------------------------------------------- beds --- */
/* a floor that is always there. type: rain (filtered noise in slow gusts with droplets), crackle (vinyl: a
 * sparse scatter of ticks with dust under them), hiss (tape), wind (a slow, dark howl), room (a mains hum's
 * shadow and a little air). Stereo from two decorrelated paths. level 0..1; density 0..1 (how many drops or ticks) */
enum { H_BED_NONE, H_BED_RAIN, H_BED_CRACKLE, H_BED_HISS, H_BED_WIND, H_BED_ROOM, H_BED_N };
static const char *const H_BED_NAME[H_BED_N] = {"None", "Rain", "Crackle", "Hiss", "Wind", "Room"};
typedef struct {
    uint8_t type;
    float level, density, gust, gust_t, drop_l, drop_r, hum;
    h_svf_t fl, fr, dl, dr;
    h_lp1_t pl, pr, wl, wr;
    h_rng_t rng;
    float pink[3];
    uint32_t n;
} h_bed_t;
static inline void h_bed_init(h_bed_t *b, uint32_t seed)
{
    int i;
    b->rng.s = seed | 1u;
    b->type = H_BED_NONE;
    b->level = 0.3f;
    b->density = 0.5f;
    b->gust = b->gust_t = 0.5f;
    b->drop_l = b->drop_r = 0.0f;
    b->hum = 0.0f;
    for (i = 0; i < 3; i++)
        b->pink[i] = 0.0f;
    h_svf_set(&b->fl, 4000.0f, 0.7f);
    h_svf_set(&b->fr, 4300.0f, 0.7f);
    h_svf_set(&b->dl, 2500.0f, 6.0f);
    h_svf_set(&b->dr, 3100.0f, 6.0f);
    b->pl.z = b->pr.z = b->wl.z = b->wr.z = 0.0f;
    b->n = 0;
}
static inline float h_pink(h_bed_t *b, float w)                  /* Kellet's economy pink */
{
    b->pink[0] = 0.99765f * b->pink[0] + w * 0.0990460f;
    b->pink[1] = 0.96300f * b->pink[1] + w * 0.2965164f;
    b->pink[2] = 0.57000f * b->pink[2] + w * 1.0526913f;
    return (b->pink[0] + b->pink[1] + b->pink[2] + w * 0.1848f) * 0.25f;
}
static inline void h_bed(h_bed_t *b, float *l, float *r)
{
    float w = h_brand(&b->rng), w2 = h_brand(&b->rng), x, y, lp, bp, hp;
    if (b->type == H_BED_NONE || b->level <= 0.0f) {
        *l = *r = 0.0f;
        return;
    }
    if ((++b->n & 4095u) == 0u) {                                  /* a gust, a lull: every ~90 ms a new target */
        b->gust_t = 0.3f + 0.7f * h_frand(&b->rng);
    }
    b->gust += (b->gust_t - b->gust) * 0.0004f;
    switch (b->type) {
    case H_BED_RAIN:
        h_svf(&b->fl, w, &lp, &bp, &hp);
        x = bp * b->gust;
        h_svf(&b->fr, w2, &lp, &bp, &hp);
        y = bp * b->gust;
        if (h_chance(&b->rng, 0.0004f * b->density))                /* a droplet: a ping through a resonance */
            b->drop_l = 0.8f;
        if (h_chance(&b->rng, 0.0004f * b->density))
            b->drop_r = 0.8f;
        h_svf(&b->dl, b->drop_l * w, &lp, &bp, &hp);
        x += bp * 0.5f;
        h_svf(&b->dr, b->drop_r * w2, &lp, &bp, &hp);
        y += bp * 0.5f;
        b->drop_l *= 0.994f;
        b->drop_r *= 0.994f;
        x *= 0.7f;
        y *= 0.7f;
        break;
    case H_BED_CRACKLE:
        x = h_lp1(&b->pl, w, 0.02f) * 0.08f;                        /* the dust under the ticks */
        y = h_lp1(&b->pr, w2, 0.02f) * 0.08f;
        if (h_chance(&b->rng, 0.00025f * b->density))
            b->drop_l = 0.6f + 0.4f * h_frand(&b->rng);
        if (h_chance(&b->rng, 0.00025f * b->density))
            b->drop_r = 0.6f + 0.4f * h_frand(&b->rng);
        h_svf(&b->dl, b->drop_l * w, &lp, &bp, &hp);
        x += hp * 0.5f + bp * 0.2f;
        h_svf(&b->dr, b->drop_r * w2, &lp, &bp, &hp);
        y += hp * 0.5f + bp * 0.2f;
        b->drop_l *= 0.93f;
        b->drop_r *= 0.93f;
        break;
    case H_BED_HISS:
        x = h_pink(b, w) * 0.35f;
        y = x * 0.7f + h_lp1(&b->pr, w2, 0.3f) * 0.1f;
        break;
    case H_BED_WIND:
        h_svf_set(&b->fl, 180.0f + 500.0f * b->gust, 2.5f);
        h_svf(&b->fl, w, &lp, &bp, &hp);
        x = bp * 1.5f * b->gust;
        h_svf_set(&b->fr, 220.0f + 420.0f * b->gust, 2.5f);
        h_svf(&b->fr, w2, &lp, &bp, &hp);
        y = bp * 1.5f * b->gust;
        break;
    default:                                                         /* room: a dark floor and a 50 Hz shadow */
        b->hum = h_wrap(b->hum + 50.0f * H_ISR);
        x = h_lp1(&b->pl, w, 0.01f) * 0.3f + h_sin(b->hum) * 0.03f + h_sin(b->hum * 3.0f) * 0.01f;
        y = h_lp1(&b->pr, w2, 0.01f) * 0.3f + h_sin(b->hum) * 0.03f;
        break;
    }
    *l = x * b->level;
    *r = y * b->level;
}

/* ------------------------------------------------------------------------------------------------ harmony --- */
/* a scale is a 12-bit pool, bit i = root + i semitones, bit 0 always set */
#define H_P(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
enum { H_SC_AEOLIAN, H_SC_DORIAN, H_SC_PHRYGIAN, H_SC_HARMMIN, H_SC_PENTMIN, H_SC_MAJOR, H_SC_LYDIAN, H_SC_WHOLE,
       H_SC_N };
static const uint16_t H_SCALE[H_SC_N] = {
    H_P(0, 2, 3, 5, 7, 8, 10), H_P(0, 2, 3, 5, 7, 9, 10), H_P(0, 1, 3, 5, 7, 8, 10), H_P(0, 2, 3, 5, 7, 8, 11),
    H_P(0, 3, 5, 7, 10, 10, 10), H_P(0, 2, 4, 5, 7, 9, 11), H_P(0, 2, 4, 6, 7, 9, 11), H_P(0, 2, 4, 6, 8, 10, 10)};
static const char *const H_SCALE_NAME[H_SC_N] = {"Aeolian", "Dorian", "Phrygian", "Harm min", "Pent min", "Major",
                                                 "Lydian", "Whole"};
static const char *const H_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
#undef H_P
static inline int h_scale_steps(uint32_t pool, int *st)         /* the pool's intervals, ascending; the count */
{
    int i, n = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            st[n++] = i;
    return n;
}
/* scale degree deg (any integer: 7 is the octave in a seven-note scale) over root (MIDI), as a MIDI note */
static inline int h_deg_note(uint32_t pool, int root, int deg)
{
    int st[12], n = h_scale_steps(pool, st), o = 0;
    while (deg < 0) {
        deg += n;
        o--;
    }
    o += deg / n;
    return root + 12 * o + st[deg % n];
}
/* the nearest scale note to MIDI note m (m itself when it is in the scale) */
static inline int h_snap(uint32_t pool, int root, int m)
{
    int d;
    for (d = 0; d < 7; d++) {
        if (pool >> (((m - d - root) % 12 + 12) % 12) & 1u)
            return m - d;
        if (pool >> (((m + d - root) % 12 + 12) % 12) & 1u)
            return m + d;
    }
    return m;
}
/* a chord on degree deg: n tones (3 or 4) in stacked thirds, as pitch classes 0..11; returns n */
static inline int h_chord(uint32_t pool, int root, int deg, int n, int *pcs)
{
    int i, st[12], m = h_scale_steps(pool, st);
    for (i = 0; i < n; i++)
        pcs[i] = ((root + st[((deg + 2 * i) % m + m) % m]) % 12 + 12) % 12;
    return n;
}
static inline int h_nearest(int from, int pc, int lo, int hi)   /* the note of class pc nearest from, in lo..hi */
{
    int base = from - ((from - pc) % 12 + 12) % 12, best = base, d, c;
    for (c = base - 12; c <= base + 12; c += 12) {
        if (c < lo || c > hi)
            continue;
        d = c > from ? c - from : from - c;
        if (best < lo || best > hi || d < (best > from ? best - from : from - best))
            best = c;
    }
    return best < lo ? best + 12 : best > hi ? best - 12 : best;
}
/* voice leading: n voices, each to a tone of the chord, moving as little as possible (greedy: the voice with the
 * closest tone first), all within lo..hi */
static inline void h_voice_lead(const int *from, const int *pcs, int n, int *to, int lo, int hi)
{
    int used = 0, done = 0, i, j, best_i = 0, best_j = 0, best_d, d, cand;
    for (i = 0; i < n; i++)
        to[i] = -1;
    while (done < n) {
        best_d = 1000;
        for (i = 0; i < n; i++) {
            if (to[i] >= 0)
                continue;
            for (j = 0; j < n; j++) {
                if (used >> j & 1)
                    continue;
                cand = h_nearest(from[i], pcs[j], lo, hi);
                d = cand > from[i] ? cand - from[i] : from[i] - cand;
                if (d < best_d) {
                    best_d = d;
                    best_i = i;
                    best_j = j;
                }
            }
        }
        to[best_i] = h_nearest(from[best_i], pcs[best_j], lo, hi);
        used |= 1 << best_j;
        done++;
    }
}

/* --------------------------------------------------------------------------------------------- parameters --- */
/* a parameter as the engine describes it and the screen shows it */
typedef struct {
    const char *name;
    int16_t lo, hi, def;
    const char *const *names;              /* value names (hi - lo + 1 of them), or 0: a number */
    const char *unit;                      /* "%", " bars", "" after a number */
} hp_t;
static const char *const H_ONOFF[2] = {"Off", "On"};

/* ------------------------------------------------------------------------------------------------- output --- */
/* n frames: float l, r (about -1..1) to the FM-1's 24-bit frames; gain from MASTER's q12 (plus the app's) */
static inline float h_gain(uint32_t gain_q12, float trim) { return (float)gain_q12 * (1.0f / 4096.0f) * 8000000.0f * trim; }
static inline void h_out(int32_t *lr, float l, float r, float g)
{
    l = fm_clampf(l, -1.0f, 1.0f) * g;
    r = fm_clampf(r, -1.0f, 1.0f) * g;
    lr[0] = (int32_t)l;
    lr[1] = (int32_t)r;
}
/* a soft limiter: a peak follower holding the loudest moment, the gain eased to keep it under 1 */
typedef struct { float env, g; } h_lim_t;
static inline void h_lim(h_lim_t *m, float *l, float *r)
{
    float p = fm_maxf(fm_fabsf(*l), fm_fabsf(*r)), t;
    m->env = p > m->env ? p : m->env + (p - m->env) * 0.0005f;
    t = m->env > 0.95f ? 0.95f / m->env : 1.0f;
    m->g += (t - m->g) * (t < m->g ? 0.05f : 0.0005f);
    *l *= m->g;
    *r *= m->g;
}
/* a tiny number to text (no printf on the device): signed, no padding; returns the end */
static inline char *h_itoa(int v, char *b)
{
    char t[12];
    int n = 0, neg = v < 0;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u && n < 11);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}
static inline char *h_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
