# Copyright (c) 2008 Jonathan M. Lange. See LICENSE for details.

"""Helpers for tests."""

import sys

__metaclass__ = type
__all__ = [
    'LoggingResult',
    ]

from testtools import TestResult


# GZ 2010-08-12: Don't do this, pointlessly creates an exc_info cycle
try:
    raise Exception
except Exception:
    an_exc_info = sys.exc_info()

# Deprecated: This classes attributes are somewhat non deterministic which
# leads to hard to predict tests (because Python upstream are changing things.
class LoggingResult(TestResult):
    """TestResult that logs its event to a list."""

    def __init__(self, log):
        self._events = log
        super(LoggingResult, self).__init__()

    def startTest(self, test):
        self._events.append(('startTest', test))
        super(LoggingResult, self).startTest(test)

    def stopTest(self, test):
        self._events.append(('stopTest', test))
        super(LoggingResult, self).stopTest(test)

    def addFailure(self, test, error):
        self._events.append(('addFailure', test, error))
        super(LoggingResult, self).addFailure(test, error)

    def addError(self, test, error):
        self._events.append(('addError', test, error))
        super(LoggingResult, self).addError(test, error)

    def addSkip(self, test, reason):
        self._events.append(('addSkip', test, reason))
        super(LoggingResult, self).addSkip(test, reason)

    def addSuccess(self, test):
        self._events.append(('addSuccess', test))
        super(LoggingResult, self).addSuccess(test)

    def startTestRun(self):
        self._events.append('startTestRun')
        super(LoggingResult, self).startTestRun()

    def stopTestRun(self):
        self._events.append('stopTestRun')
        super(LoggingResult, self).stopTestRun()

    def done(self):
        self._events.append('done')
        super(LoggingResult, self).done()

    def time(self, a_datetime):
        self._events.append(('time', a_datetime))
        super(LoggingResult, self).time(a_datetime)

# Note, the following three classes are different to LoggingResult by
# being fully defined exact matches rather than supersets.
from testtools.testresult.doubles import *
