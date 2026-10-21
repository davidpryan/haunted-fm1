/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* MIDNIGHT ROUTE on fm1-base: the project (what is saved), the panel and the screen, and the glue to the engine
 * (dsp/engine.c, its own -O2 unit). Part of fm1-base's unity build, after the platform.
 *
 *   white keys     the 16 stations: a tap cycles the tag (normal, DWELL, SKIP, PEAK); held, the station's numbers
 *   black keys     the chord palette: which of the 11 functions the harmony may use
 *   PLAY           depart / pause      PLAY held   from station 1 again
 *   REC            bookmark the station (its material comes back the same)    REC held   clear them (OCT+ confirms)
 *   SELECT         the station (the position on the route)    HOME + SELECT   the route's length
 *   ALGORITHM      the route's shape       PRESETS   the city (root, scale, bed, tempo and the sounds)
 *   OCT- / OCT+    transpose the whole route; the voice keeps its throat, so down is the ghost
 *   HOME ENV LFO SEQ ARP FX GLO SEL EDIT   the pages; KNOB 1-4 their four values
 *   SAVE           saves the route (tags, palette, bookmarks) and every setting */
#include "hui.h"
#include "engine.h"

/* ------------------------------------------------------------------------------------------------ project --- */
#define PROJ_MAGIC 0x3150524Du           /* "MRP1" */
#define PROJ_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    int16_t par[P_N];
    uint8_t tags[EN_NSTATION];
    uint16_t palette;
    uint8_t rsv0[4];
    uint32_t seed;
    uint32_t bookmarks[EN_NSTATION];
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
    proj.palette = 0x7FFu;
    proj.seed = 0x51A7E5u;
}

static void project_apply(void)         /* everything to the engine */
{
    int i;
    for (i = 0; i < P_N; i++)
        en_set(i, proj.par[i]);
    en_route_load(proj.tags, proj.palette, proj.bookmarks, proj.seed);
}

static int project_load(void)
{
    int i, ok = 1;
    if (hui_load(OBJ_PROJ, &proj, sizeof proj, PROJ_MAGIC, PROJ_FORMAT) == 0 && hp_ok(EN_PARAMS, P_N, proj.par)) {
        for (i = 0; i < EN_NSTATION; i++)
            if (proj.tags[i] >= TAG_N)
                ok = 0;
        if (ok)
            return 0;
    }
    project_defaults();
    return -1;
}

static void project_take_route(void)    /* the engine's route into the project */
{
    int i;
    for (i = 0; i < EN_NSTATION; i++) {
        proj.tags[i] = en_tags[i];
        proj.bookmarks[i] = en_bookmarks[i];
    }
    proj.palette = (uint16_t)en_palette_bits;
    proj.seed = en_seed_now;
}

static int project_save(void)
{
    project_take_route();
    proj.par[P_STATION] = (int16_t)en_station;
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

/* ----------------------------------------------------------------------------------------------- the pages --- */
enum { V_HOME, V_ENV, V_LFO, V_SEQ, V_ARP, V_FX, V_GLO, V_SEL, V_EDIT, NV };
static const char *const VIEW_NAME[NV] = {"Route", "Density", "Drift", "Cells", "Motif", "Station FX", "Harmony", "Key", "Timbre"};
static const uint8_t VIEW_BTN[NV] = {B_HOME, B_ENV, B_LFO, B_SEQ, B_ARP, B_FX, B_GLO, B_SEL, B_EDIT};
static const int8_t VIEW_KNOB[NV][4] = {
    {P_LENGTH, P_CEILING, P_CONTRAST, P_FOG},
    {P_PART, P_LEVEL0, P_SLOPE0, P_JITTER0},            /* per part: + P_PART */
    {P_DRIFT, P_DRIFTRATE, P_TIMING, P_DYNAMICS},
    {P_CELL_SUB, P_CELL_PAD, P_CELL_VOICE, P_CELL_RAIN},
    {P_RANGE, P_STEP, P_REPEAT, P_REST},
    {P_FILTER, P_TAIL, P_SWELL, P_ECHO},
    {P_CHANGE, P_SMOOTH, P_BASS, P_END},
    {P_ROOT, P_SCALE, P_BED, P_TEMPO},
    {P_PART, P_BRIGHT0, P_DECAY0, P_WIDTH0},            /* per part: + P_PART */
};
static int knob_par(int view, int i)    /* the page's knob i: the per-part ones follow the PART knob */
{
    int p = VIEW_KNOB[view][i];
    if (p == P_LEVEL0 || p == P_SLOPE0 || p == P_JITTER0 || p == P_BRIGHT0 || p == P_DECAY0 || p == P_WIDTH0)
        p += proj.par[P_PART];
    return p;
}
/* the help cards: a button held alone for a second */
static const char *const HELP[NB][2] = {
    {"STATION FX", "Filter closes, tail on dwell, swell, echo"},
    {"KEY", "Root, scale, the bed, the tempo"},
    {"DENSITY", "A part's curve: level, slope, jitter"},
    {"DRIFT", "Density walks; timing and dynamics spread"},
    {"TIMBRE", "A part's sound: bright, decay, width"},
    {"HARMONY", "Chord change rate, smoothness, bass, the end"},
    {"ROUTE", "Length, ceiling, contrast, fog. +SELECT: length"},
    {"SAVE", "Saves the route and every setting"},
    {"MOTIF", "The voice: range, step, repeats, rests"},
    {"CELLS", "Each part's rhythm cells"},
    {"PLAY", "Depart or pause. Hold: from station 1"},
    {"REC", "Bookmark the station. Hold: clear them"},
    {"DOWN", "Transpose the route: the ghost voice"},
    {"UP", "Transpose the route up"},
};

static struct {
    uint8_t view;
    int8_t held_station;                 /* a white key held: that station's numbers on the screen, else -1 */
    uint32_t confirm_until;              /* REC held: OCT+ clears the bookmarks until then */
    uint32_t key_t;                      /* when a white key went down (a short press is a tap) */
    uint32_t last_pass;
} ui;

/* ------------------------------------------------------------------------------------------------ values --- */
static void set_par(int p, int v)
{
    v = hp_clamp(&EN_PARAMS[p], v);
    if (v == proj.par[p])
        return;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    if (p != P_STATION)
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
static void city_load(int c)            /* PRESETS: the city's key, bed and tempo come with its sounds */
{
    set_par(P_CITY, c);
    set_par(P_ROOT, EN_CITY[proj.par[P_CITY]].root);
    set_par(P_SCALE, EN_CITY[proj.par[P_CITY]].scale);
    set_par(P_BED, EN_CITY[proj.par[P_CITY]].bed);
    set_par(P_TEMPO, EN_CITY[proj.par[P_CITY]].tempo);
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

static void station_text(int s, char *t)   /* "STN 7" */
{
    char n[12];
    t[0] = 0;
    h_cat(t, "STN ");
    h_itoa(s + 1, n);
    h_cat(t, n);
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
        else if ((st & 0xF0u) == 0xB0u && d1 >= 20u && d1 <= 24u) {   /* CC 20-23: the HOME knobs; 24: the station */
            int p = d1 == 24u ? P_STATION : VIEW_KNOB[V_HOME][d1 - 20u];
            set_par(p, EN_PARAMS[p].lo + (int)((uint32_t)(EN_PARAMS[p].hi - EN_PARAMS[p].lo) * d2 + 63u) / 127);
        }
    }
}

static void input(void)
{
    uint32_t i;
    int32_t e;
    char t[16];
    hui_poll();
    ui.held_station = -1;
    /* the keys: white, a station's tag; black, the palette */
    for (i = 0; i < NKEYS; i++) {
        uint32_t m = 1u << i;
        int w = white_index((int)i), b = black_index((int)i);
        if (hui.kdown & m) {
            if (w >= 0) {
                ui.key_t = hui.now;
            } else if (b >= 0) {
                uint32_t bits = en_palette_bits ^ (1u << b);
                en_palette(bits);
                hui_say(EN_FUNC_NAME[b], bits >> b & 1u ? "ALLOWED" : "FORBIDDEN");
                dirty = 1;
            }
        }
        if ((hui.kup & m) && w >= 0 && hui.now - ui.key_t < 400u) {   /* a tap: the tag cycles; a hold only shows */
            int tag = (en_tags[w] + 1) % TAG_N;
            en_tag(w, tag);
            station_text(w, t);
            hui_say(t, EN_TAG_NAME[tag]);
            dirty = 1;
        }
        if ((hui.keys & m) && w >= 0)
            ui.held_station = (int8_t)w;
    }
    /* transport and holds */
    if (hui_hold(B_PLAY, 1200u)) {
        en_restart();
        hui_say("FROM STATION 1", 0);
    }
    if (hui.released >> B_PLAY & 1u) {
        en_play(!en_playing);
        hui_say(en_playing ? "PAUSED" : en_resting ? "DEPARTED" : "DEPARTED", 0);
    }
    if (hui_hold(B_REC, 1500u)) {
        ui.confirm_until = hui.now + 4000u;
        hui_warn("CLEAR BOOKMARKS?", "OCT+ CONFIRMS");
    }
    if (hui.released >> B_REC & 1u) {
        en_bookmark(en_station);
        station_text(en_station, t);
        hui_say("BOOKMARKED", t);
        dirty = 1;
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
        ui.confirm_until = 0;
        set_par(P_TRANSPOSE, proj.par[P_TRANSPOSE] - 1);
        say_par(P_TRANSPOSE);
    }
    if (hui.pressed >> B_OCTUP & 1u) {
        if (ui.confirm_until && (int32_t)(ui.confirm_until - hui.now) > 0) {
            ui.confirm_until = 0;
            en_bookmarks_clear();
            hui_say("BOOKMARKS", "CLEARED");
            dirty = 1;
        } else {
            set_par(P_TRANSPOSE, proj.par[P_TRANSPOSE] + 1);
            say_par(P_TRANSPOSE);
        }
    }
    for (i = 0; i < NV; i++)                      /* a page's button, released: its page */
        if (hui.released >> VIEW_BTN[i] & 1u) {
            ui.view = (uint8_t)i;
            hui_say(VIEW_NAME[i], 0);
        }
    /* the encoders: SELECT is the station (HOME held: the length) */
    if ((e = hui_enc_with(EN_SELECT, B_HOME)) != 0) {
        if (hui_held(B_HOME)) {
            set_par(P_LENGTH, proj.par[P_LENGTH] + (e > 0 ? 1 : -1));
            say_par(P_LENGTH);
        } else {
            set_par(P_STATION, (int)en_station + (int)e);
            station_text(proj.par[P_STATION], t);
            hui_say(t, EN_TAG_NAME[en_tags[proj.par[P_STATION]]]);
        }
    }
    if ((e = hui.enc[EN_ALGO]) != 0) {
        set_par(P_SHAPE, proj.par[P_SHAPE] + (e > 0 ? 1 : -1));
        say_par(P_SHAPE);
    }
    if ((e = hui.enc[EN_PRESET]) != 0) {
        city_load(proj.par[P_CITY] + (e > 0 ? 1 : -1));
        say_par(P_CITY);
    }
    for (i = 0; i < 4u; i++)
        if ((e = hui.enc[EN_K1 + i]) != 0) {
            turn_par(EN_K1 + (int)i, knob_par(ui.view, (int)i), e);
            hui_touch((int)i);
        }
    midi_in();
    proj.par[P_STATION] = (int16_t)en_station;
}

/* -------------------------------------------------------------------------------------------------- lights --- */
static void lights(void)
{
    uint32_t btn = 0, keys = 0, dim = 0, i;
    if (en_playing)
        btn |= 1u << B_PLAY;
    if (en_bookmarks[en_station])
        btn |= 1u << B_REC;
    if (dirty)
        btn |= 1u << B_SAVE;
    btn |= 1u << VIEW_BTN[ui.view];
    for (i = 0; i < 16u; i++) {                   /* the station we are at; the tagged ones dim */
        if (i == en_station)
            keys |= 1u << WHITE[i];
        else if (en_tags[i] != TAG_NORMAL)
            dim |= 1u << WHITE[i];
    }
    for (i = 0; i < 11u; i++)                     /* the chord functions allowed */
        if (en_palette_bits >> i & 1u)
            dim |= 1u << BLACK[i];
    plat_play_red(0);
    plat_lights(btn, keys, 0, dim, 0);
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

#define K_SUB RGB(84, 124, 220)
#define K_PAD RGB(160, 110, 220)
#define K_VOICE RGB(236, 176, 96)
#define K_RAIN RGB(96, 190, 176)
#define MAP_X 2
#define MAP_W 15                         /* a station block, with its gap */
#define LANE_Y 14
#define LANE_H 23

static void mmss(uint32_t secs, char *t)
{
    char n[12];
    uint32_t m = secs / 60u, s = secs % 60u;
    t[0] = 0;
    h_itoa((int)m, n);
    h_cat(t, n);
    h_cat(t, ":");
    if (s < 10u)
        h_cat(t, "0");
    h_itoa((int)s, n);
    h_cat(t, n);
}

static void draw_tag(int32_t x, int tag, int bookmarked)   /* the glyph over a station */
{
    uint16_t c = TH.text;
    switch (tag) {
    case TAG_DWELL:                              /* two bars: twice as long */
        cv_rect(x + 3, 3, 8, 2, c);
        cv_rect(x + 3, 7, 8, 2, c);
        break;
    case TAG_SKIP:                               /* a cross */
        cv_line(x + 3, 2, x + 10, 9, TH.warn);
        cv_line(x + 10, 2, x + 3, 9, TH.warn);
        break;
    case TAG_PEAK:                               /* a peak */
        cv_line(x + 3, 9, x + 7, 2, TH.hi);
        cv_line(x + 7, 2, x + 11, 9, TH.hi);
        break;
    default:
        break;
    }
    if (bookmarked)
        hui_disc(x + 12, 3, 1, TH.acc);
}

static void draw_main(void)
{
    char t[40], n[12];
    int32_t s, p, x, px;
    const uint16_t pc[PT_N] = {TH.part[0], TH.part[1], TH.part[2], TH.part[3]};
    int cur = en_station, held = ui.held_station;
    cv_begin(240, HUI_MAIN_H, TH.bg);
    /* the map: 16 station blocks, four density lanes in the part colours, the tags over them */
    for (s = 0; s < EN_NSTATION; s++) {
        int skip = en_tags[s] == TAG_SKIP, here = s == cur, hl = s == held;
        x = MAP_X + s * MAP_W;
        draw_tag(x, en_tags[s], en_bookmarks[s] != 0);
        for (p = 0; p < PT_N; p++) {
            int32_t y0 = LANE_Y + p * LANE_H, h = (int32_t)en_dens[p][s] * (LANE_H - 3) / 100;
            uint16_t c = skip ? mix(TH.bg, pc[p], 4) : here || hl ? pc[p] : mix(TH.bg, pc[p], 9);
            cv_rect(x, y0 + LANE_H - 2, MAP_W - 1, 1, here || hl ? TH.line : mix(TH.bg, TH.line, 8));
            if (h > 0)
                cv_rect(x, y0 + LANE_H - 2 - h, MAP_W - 1, h, c);
        }
        if (here)
            cv_rect(x, LANE_Y - 2, MAP_W - 1, 1, TH.hi);
        if (hl && !here)
            cv_rect(x, LANE_Y - 2, MAP_W - 1, 1, TH.text);
    }
    /* the playhead */
    px = MAP_X + cur * MAP_W + (int32_t)(en_pos * (float)(MAP_W - 1));
    cv_rect(px, LANE_Y - 3, 1, LANE_H * PT_N, en_playing ? TH.hi : TH.dim);
    /* the line under it: the station and its bar, the chord, the time left (or the part's numbers of a held key) */
    if (held >= 0) {
        station_text(held, t);
        h_cat(t, " ");
        h_cat(t, EN_TAG_NAME[en_tags[held]]);
        cv_text(MAP_X + 2, 110, &FONT_XS, t, TH.text);
        for (p = 0; p < PT_N; p++) {
            h_itoa(en_dens[p][held], n);
            cv_text(124 + p * 28, 110, &FONT_XS, n, pc[p]);
        }
    } else {
        station_text(cur, t);
        h_cat(t, " ");
        h_itoa((int)en_bar + 1, n);
        h_cat(t, n);
        h_cat(t, "/");
        h_itoa((int)en_bars, n);
        h_cat(t, n);
        cv_text(MAP_X + 2, 110, &FONT_XS, t, TH.text);
        t[0] = 0;
        h_cat(t, H_NOTE_NAME[en_chord_root]);
        h_cat(t, " ");
        h_cat(t, EN_FUNC_NAME[en_chord]);
        hui_text_c(120, 110, &FONT_XS, t, TH.dim);
        if (en_resting)
            hui_text_r(238, 110, &FONT_XS, "REST", TH.dim);
        else if (!en_playing)
            hui_text_r(238, 110, &FONT_XS, "PAUSED", TH.dim);
        else {
            mmss(en_secs_left, t);
            hui_text_r(238, 110, &FONT_XS, t, TH.hi);
        }
    }
    /* the parts, lit as they fire; the level */
    for (p = 0; p < PT_N; p++) {
        int lit = en_fired >> p & 1u;
        hui_disc(MAP_X + 6 + p * 40, 132, 2, lit ? pc[p] : mix(TH.bg, pc[p], 6));
        cv_text(MAP_X + 12 + p * 40, 125, &FONT_XS, EN_PART_NAME[p], lit ? TH.text : TH.dim);
    }
    hui_bar(166, 131, 72, 2, en_level * 1.5f, TH.line, TH.acc);
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
    /* the header: the shape, the city; the time left, or the tempo */
    if (en_playing && !en_resting)
        mmss(en_secs_left, r);
    else if (en_ext) {
        r[0] = 'E';
        r[1] = 'X';
        r[2] = 'T';
        r[3] = 0;
    } else {
        h_itoa(proj.par[P_TEMPO], r);
    }
    hui_header(EN_PARAMS[P_SHAPE].names[proj.par[P_SHAPE]], TH.acc, EN_PARAMS[P_CITY].names[proj.par[P_CITY]], r, dirty);
    if (hui.help)
        hui_help_card();
    else if (hui_main_due(50u, 0))
        draw_main();
    for (i = 0; i < 4; i++) {
        int p = knob_par(ui.view, i);
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
        h_cat(t, EN_PARAMS[P_SHAPE].names[proj.par[P_SHAPE]]);
        h_cat(t, ", ");
        h_cat(t, EN_PARAMS[P_CITY].names[proj.par[P_CITY]]);
        h_cat(t, ". STATION ");
        h_itoa(en_station + 1, n);
        h_cat(t, n);
        h_cat(t, " OF 16. PAGE ");
        h_cat(t, VIEW_NAME[ui.view]);
        h_cat(t, en_playing ? ". PLAYING" : en_resting ? ". RESTING" : ". PAUSED");
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
    TH.bg = RGB(13, 15, 28);
    TH.panel = RGB(30, 34, 58);
    TH.line = RGB(48, 54, 86);
    TH.dim = RGB(118, 126, 164);
    TH.text = RGB(222, 226, 240);
    TH.acc = RGB(120, 140, 230);
    TH.hi = RGB(190, 206, 255);
    TH.warn = RGB(230, 100, 80);
    TH.part[0] = K_SUB;
    TH.part[1] = K_PAD;
    TH.part[2] = K_VOICE;
    TH.part[3] = K_RAIN;
    hui_init(HELP);
    memset(&ui, 0, sizeof ui);
    ui.held_station = -1;
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
    else if (!strcmp(what, "resting"))
        *got = en_resting;
    else if (!strcmp(what, "station"))
        *got = en_station;
    else if (!strcmp(what, "bar"))
        *got = en_bar;
    else if (!strcmp(what, "bars"))
        *got = en_bars;
    else if (!strcmp(what, "bars_total"))
        *got = (int)en_bars_total;
    else if (!strcmp(what, "pass"))
        *got = (int)en_pass;
    else if (!strcmp(what, "palette"))
        *got = (int)en_palette_bits;
    else if (!strcmp(what, "nbookmarks"))
        *got = en_nbookmarks;
    else if (!strcmp(what, "chord"))
        *got = en_chord;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "dirty"))
        *got = dirty;
    else if (!strncmp(what, "tag", 3))
        *got = en_tags[atoi(what + 3) & 15];
    else if (!strncmp(what, "dens", 4) && what[4] >= '0' && what[4] <= '3')
        *got = en_dens[what[4] - '0'][atoi(what + 5) & 15];
    else if (!strncmp(what, "par", 3))
        *got = proj.par[atoi(what + 3)];
    else
        return 0;
    return 1;
}
#endif
