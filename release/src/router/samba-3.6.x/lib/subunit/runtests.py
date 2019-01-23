#!/usr/bin/env python
# -*- Mode: python -*-
#
# Copyright (C) 2004 Canonical.com
#       Author:      Robert Collins <robert.collins@canonical.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import unittest
from subunit.tests.TestUtil import TestVisitor, TestSuite
import subunit
import sys
import os
import shutil
import logging

class ParameterisableTextTestRunner(unittest.TextTestRunner):
    """I am a TextTestRunner whose result class is
    parameterisable without further subclassing"""
    def __init__(self, **args):
        unittest.TextTestRunner.__init__(self, **args)
        self._resultFactory=None
    def resultFactory(self, *args):
        """set or retrieve the result factory"""
        if args:
            self._resultFactory=args[0]
            return self
        if self._resultFactory is None:
            self._resultFactory=unittest._TextTestResult
        return self._resultFactory

    def _makeResult(self):
        return self.resultFactory()(self.stream, self.descriptions, self.verbosity)


class EarlyStoppingTextTestResult(unittest._TextTestResult):
    """I am a TextTestResult that can optionally stop at the first failure
    or error"""

    def addError(self, test, err):
        unittest._TextTestResult.addError(self, test, err)
        if self.stopOnError():
            self.stop()

    def addFailure(self, test, err):
        unittest._TextTestResult.addError(self, test, err)
        if self.stopOnFailure():
            self.stop()

    def stopOnError(self, *args):
        """should this result indicate an abort when an error occurs?
        TODO parameterise this"""
        return True

    def stopOnFailure(self, *args):
        """should this result indicate an abort when a failure error occurs?
        TODO parameterise this"""
        return True


def earlyStopFactory(*args, **kwargs):
    """return a an early stopping text test result"""
    result=EarlyStoppingTextTestResult(*args, **kwargs)
    return result


class ShellTests(subunit.ExecTestCase):

    def test_sourcing(self):
        """./shell/tests/test_source_library.sh"""

    def test_functions(self):
        """./shell/tests/test_function_output.sh"""


def test_suite():
    result = TestSuite()
    result.addTest(subunit.test_suite())
    result.addTest(ShellTests('test_sourcing'))
    result.addTest(ShellTests('test_functions'))
    return result


class filteringVisitor(TestVisitor):
    """I accrue all the testCases I visit that pass a regexp filter on id
    into my suite
    """

    def __init__(self, filter):
        import re
        TestVisitor.__init__(self)
        self._suite=None
        self.filter=re.compile(filter)

    def suite(self):
        """answer the suite we are building"""
        if self._suite is None:
            self._suite=TestSuite()
        return self._suite

    def visitCase(self, aCase):
        if self.filter.match(aCase.id()):
            self.suite().addTest(aCase)


def main(argv):
    """To parameterise what tests are run, run this script like so:
    python test_all.py REGEX
    i.e.
    python test_all.py .*Protocol.*
    to run all tests with Protocol in their id."""
    if len(argv) > 1:
        pattern = argv[1]
    else:
        pattern = ".*"
    visitor = filteringVisitor(pattern)
    test_suite().visit(visitor)
    runner = ParameterisableTextTestRunner(verbosity=2)
    runner.resultFactory(unittest._TextTestResult)
    if not runner.run(visitor.suite()).wasSuccessful():
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
