# Haunted FM-1

Five generative firmwares for the [M-VAVE FM-1](https://www.m-vave.com/) pocket synth, each built around one idea,
all of them after the kind of haunted ambient music Burial makes: pitched-down ghost vocals, vinyl crackle and rain,
sub bass under detuned pads, swung off-grid percussion, long stretches where almost nothing happens, and arrangements
that leave and come back changed.

| Firmware | The idea | SELECT is | ALGORITHM is |
|---|---|---|---|
| [MIDNIGHT ROUTE](firmware/midnight-route) | The song has a shape before it has notes: a route of 16 stations with a density envelope per part | your position on the route | the route's shape |
| [VOICEMAIL](firmware/voicemail) | A voice that almost says something: formant vocal fragments, chopped, stuttered, reversed, in a call-and-answer | pitch, with the formants held | who is singing |
| [UNTRUE STEP](firmware/untrue-step) | Garage with no grid: every hit is a time in milliseconds with a spread, and the drums leave and come back changed | shuffle, from push to swing | the groove |
| [HALF-LIGHT](firmware/half-light) | Harmony as weather: chords move by voice leading every few seconds under a pressure that builds and breaks | the pressure | the climate |
| [WORN TAPE](firmware/worn-tape) | A loop you play once; every pass wears it out and grows something new in the holes | the tape's age, both ways | the medium |

The designs, with every control, are in [docs/DESIGN.md](docs/DESIGN.md). Each firmware has a manual in its
`docs/MANUAL.md`.

## Install

**The flasher page** (GitHub Pages, built by the workflow from `main`): open it in Chrome or Edge with the FM-1 on
USB, pick a firmware, press Install. The page also puts M-VAVE's stock firmware back. Any of the five installs over
any other.

From the command line: `pip3 install mido python-rtmidi`, then

```bash
python3 base/tools/fm1_install.py worntape-0.1.fwsc
```

Installing firmware is at your own risk. An FM-1 that fails to start twice comes up in a safe mode the installer can
reach; one that does not start at all is recovered with
[felucca-bsp's rescue tool](https://github.com/hugelton/felucca-bsp/tree/main/tools).

## Build

Everything is built on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule, with
[felucca-bsp](https://github.com/hugelton/felucca-bsp) inside it). Clone with `--recursive`.

```bash
sh tools/get_toolchain.sh          # JieLi's clang for the pi32v2, into ~/.jieli/toolchain
sh tools/get_sdk.sh                # the three JieLi SDK files the package needs, into ~/.jieli/ac79-sdk
pip3 install Pillow                # the fonts are rasterised at build time
sh tools/build.sh worn-tape        # firmware/worn-tape/build/worntape.fwsc (Docker on macOS: the toolchain is Linux x86-64)
sh tools/build.sh worn-tape --host # the host simulator, then:
sh tools/test.sh worn-tape         # its scenarios (firmware/worn-tape/tests/*.txt), screenshots and audio in build/scenarios/
sh tools/build.sh worn-tape --web  # the browser emulator (needs Emscripten)
```

The names are the directories under `firmware/`: `midnight-route`, `voicemail`, `untrue-step`, `half-light`,
`worn-tape`. `tools/make_site.py --local VERSION OUT` assembles the site from local builds.

## Layout

```
base/                fm1-base (submodule): the platform, the host simulator, the browser emulator, the tools
lib/hdsp.h           the DSP the five share: FM and formant voices, Dattorro's plate, a tape echo, wow, beds, harmony
lib/hui.h            the panel and screen helpers they share: input, help cards, the header and knob bands
firmware/NAME/       one firmware: app.h, app.c (the UI and project), dsp/engine.[ch] (the sound, at -O2),
                     fm1.toml, words.tsv, tests/, docs/MANUAL.md, site/landing.md
tools/               build.sh, test.sh, get_toolchain.sh, get_sdk.sh, make_site.py
web/site.html        the flasher page (fm1-base's installer code inlined)
.github/workflows/   build every firmware, run its scenarios, publish the site, release on a tag
```

## Continuous integration

Every push to `main` builds the five packages on GitHub's Linux runners (the toolchain runs natively there), runs
each firmware's host-simulator scenarios, builds the browser emulators, assembles the site and publishes it on GitHub
Pages (turn on Pages with Source "GitHub Actions" in the repository settings once). A tag `vX.Y` also makes a
release with the five packages. Pull requests build and test without publishing.

## Licence

The firmwares and the shared libraries are MIT (see [LICENSE](LICENSE)). fm1-base and felucca-bsp are MPL-2.0; the
JieLi SDK files the package needs are Apache-2.0 and fetched at build time, never committed. M-VAVE and FM-1 are
trademarks of their owners; this project is not affiliated with M-VAVE. The reverb is Jon Dattorro's plate
("Effect Design, Part 1", JAES 1997).
