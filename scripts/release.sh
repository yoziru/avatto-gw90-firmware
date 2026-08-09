#!/bin/sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <patch|minor|major|version>" >&2
    exit 1
fi

ARG="$1"

cd "$ROOT_DIR"

if [ -z "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]; then
    if ! gh auth status >/dev/null 2>&1; then
        echo "Error: gh is not authenticated" >&2
        exit 1
    fi
fi

current_branch=$(git branch --show-current)
if [ "$current_branch" != "main" ]; then
    echo "Error: run releases from main (current branch: $current_branch)" >&2
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    echo "Error: working tree must be clean before releasing" >&2
    exit 1
fi

git fetch origin --tags
git pull --ff-only origin main

current_version=$(tr -d '\n' < tools/version)

case "$ARG" in
    patch|minor|major)
        IFS=. read -r major minor patch <<EOF
$current_version
EOF
        case "$ARG" in
            patch) patch=$((patch + 1)) ;;
            minor) minor=$((minor + 1)); patch=0 ;;
            major) major=$((major + 1)); minor=0; patch=0 ;;
        esac
        next_version="$major.$minor.$patch"
        ;;
    *)
        case "$ARG" in
            ''|*[!0-9.]*|*.*.*.*|*..*|.*|*.)
                echo "Error: argument must be patch, minor, major, or X.Y.Z" >&2
                exit 1
                ;;
        esac
        next_version="$ARG"
        ;;
esac

if [ "$next_version" = "$current_version" ]; then
    echo "Error: next version matches current version ($current_version)" >&2
    exit 1
fi

if git rev-parse -q --verify "refs/tags/v$next_version" >/dev/null 2>&1; then
    echo "Error: tag v$next_version already exists" >&2
    exit 1
fi

VERSION="$next_version" ROOT_DIR="$ROOT_DIR" python3 <<'PY'
import os
import re
from pathlib import Path

root = Path(os.environ["ROOT_DIR"])
version = os.environ["VERSION"]

(root / "tools" / "version").write_text(f"{version}", encoding="utf-8")

manifest_path = root / "manifest.json"
manifest_text = manifest_path.read_text(encoding="utf-8")
manifest_text, count = re.subn(
    r'("version":\s*")v[^"]+(")',
    rf'\g<1>v{version}\2',
    manifest_text,
    count=1,
)
if count != 1:
    raise SystemExit("Could not update version in manifest.json")
manifest_text, count = re.subn(
    r'("path":\s*")bin/AVATTO-GW90-Ti_v[^"]+\.full\.bin(")',
    rf'\g<1>bin/AVATTO-GW90-Ti_v{version}.full.bin\2',
    manifest_text,
    count=1,
)
if count != 1:
    raise SystemExit("Could not update artifact path in manifest.json")
manifest_path.write_text(manifest_text, encoding="utf-8")
PY

git add tools/version manifest.json
if git diff --cached --quiet; then
    echo "Error: no version changes to commit" >&2
    exit 1
fi

git commit -m "release: v$next_version"
git tag "v$next_version"
git push origin main
git push origin "v$next_version"

echo "Released v$next_version"
