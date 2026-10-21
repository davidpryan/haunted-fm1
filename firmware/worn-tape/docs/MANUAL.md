# WORN TAPE

The loop that decays. You play a short loop once; the machine wears it out and grows something new in the holes.

## In a minute

1. Tap **REC**. The screen says *play the loop: one pass*.
2. Play a few notes on the keys. The first key starts the tape; the loop is **2 bars** long (the LOOP page changes
   that), and recording stops by itself when the pass is over.
3. Listen. Every two passes the tape is one **generation** older: notes drop out (**Erosion**), new ones grow where
   there is room (**Regrowth**), the highs fade, the hiss rises, the motor sags.
4. Turn **SELECT**. That is the tape's **Age**, and it runs both ways: turn right to hear the loop years from now,
   left to restore it. The aged loop is a function of your master loop, a seed and the age, so nothing is lost.
5. Hold **REC** for a second to **commit**: the aged notes become the master, at age 0. Tap **SAVE** to keep it.

## The panel

| Control | What it does |
|---|---|
| Keys | Play the instrument. While recording, the notes go into the master loop (on a 1/8 grid). |
| SEQ + black key | A **splice** at that point of the loop. Where the playhead crosses one, the tape repeats, skips or reverses a beat (the MEMORY page picks which). |
| PLAY | The tape runs or stops. |
| REC | Tap: arm; the next key starts a recording pass of one loop (on a loop that already plays, it adds to it). Hold: commit the aged loop as the master. HOME held + REC: clear the master. |
| SELECT | **Age**, 0 to 64 generations. |
| ALGORITHM | The **medium**: Cassette, Reel, Dictaphone, VHS, Cylinder, Clean. Each wears its own way: how much of the highs go, how it wobbles, how it hisses, how often it drops out. |
| PRESETS | The **instrument**: Felt piano, Music box, Pad, Choir, Guitar, Organ. |
| OCT− / OCT+ | **Tape speed**: 1/4, 1/2, 1x, 2x. Pitch and tempo together, as a tape machine does. Half speed is the pitched-down sound. |
| SAVE | Saves the master loop, the splices and every setting. A dot in the header means there is something unsaved. |
| HOME ENV LFO SEQ ARP FX GLO SEL EDIT | The pages below. KNOB 1–4 set the page's four values. Hold any button for a second for a card that says what it does. |

## The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **Tape** (HOME) | Erosion: chance a note drops, each generation | Regrowth: chance a new note grows in a hole | Wobble: wow and flutter | Dust: hiss and the bed |
| **Notes** (ENV) | Attack | Ghosts: how loud a dropped note is still whispered | Release | Tone |
| **Motor** (LFO) | Sag: a slow drift of the motor's speed, up to a semitone | Sag rate | Speedup: brief hurries, now and then | Flutter |
| **Loop** (SEQ) | Tempo | Loop: 1 to 8 bars | Quantise: 1/8 or 1/4 | Passes: loops a generation |
| **Tape 2** (ARP) | On / off | Ratio: x1.5, x2, x3 slower | Level | Octave: down 0, 1 or 2 |
| **Wear** (FX) | Saturate | HF loss: how much of the highs go by age 64 | Dropout | Reverb |
| **Memory** (GLO) | Memory: the chance a dropped master note returns | Every: generations between returns | Return: all at once (Splice) or spread out (Fade) | Splice mode: Repeat, Skip, Reverse |
| **Key** (SEL) | Root | Scale (what Regrowth draws from) | Bed: Rain, Crackle, Hiss, Wind, Room | Lights |
| **Sound** (EDIT) | Bright | Decay | Detune | Width |

## The screen

Two reels: the left empties and the right fills as the age climbs, and the oxide goes from brown to grey. Between them,
the tape: the loop, with the notes on it as marks (bright: the master's; gold: grown; a dotted ghost: dropped), the
playhead, the second tape's playhead under it, and the splices as cuts. The line below says the generation and how
many notes sound of how many there are.

## Tape 2

A second, slower tape plays a thinned copy of the same notes an octave (or two) down. With a ratio of x1.5 the two
loops come round together every three passes of the first; with x3, every three of the second. Burial's pitched-down
doubles, phasing against the original.

## MIDI

MIDI notes in (USB, TRS) play and record like the keys. MIDI clock drives the steps while it keeps coming (the header
says EXT); start and stop are followed. The music goes out over USB audio.
