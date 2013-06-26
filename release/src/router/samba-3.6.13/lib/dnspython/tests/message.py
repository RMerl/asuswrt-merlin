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
import os
import unittest

import dns.exception
import dns.message

query_text = """id 1234
opcode QUERY
rcode NOERROR
flags RD
edns 0
eflags DO
payload 4096
;QUESTION
wwww.dnspython.org. IN A
;ANSWER
;AUTHORITY
;ADDITIONAL"""

goodhex = '04d201000001000000000001047777777709646e73707974686f6e' \
          '036f726700000100010000291000000080000000'

goodwire = goodhex.decode('hex_codec')

answer_text = """id 1234
opcode QUERY
rcode NOERROR
flags QR AA RD
;QUESTION
dnspython.org. IN SOA
;ANSWER
dnspython.org. 3600 IN SOA woof.dnspython.org. hostmaster.dnspython.org. 2003052700 3600 1800 604800 3600
;AUTHORITY
dnspython.org. 3600 IN NS ns1.staff.nominum.org.
dnspython.org. 3600 IN NS ns2.staff.nominum.org.
dnspython.org. 3600 IN NS woof.play-bow.org.
;ADDITIONAL
woof.play-bow.org. 3600 IN A 204.152.186.150
"""

goodhex2 = '04d2 8500 0001 0001 0003 0001' \
           '09646e73707974686f6e036f726700 0006 0001' \
           'c00c 0006 0001 00000e10 0028 ' \
               '04776f6f66c00c 0a686f73746d6173746572c00c' \
               '7764289c 00000e10 00000708 00093a80 00000e10' \
           'c00c 0002 0001 00000e10 0014' \
               '036e7331057374616666076e6f6d696e756dc016' \
           'c00c 0002 0001 00000e10 0006 036e7332c063' \
           'c00c 0002 0001 00000e10 0010 04776f6f6608706c61792d626f77c016' \
           'c091 0001 0001 00000e10 0004 cc98ba96'


goodwire2 = goodhex2.replace(' ', '').decode('hex_codec')

query_text_2 = """id 1234
opcode QUERY
rcode 4095
flags RD
edns 0
eflags DO
payload 4096
;QUESTION
wwww.dnspython.org. IN A
;ANSWER
;AUTHORITY
;ADDITIONAL"""

goodhex3 = '04d2010f0001000000000001047777777709646e73707974686f6e' \
          '036f726700000100010000291000ff0080000000'

goodwire3 = goodhex3.decode('hex_codec')

class MessageTestCase(unittest.TestCase):

    def test_comparison_eq1(self):
        q1 = dns.message.from_text(query_text)
        q2 = dns.message.from_text(query_text)
        self.failUnless(q1 == q2)

    def test_comparison_ne1(self):
        q1 = dns.message.from_text(query_text)
        q2 = dns.message.from_text(query_text)
        q2.id = 10
        self.failUnless(q1 != q2)

    def test_comparison_ne2(self):
        q1 = dns.message.from_text(query_text)
        q2 = dns.message.from_text(query_text)
        q2.question = []
        self.failUnless(q1 != q2)

    def test_comparison_ne3(self):
        q1 = dns.message.from_text(query_text)
        self.failUnless(q1 != 1)

    def test_EDNS_to_wire1(self):
        q = dns.message.from_text(query_text)
        w = q.to_wire()
        self.failUnless(w == goodwire)

    def test_EDNS_from_wire1(self):
        m = dns.message.from_wire(goodwire)
        self.failUnless(str(m) == query_text)

    def test_EDNS_to_wire2(self):
        q = dns.message.from_text(query_text_2)
        w = q.to_wire()
        self.failUnless(w == goodwire3)

    def test_EDNS_from_wire2(self):
        m = dns.message.from_wire(goodwire3)
        self.failUnless(str(m) == query_text_2)

    def test_TooBig(self):
        def bad():
            q = dns.message.from_text(query_text)
            for i in xrange(0, 25):
                rrset = dns.rrset.from_text('foo%d.' % i, 3600,
                                            dns.rdataclass.IN,
                                            dns.rdatatype.A,
                                            '10.0.0.%d' % i)
                q.additional.append(rrset)
            w = q.to_wire(max_size=512)
        self.failUnlessRaises(dns.exception.TooBig, bad)

    def test_answer1(self):
        a = dns.message.from_text(answer_text)
        wire = a.to_wire(want_shuffle=False)
        self.failUnless(wire == goodwire2)

    def test_TrailingJunk(self):
        def bad():
            badwire = goodwire + '\x00'
            m = dns.message.from_wire(badwire)
        self.failUnlessRaises(dns.message.TrailingJunk, bad)

    def test_ShortHeader(self):
        def bad():
            badwire = '\x00' * 11
            m = dns.message.from_wire(badwire)
        self.failUnlessRaises(dns.message.ShortHeader, bad)

    def test_RespondingToResponse(self):
        def bad():
            q = dns.message.make_query('foo', 'A')
            r1 = dns.message.make_response(q)
            r2 = dns.message.make_response(r1)
        self.failUnlessRaises(dns.exception.FormError, bad)

    def test_ExtendedRcodeSetting(self):
        m = dns.message.make_query('foo', 'A')
        m.set_rcode(4095)
        self.failUnless(m.rcode() == 4095)
        m.set_rcode(2)
        self.failUnless(m.rcode() == 2)

    def test_EDNSVersionCoherence(self):
        m = dns.message.make_query('foo', 'A')
        m.use_edns(1)
        self.failUnless((m.ednsflags >> 16) & 0xFF == 1)

if __name__ == '__main__':
    unittest.main()
