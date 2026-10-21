#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 David Ryan
"""Make the site for GitHub Pages: one page that presents the five firmwares and installs any of them on an FM-1
over USB (Chrome or Edge, Web MIDI), each firmware's browser emulator and manual, and the packages to download.

  tools/make_site.py DIST VERSION OUT_DIR

DIST holds one directory per firmware (as the build workflow collects them): DIST/NAME/*.fwsc and DIST/NAME/emu/
(tools/build.sh NAME --web). Locally, after building: tools/make_site.py --local VERSION OUT_DIR gathers them from
firmware/NAME/build/ instead.

  OUT/index.html              the five, with Install / Try / Manual / Download for each; the installer is on the page
  OUT/firmware/NAME-V.fwsc    the packages
  OUT/emu/NAME/               each firmware in the browser
  OUT/manual/NAME/index.html  each manual (firmware/NAME/docs/MANUAL.md)
  OUT/words/NAME/             the screen reader's words files (fm1-reader), as words.tsv's url line promises
The installer code is fm1-base's (web/fm1pkg.js, web/fm1ota.js, MPL-2.0), inlined."""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"
sys.path.insert(0, str(BASE / "tools"))
from make_site import product_of, strip_module  # noqa: E402  (fm1-base's: the package identity, ES modules inlined)
from build import read_conf  # noqa: E402

FIRMWARES = ["midnight-route", "voicemail", "untrue-step", "half-light", "worn-tape"]
TITLES = {"midnight-route": "MIDNIGHT ROUTE", "voicemail": "VOICEMAIL", "untrue-step": "UNTRUE STEP",
          "half-light": "HALF-LIGHT", "worn-tape": "WORN TAPE"}        # as the screens say them (fm1.toml's name names files)
REPO = "https://github.com/thedavidpryan/haunted-fm1"


def gather(dist, local):
    """[{id, name, tagline, blurb, colour, version, product, pkg, has_emu, has_manual}]"""
    out = []
    for fw in FIRMWARES:
        app = ROOT / "firmware" / fw
        if not (app / "fm1.toml").exists():
            print(f"(no firmware/{fw}: skipped)")
            continue
        conf = read_conf(app / "fm1.toml")
        src = app / "build" if local else Path(dist) / fw
        pkgs = sorted(src.glob("*.fwsc"), key=lambda f: f.stat().st_mtime)   # the newest build (a release over a dev one)
        if not pkgs:
            print(f"(no package for {fw} in {src}: skipped)")
            continue
        pkg = pkgs[-1]
        raw = pkg.read_bytes()
        out.append({"id": fw, "name": TITLES.get(fw, conf.get("name", fw)), "tagline": conf.get("tagline", ""),
                    "blurb": conf.get("blurb", ""), "colour": conf.get("colour", "#d08a4a"),
                    "product": product_of(raw), "pkg_src": pkg, "pkg": f"firmware/{pkg.name}", "size": len(raw),
                    "emu": src / "emu" if (src / "emu" / "app.wasm").exists() else None,
                    "manual": app / "docs" / "MANUAL.md" if (app / "docs" / "MANUAL.md").exists() else None,
                    "img": app / "docs" / "img" if (app / "docs" / "img").is_dir() else None, "conf": conf})
    return out


def main(dist, version, out, local=False):
    out = Path(out)
    fws = gather(dist, local)
    if not fws:
        raise SystemExit("make_site: no firmware packages found")
    if out.exists():
        shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True)
    page = (ROOT / "web" / "site.html").read_text(encoding="utf-8")
    lib = strip_module((BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((BASE / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"web/site.html must contain {mark} once")
    meta = []
    for f in fws:
        shutil.copy(f["pkg_src"], out / "firmware" / f["pkg_src"].name)
        if f["emu"]:
            shutil.copytree(f["emu"], out / "emu" / f["id"])
        if f["manual"]:
            (out / "manual" / f["id"]).mkdir(parents=True)
            subprocess.run([sys.executable, str(BASE / "tools" / "manual_html.py"), str(f["manual"]),
                            str(out / "manual" / f["id"] / "index.html"), "--name", f["name"], "--repo", REPO,
                            "--tagline", f["tagline"]], check=True)
            if f["img"]:
                shutil.copytree(f["img"], out / "manual" / f["id"] / "img")
        if (ROOT / "firmware" / f["id"] / "web" / "words").is_dir():     # the screen reader's words files, every one
            shutil.copytree(ROOT / "firmware" / f["id"] / "web" / "words", out / "words" / f["id"])
        meta.append({k: f[k] for k in ("id", "name", "tagline", "blurb", "colour", "product", "pkg", "size")}
                    | {"emu": bool(f["emu"]), "manual": bool(f["manual"])})
    page = page.replace("/*LIB*/", lib).replace("/*META*/", json.dumps({"version": version, "repo": REPO, "firmwares": meta}))
    page = page.replace("{{VERSION}}", version).replace("{{REPO}}", REPO)
    (out / "index.html").write_text(page, encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: {len(fws)} firmwares, version {version}: " + ", ".join(f['id'] for f in fws))


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == "--local":
        main(None, a[1], a[2], local=True)
    elif len(a) == 3:
        main(*a)
    else:
        sys.exit(__doc__)
