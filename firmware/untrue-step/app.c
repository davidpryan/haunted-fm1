/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* UNTRUE STEP on fm1-base: the project (what is saved), the panel and the screen, and the glue to the engine
 * (dsp/engine.c, its own -O2 unit). Part of fm1-base's unity build, after the platform.
 *
 *   white keys     play the lane, soft on the left, hard on the right (the SUB and PAD lanes: scale degrees)
 *   REC + white key   the hit is kept, at the exact moment it was played   REC tap: keep every hit (overdub)
 *   SEQ + white key   an event near that anchor (the 16 eighths of the two bars), LOOSE away
 *   black keys     1-6 the lane; 7, 8 nudge the selected event -5, +5 ms; 9 removes it; 10 mutes the lane;
 *                  11 the drums leave now
 *   PLAY           start / stop        HOME + REC   the groove's template alone (the edits cleared)
 *   SELECT         SHUFFLE: push <- straight -> swing    ALGORITHM the groove   PRESETS the kit
 *   OCT- / OCT+    the sub's octave
 *   HOME ENV LFO SEQ ARP FX GLO SEL EDIT   the pages; KNOB 1-4 their four values (SEQ: the event editor)
 *   SAVE           saves the edits and every setting */
#include "hui.h"
#include "engine.h"

/* ------------------------------------------------------------------------------------------------ project --- */
#define PROJ_MAGIC 0x31505355u           /* "USP1" */
#define PROJ_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    int16_t par[P_N];
    uint8_t nuser, lane, rsv0[2];
    uint32_t seed;
    en_mod_t mods[EN_NLANES][EN_SLOTS];
    en_user_t user[EN_MAX_USER];
    uint8_t rsv[16];
} project_t;
static project_t proj;
static uint8_t dirty;
static uint32_t pat_seen;                /* en_pat_ver as last copied into the project */

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
    en_pattern_load(&proj.mods[0][0], proj.user, proj.nuser, proj.seed);
    en_lane = proj.lane < L_N ? proj.lane : 0;
}

static int project_load(void)
{
    if (hui_load(OBJ_PROJ, &proj, sizeof proj, PROJ_MAGIC, PROJ_FORMAT) == 0 && hp_ok(EN_PARAMS, P_N, proj.par) &&
        proj.nuser <= EN_MAX_USER)
        return 0;
    project_defaults();
    return -1;
}

static void project_take_pattern(void)  /* the engine's edits into the project */
{
    int i, l, k;
    for (l = 0; l < EN_NLANES; l++)
        for (k = 0; k < EN_SLOTS; k++)
            proj.mods[l][k] = en_mods[l][k];
    for (i = 0; i < en_nuser; i++)
        proj.user[i] = en_user[i];
    proj.nuser = en_nuser;
    proj.seed = en_seed_now;
    proj.lane = en_lane;
    pat_seen = en_pat_ver;
}

static int project_save(void)
{
    project_take_pattern();
    proj.par[P_EVSEL] = (int16_t)en_sel;
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

/* ----------------------------------------------------------------------------------------------- the pages --- */
enum { V_HOME, V_ENV, V_LFO, V_SEQ, V_ARP, V_FX, V_GLO, V_SEL, V_EDIT, NV };
static const char *const VIEW_NAME[NV] = {"Groove", "Decay", "Drag", "Events", "Pad", "Sub", "Leave", "Key", "Kit"};
static const uint8_t VIEW_BTN[NV] = {B_HOME, B_ENV, B_LFO, B_SEQ, B_ARP, B_FX, B_GLO, B_SEL, B_EDIT};
static const int8_t VIEW_KNOB[NV][4] = {
    {P_LOOSE, P_GHOSTS, P_DUCK, P_DUST},
    {P_DEC_KICK, P_DEC_SNARE, P_DEC_HAT, P_DEC_FOLEY},
    {P_DRAG, P_DRAGBARS, P_WANDER, P_FLAM},
    {P_EVSEL, P_EVMOVE, P_EVVEL, P_EVPROB},
    {P_PROG, P_CHORDBARS, P_DETUNE, P_PAD},
    {P_SUB, P_SUBDECAY, P_DELAY, P_FEEDBACK},
    {P_EVERY, P_AWAY, P_CHANCE, P_RETURN},
    {P_ROOT, P_SCALE, P_TEMPO, P_BED},
    {P_KTUNE, P_STONE, P_HCOLOUR, P_ROOM},
};
/* the help cards: a button held alone for a second */
static const char *const HELP[NB][2] = {
    {"SUB", "Sub level, its decay, delay send, feedback"},
    {"KEY", "Root, scale, tempo, the bed"},
    {"DECAY", "Each lane's decay: kick, snare, hat, foley"},
    {"DRAG", "The tempo breathes; wander, flams"},
    {"KIT", "Kick tune, snare tone, hat colour, room"},
    {"LEAVE", "Every N bars the drums leave for M"},
    {"GROOVE", "Loose, ghosts, duck, dust. +REC clears"},
    {"SAVE", "Saves the edits and every setting"},
    {"PAD", "Four chords, how often, detune, level"},
    {"EVENTS", "Pick a hit, move it, velocity, chance"},
    {0, 0},
    {"REC", "Hold + keys: keep the hits as played"},
    {"SUB DOWN", "The sub's octave"},
    {"SUB UP", "The sub's octave"},
};
static const char *const LANE_NAME[L_N] = {"KICK", "SNARE", "HAT", "FOLEY", "SUB", "PAD"};
static const char *const LANE_TAG[L_N] = {"K", "S", "H", "F", "B", "P"};
static const char *const ROMAN[7] = {"i", "ii", "III", "iv", "v", "VI", "VII"};

static struct {
    uint8_t view;
    uint8_t rec_momentary;               /* REC held with a key: recording until it goes up */
    uint32_t key_lit;                    /* the anchor keys of the hits just fired */
} ui;

/* ------------------------------------------------------------------------------------------------ values --- */
static void set_par(int p, int v)
{
    v = hp_clamp(&EN_PARAMS[p], v);
    if (v == proj.par[p])
        return;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    if (p != P_EVSEL)
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

/* the selected event's values, as the Events page shows them: its offset from the nearest anchor in ms, its
 * velocity, its chance */
static float ticks_per_ms(void) { return (float)EN_BEAT * en_tempo_now / 60000.0f; }
static int sel_offset_ms(void)
{
    int t, anchor;
    if (en_sel >= en_nev)
        return 0;
    t = en_ev[en_sel].t;
    anchor = ((t + EN_EIGHTH / 2) / EN_EIGHTH) * EN_EIGHTH;
    return (int)((float)(t - anchor) / ticks_per_ms() + ((t - anchor) < 0 ? -0.5f : 0.5f));
}
static int sel_value(int p)
{
    if (en_sel >= en_nev)
        return EN_PARAMS[p].def;
    if (p == P_EVMOVE)
        return hp_clamp(&EN_PARAMS[p], sel_offset_ms());
    if (p == P_EVVEL)
        return en_ev[en_sel].vel;
    if (p == P_EVPROB)
        return en_ev[en_sel].prob;
    return proj.par[p];
}

/* -------------------------------------------------------------------------------------------------- input --- */
static const uint8_t BLACK[11] = PLAT_BLACK_KEYS;
static const uint8_t WHITE[16] = PLAT_WHITE_KEYS;
static int black_index(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK[i] == k)
            return i;
    return -1;
}
static int white_index(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE[i] == k)
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
        else if ((st & 0xF0u) == 0x90u && d2) {       /* a drum note: the GM kit's lanes */
            int lane = d1 <= 36u ? L_KICK : d1 <= 40u ? L_SNARE : d1 <= 46u ? L_HAT : L_FOLEY;
            en_hit(lane, (int)d2);
        }
    }
}

static void white_key(int w, int down)
{
    int lane = en_lane;
    if (lane >= EN_NLANES) {                           /* SUB, PAD: a scale degree */
        en_note(lane, w, down);
        return;
    }
    if (!down)
        return;
    if (hui_held(B_SEQ)) {                             /* an event near this anchor */
        hui_use(B_SEQ);
        en_add(lane, w);
        hui_say("ADDED", LANE_NAME[lane]);
        dirty = 1;
        return;
    }
    if (hui_held(B_REC) && !en_recording) {            /* REC held: the hits are kept while it is */
        hui_use(B_REC);
        en_rec(1);
        ui.rec_momentary = 1;
    }
    en_hit(lane, 60 + w * 4);
}

static void black_key(int b)
{
    if (b < L_N) {
        en_lane = (uint8_t)b;
        hui_say("LANE", LANE_NAME[b]);
        dirty = 1;
    } else if (b == 6 || b == 7) {
        en_nudge(b == 6 ? -5 : 5);
        hui_say("NUDGE", b == 6 ? "-5 ms" : "+5 ms");
        dirty = 1;
    } else if (b == 8) {
        en_remove();
        hui_say("REMOVED", 0);
        dirty = 1;
    } else if (b == 9) {
        if (en_lane < EN_NLANES) {
            int on = !(en_mutes >> en_lane & 1u);
            en_mute(en_lane, on);
            hui_say(LANE_NAME[en_lane], on ? "MUTED" : "ON");
        }
    } else {
        en_leave();
        hui_say("LEAVE", "THE DRUMS GO");
    }
}

static void input(void)
{
    uint32_t i;
    int32_t e;
    hui_poll();
    for (i = 0; i < NKEYS; i++) {
        uint32_t m = 1u << i;
        int b, w;
        if (hui.kdown & m) {
            if ((b = black_index((int)i)) >= 0)
                black_key(b);
            else if ((w = white_index((int)i)) >= 0)
                white_key(w, 1);
        }
        if ((hui.kup & m) && (w = white_index((int)i)) >= 0)
            white_key(w, 0);
    }
    if (ui.rec_momentary && !hui_held(B_REC)) {
        ui.rec_momentary = 0;
        en_rec(0);
    }
    /* transport and holds */
    if (hui.pressed >> B_PLAY & 1u) {
        en_play(!en_playing);
        hui_say(en_playing ? "STOPPED" : "PLAYING", 0);
    }
    if (hui_combo(B_HOME, B_REC)) {
        en_clear_edits();
        hui_say("CLEARED", "THE TEMPLATE ALONE");
        dirty = 1;
    }
    if (hui.released >> B_REC & 1u) {
        en_rec(!en_recording);
        hui_say(en_recording ? "REC OFF" : "REC", en_recording ? 0 : "HITS ARE KEPT");
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
        set_par(P_SUBOCT, proj.par[P_SUBOCT] - 1);
        say_par(P_SUBOCT);
    }
    if (hui.pressed >> B_OCTUP & 1u) {
        set_par(P_SUBOCT, proj.par[P_SUBOCT] + 1);
        say_par(P_SUBOCT);
    }
    for (i = 0; i < NV; i++)                      /* a page's button, released: its page */
        if (hui.released >> VIEW_BTN[i] & 1u) {
            ui.view = (uint8_t)i;
            hui_say(VIEW_NAME[i], 0);
        }
    /* the encoders */
    if ((e = hui.enc[EN_SELECT]) != 0) {
        set_par(P_SHUFFLE, proj.par[P_SHUFFLE] + hui_accel(EN_SELECT, e, 100));
        say_par(P_SHUFFLE);
    }
    if ((e = hui.enc[EN_ALGO]) != 0) {
        set_par(P_GROOVE, proj.par[P_GROOVE] + (e > 0 ? 1 : -1));
        say_par(P_GROOVE);
    }
    if ((e = hui.enc[EN_PRESET]) != 0) {
        set_par(P_KIT, proj.par[P_KIT] + (e > 0 ? 1 : -1));
        say_par(P_KIT);
    }
    for (i = 0; i < 4u; i++)
        if ((e = hui.enc[EN_K1 + i]) != 0) {
            int p = VIEW_KNOB[ui.view][i];
            if (ui.view == V_SEQ && i > 0u) {         /* the event editor: the selected event's own values */
                int d = hui_accel(EN_K1 + (int)i, e, EN_PARAMS[p].hi - EN_PARAMS[p].lo);
                en_edit(i == 1u ? EV_MOVE : i == 2u ? EV_VEL : EV_PROB, d);
                dirty = 1;
            } else {
                turn_par(EN_K1 + (int)i, p, e);
            }
            hui_touch((int)i);
        }
    midi_in();
    if (en_pat_ver != pat_seen) {                     /* the edits changed: unsaved */
        pat_seen = en_pat_ver;
        dirty = 1;
    }
    proj.par[P_EVSEL] = (int16_t)en_sel;
}

/* -------------------------------------------------------------------------------------------------- lights --- */
static void lights(void)
{
    uint32_t btn = 0, keys = 0, i;
    if (en_playing)
        btn |= 1u << B_PLAY;
    if (en_recording)
        btn |= 1u << B_REC;
    if (dirty)
        btn |= 1u << B_SAVE;
    btn |= 1u << VIEW_BTN[ui.view];
    keys |= 1u << BLACK[en_lane];
    if (en_playing) {                                  /* the anchor of a hit of the lane, as it fires */
        for (i = 0; i < en_nev; i++)
            if (en_ev[i].lit > 12u && en_ev[i].lane == en_lane)
                keys |= 1u << WHITE[((en_ev[i].t + EN_EIGHTH / 2) / EN_EIGHTH) & 15];
    }
    keys |= hui.keys;
    plat_glow(1);
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

#define K_KICK RGB(214, 122, 92)
#define K_SNARE RGB(228, 198, 122)
#define K_HAT RGB(118, 192, 204)
#define K_FOLEY RGB(172, 142, 212)
#define K_SUB RGB(120, 150, 196)
#define K_PAD RGB(96, 118, 150)
#define TL_X 20
#define TL_W 204
#define TL_Y 12
#define TL_ROW 17

static int32_t tl_x(int t) { return TL_X + (int32_t)((uint32_t)t * (uint32_t)TL_W / (uint32_t)EN_CYCLE); }

static void draw_main(void)
{
    char t[32], n[12];
    int32_t i, px;
    const uint16_t lane_c[L_N] = {K_KICK, K_SNARE, K_HAT, K_FOLEY, K_SUB, K_PAD};
    int sel_view = ui.view == V_SEQ, loose_px = (int)(fm_clampf((float)proj.par[P_LOOSE], 0.0f, 100.0f) * 0.4f * ticks_per_ms() * (float)TL_W / (float)EN_CYCLE + 0.5f);
    cv_begin(240, HUI_MAIN_H, TH.bg);
    /* the ruler: two bars, eight beats */
    for (i = 0; i < 8; i++) {
        int32_t x = tl_x(i * EN_BEAT);
        cv_rect(x, 2, 1, (i & 3) ? 4 : 8, (i & 3) ? TH.line : TH.dim);
        cv_rect(x, TL_Y, 1, L_N * TL_ROW, (i & 3) ? mix(TH.bg, TH.line, 8) : TH.line);
    }
    cv_rect(TL_X + TL_W - 1, TL_Y, 1, L_N * TL_ROW, TH.line);
    /* the lanes */
    for (i = 0; i < L_N; i++) {
        int32_t y = TL_Y + i * TL_ROW, cy = y + TL_ROW / 2;
        int away = i < EN_NLANES && ((en_away | en_mutes) >> i & 1u), cur = i == en_lane;
        uint16_t c = away ? mix(lane_c[i], TH.bg, 11) : lane_c[i];
        cv_rect(TL_X, y + TL_ROW - 1, TL_W, 1, mix(TH.bg, TH.line, 10));
        cv_text(3, y + 2, &FONT_T, LANE_TAG[i], cur ? TH.text : away ? TH.line : TH.dim);
        if (cur)
            cv_rect(TL_X - 4, y + 2, 2, TL_ROW - 4, TH.text);
        if (i == L_SUB) {                              /* the sub: a floor that lights with the kick */
            uint16_t sc = en_sub_lit ? TH.hi : c;
            cv_rect(TL_X, cy, TL_W, 2, mix(TH.bg, sc, en_sub_lit ? 14 : 6));
            cv_text(TL_X + 4, y + 2, &FONT_T, EN_PARAMS[P_SUBOCT].names[proj.par[P_SUBOCT] + 1], mix(TH.bg, c, 12));
        } else if (i == L_PAD) {                       /* the pad: the chord, pumping with the duck */
            int32_t h = (int32_t)(en_duck * (float)(TL_ROW - 4));
            cv_rect(TL_X, y + TL_ROW - 2 - h, TL_W, h, mix(TH.bg, c, en_playing ? 5 : 2));
            cv_text(TL_X + 4, y + 2, &FONT_T, ROMAN[en_chord % 7], TH.dim);
        }
    }
    /* the hits: a dot whose size is the velocity, a halo for the spread, bright as it fires */
    for (i = 0; i < en_nev; i++) {
        const volatile en_ev_t *e = &en_ev[i];
        int lane = e->lane, eff = en_eff[i], lit = e->lit > 0, sel = sel_view && i == en_sel;
        int32_t x = tl_x(eff >= 0 ? eff : e->t), cy = TL_Y + lane * TL_ROW + TL_ROW / 2, r = 1 + (int32_t)e->vel * 3 / 127;
        int away = (en_away | en_mutes) >> lane & 1u;
        uint16_t c = away ? mix(lane_c[lane], TH.bg, 11) : lane_c[lane];
        if (lane >= EN_NLANES)
            continue;
        if (eff < 0) {                                 /* skipped this cycle: a hollow mark */
            hui_ring(x, cy, r, mix(TH.bg, c, 7));
            continue;
        }
        if (loose_px > 0 && !away)
            hui_ring(x, cy, r + 1 + loose_px, mix(TH.bg, c, 5));
        if (sel) {
            hui_disc(x, cy, r + 3, TH.text);
            hui_disc(x, cy, r + 1, TH.bg);
        }
        hui_disc(x, cy, lit ? r + 1 : r, lit ? TH.hi : e->user ? mix(c, TH.text, 5) : c);
    }
    if (en_ghost_lit && en_playing) {                  /* a ghost: a small spark at the playhead */
        int lane = en_ghost_lit - 1;
        hui_disc(tl_x((int)en_pos), TL_Y + lane * TL_ROW + TL_ROW / 2, 1, TH.hi);
    }
    /* the playhead */
    px = tl_x((int)en_pos);
    cv_rect(px, TL_Y - 2, 1, L_N * TL_ROW + 2, en_recording ? TH.warn : en_playing ? TH.hi : TH.dim);
    /* the line under it */
    if (sel_view && en_sel < en_nev) {                 /* the selected event: its lane, its offset */
        int ms = sel_offset_ms();
        t[0] = 0;
        h_cat(t, en_ev[en_sel].user ? "MINE " : "");
        h_cat(t, LANE_NAME[en_ev[en_sel].lane]);
        h_cat(t, " ");
        if (ms > 0)
            h_cat(t, "+");
        h_itoa(ms, n);
        h_cat(t, n);
        h_cat(t, "ms");
        cv_text(TL_X, 120, &FONT_XS, t, TH.text);
        t[0] = 0;
        h_itoa(en_sel + 1, n);
        h_cat(t, n);
        h_cat(t, " OF ");
        h_itoa(en_nev, n);
        h_cat(t, n);
        hui_text_r(TL_X + TL_W, 120, &FONT_XS, t, TH.dim);
    } else if (!en_playing) {
        hui_text_c(120, 120, &FONT_XS, "PLAY. KEYS PLAY THE LANE, REC KEEPS THEM", TH.dim);
    } else {
        t[0] = 0;
        h_cat(t, "BAR ");
        h_itoa((int)en_bar, n);
        h_cat(t, n);
        cv_text(TL_X, 120, &FONT_XS, t, TH.text);
        hui_text_c(120, 120, &FONT_XS, en_away == 0x0F ? "THE DRUMS HAVE LEFT" : en_returning ? "COMING BACK" : en_recording ? "KEEPING THE HITS" : "",
                   en_away ? TH.acc : TH.warn);
        t[0] = 0;
        h_itoa(en_nev, n);
        h_cat(t, n);
        h_cat(t, " HITS");
        hui_text_r(TL_X + TL_W, 120, &FONT_XS, t, TH.dim);
    }
    hui_bar(TL_X, 140, TL_W, 2, en_level * 1.5f, TH.line, TH.acc);
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
    /* the header: the groove, the kit; the tempo this moment, or EXT */
    if (en_ext) {
        r[0] = 'E';
        r[1] = 'X';
        r[2] = 'T';
        r[3] = 0;
    } else {
        h_itoa((int)(en_tempo_now + 0.5f), r);
    }
    hui_header(EN_PARAMS[P_GROOVE].names[proj.par[P_GROOVE]], en_away == 0x0F ? TH.dim : TH.acc,
               EN_PARAMS[P_KIT].names[proj.par[P_KIT]], r, dirty);
    if (hui.help)
        hui_help_card();
    else if (hui_main_due(50u, 0))
        draw_main();
    for (i = 0; i < 4; i++) {
        int p = VIEW_KNOB[ui.view][i], val = ui.view == V_SEQ ? sel_value(p) : proj.par[p];
        names[i] = EN_PARAMS[p].name;
        hp_text(&EN_PARAMS[p], val, txt[i]);
        vals[i] = txt[i];
        v[i] = hp_norm(&EN_PARAMS[p], val);
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
        h_cat(t, EN_PARAMS[P_GROOVE].names[proj.par[P_GROOVE]]);
        h_cat(t, ", ");
        h_cat(t, EN_PARAMS[P_KIT].names[proj.par[P_KIT]]);
        h_cat(t, ". SHUFFLE ");
        h_itoa(proj.par[P_SHUFFLE], n);
        h_cat(t, n);
        h_cat(t, ". LANE ");
        h_cat(t, LANE_NAME[en_lane]);
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
    pat_seen = en_pat_ver;
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12) { en_render(lr, n, gain_q12); }
void app_panic(void) { en_panic(); }
void app_ui_init(void)
{
    TH.bg = RGB(15, 17, 23);
    TH.panel = RGB(36, 40, 52);
    TH.line = RGB(54, 60, 74);
    TH.dim = RGB(122, 130, 146);
    TH.text = RGB(224, 230, 238);
    TH.acc = RGB(124, 164, 212);
    TH.hi = RGB(190, 220, 250);
    TH.warn = RGB(230, 108, 80);
    TH.part[0] = K_KICK;
    TH.part[1] = K_SNARE;
    TH.part[2] = K_HAT;
    TH.part[3] = K_FOLEY;
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
    else if (!strcmp(what, "recording"))
        *got = en_recording;
    else if (!strcmp(what, "bar"))
        *got = (int)en_bar;
    else if (!strcmp(what, "cycle"))
        *got = (int)en_cycle;
    else if (!strcmp(what, "nev"))
        *got = en_nev;
    else if (!strcmp(what, "nuser"))
        *got = en_nuser;
    else if (!strcmp(what, "sel"))
        *got = en_sel;
    else if (!strcmp(what, "lane"))
        *got = en_lane;
    else if (!strcmp(what, "away"))
        *got = en_away;
    else if (!strcmp(what, "returning"))
        *got = en_returning;
    else if (!strcmp(what, "leaves"))
        *got = (int)en_leaves;
    else if (!strcmp(what, "mutes"))
        *got = en_mutes;
    else if (!strcmp(what, "ext"))
        *got = en_ext;
    else if (!strcmp(what, "chord"))
        *got = en_chord;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "dirty"))
        *got = dirty;
    else if (!strcmp(what, "tempo"))
        *got = (int)(en_tempo_now + 0.5f);
    else if (!strncmp(what, "eff", 3))
        *got = en_eff[atoi(what + 3)];
    else if (!strncmp(what, "evt", 3))
        *got = en_ev[atoi(what + 3)].t;
    else if (!strncmp(what, "usert", 5))
        *got = en_user[atoi(what + 5)].t;
    else if (!strncmp(what, "par", 3))
        *got = proj.par[atoi(what + 3)];
    else
        return 0;
    return 1;
}
#endif
