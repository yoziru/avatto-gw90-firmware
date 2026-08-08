""" Create version header and tracker file if missing """
import datetime
import os
import subprocess

Import("env")


def _git(*args):
    try:
        return subprocess.run(
            args, capture_output=True, text=True, timeout=5
        ).stdout.strip()
    except Exception:
        return ""


GIT_HASH = _git("git", "rev-parse", "--short", "HEAD")
GIT_DIRTY = False
if GIT_HASH:
    try:
        GIT_DIRTY = (
            subprocess.run(
                ["git", "diff", "--quiet"], capture_output=True, text=True, timeout=5
            ).returncode
            != 0
        )
    except Exception:
        GIT_DIRTY = False

VERSION_FILE = 'tools/version'
VERSION_HEADER = 'version.h'
VERSION_PREFIX = '0.1.'
VERSION_PATCH_NUMBER = 0

if not os.path.exists("tools/.version_no_increment"):
    try:
        with open(VERSION_FILE) as FILE:
            VERSION_PATCH_NUMBER = FILE.readline()
            VERSION_PREFIX = VERSION_PATCH_NUMBER[0:VERSION_PATCH_NUMBER.rindex('.')+1]
            VERSION_PATCH_NUMBER = int(VERSION_PATCH_NUMBER[VERSION_PATCH_NUMBER.rindex('.')+1:])
            if not os.path.exists("tools/.version_no_increment_update_date"):
                VERSION_PATCH_NUMBER = VERSION_PATCH_NUMBER + 1
    except:
        print('No version file found or incorrect data in it. Starting from 0.1.0')
        VERSION_PATCH_NUMBER = 0
    with open(VERSION_FILE, 'w+') as FILE:
        FILE.write(VERSION_PREFIX + str(VERSION_PATCH_NUMBER))
        print('Build number: {}'.format(VERSION_PREFIX + str(VERSION_PATCH_NUMBER)))

    VERSION_NUMBER = VERSION_PREFIX + str(VERSION_PATCH_NUMBER)
    open('tools/.version_no_increment', 'a').close()
else:
    if os.path.exists("tools/version"):
        FILE = open(VERSION_FILE)
        VERSION_NUMBER = FILE.readline().strip()
        print('Build number: {} (waiting for upload before next increment)'.format(str(VERSION_NUMBER)))
    else:
        print('No version file found or incorrect data in it!!')
        VERSION_NUMBER = "0.0.0"

GIT_SUFFIX = ("+g" + GIT_HASH + ("-dirty" if GIT_DIRTY else "")) if GIT_HASH else ""

HEADER_FILE = """
// AUTO GENERATED FILE, DO NOT EDIT
#ifndef VERSION
    #define VERSION "{}"
#endif
#ifndef BUILD_TIMESTAMP
    #define BUILD_TIMESTAMP "{}"
#endif
""".format(VERSION_NUMBER + GIT_SUFFIX, datetime.datetime.now())

if os.environ.get('PLATFORMIO_INCLUDE_DIR') is not None:
    VERSION_HEADER = os.environ.get('PLATFORMIO_INCLUDE_DIR') + os.sep + VERSION_HEADER
elif os.path.exists("src"):
    VERSION_HEADER = "src" + os.sep + VERSION_HEADER
else:
    PROJECT_DIR = env.subst("$PROJECT_DIR")
    os.mkdir(PROJECT_DIR + os.sep + "include")
    VERSION_HEADER = "include" + os.sep + VERSION_HEADER

with open(VERSION_HEADER, 'w+') as FILE:
    FILE.write(HEADER_FILE)
