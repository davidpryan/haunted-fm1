/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* HALF-LIGHT on fm1-base: the project (what is saved), the panel and the screen, and the glue to the engine
 * (dsp/engine.c, its own -O2 unit). Part of fm1-base's unity build, after the platform.
 *
 *   white keys     PINS: tap to pin that note (one voice holds it and never moves), tap again to release
 *   black keys     BELLS: toggle a scale degree as a sparse high bell, timed in seconds
 *   PLAY           the weather runs / stops (stop: the voices release over Breath seconds)
 *   REC            FREEZE: the chord held, the pressure stopped; tap again to thaw.   HOME + REC   release every pin
 *   SELECT         PRESSURE: toward or away from home   ALGORITHM the climate   PRESETS the pad's voice
 *   OCT- / OCT+    the field's register
 *   HOME ENV LFO SEQ ARP FX GLO SEL EDIT   the pages; KNOB 1-4 their four values
 *   SAVE           saves the climate, the voice, the pins and bells, every setting */
#include "hui.h"
#include "engine.h"

/* ------------------------------------------------------------------------------------------------ project --- */
#define PROJ_MAGIC 0x31504C48u           /* "HLP1" */
#define PROJ_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    int16_t par[P_N];
    uint8_t pins[EN_NPINS];              /* each white key's pinned note, 0 none */
    uint32_t bells;                      /* bit d: degree d rings */
    uint8_t rsv[16];
} project_t;
static project_t proj;
static uint8_t dirty;

static void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_N; i++)
        proj.par[i] = EN_PARAMS[i].def;
}

static void project_apply(void)         /* everything to the engine */
{
    int i;
    for (i = 0; i < P_N; i++)
        en_set(i, proj.par[i]);
    en_pins_load(proj.pins, proj.bells);
}

static int project_load(void)
{
    if (hui_load(OBJ_PROJ, &proj, sizeof proj, PROJ_MAGIC, PROJ_FORMAT) == 0 && hp_ok(EN_PARAMS, P_N, proj.par))
        return 0;
    project_defaults();
    return -1;
}

static int project_save(void) { return plat_store_save(OBJ_PROJ, &proj, sizeof proj); }

/* ----------------------------------------------------------------------------------------------- the pages --- */
enum { V_HOME, V_ENV, V_LFO, V_SEQ, V_ARP, V_FX, V_GLO, V_SEL, V_EDIT, NV };
static const char *const VIEW_NAME[NV] = {"Sky", "Swell", "Beat", "Bells", "Bass", "Air", "Words", "Key", "Voice"};
static const uint8_t VIEW_BTN[NV] = {B_HOME, B_ENV, B_LFO, B_SEQ, B_ARP, B_FX, B_GLO, B_SEL, B_EDIT};
static const int8_t VIEW_KNOB[NV][4] = {
    {P_BREATH, P_VOICES, P_MOVEMENT, P_DEPTH},
    {P_SWELL, P_ATTACK, P_RELEASE, P_PAD},
    {P_BEAT, P_WALK, P_SHIMMER, P_WIDTH},
    {P_BELLEVERY, P_BELLSPREAD, P_BELLMAX, P_BELLECHO},
    {P_BASS, P_LATE, P_BASSLEVEL, P_SUB},
    {P_BED, P_AIR, P_HIPASS, P_DAMP},
    {P_SEVENTHS, P_ADD9, P_SUS, P_DENSE},
    {P_ROOT, P_SCALE, P_RANGE, P_LIGHTS},
    {P_BRIGHT, P_BODY, P_GRAIN, P_TONE},
};
/* the help cards: a button held alone for a second */
static const char *const HELP[NB][2] = {
    {"AIR", "The bed, its level, a high-pass, damping"},
    {"KEY", "Root, mode, the field's range, the lights"},
    {"SWELL", "A move's crossfade, attack, release, level"},
    {"BEAT", "Detune drift in cents, its walk, shimmer"},
    {"VOICE", "The pad: bright, body, grain, tone"},
    {"WORDS", "The chords allowed: 7ths, add9, sus, dense"},
    {"SKY", "Breath, voices, movement, depth. +REC unpins"},
    {"SAVE", "Saves the weather, the pins, every setting"},
    {"BASS", "Follows the root, holds, walks; how late"},
    {"BELLS", "Seconds between bells, spread, at once, echo"},
    {"PLAY", "Start. Stop: a release over Breath seconds"},
    {"FREEZE", "Holds the chord, stops the pressure"},
    {"LOWER", "The whole field an octave down"},
    {"HIGHER", "The whole field an octave up"},
};

#define FLASH_MS 1400u
static struct {
    uint8_t view;
    float vy[EN_NVOICES];                /* each voice's point, gliding to its pitch */
    uint8_t vgen[EN_NVOICES];
    uint32_t bells_seen, bell_t;         /* the last bell: when it rang (the flash) */
    int32_t bell_x, bell_y;
    uint32_t changes_seen, change_t;     /* the last change: when (the screen draws again) */
} ui;

/* ------------------------------------------------------------------------------------------------ values --- */
static void set_par(int p, int v)
{
    v = hp_clamp(&EN_PARAMS[p], v);
    if (v == proj.par[p])
        return;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    dirty = 1;
}
static void turn_par(int role, int p, int32_t e)
{
    if (!e || p < 0)
        return;
    set_par(p, proj.par[p] + hui_accel(role, e, EN_PARAMS[p].hi - EN_PARAMS[p].lo));
}
static void say_par(int p)
{
    char t[14];
    hp_text(&EN_PARAMS[p], proj.par[p], t);
    hui_say(EN_PARAMS[p].name, t);
}
static void note_name(int n, char *b)   /* "Eb4" */
{
    b[0] = 0;
    h_itoa(n / 12 - 1, h_cat(b, H_NOTE_NAME[(n % 12 + 12) % 12]));
}

/* -------------------------------------------------------------------------------------------------- input --- */
static const uint8_t WHITE[16] = PLAT_WHITE_KEYS;
static const uint8_t BLACK[11] = PLAT_BLACK_KEYS;
static int white_index(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE[i] == k)
            return i;
    return -1;
}
static int black_index(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK[i] == k)
            return i;
    return -1;
}

static void pin_toggle(int w, int note)   /* white key w: pinned at note, or released */
{
    char t[8];
    if (proj.pins[w]) {
        proj.pins[w] = 0;
        en_pin(w, 0);
        hui_say("RELEASED", 0);
    } else {
        proj.pins[w] = (uint8_t)note;
        en_pin(w, note);
        note_name(note, t);
        hui_say("PINNED", t);
    }
    dirty = 1;
}
static void bell_toggle(int b)
{
    proj.bells ^= 1u << b;
    en_bell(b, proj.bells >> b & 1u);
    hui_say("BELL", proj.bells >> b & 1u ? "ON" : "OFF");
    dirty = 1;
}
static void unpin_all(void)
{
    int i;
    for (i = 0; i < EN_NPINS; i++)
        if (proj.pins[i]) {
            proj.pins[i] = 0;
            en_pin(i, 0);
            dirty = 1;
        }
}

static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st == 0xF8u)
            en_clock(EN_CLK_TICK);
        else if (st == 0xFAu || st == 0xFBu)
            en_clock(EN_CLK_START);
        else if (st == 0xFCu)
            en_clock(EN_CLK_STOP);
        else if ((st & 0xF0u) == 0x90u && d2) {          /* a note: a pin on the key of its class, or its release */
            int w = (int)d1 % 16, i, had = -1;
            for (i = 0; i < EN_NPINS; i++)
                if (proj.pins[i] == d1)
                    had = i;
            if (had >= 0)
                pin_toggle(had, 0);
            else if (proj.pins[w])
                pin_toggle(w, 0);
            else
                pin_toggle(w, (int)d1);
        } else if ((st & 0xF0u) == 0xB0u) {              /* CC 20-23: the HOME knobs; 24: the pressure */
            if (d1 >= 20u && d1 <= 23u) {
                const hp_t *t = &EN_PARAMS[VIEW_KNOB[V_HOME][d1 - 20u]];
                set_par(VIEW_KNOB[V_HOME][d1 - 20u], t->lo + (int)((uint32_t)(t->hi - t->lo) * d2 / 127u));
            } else if (d1 == 24u) {
                set_par(P_PRESSURE, (int)d2 * 100 / 127 - 50);
            }
        }
    }
}

static void input(void)
{
    uint32_t i;
    int32_t e;
    hui_poll();
    /* the keys: white pins, black bells */
    for (i = 0; i < NKEYS; i++) {
        uint32_t m = 1u << i;
        if (hui.kdown & m) {
            int w = white_index((int)i), b = black_index((int)i);
            if (w >= 0)
                pin_toggle(w, 53 + (int)i + 12 * proj.par[P_OCTAVE]);
            else if (b >= 0)
                bell_toggle(b);
        }
    }
    /* transport and holds */
    if (hui.pressed >> B_PLAY & 1u) {
        en_play(!en_playing);
        hui_say(en_playing ? "STOPPING" : "PLAYING", en_playing ? "OVER BREATH" : 0);
    }
    if (hui_combo(B_HOME, B_REC)) {
        unpin_all();
        hui_say("RELEASED", "EVERY PIN");
    }
    if (hui.released >> B_REC & 1u) {
        if (!en_playing) {
            hui_warn("NOT PLAYING", "PLAY FIRST");
        } else {
            en_freeze(!en_frozen);
            hui_say(en_frozen ? "THAWED" : "FROZEN", en_frozen ? 0 : "THE CHORD HELD");
        }
    }
    if (hui.released >> B_SAVE & 1u) {
        if (project_save() == 0) {
            dirty = 0;
            hui_say("SAVED", 0);
        } else {
            hui_warn("NOT SAVED", "FLASH ERROR");
        }
    }
    if (hui.pressed >> B_OCTDN & 1u) {
        set_par(P_OCTAVE, proj.par[P_OCTAVE] - 1);
        say_par(P_OCTAVE);
    }
    if (hui.pressed >> B_OCTUP & 1u) {
        set_par(P_OCTAVE, proj.par[P_OCTAVE] + 1);
        say_par(P_OCTAVE);
    }
    for (i = 0; i < NV; i++)                      /* a page's button, released: its page */
        if (hui.released >> VIEW_BTN[i] & 1u) {
            ui.view = (uint8_t)i;
            hui_say(VIEW_NAME[i], 0);
        }
    /* the encoders */
    if ((e = hui.enc[EN_SELECT]) != 0) {
        set_par(P_PRESSURE, proj.par[P_PRESSURE] + hui_accel(EN_SELECT, e, 100));
        say_par(P_PRESSURE);
    }
    if ((e = hui.enc[EN_ALGO]) != 0) {
        set_par(P_CLIMATE, proj.par[P_CLIMATE] + (e > 0 ? 1 : -1));
        say_par(P_CLIMATE);
    }
    if ((e = hui.enc[EN_PRESET]) != 0) {
        set_par(P_VOICE, proj.par[P_VOICE] + (e > 0 ? 1 : -1));
        say_par(P_VOICE);
    }
    for (i = 0; i < 4u; i++)
        if ((e = hui.enc[EN_K1 + i]) != 0) {
            turn_par(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            hui_touch((int)i);
        }
    midi_in();
}

/* -------------------------------------------------------------------------------------------------- lights --- */
static void lights(void)
{
    uint32_t btn = 0, bright = 0, dim = 0, i;
    int lit = proj.par[P_LIGHTS];
    if (en_playing)
        btn |= 1u << B_PLAY;
    if (en_frozen)
        btn |= 1u << B_REC;
    if (dirty)
        btn |= 1u << B_SAVE;
    btn |= 1u << VIEW_BTN[ui.view];
    for (i = 0; i < 16u; i++)                      /* the pins, bright; the bells, dim, bright as one rings */
        if (proj.pins[i])
            bright |= 1u << WHITE[i];
    for (i = 0; i < 11u; i++)
        if (proj.bells >> i & 1u)
            dim |= 1u << BLACK[i];
    if (hui.now - ui.bell_t < 200u && en_bell_deg < 11u)
        bright |= 1u << BLACK[en_bell_deg];
    if (lit == 2 && en_playing)                    /* the voices' notes on the keys */
        for (i = 0; i < EN_NVOICES; i++)
            if (en_vnote[i] && !en_vpin[i]) {
                int k = (int)en_vnote[i] - 53 - 12 * proj.par[P_OCTAVE];
                while (k < 0)
                    k += 12;
                while (k >= NKEYS)
                    k -= 12;
                dim |= 1u << k;
            }
    plat_lights(btn, bright, 0, dim, lit ? 1 : 0);
    plat_play_red(en_frozen);
}

/* -------------------------------------------------------------------------------------------------- screen --- */
static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

#define K_DEEP RGB(12, 20, 66)
#define K_BRUISE RGB(96, 38, 112)
#define K_BREAK RGB(236, 226, 242)
#define K_GLOW RGB(190, 120, 150)
#define K_GROUND RGB(12, 11, 16)
#define K_FROST RGB(214, 226, 240)
#define K_STAR RGB(255, 236, 190)
#define K_BELL RGB(255, 250, 230)
#define K_BASS RGB(120, 110, 190)
#define HORIZON 100
static const int16_t VX[EN_NVOICES] = {36, 174, 72, 192, 138, 54, 156, 102};

static uint16_t sky_colour(float p)              /* deep blue -> bruised purple -> white at the break */
{
    if (p < 0.5f)
        return mix(K_DEEP, K_BRUISE, (int)(p * 32.0f));
    p = (p - 0.5f) * 2.0f;
    return mix(K_BRUISE, K_BREAK, (int)(p * p * 16.0f));
}
static int32_t note_y(int n)                     /* a pitch's height in the sky */
{
    int32_t lo = en_lo, hi = en_hi, y;
    if (hi <= lo)
        return HORIZON / 2;
    y = HORIZON - 6 - (n - lo) * (HORIZON - 14) / (hi - lo);
    return y < 6 ? 6 : y > HORIZON - 4 ? HORIZON - 4 : y;
}
static void chord_name(char *b)                  /* "Fm7", "Ab add9", "C 4ths" */
{
    int st[12], nd = h_scale_steps((uint32_t)H_SCALE[proj.par[P_SCALE]], st), q = en_qual, deg = en_deg;
    int third = (st[(deg + 2) % nd] - st[deg % nd] + 12) % 12;
    b[0] = 0;
    h_cat(b, H_NOTE_NAME[en_root_pc % 12]);
    if (q != Q_SUS && q != Q_QUARTAL && q != Q_CLUSTER && third == 3)
        h_cat(b, "m");
    if (q != Q_TRIAD) {
        if (q != Q_SEVENTH)
            h_cat(b, " ");
        h_cat(b, EN_QUAL_SUFFIX[q]);
    }
}

static void draw_main(void)
{
    char t[24], n[12];
    int32_t y, i;
    float p = en_pressure;
    uint16_t top = sky_colour(p), hor = mix(top, K_GLOW, 6), ground = K_GROUND, line = mix(top, K_BREAK, 5);
    int frozen = en_frozen;
    if (frozen) {
        top = mix(top, K_FROST, 7);
        hor = mix(hor, K_FROST, 7);
        ground = mix(ground, K_FROST, 3);
    }
    cv_begin(240, HUI_MAIN_H, TH.bg);
    for (y = 0; y < HORIZON; y++)                /* the sky, lighter toward the horizon */
        cv_rect(0, y, 240, 1, mix(top, hor, (int)(y * 16 / HORIZON)));
    for (y = HORIZON; y < HUI_MAIN_H; y++)
        cv_rect(0, y, 240, 1, mix(ground, TH.bg, (int)((y - HORIZON) * 16 / (HUI_MAIN_H - HORIZON))));
    cv_rect(0, HORIZON, 240, 1, line);
    /* the bass: a low glow on the horizon */
    if (en_bass_note) {
        int32_t bx = 20 + (int32_t)(en_bass_note % 12) * 18;
        cv_rect(bx - 8, HORIZON + 1, 17, 2, K_BASS);
    }
    /* the voices: points at their pitch, drifting as they move; the pins: anchored stars */
    for (i = 0; i < EN_NVOICES; i++) {
        int note = en_vnote[i], pin = en_vpin[i];
        int32_t ty, x, py;
        if (!note) {
            ui.vy[i] = -1.0f;
            continue;
        }
        ty = note_y(note);
        if (ui.vy[i] < 0.0f || ui.vgen[i] != en_vgen[i]) {
            if (ui.vy[i] < 0.0f)
                ui.vy[i] = (float)ty;
            ui.vgen[i] = en_vgen[i];
        }
        ui.vy[i] += ((float)ty - ui.vy[i]) * 0.12f;
        py = (int32_t)(ui.vy[i] + 0.5f);
        if (pin) {
            x = 14 + (int32_t)(pin - 1) * 14;
            cv_rect(x - 4, py, 9, 1, K_STAR);
            cv_rect(x, py - 4, 1, 9, K_STAR);
            cv_pset(x - 2, py - 2, K_STAR);
            cv_pset(x + 2, py - 2, K_STAR);
            cv_pset(x - 2, py + 2, K_STAR);
            cv_pset(x + 2, py + 2, K_STAR);
            cv_rect(x, py + 5, 1, HORIZON - py - 5, mix(top, K_STAR, 3));   /* anchored to the ground */
        } else {
            x = VX[i];
            hui_disc(x, py, 3, mix(top, TH.text, 5));
            hui_disc(x, py, 2, TH.text);
        }
    }
    /* a bell: a ring spreading for a moment */
    if (hui.now - ui.bell_t < FLASH_MS) {
        int32_t age = (int32_t)(hui.now - ui.bell_t), r = 2 + age * 14 / (int32_t)FLASH_MS;
        hui_ring(ui.bell_x, ui.bell_y, r, mix(K_BELL, top, age * 16 / (int32_t)FLASH_MS));
        if (age < 200)
            hui_disc(ui.bell_x, ui.bell_y, 2, K_BELL);
    }
    if (frozen) {                                /* the frost over the sky */
        for (y = 1; y < HORIZON; y += 2)
            for (i = (y / 2) % 3; i < 240; i += 3)
                cv_pset(i, y, K_FROST);
        hui_text_c(120, 44, &FONT_B, "FROZEN", TH.text);
    }
    /* the line under the horizon: the chord, the next change, the pressure */
    chord_name(t);
    cv_text(8, 118, &FONT_S, t, TH.text);
    if (!en_playing) {
        hui_text_c(120, 120, &FONT_XS, "PLAY: THE WEATHER STARTS", TH.dim);
    } else {
        t[0] = 0;
        if (frozen) {
            h_cat(t, "FROZEN");
        } else {
            h_cat(t, en_rest ? "REST " : "IN ");
            h_itoa((int)(en_next_s + 0.99f), n);
            h_cat(t, n);
            h_cat(t, " s");
        }
        hui_text_c(120, 120, &FONT_XS, t, en_rest ? TH.acc : TH.dim);
    }
    t[0] = 0;
    h_cat(t, "P ");
    h_itoa((int)(p * 100.0f + 0.5f), n);
    h_cat(t, n);
    hui_text_r(232, 120, &FONT_XS, t, p > 0.75f ? TH.hi : TH.text);
    /* the breath, a thin line filling at the bottom */
    {
        float f = en_breath_s > 0.0f && !en_rest ? 1.0f - en_next_s / en_breath_s : 0.0f;
        hui_bar(8, 139, 224, 2, en_playing ? f : 0.0f, TH.line, TH.acc);
    }
    cv_blit(0, HUI_MAIN_Y);
}

static void draw(void)
{
    const char *names[4], *vals[4];
    static char txt[4][14];
    static char key[16];
    float v[4];
    uint8_t bip[4];
    int i;
    /* the header: the climate (its colour the sky's), the voice; the key, or EXT */
    key[0] = 0;
    if (en_ext) {
        h_cat(key, "EXT");
    } else {
        h_cat(key, H_NOTE_NAME[proj.par[P_ROOT]]);
        h_cat(key, " ");
        h_cat(key, H_SCALE_NAME[proj.par[P_SCALE]]);
    }
    hui_header(EN_PARAMS[P_CLIMATE].names[proj.par[P_CLIMATE]], mix(RGB(96, 100, 190), K_BREAK, (int)(en_pressure * 16.0f)),
               EN_PARAMS[P_VOICE].names[proj.par[P_VOICE]], key, dirty);
    if (en_bells_fired != ui.bells_seen) {       /* a bell rang: where it flashes */
        ui.bells_seen = en_bells_fired;
        ui.bell_t = hui.now;
        ui.bell_x = 22 + (int32_t)(en_bell_deg % 11) * 20;
        ui.bell_y = note_y(en_bell_note) - 4;
        if (ui.bell_y < 8)
            ui.bell_y = 8;
    }
    if (hui.help)
        hui_help_card();
    else if (hui_main_due(50u, 0))
        draw_main();
    for (i = 0; i < 4; i++) {
        int p = VIEW_KNOB[ui.view][i];
        names[i] = EN_PARAMS[p].name;
        hp_text(&EN_PARAMS[p], proj.par[p], txt[i]);
        vals[i] = txt[i];
        v[i] = hp_norm(&EN_PARAMS[p], proj.par[p]);
        bip[i] = (uint8_t)hp_bipolar(&EN_PARAMS[p]);
    }
    hui_knobs(names, v, vals, bip, 0);
}

/* a screen reader that has just started listening (or asks) hears where things stand */
static void speak(void)
{
    if (ann_on() && ann_where()) {
        char t[120], n[12];
        t[0] = 0;
        h_cat(t, APP_NAME ". ");
        h_cat(t, EN_PARAMS[P_CLIMATE].names[proj.par[P_CLIMATE]]);
        h_cat(t, ", ");
        h_cat(t, EN_PARAMS[P_VOICE].names[proj.par[P_VOICE]]);
        h_cat(t, ". PRESSURE ");
        h_itoa((int)(en_pressure * 100.0f + 0.5f), n);
        h_cat(t, n);
        h_cat(t, ". CHORD ");
        chord_name(n);
        h_cat(t, n);
        h_cat(t, ". PAGE ");
        h_cat(t, VIEW_NAME[ui.view]);
        h_cat(t, en_frozen ? ". FROZEN" : en_playing ? ". PLAYING" : ". STOPPED");
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ------------------------------------------------------------------------------------------------ contract --- */
void app_init(void)
{
    en_init();
    project_load();
    project_apply();
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12); }
void app_panic(void) { en_panic(); }
void app_ui_init(void)
{
    int i;
    TH.bg = RGB(14, 13, 22);
    TH.panel = RGB(34, 32, 52);
    TH.line = RGB(52, 50, 76);
    TH.dim = RGB(132, 128, 160);
    TH.text = RGB(232, 230, 244);
    TH.acc = RGB(170, 140, 220);
    TH.hi = RGB(246, 200, 150);
    TH.warn = RGB(226, 96, 70);
    TH.part[0] = K_DEEP;
    TH.part[1] = K_BRUISE;
    TH.part[2] = K_STAR;
    TH.part[3] = K_BASS;
    hui_init(HELP);
    memset(&ui, 0, sizeof ui);
    for (i = 0; i < EN_NVOICES; i++)
        ui.vy[i] = -1.0f;
    ui.bell_t = (uint32_t)-(int32_t)FLASH_MS - 1u;
    lcd_fill(0, 0, 240, 240, TH.bg);
}
void app_frame(void)
{
    input();
    speak();
    draw();
    lights();
}
void app_input_only(void)
{
    input();
    lights();
}
void app_say(const char *a, const char *b) { hui_say(a, b); }

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    int i, k = 0;
    if (!strcmp(what, "playing"))
        *got = en_playing;
    else if (!strcmp(what, "frozen"))
        *got = en_frozen;
    else if (!strcmp(what, "changes"))
        *got = (int)en_changes;
    else if (!strcmp(what, "breaks"))
        *got = (int)en_breaks;
    else if (!strcmp(what, "root"))
        *got = en_root_pc;
    else if (!strcmp(what, "deg"))
        *got = en_deg;
    else if (!strcmp(what, "qual"))
        *got = en_qual;
    else if (!strcmp(what, "pressure"))
        *got = (int)(en_pressure * 100.0f + 0.5f);
    else if (!strcmp(what, "nvoices"))
        *got = en_nvoices;
    else if (!strcmp(what, "pins")) {
        for (i = 0; i < EN_NPINS; i++)
            k += en_pins[i] != 0;
        *got = k;
    } else if (!strcmp(what, "bells"))
        *got = (int)en_bells;
    else if (!strcmp(what, "bellsfired"))
        *got = (int)en_bells_fired;
    else if (!strcmp(what, "rest"))
        *got = en_rest;
    else if (!strcmp(what, "ext"))
        *got = en_ext;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "dirty"))
        *got = dirty;
    else if (!strncmp(what, "par", 3))
        *got = proj.par[atoi(what + 3)];
    else
        return 0;
    return 1;
}
#endif
