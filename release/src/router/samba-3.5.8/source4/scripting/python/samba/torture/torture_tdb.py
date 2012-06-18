#!/usr/bin/python

import sys, os
import Tdb

def fail(msg):
    print 'FAILED:', msg
    sys.exit(1)

tdb_file = '/tmp/torture_tdb.tdb'

# Create temporary tdb file

t = Tdb.Tdb(tdb_file, flags = Tdb.CLEAR_IF_FIRST)

# Check non-existent key throws KeyError exception

try:
    t['__none__']
except KeyError:
    pass
else:
    fail('non-existent key did not throw KeyError')

# Check storing key

t['bar'] = '1234'
if t['bar'] != '1234':
    fail('store key failed')

# Check key exists

if not t.has_key('bar'):
    fail('has_key() failed for existing key')

if t.has_key('__none__'):
    fail('has_key() succeeded for non-existent key')

# Delete key

try:
    del(t['__none__'])
except KeyError:
    pass
else:
    fail('delete of non-existent key did not throw KeyError')

del t['bar']
if t.has_key('bar'):
    fail('delete of existing key did not delete key')

# Clear all keys

t.clear()
if len(t) != 0:
    fail('clear failed to remove all keys')

# Other dict functions

t['a'] = '1'
t['ab'] = '12'
t['abc'] = '123'

if len(t) != 3:
    fail('len method produced wrong value')

keys = t.keys()
values = t.values()
items = t.items()

if set(keys) != set(['a', 'ab', 'abc']):
    fail('keys method produced wrong values')

if set(values) != set(['1', '12', '123']):
    fail('values method produced wrong values')

if set(items) != set([('a', '1'), ('ab', '12'), ('abc', '123')]):
    fail('values method produced wrong values')

t.close()

# Re-open read-only

t = Tdb.Tdb(tdb_file, open_flags = os.O_RDONLY)
t.keys()
t.close()

# Clean up

os.unlink(tdb_file)
