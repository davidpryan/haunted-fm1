/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* VOICEMAIL on fm1-base: the project (what is saved), the panel and the screen, and the glue to the engine
 * (dsp/engine.c, its own -O2 unit). Part of fm1-base's unity build, after the platform.
 *
 *   white keys     the ghost sings a phrase at that pitch, from the next 1/8; writing (REC), they go into the melody
 *   black keys     held with a white key: the vowel (a e i o u, three breathy, three whispered)
 *   PLAY           the conversation runs / stops       REC   write: the keys write the melody; again to finish
 *   REC held       KEEP: the phrase being sung becomes the melody      HOME + REC   clear the melody
 *   SELECT         PITCH, -24..+12 semitones, the throat held    ALGORITHM who sings    PRESETS the phrase book
 *   OCT- / OCT+    the pad's register
 *   HOME ENV LFO SEQ ARP FX GLO SEL EDIT   the pages; KNOB 1-4 their four values
 *   SAVE           saves the melody and every setting */
#include "hui.h"
#include "engine.h"

/* ------------------------------------------------------------------------------------------------ project --- */
#define PROJ_MAGIC 0x31504D56u           /* "VMP1" */
#define PROJ_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    int16_t par[P_N];
    uint8_t nmelody, melody_steps, rsv0[2];
    uint32_t seed;
    en_note_t melody[EN_MAX_MELODY];
    uint8_t rsv[16];
} project_t;
static project_t proj;
static uint8_t dirty;
static uint32_t melody_seen;             /* en_melody_ver as last copied into the project */

static void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_N; i++)
        proj.par[i] = EN_PARAMS[i].def;
    proj.seed = 0x5EED5u;
}

static void project_apply(void)         /* everything to the engine */
{
    int i;
    for (i = 0; i < P_N; i++)
        en_set(i, proj.par[i]);
    en_melody_load(proj.melody, proj.nmelody, proj.melody_steps, proj.seed);
}

static int project_load(void)
{
    if (hui_load(OBJ_PROJ, &proj, sizeof proj, PROJ_MAGIC, PROJ_FORMAT) == 0 && hp_ok(EN_PARAMS, P_N, proj.par) &&
        proj.nmelody <= EN_MAX_MELODY && proj.melody_steps <= EN_MAX_STEPS)
        return 0;
    project_defaults();
    return -1;
}

static void project_take_melody(void)   /* the engine's melody into the project (not while it is being written) */
{
    int i;
    if (en_writing)
        return;
    for (i = 0; i < en_nmelody; i++)
        proj.melody[i] = en_melody[i];
    proj.nmelody = en_nmelody;
    proj.melody_steps = en_melody_steps;
    proj.seed = en_seed_now;
    melody_seen = en_melody_ver;
}

static int project_save(void)
{
    project_take_melody();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

/* ----------------------------------------------------------------------------------------------- the pages --- */
enum { V_HOME, V_ENV, V_LFO, V_SEQ, V_ARP, V_FX, V_GLO, V_SEL, V_EDIT, NV };
static const char *const VIEW_NAME[NV] = {"Ghost", "Breath", "Tape", "Place", "Pad", "Room", "Answer", "Key", "Throat"};
static const uint8_t VIEW_BTN[NV] = {B_HOME, B_ENV, B_LFO, B_SEQ, B_ARP, B_FX, B_GLO, B_SEL, B_EDIT};
static const int8_t VIEW_KNOB[NV][4] = {
    {P_FRAGMENT, P_STUTTER, P_REVERSE, P_WASH},
    {P_ATTACK, P_DECAY, P_BREATH, P_CONSONANT},
    {P_WOW, P_WOWRATE, P_FLUTTER, P_DROPOUT},
    {P_TEMPO, P_DENSITY, P_SILENCE, P_REPEAT},
    {P_CHORD1, P_CHORD2, P_CHORD3, P_CHORD4},
    {P_BED, P_BEDLEVEL, P_HIPASS, P_PADLEVEL},
    {P_ANSDELAY, P_ANSTRANS, P_ANSCHANCE, P_ANSMODE},
    {P_ROOT, P_SCALE, P_PADBARS, P_LIGHTS},
    {P_THROAT, P_TONE, P_RASP, P_VIBRATO},
};
/* the help cards: a button held alone for a second */
static const char *const HELP[NB][2] = {
    {"ROOM", "The bed, its level, a high-pass, the pad"},
    {"KEY", "Root, scale, bars a chord, the lights"},
    {"BREATH", "Attack, decay to breath, breath, consonant"},
    {"TAPE", "Wow, its rate, flutter, dropouts"},
    {"THROAT", "The formants: throat, tone, rasp, vibrato"},
    {"ANSWER", "The second voice: delay, transpose, chance"},
    {"GHOST", "Fragment, stutter, reverse, wash. +REC clears"},
    {"SAVE", "Saves the melody and every setting"},
    {"PAD", "The four chords under the voice"},
    {"PLACE", "Tempo, density, silence bars, repeats"},
    {0, 0},
    {"REC", "Tap: the keys write the melody. Hold: keep"},
    {"LOWER", "The pad's register"},
    {"HIGHER", "The pad's register"},
};

static struct {
    uint8_t view;
    uint32_t saved_t;
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

/* -------------------------------------------------------------------------------------------------- input --- */
static const uint8_t BLACK[11] = PLAT_BLACK_KEYS;
static const char *const VOWEL_NAME[EN_VOW_N] = {"A", "E", "I", "O", "U", "AH", "OH", "UH", "HA", "HI", "HU"};
static int black_index(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK[i] == k)
            return i;
    return -1;
}
static int black_held(void)                   /* the black key held, or -1 */
{
    int i;
    for (i = 10; i >= 0; i--)
        if (hui.keys >> BLACK[i] & 1u)
            return i;
    return -1;
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
        else if ((st & 0xF0u) == 0x90u && d2)
            en_key((int)d1, (int)d2, 1);
        else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
            en_key((int)d1, 0, 0);
    }
}

static void input(void)
{
    uint32_t i;
    int32_t e;
    int b;
    hui_poll();
    /* the keys: the white ones sing; a black one held is the vowel */
    for (i = 0; i < NKEYS; i++) {
        uint32_t m = 1u << i;
        b = black_index((int)i);
        if (hui.kdown & m) {
            if (b >= 0) {
                en_vowel(b);
                hui_say("VOWEL", VOWEL_NAME[b]);
            } else {
                en_key(53 + (int)i, 100, 1);
            }
        }
        if (hui.kup & m) {
            if (b >= 0)
                en_vowel(black_held());
            else
                en_key(53 + (int)i, 0, 0);
        }
    }
    /* transport and holds */
    if (hui.pressed >> B_PLAY & 1u) {
        en_play(!en_playing);
        hui_say(en_playing ? "STOPPED" : "PLAYING", 0);
    }
    if (hui_combo(B_HOME, B_REC)) {
        en_clear();
        hui_say("CLEARED", "THE MELODY");
        dirty = 1;
    }
    if (hui_hold(B_REC, 1500u)) {
        en_keep();
        hui_say("KEPT", "THE PHRASE SUNG");
        dirty = 1;
    }
    if (hui.released >> B_REC & 1u) {
        en_arm(!en_armed);
        hui_say(en_armed ? "WRITTEN" : "WRITE", en_armed ? 0 : "PLAY THE MELODY");
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
        set_par(P_PADOCT, proj.par[P_PADOCT] - 1);
        say_par(P_PADOCT);
    }
    if (hui.pressed >> B_OCTUP & 1u) {
        set_par(P_PADOCT, proj.par[P_PADOCT] + 1);
        say_par(P_PADOCT);
    }
    for (i = 0; i < NV; i++)                      /* a page's button, released: its page */
        if (hui.released >> VIEW_BTN[i] & 1u) {
            ui.view = (uint8_t)i;
            hui_say(VIEW_NAME[i], 0);
        }
    /* the encoders */
    if ((e = hui.enc[EN_SELECT]) != 0) {
        set_par(P_PITCH, proj.par[P_PITCH] + hui_accel(EN_SELECT, e, 36));
        say_par(P_PITCH);
    }
    if ((e = hui.enc[EN_ALGO]) != 0) {
        set_par(P_SINGER, proj.par[P_SINGER] + (e > 0 ? 1 : -1));
        say_par(P_SINGER);
    }
    if ((e = hui.enc[EN_PRESET]) != 0) {
        set_par(P_BOOK, proj.par[P_BOOK] + (e > 0 ? 1 : -1));
        say_par(P_BOOK);
    }
    for (i = 0; i < 4u; i++)
        if ((e = hui.enc[EN_K1 + i]) != 0) {
            turn_par(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            hui_touch((int)i);
        }
    midi_in();
    if (en_melody_ver != melody_seen && !en_writing) {   /* a melody written, kept or cleared: unsaved */
        melody_seen = en_melody_ver;
        dirty = 1;
    }
}

/* -------------------------------------------------------------------------------------------------- lights --- */
static void lights(void)
{
    uint32_t btn = 0, keys = hui.keys, i;
    int lit = proj.par[P_LIGHTS];
    if (en_playing)
        btn |= 1u << B_PLAY;
    if (en_armed || en_writing)
        btn |= 1u << B_REC;
    if (dirty)
        btn |= 1u << B_SAVE;
    btn |= 1u << VIEW_BTN[ui.view];
    if (lit)                                       /* the notes sung, on the keys */
        for (i = 0; i < 2u; i++)
            if (en_vsyl[i] >= 0) {
                int k = (int)en_vnote[i] - 53;
                while (k < 0)
                    k += 12;
                while (k >= NKEYS)
                    k -= 12;
                keys |= 1u << k;
            }
    plat_glow(lit == 1);
    plat_play_red(en_armed || en_writing);
    plat_leds(btn, keys);
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

#define K_VOICE RGB(150, 176, 226)
#define K_ANSWER RGB(226, 170, 120)
#define K_PAD RGB(96, 112, 140)
#define K_SILENT RGB(84, 88, 100)
static const char *const PHASE_NAME[4] = {"CALL", "ANSWER", "CHORUS", "SILENCE"};
static const char *const NEXT_NAME[4] = {"ANSWER IN", "CHORUS IN", "SILENCE IN", "CALL IN"};

static uint16_t phase_colour(int ph)
{
    return ph == PH_CALL ? K_VOICE : ph == PH_ANSWER ? K_ANSWER : ph == PH_CHORUS ? TH.hi : K_SILENT;
}

/* a voice's phrase as a contour: each syllable a bar at its pitch, the ones heard bright, the sounding one lit,
 * a thin line joining them; the voice's playhead over it */
static void draw_phrase(int vi, int32_t x0, int32_t y0, int32_t w, int32_t h, int lo, int hi, uint16_t c)
{
    const volatile en_phrase_t *p = &en_phrase[vi];
    int i, n = p->n, steps = p->steps ? p->steps : 16, px = -1, py = -1;
    float vpos = en_vpos[vi];
    if (!n)
        return;
    for (i = 0; i < n; i++) {
        const volatile en_syl_t *s = &p->s[i];
        int note = s->note, sounding = en_vsyl[vi] == i;
        int32_t sx = x0 + (int32_t)s->step * w / steps, sw = (int32_t)s->len * w / steps, sy;
        if (sw < 2)
            sw = 2;
        note = note < lo ? lo : note > hi ? hi : note;
        sy = y0 + h - 2 - (note - lo) * (h - 4) / (hi - lo);
        if (s->on) {
            if (px >= 0)
                cv_line(px, py, sx, sy, mix(TH.bg, c, 6));
            cv_rect(sx, sy - (sounding ? 2 : 1), sw, sounding ? 5 : 3, sounding ? TH.hi : c);
            px = sx + sw;
            py = sy;
        } else {                                       /* a fragment cut away */
            int32_t k;
            for (k = 0; k < sw; k += 4)
                cv_rect(sx + k, sy, 2, 1, mix(TH.bg, c, 5));
        }
    }
    if (vpos >= 0.0f && vpos <= (float)steps) {
        int32_t hx = x0 + (int32_t)(vpos * (float)w / (float)steps);
        cv_rect(hx, y0 - 3, 1, h + 6, vi ? K_ANSWER : TH.hi);
    }
}

static void draw_main(void)
{
    char t[32], n[12];
    int ph = en_phase, lo = 127, hi = 0, i, v, any = 0;
    int32_t x0 = 12, w = 216, cy = 40, ch = 60;
    uint16_t pc = phase_colour(ph);
    cv_begin(240, HUI_MAIN_H, TH.bg);
    /* the phase, and what is coming */
    cv_text(x0, 4, &FONT_B, en_playing ? PHASE_NAME[ph] : "STOPPED", en_playing ? pc : TH.dim);
    if (en_playing && en_next_bars > 0) {
        t[0] = 0;
        h_cat(t, NEXT_NAME[ph]);
        h_cat(t, " ");
        h_itoa(en_next_bars, n);
        h_cat(t, n);
        h_cat(t, en_next_bars == 1 ? " BAR" : " BARS");
        cv_text(x0 + 70, 7, &FONT_XS, t, TH.dim);
    }
    /* the pitch, large */
    t[0] = 0;
    if (proj.par[P_PITCH] > 0)
        h_cat(t, "+");
    h_itoa(proj.par[P_PITCH], n);
    h_cat(t, n);
    hui_text_r(x0 + w, 0, &FONT_L, t, proj.par[P_PITCH] ? TH.text : TH.dim);
    /* the contour: both voices' phrases over one pitch range */
    for (v = 0; v < 2; v++) {
        const volatile en_phrase_t *p = &en_phrase[v];
        if (v == 1 && en_vpos[1] < 0.0f)
            continue;
        for (i = 0; i < p->n; i++) {
            int note = p->s[i].note;
            if (note < lo)
                lo = note;
            if (note > hi)
                hi = note;
            any = 1;
        }
    }
    if (!any) {
        lo = 55;
        hi = 67;
    }
    lo -= 2;
    hi += 2;
    while (hi - lo < 12) {
        lo--;
        hi++;
    }
    cv_rect(x0, cy + ch / 2, w, 1, TH.line);
    if (en_vpos[1] >= 0.0f)
        draw_phrase(1, x0, cy, w, ch, lo, hi, K_ANSWER);
    draw_phrase(0, x0, cy, w, ch, lo, hi, mix(K_VOICE, TH.dim, en_vpos[0] >= 0.0f ? 0 : 8));
    if (!any)
        hui_text_c(120, cy + 22, &FONT_XS, en_playing ? "LISTENING" : "PLAY, OR SING A KEY", TH.dim);
    /* the countdown: a bar that dims as the next event comes */
    hui_bar(x0, 106, w, 4, en_playing ? en_next_frac : 0.0f, TH.line, mix(TH.dim, pc, (int)(en_next_frac * 16.0f)));
    /* the pad's four chords, the one sounding filled; the melody, the cuts */
    for (i = 0; i < 4; i++) {
        int on = en_playing && en_chord_slot == i, d = proj.par[P_CHORD1 + i];
        int32_t bx = x0 + i * 34;
        hui_round(bx, 118, 30, 20, 3, on ? K_PAD : TH.panel);
        hui_text_c(bx + 15, 121, &FONT_XS, d ? EN_PARAMS[P_CHORD1].names[d] : "-", on ? TH.text : TH.dim);
    }
    if (en_writing || en_armed) {
        hui_text_r(x0 + w, 121, &FONT_XS, en_writing ? "WRITING" : "WRITE: PLAY KEYS", TH.warn);
    } else if (en_stutter || en_reverse) {
        hui_text_r(x0 + w, 121, &FONT_XS, en_stutter ? "STUTTER" : "REVERSE", TH.acc);
    } else if (en_nmelody) {
        t[0] = 0;
        h_cat(t, "MEL ");
        h_itoa(en_nmelody, n);
        h_cat(t, n);
        hui_text_r(x0 + w, 121, &FONT_XS, t, TH.text);
    } else {
        hui_text_r(x0 + w, 121, &FONT_XS, EN_PARAMS[P_BOOK].names[proj.par[P_BOOK]], TH.dim);
    }
    cv_blit(0, HUI_MAIN_Y);
}

static void draw(void)
{
    const char *names[4], *vals[4];
    static char txt[4][14];
    float v[4];
    uint8_t bip[4];
    char r[12];
    int i;
    /* the header: the singer in the phase's colour, the book or the melody; the tempo, or EXT */
    if (en_ext) {
        r[0] = 'E';
        r[1] = 'X';
        r[2] = 'T';
        r[3] = 0;
    } else {
        h_itoa(proj.par[P_TEMPO], r);
    }
    hui_header(EN_PARAMS[P_SINGER].names[proj.par[P_SINGER]], en_playing ? phase_colour(en_phase) : K_SILENT,
               en_nmelody ? "Melody" : EN_PARAMS[P_BOOK].names[proj.par[P_BOOK]], r, dirty);
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
        h_cat(t, EN_PARAMS[P_SINGER].names[proj.par[P_SINGER]]);
        h_cat(t, ", ");
        h_cat(t, EN_PARAMS[P_BOOK].names[proj.par[P_BOOK]]);
        h_cat(t, ". PITCH ");
        h_itoa(proj.par[P_PITCH], n);
        h_cat(t, n);
        h_cat(t, ". PAGE ");
        h_cat(t, VIEW_NAME[ui.view]);
        if (en_playing) {
            h_cat(t, ". ");
            h_cat(t, PHASE_NAME[en_phase]);
        } else {
            h_cat(t, ". STOPPED");
        }
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
    melody_seen = en_melody_ver;
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12); }
void app_panic(void) { en_panic(); }
void app_ui_init(void)
{
    TH.bg = RGB(16, 18, 26);
    TH.panel = RGB(36, 40, 54);
    TH.line = RGB(52, 58, 76);
    TH.dim = RGB(118, 126, 150);
    TH.text = RGB(222, 228, 240);
    TH.acc = RGB(150, 176, 226);
    TH.hi = RGB(214, 226, 255);
    TH.warn = RGB(226, 120, 90);
    TH.part[0] = K_VOICE;
    TH.part[1] = K_ANSWER;
    TH.part[2] = K_PAD;
    TH.part[3] = K_SILENT;
    hui_init(HELP);
    memset(&ui, 0, sizeof ui);
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
    if (!strcmp(what, "playing"))
        *got = en_playing;
    else if (!strcmp(what, "armed"))
        *got = en_armed;
    else if (!strcmp(what, "writing"))
        *got = en_writing;
    else if (!strcmp(what, "phase"))
        *got = en_phase;
    else if (!strcmp(what, "phases"))
        *got = en_phases;
    else if (!strcmp(what, "cycles"))
        *got = (int)en_cycles;
    else if (!strcmp(what, "nmelody"))
        *got = en_nmelody;
    else if (!strcmp(what, "melody_steps"))
        *got = en_melody_steps;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "dirty"))
        *got = dirty;
    else if (!strcmp(what, "steps"))
        *got = (int)en_steps_total;
    else if (!strcmp(what, "chord"))
        *got = en_chord_slot;
    else if (!strncmp(what, "par", 3))
        *got = proj.par[atoi(what + 3)];
    else
        return 0;
    return 1;
}
#endif
