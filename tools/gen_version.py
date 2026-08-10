""" Generate src/version.h from tools/version.

The version is constant (only the release workflow bumps tools/version).
Builds from a dirty working tree get a -dirty suffix.
"""
import datetime
import os
import sys

Import("env")

sys.path.insert(0, os.path.join(env.subst("$PROJECT_DIR"), "tools"))
from version_util import describe  # noqa: E402

VERSION_NUMBER = describe()
print('Build number: {}'.format(VERSION_NUMBER))

HEADER_FILE = """
// AUTO GENERATED FILE, DO NOT EDIT
#ifndef VERSION
    #define VERSION "{}"
#endif
#ifndef BUILD_TIMESTAMP
    #define BUILD_TIMESTAMP "{}"
#endif
""".format(VERSION_NUMBER, datetime.datetime.now())

VERSION_HEADER = 'version.h'
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
