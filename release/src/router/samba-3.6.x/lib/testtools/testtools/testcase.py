# Copyright (c) 2008-2010 Jonathan M. Lange. See LICENSE for details.

"""Test case related stuff."""

__metaclass__ = type
__all__ = [
    'clone_test_with_new_id',
    'run_test_with',
    'skip',
    'skipIf',
    'skipUnless',
    'TestCase',
    ]

import copy
import itertools
import sys
import types
import unittest

from testtools import (
    content,
    try_import,
    )
from testtools.compat import advance_iterator
from testtools.matchers import (
    Annotate,
    Equals,
    )
from testtools.monkey import patch
from testtools.runtest import RunTest
from testtools.testresult import TestResult

wraps = try_import('functools.wraps')

class TestSkipped(Exception):
    """Raised within TestCase.run() when a test is skipped."""
TestSkipped = try_import('unittest.case.SkipTest', TestSkipped)


class _UnexpectedSuccess(Exception):
    """An unexpected success was raised.

    Note that this exception is private plumbing in testtools' testcase
    module.
    """
_UnexpectedSuccess = try_import(
    'unittest.case._UnexpectedSuccess', _UnexpectedSuccess)

class _ExpectedFailure(Exception):
    """An expected failure occured.

    Note that this exception is private plumbing in testtools' testcase
    module.
    """
_ExpectedFailure = try_import(
    'unittest.case._ExpectedFailure', _ExpectedFailure)


def run_test_with(test_runner, **kwargs):
    """Decorate a test as using a specific `RunTest`.

    e.g.
      @run_test_with(CustomRunner, timeout=42)
      def test_foo(self):
          self.assertTrue(True)

    The returned decorator works by setting an attribute on the decorated
    function.  `TestCase.__init__` looks for this attribute when deciding
    on a `RunTest` factory.  If you wish to use multiple decorators on a test
    method, then you must either make this one the top-most decorator, or
    you must write your decorators so that they update the wrapping function
    with the attributes of the wrapped function.  The latter is recommended
    style anyway.  `functools.wraps`, `functools.wrapper` and
    `twisted.python.util.mergeFunctionMetadata` can help you do this.

    :param test_runner: A `RunTest` factory that takes a test case and an
        optional list of exception handlers.  See `RunTest`.
    :param **kwargs: Keyword arguments to pass on as extra arguments to
        `test_runner`.
    :return: A decorator to be used for marking a test as needing a special
        runner.
    """
    def decorator(function):
        # Set an attribute on 'function' which will inform TestCase how to
        # make the runner.
        function._run_test_with = (
            lambda case, handlers=None:
                test_runner(case, handlers=handlers, **kwargs))
        return function
    return decorator


class TestCase(unittest.TestCase):
    """Extensions to the basic TestCase.

    :ivar exception_handlers: Exceptions to catch from setUp, runTest and
        tearDown. This list is able to be modified at any time and consists of
        (exception_class, handler(case, result, exception_value)) pairs.
    :cvar run_tests_with: A factory to make the `RunTest` to run tests with.
        Defaults to `RunTest`.  The factory is expected to take a test case
        and an optional list of exception handlers.
    """

    skipException = TestSkipped

    run_tests_with = RunTest

    def __init__(self, *args, **kwargs):
        """Construct a TestCase.

        :param testMethod: The name of the method to run.
        :param runTest: Optional class to use to execute the test. If not
            supplied `testtools.runtest.RunTest` is used. The instance to be
            used is created when run() is invoked, so will be fresh each time.
            Overrides `run_tests_with` if given.
        """
        runTest = kwargs.pop('runTest', None)
        unittest.TestCase.__init__(self, *args, **kwargs)
        self._cleanups = []
        self._unique_id_gen = itertools.count(1)
        # Generators to ensure unique traceback ids.  Maps traceback label to
        # iterators.
        self._traceback_id_gens = {}
        self.__setup_called = False
        self.__teardown_called = False
        # __details is lazy-initialized so that a constructed-but-not-run
        # TestCase is safe to use with clone_test_with_new_id.
        self.__details = None
        test_method = self._get_test_method()
        if runTest is None:
            runTest = getattr(
                test_method, '_run_test_with', self.run_tests_with)
        self.__RunTest = runTest
        self.__exception_handlers = []
        self.exception_handlers = [
            (self.skipException, self._report_skip),
            (self.failureException, self._report_failure),
            (_ExpectedFailure, self._report_expected_failure),
            (_UnexpectedSuccess, self._report_unexpected_success),
            (Exception, self._report_error),
            ]
        if sys.version_info < (2, 6):
            # Catch old-style string exceptions with None as the instance
            self.exception_handlers.append((type(None), self._report_error))

    def __eq__(self, other):
        eq = getattr(unittest.TestCase, '__eq__', None)
        if eq is not None and not unittest.TestCase.__eq__(self, other):
            return False
        return self.__dict__ == other.__dict__

    def __repr__(self):
        # We add id to the repr because it makes testing testtools easier.
        return "<%s id=0x%0x>" % (self.id(), id(self))

    def addDetail(self, name, content_object):
        """Add a detail to be reported with this test's outcome.

        For more details see pydoc testtools.TestResult.

        :param name: The name to give this detail.
        :param content_object: The content object for this detail. See
            testtools.content for more detail.
        """
        if self.__details is None:
            self.__details = {}
        self.__details[name] = content_object

    def getDetails(self):
        """Get the details dict that will be reported with this test's outcome.

        For more details see pydoc testtools.TestResult.
        """
        if self.__details is None:
            self.__details = {}
        return self.__details

    def patch(self, obj, attribute, value):
        """Monkey-patch 'obj.attribute' to 'value' while the test is running.

        If 'obj' has no attribute, then the monkey-patch will still go ahead,
        and the attribute will be deleted instead of restored to its original
        value.

        :param obj: The object to patch. Can be anything.
        :param attribute: The attribute on 'obj' to patch.
        :param value: The value to set 'obj.attribute' to.
        """
        self.addCleanup(patch(obj, attribute, value))

    def shortDescription(self):
        return self.id()

    def skipTest(self, reason):
        """Cause this test to be skipped.

        This raises self.skipException(reason). skipException is raised
        to permit a skip to be triggered at any point (during setUp or the
        testMethod itself). The run() method catches skipException and
        translates that into a call to the result objects addSkip method.

        :param reason: The reason why the test is being skipped. This must
            support being cast into a unicode string for reporting.
        """
        raise self.skipException(reason)

    # skipTest is how python2.7 spells this. Sometime in the future
    # This should be given a deprecation decorator - RBC 20100611.
    skip = skipTest

    def _formatTypes(self, classOrIterable):
        """Format a class or a bunch of classes for display in an error."""
        className = getattr(classOrIterable, '__name__', None)
        if className is None:
            className = ', '.join(klass.__name__ for klass in classOrIterable)
        return className

    def addCleanup(self, function, *arguments, **keywordArguments):
        """Add a cleanup function to be called after tearDown.

        Functions added with addCleanup will be called in reverse order of
        adding after tearDown, or after setUp if setUp raises an exception.

        If a function added with addCleanup raises an exception, the error
        will be recorded as a test error, and the next cleanup will then be
        run.

        Cleanup functions are always called before a test finishes running,
        even if setUp is aborted by an exception.
        """
        self._cleanups.append((function, arguments, keywordArguments))

    def addOnException(self, handler):
        """Add a handler to be called when an exception occurs in test code.

        This handler cannot affect what result methods are called, and is
        called before any outcome is called on the result object. An example
        use for it is to add some diagnostic state to the test details dict
        which is expensive to calculate and not interesting for reporting in
        the success case.

        Handlers are called before the outcome (such as addFailure) that
        the exception has caused.

        Handlers are called in first-added, first-called order, and if they
        raise an exception, that will propogate out of the test running
        machinery, halting test processing. As a result, do not call code that
        may unreasonably fail.
        """
        self.__exception_handlers.append(handler)

    def _add_reason(self, reason):
        self.addDetail('reason', content.Content(
            content.ContentType('text', 'plain'),
            lambda: [reason.encode('utf8')]))

    def assertEqual(self, expected, observed, message=''):
        """Assert that 'expected' is equal to 'observed'.

        :param expected: The expected value.
        :param observed: The observed value.
        :param message: An optional message to include in the error.
        """
        matcher = Equals(expected)
        if message:
            matcher = Annotate(message, matcher)
        self.assertThat(observed, matcher)

    failUnlessEqual = assertEquals = assertEqual

    def assertIn(self, needle, haystack):
        """Assert that needle is in haystack."""
        self.assertTrue(
            needle in haystack, '%r not in %r' % (needle, haystack))

    def assertIs(self, expected, observed, message=''):
        """Assert that 'expected' is 'observed'.

        :param expected: The expected value.
        :param observed: The observed value.
        :param message: An optional message describing the error.
        """
        if message:
            message = ': ' + message
        self.assertTrue(
            expected is observed,
            '%r is not %r%s' % (expected, observed, message))

    def assertIsNot(self, expected, observed, message=''):
        """Assert that 'expected' is not 'observed'."""
        if message:
            message = ': ' + message
        self.assertTrue(
            expected is not observed,
            '%r is %r%s' % (expected, observed, message))

    def assertNotIn(self, needle, haystack):
        """Assert that needle is not in haystack."""
        self.assertTrue(
            needle not in haystack, '%r in %r' % (needle, haystack))

    def assertIsInstance(self, obj, klass, msg=None):
        if msg is None:
            msg = '%r is not an instance of %s' % (
                obj, self._formatTypes(klass))
        self.assertTrue(isinstance(obj, klass), msg)

    def assertRaises(self, excClass, callableObj, *args, **kwargs):
        """Fail unless an exception of class excClass is thrown
           by callableObj when invoked with arguments args and keyword
           arguments kwargs. If a different type of exception is
           thrown, it will not be caught, and the test case will be
           deemed to have suffered an error, exactly as for an
           unexpected exception.
        """
        try:
            ret = callableObj(*args, **kwargs)
        except excClass:
            return sys.exc_info()[1]
        else:
            excName = self._formatTypes(excClass)
            self.fail("%s not raised, %r returned instead." % (excName, ret))
    failUnlessRaises = assertRaises

    def assertThat(self, matchee, matcher):
        """Assert that matchee is matched by matcher.

        :param matchee: An object to match with matcher.
        :param matcher: An object meeting the testtools.Matcher protocol.
        :raises self.failureException: When matcher does not match thing.
        """
        mismatch = matcher.match(matchee)
        if not mismatch:
            return
        existing_details = self.getDetails()
        for (name, content) in mismatch.get_details().items():
            full_name = name
            suffix = 1
            while full_name in existing_details:
                full_name = "%s-%d" % (name, suffix)
                suffix += 1
            self.addDetail(full_name, content)
        self.fail('Match failed. Matchee: "%s"\nMatcher: %s\nDifference: %s\n'
            % (matchee, matcher, mismatch.describe()))

    def defaultTestResult(self):
        return TestResult()

    def expectFailure(self, reason, predicate, *args, **kwargs):
        """Check that a test fails in a particular way.

        If the test fails in the expected way, a KnownFailure is caused. If it
        succeeds an UnexpectedSuccess is caused.

        The expected use of expectFailure is as a barrier at the point in a
        test where the test would fail. For example:
        >>> def test_foo(self):
        >>>    self.expectFailure("1 should be 0", self.assertNotEqual, 1, 0)
        >>>    self.assertEqual(1, 0)

        If in the future 1 were to equal 0, the expectFailure call can simply
        be removed. This separation preserves the original intent of the test
        while it is in the expectFailure mode.
        """
        self._add_reason(reason)
        try:
            predicate(*args, **kwargs)
        except self.failureException:
            # GZ 2010-08-12: Don't know how to avoid exc_info cycle as the new
            #                unittest _ExpectedFailure wants old traceback
            exc_info = sys.exc_info()
            try:
                self._report_traceback(exc_info)
                raise _ExpectedFailure(exc_info)
            finally:
                del exc_info
        else:
            raise _UnexpectedSuccess(reason)

    def getUniqueInteger(self):
        """Get an integer unique to this test.

        Returns an integer that is guaranteed to be unique to this instance.
        Use this when you need an arbitrary integer in your test, or as a
        helper for custom anonymous factory methods.
        """
        return advance_iterator(self._unique_id_gen)

    def getUniqueString(self, prefix=None):
        """Get a string unique to this test.

        Returns a string that is guaranteed to be unique to this instance. Use
        this when you need an arbitrary string in your test, or as a helper
        for custom anonymous factory methods.

        :param prefix: The prefix of the string. If not provided, defaults
            to the id of the tests.
        :return: A bytestring of '<prefix>-<unique_int>'.
        """
        if prefix is None:
            prefix = self.id()
        return '%s-%d' % (prefix, self.getUniqueInteger())

    def onException(self, exc_info, tb_label='traceback'):
        """Called when an exception propogates from test code.

        :seealso addOnException:
        """
        if exc_info[0] not in [
            TestSkipped, _UnexpectedSuccess, _ExpectedFailure]:
            self._report_traceback(exc_info, tb_label=tb_label)
        for handler in self.__exception_handlers:
            handler(exc_info)

    @staticmethod
    def _report_error(self, result, err):
        result.addError(self, details=self.getDetails())

    @staticmethod
    def _report_expected_failure(self, result, err):
        result.addExpectedFailure(self, details=self.getDetails())

    @staticmethod
    def _report_failure(self, result, err):
        result.addFailure(self, details=self.getDetails())

    @staticmethod
    def _report_skip(self, result, err):
        if err.args:
            reason = err.args[0]
        else:
            reason = "no reason given."
        self._add_reason(reason)
        result.addSkip(self, details=self.getDetails())

    def _report_traceback(self, exc_info, tb_label='traceback'):
        id_gen = self._traceback_id_gens.setdefault(
            tb_label, itertools.count(0))
        tb_id = advance_iterator(id_gen)
        if tb_id:
            tb_label = '%s-%d' % (tb_label, tb_id)
        self.addDetail(tb_label, content.TracebackContent(exc_info, self))

    @staticmethod
    def _report_unexpected_success(self, result, err):
        result.addUnexpectedSuccess(self, details=self.getDetails())

    def run(self, result=None):
        return self.__RunTest(self, self.exception_handlers).run(result)

    def _run_setup(self, result):
        """Run the setUp function for this test.

        :param result: A testtools.TestResult to report activity to.
        :raises ValueError: If the base class setUp is not called, a
            ValueError is raised.
        """
        ret = self.setUp()
        if not self.__setup_called:
            raise ValueError(
                "TestCase.setUp was not called. Have you upcalled all the "
                "way up the hierarchy from your setUp? e.g. Call "
                "super(%s, self).setUp() from your setUp()."
                % self.__class__.__name__)
        return ret

    def _run_teardown(self, result):
        """Run the tearDown function for this test.

        :param result: A testtools.TestResult to report activity to.
        :raises ValueError: If the base class tearDown is not called, a
            ValueError is raised.
        """
        ret = self.tearDown()
        if not self.__teardown_called:
            raise ValueError(
                "TestCase.tearDown was not called. Have you upcalled all the "
                "way up the hierarchy from your tearDown? e.g. Call "
                "super(%s, self).tearDown() from your tearDown()."
                % self.__class__.__name__)
        return ret

    def _get_test_method(self):
        absent_attr = object()
        # Python 2.5+
        method_name = getattr(self, '_testMethodName', absent_attr)
        if method_name is absent_attr:
            # Python 2.4
            method_name = getattr(self, '_TestCase__testMethodName')
        return getattr(self, method_name)

    def _run_test_method(self, result):
        """Run the test method for this test.

        :param result: A testtools.TestResult to report activity to.
        :return: None.
        """
        return self._get_test_method()()

    def useFixture(self, fixture):
        """Use fixture in a test case.

        The fixture will be setUp, and self.addCleanup(fixture.cleanUp) called.

        :param fixture: The fixture to use.
        :return: The fixture, after setting it up and scheduling a cleanup for
           it.
        """
        fixture.setUp()
        self.addCleanup(fixture.cleanUp)
        self.addCleanup(self._gather_details, fixture.getDetails)
        return fixture

    def _gather_details(self, getDetails):
        """Merge the details from getDetails() into self.getDetails()."""
        details = getDetails()
        my_details = self.getDetails()
        for name, content_object in details.items():
            new_name = name
            disambiguator = itertools.count(1)
            while new_name in my_details:
                new_name = '%s-%d' % (name, advance_iterator(disambiguator))
            name = new_name
            content_bytes = list(content_object.iter_bytes())
            content_callback = lambda:content_bytes
            self.addDetail(name,
                content.Content(content_object.content_type, content_callback))

    def setUp(self):
        unittest.TestCase.setUp(self)
        self.__setup_called = True

    def tearDown(self):
        unittest.TestCase.tearDown(self)
        self.__teardown_called = True


class PlaceHolder(object):
    """A placeholder test.

    `PlaceHolder` implements much of the same interface as `TestCase` and is
    particularly suitable for being added to `TestResult`s.
    """

    def __init__(self, test_id, short_description=None):
        """Construct a `PlaceHolder`.

        :param test_id: The id of the placeholder test.
        :param short_description: The short description of the place holder
            test. If not provided, the id will be used instead.
        """
        self._test_id = test_id
        self._short_description = short_description

    def __call__(self, result=None):
        return self.run(result=result)

    def __repr__(self):
        internal = [self._test_id]
        if self._short_description is not None:
            internal.append(self._short_description)
        return "<%s.%s(%s)>" % (
            self.__class__.__module__,
            self.__class__.__name__,
            ", ".join(map(repr, internal)))

    def __str__(self):
        return self.id()

    def countTestCases(self):
        return 1

    def debug(self):
        pass

    def id(self):
        return self._test_id

    def run(self, result=None):
        if result is None:
            result = TestResult()
        result.startTest(self)
        result.addSuccess(self)
        result.stopTest(self)

    def shortDescription(self):
        if self._short_description is None:
            return self.id()
        else:
            return self._short_description


class ErrorHolder(PlaceHolder):
    """A placeholder test that will error out when run."""

    failureException = None

    def __init__(self, test_id, error, short_description=None):
        """Construct an `ErrorHolder`.

        :param test_id: The id of the test.
        :param error: The exc info tuple that will be used as the test's error.
        :param short_description: An optional short description of the test.
        """
        super(ErrorHolder, self).__init__(
            test_id, short_description=short_description)
        self._error = error

    def __repr__(self):
        internal = [self._test_id, self._error]
        if self._short_description is not None:
            internal.append(self._short_description)
        return "<%s.%s(%s)>" % (
            self.__class__.__module__,
            self.__class__.__name__,
            ", ".join(map(repr, internal)))

    def run(self, result=None):
        if result is None:
            result = TestResult()
        result.startTest(self)
        result.addError(self, self._error)
        result.stopTest(self)


# Python 2.4 did not know how to copy functions.
if types.FunctionType not in copy._copy_dispatch:
    copy._copy_dispatch[types.FunctionType] = copy._copy_immutable


def clone_test_with_new_id(test, new_id):
    """Copy a TestCase, and give the copied test a new id.

    This is only expected to be used on tests that have been constructed but
    not executed.
    """
    newTest = copy.copy(test)
    newTest.id = lambda: new_id
    return newTest


def skip(reason):
    """A decorator to skip unit tests.

    This is just syntactic sugar so users don't have to change any of their
    unit tests in order to migrate to python 2.7, which provides the
    @unittest.skip decorator.
    """
    def decorator(test_item):
        if wraps is not None:
            @wraps(test_item)
            def skip_wrapper(*args, **kwargs):
                raise TestCase.skipException(reason)
        else:
            def skip_wrapper(test_item):
                test_item.skip(reason)
        return skip_wrapper
    return decorator


def skipIf(condition, reason):
    """Skip a test if the condition is true."""
    if condition:
        return skip(reason)
    def _id(obj):
        return obj
    return _id


def skipUnless(condition, reason):
    """Skip a test unless the condition is true."""
    if not condition:
        return skip(reason)
    def _id(obj):
        return obj
    return _id
