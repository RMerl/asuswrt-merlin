#
#  subunit: extensions to python unittest to get test results from subprocesses.
#  Copyright (C) 2005  Robert Collins <robertc@robertcollins.net>
#
#  Licensed under either the Apache License, Version 2.0 or the BSD 3-clause
#  license at the users choice. A copy of both licenses are available in the
#  project source as Apache-2.0 and BSD. You may not use this file except in
#  compliance with one of these two licences.
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under these licenses is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
#  license you chose for the specific language governing permissions and
#  limitations under that license.
#

"""Tests for subunit.tag_stream."""

import unittest
from StringIO import StringIO

import subunit
import subunit.test_results


class TestSubUnitTags(unittest.TestCase):

    def setUp(self):
        self.original = StringIO()
        self.filtered = StringIO()

    def test_add_tag(self):
        self.original.write("tags: foo\n")
        self.original.write("test: test\n")
        self.original.write("tags: bar -quux\n")
        self.original.write("success: test\n")
        self.original.seek(0)
        result = subunit.tag_stream(self.original, self.filtered, ["quux"])
        self.assertEqual([
            "tags: quux",
            "tags: foo",
            "test: test",
            "tags: bar",
            "success: test",
            ],
            self.filtered.getvalue().splitlines())

    def test_remove_tag(self):
        self.original.write("tags: foo\n")
        self.original.write("test: test\n")
        self.original.write("tags: bar -quux\n")
        self.original.write("success: test\n")
        self.original.seek(0)
        result = subunit.tag_stream(self.original, self.filtered, ["-bar"])
        self.assertEqual([
            "tags: -bar",
            "tags: foo",
            "test: test",
            "tags: -quux",
            "success: test",
            ],
            self.filtered.getvalue().splitlines())


def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result
