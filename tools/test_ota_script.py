#!/usr/bin/env python3
"""Check uploader failure reporting without touching a device."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "scripts").mkdir()
    (work / "bin").mkdir()
    (work / "fake").mkdir()
    shutil.copy(ROOT / "scripts/ota_flash.sh", work / "scripts/ota_flash.sh")
    (work / "bin/AVATTO-GW90-Ti.bin").write_bytes(b"test")
    curl = work / "fake/curl"
    curl.write_text(r"""#!/bin/sh
case "$*" in
  *update?force=1*)
    case "$*" in *--digest*--fail-with-body*|*--fail-with-body*--digest*) ;; *) exit 90;; esac
    case "$*" in *X-Gateway-Request*) ;; *) exit 91;; esac
    if [ "$SCENARIO" = http_error ]; then echo FAIL; exit 22; fi
    if [ "$SCENARIO" = fail_body ]; then echo FAIL; else echo OK; fi;;
  *)
    if [ "$SCENARIO" = no_reboot ]; then echo 200; exit 0; fi
    if [ ! -f "$STATE" ]; then touch "$STATE"; echo 000; else echo 200; fi;;
esac
""")
    sleep = work / "fake/sleep"
    sleep.write_text("#!/bin/sh\nexit 0\n")
    curl.chmod(0o755)
    sleep.chmod(0o755)
    for scenario in ("http_error", "fail_body", "no_reboot", "success"):
        env = {**os.environ, "PATH": str(work / "fake") + os.pathsep + os.environ["PATH"],
               "OTA_DEVICE_IP": "example.invalid", "OTA_USER": "test", "OTA_PASS": "test",
               "SCENARIO": scenario, "STATE": str(work / (scenario + ".state"))}
        result = subprocess.run(["sh", str(work / "scripts/ota_flash.sh")], env=env, capture_output=True, text=True, timeout=10)
        assert (result.returncode == 0) == (scenario == "success"), (scenario, result.stdout, result.stderr)
print("OTA script failure/Digest/reboot regressions passed")
