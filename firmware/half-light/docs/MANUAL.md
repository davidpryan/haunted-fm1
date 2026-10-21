# HALF-LIGHT

Harmonic weather. There is no grid: time is in seconds. Up to eight voices hold a chord, and every few seconds some of
them move to the nearest notes of the next chord, under a *pressure* that builds for minutes and breaks in a front.

## In a minute

1. Tap **PLAY**. Five voices settle on the tonic, the bass finds the root, rain runs underneath.
2. Wait. Every **Breath** seconds (20 by default) one or two voices move: the chord changes by the smallest step it
   can. The sky is the pressure: deep blue near home, bruised purple as it climbs, white at the break.
3. Turn **SELECT**. That is the **pressure**: right pushes the harmony away from home and toward the dissonant
   chords, left pulls it back. In the Manual climate it is the whole instrument; in the others it leans on the curve.
4. Tap a **white key** to pin that note: one voice holds it and never moves, a drone under the weather. Tap a **black
   key** to make that scale degree a bell: a high, sparse bell rings on it now and then, timed in seconds.
5. Tap **REC** to freeze: the chord is held, the pressure stops, the screen frosts over. Tap again to let it go. Tap
   **SAVE** to keep everything.

## The panel

| Control | What it does |
|---|---|
| White keys | **Pins.** Tap to pin that note (the key's note, in the current register): a voice holds it forever. Tap again to release it. Pins take voices from the eight. |
| Black keys | **Bells.** Toggle a scale degree (left to right: the tonic, the second... up to the eleventh) as a bell. |
| PLAY | Start. Stop lets the voices release over Breath seconds, not at once. |
| REC | **Freeze**: the chord held, the pressure stopped. Tap again to thaw. HOME held + REC: release every pin. |
| SELECT | **Pressure**, −50 to +50. Manual climate: the pressure itself (−50 is home, +50 the storm). Other climates: an offset on the curve. |
| ALGORITHM | The **climate**: Still, Front, Storm, Clearing, Fog, Manual. See below. |
| PRESETS | The pad's **voice**: Glass, Saw, Bowed, Organ, Choir, Strings. The Voice page (EDIT) bends it; a saved project keeps the result, so the project is your user preset. |
| OCT− / OCT+ | The **register** of the whole field, two octaves either way. Pins already placed stay where they are. |
| SAVE | Saves the climate, the voice, the pins and bells, and every setting. A dot in the header means there is something unsaved. |
| HOME ENV LFO SEQ ARP FX GLO SEL EDIT | The pages below. KNOB 1–4 set the page's four values. Hold any button for a second for a card that says what it does. |

## The climates

| Climate | What the pressure does |
|---|---|
| **Still** | Barely moves: it drifts between 8 and 40 over a minute or two. The chain likes to stay put or move by thirds. |
| **Front** | Builds from near zero to the break over two to four minutes, then breaks: the voices resolve to a fifth over the bass and rest for about a minute, with nothing but Air. Then it builds again, the voices coming back one per change. |
| **Storm** | The same, faster: a build of 40–90 seconds, a rest of 15–30, and the chain moves by steps as often as by fifths. |
| **Clearing** | Starts high and resolves over five minutes to home, favouring falling fifths, then stays there. PLAY again for another clearing. |
| **Fog** | Wanders between 30 and 85 and never resolves. Every step is as likely as any other. |
| **Manual** | No curve: SELECT is the pressure. |

Under pressure more voices move per change (at the top, everything), the chain reaches the far degrees and the
dissonant qualities the Words page allows, and the bells come more often.

## The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **Sky** (HOME) | Breath: seconds between changes, 3–120 | Voices: 2–8 | Movement: voices that move per change, 1 (the smoothest voice leading) to All (block chords) | Depth: the plate's size and feedback, and above the middle a pitched echo whose repeats fall an octave |
| **Swell** (ENV) | Swell: a move's crossfade, 20 ms (a cut) to 10 s (the smear: both notes sound together) | Attack: a voice entering, 50 ms to 10 s | Release: a voice leaving | Pad: the pad's level |
| **Beat** (LFO) | Beat: detune drift between voices, in cents | Walk: how fast the drift wanders | Shimmer: a slow wobble in each voice's level | Width: stereo spread |
| **Bells** (SEQ) | Every: mean seconds between bells | Spread: how irregular | At once: bells that may ring together, 1–4 | Echo: chance a bell goes into the pitched echo |
| **Bass** (ARP) | Bass: Off, Root (follows the chord's root), Tonic (holds it), 5ths (walks to the chord tone a fifth away) | Late: seconds after a change before the bass moves | Level | Sub: dark to bright |
| **Air** (FX) | Bed: None, Rain, Vinyl, Hiss, Wind, Room | Air: the bed's level | Hi-pass: on the pad, before the reverb | Damping: the plate's |
| **Words** (GLO) | Sevenths | Add 9 | Sus | Dense: Off, 4ths (quartal stacks), Clus (clusters of four adjacent degrees), Both. Triads are always allowed. |
| **Key** (SEL) | Root | Mode: Aeolian, Dorian, Phrygian, harmonic minor, minor pentatonic, major, Lydian, whole tone | Range: the field's height, 2, 3 or 4 octaves around the register | Lights: Off, On, Keys (the voices' notes dim on the keys) |
| **Voice** (EDIT) | Bright | Body: the modulator's ratio bent down or up, an octave either way | Grain: feedback | Tone: a lowpass |

Every knob is audible by the next change: nothing on the Sky page sets a number you cannot hear.

## The screen

A horizon. The sky's colour is the pressure. The voices are points at their pitch height, placed left to right by
voice, and they drift to their new height when they move. Pins are stars anchored to the ground, placed by their key.
A bell is a ring spreading from where it rang. The bass is a glow on the horizon at its note. Under the horizon: the
chord (Dm7, Ab add9, G sus, C 4ths, E clus), the seconds to the next change (or REST after a break, or FROZEN), and the
pressure as a number. The thin line at the bottom fills with each breath. The header is the climate, coloured as the
sky, the voice, and the key.

## How the harmony moves

A Markov chain over the mode's degrees. Each candidate chord (a degree and a quality) has a tension: how far its
degree is from the tonic, plus how dissonant its quality is. The chain prefers chords whose tension is near the
pressure, weighted by the climate's taste for the interval it would move by. The voices that move go to the nearest
note of a chord tone nobody holds yet; a voice already on a chord tone stays. With Movement at 1 a chord can take
several changes to arrive, which is the Stars of the Lid end of it; at All it arrives at once, which is the organ.

## MIDI

MIDI clock in (USB, TRS) rounds Breath to whole bars at the clock's tempo while it keeps coming (the header says
EXT), so the weather can sit with another box; start and stop are followed. MIDI notes pin and release like the keys.
CC 20–23 are the Sky page's knobs, CC 24 is the pressure.
