#
#  subunit: extensions to python unittest to get test results from subprocesses.
#  Copyright (C) 2005  Robert Collins <robertc@robertcollins.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import unittest
from StringIO import StringIO
import os
import subunit
import sys

try:
    class MockTestProtocolServerClient(object):
        """A mock protocol server client to test callbacks."""

        def __init__(self):
            self.end_calls = []
            self.error_calls = []
            self.failure_calls = []
            self.start_calls = []
            self.success_calls = []
            super(MockTestProtocolServerClient, self).__init__()

        def addError(self, test, error):
            self.error_calls.append((test, error))

        def addFailure(self, test, error):
            self.failure_calls.append((test, error))

        def addSuccess(self, test):
            self.success_calls.append(test)

        def stopTest(self, test):
            self.end_calls.append(test)

        def startTest(self, test):
            self.start_calls.append(test)

except AttributeError:
    MockTestProtocolServer = None


class TestMockTestProtocolServer(unittest.TestCase):

    def test_start_test(self):
        protocol = MockTestProtocolServerClient()
        protocol.startTest(subunit.RemotedTestCase("test old mcdonald"))
        self.assertEqual(protocol.start_calls,
                         [subunit.RemotedTestCase("test old mcdonald")])
        self.assertEqual(protocol.end_calls, [])
        self.assertEqual(protocol.error_calls, [])
        self.assertEqual(protocol.failure_calls, [])
        self.assertEqual(protocol.success_calls, [])

    def test_add_error(self):
        protocol = MockTestProtocolServerClient()
        protocol.addError(subunit.RemotedTestCase("old mcdonald"),
                          subunit.RemoteError("omg it works"))
        self.assertEqual(protocol.start_calls, [])
        self.assertEqual(protocol.end_calls, [])
        self.assertEqual(protocol.error_calls, [(
                            subunit.RemotedTestCase("old mcdonald"),
                            subunit.RemoteError("omg it works"))])
        self.assertEqual(protocol.failure_calls, [])
        self.assertEqual(protocol.success_calls, [])

    def test_add_failure(self):
        protocol = MockTestProtocolServerClient()
        protocol.addFailure(subunit.RemotedTestCase("old mcdonald"),
                            subunit.RemoteError("omg it works"))
        self.assertEqual(protocol.start_calls, [])
        self.assertEqual(protocol.end_calls, [])
        self.assertEqual(protocol.error_calls, [])
        self.assertEqual(protocol.failure_calls, [
                            (subunit.RemotedTestCase("old mcdonald"),
                             subunit.RemoteError("omg it works"))])
        self.assertEqual(protocol.success_calls, [])

    def test_add_success(self):
        protocol = MockTestProtocolServerClient()
        protocol.addSuccess(subunit.RemotedTestCase("test old mcdonald"))
        self.assertEqual(protocol.start_calls, [])
        self.assertEqual(protocol.end_calls, [])
        self.assertEqual(protocol.error_calls, [])
        self.assertEqual(protocol.failure_calls, [])
        self.assertEqual(protocol.success_calls,
                         [subunit.RemotedTestCase("test old mcdonald")])

    def test_end_test(self):
        protocol = MockTestProtocolServerClient()
        protocol.stopTest(subunit.RemotedTestCase("test old mcdonald"))
        self.assertEqual(protocol.end_calls,
                         [subunit.RemotedTestCase("test old mcdonald")])
        self.assertEqual(protocol.error_calls, [])
        self.assertEqual(protocol.failure_calls, [])
        self.assertEqual(protocol.success_calls, [])
        self.assertEqual(protocol.start_calls, [])


class TestTestImports(unittest.TestCase):

    def test_imports(self):
        from subunit import TestProtocolServer
        from subunit import RemotedTestCase
        from subunit import RemoteError
        from subunit import ExecTestCase
        from subunit import IsolatedTestCase
        from subunit import TestProtocolClient


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
                         [(an_error, 'RemoteException: \n\n')])
        self.assertEqual(
            client.failures,
            [(bing, "RemoteException: foo.c:53:ERROR invalid state\n\n")])
        self.assertEqual(client.testsRun, 3)


class TestTestProtocolServerStartTest(unittest.TestCase):

    def setUp(self):
        self.client = MockTestProtocolServerClient()
        self.protocol = subunit.TestProtocolServer(self.client)

    def test_start_test(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.assertEqual(self.client.start_calls,
                         [subunit.RemotedTestCase("old mcdonald")])

    def test_start_testing(self):
        self.protocol.lineReceived("testing old mcdonald\n")
        self.assertEqual(self.client.start_calls,
                         [subunit.RemotedTestCase("old mcdonald")])

    def test_start_test_colon(self):
        self.protocol.lineReceived("test: old mcdonald\n")
        self.assertEqual(self.client.start_calls,
                         [subunit.RemotedTestCase("old mcdonald")])

    def test_start_testing_colon(self):
        self.protocol.lineReceived("testing: old mcdonald\n")
        self.assertEqual(self.client.start_calls,
                         [subunit.RemotedTestCase("old mcdonald")])


class TestTestProtocolServerPassThrough(unittest.TestCase):

    def setUp(self):
        from StringIO import StringIO
        self.stdout = StringIO()
        self.test = subunit.RemotedTestCase("old mcdonald")
        self.client = MockTestProtocolServerClient()
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
        self.assertEqual(self.client.start_calls, [])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_keywords_after_error(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("error old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls,
                         [(self.test, subunit.RemoteError(""))])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_keywords_after_failure(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError())])
        self.assertEqual(self.client.success_calls, [])

    def test_keywords_after_success(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("success old mcdonald\n")
        self.keywords_before_test()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [self.test])

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
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError())])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_keywords_during_failure(self):
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
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError("test old mcdonald\n"
                                                  "failure a\n"
                                                  "failure: a\n"
                                                  "error a\n"
                                                  "error: a\n"
                                                  "success a\n"
                                                  "success: a\n"
                                                  "successful a\n"
                                                  "successful: a\n"
                                                  "]\n"))])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_stdout_passthrough(self):
        """Lines received which cannot be interpreted as any protocol action
        should be passed through to sys.stdout.
        """
        bytes = "randombytes\n"
        self.protocol.lineReceived(bytes)
        self.assertEqual(self.stdout.getvalue(), bytes)


class TestTestProtocolServerLostConnection(unittest.TestCase):

    def setUp(self):
        self.client = MockTestProtocolServerClient()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.test = subunit.RemotedTestCase("old mcdonald")

    def test_lost_connection_no_input(self):
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connection_after_start(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError("lost connection during "
                                            "test 'old mcdonald'"))])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connected_after_error(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("error old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError(""))])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connection_during_error(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("error old mcdonald [\n")
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError("lost connection during error "
                                            "report of test 'old mcdonald'"))])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connected_after_failure(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure old mcdonald\n")
        self.protocol.lostConnection()
        test = subunit.RemotedTestCase("old mcdonald")
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError())])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connection_during_failure(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("failure old mcdonald [\n")
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls,
                         [(self.test,
                           subunit.RemoteError("lost connection during "
                                               "failure report"
                                               " of test 'old mcdonald'"))])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [])

    def test_lost_connection_after_success(self):
        self.protocol.lineReceived("test old mcdonald\n")
        self.protocol.lineReceived("success old mcdonald\n")
        self.protocol.lostConnection()
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls, [])
        self.assertEqual(self.client.success_calls, [self.test])


class TestTestProtocolServerAddError(unittest.TestCase):

    def setUp(self):
        self.client = MockTestProtocolServerClient()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def simple_error_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError(""))])
        self.assertEqual(self.client.failure_calls, [])

    def test_simple_error(self):
        self.simple_error_keyword("error")

    def test_simple_error_colon(self):
        self.simple_error_keyword("error:")

    def test_error_empty_message(self):
        self.protocol.lineReceived("error mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError(""))])
        self.assertEqual(self.client.failure_calls, [])

    def error_quoted_bracket(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [
            (self.test, subunit.RemoteError("]\n"))])
        self.assertEqual(self.client.failure_calls, [])

    def test_error_quoted_bracket(self):
        self.error_quoted_bracket("error")

    def test_error_colon_quoted_bracket(self):
        self.error_quoted_bracket("error:")


class TestTestProtocolServerAddFailure(unittest.TestCase):

    def setUp(self):
        self.client = MockTestProtocolServerClient()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def simple_failure_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError())])

    def test_simple_failure(self):
        self.simple_failure_keyword("failure")

    def test_simple_failure_colon(self):
        self.simple_failure_keyword("failure:")

    def test_failure_empty_message(self):
        self.protocol.lineReceived("failure mcdonalds farm [\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError())])

    def failure_quoted_bracket(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm [\n" % keyword)
        self.protocol.lineReceived(" ]\n")
        self.protocol.lineReceived("]\n")
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.failure_calls,
                         [(self.test, subunit.RemoteError("]\n"))])

    def test_failure_quoted_bracket(self):
        self.failure_quoted_bracket("failure")

    def test_failure_colon_quoted_bracket(self):
        self.failure_quoted_bracket("failure:")


class TestTestProtocolServerAddSuccess(unittest.TestCase):

    def setUp(self):
        self.client = MockTestProtocolServerClient()
        self.protocol = subunit.TestProtocolServer(self.client)
        self.protocol.lineReceived("test mcdonalds farm\n")
        self.test = subunit.RemotedTestCase("mcdonalds farm")

    def simple_success_keyword(self, keyword):
        self.protocol.lineReceived("%s mcdonalds farm\n" % keyword)
        self.assertEqual(self.client.start_calls, [self.test])
        self.assertEqual(self.client.end_calls, [self.test])
        self.assertEqual(self.client.error_calls, [])
        self.assertEqual(self.client.success_calls, [self.test])

    def test_simple_success(self):
        self.simple_success_keyword("failure")

    def test_simple_success_colon(self):
        self.simple_success_keyword("failure:")

    def test_simple_success(self):
        self.simple_success_keyword("successful")

    def test_simple_success_colon(self):
        self.simple_success_keyword("successful:")


class TestRemotedTestCase(unittest.TestCase):

    def test_simple(self):
        test = subunit.RemotedTestCase("A test description")
        self.assertRaises(NotImplementedError, test.setUp)
        self.assertRaises(NotImplementedError, test.tearDown)
        self.assertEqual("A test description",
                         test.shortDescription())
        self.assertEqual("subunit.RemotedTestCase.A test description",
                         test.id())
        self.assertEqual("A test description (subunit.RemotedTestCase)", "%s" % test)
        self.assertEqual("<subunit.RemotedTestCase description="
                         "'A test description'>", "%r" % test)
        result = unittest.TestResult()
        test.run(result)
        self.assertEqual([(test, "RemoteException: "
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
        error = subunit.RemoteError("Something went wrong")
        another_error = subunit.RemoteError("Something went wrong")
        different_error = subunit.RemoteError("boo!")
        self.assertEqual(error, another_error)
        self.assertNotEqual(error, different_error)
        self.assertNotEqual(different_error, another_error)

    def test_empty_constructor(self):
        self.assertEqual(subunit.RemoteError(), subunit.RemoteError(""))


class TestExecTestCase(unittest.TestCase):

    class SampleExecTestCase(subunit.ExecTestCase):

        def test_sample_method(self):
            """sample-script.py"""
            # the sample script runs three tests, one each
            # that fails, errors and succeeds


    def test_construct(self):
        test = self.SampleExecTestCase("test_sample_method")
        self.assertEqual(test.script,
                         subunit.join_dir(__file__, 'sample-script.py'))

    def test_run(self):
        runner = MockTestProtocolServerClient()
        test = self.SampleExecTestCase("test_sample_method")
        test.run(runner)
        mcdonald = subunit.RemotedTestCase("old mcdonald")
        bing = subunit.RemotedTestCase("bing crosby")
        an_error = subunit.RemotedTestCase("an error")
        self.assertEqual(runner.error_calls,
                         [(an_error, subunit.RemoteError())])
        self.assertEqual(runner.failure_calls,
                         [(bing,
                           subunit.RemoteError(
                            "foo.c:53:ERROR invalid state\n"))])
        self.assertEqual(runner.start_calls, [mcdonald, bing, an_error])
        self.assertEqual(runner.end_calls, [mcdonald, bing, an_error])

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


    def test_start_test(self):
        """Test startTest on a TestProtocolClient."""
        self.protocol.startTest(self.test)
        self.assertEqual(self.io.getvalue(), "test: %s\n" % self.test.id())

    def test_stop_test(self):
        """Test stopTest on a TestProtocolClient."""
        self.protocol.stopTest(self.test)
        self.assertEqual(self.io.getvalue(), "")

    def test_add_success(self):
        """Test addSuccess on a TestProtocolClient."""
        self.protocol.addSuccess(self.test)
        self.assertEqual(
            self.io.getvalue(), "successful: %s\n" % self.test.id())

    def test_add_failure(self):
        """Test addFailure on a TestProtocolClient."""
        self.protocol.addFailure(self.test, subunit.RemoteError("boo"))
        self.assertEqual(
            self.io.getvalue(),
            'failure: %s [\nRemoteException: boo\n]\n' % self.test.id())

    def test_add_error(self):
        """Test stopTest on a TestProtocolClient."""
        self.protocol.addError(self.test, subunit.RemoteError("phwoar"))
        self.assertEqual(
            self.io.getvalue(),
            'error: %s [\n'
            "RemoteException: phwoar\n"
            "]\n" % self.test.id())


def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result
