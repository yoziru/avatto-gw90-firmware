#!/usr/bin/env python3
"""Web asset pipeline: stage frontend sources and gzip each file into a
PROGMEM C header in src/webh/.

Pure stdlib - replaces the old gulp toolchain (gulpfile.js + package.json),
so the web build needs no npm/node install step.
"""

import gzip
import os
import shutil
from pathlib import Path

WEBSRC = Path("src/websrc")
STAGE = WEBSRC / "gzipped"
WEBH = Path("src/webh")


def write_header(filename, data):
    sym = filename.replace(".", "_").replace("-", "_")
    toks = ["0x%02x" % b for b in data]
    body = ",\n".join(",".join(toks[i:i + 1000]) for i in range(0, len(toks), 1000))
    (WEBH / (filename + ".h")).write_text(
        "#define %s_len %d\nconst uint8_t %s[] PROGMEM = {\n%s\n};" % (sym, len(data), sym, body))


def stage_and_emit():
    # Wipe first: the old pipeline never cleaned, so headers/routes for
    # long-deleted sources (bootstrap, jquery, AVATTO_B, ...) shipped forever.
    if STAGE.exists():
        shutil.rmtree(STAGE)
    for old in WEBH.glob("*.gz.h"):
        old.unlink()
    WEBH.mkdir(parents=True, exist_ok=True)
    # Old stylesConcat: water.css + style.css -> tracked required.css ('\n' join)
    css = (WEBSRC / "css" / "water.css").read_bytes() + b"\n" + (WEBSRC / "css" / "style.css").read_bytes()
    (WEBSRC / "css" / "required.css").write_bytes(css)
    (STAGE / "css").mkdir(parents=True)
    (STAGE / "css" / "required.css").write_bytes(css)
    write_header("required.css.gz", gzip.compress(css, compresslevel=9))
    for kind in ("html", "js", "img"):
        src = WEBSRC / kind
        if not src.is_dir():
            continue
        dst = STAGE / kind
        shutil.copytree(src, dst)
        for f in sorted(dst.iterdir()):
            if f.is_file():
                write_header(f.name + ".gz", gzip.compress(f.read_bytes(), compresslevel=9))


if not os.path.exists("tools/webfilesbuilder/.no_web_update"):
    print("Staging + gzipping web assets...")
    stage_and_emit()
