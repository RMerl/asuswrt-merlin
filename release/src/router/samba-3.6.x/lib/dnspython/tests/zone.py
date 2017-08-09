# Copyright (C) 2003-2007, 2009, 2010 Nominum, Inc.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose with or without fee is hereby granted,
# provided that the above copyright notice and this permission notice
# appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND NOMINUM DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL NOMINUM BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import cStringIO
import filecmp
import os
import unittest

import dns.exception
import dns.rdata
import dns.rdataclass
import dns.rdatatype
import dns.rrset
import dns.zone

example_text = """$TTL 3600
$ORIGIN example.
@ soa foo bar 1 2 3 4 5
@ ns ns1
@ ns ns2
ns1 a 10.0.0.1
ns2 a 10.0.0.2
$TTL 300
$ORIGIN foo.example.
bar mx 0 blaz
"""

example_text_output = """@ 3600 IN SOA foo bar 1 2 3 4 5
@ 3600 IN NS ns1
@ 3600 IN NS ns2
bar.foo 300 IN MX 0 blaz.foo
ns1 3600 IN A 10.0.0.1
ns2 3600 IN A 10.0.0.2
"""

something_quite_similar = """@ 3600 IN SOA foo bar 1 2 3 4 5
@ 3600 IN NS ns1
@ 3600 IN NS ns2
bar.foo 300 IN MX 0 blaz.foo
ns1 3600 IN A 10.0.0.1
ns2 3600 IN A 10.0.0.3
"""

something_different = """@ 3600 IN SOA fooa bar 1 2 3 4 5
@ 3600 IN NS ns11
@ 3600 IN NS ns21
bar.fooa 300 IN MX 0 blaz.fooa
ns11 3600 IN A 10.0.0.11
ns21 3600 IN A 10.0.0.21
"""

ttl_example_text = """$TTL 1h
$ORIGIN example.
@ soa foo bar 1 2 3 4 5
@ ns ns1
@ ns ns2
ns1 1d1s a 10.0.0.1
ns2 1w1D1h1m1S a 10.0.0.2
"""

no_soa_text = """$TTL 1h
$ORIGIN example.
@ ns ns1
@ ns ns2
ns1 1d1s a 10.0.0.1
ns2 1w1D1h1m1S a 10.0.0.2
"""

no_ns_text = """$TTL 1h
$ORIGIN example.
@ soa foo bar 1 2 3 4 5
"""

include_text = """$INCLUDE "example"
"""

bad_directive_text = """$FOO bar
$ORIGIN example.
@ soa foo bar 1 2 3 4 5
@ ns ns1
@ ns ns2
ns1 1d1s a 10.0.0.1
ns2 1w1D1h1m1S a 10.0.0.2
"""

_keep_output = False

class ZoneTestCase(unittest.TestCase):

    def testFromFile1(self):
        z = dns.zone.from_file('example', 'example')
        ok = False
        try:
            z.to_file('example1.out', nl='\x0a')
            ok = filecmp.cmp('example1.out', 'example1.good')
        finally:
            if not _keep_output:
                os.unlink('example1.out')
        self.failUnless(ok)

    def testFromFile2(self):
        z = dns.zone.from_file('example', 'example', relativize=False)
        ok = False
        try:
            z.to_file('example2.out', relativize=False, nl='\x0a')
            ok = filecmp.cmp('example2.out', 'example2.good')
        finally:
            if not _keep_output:
                os.unlink('example2.out')
        self.failUnless(ok)

    def testFromText(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        f = cStringIO.StringIO()
        names = z.nodes.keys()
        names.sort()
        for n in names:
            print >> f, z[n].to_text(n)
        self.failUnless(f.getvalue() == example_text_output)
            
    def testTorture1(self):
        #
        # Read a zone containing all our supported RR types, and
        # for each RR in the zone, convert the rdata into wire format
        # and then back out, and see if we get equal rdatas.
        #
        f = cStringIO.StringIO()
        o = dns.name.from_text('example.')
        z = dns.zone.from_file('example', o)
        for (name, node) in z.iteritems():
            for rds in node:
                for rd in rds:
                    f.seek(0)
                    f.truncate()
                    rd.to_wire(f, origin=o)
                    wire = f.getvalue()
                    rd2 = dns.rdata.from_wire(rds.rdclass, rds.rdtype,
                                              wire, 0, len(wire),
                                              origin = o)
                    self.failUnless(rd == rd2)

    def testEqual(self):
        z1 = dns.zone.from_text(example_text, 'example.', relativize=True)
        z2 = dns.zone.from_text(example_text_output, 'example.',
                                relativize=True)
        self.failUnless(z1 == z2)

    def testNotEqual1(self):
        z1 = dns.zone.from_text(example_text, 'example.', relativize=True)
        z2 = dns.zone.from_text(something_quite_similar, 'example.',
                                relativize=True)
        self.failUnless(z1 != z2)

    def testNotEqual2(self):
        z1 = dns.zone.from_text(example_text, 'example.', relativize=True)
        z2 = dns.zone.from_text(something_different, 'example.',
                                relativize=True)
        self.failUnless(z1 != z2)

    def testNotEqual3(self):
        z1 = dns.zone.from_text(example_text, 'example.', relativize=True)
        z2 = dns.zone.from_text(something_different, 'example2.',
                                relativize=True)
        self.failUnless(z1 != z2)

    def testFindRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rds = z.find_rdataset('@', 'soa')
        exrds = dns.rdataset.from_text('IN', 'SOA', 300, 'foo bar 1 2 3 4 5')
        self.failUnless(rds == exrds)

    def testFindRdataset2(self):
        def bad():
            z = dns.zone.from_text(example_text, 'example.', relativize=True)
            rds = z.find_rdataset('@', 'loc')
        self.failUnlessRaises(KeyError, bad)

    def testFindRRset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rrs = z.find_rrset('@', 'soa')
        exrrs = dns.rrset.from_text('@', 300, 'IN', 'SOA', 'foo bar 1 2 3 4 5')
        self.failUnless(rrs == exrrs)

    def testFindRRset2(self):
        def bad():
            z = dns.zone.from_text(example_text, 'example.', relativize=True)
            rrs = z.find_rrset('@', 'loc')
        self.failUnlessRaises(KeyError, bad)

    def testGetRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rds = z.get_rdataset('@', 'soa')
        exrds = dns.rdataset.from_text('IN', 'SOA', 300, 'foo bar 1 2 3 4 5')
        self.failUnless(rds == exrds)

    def testGetRdataset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rds = z.get_rdataset('@', 'loc')
        self.failUnless(rds == None)

    def testGetRRset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rrs = z.get_rrset('@', 'soa')
        exrrs = dns.rrset.from_text('@', 300, 'IN', 'SOA', 'foo bar 1 2 3 4 5')
        self.failUnless(rrs == exrrs)

    def testGetRRset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rrs = z.get_rrset('@', 'loc')
        self.failUnless(rrs == None)

    def testReplaceRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rdataset = dns.rdataset.from_text('in', 'ns', 300, 'ns3', 'ns4')
        z.replace_rdataset('@', rdataset)
        rds = z.get_rdataset('@', 'ns')
        self.failUnless(rds is rdataset)

    def testReplaceRdataset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        rdataset = dns.rdataset.from_text('in', 'txt', 300, '"foo"')
        z.replace_rdataset('@', rdataset)
        rds = z.get_rdataset('@', 'txt')
        self.failUnless(rds is rdataset)

    def testDeleteRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        z.delete_rdataset('@', 'ns')
        rds = z.get_rdataset('@', 'ns')
        self.failUnless(rds is None)

    def testDeleteRdataset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        z.delete_rdataset('ns1', 'a')
        node = z.get_node('ns1')
        self.failUnless(node is None)

    def testNodeFindRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        node = z['@']
        rds = node.find_rdataset(dns.rdataclass.IN, dns.rdatatype.SOA)
        exrds = dns.rdataset.from_text('IN', 'SOA', 300, 'foo bar 1 2 3 4 5')
        self.failUnless(rds == exrds)

    def testNodeFindRdataset2(self):
        def bad():
            z = dns.zone.from_text(example_text, 'example.', relativize=True)
            node = z['@']
            rds = node.find_rdataset(dns.rdataclass.IN, dns.rdatatype.LOC)
        self.failUnlessRaises(KeyError, bad)

    def testNodeGetRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        node = z['@']
        rds = node.get_rdataset(dns.rdataclass.IN, dns.rdatatype.SOA)
        exrds = dns.rdataset.from_text('IN', 'SOA', 300, 'foo bar 1 2 3 4 5')
        self.failUnless(rds == exrds)

    def testNodeGetRdataset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        node = z['@']
        rds = node.get_rdataset(dns.rdataclass.IN, dns.rdatatype.LOC)
        self.failUnless(rds == None)

    def testNodeDeleteRdataset1(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        node = z['@']
        rds = node.delete_rdataset(dns.rdataclass.IN, dns.rdatatype.SOA)
        rds = node.get_rdataset(dns.rdataclass.IN, dns.rdatatype.SOA)
        self.failUnless(rds == None)

    def testNodeDeleteRdataset2(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        node = z['@']
        rds = node.delete_rdataset(dns.rdataclass.IN, dns.rdatatype.LOC)
        rds = node.get_rdataset(dns.rdataclass.IN, dns.rdatatype.LOC)
        self.failUnless(rds == None)

    def testIterateRdatasets(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        ns = [n for n, r in z.iterate_rdatasets('A')]
        ns.sort()
        self.failUnless(ns == [dns.name.from_text('ns1', None),
                               dns.name.from_text('ns2', None)])

    def testIterateAllRdatasets(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        ns = [n for n, r in z.iterate_rdatasets()]
        ns.sort()
        self.failUnless(ns == [dns.name.from_text('@', None),
                               dns.name.from_text('@', None),
                               dns.name.from_text('bar.foo', None),
                               dns.name.from_text('ns1', None),
                               dns.name.from_text('ns2', None)])

    def testIterateRdatas(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        l = list(z.iterate_rdatas('A'))
        l.sort()
        exl = [(dns.name.from_text('ns1', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.A,
                                    '10.0.0.1')),
               (dns.name.from_text('ns2', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.A,
                                    '10.0.0.2'))]
        self.failUnless(l == exl)

    def testIterateAllRdatas(self):
        z = dns.zone.from_text(example_text, 'example.', relativize=True)
        l = list(z.iterate_rdatas())
        l.sort()
        exl = [(dns.name.from_text('@', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.NS,
                                    'ns1')),
               (dns.name.from_text('@', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.NS,
                                    'ns2')),
               (dns.name.from_text('@', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.SOA,
                                    'foo bar 1 2 3 4 5')),
               (dns.name.from_text('bar.foo', None),
                300,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.MX,
                                    '0 blaz.foo')),
               (dns.name.from_text('ns1', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.A,
                                    '10.0.0.1')),
               (dns.name.from_text('ns2', None),
                3600,
                dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.A,
                                    '10.0.0.2'))]
        self.failUnless(l == exl)

    def testTTLs(self):
        z = dns.zone.from_text(ttl_example_text, 'example.', relativize=True)
        n = z['@']
        rds = n.get_rdataset(dns.rdataclass.IN, dns.rdatatype.SOA)
        self.failUnless(rds.ttl == 3600)
        n = z['ns1']
        rds = n.get_rdataset(dns.rdataclass.IN, dns.rdatatype.A)
        self.failUnless(rds.ttl == 86401)
        n = z['ns2']
        rds = n.get_rdataset(dns.rdataclass.IN, dns.rdatatype.A)
        self.failUnless(rds.ttl == 694861)

    def testNoSOA(self):
        def bad():
            z = dns.zone.from_text(no_soa_text, 'example.',
                                   relativize=True)
        self.failUnlessRaises(dns.zone.NoSOA, bad)

    def testNoNS(self):
        def bad():
            z = dns.zone.from_text(no_ns_text, 'example.',
                                   relativize=True)
        self.failUnlessRaises(dns.zone.NoNS, bad)

    def testInclude(self):
        z1 = dns.zone.from_text(include_text, 'example.', relativize=True,
                                allow_include=True)
        z2 = dns.zone.from_file('example', 'example.', relativize=True)
        self.failUnless(z1 == z2)

    def testBadDirective(self):
        def bad():
            z = dns.zone.from_text(bad_directive_text, 'example.',
                                   relativize=True)
        self.failUnlessRaises(dns.exception.SyntaxError, bad)

if __name__ == '__main__':
    unittest.main()
