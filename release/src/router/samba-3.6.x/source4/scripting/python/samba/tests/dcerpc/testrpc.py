#!/usr/bin/env python
#
# test generated python code from pidl
# Andrew Tridgell August 2010
#
import sys

sys.path.insert(0, "bin/python")

import samba
import samba.tests
from samba.dcerpc import drsuapi
import talloc

talloc.enable_null_tracking()

class RpcTests(object):
    '''test type behaviour of pidl generated python RPC code'''

    def check_blocks(self, object, num_expected):
        '''check that the number of allocated blocks is correct'''
        nblocks = talloc.total_blocks(object)
        if object is None:
            nblocks -= self.initial_blocks
        leaked_blocks = (nblocks - num_expected)
        if leaked_blocks != 0:
            print "Leaked %d blocks" % leaked_blocks

    def check_type(self, interface, typename, type):
        print "Checking type %s" % typename
        v = type()
        for n in dir(v):
            if n[0] == '_':
                continue
            try:
                value = getattr(v, n)
            except TypeError, errstr:
                if str(errstr) == "unknown union level":
                    print "ERROR: Unknown union level in %s.%s" % (typename, n)
                    self.errcount += 1
                    continue
                print str(errstr)[1:21]
                if str(errstr)[0:21] == "Can not convert C Type":
                    print "ERROR: Unknown C type for %s.%s" % (typename, n)
                    self.errcount += 1
                    continue
                else:
                    print "ERROR: Failed to instantiate %s.%s" % (typename, n)
                    self.errcount += 1
                    continue
            except:
                print "ERROR: Failed to instantiate %s.%s" % (typename, n)
                self.errcount += 1
                continue

            # now try setting the value back
            try:
                print "Setting %s.%s" % (typename, n)
                setattr(v, n, value)
            except Exception, e:
                if isinstance(e, AttributeError) and str(e).endswith("is read-only"):
                    # readonly, ignore
                    continue
                else:
                    print "ERROR: Failed to set %s.%s: %r: %s" % (typename, n, e.__class__, e)
                    self.errcount += 1
                    continue

            # and try a comparison
            try:
                if value != getattr(v, n):
                    print "ERROR: Comparison failed for %s.%s: %r != %r" % (typename, n, value, getattr(v, n))
                    continue
            except Exception, e:
                print "ERROR: compare exception for %s.%s: %r: %s" % (typename, n, e.__class__, e)
                continue

    def check_interface(self, interface, iname):
        errcount = self.errcount
        for n in dir(interface):
            if n[0] == '_' or n == iname:
                # skip the special ones
                continue
            value = getattr(interface, n)
            if isinstance(value, str):
                #print "%s=\"%s\"" % (n, value)
                pass
            elif isinstance(value, int) or isinstance(value, long):
                #print "%s=%d" % (n, value)
                pass
            elif isinstance(value, type):
                try:
                    initial_blocks = talloc.total_blocks(None)
                    self.check_type(interface, n, value)
                    self.check_blocks(None, initial_blocks)
                except Exception, e:
                    print "ERROR: Failed to check_type %s.%s: %r: %s" % (iname, n, e.__class__, e)
                    self.errcount += 1
            elif callable(value):
                pass # Method
            else:
                print "UNKNOWN: %s=%s" % (n, value)
        if self.errcount - errcount != 0:
            print "Found %d errors in %s" % (self.errcount - errcount, iname)

    def check_all_interfaces(self):
        for iname in dir(samba.dcerpc):
            if iname[0] == '_':
                continue
            if iname == 'ClientConnection' or iname == 'base':
                continue
            print "Checking interface %s" % iname
            iface = getattr(samba.dcerpc, iname)
            initial_blocks = talloc.total_blocks(None)
            self.check_interface(iface, iname)
            self.check_blocks(None, initial_blocks)

    def run(self):
        self.initial_blocks = talloc.total_blocks(None)
        self.errcount = 0
        self.check_all_interfaces()
        return self.errcount

tests = RpcTests()
errcount = tests.run()
if errcount == 0:
    sys.exit(0)
else:
    print "%d failures" % errcount
    sys.exit(1)
