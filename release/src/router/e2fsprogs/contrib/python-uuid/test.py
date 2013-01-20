#!/usr/bin/python
import e2fsprogs_uuid
import time

print "Generating uuid...",
try:
    time = time.time()
    u = e2fsprogs_uuid.generate()
except:
    u = "FAIL"
print u, "...", time

print "Calling generate with param...",
try:
    e2fsprogs_uuid.generate("param")
    print "FAIL."
except:
    print "OK"
