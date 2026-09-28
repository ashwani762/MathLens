# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ashwani

"""Drive the app by visible label.  usage: tap.py Label sleep:1.5 shot:out.png back swipeup ...
Uses uiautomator dumps, so taps land on the element with that text or content description."""
import os, re, subprocess, sys, time
ADB = os.path.expanduser('~/Android/Sdk/platform-tools/adb')
SER = os.environ.get('SER', 'emulator-5554')
def adb(*a, **kw): return subprocess.run([ADB, '-s', SER, *a], capture_output=True, **kw)
def dump():
    adb('shell', 'uiautomator', 'dump', '/sdcard/ui.xml')
    return adb('shell', 'cat', '/sdcard/ui.xml', text=True).stdout
def tap(label, nth=0):
    for _ in range(3):
        x = dump(); hits = []
        for node in re.finditer(r'<node [^>]*>', x):
            n = node.group(0)
            t = re.search(r' text="([^"]*)"', n); d = re.search(r'content-desc="([^"]*)"', n)
            b = re.search(r'bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"', n)
            if b and ((t and t.group(1) == label) or (d and d.group(1) == label)): hits.append(tuple(map(int, b.groups())))
        if len(hits) > nth:
            x0, y0, x1, y1 = hits[nth]; adb('shell', 'input', 'tap', str((x0 + x1) // 2), str((y0 + y1) // 2)); return True
        time.sleep(1)
    print('NOT FOUND', label); return False
for arg in sys.argv[1:]:
    if arg.startswith('sleep:'): time.sleep(float(arg[6:]))
    elif arg == 'back': adb('shell', 'input', 'keyevent', '4'); time.sleep(1)
    elif arg.startswith('shot:'): open(arg[5:], 'wb').write(adb('exec-out', 'screencap', '-p').stdout)
    elif arg == 'swipeup': adb('shell', 'input', 'swipe', '540', '1900', '540', '700', '400'); time.sleep(1)
    elif arg.startswith('xy:'): x, y = arg[3:].split(','); adb('shell', 'input', 'tap', x, y)
    else: tap(arg)
