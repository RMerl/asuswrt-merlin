#
#  subunit: extensions to Python unittest to get test results from subprocesses.
#  Copyright (C) 2009  Robert Collins <robertc@robertcollins.net>
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

import datetime
import unittest
from StringIO import StringIO
import os
import sys

from testtools.content_type import ContentType
from testtools.content import Content

import subunit
import subunit.iso8601 as iso8601
import subunit.test_results


class LoggingDecorator(subunit.test_results.HookedTestResultDecorator):

    def __init__(self, decorated):
        self._calls = 0
        super(LoggingDecorator, self).__init__(decorated)

    def _before_event(self):
        self._calls += 1


class AssertBeforeTestResult(LoggingDecorator):
    """A TestResult for checking preconditions."""

    def __init__(self, decorated, test):
        self.test = test
        super(AssertBeforeTestResult, self).__init__(decorated)

    def _before_event(self):
        self.test.assertEqual(1, self.earlier._calls)
        super(AssertBeforeTestResult, self)._before_event()


class TimeCapturingResult(unittest.TestResult):

    def __init__(self):
        super(TimeCapturingResult, self).__init__()
        self._calls = []

    def time(self, a_datetime):
        self._calls.append(a_datetime)


class TestHookedTestResultDecorator(unittest.TestCase):

    def setUp(self):
        # An end to the chain
        terminal = unittest.TestResult()
        # Asserts that the call was made to self.result before asserter was
        # called.
        asserter = AssertBeforeTestResult(terminal, self)
        # The result object we call, which much increase its call count.
        self.result = LoggingDecorator(asserter)
        asserter.earlier = self.result
        self.decorated = asserter

    def tearDown(self):
        # The hook in self.result must have been called
        self.assertEqual(1, self.result._calls)
        # The hook in asserter must have been called too, otherwise the
        # assertion about ordering won't have completed.
        self.assertEqual(1, self.decorated._calls)

    def test_startTest(self):
        self.result.startTest(self)
        
    def test_startTestRun(self):
        self.result.startTestRun()
        
    def test_stopTest(self):
        self.result.stopTest(self)
        
    def test_stopTestRun(self):
        self.result.stopTestRun()

    def test_addError(self):
        self.result.addError(self, subunit.RemoteError())
        
    def test_addError_details(self):
        self.result.addError(self, details={})
        
    def test_addFailure(self):
        self.result.addFailure(self, subunit.RemoteError())

    def test_addFailure_details(self):
        self.result.addFailure(self, details={})

    def test_addSuccess(self):
        self.result.addSuccess(self)

    def test_addSuccess_details(self):
        self.result.addSuccess(self, details={})

    def test_addSkip(self):
        self.result.addSkip(self, "foo")

    def test_addSkip_details(self):
        self.result.addSkip(self, details={})

    def test_addExpectedFailure(self):
        self.result.addExpectedFailure(self, subunit.RemoteError())

    def test_addExpectedFailure_details(self):
        self.result.addExpectedFailure(self, details={})

    def test_addUnexpectedSuccess(self):
        self.result.addUnexpectedSuccess(self)

    def test_addUnexpectedSuccess_details(self):
        self.result.addUnexpectedSuccess(self, details={})

    def test_progress(self):
        self.result.progress(1, subunit.PROGRESS_SET)

    def test_wasSuccessful(self):
        self.result.wasSuccessful()

    def test_shouldStop(self):
        self.result.shouldStop

    def test_stop(self):
        self.result.stop()

    def test_time(self):
        self.result.time(None)
 

class TestAutoTimingTestResultDecorator(unittest.TestCase):

    def setUp(self):
        # And end to the chain which captures time events.
        terminal = TimeCapturingResult()
        # The result object under test.
        self.result = subunit.test_results.AutoTimingTestResultDecorator(
            terminal)
        self.decorated = terminal

    def test_without_time_calls_time_is_called_and_not_None(self):
        self.result.startTest(self)
        self.assertEqual(1, len(self.decorated._calls))
        self.assertNotEqual(None, self.decorated._calls[0])

    def test_no_time_from_progress(self):
        self.result.progress(1, subunit.PROGRESS_CUR)
        self.assertEqual(0, len(self.decorated._calls))

    def test_no_time_from_shouldStop(self):
        self.decorated.stop()
        self.result.shouldStop
        self.assertEqual(0, len(self.decorated._calls))

    def test_calling_time_inhibits_automatic_time(self):
        # Calling time() outputs a time signal immediately and prevents
        # automatically adding one when other methods are called.
        time = datetime.datetime(2009,10,11,12,13,14,15, iso8601.Utc())
        self.result.time(time)
        self.result.startTest(self)
        self.result.stopTest(self)
        self.assertEqual(1, len(self.decorated._calls))
        self.assertEqual(time, self.decorated._calls[0])

    def test_calling_time_None_enables_automatic_time(self):
        time = datetime.datetime(2009,10,11,12,13,14,15, iso8601.Utc())
        self.result.time(time)
        self.assertEqual(1, len(self.decorated._calls))
        self.assertEqual(time, self.decorated._calls[0])
        # Calling None passes the None through, in case other results care.
        self.result.time(None)
        self.assertEqual(2, len(self.decorated._calls))
        self.assertEqual(None, self.decorated._calls[1])
        # Calling other methods doesn't generate an automatic time event.
        self.result.startTest(self)
        self.assertEqual(3, len(self.decorated._calls))
        self.assertNotEqual(None, self.decorated._calls[2])


def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result
