#
#  subunit: extensions to Python unittest to get test results from subprocesses.
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

import datetime
import unittest
from StringIO import StringIO
import os
import sys

from testtools.content import Content, TracebackContent
from testtools.content_type import ContentType
from testtools.tests.helpers import (
    Python26TestResult,
    Python27TestResult,
    ExtendedTestResult,
    )

import subunit
from subunit import _remote_exception_str
import subunit.iso8601 as iso8601


class TestTestImports(unittest.TestCase):

    def test_imports(self):
        from subunit import DiscardStream
        from subunit import TestProtocolServer
        from subunit import RemotedTestCase
        from subunit import RemoteError
        from subunit import ExecTestCase
        from subunit import IsolatedTestCase
        from subunit import TestProtocolClient
        from subunit import ProtocolTestCase


class TestDiscardStream(unittest.TestCase):

    def test_write(self):
        subunit.DiscardStream().write("content")


class TestProtocolServerForward(unittest.TestCase):

    def test_story(self):
        client = unittest.TestResult()
        out = StringIO()
        protocol = subunit.TestProtocolServer(client, forward_stream=out)
        pipe = StringIO("test old mcdonald\n"
                        "success old mcdonald\n")
        protocol.readFrom(pipe)
        mcdonald = subunit.RemotedTestCase("old mcdonald")
        self.assertEqual(client.testsRun, 1)
        self.assertEqual(pipe.getvalue(), out.getvalue())

    def test_not_command(self):
        client = unittest.TestResult()
        out = StringIO()
        protocol = subunit.TestProtocolServer(client,
            stream=subunit.DiscardStream(), forward_stream=out)
        pipe = StringIO("success old mcdonald\n")
        protocol.readFrom(pipe)
        self.assertEqual(client.testsRun, 0)
        self.assertEqual("", out.getvalue())
        

class TestTestProtocolServerPipe(unittest.TestCase):

    def test_story(self):
        client = unittest.TestResult()
        protocol = subunit.TestProtocolServer(client)
        pipe = StringIO("test old mcdonald\n"
                        "success old mcdonald\n"
                        "test bing crosby\n"
                        "failure bing crosby [\n"
                        "foo.c:53:ERROR invalid state\n"
                        "]\n"
                        "test an error\n"
                        "error an error\n")
        protocol.readFrom(pipe)
        mcdonald = subunit.RemotedTestCase("old mcdonald")
        bing = subunit.RemotedTestCase("bing crosby")
        an_error = subunit.RemotedTestCase("an error")
        self.assertEqual(client.errors,
                         [(an_error, _remote_exception_str + '\n')])
        self.assertEqual(
            client.failures,
            [(bing, _remote_exception_str + ": Text attachment: traceback\n"
                "------------\nfoo.c:53:ERROR invalid state\n"
                "------------\n\n")])
        self.assertEqual(client.testsRun, 3)

    def test_non_test_characters_forwarded_immediately(self):
        pass


class TestTestProtocolServerStartTest(unittest.TestCase):

    def setUp(self):
        self.client = Python26TestResult()
        self.protocol = subunit.TestProtocolServer(self.client)

    def test_start_test(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.assertEqual(self.client._events,
            [('startTest', subunit.RemotedTestCase("old mcdonald"))])

    def test_start_testing(self):
        self.protocol.lineReceived("testing old mcdonald\n")
        self.assertEqual(self.client._events,
            [('startTest', subunit.RemotedTestCase("old mcdonald"))])

    def test_start_test_colon(self):
        self.protocol.lineReceived("test: old mcdonald\n")
        self.assertEqual(self.client._events,
            [('startTest', subunit.RemotedTestCase("old mcdonald"))])

    def test_indented_test_colon_ignored(self):
        self.protocol.lineReceived(" test: old mcdonald\n")
        self.assertEqual([], self.client._events)

    def test_start_testing_colon(self):
        self.protocol.lineReceived("testing: old mcdonald\n")
        self.assertEqual(self.client._events,
            [('startTest', subunit.RemotedTestCase("old mcdonald"))])


class TestTestProtocolServerPassThrough(unittest.TestCase):

    def setUp(self):
        self.stdout = StringIO()
        self.test = subunit.RemotedTestCase("old mcdonald")
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client, self.stdout)

    def keywords_before_test(self):
        self.protocol.lineReceived("failure a\n")
        self.protocol.lineReceived("failure: a\n")
        self.protocol.lineReceived("error a\n")
        self.protocol.lineReceived("error: a\n")
        self.protocol.lineReceived("success a\n")
        self.protocol.lineReceived("success: a\n")
        self.protocol.lineReceived("successful a\n")
        self.protocol.lineReceived("successful: a\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.stdout.getvalue(), "failure a\n"
                                                 "failure: a\n"
                                                 "error a\n"
                                                 "error: a\n"
                                                 "success a\n"
                                                 "success: a\n"
                                                 "successful a\n"
                                                 "successful: a\n"
                                                 "]\n")

    def test_keywords_before_test(self):
        self.keywords_before_test()
        self.assertEqual(self.client._events, [])

    def test_keywords_after_error(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("error old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, {}),
            ('stopTest', self.test),
            ], self.client._events)

    def test_keywords_after_failure(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual(self.client._events, [
            ('startTest', self.test),
            ('addFailure', self.test, {}),
            ('stopTest', self.test),
            ])

    def test_keywords_after_success(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("success old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual([
            ('startTest', self.test),
            ('addSuccess', self.test),
            ('stopTest', self.test),
            ], self.client._events)

    def test_keywords_after_test(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure a\n")
        self.protocol.lineReceived("failure: a\n")
        self.protocol.lineReceived("error a\n")
        self.protocol.lineReceived("error: a\n")
        self.protocol.lineReceived("success a\n")
        self.protocol.lineReceived("success: a\n")
        self.protocol.lineReceived("successful a\n")
        self.protocol.lineReceived("successful: a\n")
        self.protocol.lineReceived("]\n")
        self.protocol.lineReceived("failure old mcdonald\n")
        self.assertEqual(self.stdout.getvalue(), "test old mcdonald\n"
                                                 "failure a\n"
                                                 "failure: a\n"
                                                 "error a\n"
                                                 "error: a\n"
                                                 "success a\n"
                                                 "success: a\n"
                                                 "successful a\n"
                                                 "successful: a\n"
                                                 "]\n")
        self.assertEqual(self.client._events, [
            ('startTest', self.test),
            ('addFailure', self.test, {}),
            ('stopTest', self.test),
            ])

    def test_keywords_during_failure(self):
        # A smoke test to make sure that the details parsers have control
        # appropriately.
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure: old mcdonald [\n")
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure a\n")
        self.protocol.lineReceived("failure: a\n")
        self.protocol.lineReceived("error a\n")
        self.protocol.lineReceived("error: a\n")
        self.protocol.lineReceived("success a\n")
        self.protocol.lineReceived("success: a\n")
        self.protocol.lineReceived("successful a\n")
        self.protocol.lineReceived("successful: a\n")
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.stdout.getvalue(), "")
        details = {}
        details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}),
            lambda:[
            "test old mcdonald\n"
            "failure a\n"
            "failure: a\n"
            "error a\n"
            "error: a\n"
            "success a\n"
            "success: a\n"
            "successful a\n"
            "successful: a\n"
            "]\n"])
        self.assertEqual(self.client._events, [
            ('startTest', self.test),
            ('addFailure', self.test, details),
            ('stopTest', self.test),
            ])

    def test_stdout_passthrough(self):
        """Lines received which cannot be interpreted as any protocol action
        should be passed through to sys.stdout.
        """
        bytes = "randombytes\n"
        self.protocol.lineReceived(bytes)
        self.assertEqual(self.stdout.getvalue(), bytes)


class TestTestProtocolServerLostConnection(unittest.TestCase):

    def setUp(self):
        self.client = Python26TestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.test = subunit.RemotedTestCase("old mcdonald")

    def test_lost_connection_no_input(self):
        self.protocol.lostConnection()
        self.assertEqual([], self.client._events)

    def test_lost_connection_after_start(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lostConnection()
        failure = subunit.RemoteError(
            u"lost connection during test 'old mcdonald'")
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, failure),
            ('stopTest', self.test),
            ], self.client._events)

    def test_lost_connected_after_error(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("error old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, subunit.RemoteError(u"")),
            ('stopTest', self.test),
            ], self.client._events)

    def do_connection_lost(self, outcome, opening):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("%s old mcdonald %s" % (outcome, opening))
        self.protocol.lostConnection()
        failure = subunit.RemoteError(
            u"lost connection during %s report of test 'old mcdonald'" % 
            outcome)
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, failure),
            ('stopTest', self.test),
            ], self.client._events)

    def test_lost_connection_during_error(self):
        self.do_connection_lost("error", "[\n")

    def test_lost_connection_during_error_details(self):
        self.do_connection_lost("error", "[ multipart\n")

    def test_lost_connected_after_failure(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual([
            ('startTest', self.test),
            ('addFailure', self.test, subunit.RemoteError(u"")),
            ('stopTest', self.test),
            ], self.client._events)

    def test_lost_connection_during_failure(self):
        self.do_connection_lost("failure", "[\n")

    def test_lost_connection_during_failure_details(self):
        self.do_connection_lost("failure", "[ multipart\n")

    def test_lost_connection_after_success(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("success old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual([
            ('startTest', self.test),
            ('addSuccess', self.test),
            ('stopTest', self.test),
            ], self.client._events)

    def test_lost_connection_during_success(self):
        self.do_connection_lost("success", "[\n")

    def test_lost_connection_during_success_details(self):
        self.do_connection_lost("success", "[ multipart\n")

    def test_lost_connection_during_skip(self):
        self.do_connection_lost("skip", "[\n")

    def test_lost_connection_during_skip_details(self):
        self.do_connection_lost("skip", "[ multipart\n")

    def test_lost_connection_during_xfail(self):
        self.do_connection_lost("xfail", "[\n")

    def test_lost_connection_during_xfail_details(self):
        self.do_connection_lost("xfail", "[ multipart\n")


class TestInTestMultipart(unittest.TestCase):

    def setUp(self):
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def test__outcome_sets_details_parser(self):
        self.protocol._reading_success_details.details_parser = None
        self.protocol._state._outcome(0, "mcdonalds farm [ multipart\n",
            None, self.protocol._reading_success_details)
        parser = self.protocol._reading_success_details.details_parser
        self.assertNotEqual(None, parser)
        self.assertTrue(isinstance(parser,
            subunit.details.MultipartDetailsParser))


class TestTestProtocolServerAddError(unittest.TestCase):

    def setUp(self):
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def simple_error_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        details = {}
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def test_simple_error(self):
        self.simple_error_keyword("error")

    def test_simple_error_colon(self):
        self.simple_error_keyword("error:")

    def test_error_empty_message(self):
        self.protocol.lineReceived("error mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}), lambda:[""])
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def error_quoted_bracket(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}), lambda:["]\n"])
        self.assertEqual([
            ('startTest', self.test),
            ('addError', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def test_error_quoted_bracket(self):
        self.error_quoted_bracket("error")

    def test_error_colon_quoted_bracket(self):
        self.error_quoted_bracket("error:")


class TestTestProtocolServerAddFailure(unittest.TestCase):

    def setUp(self):
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def assertFailure(self, details):
        self.assertEqual([
            ('startTest', self.test),
            ('addFailure', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def simple_failure_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        details = {}
        self.assertFailure(details)

    def test_simple_failure(self):
        self.simple_failure_keyword("failure")

    def test_simple_failure_colon(self):
        self.simple_failure_keyword("failure:")

    def test_failure_empty_message(self):
        self.protocol.lineReceived("failure mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}), lambda:[""])
        self.assertFailure(details)

    def failure_quoted_bracket(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}), lambda:["]\n"])
        self.assertFailure(details)

    def test_failure_quoted_bracket(self):
        self.failure_quoted_bracket("failure")

    def test_failure_colon_quoted_bracket(self):
        self.failure_quoted_bracket("failure:")


class TestTestProtocolServerAddxFail(unittest.TestCase):
    """Tests for the xfail keyword.

    In Python this can thunk through to Success due to stdlib limitations (see
    README).
    """

    def capture_expected_failure(self, test, err):
        self._events.append((test, err))

    def setup_python26(self):
        """Setup a test object ready to be xfailed and thunk to success."""
        self.client = Python26TestResult()
        self.setup_protocol()

    def setup_python27(self):
        """Setup a test object ready to be xfailed."""
        self.client = Python27TestResult()
        self.setup_protocol()

    def setup_python_ex(self):
        """Setup a test object ready to be xfailed with details."""
        self.client = ExtendedTestResult()
        self.setup_protocol()

    def setup_protocol(self):
        """Setup the protocol based on self.client."""
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = self.client._events[-1][-1]

    def simple_xfail_keyword(self, keyword, as_success):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.check_success_or_xfail(as_success)

    def check_success_or_xfail(self, as_success, error_message=None):
        if as_success:
            self.assertEqual([
                ('startTest', self.test),
                ('addSuccess', self.test),
                ('stopTest', self.test),
                ], self.client._events)
        else:
            details = {}
            if error_message is not None:
                details['traceback'] = Content(
                    ContentType("text", "x-traceback", {'charset': 'utf8'}),
                    lambda:[error_message])
            if isinstance(self.client, ExtendedTestResult):
                value = details
            else:
                if error_message is not None:
                    value = subunit.RemoteError(u'Text attachment: traceback\n'
                        '------------\n' + error_message + '------------\n')
                else:
                    value = subunit.RemoteError()
            self.assertEqual([
                ('startTest', self.test),
                ('addExpectedFailure', self.test, value),
                ('stopTest', self.test),
                ], self.client._events)

    def test_simple_xfail(self):
        self.setup_python26()
        self.simple_xfail_keyword("xfail", True)
        self.setup_python27()
        self.simple_xfail_keyword("xfail",  False)
        self.setup_python_ex()
        self.simple_xfail_keyword("xfail",  False)

    def test_simple_xfail_colon(self):
        self.setup_python26()
        self.simple_xfail_keyword("xfail:", True)
        self.setup_python27()
        self.simple_xfail_keyword("xfail:", False)
        self.setup_python_ex()
        self.simple_xfail_keyword("xfail:", False)

    def test_xfail_empty_message(self):
        self.setup_python26()
        self.empty_message(True)
        self.setup_python27()
        self.empty_message(False)
        self.setup_python_ex()
        self.empty_message(False, error_message="")

    def empty_message(self, as_success, error_message="\n"):
        self.protocol.lineReceived("xfail mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        self.check_success_or_xfail(as_success, error_message)

    def xfail_quoted_bracket(self, keyword, as_success):
        # This tests it is accepted, but cannot test it is used today, because
        # of not having a way to expose it in Python so far.
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        self.check_success_or_xfail(as_success, "]\n")

    def test_xfail_quoted_bracket(self):
        self.setup_python26()
        self.xfail_quoted_bracket("xfail", True)
        self.setup_python27()
        self.xfail_quoted_bracket("xfail", False)
        self.setup_python_ex()
        self.xfail_quoted_bracket("xfail", False)

    def test_xfail_colon_quoted_bracket(self):
        self.setup_python26()
        self.xfail_quoted_bracket("xfail:", True)
        self.setup_python27()
        self.xfail_quoted_bracket("xfail:", False)
        self.setup_python_ex()
        self.xfail_quoted_bracket("xfail:", False)


class TestTestProtocolServerAddSkip(unittest.TestCase):
    """Tests for the skip keyword.

    In Python this meets the testtools extended TestResult contract.
    (See https://launchpad.net/testtools).
    """

    def setUp(self):
        """Setup a test object ready to be skipped."""
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = self.client._events[-1][-1]

    def assertSkip(self, reason):
        details = {}
        if reason is not None:
            details['reason'] = Content(
                ContentType("text", "plain"), lambda:[reason])
        self.assertEqual([
            ('startTest', self.test),
            ('addSkip', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def simple_skip_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.assertSkip(None)

    def test_simple_skip(self):
        self.simple_skip_keyword("skip")

    def test_simple_skip_colon(self):
        self.simple_skip_keyword("skip:")

    def test_skip_empty_message(self):
        self.protocol.lineReceived("skip mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        self.assertSkip("")

    def skip_quoted_bracket(self, keyword):
        # This tests it is accepted, but cannot test it is used today, because
        # of not having a way to expose it in Python so far.
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        self.assertSkip("]\n")

    def test_skip_quoted_bracket(self):
        self.skip_quoted_bracket("skip")

    def test_skip_colon_quoted_bracket(self):
        self.skip_quoted_bracket("skip:")


class TestTestProtocolServerAddSuccess(unittest.TestCase):

    def setUp(self):
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def simple_success_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.assertEqual([
            ('startTest', self.test),
            ('addSuccess', self.test),
            ('stopTest', self.test),
            ], self.client._events)

    def test_simple_success(self):
        self.simple_success_keyword("failure")

    def test_simple_success_colon(self):
        self.simple_success_keyword("failure:")

    def test_simple_success(self):
        self.simple_success_keyword("successful")

    def test_simple_success_colon(self):
        self.simple_success_keyword("successful:")

    def assertSuccess(self, details):
        self.assertEqual([
            ('startTest', self.test),
            ('addSuccess', self.test, details),
            ('stopTest', self.test),
            ], self.client._events)

    def test_success_empty_message(self):
        self.protocol.lineReceived("success mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['message'] = Content(ContentType("text", "plain"),
            lambda:[""])
        self.assertSuccess(details)

    def success_quoted_bracket(self, keyword):
        # This tests it is accepted, but cannot test it is used today, because
        # of not having a way to expose it in Python so far.
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        details = {}
        details['message'] = Content(ContentType("text", "plain"),
            lambda:["]\n"])
        self.assertSuccess(details)

    def test_success_quoted_bracket(self):
        self.success_quoted_bracket("success")

    def test_success_colon_quoted_bracket(self):
        self.success_quoted_bracket("success:")


class TestTestProtocolServerProgress(unittest.TestCase):
    """Test receipt of progress: directives."""

    def test_progress_accepted_stdlib(self):
        self.result = Python26TestResult()
        self.stream = StringIO()
        self.protocol = subunit.TestProtocolServer(self.result,
            stream=self.stream)
        self.protocol.lineReceived("progress: 23")
        self.protocol.lineReceived("progress: -2")
        self.protocol.lineReceived("progress: +4")
        self.assertEqual("", self.stream.getvalue())

    def test_progress_accepted_extended(self):
        # With a progress capable TestResult, progress events are emitted.
        self.result = ExtendedTestResult()
        self.stream = StringIO()
        self.protocol = subunit.TestProtocolServer(self.result,
            stream=self.stream)
        self.protocol.lineReceived("progress: 23")
        self.protocol.lineReceived("progress: push")
        self.protocol.lineReceived("progress: -2")
        self.protocol.lineReceived("progress: pop")
        self.protocol.lineReceived("progress: +4")
        self.assertEqual("", self.stream.getvalue())
        self.assertEqual([
            ('progress', 23, subunit.PROGRESS_SET),
            ('progress', None, subunit.PROGRESS_PUSH),
            ('progress', -2, subunit.PROGRESS_CUR),
            ('progress', None, subunit.PROGRESS_POP),
            ('progress', 4, subunit.PROGRESS_CUR),
            ], self.result._events)


class TestTestProtocolServerStreamTags(unittest.TestCase):
    """Test managing tags on the protocol level."""

    def setUp(self):
        self.client = ExtendedTestResult()
        self.protocol = subunit.TestProtocolServer(self.client)

    def test_initial_tags(self):
        self.protocol.lineReceived("tags: foo bar:baz  quux\n")
        self.assertEqual([
            ('tags', set(["foo", "bar:baz", "quux"]), set()),
            ], self.client._events)

    def test_minus_removes_tags(self):
        self.protocol.lineReceived("tags: -bar quux\n")
        self.assertEqual([
            ('tags', set(["quux"]), set(["bar"])),
            ], self.client._events)

    def test_tags_do_not_get_set_on_test(self):
        self.protocol.lineReceived("test mcdonalds farm\n")
        test = self.client._events[0][-1]
        self.assertEqual(None, getattr(test, 'tags', None))

    def test_tags_do_not_get_set_on_global_tags(self):
        self.protocol.lineReceived("tags: foo bar\n")
        self.protocol.lineReceived("test mcdonalds farm\n")
        test = self.client._events[-1][-1]
        self.assertEqual(None, getattr(test, 'tags', None))

    def test_tags_get_set_on_test_tags(self):
        self.protocol.lineReceived("test mcdonalds farm\n")
        test = self.client._events[-1][-1]
        self.protocol.lineReceived("tags: foo bar\n")
        self.protocol.lineReceived("success mcdonalds farm\n")
        self.assertEqual(None, getattr(test, 'tags', None))


class TestTestProtocolServerStreamTime(unittest.TestCase):
    """Test managing time information at the protocol level."""

    def test_time_accepted_stdlib(self):
        self.result = Python26TestResult()
        self.stream = StringIO()
        self.protocol = subunit.TestProtocolServer(self.result,
            stream=self.stream)
        self.protocol.lineReceived("time: 2001-12-12 12:59:59Z\n")
        self.assertEqual("", self.stream.getvalue())

    def test_time_accepted_extended(self):
        self.result = ExtendedTestResult()
        self.stream = StringIO()
        self.protocol = subunit.TestProtocolServer(self.result,
            stream=self.stream)
        self.protocol.lineReceived("time: 2001-12-12 12:59:59Z\n")
        self.assertEqual("", self.stream.getvalue())
        self.assertEqual([
            ('time', datetime.datetime(2001, 12, 12, 12, 59, 59, 0,
            iso8601.Utc()))
            ], self.result._events)


class TestRemotedTestCase(unittest.TestCase):

    def test_simple(self):
        test = subunit.RemotedTestCase("A test description")
        self.assertRaises(NotImplementedError, test.setUp)
        self.assertRaises(NotImplementedError, test.tearDown)
        self.assertEqual("A test description",
                         test.shortDescription())
        self.assertEqual("A test description",
                         test.id())
        self.assertEqual("A test description (subunit.RemotedTestCase)", "%s" % test)
        self.assertEqual("<subunit.RemotedTestCase description="
                         "'A test description'>", "%r" % test)
        result = unittest.TestResult()
        test.run(result)
        self.assertEqual([(test, _remote_exception_str + ": "
                                 "Cannot run RemotedTestCases.\n\n")],
                         result.errors)
        self.assertEqual(1, result.testsRun)
        another_test = subunit.RemotedTestCase("A test description")
        self.assertEqual(test, another_test)
        different_test = subunit.RemotedTestCase("ofo")
        self.assertNotEqual(test, different_test)
        self.assertNotEqual(another_test, different_test)


class TestRemoteError(unittest.TestCase):

    def test_eq(self):
        error = subunit.RemoteError(u"Something went wrong")
        another_error = subunit.RemoteError(u"Something went wrong")
        different_error = subunit.RemoteError(u"boo!")
        self.assertEqual(error, another_error)
        self.assertNotEqual(error, different_error)
        self.assertNotEqual(different_error, another_error)

    def test_empty_constructor(self):
        self.assertEqual(subunit.RemoteError(), subunit.RemoteError(u""))


class TestExecTestCase(unittest.TestCase):

    class SampleExecTestCase(subunit.ExecTestCase):

        def test_sample_method(self):
            """sample-script.py"""
            # the sample script runs three tests, one each
            # that fails, errors and succeeds

        def test_sample_method_args(self):
            """sample-script.py foo"""
            # sample that will run just one test.

    def test_construct(self):
        test = self.SampleExecTestCase("test_sample_method")
        self.assertEqual(test.script,
                         subunit.join_dir(__file__, 'sample-script.py'))

    def test_args(self):
        result = unittest.TestResult()
        test = self.SampleExecTestCase("test_sample_method_args")
        test.run(result)
        self.assertEqual(1, result.testsRun)

    def test_run(self):
        result = ExtendedTestResult()
        test = self.SampleExecTestCase("test_sample_method")
        test.run(result)
        mcdonald = subunit.RemotedTestCase("old mcdonald")
        bing = subunit.RemotedTestCase("bing crosby")
        bing_details = {}
        bing_details['traceback'] = Content(ContentType("text", "x-traceback",
            {'charset': 'utf8'}), lambda:["foo.c:53:ERROR invalid state\n"])
        an_error = subunit.RemotedTestCase("an error")
        error_details = {}
        self.assertEqual([
            ('startTest', mcdonald),
            ('addSuccess', mcdonald),
            ('stopTest', mcdonald),
            ('startTest', bing),
            ('addFailure', bing, bing_details),
            ('stopTest', bing),
            ('startTest', an_error),
            ('addError', an_error, error_details),
            ('stopTest', an_error),
            ], result._events)

    def test_debug(self):
        test = self.SampleExecTestCase("test_sample_method")
        test.debug()

    def test_count_test_cases(self):
        """TODO run the child process and count responses to determine the count."""

    def test_join_dir(self):
        sibling = subunit.join_dir(__file__, 'foo')
        expected = '%s/foo' % (os.path.split(__file__)[0],)
        self.assertEqual(sibling, expected)


class DoExecTestCase(subunit.ExecTestCase):

    def test_working_script(self):
        """sample-two-script.py"""


class TestIsolatedTestCase(unittest.TestCase):

    class SampleIsolatedTestCase(subunit.IsolatedTestCase):

        SETUP = False
        TEARDOWN = False
        TEST = False

        def setUp(self):
            TestIsolatedTestCase.SampleIsolatedTestCase.SETUP = True

        def tearDown(self):
            TestIsolatedTestCase.SampleIsolatedTestCase.TEARDOWN = True

        def test_sets_global_state(self):
            TestIsolatedTestCase.SampleIsolatedTestCase.TEST = True


    def test_construct(self):
        test = self.SampleIsolatedTestCase("test_sets_global_state")

    def test_run(self):
        result = unittest.TestResult()
        test = self.SampleIsolatedTestCase("test_sets_global_state")
        test.run(result)
        self.assertEqual(result.testsRun, 1)
        self.assertEqual(self.SampleIsolatedTestCase.SETUP, False)
        self.assertEqual(self.SampleIsolatedTestCase.TEARDOWN, False)
        self.assertEqual(self.SampleIsolatedTestCase.TEST, False)

    def test_debug(self):
        pass
        #test = self.SampleExecTestCase("test_sample_method")
        #test.debug()


class TestIsolatedTestSuite(unittest.TestCase):

    class SampleTestToIsolate(unittest.TestCase):

        SETUP = False
        TEARDOWN = False
        TEST = False

        def setUp(self):
            TestIsolatedTestSuite.SampleTestToIsolate.SETUP = True

        def tearDown(self):
            TestIsolatedTestSuite.SampleTestToIsolate.TEARDOWN = True

        def test_sets_global_state(self):
            TestIsolatedTestSuite.SampleTestToIsolate.TEST = True


    def test_construct(self):
        suite = subunit.IsolatedTestSuite()

    def test_run(self):
        result = unittest.TestResult()
        suite = subunit.IsolatedTestSuite()
        sub_suite = unittest.TestSuite()
        sub_suite.addTest(self.SampleTestToIsolate("test_sets_global_state"))
        sub_suite.addTest(self.SampleTestToIsolate("test_sets_global_state"))
        suite.addTest(sub_suite)
        suite.addTest(self.SampleTestToIsolate("test_sets_global_state"))
        suite.run(result)
        self.assertEqual(result.testsRun, 3)
        self.assertEqual(self.SampleTestToIsolate.SETUP, False)
        self.assertEqual(self.SampleTestToIsolate.TEARDOWN, False)
        self.assertEqual(self.SampleTestToIsolate.TEST, False)


class TestTestProtocolClient(unittest.TestCase):

    def setUp(self):
        self.io = StringIO()
        self.protocol = subunit.TestProtocolClient(self.io)
        self.test = TestTestProtocolClient("test_start_test")
        self.sample_details = {'something':Content(
            ContentType('text', 'plain'), lambda:['serialised\nform'])}
        self.sample_tb_details = dict(self.sample_details)
        self.sample_tb_details['traceback'] = TracebackContent(
            subunit.RemoteError(u"boo qux"), self.test)

    def test_start_test(self):
        """Test startTest on a TestProtocolClient."""
        self.protocol.startTest(self.test)
        self.assertEqual(self.io.getvalue(), "test: %s\n" % self.test.id())

    def test_stop_test(self):
        # stopTest doesn't output anything.
        self.protocol.stopTest(self.test)
        self.assertEqual(self.io.getvalue(), "")

    def test_add_success(self):
        """Test addSuccess on a TestProtocolClient."""
        self.protocol.addSuccess(self.test)
        self.assertEqual(
            self.io.getvalue(), "successful: %s\n" % self.test.id())

    def test_add_success_details(self):
        """Test addSuccess on a TestProtocolClient with details."""
        self.protocol.addSuccess(self.test, details=self.sample_details)
        self.assertEqual(
            self.io.getvalue(), "successful: %s [ multipart\n"
                "Content-Type: text/plain\n"
                "something\n"
                "F\r\nserialised\nform0\r\n]\n" % self.test.id())

    def test_add_failure(self):
        """Test addFailure on a TestProtocolClient."""
        self.protocol.addFailure(
            self.test, subunit.RemoteError(u"boo qux"))
        self.assertEqual(
            self.io.getvalue(),
            ('failure: %s [\n' + _remote_exception_str + ': boo qux\n]\n')
            % self.test.id())

    def test_add_failure_details(self):
        """Test addFailure on a TestProtocolClient with details."""
        self.protocol.addFailure(
            self.test, details=self.sample_tb_details)
        self.assertEqual(
            self.io.getvalue(),
            ("failure: %s [ multipart\n"
            "Content-Type: text/plain\n"
            "something\n"
            "F\r\nserialised\nform0\r\n"
            "Content-Type: text/x-traceback;charset=utf8,language=python\n"
            "traceback\n"
            "1A\r\n" + _remote_exception_str + ": boo qux\n0\r\n"
            "]\n") % self.test.id())

    def test_add_error(self):
        """Test stopTest on a TestProtocolClient."""
        self.protocol.addError(
            self.test, subunit.RemoteError(u"phwoar crikey"))
        self.assertEqual(
            self.io.getvalue(),
            ('error: %s [\n' +
            _remote_exception_str + ": phwoar crikey\n"
            "]\n") % self.test.id())

    def test_add_error_details(self):
        """Test stopTest on a TestProtocolClient with details."""
        self.protocol.addError(
            self.test, details=self.sample_tb_details)
        self.assertEqual(
            self.io.getvalue(),
            ("error: %s [ multipart\n"
            "Content-Type: text/plain\n"
            "something\n"
            "F\r\nserialised\nform0\r\n"
            "Content-Type: text/x-traceback;charset=utf8,language=python\n"
            "traceback\n"
            "1A\r\n" + _remote_exception_str + ": boo qux\n0\r\n"
            "]\n") % self.test.id())

    def test_add_expected_failure(self):
        """Test addExpectedFailure on a TestProtocolClient."""
        self.protocol.addExpectedFailure(
            self.test, subunit.RemoteError(u"phwoar crikey"))
        self.assertEqual(
            self.io.getvalue(),
            ('xfail: %s [\n' +
            _remote_exception_str + ": phwoar crikey\n"
            "]\n") % self.test.id())

    def test_add_expected_failure_details(self):
        """Test addExpectedFailure on a TestProtocolClient with details."""
        self.protocol.addExpectedFailure(
            self.test, details=self.sample_tb_details)
        self.assertEqual(
            self.io.getvalue(),
            ("xfail: %s [ multipart\n"
            "Content-Type: text/plain\n"
            "something\n"
            "F\r\nserialised\nform0\r\n"
            "Content-Type: text/x-traceback;charset=utf8,language=python\n"
            "traceback\n"
            "1A\r\n"+ _remote_exception_str + ": boo qux\n0\r\n"
            "]\n") % self.test.id())

    def test_add_skip(self):
        """Test addSkip on a TestProtocolClient."""
        self.protocol.addSkip(
            self.test, "Has it really?")
        self.assertEqual(
            self.io.getvalue(),
            'skip: %s [\nHas it really?\n]\n' % self.test.id())
    
    def test_add_skip_details(self):
        """Test addSkip on a TestProtocolClient with details."""
        details = {'reason':Content(
            ContentType('text', 'plain'), lambda:['Has it really?'])}
        self.protocol.addSkip(
            self.test, details=details)
        self.assertEqual(
            self.io.getvalue(),
            "skip: %s [ multipart\n"
            "Content-Type: text/plain\n"
            "reason\n"
            "E\r\nHas it really?0\r\n"
            "]\n" % self.test.id())

    def test_progress_set(self):
        self.protocol.progress(23, subunit.PROGRESS_SET)
        self.assertEqual(self.io.getvalue(), 'progress: 23\n')

    def test_progress_neg_cur(self):
        self.protocol.progress(-23, subunit.PROGRESS_CUR)
        self.assertEqual(self.io.getvalue(), 'progress: -23\n')

    def test_progress_pos_cur(self):
        self.protocol.progress(23, subunit.PROGRESS_CUR)
        self.assertEqual(self.io.getvalue(), 'progress: +23\n')

    def test_progress_pop(self):
        self.protocol.progress(1234, subunit.PROGRESS_POP)
        self.assertEqual(self.io.getvalue(), 'progress: pop\n')

    def test_progress_push(self):
        self.protocol.progress(1234, subunit.PROGRESS_PUSH)
        self.assertEqual(self.io.getvalue(), 'progress: push\n')

    def test_time(self):
        # Calling time() outputs a time signal immediately.
        self.protocol.time(
            datetime.datetime(2009,10,11,12,13,14,15, iso8601.Utc()))
        self.assertEqual(
            "time: 2009-10-11 12:13:14.000015Z\n",
            self.io.getvalue())

    def test_add_unexpected_success(self):
        """Test addUnexpectedSuccess on a TestProtocolClient."""
        self.protocol.addUnexpectedSuccess(self.test)
        self.assertEqual(
            self.io.getvalue(), "successful: %s\n" % self.test.id())

    def test_add_unexpected_success_details(self):
        """Test addUnexpectedSuccess on a TestProtocolClient with details."""
        self.protocol.addUnexpectedSuccess(self.test, details=self.sample_details)
        self.assertEqual(
            self.io.getvalue(), "successful: %s [ multipart\n"
                "Content-Type: text/plain\n"
                "something\n"
                "F\r\nserialised\nform0\r\n]\n" % self.test.id())


def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result
