# Copyright 2013-2015, The Tor Project, Inc
# See LICENSE for licensing information

"""
bt_test.py

This file tests the output from test-bt-cl to make sure it's as expected.

Example usage:

$ ./src/test/test-bt-cl crash | ./src/test/bt_test.py
OK
$ ./src/test/test-bt-cl assert | ./src/test/bt_test.py
OK

"""

from __future__ import print_function
import sys


def matches(lines, funcs):
    if len(lines) < len(funcs):
        return False
    try:
        for l, f in zip(lines, funcs):
            l.index(f)
    except ValueError:
        return False
    else:
        return True

FUNCNAMES = "crash oh_what a_tangled_web we_weave main".split()

LINES = sys.stdin.readlines()

for I in range(len(LINES)):
    if matches(LINES[I:], FUNCNAMES):
        print("OK")
        sys.exit(0)

print("BAD")

for l in LINES:
    print("{}".format(l), end="")

if sys.platform.startswith('freebsd'):
    # See bug #17808 if you know how to fix this.
    print("Test failed; but FreeBSD is known to have backtrace problems.\n"
          "Treating as 'SKIP'.")
    sys.exit(77)

sys.exit(1)
