#!/usr/bin/env python
# Trivial reimplementation of tdbdump in Python

import tdb, sys

if len(sys.argv) < 2:
    print "Usage: tdbdump.py <tdb-file>"
    sys.exit(1)

db = tdb.Tdb(sys.argv[1])
for (k, v) in db.iteritems():
    print "{\nkey(%d) = %r\ndata(%d) = %r\n}" % (len(k), k, len(v), v)
