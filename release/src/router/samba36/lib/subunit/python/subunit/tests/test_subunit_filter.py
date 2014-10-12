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

"""Tests for subunit.TestResultFilter."""

import unittest
from StringIO import StringIO

import subunit
from subunit.test_results import TestResultFilter


class TestTestResultFilter(unittest.TestCase):
    """Test for TestResultFilter, a TestResult object which filters tests."""

    def _setUp(self):
        self.output = StringIO()

    def test_default(self):
        """The default is to exclude success and include everything else."""
        self.filtered_result = unittest.TestResult()
        self.filter = TestResultFilter(self.filtered_result)
        self.run_tests()
        # skips are seen as success by default python TestResult.
        self.assertEqual(['error'],
            [error[0].id() for error in self.filtered_result.errors])
        self.assertEqual(['failed'],
            [failure[0].id() for failure in
            self.filtered_result.failures])
        self.assertEqual(4, self.filtered_result.testsRun)

    def test_exclude_errors(self):
        self.filtered_result = unittest.TestResult()
        self.filter = TestResultFilter(self.filtered_result,
            filter_error=True)
        self.run_tests()
        # skips are seen as errors by default python TestResult.
        self.assertEqual([], self.filtered_result.errors)
        self.assertEqual(['failed'],
            [failure[0].id() for failure in
            self.filtered_result.failures])
        self.assertEqual(3, self.filtered_result.testsRun)

    def test_exclude_failure(self):
        self.filtered_result = unittest.TestResult()
        self.filter = TestResultFilter(self.filtered_result,
            filter_failure=True)
        self.run_tests()
        self.assertEqual(['error'],
            [error[0].id() for error in self.filtered_result.errors])
        self.assertEqual([],
            [failure[0].id() for failure in
            self.filtered_result.failures])
        self.assertEqual(3, self.filtered_result.testsRun)

    def test_exclude_skips(self):
        self.filtered_result = subunit.TestResultStats(None)
        self.filter = TestResultFilter(self.filtered_result,
            filter_skip=True)
        self.run_tests()
        self.assertEqual(0, self.filtered_result.skipped_tests)
        self.assertEqual(2, self.filtered_result.failed_tests)
        self.assertEqual(3, self.filtered_result.testsRun)

    def test_include_success(self):
        """Success's can be included if requested."""
        self.filtered_result = unittest.TestResult()
        self.filter = TestResultFilter(self.filtered_result,
            filter_success=False)
        self.run_tests()
        self.assertEqual(['error'],
            [error[0].id() for error in self.filtered_result.errors])
        self.assertEqual(['failed'],
            [failure[0].id() for failure in
            self.filtered_result.failures])
        self.assertEqual(5, self.filtered_result.testsRun)

    def test_filter_predicate(self):
        """You can filter by predicate callbacks"""
        self.filtered_result = unittest.TestResult()
        def filter_cb(test, outcome, err, details):
            return outcome == 'success'
        self.filter = TestResultFilter(self.filtered_result,
            filter_predicate=filter_cb,
            filter_success=False)
        self.run_tests()
        # Only success should pass
        self.assertEqual(1, self.filtered_result.testsRun)

    def run_tests(self):
        self.setUpTestStream()
        self.test = subunit.ProtocolTestCase(self.input_stream)
        self.test.run(self.filter)

    def setUpTestStream(self):
        # While TestResultFilter works on python objects, using a subunit
        # stream is an easy pithy way of getting a series of test objects to
        # call into the TestResult, and as TestResultFilter is intended for
        # use with subunit also has the benefit of detecting any interface
        # skew issues.
        self.input_stream = StringIO()
        self.input_stream.write("""tags: global
test passed
success passed
test failed
tags: local
failure failed
test error
error error [
error details
]
test skipped
skip skipped
test todo
xfail todo
""")
        self.input_stream.seek(0)
    

def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result
