import sys
import re

start_re = "(^.*released under the GPL version 2 \(see below\)).*however due"
skip = 0
while True:
    line = sys.stdin.readline()
    if not line:
        break
    m = re.match (start_re, line)
    if m:
        g = m.groups()
        print g[0] + '.'
        skip = 5
    if skip > 0:
        skip -= 1
    else:
        print line,

sys.exit(0)
