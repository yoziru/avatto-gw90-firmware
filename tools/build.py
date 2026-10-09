#!/usr/bin/env python3
Import("env")
import shutil
import os
import sys
from glob import glob

sys.path.insert(0, os.path.join(env.subst("$PROJECT_DIR"), "tools"))
from version_util import read_version  # noqa: E402

def after_build(source, target, env):
    os.makedirs('bin', exist_ok=True)
    build_dir = env.subst("$BUILD_DIR")
    full_source = os.path.join(build_dir, "firmware.factory.bin")
    # The platform's esptool step already merges bootloader, partition table,
    # boot_app0 (OTA data), and app with the configured flash settings.
    for path in (firmware_source, full_source):
        if not os.path.isfile(path) or os.path.getsize(path) == 0:
            raise RuntimeError("Missing or empty firmware artifact: " + path)
    for f in glob ('bin/AVATTO-GW90-Ti*.bin'):
        os.unlink (f)

    VERSION_NUMBER = read_version()

    NEW_NAME_FULL = 'bin/AVATTO-GW90-Ti_v'+VERSION_NUMBER+'.full.bin'
    NEW_NAME = 'bin/AVATTO-GW90-Ti.bin'

    shutil.copy(full_source, NEW_NAME_FULL)
    shutil.copy(firmware_source, NEW_NAME)

    print('')
    print('--------------------------------------------------------')
    print('{} created with success !'.format(str(NEW_NAME_FULL)))
    print('{} created with success !'.format(str(NEW_NAME)))
    print('--------------------------------------------------------')
    print('')

env.AddPostAction("buildprog", after_build)

firmware_source = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
