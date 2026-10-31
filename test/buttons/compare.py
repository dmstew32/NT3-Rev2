"""Run the reference and the new button code on the same randomized button
sessions and compare their traces.

usage: compare.py SIM_REFERENCE.exe SIM_NEW.exe [SEEDS] [GESTURES]
"""
import os
import subprocess
import sys

ref, new = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
seeds = int(sys.argv[3]) if len(sys.argv) > 3 else 300
gestures = sys.argv[4] if len(sys.argv) > 4 else '40'

lines = 0
for seed in range(1, seeds + 1):
    a = subprocess.run([ref, str(seed), gestures], capture_output=True, text=True, check=True).stdout
    b = subprocess.run([new, str(seed), gestures], capture_output=True, text=True, check=True).stdout
    if a != b:
        al, bl = a.splitlines(), b.splitlines()
        i = next((k for k in range(min(len(al), len(bl))) if al[k] != bl[k]), min(len(al), len(bl)))
        print('MISMATCH seed %d at trace line %d' % (seed, i + 1))
        for k in range(max(0, i - 5), i):
            print('   ', al[k])
        print(' reference:', al[i] if i < len(al) else '<end>')
        print(' new      :', bl[i] if i < len(bl) else '<end>')
        print('rerun: %s %d %s' % (new, seed, gestures))
        sys.exit(1)
    lines += a.count('\n')

print('PASS: %d button sessions, %d gestures each, %d identical trace lines' % (seeds, int(gestures), lines))
