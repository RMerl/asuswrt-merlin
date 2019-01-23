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

import dns.rdataclass
import dns.rdatatype

class RdTypeAndClassTestCase(unittest.TestCase):

    # Classes
    
    def test_class_meta1(self):
        self.failUnless(dns.rdataclass.is_metaclass(dns.rdataclass.ANY))

    def test_class_meta2(self):
        self.failUnless(not dns.rdataclass.is_metaclass(dns.rdataclass.IN))

    def test_class_bytext1(self):
        self.failUnless(dns.rdataclass.from_text('IN') == dns.rdataclass.IN)

    def test_class_bytext2(self):
        self.failUnless(dns.rdataclass.from_text('CLASS1') ==
                        dns.rdataclass.IN)

    def test_class_bytext_bounds1(self):
        self.failUnless(dns.rdataclass.from_text('CLASS0') == 0)
        self.failUnless(dns.rdataclass.from_text('CLASS65535') == 65535)

    def test_class_bytext_bounds2(self):
        def bad():
            junk = dns.rdataclass.from_text('CLASS65536')
        self.failUnlessRaises(ValueError, bad)

    def test_class_bytext_unknown(self):
        def bad():
            junk = dns.rdataclass.from_text('XXX')
        self.failUnlessRaises(dns.rdataclass.UnknownRdataclass, bad)

    def test_class_totext1(self):
        self.failUnless(dns.rdataclass.to_text(dns.rdataclass.IN) == 'IN')

    def test_class_totext1(self):
        self.failUnless(dns.rdataclass.to_text(999) == 'CLASS999')

    def test_class_totext_bounds1(self):
        def bad():
            junk = dns.rdataclass.to_text(-1)
        self.failUnlessRaises(ValueError, bad)

    def test_class_totext_bounds2(self):
        def bad():
            junk = dns.rdataclass.to_text(65536)
        self.failUnlessRaises(ValueError, bad)

    # Types
    
    def test_type_meta1(self):
        self.failUnless(dns.rdatatype.is_metatype(dns.rdatatype.ANY))

    def test_type_meta2(self):
        self.failUnless(dns.rdatatype.is_metatype(dns.rdatatype.OPT))

    def test_type_meta3(self):
        self.failUnless(not dns.rdatatype.is_metatype(dns.rdatatype.A))

    def test_type_singleton1(self):
        self.failUnless(dns.rdatatype.is_singleton(dns.rdatatype.SOA))

    def test_type_singleton2(self):
        self.failUnless(not dns.rdatatype.is_singleton(dns.rdatatype.A))

    def test_type_bytext1(self):
        self.failUnless(dns.rdatatype.from_text('A') == dns.rdatatype.A)

    def test_type_bytext2(self):
        self.failUnless(dns.rdatatype.from_text('TYPE1') ==
                        dns.rdatatype.A)

    def test_type_bytext_bounds1(self):
        self.failUnless(dns.rdatatype.from_text('TYPE0') == 0)
        self.failUnless(dns.rdatatype.from_text('TYPE65535') == 65535)

    def test_type_bytext_bounds2(self):
        def bad():
            junk = dns.rdatatype.from_text('TYPE65536')
        self.failUnlessRaises(ValueError, bad)

    def test_type_bytext_unknown(self):
        def bad():
            junk = dns.rdatatype.from_text('XXX')
        self.failUnlessRaises(dns.rdatatype.UnknownRdatatype, bad)

    def test_type_totext1(self):
        self.failUnless(dns.rdatatype.to_text(dns.rdatatype.A) == 'A')

    def test_type_totext1(self):
        self.failUnless(dns.rdatatype.to_text(999) == 'TYPE999')

    def test_type_totext_bounds1(self):
        def bad():
            junk = dns.rdatatype.to_text(-1)
        self.failUnlessRaises(ValueError, bad)

    def test_type_totext_bounds2(self):
        def bad():
            junk = dns.rdatatype.to_text(65536)
        self.failUnlessRaises(ValueError, bad)

if __name__ == '__main__':
    unittest.main()
