/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* WORN TAPE on fm1-base: the project (what is saved), the panel and the screen, and the glue to the engine
 * (dsp/engine.c, its own -O2 unit). Part of fm1-base's unity build, after the platform.
 *
 *   keys           play the instrument; while recording (REC, then the first key), they go into the master loop
 *   SEQ + black key   a splice point on the loop (the tape repeats, skips or reverses a beat there)
 *   PLAY           the tape runs / stops          REC   arm: the next key starts a recording pass (one loop)
 *   REC held       COMMIT: the aged notes become the master, age 0      HOME + REC   clear the master
 *   SELECT         AGE, the tape's generation, both ways    ALGORITHM the medium   PRESETS the instrument
 *   OCT- / OCT+    tape speed: 1/4, 1/2, 1x, 2x (pitch and tempo together)
 *   HOME ENV LFO SEQ ARP FX GLO SEL EDIT   the pages; KNOB 1-4 their four values
 *   SAVE           saves the master and every setting */
#include "hui.h"
#include "engine.h"

/* ------------------------------------------------------------------------------------------------ project --- */
#define PROJ_MAGIC 0x31505457u           /* "WTP1" */
#define PROJ_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    int16_t par[P_N];
    uint8_t nmaster, loop_steps, rsv0[2];
    uint32_t seed, splices;
    en_note_t master[EN_MAX_NOTES];
    uint8_t rsv[16];
} project_t;
static project_t proj;
static uint8_t dirty;
static uint32_t master_seen;             /* en_master_ver as last copied into the project */

static void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_N; i++)
        proj.par[i] = EN_PARAMS[i].def;
    proj.loop_steps = (uint8_t)(proj.par[P_LOOPBARS] * 8);
    proj.seed = 0x5EED5u;
}

static void project_apply(void)         /* everything to the engine */
{
    int i;
    for (i = 0; i < P_N; i++)
        en_set(i, proj.par[i]);
    en_master_load(proj.master, proj.nmaster, proj.loop_steps, proj.seed, proj.splices);
}

static int project_load(void)
{
    if (hui_load(OBJ_PROJ, &proj, sizeof proj, PROJ_MAGIC, PROJ_FORMAT) == 0 && hp_ok(EN_PARAMS, P_N, proj.par) &&
        proj.nmaster <= EN_MAX_NOTES && proj.loop_steps == proj.par[P_LOOPBARS] * 8)
        return 0;
    project_defaults();
    return -1;
}

static void project_take_master(void)   /* the engine's master into the project (not while it is being written) */
{
    int i;
    if (en_recording)
        return;
    for (i = 0; i < en_nmaster; i++)
        proj.master[i] = en_master[i];
    proj.nmaster = en_nmaster;
    proj.loop_steps = en_loop_steps;
    proj.seed = en_seed_now;
    proj.splices = en_splices;
    master_seen = en_master_ver;
}

static int project_save(void)
{
    project_take_master();
    proj.par[P_AGE] = (int16_t)en_age;
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

/* ----------------------------------------------------------------------------------------------- the pages --- */
enum { V_HOME, V_ENV, V_LFO, V_SEQ, V_ARP, V_FX, V_GLO, V_SEL, V_EDIT, NV };
static const char *const VIEW_NAME[NV] = {"Tape", "Notes", "Motor", "Loop", "Tape 2", "Wear", "Memory", "Key", "Sound"};
static const uint8_t VIEW_BTN[NV] = {B_HOME, B_ENV, B_LFO, B_SEQ, B_ARP, B_FX, B_GLO, B_SEL, B_EDIT};
static const int8_t VIEW_KNOB[NV][4] = {
    {P_EROSION, P_REGROWTH, P_WOBBLE, P_DUST},
    {P_ATTACK, P_GHOST, P_RELEASE, P_TONE},
    {P_SAG, P_SAGRATE, P_SPEEDUP, P_FLUTTER},
    {P_TEMPO, P_LOOPBARS, P_QUANT, P_GENPASS},
    {P_TAPE2, P_RATIO, P_T2LEVEL, P_T2OCT},
    {P_SAT, P_HFLOSS, P_DROPOUT, P_REVERB},
    {P_MEMORY, P_EVERY, P_RETURN, P_SPLICE},
    {P_ROOT, P_SCALE, P_BED, P_LIGHTS},
    {P_BRIGHT, P_DECAY, P_DETUNE, P_WIDTH},
};
/* the help cards: a button held alone for a second */
static const char *const HELP[NB][2] = {
    {"WEAR", "Saturation, HF loss, dropouts, reverb"},
    {"KEY", "Root, scale, the bed, the lights"},
    {"NOTES", "Attack, how loud ghosts are, release, tone"},
    {"MOTOR", "Sag, its rate, speedups, flutter"},
    {"SOUND", "The instrument: bright, decay, detune, width"},
    {"MEMORY", "How the master returns; the splice mode"},
    {"TAPE", "Erosion, regrowth, wobble, dust. +REC clears"},
    {"SAVE", "Saves the master loop and every setting"},
    {"TAPE 2", "A second, slower tape of the same notes"},
    {"LOOP", "Tempo, bars, quantise, passes a generation"},
    {0, 0},
    {"REC", "Tap: record a pass. Hold: commit the aged loop"},
    {"SLOWER", "Tape speed: pitch and tempo together"},
    {"FASTER", "Tape speed: pitch and tempo together"},
};

static struct {
    uint8_t view;
    uint32_t last_fire;                  /* when a note last started (the keys light) */
    uint32_t lit_keys;                   /* the keys of the notes that just fired */
    uint8_t prev_nalive;
    int32_t prev_step;
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
    if (p != P_AGE)
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
static int black_index(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK[i] == k)
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
    hui_poll();
    /* the keys: notes; with SEQ held, the black keys are splices */
    for (i = 0; i < NKEYS; i++) {
        uint32_t m = 1u << i;
        if (hui.kdown & m) {
            int b = black_index((int)i);
            if (hui_held(B_SEQ) && b >= 0) {
                hui_use(B_SEQ);
                en_splice(b, !(en_splices >> b & 1u));
                hui_say("SPLICE", en_splices >> b & 1u ? "OFF" : "ON");
                dirty = 1;
            } else {
                en_key(53 + (int)i, 100, 1);
            }
        }
        if (hui.kup & m)
            en_key(53 + (int)i, 0, 0);
    }
    /* transport and holds */
    if (hui.pressed >> B_PLAY & 1u) {
        en_play(!en_playing);
        hui_say(en_playing ? "STOPPED" : "PLAYING", 0);
    }
    if (hui_combo(B_HOME, B_REC)) {
        en_clear();
        hui_say("CLEARED", "THE MASTER");
        dirty = 1;
    }
    if (hui_hold(B_REC, 1500u)) {
        en_commit();
        hui_say("COMMITTED", "AGE 0");
        dirty = 1;
    }
    if (hui.released >> B_REC & 1u) {
        en_arm(!en_armed);
        hui_say(en_armed ? "DISARMED" : "REC ARMED", en_armed ? 0 : "PLAY THE LOOP");
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
        set_par(P_SPEED, proj.par[P_SPEED] - 1);
        say_par(P_SPEED);
    }
    if (hui.pressed >> B_OCTUP & 1u) {
        set_par(P_SPEED, proj.par[P_SPEED] + 1);
        say_par(P_SPEED);
    }
    for (i = 0; i < NV; i++)                      /* a page's button, released: its page */
        if (hui.released >> VIEW_BTN[i] & 1u) {
            ui.view = (uint8_t)i;
            hui_say(VIEW_NAME[i], 0);
        }
    /* the encoders */
    if ((e = hui.enc[EN_SELECT]) != 0) {
        set_par(P_AGE, proj.par[P_AGE] + hui_accel(EN_SELECT, e, 64));
        say_par(P_AGE);
    }
    if ((e = hui.enc[EN_ALGO]) != 0) {
        set_par(P_MEDIUM, proj.par[P_MEDIUM] + (e > 0 ? 1 : -1));
        say_par(P_MEDIUM);
    }
    if ((e = hui.enc[EN_PRESET]) != 0) {
        set_par(P_INSTR, proj.par[P_INSTR] + (e > 0 ? 1 : -1));
        say_par(P_INSTR);
    }
    for (i = 0; i < 4u; i++)
        if ((e = hui.enc[EN_K1 + i]) != 0) {
            turn_par(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            hui_touch((int)i);
        }
    midi_in();
    if (en_master_ver != master_seen && !en_recording) {   /* a pass recorded: unsaved */
        master_seen = en_master_ver;
        dirty = 1;
    }
    proj.par[P_AGE] = (int16_t)en_age;
}

/* -------------------------------------------------------------------------------------------------- lights --- */
static void lights(void)
{
    uint32_t btn = 0, keys = hui.keys, i;
    int lit = proj.par[P_LIGHTS];
    if (en_playing)
        btn |= 1u << B_PLAY;
    if (en_armed || en_recording)
        btn |= 1u << B_REC;
    if (dirty)
        btn |= 1u << B_SAVE;
    btn |= 1u << VIEW_BTN[ui.view];
    if (lit) {                                     /* the notes sounding at the playhead, on the keys */
        int st = (int)en_pos;
        if (st != ui.prev_step) {
            ui.prev_step = st;
            ui.lit_keys = 0;
            for (i = 0; i < en_naged; i++)
                if (en_aged[i].state != NS_GHOST && en_aged[i].n.step % en_loop_steps == st) {
                    int k = (int)en_aged[i].n.note - 53;
                    while (k < 0)
                        k += 12;
                    while (k >= NKEYS)
                        k -= 12;
                    ui.lit_keys |= 1u << k;
                }
            ui.last_fire = hui.now;
        }
        if (en_playing && hui.now - ui.last_fire < 120u)
            keys |= ui.lit_keys;
    }
    plat_glow(lit == 1);
    plat_play_red(en_recording);
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

#define K_OXIDE RGB(150, 96, 52)
#define K_WORN RGB(118, 116, 112)
#define K_GHOST RGB(78, 70, 64)
#define K_GROWN RGB(214, 170, 92)
#define K_REEL RGB(52, 48, 46)
#define K_TAPE2 RGB(96, 110, 130)

static void draw_reel(int32_t cx, int32_t cy, int32_t r, float turns, uint16_t c)
{
    int i;
    hui_disc(cx, cy, r, K_REEL);
    hui_ring(cx, cy, r, TH.line);
    for (i = 0; i < 3; i++) {                     /* three spokes */
        float a = turns + (float)i / 3.0f;
        cv_line(cx, cy, cx + (int32_t)(h_sin(a + 0.25f) * (float)(r - 3)), cy + (int32_t)(h_sin(a) * (float)(r - 3)), c);
    }
    hui_disc(cx, cy, 4, c);
}

static void draw_main(void)
{
    char t[32], n[12];
    int32_t i, steps = en_loop_steps, x0 = 20, w = 200, ty = 82, th = 34;
    int age = en_age, drop = en_dropout;
    uint16_t oxide = mix(K_OXIDE, K_WORN, age / 4), bg = drop ? mix(TH.bg, TH.panel, 6) : TH.bg;
    float spin = en_pos / (float)steps;
    cv_begin(240, HUI_MAIN_H, TH.bg);
    /* the reels: the left one empties as the age climbs, the right one fills; the tape runs between them */
    cv_rect(48, 68, 144, 1, mix(bg, oxide, 6));
    draw_reel(48, 42, 24 - age / 8, spin * 3.0f, oxide);
    draw_reel(192, 42, 14 + age / 8, spin * 3.0f * 1.3f, oxide);
    /* the tape: the loop, its notes on it */
    cv_rect(x0 - 4, ty - 2, w + 8, th + 4, mix(bg, oxide, 3));
    cv_rect(x0, ty, w, th, mix(bg, oxide, drop ? 1 : 5));
    for (i = 0; i < steps; i += 8)
        cv_rect(x0 + i * w / steps, ty, 1, th, mix(bg, oxide, 9));
    for (i = 0; i < 11; i++)                      /* splices: a cut */
        if (en_splices >> i & 1u) {
            int32_t sx = x0 + ((int32_t)(((uint32_t)i * (uint32_t)steps + 5u) / 11u) % steps) * w / steps;
            uint16_t c = en_splice_glow == i + 1 ? TH.hi : TH.text;
            cv_line(sx, ty - 2, sx + 3, ty + th + 1, c);
            cv_rect(sx - 2, ty - 5, 5, 3, c);
        }
    for (i = 0; i < en_naged; i++) {
        const volatile en_aged_t *a = &en_aged[i];
        int32_t nx = x0 + (a->n.step % steps) * w / steps, nw = (int32_t)a->n.len * w / steps, ny;
        int p = (int)a->n.note;
        if (nw < 3)
            nw = 3;
        p = p < 40 ? 40 : p > 92 ? 92 : p;
        ny = ty + th - 3 - (p - 40) * (th - 6) / 52;
        if (a->state == NS_GHOST) {
            cv_rect(nx, ny, nw, 1, K_GHOST);
            cv_rect(nx, ny + 2, nw, 1, K_GHOST);
        } else {
            cv_rect(nx, ny, nw, 3, a->state == NS_GROWN ? K_GROWN : mix(TH.text, oxide, 6));
        }
    }
    {
        int32_t px = x0 + (int32_t)(en_pos * (float)w / (float)steps);
        cv_rect(px, ty - 6, 2, th + 12, en_recording ? TH.warn : TH.hi);
        if (proj.par[P_TAPE2]) {
            int32_t p2 = x0 + (int32_t)(en_pos2 * (float)w / (float)steps);
            cv_rect(p2, ty + th + 2, 2, 5, K_TAPE2);
        }
    }
    /* the line under it: the generation and the notes; a hint when there is no master yet */
    if (!en_nmaster && !en_recording) {
        hui_text_c(120, 92, &FONT_XS, en_armed ? "PLAY THE LOOP: ONE PASS" : "REC, THEN PLAY A LOOP", TH.dim);
    } else {
        t[0] = 0;
        h_cat(t, "GEN ");
        h_itoa(age, n);
        h_cat(t, n);
        cv_text(x0, 124, &FONT_XS, t, age >= 64 ? TH.warn : TH.text);
        t[0] = 0;
        h_itoa(en_nalive, n);
        h_cat(t, n);
        h_cat(t, " OF ");
        h_itoa(en_naged, n);
        h_cat(t, n);
        h_cat(t, " NOTES");
        hui_text_c(120, 124, &FONT_XS, t, TH.dim);
        hui_text_r(x0 + w, 124, &FONT_XS, en_recording ? "REC" : en_armed ? "ARMED" : en_playing ? "PLAY" : "STOP",
                   en_recording || en_armed ? TH.warn : en_playing ? TH.hi : TH.dim);
    }
    /* the level, a thin line at the bottom */
    hui_bar(x0, 140, w, 2, en_level * 1.5f, TH.line, oxide);
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
    /* the header: the medium (greyer as it wears), the instrument; the tempo, or EXT */
    if (ui.view == V_SEL) {                   /* the Key page: the audio load, for the curious */
        r[0] = 0;
        h_cat(r, "CPU ");
        h_itoa((int)plat_cpu_pct(), r + 4);
        h_cat(r, "%");
    } else if (en_ext) {
        r[0] = 'E';
        r[1] = 'X';
        r[2] = 'T';
        r[3] = 0;
    } else {
        h_itoa(proj.par[P_TEMPO], r);
    }
    hui_header(EN_PARAMS[P_MEDIUM].names[proj.par[P_MEDIUM]], mix(K_OXIDE, K_WORN, en_age / 4),
               EN_PARAMS[P_INSTR].names[proj.par[P_INSTR]], r, dirty);
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
        h_cat(t, EN_PARAMS[P_MEDIUM].names[proj.par[P_MEDIUM]]);
        h_cat(t, ", ");
        h_cat(t, EN_PARAMS[P_INSTR].names[proj.par[P_INSTR]]);
        h_cat(t, ". AGE ");
        h_itoa(en_age, n);
        h_cat(t, n);
        h_cat(t, ". PAGE ");
        h_cat(t, VIEW_NAME[ui.view]);
        h_cat(t, en_playing ? ". PLAYING" : ". STOPPED");
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
    master_seen = en_master_ver;
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12); }
void app_panic(void) { en_panic(); }
void app_ui_init(void)
{
    TH.bg = RGB(22, 19, 17);
    TH.panel = RGB(44, 38, 33);
    TH.line = RGB(62, 54, 47);
    TH.dim = RGB(132, 118, 104);
    TH.text = RGB(236, 224, 206);
    TH.acc = RGB(214, 170, 92);
    TH.hi = RGB(240, 200, 120);
    TH.warn = RGB(226, 96, 70);
    TH.part[0] = K_OXIDE;
    TH.part[1] = K_GROWN;
    TH.part[2] = K_TAPE2;
    TH.part[3] = K_GHOST;
    hui_init(HELP);
    memset(&ui, 0, sizeof ui);
    ui.prev_step = -1;
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
    else if (!strcmp(what, "recording"))
        *got = en_recording;
    else if (!strcmp(what, "armed"))
        *got = en_armed;
    else if (!strcmp(what, "age"))
        *got = en_age;
    else if (!strcmp(what, "nmaster"))
        *got = en_nmaster;
    else if (!strcmp(what, "naged"))
        *got = en_naged;
    else if (!strcmp(what, "nalive"))
        *got = en_nalive;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "dirty"))
        *got = dirty;
    else if (!strcmp(what, "steps"))
        *got = (int)en_steps_total;
    else if (!strcmp(what, "splices"))
        *got = (int)en_splices;
    else if (!strncmp(what, "par", 3))
        *got = proj.par[atoi(what + 3)];
    else
        return 0;
    return 1;
}
#endif
