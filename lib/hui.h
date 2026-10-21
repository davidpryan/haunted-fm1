/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan
 *
 * hui.h: the panel and screen helpers the five haunted firmwares share. Included in a firmware's app.c, which is
 * part of fm1-base's unity build, after the platform: plat.h, gfx.c (cv_*, the fonts, RGB) and announce.c (the
 * screen reader) are in scope.
 *
 *   input     hui_poll once a frame (and between frames): the buttons' edges (pressed, released), the keys', each
 *             encoder's detents; holds (hui_hold), combos (hui_combo), a help card for a button held alone for a
 *             second (hui.help, from the firmware's table), knob acceleration (hui_accel)
 *   messages  hui_say and hui_warn: a line in the header for a moment, and to a screen reader
 *   drawing   the three bands of every screen: the header (28 px), the main picture (144), the four knobs (68);
 *             rounded boxes, discs, bars, arcs, centred and right-aligned text
 *   values    hp_t: a parameter's name, range, default, value names; its text and its 0..1 position
 */
#pragma once
#include "hdsp.h"

#define HUI_HDR_H 28
#define HUI_MAIN_Y 28
#define HUI_MAIN_H 144
#define HUI_KNB_Y 172
#define HUI_KNB_H 68
#define HUI_HELP_MS 900u

/* the screen's colours: a firmware fills this in app_ui_init (hui_theme) */
typedef struct {
    uint16_t bg, panel, line, dim, text, acc, hi, warn;
    uint16_t part[4];                      /* the four parts' colours */
} hui_theme_t;
static hui_theme_t TH;

typedef struct {
    uint32_t btn, keys;                    /* held now */
    uint32_t pressed, released;            /* this poll's edges (released: those whose press was not used) */
    uint32_t kdown, kup;                   /* the keys' */
    uint32_t down_t[NB];
    uint32_t used;                         /* a button whose release has nothing left to do (a hold acted, a combo) */
    uint32_t helped;                       /* a button whose help card showed: its release does nothing, a hold may still act */
    int32_t enc[NE];                       /* this poll's detents, raw */
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];
    uint32_t turn_t, act_t, now;
    uint8_t help;                          /* a help card is up: the button + 1 */
    const char *const (*help_tab)[2];      /* NB entries: a title and a line (a null title: no card) */
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                        /* the knob turned last (0..3), bright for a moment */
    uint32_t touch_until;
    uint32_t sig[4];                       /* each band's signature: drawn again when it changes */
    uint32_t main_t;
} hui_t;
static hui_t hui;

static void hui_init(const char *const (*help_tab)[2])
{
    memset(&hui, 0, sizeof hui);
    hui.touched = -1;
    hui.help_tab = help_tab;
    hui.btn = plat_buttons();              /* a button held at boot is not a press */
    hui.keys = plat_keys();
}

static void hui_poll(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i, m, turned = 0, now = plat_ms();
    hui.now = now;
    ch = btn ^ hui.btn;
    hui.pressed = ch & btn;
    hui.released = 0;
    for (i = 0; i < NB; i++) {
        m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            hui.used &= ~m;
            hui.helped &= ~m;
            hui.down_t[i] = now;
        } else if (!((hui.used | hui.helped) & m)) {
            hui.released |= m;
        }
    }
    hui.btn = btn;
    ch = keys ^ hui.keys;
    hui.kdown = ch & keys;
    hui.kup = ch & hui.keys;
    hui.keys = keys;
    for (i = 0; i < NE; i++) {
        hui.enc[i] = plat_enc((int)i);
        if (hui.enc[i])
            turned = 1;
    }
    if (turned)
        hui.turn_t = now;
    if (hui.pressed || hui.kdown || turned)
        hui.act_t = now;
    /* help cards: one button held alone for a while, nothing else touched meanwhile */
    if (hui.help) {
        m = 1u << (hui.help - 1u);
        if (btn != m || keys || (int32_t)(hui.turn_t - hui.down_t[hui.help - 1u]) > 0)
            hui.help = 0;
    } else if (btn && !(btn & (btn - 1u)) && !keys && hui.help_tab) {
        for (i = 0; i < NB; i++)
            if (btn == 1u << i && !((hui.used | hui.helped) & btn) && hui.help_tab[i][0] &&
                now - hui.down_t[i] >= HUI_HELP_MS && (int32_t)(hui.turn_t - hui.down_t[i]) <= 0) {
                hui.help = (uint8_t)(i + 1u);
                hui.helped |= btn;
            }
    }
}

/* once: button b held for ms (its release then does nothing) */
static int hui_hold(int b, uint32_t ms)
{
    uint32_t m = 1u << b;
    if ((hui.btn & m) && !(hui.used & m) && hui.now - hui.down_t[b] >= ms) {
        hui.used |= m;
        hui.help = 0;
        return 1;
    }
    return 0;
}
static int hui_held(int b) { return (hui.btn >> b) & 1u; }
static void hui_use(int b) { hui.used |= 1u << b; hui.help = 0; }
/* hold held, tap tapped: 1 on the tap (neither button's release then does anything) */
static int hui_combo(int hold, int tap)
{
    if (hui_held(hold) && (hui.pressed >> tap & 1u)) {
        hui_use(hold);
        hui_use(tap);
        return 1;
    }
    return 0;
}
/* a turn while a button is held: that button's release does nothing */
static int32_t hui_enc_with(int role, int b)
{
    if (hui.enc[role] && hui_held(b))
        hui_use(b);
    return hui.enc[role];
}

/* knob acceleration by speed (X0X's idea): a few clicks are one step each however quick; a longer quick turn speeds
 * up, so half a turn sweeps 0..100. Small ranges never accelerate. */
static int32_t hui_accel(int role, int32_t s, int range)
{
    uint32_t now = hui.now, dt = now - hui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    hui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1u)
        dt /= a;
    hui.enc_run[role] = (uint8_t)(dt < 60u ? (hui.enc_run[role] + a > 255u ? 255u : hui.enc_run[role] + a) : a);
    if (range <= 24 || hui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}
static void hui_touch(int k)
{
    hui.touched = (int8_t)k;
    hui.touch_until = hui.now + 1200u;
}

/* --------------------------------------------------------------------------------------------- messages --- */
static void hui_msg(const char *a, const char *b, uint32_t flags)
{
    uint32_t i, k = 0;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        hui.msg[0][i] = a[i];
    hui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        hui.msg[1][i] = b[i];
    hui.msg[1][i] = 0;
    hui.msg_until = plat_ms() + 1400u;
    if (ann_on()) {
        for (i = 0; i < 23u && a && a[i]; i++)
            t[k++] = a[i];
        if (b && b[0])
            t[k++] = ' ';
        for (i = 0; i < 23u && b && b[i]; i++)
            t[k++] = b[i];
        t[k] = 0;
        ann_say(t, 0, flags);
    }
}
static void hui_say(const char *a, const char *b) { hui_msg(a, b, ANN_NOW); }
static void hui_warn(const char *a, const char *b) { hui_msg(a, b, ANN_NOW | ANN_KEEP); }
static int hui_msg_up(void) { return plat_ms() < hui.msg_until; }

/* ---------------------------------------------------------------------------------------------- drawing --- */
static void hui_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[5][4] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}};
    int32_t j;
    if (r > 4)
        r = 4;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}
static void hui_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    for (j = -r; j <= r; j++) {
        int32_t w = (int32_t)fm_sqrtf((float)(r * r - j * j) + 0.5f);
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}
static void hui_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a thin circle */
{
    int32_t i, n = r * 6 + 8;
    for (i = 0; i < n; i++) {
        float a = (float)i / (float)n;
        cv_pset(cx + (int32_t)(h_sin(a + 0.25f) * (float)r + (h_sin(a + 0.25f) < 0.0f ? -0.5f : 0.5f)),
                cy + (int32_t)(h_sin(a) * (float)r + (h_sin(a) < 0.0f ? -0.5f : 0.5f)), c);
    }
}
static void hui_text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void hui_text_r(int32_t xr, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(xr - text_w(f, s), y, f, s, c);
}
static void hui_bar(int32_t x, int32_t y, int32_t w, int32_t h, float v, uint16_t track, uint16_t fill)
{
    int32_t k = (int32_t)(fm_clampf(v, 0.0f, 1.0f) * (float)w + 0.5f);
    cv_rect(x, y, w, h, track);
    if (k > 0)
        cv_rect(x, y, k, h, fill);
}
static void hui_vbar(int32_t x, int32_t y, int32_t w, int32_t h, float v, uint16_t track, uint16_t fill)
{
    int32_t k = (int32_t)(fm_clampf(v, 0.0f, 1.0f) * (float)h + 0.5f);
    cv_rect(x, y, w, h, track);
    if (k > 0)
        cv_rect(x, y + h - k, w, k, fill);
}
/* a knob's arc: 270 degrees from 7:30 round to 4:30, 3 px thick; the value filled from the start, or from the
 * top when bipolar; a dot at the value */
static void hui_arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, int bipolar)
{
    int32_t i, n = r * 5 + 24, t;
    float lo, hi;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (t = 0; t < 3; t++) {
        float rr = (float)r - (float)t;
        for (i = 0; i <= n; i++) {
            float p = (float)i / (float)n, a = 0.375f + 0.75f * p;   /* turns, clockwise from 7:30 */
            float x = (float)cx + 0.5f + rr * h_sin(a + 0.25f), y = (float)cy + 0.5f + rr * h_sin(a);
            cv_pset((int32_t)fm_floorf(x), (int32_t)fm_floorf(y), p >= lo && p <= hi ? fill : track);
        }
    }
    {
        float a = 0.375f + 0.75f * v, rr = (float)r - 1.0f;
        hui_disc((int32_t)fm_floorf((float)cx + 0.5f + rr * h_sin(a + 0.25f)),
                 (int32_t)fm_floorf((float)cy + 0.5f + rr * h_sin(a)), 3, fill);
    }
}

/* the header: a tag (the piece, the mode) in a box, a line beside it, a value on the right (tempo, a count), a
 * dot while there is something unsaved; a message takes its place for a moment. Drawn when its signature changes. */
static void hui_header(const char *tag, uint16_t tagc, const char *l2, const char *r1, int dirty)
{
    uint32_t h = 2166136261u;
    const char *s;
    int msg = hui_msg_up();
    for (s = tag; s && *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    for (s = l2; s && *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    for (s = r1; s && *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    h = (h ^ (uint32_t)dirty ^ (uint32_t)msg << 1 ^ tagc << 4) * 16777619u;
    if (msg)
        h = (h ^ hui.msg_until) * 16777619u;
    if (h == hui.sig[0])
        return;
    hui.sig[0] = h;
    cv_begin(240, HUI_HDR_H, TH.bg);
    if (msg) {
        int32_t x = cv_text(8, 6, &FONT_B, hui.msg[0], TH.text);
        cv_text(x + 8, 6, &FONT_B, hui.msg[1], TH.acc);
    } else {
        int32_t w = text_w(&FONT_B, tag) + 12, x = 232;
        hui_round(4, 4, w, 20, 4, tagc);
        cv_text(10, 6, &FONT_B, tag, TH.bg);
        if (l2)
            cv_text(w + 10, 8, &FONT_XS, l2, TH.dim);
        if (dirty) {
            hui_disc(x - 2, 14, 2, TH.dim);
            x -= 10;
        }
        if (r1)
            hui_text_r(x, 6, &FONT_S, r1, TH.text);
    }
    cv_rect(0, HUI_HDR_H - 1, 240, 1, TH.line);
    cv_blit(0, 0);
}

/* the knob band: four cards, each a name, a value and an arc; the knob turned last is bright for a moment */
static void hui_knobs(const char *const name[4], const float v[4], const char *const val[4], const uint8_t *bipolar,
                      const uint16_t *colour)
{
    uint32_t h = 2166136261u, i;
    int touched = hui.touched >= 0 && hui.now < hui.touch_until ? hui.touched : -1;
    for (i = 0; i < 4u; i++) {
        const char *s;
        for (s = name[i]; s && *s; s++)
            h = (h ^ (uint8_t)*s) * 16777619u;
        for (s = val[i]; s && *s; s++)
            h = (h ^ (uint8_t)*s) * 16777619u;
        h = (h ^ (uint32_t)(v[i] * 255.0f)) * 16777619u;
    }
    h = (h ^ (uint32_t)(touched + 1)) * 16777619u;
    if (h == hui.sig[2])
        return;
    hui.sig[2] = h;
    cv_begin(240, HUI_KNB_H, TH.bg);
    cv_rect(0, 0, 240, 1, TH.line);
    for (i = 0; i < 4u; i++) {
        int32_t x = 2 + (int32_t)i * 59, lit = (int)i == touched;
        uint16_t c = colour ? colour[i] : TH.hi;
        hui_round(x, 3, 57, 64, 3, lit ? TH.panel : TH.bg);
        hui_text_c(x + 28, 4, &FONT_XS, name[i], lit ? TH.text : TH.dim);
        hui_arc(x + 28, 33, 10, v[i], TH.line, lit ? TH.text : c, bipolar ? bipolar[i] : 0);
        hui_text_c(x + 28, 49, &FONT_XS, val[i], TH.text);   /* the card's whole width: "Phrygian" fits */
    }
    cv_blit(0, HUI_KNB_Y);
}

/* a help card over the main band: the button's name, what it does */
static void hui_help_card(void)
{
    const char *const *t = hui.help_tab[hui.help - 1u];
    cv_begin(240, HUI_MAIN_H, TH.bg);
    hui_round(16, 30, 208, 84, 4, TH.panel);
    hui_round(16, 30, 208, 4, 2, TH.acc);
    hui_text_c(120, 44, &FONT_B, t[0], TH.text);
    hui_text_c(120, 70, &FONT_XS, t[1], TH.dim);
    cv_blit(0, HUI_MAIN_Y);
    hui.sig[1] = 0;                        /* the picture under it: drawn again when the card goes */
}

/* the main band is the firmware's picture; it is drawn when hui_main_due says so (at most every ms, or when
 * force), and the help card wins while it is up: a firmware's draw() is
 *   if (hui.help) hui_help_card(); else if (hui_main_due(50, changed)) { cv_begin(240, HUI_MAIN_H, TH.bg); ... cv_blit(0, HUI_MAIN_Y); } */
static int hui_main_due(uint32_t ms, int force)
{
    if (!force && hui.now - hui.main_t < ms)
        return 0;
    hui.main_t = hui.now;
    return 1;
}

/* ------------------------------------------------------------------------------------------- parameters --- */
/* hp_t, a parameter's description, is in hdsp.h: the engine's table is the UI's */
static int hp_ok(const hp_t *t, int n, const int16_t *par)
{
    int i;
    for (i = 0; i < n; i++)
        if (par[i] < t[i].lo || par[i] > t[i].hi)
            return 0;
    return 1;
}
static void hp_text(const hp_t *t, int v, char *b)
{
    if (t->names) {
        const char *s = t->names[v - t->lo];
        int k = 0;
        while (s[k] && k < 11) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (t->lo < 0 && v > 0)
        *b++ = '+';
    b = h_itoa(v, b);
    if (t->unit)
        h_cat(b, t->unit);
}
static float hp_norm(const hp_t *t, int v)
{
    return t->hi > t->lo ? (float)(v - t->lo) / (float)(t->hi - t->lo) : 0.0f;
}
static int hp_bipolar(const hp_t *t) { return t->lo < 0 && t->hi > 0; }
static int hp_clamp(const hp_t *t, int v) { return v < t->lo ? t->lo : v > t->hi ? t->hi : v; }

/* a saved object with a magic and a format at its head: 0 loaded (the whole of it), else defaults are wanted */
static int hui_load(uint32_t obj, void *dst, uint32_t n, uint32_t magic, uint32_t format)
{
    const uint32_t *h = dst;
    int got = plat_store_load(obj, dst, n);
    return got == (int)n && h[0] == magic && h[1] == format ? 0 : -1;
}
