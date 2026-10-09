#!/usr/bin/env python3
"""Exercise actual packaging and release-note extraction, without a release."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
workflow = (ROOT / '.github/workflows/release.yml').read_text()
extract = workflow.split("<<'PY'\n", 1)[1].split('\n          PY', 1)[0]
extract = '\n'.join(line[10:] for line in extract.splitlines())
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for changelog, expected in [
        ('## v1.2.3 — 2026-10-10\nShipped fixes\n', 'Shipped fixes'),
        ('## v1.2.3 — 2026-10-10\n\n## v1.2.2 — yesterday\nOlder\n', ''),
        ('## [Unreleased]\nPending\n', '')
    ]:
        (work / 'CHANGELOG.md').write_text(changelog)
        result = subprocess.run(['python3', '-c', extract], cwd=work,
                                env={**os.environ, 'VERSION': '1.2.3'}, capture_output=True, text=True, check=True)
        assert result.stdout.strip() == expected
    build = work / 'build'
    build.mkdir()
    (build / 'firmware.bin').write_bytes(b'app')
    (build / 'firmware.factory.bin').write_bytes(b'native full image')
    class Env:
        action = None
        def subst(self, key):
            return str(build if key == '$BUILD_DIR' else ROOT)
        def AddPostAction(self, target, action):
            self.action = action
    env = Env()
    scope = {'env': env, 'Import': lambda _: None}
    exec(compile((ROOT / 'tools/build.py').read_text(), 'tools/build.py', 'exec'), scope)
    original = Path.cwd()
    try:
        os.chdir(work)
        env.action(None, None, env)
        assert (work / 'bin/AVATTO-GW90-Ti.bin').read_bytes() == b'app'
        full = next((work / 'bin').glob('*.full.bin'))
        assert full.read_bytes() == b'native full image'
        (build / 'firmware.factory.bin').write_bytes(b'')
        try:
            env.action(None, None, env)
        except RuntimeError:
            pass
        else:
            raise AssertionError('empty native artifact accepted')
    finally:
        os.chdir(original)
print('Packaging/missing-artifact/release-note fallback checks passed')
