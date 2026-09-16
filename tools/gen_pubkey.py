""" Regenerate src/fw_pubkey.h from keys/fw-signing-pub.pem.

Run after replacing the public key: python3 tools/gen_pubkey.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
PUBKEY_FILE = ROOT / "keys" / "fw-signing-pub.pem"
HEADER_FILE = ROOT / "src" / "fw_pubkey.h"

pem = PUBKEY_FILE.read_text(encoding="ascii").strip()
lines = "".join('    "{}"\n'.format(line + "\\n") for line in pem.splitlines())

HEADER_FILE.write_text(
    "// AUTO GENERATED FILE, DO NOT EDIT (tools/gen_pubkey.py)\n"
    "#ifndef FW_PUBKEY_H_\n"
    "#define FW_PUBKEY_H_\n\n"
    "static const char FW_SIGNING_PUBKEY_PEM[] =\n"
    + lines.rstrip("\n")
    + ";\n\n#endif\n",
    encoding="ascii",
)
print("Wrote {}".format(HEADER_FILE.relative_to(ROOT)))
