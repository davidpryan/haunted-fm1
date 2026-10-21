# VOICEMAIL

The ghost-vocal fragment machine. A voice that almost says something: a formant singer sings phrases, not steps,
in a conversation that goes round and round, and the pitch drops with the throat held.

## In a minute

1. Tap **PLAY**. The pad settles on its first chord and the voice **calls**: a phrase from the book, over the
   chord. Then it waits, and a second voice **answers** it, transposed; then both sing the **chorus** as the wash
   opens; then **silence**, the bed and the pad alone, for a few bars. Then a new phrase, a new call.
2. Turn **SELECT** to the left. That is the **pitch**, down to −24 semitones, with the formants held where they
   are: the voice drops without its throat changing, and a lullaby becomes a haunting.
3. The four knobs are the four cuts: **Fragment** (how much of each phrase is heard), **Stutter**, **Reverse**,
   **Wash**. Centre is as written, left is none, right is too much.
4. Play a white key: the ghost sings a phrase at that pitch, from the next 1/8. Hold a black key with it to pick
   the vowel.
5. Tap **REC**, play a few keys, tap **REC** again: that is the **melody**, and the ghost sings it instead of the
   book's phrases from now on. Tap **SAVE** to keep it.

## The panel

| Control | What it does |
|---|---|
| White keys | The ghost sings a phrase at that pitch, from the next 1/8 (it cuts in on the call). While writing, the keys go into the melody, quantised to 1/8, with the lengths you hold them. |
| Black keys | Held with a white key: the vowel. From the left: a, e, i, o, u; then breathy a, o, u; then whispered a, i, u. |
| PLAY | The conversation runs or stops. A key starts it too. |
| REC | Tap: **write**; the next keys write a fresh melody (up to 16 notes, 4 bars); tap again to finish. Hold: **keep**; the phrase being sung (or the last sung) becomes the melody. HOME held + REC: clear the melody (the book sings again). |
| SELECT | **Pitch**, −24 to +12 semitones, the formants held. |
| ALGORITHM | **Who sings**: Child, Woman, Man, Choir (two voices, detuned), Whisper, Slowed (a man's formants at half speed), Radio (band-limited, crushed). |
| PRESETS | The **phrase book**: Hymn, Soul, Lullaby, Siren, Plea, Static. Six contours (sigh, call, question, mantra, fall, held), three a book, at the book's pace and width, in its vowels. |
| OCT− / OCT+ | The pad's **register**, two octaves down to one up. |
| SAVE | Saves the melody and every setting. A dot in the header means there is something unsaved. |
| HOME ENV LFO SEQ ARP FX GLO SEL EDIT | The pages below. KNOB 1–4 set the page's four values. Hold any button for a second for a card that says what it does. |

## The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **Ghost** (HOME) | Fragment: the whole phrase, a half, a syllable, a consonant | Stutter: the chance a syllable's first slice (a 1/16 or a 1/32) comes 2 to 4 times | Reverse: the chance a syllable plays backwards; above 70 % into a swelling reverb | Wash: the plate and the 1/8-dot echo, darker as it rises |
| **Breath** (ENV) | Attack: swell to clip | Decay: how far the tone decays into breath | Breath: the breath under the tone | Consonant: the breath before each syllable |
| **Tape** (LFO) | Wow | Wow rate: 0.3 to 2 Hz | Flutter | Dropout: brief holes in the voice |
| **Place** (SEQ) | Tempo, 50 to 140 | Density: the chance a phrase starts at a bar line | Silence: bars of it, give or take | Repeat: how many times the call is sung |
| **Pad** (ARP) | Chord 1 | Chord 2 | Chord 3 | Chord 4: degrees of the scale, or a rest |
| **Room** (FX) | Bed: Rain, Crackle, Both, Hiss, Wind, Room | Bed level | High-pass: before the reverb | Pad: its level |
| **Answer** (GLO) | Delay: bars before the answer | Transpose: Unison, Up 4th, Down 5th, Invert | Chance: that it answers at all | Mode: Overlap (counted from the call's start) or Wait (from its end) |
| **Key** (SEL) | Root | Scale | Bars: a chord lasts | Lights |
| **Throat** (EDIT) | Throat: the formants up or down, the pitch untouched | Tone | Rasp: how hard the glottis | Vibrato |

## The screen

The phase in its colour (CALL, ANSWER, CHORUS, SILENCE) and what is coming, in bars; the pitch in large type.
Below, the phrase as a contour: each syllable a bar at its pitch, joined by a thin line, the one sounding lit, the
ones Fragment cut away dotted; the voice's playhead runs over it, and the answer's phrase in its own colour when the
second voice sings. Under that, a countdown bar that dims as the next event comes. At the bottom, the pad's four
chords with the one sounding filled, and on the right the melody's note count, the book, or STUTTER and REVERSE as
they happen.

## The conversation

The call is sung Repeat times, each start a throw of the Density dice at a bar line. The answer comes Delay bars
later, with Chance, transposed as the Answer page says; with no answer, the silence comes at once. In the chorus the
second voice follows the first half a bar behind and the wash opens by itself. The silence is Silence bars, a little
more or less. The pad's progression runs on its own count of bars underneath, so each call lands on a different
chord, and the phrase is built on the chord of the moment: the same contour over I and over VI are two different
phrases.

The voice is heard through a short buffer, 140 ms behind, which is what lets a syllable be stuttered or reversed as
it comes rather than after it has gone.

## MIDI

MIDI notes in (USB, TRS) sing and write like the keys. MIDI clock drives the steps while it keeps coming (the header
says EXT); start and stop are followed. The music goes out over USB audio.
