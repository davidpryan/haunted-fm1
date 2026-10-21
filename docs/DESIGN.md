# Five Generative Firmwares for the M-VAVE FM-1

Haunting ambient in the Burial vein: pitched-down ghost vocals, vinyl crackle and rain, sub bass under detuned pads, swung off-grid percussion, long stretches where almost nothing happens, and arrangements that leave and come back changed.

Each firmware below is built around one organising idea so that the controls on the panel mean one coherent thing. The designs are deliberately different from the existing FM-1 ambient firmwares (AMB-1, MAKU, Pelucco, Tuna), which mostly generate a steady state you tend. These are about *what happens over time*.

---

## The hardware you are mapping to

| Control | Physical reality |
|---|---|
| MASTER | Volume pot. Left alone in every design. |
| ALGORITHM | Detented encoder, top left. The "big choice" in every design. |
| PRESETS | Detented encoder. Chooses a palette or library item. |
| SELECT | Detented encoder next to the screen. The "scrub" control in every design: it moves you along the piece's main axis. |
| KNOB 1–4 | Four pots, colour-matched to the screen. Always the four macros of the current page; the HOME page is the one that matters live. |
| 27 keys | 16 white, 11 black, capacitive. Playable, but also 16 toggles + 11 modifiers. |
| OCT− / OCT+ | Pair of buttons. Also confirm/cancel. |
| Top row | FX, SEL, ENV, LFO, EDIT, GLO. Tap = open page, hold = temporary layer over the keys and knobs. |
| Bottom row | HOME, SAVE, ARP, SEQ, PLAY, REC. |
| Screen | 240 × 240 colour. |
| Engine | JieLi AC791N, two pi32v2 cores, 1 MiB flash (full on most firmwares), 44.1 kHz fixed point, 8 voices on one core / 12 on two. USB-C audio in and out, TRS MIDI in, MIDI clock sync, speaker, battery. |

Shared conventions used by all five designs:

- **Every knob has a landmark.** Centre is "as written", fully left is "off / none", fully right is "too much". You can set a knob blind.
- **Every knob is audible within one bar.** Nothing on the HOME page changes a number you can't hear.
- **Knobs are musical nouns, not DSP nouns.** "Fog", not "reverb wet". The DSP mapping is documented but never on the panel.
- **SELECT is always the piece's main axis.** Position in the song, tape age, harmonic pressure, pitch, swing. It is the one encoder you can grab without looking.
- **Hold HOME shows the map.** Every button and key's current function drawn on the screen, as Skiff does.
- **REC always means "keep this."** Freeze, bookmark, or capture; never "destroy."
- **Nothing is ever silent by accident.** A bed (rain, crackle, hiss, room tone) is always running under the music unless you turn it off, so dropouts sound intentional.

---

## 1. MIDNIGHT ROUTE — the arrangement engine

**One idea:** the song has a shape before it has notes. You compose the route; the machine fills in the journey.

A piece is a *route* of 6–30 minutes made of 16 *stations* (sections). Each station has a density target for four parts: SUB (bass), PAD (detuned FM pad), VOICE (pitched fragment melody), RAIN (percussion and bed). The firmware draws a density envelope through the stations, then generates note material to meet that density. The notes are disposable; the shape is the composition.

### Controls

| Control | Role |
|---|---|
| ALGORITHM | **Route shape.** Arch · Slow Burn · Collapse · Waves · Midnight (long intro, brief peak, long tail) · Dawn (nothing, then everything, then gone) · Flat (no form, pure loop). Changes the density envelope template across all 16 stations. |
| PRESETS | **City.** A sound world: root, scale, four timbres, FX colour, bed type. Eight factory cities, eight user slots. |
| SELECT | **Position on the route.** Turn to jump to a station; the music crossfades there over 2 bars. While playing, the screen marker shows where you are. Hold HOME + SELECT = travel speed (how many minutes the route takes). |
| KNOB 1 | **Length.** Route duration, 4–30 min. Changes station durations proportionally. |
| KNOB 2 | **Ceiling.** Maximum density any station may reach. Low = the whole piece is sparse. |
| KNOB 3 | **Contrast.** How far quiet stations drop below the ceiling. High contrast = long near-silences. |
| KNOB 4 | **Fog.** Master wash: reverb size + pre-delay + high damping + bed level as one curve. |
| White keys 1–16 | **Stations.** Tap a key to cycle its tag: normal → DWELL (double length) → SKIP (omitted) → PEAK (forced to ceiling). Lit in the part colour of whichever part dominates. |
| Black keys | **Chord palette.** Toggle which of 11 chord functions the harmony may use (i, iv, v, VI, III, bII, ii°, VII, iv/add9, v sus, open 5ths). Off = forbidden everywhere. |
| OCT− / OCT+ | **Transpose the whole route** in semitones ±12. Because VOICE is formant-locked, going down produces the pitched-down ghost-vocal effect; going up thins it. |
| ENV | **Density envelopes.** Per part, draw the 16-station curve with knobs (part select / level / slope / jitter). |
| LFO | **Drift.** Slow random walk on each part's density within a station, so two passes never match. |
| SEQ | **Rhythm cells.** Per part, pick which of 16 rhythmic cells (2-step, half-time, held, pulse, swung 16ths, nothing) each station may draw from. |
| ARP | **Motif.** The VOICE part's melodic contour rules: range, step size, repetition, rest ratio. |
| FX | **Station FX.** Per-station overrides: filter closes on Collapse stations, reverb tail doubles on DWELL, bed swells on SKIP transitions. |
| GLO | **Harmony rules.** Chord change rate (every 2/4/8/16 bars), voice-leading smoothness, bass follows root or pedal. |
| SEL | **Root + scale.** |
| EDIT | **Timbre edit** for the selected part (FM6 operator page, as in Felucca). |
| PLAY | **Depart / pause.** Hold = restart from station 1 with a 1-bar fade. |
| REC | **Bookmark.** Snapshots the current station's generated material so it returns identically when the route loops. Hold = clear bookmarks (confirm with OCT+). |
| SAVE | Save route + city. Hold = save as new city preset. |
| HOME | Route map. |

### Form
The route *is* the form. A Midnight route: three stations of SUB + RAIN only, two stations where PAD arrives, one PEAK station where VOICE sings over everything, then nine stations of slow subtraction until only crackle is left. At route end, the piece either loops (with bookmarks) or stops and rests on the bed.

### Sequencing
Density-driven. Each bar, each part asks "how many events should I place?" from its envelope, then draws rhythmic cells and pitches from the station's allowed pools. Below 20 % density a part switches from grid to *gesture* mode: a single phrase placed somewhere in the bar with ±1/8 jitter, which is what makes sparse sections feel played rather than programmed.

### Effects
One reverb (Dattorro plate), one tempo delay, one bed generator (rain / crackle / hiss / room), per-part filter and drive. Effects are *station-aware*: the mix engine reads the route and automates them, so Fog isn't a static level but a ceiling the route moves under.

### Screen
A horizontal map: 16 station blocks, four density lanes drawn as filled curves in part colours, a playhead, and tag glyphs. Hold a key and the station expands to show its envelope values.

### Why the options feel meaningful
Turning ALGORITHM changes the next twenty minutes, not the next bar. SELECT lets you audition the form by scrubbing it. Station tags are the "arrangement edit" a producer would make in a DAW, done on 16 keys.

---

## 2. VOICEMAIL — the ghost-vocal fragment machine

**One idea:** a voice that almost says something. Burial's chopped, pitched vocal fragments, reproduced with a formant vocal engine and a *phrase* sequencer rather than a step grid.

The sound source is a formant synth (five formants + breath noise + glottal pulse, with a "pitch-down" mode that keeps formants fixed while pitch falls, which is exactly why Burial's slowed samples sound like ghosts). An optional capture mode records 1.5 s snippets from USB audio in into three RAM slots (flash is full; RAM only, lost on power-off unless SAVE moves one to the single flash slot).

A *phrase* is a 1–4 bar gesture: a contour (sigh, call, question, mantra, fall, held), a vowel sequence, and a breath pattern. The sequencer places phrases, not steps.

### Controls

| Control | Role |
|---|---|
| ALGORITHM | **Who is singing.** Child · Woman · Man · Choir (3 detuned) · Whisper (noise-dominant) · Slowed (man-at-half-speed formants) · Radio (band-limited) · Captured (uses the RAM sample slots). |
| PRESETS | **Phrase book.** Sets of eight phrase contours. Factory books: Hymn, Soul Ad-lib, Lullaby, Siren, Plea, Static, user ×2. |
| SELECT | **Pitch.** −24 to +12 semitones with formants held. Centre detent = natural. This is the single most Burial control on any of the five. |
| KNOB 1 | **Fragment.** How much of each phrase is heard: full phrase → half → a syllable → a consonant. Left = whole words, right = unrecognisable. |
| KNOB 2 | **Stutter.** Chance a fragment re-triggers 2–4 times at 1/16 or 1/32 before continuing. |
| KNOB 3 | **Reverse.** Chance a fragment plays backward; above 70 % also adds reverse-reverb pre-swell. |
| KNOB 4 | **Wash.** Reverb + 1/8-dot delay + pitch-shifted delay feedback (−12) as one curve. |
| White keys | **Sing.** Play a phrase at that pitch, quantised to the next 1/8. Holding REC and playing writes a melody the ghost will sing in sequence. |
| Black keys | **Vowel per step.** When held with a white key, override the vowel (a e i o u + three breath variants + 3 captured slots). |
| OCT− / OCT+ | **Register** of the pad bed underneath. |
| ENV | **Breath.** Phrase envelope: Swell (long attack, no release) ↔ Clip (gated, hard stop). Knobs: attack, decay to breath, breath level, consonant level. |
| LFO | **Tape.** Wow (0.3–2 Hz pitch wobble), flutter, dropout chance. |
| SEQ | **Placement.** The 4-bar phrase grid: where phrases may start (density mask), rest bars, repeat count. |
| ARP | **Bed.** The pad chord progression under the voice: four slots, each a chord from the scale, bars per chord. |
| FX | **Room.** Bed type (rain, crackle, both, none) and level, plus pre-reverb high-pass. |
| GLO | **Answer.** Rules for the second voice: delay in bars (1–8), transposition (unison, +5, −7, inversion), chance it answers at all, whether it overlaps or waits for silence. |
| SEL | **Scale.** Minor variants only by default (Aeolian, Dorian, Phrygian, harmonic minor); hold for all. |
| EDIT | **Formant edit.** Five formant frequencies/bandwidths, breath colour, glottal shape. |
| PLAY | Start / stop. |
| REC | **Capture.** Tap = arm melody-write mode on the keys. Hold 2 s = record 1.5 s from USB audio into the next RAM slot (screen shows a waveform filling). |
| SAVE | Save patch + phrase edits. Hold = commit one RAM slot to flash. |
| HOME | Live page: current phrase drawn as a contour, the Answer voice's countdown, pitch readout. |

### Form
A *conversation*: Call (voice 1 alone) → Answer (voice 2 responds per GLO rules) → Chorus (both overlap, Wash opens) → Silence (bed only, 2–8 bars, the pad holds). The firmware cycles this, with Silence lengths drawn from a distribution you set in SEQ. The pad progression runs underneath independently, so the voice re-enters over a different chord each cycle.

### Sequencing
Phrase placement with density mask + Answer logic. Each phrase is rendered from its contour against the current scale and chord, then chopped per KNOB 1 at render time, so the same phrase at different Fragment settings yields related but different results.

### Effects
Pitch-shift with formant hold (the core), stutter buffer, reverse buffer, plate reverb, pitched feedback delay, tape wow/flutter/dropout, rain/crackle bed.

### Screen
Centre: the phrase contour as a glowing line, scrubbed by the playhead. Below: the Answer countdown as a dimming bar. Pitch in large type.

### Why the options feel meaningful
SELECT alone takes a lullaby to a haunting. Fragment, Stutter and Reverse are the three edits Burial actually makes to a vocal. GLO's Answer rules create dialogue instead of repetition.

---

## 3. UNTRUE STEP — ghost garage without a grid

**One idea:** the haunting is in the timing. Burial placed his drums by ear, off the grid. This firmware has no step grid for percussion at all: every hit is an *event* with a mean time, a spread, and a drag.

Four percussion lanes (KICK, SNARE/CLAP, HAT/SHAKER, FOLEY) plus SUB and PAD. The kit is foley-flavoured FM/noise models: lighter click, vinyl tick, metallic snare, pitched shaker, bottle, key jangle. Hits are stored as time-in-milliseconds within a 2-bar cycle, not as step numbers.

### Controls

| Control | Role |
|---|---|
| ALGORITHM | **Groove.** 2-Step · Half-time · Broken · Dub step (kick + delay only) · Ghost 4×4 (kick barely there) · Pulse (one hit per bar) · None (drums leave). Loads a template of events; your edits survive switching because templates are stored as offsets from your pattern. |
| PRESETS | **Kit.** Eight foley kits + eight user. |
| SELECT | **Shuffle.** Centre = straight. Right = swing (late off-beats, up to 2-step triplet feel). Left = push (early, anxious). This bends event times, not step grids, so it works on any pattern. |
| KNOB 1 | **Loose.** Timing spread per hit: 0 ms → 40 ms Gaussian. Also widens velocity spread. |
| KNOB 2 | **Ghosts.** Density of quiet extra hits (−18 dB, filtered) placed between the main events. |
| KNOB 3 | **Duck.** Sidechain depth of PAD and bed to the KICK lane, with release in beats. |
| KNOB 4 | **Dust.** Crackle + rain bed, plus hi-hat high-pass. |
| White keys 1–16 | **Events.** In SEQ layer, the 16 keys are the 16 *eighth-notes* of a 2-bar cycle as *anchors*: tap to add an event near that anchor (it lands ±Loose ms away). Outside SEQ, white keys play the current lane live, and REC captures their timing unquantised. |
| Black keys | **Lane select** (1–6) and, held with a white key, **nudge** that event ±5 ms per tap. |
| OCT− / OCT+ | SUB octave. |
| ENV | **Decay** per lane: tight → long. Long FOLEY decay is the reverb-drenched snare. |
| LFO | **Drag.** Slow tempo breathing: the whole cycle speeds up and slows by ±2 % over 8–32 bars, like a tape machine. |
| SEQ | **Event editor.** Screen shows the 2-bar timeline with hits as dots; KNOB 1 selects a hit, KNOB 2 moves it in ms, KNOB 3 velocity, KNOB 4 probability. |
| ARP | **Pad progression.** Four chords, bars per chord, detune amount. |
| FX | **Sub + space.** Sub waveform/decay, send to tempo delay, delay feedback. |
| GLO | **Leave.** The drop-out rules: every N bars (8–64), chance the drums leave for M bars, and which lanes come back first. Also whether they come back *changed* (re-shuffled Loose, new ghosts). |
| SEL | Scale / root for SUB and PAD. |
| EDIT | Kit sound edit. |
| PLAY | Start / stop. |
| REC | **Tap in.** Hold and play white keys; hits are recorded with the exact timing you played, no quantise. Tap alone = toggle live-tap overdub. |
| SAVE | Save pattern + kit. |
| HOME | Live page: the 2-bar timeline scrolling with hits lighting as they fire. |

### Form
*The drums leave.* GLO governs a slow cycle: groove plays for 16–48 bars, then the drums leave and only PAD, SUB and Dust remain for 4–16 bars, then they return lane by lane. Because each return re-rolls Loose and Ghosts, the groove never comes back identical, which is the Burial trick of a drop that feels like a memory of the earlier one.

### Sequencing
Event list in ms, per lane, with per-event probability and a Gaussian jitter. Shuffle is a time-warp function applied at playback. Clock still syncs to MIDI; the warp is applied within each bar.

### Effects
Sidechain duck (the pumping pad), tempo delay with filter in feedback path, plate reverb on SNARE/FOLEY only, crackle/rain bed, per-lane high-pass.

### Screen
A two-bar timeline, six lanes stacked, hits as dots whose size is velocity and halo is Loose spread. In SEQ page the selected hit is enlarged and shows its ms offset.

### Why the options feel meaningful
Loose and Shuffle are things you cannot do on a step grid, and they are what make garage sound haunted rather than mechanical. GLO makes the drop-out a composed event, not a mute button.

---

## 4. HALF-LIGHT — harmonic weather

**One idea:** harmony as weather. No grid at all; time is in seconds. Chords move by voice-leading under a pressure system that builds and releases.

Up to 8 pad voices (one core) or 12 (both cores) hold a chord. Every T seconds, some voices move to the nearest scale tone that forms the next chord in a Markov chain. *Pressure* governs how far from the tonic the chain may wander and how dissonant the chords get. Pressure climbs on its own and breaks in a *front*: a dense, dissonant passage that resolves to a thin consonant one.

### Controls

| Control | Role |
|---|---|
| ALGORITHM | **Climate.** Still (pressure barely moves) · Front (regular build and break) · Storm (frequent, large breaks) · Clearing (one long resolution) · Fog (pressure wanders, never resolves) · Manual (pressure only from SELECT). Each is a transition matrix + pressure curve. |
| PRESETS | **Voice.** Pad timbre set: Glass FM, Detuned Saw-ish FM, Bowed, Organ, Choir-formant, Tape Strings, user ×2. |
| SELECT | **Pressure.** Turn to push the harmony toward or away from home. In Manual climate this is the whole instrument. In others it offsets the automatic curve. |
| KNOB 1 | **Breath.** Seconds between changes, 3 s → 120 s. |
| KNOB 2 | **Voices.** 2 → 8 (12 with dual core). Fewer voices = open fifths; more = thick clusters. |
| KNOB 3 | **Movement.** How many voices change per step: 1 (smoothest voice leading) → all (block chords). |
| KNOB 4 | **Depth.** Reverb size + feedback + a touch of pitched (−12) feedback delay. |
| White keys | **Pins.** Tap a key to pin that note: it is held by one voice forever and never moves. Tap again to release. A pedal tone under weather is the drone. |
| Black keys | **Bells.** Toggle a scale degree as a bell: a sparse, high FM bell on that degree fires with low probability, timed in seconds not beats. |
| OCT− / OCT+ | Register of the whole field. |
| ENV | **Swell.** Crossfade shape when a voice moves: hard cut → equal-power → long overlap (both notes sound together, the "smear"). |
| LFO | **Beat.** Detune drift between voices in cents, with a slow random walk, so chords shimmer. |
| SEQ | **Bells schedule.** Mean seconds between bells, spread, max simultaneous, probability it echoes. |
| ARP | **Bass.** Whether a sub voice follows the root, holds the tonic, or walks by fifths; how late it moves after the chord changes. |
| FX | **Air.** Bed (rain, wind noise, hiss, none), high-pass, reverb damping. |
| GLO | **Vocabulary.** Which chord qualities the chain may use: triads, 7ths, add9, sus, clusters, quartal. Toggles. |
| SEL | Root + mode (Aeolian default). |
| EDIT | Pad timbre edit. |
| PLAY | Start / stop (stop = voices release over Breath seconds, not instantly). |
| REC | **Freeze.** Hold the current chord forever; pressure stops. Tap again to release. The screen literally frosts over. |
| SAVE | Save climate + voice + pins. |
| HOME | Live page. |

### Form
Weather. In Front climate: 2–4 minutes of near-stillness around the tonic, pressure building (the screen darkens), then a front: Movement rises, Vocabulary opens to clusters, a bell or two, then resolution to a thin two-voice fifth over the bass and 60 seconds of nothing but Air. Pressure resets and begins again.

### Sequencing
None in the step sense. A seconds-based scheduler with a Markov chord chain weighted by pressure, nearest-neighbour voice assignment, and a Poisson bell process. Everything is MIDI-clock-free by design; it does accept MIDI clock to *scale* Breath to a tempo if you want to layer it with another box.

### Effects
Large plate reverb with feedback path, pitched delay, detune drift, bed generator, output limiter.

### Screen
A horizon. Pressure is the sky colour (deep blue → bruised purple → white at the break). Voices are points at their pitch height, drifting when they move. Pins are anchored stars. Bells flash.

### Why the options feel meaningful
Pressure is a single knob that controls tension, which is the thing an ambient listener actually feels. Pins turn a wandering field into a drone with movement over it. Movement = 1 versus all is the difference between Stars of the Lid and a church organ.

---

## 5. WORN TAPE — the loop that decays

**One idea:** you play a loop once; the machine wears it out and grows something new in the holes.

You record a short musical loop (1–8 bars) on the keys. It is stored as *the master*. Each pass is a new *generation*: notes may drop (Erosion), new notes from the scale may appear where there is space (Regrowth), pitch sags and wobbles (tape model), and the sound darkens. Generations accumulate until the loop is unrecognisable, then *Memory* pulls it back toward the master. The piece is the life cycle of a cassette.

### Controls

| Control | Role |
|---|---|
| ALGORITHM | **Medium.** Cassette · Reel-to-reel · Dictaphone · VHS Hi-Fi · Wax cylinder · Clean (digital, decay still happens but no tape colour). Each medium is a signature: wow/flutter rate, hiss spectrum, saturation curve, dropout character, high-frequency loss per generation. |
| PRESETS | **Instrument.** What the loop is played on: Felt Piano FM, Music Box, Pad, Choir-formant, Guitar-ish, Organ, user ×2. |
| SELECT | **Age.** Generation count, 0 → 64. Turn right to age the tape ahead of time; turn left to restore. The whole state (dropped notes, regrown notes, HF loss) is a function of Age, so it is reversible and scrubbable. |
| KNOB 1 | **Erosion.** Chance per note per generation that it drops out. |
| KNOB 2 | **Regrowth.** Chance per empty slot per generation that a new scale note appears (pitched near its neighbours, quiet). |
| KNOB 3 | **Wobble.** Wow + flutter depth. Cassette at full Wobble is seasick; Dictaphone at zero is just dark. |
| KNOB 4 | **Dust.** Hiss + crackle + rain bed, with the medium's spectrum. |
| White keys | **Play.** Record the loop with REC; otherwise overdub live. |
| Black keys | **Splices.** Toggle a splice point at that position in the loop: the tape cuts there and either reverses, skips, or repeats the next beat. Splices survive aging. |
| OCT− / OCT+ | **Tape speed** in octaves: half-speed is the Burial pitched-down everything, formants included. |
| ENV | **Fade.** How a dropped note disappears: instant → ducked → long tail into the reverb. |
| LFO | **Sag.** Very slow pitch drift (minutes) as if the motor is dying, with chance of brief speed-ups. |
| SEQ | **Loop.** Length (1–8 bars), and a second loop layer length (ARP page) so two loops of unequal length phase against each other. |
| ARP | **Second tape.** A second, slower loop (×1.5, ×2, ×3 length) that plays a thinned copy of the master an octave down. On/off, ratio, level. |
| FX | **Tape stack.** Saturation, head-bump EQ, HF loss per generation, dropout length, reverb (spring on Cassette, plate on Reel). |
| GLO | **Memory.** How strongly the piece is pulled back to the master as Age climbs: none (it never returns) → strong (it returns every 16 generations). Also whether returns are abrupt (a splice) or gradual. |
| SEL | **Scale** that Regrowth draws from (defaults to the notes you played, inferred). |
| EDIT | Instrument edit. |
| PLAY | Start / stop the transport. |
| REC | **Record the master.** Hold to arm, play, release to set loop length. Tap while playing = commit the *current* generation as the new master (the tape you have is now the original). |
| SAVE | Save master + settings. Hold = save the current aged state as a preset. |
| HOME | Live page. |

### Form
Decay cycles. From Age 0 the loop plays as recorded. Over minutes it thins and bends. Around Age 24–40 it is a few notes and a lot of tape. If Memory is set, the master reasserts itself and you hear the original return through the wear, changed by whatever regrew. If Memory is off, the loop eventually becomes drone and dust, which is also a valid end.

### Sequencing
A note list per bar with per-note generation tags. Each generation applies Erosion/Regrowth as independent trials; the result is deterministic given (master, seed, Age), which is what makes SELECT reversible.

### Effects
Tape model (wow, flutter, saturation, head bump, HF loss, dropouts), hiss/crackle/rain bed, reverb (medium-dependent), tape-speed pitch shift, splice buffer.

### Screen
A tape reel pair. The loop is drawn as notes on the tape between the reels; dropped notes fade to ghosts, regrown notes glow. Age is the oxide colour, from brown to grey. Splices are visible cuts.

### Why the options feel meaningful
Age on SELECT is a time machine for the piece. Erosion and Regrowth are the two forces that make the loop *live*, and they are the two knobs you reach for. Memory decides whether the piece is tragic or cyclical.

---

## Comparison

| | Centre of gravity | Time axis (SELECT) | Big choice (ALGORITHM) | Keys do | Form |
|---|---|---|---|---|---|
| MIDNIGHT ROUTE | Arrangement | Position in route | Route shape | Tag stations / chord palette | Composed route of 16 stations |
| VOICEMAIL | Voice | Pitch (formants held) | Who sings | Sing / vowel | Call → answer → chorus → silence |
| UNTRUE STEP | Timing | Shuffle (push ↔ swing) | Groove | Event anchors / nudge | Drums leave and return changed |
| HALF-LIGHT | Harmony | Pressure | Climate | Pins / bells | Weather fronts |
| WORN TAPE | Decay | Age | Medium | Play / splice | Decay cycles with memory |

They combine. The natural pairings are MIDNIGHT ROUTE as the host with any other as a part engine, or VOICEMAIL over HALF-LIGHT. A sixth design would be the host that runs the other four as parts, which is what the 12-voice dual-core budget would just about allow.

---

## Implementation notes

- **Base:** Felucca (Hügelton) or its Dinghy minimal template, or SLOOP. Felucca gives you the FM6 and PHYS engines, the Dattorro plate, and the HAL for free. All five designs fit the Felucca page/layer UI model.
- **Flash:** 1 MiB and already full on Felucca 2.x. Each of these should start from Dinghy and pull in only the engines it needs. VOICEMAIL's capture slots must be RAM, with at most one flash slot.
- **CPU budget:** 44.1 kHz fixed point. Plate reverb + delay + bed + 8 voices is the one-core budget. HALF-LIGHT's 12 voices and WORN TAPE's tape stack want the second core. UNTRUE STEP is cheap. MIDNIGHT ROUTE depends on part engines.
- **Testing:** fm1-emu runs FM-1 firmware unchanged in a browser or as a plugin, so form-over-20-minutes testing can be done at faster-than-realtime with a scripted clock.
- **MIDI:** all five follow MIDI clock (TRS or USB). HALF-LIGHT and WORN TAPE treat clock as optional. All expose their HOME knobs on CC 20–23 and SELECT on CC 24 so an external controller can drive the main axis.
- **Audio:** stock engine and effects are mono; Felucca's SPREAD gives width. Reverb should be true stereo where the budget allows, since the wash is most of the sound.
- **Screen:** every HOME page is a single picture of the piece's state (map, contour, timeline, horizon, reels). No text lists on HOME.

## Sources
- M-VAVE FM-1 coverage: https://www.synthtopia.com/content/2026/07/13/m-vave-introduces-fm-1-fm-pocket-synthesizer/ · https://synthanatomy.com/2026/07/m-vave-fm-1-review-low-budget-pocket-fm-ynthesizer-with-iconic-sounds.html
- Custom firmware hub (hardware, protocol, directory): https://github.com/aroum/fm1-custom-fw
- Felucca (base framework): https://github.com/hugelton/Felucca
- SLOOP (live groovebox core): https://github.com/isod89/sloop-fm1
- Skiff (control-surface conventions, constraints): https://github.com/illuminus5/skiff
- Existing ambient firmwares for contrast: https://github.com/charlesvestal/fm1-amb · https://github.com/okumurari2/maku
- Emulator: https://github.com/joakimeriksson/fm1-emu
