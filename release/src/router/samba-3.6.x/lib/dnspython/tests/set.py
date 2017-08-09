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

import dns.set

# for convenience
S = dns.set.Set

class SimpleSetTestCase(unittest.TestCase):
        
    def testLen1(self):
        s1 = S()
        self.failUnless(len(s1) == 0)

    def testLen2(self):
        s1 = S([1, 2, 3])
        self.failUnless(len(s1) == 3)

    def testLen3(self):
        s1 = S([1, 2, 3, 3, 3])
        self.failUnless(len(s1) == 3)

    def testUnion1(self):
        s1 = S([1, 2, 3])
        s2 = S([1, 2, 3])
        e = S([1, 2, 3])
        self.failUnless(s1 | s2 == e)

    def testUnion2(self):
        s1 = S([1, 2, 3])
        s2 = S([])
        e = S([1, 2, 3])
        self.failUnless(s1 | s2 == e)

    def testUnion3(self):
        s1 = S([1, 2, 3])
        s2 = S([3, 4])
        e = S([1, 2, 3, 4])
        self.failUnless(s1 | s2 == e)

    def testIntersection1(self):
        s1 = S([1, 2, 3])
        s2 = S([1, 2, 3])
        e = S([1, 2, 3])
        self.failUnless(s1 & s2 == e)

    def testIntersection2(self):
        s1 = S([0, 1, 2, 3])
        s2 = S([1, 2, 3, 4])
        e = S([1, 2, 3])
        self.failUnless(s1 & s2 == e)

    def testIntersection3(self):
        s1 = S([1, 2, 3])
        s2 = S([])
        e = S([])
        self.failUnless(s1 & s2 == e)

    def testIntersection4(self):
        s1 = S([1, 2, 3])
        s2 = S([5, 4])
        e = S([])
        self.failUnless(s1 & s2 == e)

    def testDifference1(self):
        s1 = S([1, 2, 3])
        s2 = S([5, 4])
        e = S([1, 2, 3])
        self.failUnless(s1 - s2 == e)

    def testDifference2(self):
        s1 = S([1, 2, 3])
        s2 = S([])
        e = S([1, 2, 3])
        self.failUnless(s1 - s2 == e)

    def testDifference3(self):
        s1 = S([1, 2, 3])
        s2 = S([3, 2])
        e = S([1])
        self.failUnless(s1 - s2 == e)

    def testDifference4(self):
        s1 = S([1, 2, 3])
        s2 = S([3, 2, 1])
        e = S([])
        self.failUnless(s1 - s2 == e)

    def testSubset1(self):
        s1 = S([1, 2, 3])
        s2 = S([3, 2, 1])
        self.failUnless(s1.issubset(s2))

    def testSubset2(self):
        s1 = S([1, 2, 3])
        self.failUnless(s1.issubset(s1))

    def testSubset3(self):
        s1 = S([])
        s2 = S([1, 2, 3])
        self.failUnless(s1.issubset(s2))

    def testSubset4(self):
        s1 = S([1])
        s2 = S([1, 2, 3])
        self.failUnless(s1.issubset(s2))

    def testSubset5(self):
        s1 = S([])
        s2 = S([])
        self.failUnless(s1.issubset(s2))

    def testSubset6(self):
        s1 = S([1, 4])
        s2 = S([1, 2, 3])
        self.failUnless(not s1.issubset(s2))

    def testSuperset1(self):
        s1 = S([1, 2, 3])
        s2 = S([3, 2, 1])
        self.failUnless(s1.issuperset(s2))

    def testSuperset2(self):
        s1 = S([1, 2, 3])
        self.failUnless(s1.issuperset(s1))

    def testSuperset3(self):
        s1 = S([1, 2, 3])
        s2 = S([])
        self.failUnless(s1.issuperset(s2))

    def testSuperset4(self):
        s1 = S([1, 2, 3])
        s2 = S([1])
        self.failUnless(s1.issuperset(s2))

    def testSuperset5(self):
        s1 = S([])
        s2 = S([])
        self.failUnless(s1.issuperset(s2))

    def testSuperset6(self):
        s1 = S([1, 2, 3])
        s2 = S([1, 4])
        self.failUnless(not s1.issuperset(s2))

    def testUpdate1(self):
        s1 = S([1, 2, 3])
        u = (4, 5, 6)
        e = S([1, 2, 3, 4, 5, 6])
        s1.update(u)
        self.failUnless(s1 == e)

    def testUpdate2(self):
        s1 = S([1, 2, 3])
        u = []
        e = S([1, 2, 3])
        s1.update(u)
        self.failUnless(s1 == e)

    def testGetitem(self):
        s1 = S([1, 2, 3])
        i0 = s1[0]
        i1 = s1[1]
        i2 = s1[2]
        s2 = S([i0, i1, i2])
        self.failUnless(s1 == s2)

    def testGetslice(self):
        s1 = S([1, 2, 3])
        slice = s1[0:2]
        self.failUnless(len(slice) == 2)
        item = s1[2]
        slice.append(item)
        s2 = S(slice)
        self.failUnless(s1 == s2)

    def testDelitem(self):
        s1 = S([1, 2, 3])
        del s1[0]
        i1 = s1[0]
        i2 = s1[1]
        self.failUnless(i1 != i2)
        self.failUnless(i1 == 1 or i1 == 2 or i1 == 3)
        self.failUnless(i2 == 1 or i2 == 2 or i2 == 3)

    def testDelslice(self):
        s1 = S([1, 2, 3])
        del s1[0:2]
        i1 = s1[0]
        self.failUnless(i1 == 1 or i1 == 2 or i1 == 3)

if __name__ == '__main__':
    unittest.main()
