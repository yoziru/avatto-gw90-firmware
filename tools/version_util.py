""" Shared version helpers for the build scripts. """

import subprocess

VERSION_FILE = "tools/version"


def read_version():
    try:
        with open(VERSION_FILE) as f:
            return f.readline().strip()
    except (IOError, ValueError):
        return "0.0.0"


def _git(*args):
    try:
        return subprocess.run(
            args, capture_output=True, text=True, timeout=5
        ).stdout.strip()
    except Exception:
        return ""


def describe():
    """Return tools/version plus git metadata, e.g. "1.0.7+gabc123" or "1.0.7-dirty+gabc123"."""
    version = read_version()
    git_hash = _git("git", "rev-parse", "--short", "HEAD")
    if git_hash:
        if _git("git", "status", "--porcelain"):
            version += "-dirty"
        version += "+g" + git_hash
    return version
