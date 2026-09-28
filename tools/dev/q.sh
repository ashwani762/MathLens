#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ashwani

# Open MathLens on a query and screenshot it.  usage: q.sh "<query>" out.png [seconds]
ADB="$HOME/Android/Sdk/platform-tools/adb -s ${SER:-emulator-5554}"
$ADB shell am start -n com.mathlens.app/.MainActivity -e query64 "$(printf %s "$1" | base64 -w0)" >/dev/null 2>&1
sleep ${3:-6}
$ADB exec-out screencap -p > "$2"
