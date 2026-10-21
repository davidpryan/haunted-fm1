# UNTRUE STEP

Ghost garage without a grid. The haunting is in the timing: every hit is an event with a time in milliseconds, a
spread and a drag, and there is no step grid to hold it still.

## In a minute

1. Tap **PLAY**. A 2-step groove runs on a two-bar cycle: four lanes of drums (KICK, SNARE, HAT, FOLEY) over a SUB
   that follows the kick and a detuned PAD holding a progression of four chords, with vinyl crackle and rain under
   everything.
2. Turn **SELECT**. That is **Shuffle**: centre is straight, right swings the off-beats late (all the way is the
   two-thirds of a triplet), left pushes them early and anxious. It bends the *times*, so it works on any pattern.
3. Turn **KNOB 1**, **Loose**. Every cycle each hit is rolled again: where it lands (up to 40 ms of gaussian spread),
   whether it plays at all (its chance), how hard. Nothing repeats exactly.
4. Play the white keys: they play the current lane, soft on the left, hard on the right. Hold **REC** while you play
   and the hits are kept, at the exact moment you played them: no quantise. Tap **SAVE** to keep the pattern.
5. Wait. Every so often the drums leave, the pad and the sub carry on alone with the dust, and the drums come back lane
   by lane, changed: the spread and the ghosts rolled again. That is **GLO**, *Leave*, and it is the form.

## The panel

| Control | What it does |
|---|---|
| White keys | Play the current lane live: velocity rises from left to right. On the SUB and PAD lanes they play scale degrees. |
| REC + white key | The hit is **kept** at the exact time it was played, in the current lane. REC tapped alone: every hit is kept until it is tapped again (the red light). |
| SEQ + white key | **Adds** an event near that anchor: the 16 white keys are the 16 eighths of the two bars, and the event lands Loose away from it. |
| Black keys 1–6 | The **lane**: KICK, SNARE, HAT, FOLEY, SUB, PAD. The lit black key says which. |
| Black keys 7, 8 | **Nudge** the selected event −5 / +5 ms. |
| Black key 9 | **Removes** the selected event (a template hit is marked off; your own is gone). |
| Black key 10 | **Mutes** the current lane. |
| Black key 11 | The drums **leave now**. |
| PLAY | Start / stop. |
| SELECT | **Shuffle**, −50 push … 0 straight … +50 swing. |
| ALGORITHM | The **groove**: 2-Step, Half-time, Broken, Dub step (kick and delay), Ghost 4×4 (the kick barely there), Pulse, None. A template of events; your edits are kept apart from it, so turning the groove keeps them. |
| PRESETS | The **kit**: Lighter, Vinyl, Bottle, Keys, Metal, Paper, Wood, Garage. Foley-flavoured models: a lighter's click, a bottle's ring, a key jangle, paper. |
| OCT− / OCT+ | The **sub's octave**: Low, Mid, High. |
| HOME + REC | The groove's template alone: your edits cleared. |
| SAVE | Saves the edits and every setting. A dot in the header means there is something unsaved. |
| HOME ENV LFO SEQ ARP FX GLO SEL EDIT | The pages below. KNOB 1–4 set the page's four values. Hold any button for a second for a card that says what it does. |

## The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **Groove** (HOME) | Loose: the spread of every hit, 0 to 40 ms, and of its velocity | Ghosts: quiet extra hits between the events, new ones every cycle | Duck: the pad and the bed duck to the kick; further right, a longer release | Dust: the crackle and rain bed, and the hat's high-pass |
| **Decay** (ENV) | Kick | Snare | Hat | Foley: long is the reverb-drenched hit |
| **Drag** (LFO) | Drag: the tempo breathes, up to ±2 % | Over: bars per breath, 8 to 32 | Wander: each hit's mean drifts slowly | Flam: the chance of a quieter hit just before a snare or foley |
| **Events** (SEQ) | Event: pick a hit (it is ringed on the timeline) | Move: its offset from its anchor, in ms | Velocity | Chance |
| **Pad** (ARP) | Chords: a progression of four scale degrees | Change: bars per chord | Detune | Pad level |
| **Sub** (FX) | Sub level | Sub decay | Delay: snare, foley and hat to a dotted-eighth tape delay | Feedback: each repeat darker |
| **Leave** (GLO) | Every: bars between departures | Away: bars the drums stay away | Chance that they go | Return: All at once, Kick first, Hats first, or New (lane by lane, and Loose and Ghosts rolled again) |
| **Key** (SEL) | Root | Scale | Tempo, 80 to 160 | Bed: None, Vinyl, Rain, Both, Hiss, Room |
| **Kit** (EDIT) | Kick tune, ±12 semitones | Snare tone: darker or brighter, more or less tone under the noise | Hat colour: the high-pass and the metallic tick | Room: the plate on the snare and the foley (and a little on the pad) |

## The screen

The two-bar timeline, eight beats across, six lanes stacked: K S H F for the drums, B for the sub, P for the pad.
Every hit is a dot whose size is its velocity, with a halo as wide as Loose; a hollow mark is a hit the cycle's roll
skipped; a brighter dot is your own. Hits light as they fire, the sub's line lights with the kick, and the pad's row
pumps with the duck. The playhead crosses it all; when the drums have left, their lanes go grey and the line under
the picture says so. The black key of the current lane is lit, and the white key of a hit's anchor lights as it fires.

On the Events page the selected hit is ringed, and the line under the picture names its lane and its offset in ms.

## Timing, in detail

A cycle is two bars at the tempo. Each event keeps its time as written; each cycle it is rolled: a gaussian spread of
Loose, a chance, a velocity. Shuffle is then applied inside each beat, a warp that moves the half-beat to one third
(push) or two thirds (swing) and everything else in proportion, so sixteenths and your off-grid hits bend with it.
Events fire at their sample inside the audio block, not on a tick. A hit recorded with REC is stored at the written
time that plays back at that very moment under the current Shuffle, so it comes back where you played it.

The templates sit on the cycle's sixteenths with their own micro-offsets (a snare a few ms late, a hat dragged). Your
edits are mods over those slots and a list of your own events, which is why they survive a change of groove.

## MIDI

MIDI notes in (USB, TRS) play the lanes: 35–36 the kick, 37–40 the snare, 41–46 the hat, anything else the foley.
MIDI clock sets the tempo and pulls the phase while it keeps coming (the header says EXT); start and stop are
followed. The shuffle and the spread stay inside the beat, so the warp rides any clock. The music goes out over USB
audio.
