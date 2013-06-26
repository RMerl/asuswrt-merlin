#!/usr/bin/python
#
# Simple subunit testrunner for python
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
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

"""Run a unittest testcase reporting results as Subunit.

  $ python -m subunit.run mylib.tests.test_suite
"""

import sys

from subunit import TestProtocolClient, get_default_formatter
from testtools.run import (
    BUFFEROUTPUT,
    CATCHBREAK,
    FAILFAST,
    TestProgram,
    USAGE_AS_MAIN,
    )


class SubunitTestRunner(object):
    def __init__(self, stream=sys.stdout):
        self.stream = stream

    def run(self, test):
        "Run the given test case or test suite."
        result = TestProtocolClient(self.stream)
        test(result)
        return result


class SubunitTestProgram(TestProgram):

    USAGE = USAGE_AS_MAIN

    def usageExit(self, msg=None):
        if msg:
            print msg
        usage = {'progName': self.progName, 'catchbreak': '', 'failfast': '',
                 'buffer': ''}
        if self.failfast != False:
            usage['failfast'] = FAILFAST
        if self.catchbreak != False:
            usage['catchbreak'] = CATCHBREAK
        if self.buffer != False:
            usage['buffer'] = BUFFEROUTPUT
        usage_text = self.USAGE % usage
        usage_lines = usage_text.split('\n')
        usage_lines.insert(2, "Run a test suite with a subunit reporter.")
        usage_lines.insert(3, "")
        print('\n'.join(usage_lines))
        sys.exit(2)


if __name__ == '__main__':
    stream = get_default_formatter()
    runner = SubunitTestRunner(stream)
    SubunitTestProgram(module=None, argv=sys.argv, testRunner=runner,
        stdout=sys.stdout)
