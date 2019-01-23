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

"""TestResult helper classes used to by subunit."""

import datetime

import iso8601
import testtools


# NOT a TestResult, because we are implementing the interface, not inheriting
# it.
class TestResultDecorator(object):
    """General pass-through decorator.

    This provides a base that other TestResults can inherit from to
    gain basic forwarding functionality. It also takes care of
    handling the case where the target doesn't support newer methods
    or features by degrading them.
    """

    def __init__(self, decorated):
        """Create a TestResultDecorator forwarding to decorated."""
        # Make every decorator degrade gracefully.
        self.decorated = testtools.ExtendedToOriginalDecorator(decorated)

    def startTest(self, test):
        return self.decorated.startTest(test)

    def startTestRun(self):
        return self.decorated.startTestRun()

    def stopTest(self, test):
        return self.decorated.stopTest(test)

    def stopTestRun(self):
        return self.decorated.stopTestRun()

    def addError(self, test, err=None, details=None):
        return self.decorated.addError(test, err, details=details)

    def addFailure(self, test, err=None, details=None):
        return self.decorated.addFailure(test, err, details=details)

    def addSuccess(self, test, details=None):
        return self.decorated.addSuccess(test, details=details)

    def addSkip(self, test, reason=None, details=None):
        return self.decorated.addSkip(test, reason, details=details)

    def addExpectedFailure(self, test, err=None, details=None):
        return self.decorated.addExpectedFailure(test, err, details=details)

    def addUnexpectedSuccess(self, test, details=None):
        return self.decorated.addUnexpectedSuccess(test, details=details)

    def progress(self, offset, whence):
        return self.decorated.progress(offset, whence)

    def wasSuccessful(self):
        return self.decorated.wasSuccessful()

    @property
    def shouldStop(self):
        return self.decorated.shouldStop

    def stop(self):
        return self.decorated.stop()

    def tags(self, new_tags, gone_tags):
        return self.decorated.time(new_tags, gone_tags)

    def time(self, a_datetime):
        return self.decorated.time(a_datetime)


class HookedTestResultDecorator(TestResultDecorator):
    """A TestResult which calls a hook on every event."""

    def __init__(self, decorated):
        self.super = super(HookedTestResultDecorator, self)
        self.super.__init__(decorated)

    def startTest(self, test):
        self._before_event()
        return self.super.startTest(test)

    def startTestRun(self):
        self._before_event()
        return self.super.startTestRun()

    def stopTest(self, test):
        self._before_event()
        return self.super.stopTest(test)

    def stopTestRun(self):
        self._before_event()
        return self.super.stopTestRun()

    def addError(self, test, err=None, details=None):
        self._before_event()
        return self.super.addError(test, err, details=details)

    def addFailure(self, test, err=None, details=None):
        self._before_event()
        return self.super.addFailure(test, err, details=details)

    def addSuccess(self, test, details=None):
        self._before_event()
        return self.super.addSuccess(test, details=details)

    def addSkip(self, test, reason=None, details=None):
        self._before_event()
        return self.super.addSkip(test, reason, details=details)

    def addExpectedFailure(self, test, err=None, details=None):
        self._before_event()
        return self.super.addExpectedFailure(test, err, details=details)

    def addUnexpectedSuccess(self, test, details=None):
        self._before_event()
        return self.super.addUnexpectedSuccess(test, details=details)

    def progress(self, offset, whence):
        self._before_event()
        return self.super.progress(offset, whence)

    def wasSuccessful(self):
        self._before_event()
        return self.super.wasSuccessful()

    @property
    def shouldStop(self):
        self._before_event()
        return self.super.shouldStop

    def stop(self):
        self._before_event()
        return self.super.stop()

    def time(self, a_datetime):
        self._before_event()
        return self.super.time(a_datetime)


class AutoTimingTestResultDecorator(HookedTestResultDecorator):
    """Decorate a TestResult to add time events to a test run.

    By default this will cause a time event before every test event,
    but if explicit time data is being provided by the test run, then
    this decorator will turn itself off to prevent causing confusion.
    """

    def __init__(self, decorated):
        self._time = None
        super(AutoTimingTestResultDecorator, self).__init__(decorated)

    def _before_event(self):
        time = self._time
        if time is not None:
            return
        time = datetime.datetime.utcnow().replace(tzinfo=iso8601.Utc())
        self.decorated.time(time)

    def progress(self, offset, whence):
        return self.decorated.progress(offset, whence)

    @property
    def shouldStop(self):
        return self.decorated.shouldStop

    def time(self, a_datetime):
        """Provide a timestamp for the current test activity.

        :param a_datetime: If None, automatically add timestamps before every
            event (this is the default behaviour if time() is not called at
            all).  If not None, pass the provided time onto the decorated
            result object and disable automatic timestamps.
        """
        self._time = a_datetime
        return self.decorated.time(a_datetime)


class TestResultFilter(TestResultDecorator):
    """A pyunit TestResult interface implementation which filters tests.

    Tests that pass the filter are handed on to another TestResult instance
    for further processing/reporting. To obtain the filtered results,
    the other instance must be interrogated.

    :ivar result: The result that tests are passed to after filtering.
    :ivar filter_predicate: The callback run to decide whether to pass
        a result.
    """

    def __init__(self, result, filter_error=False, filter_failure=False,
        filter_success=True, filter_skip=False,
        filter_predicate=None):
        """Create a FilterResult object filtering to result.

        :param filter_error: Filter out errors.
        :param filter_failure: Filter out failures.
        :param filter_success: Filter out successful tests.
        :param filter_skip: Filter out skipped tests.
        :param filter_predicate: A callable taking (test, outcome, err,
            details) and returning True if the result should be passed
            through.  err and details may be none if no error or extra
            metadata is available. outcome is the name of the outcome such
            as 'success' or 'failure'.
        """
        TestResultDecorator.__init__(self, result)
        self._filter_error = filter_error
        self._filter_failure = filter_failure
        self._filter_success = filter_success
        self._filter_skip = filter_skip
        if filter_predicate is None:
            filter_predicate = lambda test, outcome, err, details: True
        self.filter_predicate = filter_predicate
        # The current test (for filtering tags)
        self._current_test = None
        # Has the current test been filtered (for outputting test tags)
        self._current_test_filtered = None
        # The (new, gone) tags for the current test.
        self._current_test_tags = None

    def addError(self, test, err=None, details=None):
        if (not self._filter_error and
            self.filter_predicate(test, 'error', err, details)):
            self.decorated.startTest(test)
            self.decorated.addError(test, err, details=details)
        else:
            self._filtered()

    def addFailure(self, test, err=None, details=None):
        if (not self._filter_failure and
            self.filter_predicate(test, 'failure', err, details)):
            self.decorated.startTest(test)
            self.decorated.addFailure(test, err, details=details)
        else:
            self._filtered()

    def addSkip(self, test, reason=None, details=None):
        if (not self._filter_skip and
            self.filter_predicate(test, 'skip', reason, details)):
            self.decorated.startTest(test)
            self.decorated.addSkip(test, reason, details=details)
        else:
            self._filtered()

    def addSuccess(self, test, details=None):
        if (not self._filter_success and
            self.filter_predicate(test, 'success', None, details)):
            self.decorated.startTest(test)
            self.decorated.addSuccess(test, details=details)
        else:
            self._filtered()

    def addExpectedFailure(self, test, err=None, details=None):
        if self.filter_predicate(test, 'expectedfailure', err, details):
            self.decorated.startTest(test)
            return self.decorated.addExpectedFailure(test, err,
                details=details)
        else:
            self._filtered()

    def addUnexpectedSuccess(self, test, details=None):
        self.decorated.startTest(test)
        return self.decorated.addUnexpectedSuccess(test, details=details)

    def _filtered(self):
        self._current_test_filtered = True

    def startTest(self, test):
        """Start a test.

        Not directly passed to the client, but used for handling of tags
        correctly.
        """
        self._current_test = test
        self._current_test_filtered = False
        self._current_test_tags = set(), set()

    def stopTest(self, test):
        """Stop a test.

        Not directly passed to the client, but used for handling of tags
        correctly.
        """
        if not self._current_test_filtered:
            # Tags to output for this test.
            if self._current_test_tags[0] or self._current_test_tags[1]:
                self.decorated.tags(*self._current_test_tags)
            self.decorated.stopTest(test)
        self._current_test = None
        self._current_test_filtered = None
        self._current_test_tags = None

    def tags(self, new_tags, gone_tags):
        """Handle tag instructions.

        Adds and removes tags as appropriate. If a test is currently running,
        tags are not affected for subsequent tests.

        :param new_tags: Tags to add,
        :param gone_tags: Tags to remove.
        """
        if self._current_test is not None:
            # gather the tags until the test stops.
            self._current_test_tags[0].update(new_tags)
            self._current_test_tags[0].difference_update(gone_tags)
            self._current_test_tags[1].update(gone_tags)
            self._current_test_tags[1].difference_update(new_tags)
        return self.decorated.tags(new_tags, gone_tags)

    def id_to_orig_id(self, id):
        if id.startswith("subunit.RemotedTestCase."):
            return id[len("subunit.RemotedTestCase."):]
        return id


class TestIdPrintingResult(testtools.TestResult):

    def __init__(self, stream, show_times=False):
        """Create a FilterResult object outputting to stream."""
        testtools.TestResult.__init__(self)
        self._stream = stream
        self.failed_tests = 0
        self.__time = 0
        self.show_times = show_times
        self._test = None
        self._test_duration = 0

    def addError(self, test, err):
        self.failed_tests += 1
        self._test = test

    def addFailure(self, test, err):
        self.failed_tests += 1
        self._test = test

    def addSuccess(self, test):
        self._test = test

    def reportTest(self, test, duration):
        if self.show_times:
            seconds = duration.seconds
            seconds += duration.days * 3600 * 24
            seconds += duration.microseconds / 1000000.0
            self._stream.write(test.id() + ' %0.3f\n' % seconds)
        else:
            self._stream.write(test.id() + '\n')

    def startTest(self, test):
        self._start_time = self._time()

    def stopTest(self, test):
        test_duration = self._time() - self._start_time
        self.reportTest(self._test, test_duration)

    def time(self, time):
        self.__time = time

    def _time(self):
        return self.__time

    def wasSuccessful(self):
        "Tells whether or not this result was a success"
        return self.failed_tests == 0
