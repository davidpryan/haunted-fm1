# MIDNIGHT ROUTE

The arrangement engine. The song has a shape before it has notes: you compose the route, the machine fills in the
journey.

## In a minute

1. Tap **PLAY**. The route departs from station 1. Under the rain, a sub and a thump; that is a *Midnight* route's
   long intro.
2. Watch the map. Sixteen blocks are the sixteen **stations** of the route; the four lanes are how dense each part is
   there: **Sub** (blue), **Pad** (violet), **Voice** (amber), **Rain** (teal). The bright line is where you are. The
   header counts down what is left of the route (12 minutes by default).
3. Turn **SELECT**. You jump to another station, and the music goes there within two bars: the pad arrives, the voice
   sings at the peak, everything thins out on the long way down. The route is the composition; the notes are
   disposable.
4. Turn **ALGORITHM**. Another **shape**: an arch, a slow burn, a collapse, waves, a dawn (nothing, then everything,
   then gone). That changes the next twenty minutes, not the next bar.
5. Tap a **white key**. The station gets a tag: **DWELL** (twice as long), **SKIP** (left out), **PEAK** (forced to
   the ceiling), then normal again. Hold a key and the station's four numbers show. The **black keys** are the chord
   palette: which of eleven functions the harmony may use.
6. Tap **OCT−** a few times. The whole route goes down, but the voice keeps its throat: that is the pitched-down ghost.
7. Tap **SAVE** to keep the route.

## The panel

| Control | What it does |
|---|---|
| White keys 1–16 | The stations. A tap cycles the tag: normal, **DWELL**, **SKIP**, **PEAK**. Held: the station's four densities on the screen. The key of the station you are at is lit; tagged stations are dim. |
| Black keys | The **chord palette**: i, iv, v, VI, III, bII, ii°, VII, iv add9, v sus, open fifths. Lit when allowed; off means forbidden everywhere. With none allowed the harmony stays on i. |
| PLAY | **Depart** or **pause**. Held: from station 1 again, with a fade. |
| REC | **Bookmark** the station you are at: its material comes back identically on the next pass of the route. Held, then OCT+: clear every bookmark. |
| SELECT | **Position on the route**: the station. HOME held + SELECT: the route's length in minutes. |
| ALGORITHM | The **shape**: Arch, Slow burn, Collapse, Waves, Midnight (long intro, brief peak, long tail), Dawn, Flat (no form). |
| PRESETS | The **city**: a sound world. South London, Estuary, Night bus, Tower, Harbour, Underpass, Last train, Fog lane. Each brings its root, scale, bed and tempo (which you may change after) and its own pad, sub, voice and percussion. |
| OCT− / OCT+ | **Transpose** the whole route, ±12 semitones. The voice's formants are held, so down is a ghost and up is thin. |
| SAVE | Saves the route (tags, palette, bookmarks) and every setting. A dot in the header means there is something unsaved. |
| HOME ENV LFO SEQ ARP FX GLO SEL EDIT | The pages below. KNOB 1–4 set the page's four values. Hold any button for a second for a card that says what it does. |

## The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **Route** (HOME) | Minutes: the route's length, 4 to 30 | Ceiling: the densest any station may be | Contrast: how far the quiet stations fall below it | Fog: reverb size, pre-delay, damping and the bed, as one |
| **Density** (ENV) | Part: the part the other three edit | Level: the part's curve up or down | Slope: tilted towards the end or the start | Jitter: a little chance per station |
| **Drift** (LFO) | Drift: how far a part's density walks within a station | Rate: how fast | Timing: a spread on every event, up to 40 ms | Dynamics: a spread on every velocity |
| **Cells** (SEQ) | Sub: its rhythm cells | Pad: its cells | Voice: its cells | Rain: its cells |
| **Motif** (ARP) | Range: how far the voice wanders | Step: how big its steps are | Repeat: the chance a note is sung again | Rest: the chance a note is left out |
| **Station FX** (FX) | Filter: how far it closes as the route empties | Tail: how much longer the reverb rings on a DWELL station | Swell: the bed over a skipped station | Echo: the voice and the hats into the tempo echo |
| **Harmony** (GLO) | Change: a chord every 2, 4, 8 or 16 bars | Smooth: under 50 the pad is struck again at a change, above it glides | Bass: the chord's root, a pedal, or fifths | End: the route loops, or rests on the bed |
| **Key** (SEL) | Root | Scale | Bed: Rain, Crackle, Hiss, Wind, Room | Tempo |
| **Timbre** (EDIT) | Part | Bright | Decay | Width (the pad's detune and spread) |

Rhythm cells: *Held* is one event a bar (the pad's default: a chord held), *Pulse* quarters, *Half* half-time,
*2-step* the garage cell, *Swung* swung sixteenths, *Sparse* two or three hits in odd places, *Any* all sixteen.

## How the route is made

The shape is a curve through the sixteen stations. The ceiling caps it; the contrast pulls the quiet stations down
(at 0, every station is at the ceiling; at 100, the quiet ones are near silence). Each part reads the curve its own
way: the sub and the rain are there as soon as there is anything, the pad arrives when the curve passes a quarter,
the voice only above half. The Density page then offsets, tilts and jitters a part's own curve, and a PEAK tag forces
a station to the ceiling.

Every bar, every part asks its density how many events to place, draws a rhythm cell and puts notes on it. Below 20 %
a part is in *gesture* mode: one phrase placed somewhere in the bar with an eighth of jitter, and only sometimes, which
is what makes a sparse station sound played rather than programmed. The chord changes every few bars from the palette,
with the pad's four voices moving as little as they can; the voice sings chord tones on the strong steps and the
scale between them.

The material is a function of a seed, the pass, the station and the bar. A bookmark keeps a station's seed, so the
station comes back the same on the next pass while everything else is drawn again; Drift still breathes on top.

The effects read the route: the plate opens in the quiet stations and closes at the peak, the filter closes as the
density falls, the tail doubles on a DWELL station, the bed swells across a SKIP. Fog is the ceiling all of that moves
under, not a static level. The bed is always there, so a dropout sounds intended.

## The screen

The route map: sixteen station blocks, four density lanes in the part colours, the tags over them (two bars for
DWELL, a cross for SKIP, a peak for PEAK, a dot for a bookmark), a playhead. Under it: the station and its bar, the
chord sounding, and the time left on the route. The four parts light as they fire.

## MIDI

MIDI clock drives the bars while it keeps coming (the header says EXT); start and stop are followed. CC 20–23 are the
HOME knobs and CC 24 is the station, so an external controller can drive the route. The music goes out over USB
audio.
