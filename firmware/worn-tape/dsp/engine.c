/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 David Ryan */
/* WORN TAPE's engine (engine.h). One compilation unit, -O2. */
#include "engine.h"

/* ---------------------------------------------------------------------------------------------- parameters --- */
static const char *const MEDIUM_NAME[MED_N] = {"Cassette", "Reel", "Dictaphone", "VHS", "Cylinder", "Clean"};
static const char *const INSTR_NAME[INS_N] = {"Felt piano", "Music box", "Pad", "Choir", "Guitar", "Organ"};
static const char *const QUANT_NAME[2] = {"1/8", "1/4"};
static const char *const RATIO_NAME[3] = {"x1.5", "x2", "x3"};
static const char *const RETURN_NAME[2] = {"Splice", "Fade"};
static const char *const SPLICE_NAME[3] = {"Repeat", "Skip", "Reverse"};
static const char *const LIGHTS_NAME[3] = {"Off", "On", "Keys"};
static const char *const SPEED_NAME[4] = {"1/4", "1/2", "1x", "2x"};
const hp_t EN_PARAMS[P_N] = {
    {"Medium", 0, MED_N - 1, 0, MEDIUM_NAME, 0}, {"Instrument", 0, INS_N - 1, 0, INSTR_NAME, 0}, {"Age", 0, 64, 0, 0, ""},
    {"Erosion", 0, 100, 30, 0, "%"}, {"Regrowth", 0, 100, 25, 0, "%"}, {"Wobble", 0, 100, 35, 0, ""}, {"Dust", 0, 100, 40, 0, ""},
    {"Attack", 0, 100, 10, 0, ""}, {"Ghosts", 0, 100, 20, 0, ""}, {"Release", 0, 100, 40, 0, ""}, {"Tone", 0, 100, 60, 0, ""},
    {"Sag", 0, 100, 20, 0, ""}, {"Sag rate", 0, 100, 30, 0, ""}, {"Speedup", 0, 100, 10, 0, "%"}, {"Flutter", 0, 100, 20, 0, ""},
    {"Tempo", 40, 160, 76, 0, ""}, {"Loop", 1, 8, 2, 0, " bar"}, {"Quantise", 0, 1, 0, QUANT_NAME, 0}, {"Passes", 1, 8, 2, 0, ""},
    {"Tape 2", 0, 1, 1, H_ONOFF, 0}, {"Ratio", 0, 2, 1, RATIO_NAME, 0}, {"Level", 0, 100, 45, 0, ""}, {"Octave", -2, 0, -1, 0, ""},
    {"Saturate", 0, 100, 30, 0, ""}, {"HF loss", 0, 100, 60, 0, ""}, {"Dropout", 0, 100, 30, 0, ""}, {"Reverb", 0, 100, 45, 0, ""},
    {"Memory", 0, 100, 50, 0, "%"}, {"Every", 2, 32, 12, 0, " gen"}, {"Return", 0, 1, 0, RETURN_NAME, 0}, {"Splice", 0, 2, 0, SPLICE_NAME, 0},
    {"Root", 0, 11, 9, H_NOTE_NAME, 0}, {"Scale", 0, H_SC_N - 1, 0, H_SCALE_NAME, 0}, {"Bed", 0, H_BED_N - 1, H_BED_CRACKLE, H_BED_NAME, 0},
    {"Lights", 0, 2, 1, LIGHTS_NAME, 0},
    {"Bright", 0, 100, 50, 0, ""}, {"Decay", 0, 100, 50, 0, ""}, {"Detune", 0, 100, 20, 0, ""}, {"Width", 0, 100, 50, 0, ""},
    {"Speed", -2, 1, 0, SPEED_NAME, 0},
};

/* the media: how each one wears. min_hf: the highs left at full age; wow, hiss, drop, sat: multipliers; hp: a
 * narrow medium's low cut */
typedef struct { float min_hf, wow, hiss, drop, sat, hp, decay; } medium_t;
static const medium_t MEDIA[MED_N] = {
    {3500.0f, 1.0f, 1.0f, 1.0f, 1.0f, 40.0f, 0.86f},     /* cassette */
    {7000.0f, 0.5f, 0.6f, 0.3f, 1.3f, 30.0f, 0.88f},     /* reel */
    {2200.0f, 1.4f, 1.6f, 1.5f, 1.6f, 300.0f, 0.80f},    /* dictaphone */
    {5000.0f, 0.3f, 0.8f, 2.0f, 0.8f, 60.0f, 0.90f},     /* VHS */
    {1800.0f, 2.0f, 2.5f, 2.5f, 2.0f, 500.0f, 0.78f},    /* wax cylinder */
    {16000.0f, 0.0f, 0.0f, 0.0f, 0.3f, 20.0f, 0.90f},    /* clean */
};
/* the instruments: a two-operator FM recipe */
typedef struct { float ratio, idx, fb, a_ms, d_ms, s, r_ms, md_ms, ms; uint8_t pair; } instr_t;
static const instr_t INSTR[INS_N] = {
    {1.0f, 0.9f, 0.12f, 3.0f, 1600.0f, 0.0f, 500.0f, 220.0f, 0.15f, 0},    /* felt piano */
    {3.5f, 1.5f, 0.0f, 1.0f, 900.0f, 0.0f, 350.0f, 70.0f, 0.0f, 0},        /* music box */
    {1.0f, 0.35f, 0.3f, 700.0f, 2500.0f, 0.75f, 1800.0f, 3000.0f, 0.3f, 1},/* pad */
    {2.0f, 0.22f, 0.45f, 400.0f, 2000.0f, 0.9f, 1400.0f, 2000.0f, 0.2f, 1},/* choir */
    {1.0f, 0.75f, 0.32f, 1.0f, 800.0f, 0.0f, 250.0f, 140.0f, 0.1f, 0},     /* guitar */
    {2.0f, 0.5f, 0.0f, 12.0f, 400.0f, 1.0f, 90.0f, 400.0f, 0.5f, 0},       /* organ */
};

/* ------------------------------------------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_ARM, C_KEY, C_COMMIT, C_CLEAR, C_SPLICE, C_MASTER, C_SEED, C_PANIC, C_CLOCK };
static h_cmdq_t cq;
void en_set(int p, int v) { h_post(&cq, C_SET, p, v); }
void en_play(int on) { h_post(&cq, C_PLAY, on, 0); }
void en_arm(int on) { h_post(&cq, C_ARM, on, 0); }
void en_key(int note, int vel, int on) { h_post(&cq, C_KEY, note, on ? vel : 0); }
void en_commit(void) { h_post(&cq, C_COMMIT, 0, 0); }
void en_clear(void) { h_post(&cq, C_CLEAR, 0, 0); }
void en_splice(int i, int on) { h_post(&cq, C_SPLICE, i, on); }
void en_seed(uint32_t s) { h_post(&cq, C_SEED, 0, (int32_t)(s & 0x7FFFFFFFu)); }
void en_panic(void) { h_post(&cq, C_PANIC, 0, 0); }
void en_clock(int msg) { h_post(&cq, C_CLOCK, msg, 0); }

static en_note_t shadow[EN_MAX_NOTES];                /* a master handed over by the UI */
static volatile uint8_t shadow_n, shadow_steps;
static volatile uint32_t shadow_seed, shadow_splices;
void en_master_load(const en_note_t *notes, int n, int loop_steps, uint32_t seed, uint32_t splices)
{
    int i;
    if (n > EN_MAX_NOTES)
        n = EN_MAX_NOTES;
    for (i = 0; i < n; i++)
        shadow[i] = notes[i];
    shadow_n = (uint8_t)n;
    shadow_steps = (uint8_t)loop_steps;
    shadow_seed = seed;
    shadow_splices = splices;
    H_BARRIER();
    h_post(&cq, C_MASTER, 0, 0);
}

/* --------------------------------------------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_armed, en_recording, en_age, en_loop_steps = 16, en_dropout, en_naged, en_nalive,
    en_nmaster, en_splice_glow, en_ext;
volatile float en_pos, en_pos2, en_level, en_speed = 1.0f;
volatile en_aged_t en_aged[EN_MAX_NOTES];
volatile en_note_t en_master[EN_MAX_NOTES];
volatile uint32_t en_master_ver, en_splices, en_seed_now = 0x5EED5u, en_steps_total;

static int16_t par[P_N];
static h_rng_t rng;                                     /* the render's own dice (dropouts, speedups) */
static float step_len = 11025.0f;                       /* samples per 1/8 at the tempo */
static float speed_oct = 1.0f;                          /* 2^P_SPEED */
static float sag, sag_t, sag_k;                         /* the motor's drift, in semitones */
static float burst, burst_left;                         /* a speedup: its factor, samples left */
static h_wow_t wow;
static float flutter_ph;
static uint32_t passes;                                 /* passes of the loop since the last generation */
static int last_step = -1, last_step2 = -1;
static float pos, pos2;                                 /* the playheads, in steps */
static int rev_base = -1, rev_until = -1;               /* a reverse splice's window */
static int rec_start_pending;                           /* armed on an empty master: the first key starts the pass */
static int32_t rec_end_abs = -1;                        /* recording ends at this absolute step */
static uint8_t key_start[128];                          /* a held key's start step + 1 (recording), 0 none */
static int ext_ticks;
static uint32_t ext_age;

typedef struct {
    h_fm_t fm, fm2;                                     /* fm2: the pair voice (pads, choir), detuned */
    float pan, lp_k, ghost;
    h_lp1_t lp;
    int32_t off_at;                                     /* the absolute step the gate closes at, -1 held by a key */
    uint8_t note, tape, on, age_n;
    uint32_t born;
} voice_t;
static voice_t vc[EN_NVOICES];
static uint32_t voice_n;
static int32_t abs_step, abs_step2;                     /* steps since the start, for the gates */

static h_plate_t plate;
H_PLATE_BUFFERS(pl);
static h_bed_t bed, hiss;
static h_lp1_t hf_l, hf_r, hp_l, hp_r, lvl_lp;
static h_lim_t lim = {0.0f, 1.0f};
static h_dc_t dcl, dcr;
static float drop_g = 1.0f, drop_t = 1.0f, drop_left;
static float rev_send, hf_cut = 16000.0f, drive = 1.0f, hiss_lvl;
static float master_trim = 0.55f;

/* --------------------------------------------------------------------------------------------- the master --- */
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

static uint32_t pool_abs(void) { return (uint32_t)H_SCALE[par[P_SCALE]]; }

/* the aged set at age N: the master's notes with their fates, and what grew; a function of (master, seed, N) */
static void rebuild(int age)
{
    static uint8_t alive[EN_MAX_NOTES];                   /* the master's: 1 sounding, 0 a ghost */
    static en_aged_t grown[EN_MAX_NOTES];
    int ngrown = 0, i, g, s, n = en_nmaster, steps = en_loop_steps, nalive = 0, k;
    float ero = fm_powf((float)par[P_EROSION] * 0.01f, 1.5f) * 0.45f;
    float grow = (float)par[P_REGROWTH] * 0.01f * 0.14f;
    float mem = (float)par[P_MEMORY] * 0.01f;
    int every = par[P_EVERY], fade = par[P_RETURN];
    for (i = 0; i < n; i++)
        alive[i] = 1;
    for (g = 1; g <= age; g++) {
        h_rng_t r;
        uint8_t taken[EN_MAX_STEPS];
        r.s = hash32(en_seed_now, (uint32_t)g) | 1u;
        for (i = 0; i < n; i++)                         /* erosion: the living drop */
            if (alive[i] && h_chance(&r, ero))
                alive[i] = 0;
        for (i = 0; i < ngrown; i++)
            if (h_chance(&r, ero * 1.3f)) {              /* what grew is frailer */
                grown[i] = grown[--ngrown];
                i--;
            }
        if (mem > 0.0f && every > 0) {                   /* memory: the master's ghosts come back */
            float p = fade ? mem / (float)every : (g % every == 0 ? mem : 0.0f);
            if (p > 0.0f)
                for (i = 0; i < n; i++)
                    if (!alive[i] && h_chance(&r, p))
                        alive[i] = 1;
        }
        for (s = 0; s < steps; s++)
            taken[s] = 0;
        for (i = 0; i < n; i++)
            if (alive[i])
                taken[en_master[i].step % steps] = 1;
        for (i = 0; i < ngrown; i++)
            taken[grown[i].n.step % steps] = 1;
        for (s = 0; s < steps && n + ngrown < EN_MAX_NOTES; s++) {   /* regrowth: a note in a hole */
            int near = -1, best = 1000, j;
            float p;
            if (taken[s])
                continue;
            for (j = 0; j < n; j++) {                    /* the nearest living neighbour, in steps */
                int d = (int)en_master[j].step - s;
                if (d < 0)
                    d = -d;
                if (d > steps / 2)
                    d = steps - d;
                if (alive[j] && d < best) {
                    best = d;
                    near = en_master[j].note;
                }
            }
            for (j = 0; j < ngrown; j++) {
                int d = (int)grown[j].n.step - s;
                if (d < 0)
                    d = -d;
                if (d > steps / 2)
                    d = steps - d;
                if (d < best) {
                    best = d;
                    near = grown[j].n.note;
                }
            }
            p = grow * (near >= 0 ? 1.0f : 0.3f);
            if (!h_chance(&r, p))
                continue;
            k = near >= 0 ? near + (h_irand(&r, 7) - 3) : 60 + h_irand(&r, 12);
            k = h_snap(pool_abs(), par[P_ROOT], k < 36 ? 36 : k > 96 ? 96 : k);
            grown[ngrown].n.step = (uint8_t)s;
            grown[ngrown].n.len = (uint8_t)(1 + h_irand(&r, 2));
            grown[ngrown].n.note = (uint8_t)k;
            grown[ngrown].n.vel = (uint8_t)(34 + h_irand(&r, 36));
            grown[ngrown].state = NS_GROWN;
            grown[ngrown].gen = (uint8_t)g;
            ngrown++;
            taken[s] = 1;
        }
    }
    for (i = 0; i < n; i++) {
        en_aged[i].n = en_master[i];
        en_aged[i].state = alive[i] ? NS_MASTER : NS_GHOST;
        en_aged[i].gen = 0;
        nalive += alive[i];
    }
    for (i = 0; i < ngrown; i++)
        en_aged[n + i] = grown[i];
    nalive += ngrown;
    H_BARRIER();
    en_naged = (uint8_t)(n + ngrown);
    en_nalive = (uint8_t)nalive;
}

static void master_changed(void)
{
    en_master_ver++;
    rebuild(en_age);
}

static void commit(void)                                /* the aged set is the master now; age 0 */
{
    int i, n = 0;
    for (i = 0; i < en_naged && n < EN_MAX_NOTES; i++)
        if (en_aged[i].state != NS_GHOST)
            en_master[n++] = en_aged[i].n;
    en_nmaster = (uint8_t)n;
    en_seed_now = hash32(en_seed_now, 0xC0FFEEu);
    par[P_AGE] = 0;
    en_age = 0;
    master_changed();
}

static void clear(void)
{
    en_nmaster = 0;
    par[P_AGE] = 0;
    en_age = 0;
    master_changed();
}

/* ---------------------------------------------------------------------------------------------- the tape --- */
static void tempo_set(void) { step_len = H_SR * 60.0f / ((float)par[P_TEMPO] * 2.0f); }

static void age_colour(void)                            /* the tape's wear at this age */
{
    const medium_t *m = &MEDIA[par[P_MEDIUM]];
    float a = (float)en_age * (1.0f / 64.0f), loss = (float)par[P_HFLOSS] * 0.01f;
    hf_cut = h_lerp(16000.0f, m->min_hf, fm_clampf(a * loss * 1.6f, 0.0f, 1.0f));
    drive = 1.0f + (float)par[P_SAT] * 0.01f * m->sat * (0.4f + 1.6f * a);
    hiss_lvl = (float)par[P_DUST] * 0.01f * m->hiss * (0.06f + 0.25f * a);
    bed.level = (float)par[P_DUST] * 0.01f * (0.25f + 0.45f * a);
    bed.density = 0.4f + 0.6f * a;
}

static void apply(int p, int v)
{
    const medium_t *m;
    par[p] = (int16_t)v;
    switch (p) {
    case P_TEMPO: tempo_set(); break;
    case P_SPEED: speed_oct = fm_exp2f((float)v); break;
    case P_LOOPBARS:
        en_loop_steps = (uint8_t)(v * 8);
        rebuild(en_age);
        break;
    case P_AGE:
        en_age = (uint8_t)v;
        passes = 0;
        rebuild(en_age);
        age_colour();
        break;
    case P_EROSION: case P_REGROWTH: case P_MEMORY: case P_EVERY: case P_RETURN: case P_ROOT: case P_SCALE:
        rebuild(en_age);
        break;
    case P_MEDIUM: case P_HFLOSS: case P_SAT: case P_DUST:
        m = &MEDIA[par[P_MEDIUM]];
        h_plate_set(&plate, m->decay * (0.9f + 0.1f * (float)par[P_REVERB] * 0.01f), par[P_MEDIUM] == MED_CLEAN ? 0.2f : 0.5f, 0.6f, 12.0f);
        age_colour();
        break;
    case P_REVERB:
        rev_send = (float)v * 0.01f;
        rev_send *= rev_send;
        m = &MEDIA[par[P_MEDIUM]];
        h_plate_set(&plate, m->decay * (0.9f + 0.1f * (float)v * 0.01f), par[P_MEDIUM] == MED_CLEAN ? 0.2f : 0.5f, 0.6f, 12.0f);
        break;
    case P_BED:
        bed.type = (uint8_t)v;
        break;
    case P_WOBBLE:
        wow.depth = (float)v * 0.01f * MEDIA[par[P_MEDIUM]].wow;
        break;
    case P_SAGRATE:
        sag_k = 1.0f / (H_SR * (300.0f - 2.9f * (float)v));   /* 5 min .. 10 s to a new target */
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------------------------------------- voices --- */
static float ms_curve(int v, float lo, float hi) { return lo * fm_exp2f((float)v * 0.01f * fm_log2f(hi / lo)); }

static voice_t *voice_alloc(void)
{
    int i, best = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    for (i = 0; i < EN_NVOICES; i++)
        if (!h_env_busy(&vc[i].fm.amp))
            return &vc[i];
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].born < oldest) {
            oldest = vc[i].born;
            best = i;
        }
    return &vc[best];
}

static void voice_on(int note, float vel, int tape, int32_t off_at, int ghost, int reverse)
{
    const instr_t *in = &INSTR[par[P_INSTR]];
    voice_t *v = voice_alloc();
    float bright = (float)par[P_BRIGHT] * 0.01f, dec = (float)par[P_DECAY] * 0.01f, det = (float)par[P_DETUNE] * 0.01f;
    float a_ms = reverse ? 600.0f : ms_curve(par[P_ATTACK], 1.0f, 2500.0f) * (in->a_ms > 50.0f ? in->a_ms / 50.0f : 1.0f);
    float r_ms = ms_curve(par[P_RELEASE], 40.0f, 4000.0f) * (in->r_ms / 400.0f);
    float d_ms = in->d_ms * (0.3f + 1.7f * dec), md = in->md_ms * (0.4f + 1.2f * bright);
    float f = h_mtof((float)note);
    v->note = (uint8_t)note;
    v->tape = (uint8_t)tape;
    v->off_at = off_at;
    v->born = ++voice_n;
    v->on = 1;
    v->ghost = ghost ? (float)par[P_GHOST] * 0.01f * 0.4f : 1.0f;
    v->pan = (float)par[P_WIDTH] * 0.01f * (((note * 7 + tape * 5) & 1) ? 0.8f : -0.8f);
    v->lp_k = h_lp1_k(h_lerp(1200.0f, 12000.0f, (float)par[P_TONE] * 0.01f) * (ghost ? 0.35f : 1.0f) * (tape ? 0.5f : 1.0f));
    v->fm.ratio = in->ratio;
    v->fm.idx = in->idx * (0.4f + 1.2f * bright) * (0.5f + 0.5f * vel);
    v->fm.fb = in->fb;
    v->fm.det = 1.0f;
    h_env_set(&v->fm.amp, a_ms, d_ms, in->s, r_ms);
    h_env_set(&v->fm.mod, 0.5f, md, in->ms, r_ms);
    h_fm_on(&v->fm, f, vel * v->ghost);
    if (in->pair) {
        v->fm2 = v->fm;
        v->fm2.det = 1.0f + 0.004f * det + 0.001f;
        v->fm.det = 1.0f - 0.004f * det;
        h_fm_on(&v->fm2, f, vel * v->ghost);
    } else {
        v->fm2.amp.st = H_ENV_OFF;
        v->fm2.amp.v = 0.0f;
    }
}

static void voice_off_note(int note)                    /* a key up: its held voice goes */
{
    int i;
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].on && vc[i].off_at == -1 && vc[i].note == note) {
            h_env_off(&vc[i].fm.amp);
            h_env_off(&vc[i].fm2.amp);
            vc[i].on = 0;
        }
}

static void gates(int32_t now, int tape)
{
    int i;
    for (i = 0; i < EN_NVOICES; i++)
        if (vc[i].on && vc[i].tape == tape && vc[i].off_at >= 0 && now >= vc[i].off_at) {
            h_env_off(&vc[i].fm.amp);
            h_env_off(&vc[i].fm2.amp);
            vc[i].on = 0;
        }
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < EN_NVOICES; i++) {
        h_env_off(&vc[i].fm.amp);
        h_env_off(&vc[i].fm2.amp);
        vc[i].on = 0;
    }
}

static void quiet(void)
{
    int i;
    for (i = 0; i < EN_NVOICES; i++) {
        vc[i].fm.amp.st = vc[i].fm2.amp.st = H_ENV_OFF;
        vc[i].fm.amp.v = vc[i].fm2.amp.v = 0.0f;
        vc[i].on = 0;
    }
    H_PLATE_INIT(&plate, pl);
    apply(P_REVERB, par[P_REVERB]);
}

/* ------------------------------------------------------------------------------------------- the playheads --- */
static int splice_step(int i) { return (int)(((uint32_t)i * (uint32_t)en_loop_steps + 5u) / 11u) % en_loop_steps; }

static void fire_step(int st, int tape, int32_t now)   /* the notes at step st start */
{
    int i, play_st = st, reverse = 0;
    if (!tape && rev_base >= 0 && st >= rev_base && st < rev_until) {
        play_st = (rev_base + rev_until - 1 - st + en_loop_steps) % en_loop_steps;
        reverse = 1;
    }
    for (i = 0; i < en_naged; i++) {
        const volatile en_aged_t *a = &en_aged[i];
        int ghost = a->state == NS_GHOST;
        if (a->n.step % en_loop_steps != play_st)
            continue;
        if (ghost && par[P_GHOST] == 0)
            continue;
        if (tape) {                                    /* tape 2: thinned, an octave or two down, no ghosts */
            int n;
            if (ghost || ((a->n.step * 7u + a->n.note) & 1u))
                continue;
            n = a->n.note + 12 * par[P_T2OCT];
            voice_on(n < 24 ? n + 12 : n, (float)a->n.vel * (1.0f / 127.0f) * (float)par[P_T2LEVEL] * 0.01f, 1,
                     now + a->n.len, 0, 0);
        } else {
            voice_on(a->n.note, (float)a->n.vel * (1.0f / 127.0f), 0, now + a->n.len, ghost, reverse);
        }
    }
}

static void pass_done(void)                            /* the loop came round */
{
    if (++passes >= (uint32_t)par[P_GENPASS] && en_nmaster) {
        passes = 0;
        if (en_age < 64) {
            en_age++;
            par[P_AGE] = (int16_t)en_age;
            rebuild(en_age);
            age_colour();
        }
    }
}

static void advance(float dpos)
{
    int st, i;
    float ratio = par[P_RATIO] == 0 ? 1.5f : par[P_RATIO] == 1 ? 2.0f : 3.0f;
    pos += dpos;
    while (pos >= (float)en_loop_steps) {
        pos -= (float)en_loop_steps;
        last_step = -1;
        rev_base = rev_until = -1;
        pass_done();
    }
    st = (int)pos;
    if (st != last_step) {
        last_step = st;
        abs_step++;
        en_steps_total++;
        gates(abs_step, 0);
        if (en_recording && rec_end_abs >= 0 && abs_step >= rec_end_abs) {   /* the recording pass is over */
            en_recording = 0;
            rec_end_abs = -1;
            master_changed();
        }
        for (i = 0; i < 11; i++)                       /* a splice here? */
            if ((en_splices >> i & 1u) && splice_step(i) == st && en_naged) {
                en_splice_glow = (uint8_t)(i + 1);
                if (par[P_SPLICE] == SPL_REPEAT) {
                    if (st >= 2 && rev_base != st) {       /* back a beat, once (rev_base marks it done) */
                        pos -= 2.0f;
                        rev_base = st;
                        rev_until = st;
                        st = (int)pos;
                        last_step = st;
                    }
                } else if (par[P_SPLICE] == SPL_SKIP) {
                    pos += 2.0f;
                    if (pos >= (float)en_loop_steps)
                        pos -= (float)en_loop_steps;
                    st = (int)pos;
                    last_step = st;
                } else {
                    rev_base = st;
                    rev_until = st + 2 > en_loop_steps ? en_loop_steps : st + 2;
                }
                break;
            }
        fire_step(st, 0, abs_step);
    }
    if (par[P_TAPE2]) {
        pos2 += dpos / ratio;
        while (pos2 >= (float)en_loop_steps) {
            pos2 -= (float)en_loop_steps;
            last_step2 = -1;
        }
        st = (int)pos2;
        if (st != last_step2) {
            last_step2 = st;
            abs_step2++;
            gates(abs_step2, 1);
            fire_step(st, 1, abs_step2);
        }
    }
    en_pos = pos;
    en_pos2 = pos2;
}

static void play_start(void)
{
    pos = pos2 = 0.0f;
    last_step = last_step2 = -1;
    rev_base = rev_until = -1;
    passes = 0;
    en_playing = 1;
}

static void play_stop(void)
{
    en_playing = 0;
    en_recording = 0;
    rec_start_pending = 0;
    notes_off();
}

/* a key: played now; recorded into the master while recording (its step on the grid, its length on release) */
static void key(int note, int vel)
{
    if (note < 0 || note > 127)
        return;
    if (vel) {
        if (en_armed && !en_recording) {               /* the first key starts the recording pass */
            en_armed = 0;
            en_recording = 1;
            if (!en_playing || rec_start_pending) {
                play_start();
                rec_start_pending = 0;
            }
            rec_end_abs = abs_step + en_loop_steps;
            passes = 0;
        }
        if (en_recording && en_nmaster < EN_MAX_NOTES) {
            int q = par[P_QUANT] ? 2 : 1;
            int st = (int)((pos + (float)q * 0.5f) / (float)q) * q;
            key_start[note] = (uint8_t)(st % en_loop_steps + 1);
        }
        voice_on(note, (float)vel * (1.0f / 127.0f), 0, -1, 0, 0);
    } else {
        voice_off_note(note);
        if (en_recording && key_start[note] && en_nmaster < EN_MAX_NOTES) {
            int st = key_start[note] - 1, len = (int)(pos - (float)st + 0.5f);
            if (len < 0)
                len += en_loop_steps;
            if (len < 1)
                len = 1;
            if (len > en_loop_steps)
                len = en_loop_steps;
            en_master[en_nmaster].step = (uint8_t)st;
            en_master[en_nmaster].len = (uint8_t)len;
            en_master[en_nmaster].note = (uint8_t)note;
            en_master[en_nmaster].vel = (uint8_t)(vel ? vel : 100);
            en_nmaster++;
            en_master_ver++;
        }
        key_start[note] = 0;
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
            en_armed = (uint8_t)(c.a != 0);
            if (en_armed && !en_playing)
                rec_start_pending = 1;
            break;
        case C_KEY: key(c.a, (int)c.b); break;
        case C_COMMIT: commit(); break;
        case C_CLEAR: clear(); break;
        case C_SPLICE:
            if (c.b)
                en_splices |= 1u << c.a;
            else
                en_splices &= ~(1u << c.a);
            break;
        case C_MASTER: {
            int i;
            for (i = 0; i < shadow_n; i++)
                en_master[i] = shadow[i];
            en_nmaster = shadow_n;
            if (shadow_steps >= 8 && shadow_steps <= EN_MAX_STEPS)
                en_loop_steps = shadow_steps;
            en_seed_now = shadow_seed ? shadow_seed : 0x5EED5u;
            en_splices = shadow_splices;
            master_changed();
            break;
        }
        case C_SEED:
            en_seed_now = (uint32_t)c.b | 1u;
            rng.s = en_seed_now * 2654435761u;
            rebuild(en_age);
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
                    advance(1.0f / 12.0f);
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
    wow.rng.s = 0x1234567u;
    wow.depth = 0.35f;
    h_bed_init(&bed, 0x777u);
    h_bed_init(&hiss, 0x999u);
    hiss.type = H_BED_HISS;
    hiss.level = 1.0f;
    H_PLATE_INIT(&plate, pl);
    en_loop_steps = (uint8_t)(par[P_LOOPBARS] * 8);
    for (i = 0; i < P_N; i++)
        apply(i, par[i]);
    rebuild(0);
}

static void control(uint32_t n)                         /* once a block: the slow things */
{
    float fn = (float)n;
    const medium_t *m = &MEDIA[par[P_MEDIUM]];
    /* the motor: sag drifts to a target over minutes; a speedup now and then */
    sag += (sag_t - sag) * sag_k * fn;
    if (h_chance(&rng, fn * sag_k * 0.5f))
        sag_t = h_brand(&rng) * (float)par[P_SAG] * 0.01f * 0.9f;   /* up to about a semitone */
    if (burst_left > 0.0f) {
        burst_left -= fn;
        if (burst_left <= 0.0f)
            burst = 0.0f;
    } else if (par[P_SPEEDUP] && h_chance(&rng, fn * (float)par[P_SPEEDUP] * 0.01f / (H_SR * 40.0f))) {
        burst = 0.015f + 0.03f * h_frand(&rng);          /* a brief hurry, 1 to 3 s */
        burst_left = H_SR * (1.0f + 2.0f * h_frand(&rng));
    }
    /* dropouts: more with age and the medium */
    if (drop_left > 0.0f) {
        drop_left -= fn;
        if (drop_left <= 0.0f)
            drop_t = 1.0f;
    } else if (par[P_DROPOUT] && h_chance(&rng, fn * (float)par[P_DROPOUT] * 0.01f * m->drop * (0.02f + (float)en_age * 0.02f) / H_SR)) {
        drop_t = par[P_MEDIUM] == MED_VHS ? 0.0f : 0.05f + 0.25f * h_frand(&rng);
        drop_left = H_SR * (0.03f + 0.25f * h_frand(&rng));
    }
    en_dropout = drop_t < 0.9f;
    if (en_ext && ++ext_age > 60u)
        en_ext = 0;
    if (en_splice_glow && (abs_step % 4) == 3)
        en_splice_glow = 0;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float g = h_gain(gain_q12, master_trim);
    const medium_t *m;
    float hf_k, hp_k, pitch, dpos, sagf, peak = 0.0f;
    uint32_t i;
    int v;
    drain();
    control(n);
    m = &MEDIA[par[P_MEDIUM]];
    hf_k = h_lp1_k(hf_cut);
    hp_k = h_lp1_k(m->hp);
    sagf = h_semis(sag) * (1.0f + burst);
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, x, rl, rr, bl, br, hl, hr, w;
        /* the tape's speed this sample: octaves, the motor's drift, wow and flutter */
        w = h_wow(&wow);
        if ((i & 7u) == 0u)
            flutter_ph = h_wrap(flutter_ph + 8.0f * 7.3f * H_ISR);
        pitch = speed_oct * sagf * (w + 0.0025f * (float)par[P_FLUTTER] * 0.01f * h_sin(flutter_ph) * m->wow);
        en_speed = pitch;
        if (en_playing && !en_ext) {
            dpos = pitch / step_len;
            advance(dpos);
        }
        for (v = 0; v < EN_NVOICES; v++) {
            voice_t *vo = &vc[v];
            float s;
            if (!h_env_busy(&vo->fm.amp))
                continue;
            vo->fm.freq = h_mtof((float)vo->note) * pitch;
            s = h_fm(&vo->fm);
            if (h_env_busy(&vo->fm2.amp)) {
                vo->fm2.freq = vo->fm.freq;
                s = (s + h_fm(&vo->fm2)) * 0.7f;
            }
            s = h_lp1(&vo->lp, s, vo->lp_k) * 0.5f;
            l += s * (1.0f - vo->pan * 0.5f);
            r += s * (1.0f + vo->pan * 0.5f);
        }
        /* the tape: saturation, the highs lost to age, the medium's low cut, a dropout */
        drop_g += (drop_t - drop_g) * 0.002f;
        l = h_lp1(&hf_l, h_sat(l, drive), hf_k);
        r = h_lp1(&hf_r, h_sat(r, drive), hf_k);
        l = h_hp1(&hp_l, l, hp_k) * drop_g;
        r = h_hp1(&hp_r, r, hp_k) * drop_g;
        x = (l + r) * 0.5f;
        h_plate(&plate, x * rev_send, &rl, &rr);
        h_bed(&bed, &bl, &br);
        h_bed(&hiss, &hl, &hr);
        l += rl + bl + hl * hiss_lvl;
        r += rr + br + hr * hiss_lvl;
        l = h_dc(&dcl, l);
        r = h_dc(&dcr, r);
        h_lim(&lim, &l, &r);
        if (fm_fabsf(l) > peak)
            peak = fm_fabsf(l);
        h_out(out + 2 * i, l, r, g);
    }
    en_level = h_lp1(&lvl_lp, peak, 0.3f);
}
