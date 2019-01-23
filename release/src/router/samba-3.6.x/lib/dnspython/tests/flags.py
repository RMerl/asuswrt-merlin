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

import unittest

import dns.flags
import dns.rcode
import dns.opcode

class FlagsTestCase(unittest.TestCase):

    def test_rcode1(self):
        self.failUnless(dns.rcode.from_text('FORMERR') ==  dns.rcode.FORMERR)

    def test_rcode2(self):
        self.failUnless(dns.rcode.to_text(dns.rcode.FORMERR) == "FORMERR")

    def test_rcode3(self):
        self.failUnless(dns.rcode.to_flags(dns.rcode.FORMERR) == (1, 0))

    def test_rcode4(self):
        self.failUnless(dns.rcode.to_flags(dns.rcode.BADVERS) == \
                        (0, 0x01000000))

    def test_rcode6(self):
        self.failUnless(dns.rcode.from_flags(0, 0x01000000) == \
                        dns.rcode.BADVERS)

    def test_rcode6(self):
        self.failUnless(dns.rcode.from_flags(5, 0) == dns.rcode.REFUSED)

    def test_rcode7(self):
        def bad():
            dns.rcode.to_flags(4096)
        self.failUnlessRaises(ValueError, bad)

    def test_flags1(self):
        self.failUnless(dns.flags.from_text("RA RD AA QR") == \
                        dns.flags.QR|dns.flags.AA|dns.flags.RD|dns.flags.RA)

    def test_flags2(self):
        flags = dns.flags.QR|dns.flags.AA|dns.flags.RD|dns.flags.RA
        self.failUnless(dns.flags.to_text(flags) == "QR AA RD RA")


if __name__ == '__main__':
    unittest.main()
