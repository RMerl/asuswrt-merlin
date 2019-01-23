# Python module for parsing and generating the Subunit protocol
# (Samba-specific)
# Copyright (C) 2008-2009 Jelmer Vernooij <jelmer@samba.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

__all__ = ['parse_results']

import re
import sys
import subunit
import subunit.iso8601
import testtools

VALID_RESULTS = ['success', 'successful', 'failure', 'fail', 'skip', 'knownfail', 'error', 'xfail', 'skip-testsuite', 'testsuite-failure', 'testsuite-xfail', 'testsuite-success', 'testsuite-error']

class TestsuiteEnabledTestResult(testtools.testresult.TestResult):

    def start_testsuite(self, name):
        raise NotImplementedError(self.start_testsuite)


def parse_results(msg_ops, statistics, fh):
    exitcode = 0
    expected_fail = 0
    open_tests = {}

    while fh:
        l = fh.readline()
        if l == "":
            break
        parts = l.split(None, 1)
        if not len(parts) == 2 or not l.startswith(parts[0]):
            msg_ops.output_msg(l)
            continue
        command = parts[0].rstrip(":")
        arg = parts[1]
        if command in ("test", "testing"):
            msg_ops.control_msg(l)
            name = arg.rstrip()
            test = subunit.RemotedTestCase(name)
            if name in open_tests:
                msg_ops.addError(open_tests.pop(name), subunit.RemoteError(u"Test already running"))
            msg_ops.startTest(test)
            open_tests[name] = test
        elif command == "time":
            msg_ops.control_msg(l)
            try:
                dt = subunit.iso8601.parse_date(arg.rstrip("\n"))
            except TypeError, e:
                print "Unable to parse time line: %s" % arg.rstrip("\n")
            else:
                msg_ops.time(dt)
        elif command in VALID_RESULTS:
            msg_ops.control_msg(l)
            result = command
            grp = re.match("(.*?)( \[)?([ \t]*)( multipart)?\n", arg)
            (testname, hasreason) = (grp.group(1), grp.group(2))
            if hasreason:
                reason = ""
                # reason may be specified in next lines
                terminated = False
                while fh:
                    l = fh.readline()
                    if l == "":
                        break
                    msg_ops.control_msg(l)
                    if l == "]\n":
                        terminated = True
                        break
                    else:
                        reason += l

                remote_error = subunit.RemoteError(reason.decode("utf-8"))

                if not terminated:
                    statistics['TESTS_ERROR']+=1
                    msg_ops.addError(subunit.RemotedTestCase(testname), subunit.RemoteError(u"reason (%s) interrupted" % result))
                    return 1
            else:
                reason = None
                remote_error = subunit.RemoteError(u"No reason specified")
            if result in ("success", "successful"):
                try:
                    test = open_tests.pop(testname)
                except KeyError:
                    statistics['TESTS_ERROR']+=1
                    exitcode = 1
                    msg_ops.addError(subunit.RemotedTestCase(testname), subunit.RemoteError(u"Test was never started"))
                else:
                    statistics['TESTS_EXPECTED_OK']+=1
                    msg_ops.addSuccess(test)
            elif result in ("xfail", "knownfail"):
                try:
                    test = open_tests.pop(testname)
                except KeyError:
                    statistics['TESTS_ERROR']+=1
                    exitcode = 1
                    msg_ops.addError(subunit.RemotedTestCase(testname), subunit.RemoteError(u"Test was never started"))
                else:
                    statistics['TESTS_EXPECTED_FAIL']+=1
                    msg_ops.addExpectedFailure(test, remote_error)
                    expected_fail+=1
            elif result in ("failure", "fail"):
                try:
                    test = open_tests.pop(testname)
                except KeyError:
                    statistics['TESTS_ERROR']+=1
                    exitcode = 1
                    msg_ops.addError(subunit.RemotedTestCase(testname), subunit.RemoteError(u"Test was never started"))
                else:
                    statistics['TESTS_UNEXPECTED_FAIL']+=1
                    exitcode = 1
                    msg_ops.addFailure(test, remote_error)
            elif result == "skip":
                statistics['TESTS_SKIP']+=1
                # Allow tests to be skipped without prior announcement of test
                try:
                    test = open_tests.pop(testname)
                except KeyError:
                    test = subunit.RemotedTestCase(testname)
                msg_ops.addSkip(test, reason)
            elif result == "error":
                statistics['TESTS_ERROR']+=1
                exitcode = 1
                try:
                    test = open_tests.pop(testname)
                except KeyError:
                    test = subunit.RemotedTestCase(testname)
                msg_ops.addError(test, remote_error)
            elif result == "skip-testsuite":
                msg_ops.skip_testsuite(testname)
            elif result == "testsuite-success":
                msg_ops.end_testsuite(testname, "success", reason)
            elif result == "testsuite-failure":
                msg_ops.end_testsuite(testname, "failure", reason)
                exitcode = 1
            elif result == "testsuite-xfail":
                msg_ops.end_testsuite(testname, "xfail", reason)
            elif result == "testsuite-error":
                msg_ops.end_testsuite(testname, "error", reason)
                exitcode = 1
            else:
                raise AssertionError("Recognized but unhandled result %r" %
                    result)
        elif command == "testsuite":
            msg_ops.start_testsuite(arg.strip())
        elif command == "progress":
            arg = arg.strip()
            if arg == "pop":
                msg_ops.progress(None, subunit.PROGRESS_POP)
            elif arg == "push":
                msg_ops.progress(None, subunit.PROGRESS_PUSH)
            elif arg[0] in '+-':
                msg_ops.progress(int(arg), subunit.PROGRESS_CUR)
            else:
                msg_ops.progress(int(arg), subunit.PROGRESS_SET)
        else:
            msg_ops.output_msg(l)

    while open_tests:
        test = subunit.RemotedTestCase(open_tests.popitem()[1])
        msg_ops.addError(test, subunit.RemoteError(u"was started but never finished!"))
        statistics['TESTS_ERROR']+=1
        exitcode = 1

    return exitcode


class SubunitOps(subunit.TestProtocolClient,TestsuiteEnabledTestResult):

    # The following are Samba extensions:
    def start_testsuite(self, name):
        self._stream.write("testsuite: %s\n" % name)

    def skip_testsuite(self, name, reason=None):
        if reason:
            self._stream.write("skip-testsuite: %s [\n%s\n]\n" % (name, reason))
        else:
            self._stream.write("skip-testsuite: %s\n" % name)

    def end_testsuite(self, name, result, reason=None):
        if reason:
            self._stream.write("testsuite-%s: %s [\n%s\n]\n" % (result, name, reason))
        else:
            self._stream.write("testsuite-%s: %s\n" % (result, name))

    def output_msg(self, msg):
        self._stream.write(msg)


def read_test_regexes(name):
    ret = {}
    f = open(name, 'r')
    try:
        for l in f:
            l = l.strip()
            if l == "" or l[0] == "#":
                continue
            if "#" in l:
                (regex, reason) = l.split("#", 1)
                ret[regex.strip()] = reason.strip()
            else:
                ret[l] = None
    finally:
        f.close()
    return ret


def find_in_list(regexes, fullname):
    for regex, reason in regexes.iteritems():
        if re.match(regex, fullname):
            if reason is None:
                return ""
            return reason
    return None


class ImmediateFail(Exception):
    """Raised to abort immediately."""

    def __init__(self):
        super(ImmediateFail, self).__init__("test failed and fail_immediately set")


class FilterOps(testtools.testresult.TestResult):

    def control_msg(self, msg):
        pass # We regenerate control messages, so ignore this

    def time(self, time):
        self._ops.time(time)

    def progress(self, delta, whence):
        self._ops.progress(delta, whence)

    def output_msg(self, msg):
        if self.output is None:
            sys.stdout.write(msg)
        else:
            self.output+=msg

    def startTest(self, test):
        self.seen_output = True
        test = self._add_prefix(test)
        if self.strip_ok_output:
           self.output = ""

        self._ops.startTest(test)

    def _add_prefix(self, test):
        if self.prefix is not None:
            return subunit.RemotedTestCase(self.prefix + test.id())
        else:
            return test

    def addError(self, test, details=None):
        test = self._add_prefix(test)
        self.error_added+=1
        self.total_error+=1
        self._ops.addError(test, details)
        self.output = None
        if self.fail_immediately:
            raise ImmediateFail()

    def addSkip(self, test, details=None):
        self.seen_output = True
        test = self._add_prefix(test)
        self._ops.addSkip(test, details)
        self.output = None

    def addExpectedFailure(self, test, details=None):
        test = self._add_prefix(test)
        self._ops.addExpectedFailure(test, details)
        self.output = None

    def addFailure(self, test, details=None):
        test = self._add_prefix(test)
        xfail_reason = find_in_list(self.expected_failures, test.id())
        if xfail_reason is not None:
            self.xfail_added+=1
            self.total_xfail+=1
            if details is not None:
                details = subunit.RemoteError(unicode(details[1]) + xfail_reason.decode("utf-8"))
            else:
                details = subunit.RemoteError(xfail_reason.decode("utf-8"))
            self._ops.addExpectedFailure(test, details)
        else:
            self.fail_added+=1
            self.total_fail+=1
            self._ops.addFailure(test, details)
            if self.output:
                self._ops.output_msg(self.output)
            if self.fail_immediately:
                raise ImmediateFail()
        self.output = None

    def addSuccess(self, test, details=None):
        test = self._add_prefix(test)
        self._ops.addSuccess(test, details)
        self.output = None

    def skip_testsuite(self, name, reason=None):
        self._ops.skip_testsuite(name, reason)

    def start_testsuite(self, name):
        self._ops.start_testsuite(name)
        self.error_added = 0
        self.fail_added = 0
        self.xfail_added = 0

    def end_testsuite(self, name, result, reason=None):
        xfail = False

        if self.xfail_added > 0:
            xfail = True
        if self.fail_added > 0 or self.error_added > 0:
            xfail = False

        if xfail and result in ("fail", "failure"):
            result = "xfail"

        if self.fail_added > 0 and result != "failure":
            result = "failure"
            if reason is None:
                reason = "Subunit/Filter Reason"
            reason += "\n failures[%d]" % self.fail_added

        if self.error_added > 0 and result != "error":
            result = "error"
            if reason is None:
                reason = "Subunit/Filter Reason"
            reason += "\n errors[%d]" % self.error_added

        self._ops.end_testsuite(name, result, reason)

    def __init__(self, out, prefix=None, expected_failures=None,
                 strip_ok_output=False, fail_immediately=False):
        self._ops = out
        self.seen_output = False
        self.output = None
        self.prefix = prefix
        if expected_failures is not None:
            self.expected_failures = expected_failures
        else:
            self.expected_failures = {}
        self.strip_ok_output = strip_ok_output
        self.xfail_added = 0
        self.fail_added = 0
        self.total_xfail = 0
        self.total_error = 0
        self.total_fail = 0
        self.error_added = 0
        self.fail_immediately = fail_immediately


class PlainFormatter(TestsuiteEnabledTestResult):

    def __init__(self, verbose, immediate, statistics,
            totaltests=None):
        super(PlainFormatter, self).__init__()
        self.verbose = verbose
        self.immediate = immediate
        self.statistics = statistics
        self.start_time = None
        self.test_output = {}
        self.suitesfailed = []
        self.suites_ok = 0
        self.skips = {}
        self.index = 0
        self.name = None
        self._progress_level = 0
        self.totalsuites = totaltests
        self.last_time = None

    @staticmethod
    def _format_time(delta):
        minutes, seconds = divmod(delta.seconds, 60)
        hours, minutes = divmod(minutes, 60)
        ret = ""
        if hours:
            ret += "%dh" % hours
        if minutes:
            ret += "%dm" % minutes
        ret += "%ds" % seconds
        return ret

    def progress(self, offset, whence):
        if whence == subunit.PROGRESS_POP:
            self._progress_level -= 1
        elif whence == subunit.PROGRESS_PUSH:
            self._progress_level += 1
        elif whence == subunit.PROGRESS_SET:
            if self._progress_level == 0:
                self.totalsuites = offset
        elif whence == subunit.PROGRESS_CUR:
            raise NotImplementedError

    def time(self, dt):
        if self.start_time is None:
            self.start_time = dt
        self.last_time = dt

    def start_testsuite(self, name):
        self.index += 1
        self.name = name

        if not self.verbose:
            self.test_output[name] = ""

        out = "[%d" % self.index
        if self.totalsuites is not None:
            out += "/%d" % self.totalsuites
        if self.start_time is not None:
            out += " in " + self._format_time(self.last_time - self.start_time)
        if self.suitesfailed:
            out += ", %d errors" % (len(self.suitesfailed),)
        out += "] %s" % name
        if self.immediate:
            sys.stdout.write(out + "\n")
        else:
            sys.stdout.write(out + ": ")

    def output_msg(self, output):
        if self.verbose:
            sys.stdout.write(output)
        elif self.name is not None:
            self.test_output[self.name] += output
        else:
            sys.stdout.write(output)

    def control_msg(self, output):
        pass

    def end_testsuite(self, name, result, reason):
        out = ""
        unexpected = False

        if not name in self.test_output:
            print "no output for name[%s]" % name

        if result in ("success", "xfail"):
            self.suites_ok+=1
        else:
            self.output_msg("ERROR: Testsuite[%s]\n" % name)
            if reason is not None:
                self.output_msg("REASON: %s\n" % (reason,))
            self.suitesfailed.append(name)
            if self.immediate and not self.verbose and name in self.test_output:
                out += self.test_output[name]
            unexpected = True

        if not self.immediate:
            if not unexpected:
                out += " ok\n"
            else:
                out += " " + result.upper() + "\n"

        sys.stdout.write(out)

    def startTest(self, test):
        pass

    def addSuccess(self, test):
        self.end_test(test.id(), "success", False)

    def addError(self, test, details=None):
        self.end_test(test.id(), "error", True, details)

    def addFailure(self, test, details=None):
        self.end_test(test.id(), "failure", True, details)

    def addSkip(self, test, details=None):
        self.end_test(test.id(), "skip", False, details)

    def addExpectedFail(self, test, details=None):
        self.end_test(test.id(), "xfail", False, details)

    def end_test(self, testname, result, unexpected, reason=None):
        if not unexpected:
            self.test_output[self.name] = ""
            if not self.immediate:
                sys.stdout.write({
                    'failure': 'f',
                    'xfail': 'X',
                    'skip': 's',
                    'success': '.'}.get(result, "?(%s)" % result))
            return

        if not self.name in self.test_output:
            self.test_output[self.name] = ""

        self.test_output[self.name] += "UNEXPECTED(%s): %s\n" % (result, testname)
        if reason is not None:
            self.test_output[self.name] += "REASON: %s\n" % (unicode(reason[1]).encode("utf-8").strip(),)

        if self.immediate and not self.verbose:
            print self.test_output[self.name]
            self.test_output[self.name] = ""

        if not self.immediate:
            sys.stdout.write({
               'error': 'E',
               'failure': 'F',
               'success': 'S'}.get(result, "?"))

    def write_summary(self, path):
        f = open(path, 'w+')

        if self.suitesfailed:
            f.write("= Failed tests =\n")

            for suite in self.suitesfailed:
                f.write("== %s ==\n" % suite)
                if suite in self.test_output:
                    f.write(self.test_output[suite]+"\n\n")

            f.write("\n")

        if not self.immediate and not self.verbose:
            for suite in self.suitesfailed:
                print "=" * 78
                print "FAIL: %s" % suite
                if suite in self.test_output:
                    print self.test_output[suite]
                print ""

        f.write("= Skipped tests =\n")
        for reason in self.skips.keys():
            f.write(reason + "\n")
            for name in self.skips[reason]:
                f.write("\t%s\n" % name)
            f.write("\n")
        f.close()

        if (not self.suitesfailed and
            not self.statistics['TESTS_UNEXPECTED_FAIL'] and
            not self.statistics['TESTS_ERROR']):
            ok = (self.statistics['TESTS_EXPECTED_OK'] +
                  self.statistics['TESTS_EXPECTED_FAIL'])
            print "\nALL OK (%d tests in %d testsuites)" % (ok, self.suites_ok)
        else:
            print "\nFAILED (%d failures and %d errors in %d testsuites)" % (
                self.statistics['TESTS_UNEXPECTED_FAIL'],
                self.statistics['TESTS_ERROR'],
                len(self.suitesfailed))

    def skip_testsuite(self, name, reason="UNKNOWN"):
        self.skips.setdefault(reason, []).append(name)
        if self.totalsuites:
            self.totalsuites-=1
