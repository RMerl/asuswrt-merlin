# Copyright (c) 2008-2010 Jonathan M. Lange. See LICENSE for details.

"""Tests for matchers."""

import doctest
import sys

from testtools import (
    Matcher, # check that Matcher is exposed at the top level for docs.
    TestCase,
    )
from testtools.matchers import (
    Annotate,
    Equals,
    DocTestMatches,
    DoesNotEndWith,
    DoesNotStartWith,
    EndsWith,
    KeysEqual,
    Is,
    LessThan,
    MatchesAny,
    MatchesAll,
    MatchesException,
    Mismatch,
    Not,
    NotEquals,
    Raises,
    raises,
    StartsWith,
    )

# Silence pyflakes.
Matcher


class TestMismatch(TestCase):

    def test_constructor_arguments(self):
        mismatch = Mismatch("some description", {'detail': "things"})
        self.assertEqual("some description", mismatch.describe())
        self.assertEqual({'detail': "things"}, mismatch.get_details())

    def test_constructor_no_arguments(self):
        mismatch = Mismatch()
        self.assertThat(mismatch.describe,
            Raises(MatchesException(NotImplementedError)))
        self.assertEqual({}, mismatch.get_details())


class TestMatchersInterface(object):

    def test_matches_match(self):
        matcher = self.matches_matcher
        matches = self.matches_matches
        mismatches = self.matches_mismatches
        for candidate in matches:
            self.assertEqual(None, matcher.match(candidate))
        for candidate in mismatches:
            mismatch = matcher.match(candidate)
            self.assertNotEqual(None, mismatch)
            self.assertNotEqual(None, getattr(mismatch, 'describe', None))

    def test__str__(self):
        # [(expected, object to __str__)].
        examples = self.str_examples
        for expected, matcher in examples:
            self.assertThat(matcher, DocTestMatches(expected))

    def test_describe_difference(self):
        # [(expected, matchee, matcher), ...]
        examples = self.describe_examples
        for difference, matchee, matcher in examples:
            mismatch = matcher.match(matchee)
            self.assertEqual(difference, mismatch.describe())

    def test_mismatch_details(self):
        # The mismatch object must provide get_details, which must return a
        # dictionary mapping names to Content objects.
        examples = self.describe_examples
        for difference, matchee, matcher in examples:
            mismatch = matcher.match(matchee)
            details = mismatch.get_details()
            self.assertEqual(dict(details), details)


class TestDocTestMatchesInterface(TestCase, TestMatchersInterface):

    matches_matcher = DocTestMatches("Ran 1 test in ...s", doctest.ELLIPSIS)
    matches_matches = ["Ran 1 test in 0.000s", "Ran 1 test in 1.234s"]
    matches_mismatches = ["Ran 1 tests in 0.000s", "Ran 2 test in 0.000s"]

    str_examples = [("DocTestMatches('Ran 1 test in ...s\\n')",
        DocTestMatches("Ran 1 test in ...s")),
        ("DocTestMatches('foo\\n', flags=8)", DocTestMatches("foo", flags=8)),
        ]

    describe_examples = [('Expected:\n    Ran 1 tests in ...s\nGot:\n'
        '    Ran 1 test in 0.123s\n', "Ran 1 test in 0.123s",
        DocTestMatches("Ran 1 tests in ...s", doctest.ELLIPSIS))]


class TestDocTestMatchesSpecific(TestCase):

    def test___init__simple(self):
        matcher = DocTestMatches("foo")
        self.assertEqual("foo\n", matcher.want)

    def test___init__flags(self):
        matcher = DocTestMatches("bar\n", doctest.ELLIPSIS)
        self.assertEqual("bar\n", matcher.want)
        self.assertEqual(doctest.ELLIPSIS, matcher.flags)


class TestEqualsInterface(TestCase, TestMatchersInterface):

    matches_matcher = Equals(1)
    matches_matches = [1]
    matches_mismatches = [2]

    str_examples = [("Equals(1)", Equals(1)), ("Equals('1')", Equals('1'))]

    describe_examples = [("1 != 2", 2, Equals(1))]


class TestNotEqualsInterface(TestCase, TestMatchersInterface):

    matches_matcher = NotEquals(1)
    matches_matches = [2]
    matches_mismatches = [1]

    str_examples = [
        ("NotEquals(1)", NotEquals(1)), ("NotEquals('1')", NotEquals('1'))]

    describe_examples = [("1 == 1", 1, NotEquals(1))]


class TestIsInterface(TestCase, TestMatchersInterface):

    foo = object()
    bar = object()

    matches_matcher = Is(foo)
    matches_matches = [foo]
    matches_mismatches = [bar, 1]

    str_examples = [("Is(2)", Is(2))]

    describe_examples = [("1 is not 2", 2, Is(1))]


class TestLessThanInterface(TestCase, TestMatchersInterface):

    matches_matcher = LessThan(4)
    matches_matches = [-5, 3]
    matches_mismatches = [4, 5, 5000]

    str_examples = [
        ("LessThan(12)", LessThan(12)),
        ]

    describe_examples = [('4 is >= 4', 4, LessThan(4))]


def make_error(type, *args, **kwargs):
    try:
        raise type(*args, **kwargs)
    except type:
        return sys.exc_info()


class TestMatchesExceptionInstanceInterface(TestCase, TestMatchersInterface):

    matches_matcher = MatchesException(ValueError("foo"))
    error_foo = make_error(ValueError, 'foo')
    error_bar = make_error(ValueError, 'bar')
    error_base_foo = make_error(Exception, 'foo')
    matches_matches = [error_foo]
    matches_mismatches = [error_bar, error_base_foo]

    str_examples = [
        ("MatchesException(Exception('foo',))",
         MatchesException(Exception('foo')))
        ]
    describe_examples = [
        ("%r is not a %r" % (Exception, ValueError),
         error_base_foo,
         MatchesException(ValueError("foo"))),
        ("ValueError('bar',) has different arguments to ValueError('foo',).",
         error_bar,
         MatchesException(ValueError("foo"))),
        ]


class TestMatchesExceptionTypeInterface(TestCase, TestMatchersInterface):

    matches_matcher = MatchesException(ValueError)
    error_foo = make_error(ValueError, 'foo')
    error_sub = make_error(UnicodeError, 'bar')
    error_base_foo = make_error(Exception, 'foo')
    matches_matches = [error_foo, error_sub]
    matches_mismatches = [error_base_foo]

    str_examples = [
        ("MatchesException(%r)" % Exception,
         MatchesException(Exception))
        ]
    describe_examples = [
        ("%r is not a %r" % (Exception, ValueError),
         error_base_foo,
         MatchesException(ValueError)),
        ]


class TestNotInterface(TestCase, TestMatchersInterface):

    matches_matcher = Not(Equals(1))
    matches_matches = [2]
    matches_mismatches = [1]

    str_examples = [
        ("Not(Equals(1))", Not(Equals(1))),
        ("Not(Equals('1'))", Not(Equals('1')))]

    describe_examples = [('1 matches Equals(1)', 1, Not(Equals(1)))]


class TestMatchersAnyInterface(TestCase, TestMatchersInterface):

    matches_matcher = MatchesAny(DocTestMatches("1"), DocTestMatches("2"))
    matches_matches = ["1", "2"]
    matches_mismatches = ["3"]

    str_examples = [(
        "MatchesAny(DocTestMatches('1\\n'), DocTestMatches('2\\n'))",
        MatchesAny(DocTestMatches("1"), DocTestMatches("2"))),
        ]

    describe_examples = [("""Differences: [
Expected:
    1
Got:
    3

Expected:
    2
Got:
    3

]""",
        "3", MatchesAny(DocTestMatches("1"), DocTestMatches("2")))]


class TestMatchesAllInterface(TestCase, TestMatchersInterface):

    matches_matcher = MatchesAll(NotEquals(1), NotEquals(2))
    matches_matches = [3, 4]
    matches_mismatches = [1, 2]

    str_examples = [
        ("MatchesAll(NotEquals(1), NotEquals(2))",
         MatchesAll(NotEquals(1), NotEquals(2)))]

    describe_examples = [("""Differences: [
1 == 1
]""",
                          1, MatchesAll(NotEquals(1), NotEquals(2)))]


class TestKeysEqual(TestCase, TestMatchersInterface):

    matches_matcher = KeysEqual('foo', 'bar')
    matches_matches = [
        {'foo': 0, 'bar': 1},
        ]
    matches_mismatches = [
        {},
        {'foo': 0},
        {'bar': 1},
        {'foo': 0, 'bar': 1, 'baz': 2},
        {'a': None, 'b': None, 'c': None},
        ]

    str_examples = [
        ("KeysEqual('foo', 'bar')", KeysEqual('foo', 'bar')),
        ]

    describe_examples = [
        ("['bar', 'foo'] does not match {'baz': 2, 'foo': 0, 'bar': 1}: "
         "Keys not equal",
         {'foo': 0, 'bar': 1, 'baz': 2}, KeysEqual('foo', 'bar')),
        ]


class TestAnnotate(TestCase, TestMatchersInterface):

    matches_matcher = Annotate("foo", Equals(1))
    matches_matches = [1]
    matches_mismatches = [2]

    str_examples = [
        ("Annotate('foo', Equals(1))", Annotate("foo", Equals(1)))]

    describe_examples = [("1 != 2: foo", 2, Annotate('foo', Equals(1)))]


class TestRaisesInterface(TestCase, TestMatchersInterface):

    matches_matcher = Raises()
    def boom():
        raise Exception('foo')
    matches_matches = [boom]
    matches_mismatches = [lambda:None]

    # Tricky to get function objects to render constantly, and the interfaces
    # helper uses assertEqual rather than (for instance) DocTestMatches.
    str_examples = []

    describe_examples = []


class TestRaisesExceptionMatcherInterface(TestCase, TestMatchersInterface):

    matches_matcher = Raises(
        exception_matcher=MatchesException(Exception('foo')))
    def boom_bar():
        raise Exception('bar')
    def boom_foo():
        raise Exception('foo')
    matches_matches = [boom_foo]
    matches_mismatches = [lambda:None, boom_bar]

    # Tricky to get function objects to render constantly, and the interfaces
    # helper uses assertEqual rather than (for instance) DocTestMatches.
    str_examples = []

    describe_examples = []


class TestRaisesBaseTypes(TestCase):

    def raiser(self):
        raise KeyboardInterrupt('foo')

    def test_KeyboardInterrupt_matched(self):
        # When KeyboardInterrupt is matched, it is swallowed.
        matcher = Raises(MatchesException(KeyboardInterrupt))
        self.assertThat(self.raiser, matcher)

    def test_KeyboardInterrupt_propogates(self):
        # The default 'it raised' propogates KeyboardInterrupt.
        match_keyb = Raises(MatchesException(KeyboardInterrupt))
        def raise_keyb_from_match():
            matcher = Raises()
            matcher.match(self.raiser)
        self.assertThat(raise_keyb_from_match, match_keyb)

    def test_KeyboardInterrupt_match_Exception_propogates(self):
        # If the raised exception isn't matched, and it is not a subclass of
        # Exception, it is propogated.
        match_keyb = Raises(MatchesException(KeyboardInterrupt))
        def raise_keyb_from_match():
            if sys.version_info > (2, 5):
                matcher = Raises(MatchesException(Exception))
            else:
                # On Python 2.4 KeyboardInterrupt is a StandardError subclass
                # but should propogate from less generic exception matchers
                matcher = Raises(MatchesException(EnvironmentError))
            matcher.match(self.raiser)
        self.assertThat(raise_keyb_from_match, match_keyb)


class TestRaisesConvenience(TestCase):

    def test_exc_type(self):
        self.assertThat(lambda: 1/0, raises(ZeroDivisionError))

    def test_exc_value(self):
        e = RuntimeError("You lose!")
        def raiser():
            raise e
        self.assertThat(raiser, raises(e))


class DoesNotStartWithTests(TestCase):

    def test_describe(self):
        mismatch = DoesNotStartWith("fo", "bo")
        self.assertEqual("'fo' does not start with 'bo'.", mismatch.describe())


class StartsWithTests(TestCase):

    def test_str(self):
        matcher = StartsWith("bar")
        self.assertEqual("Starts with 'bar'.", str(matcher))

    def test_match(self):
        matcher = StartsWith("bar")
        self.assertIs(None, matcher.match("barf"))

    def test_mismatch_returns_does_not_start_with(self):
        matcher = StartsWith("bar")
        self.assertIsInstance(matcher.match("foo"), DoesNotStartWith)

    def test_mismatch_sets_matchee(self):
        matcher = StartsWith("bar")
        mismatch = matcher.match("foo")
        self.assertEqual("foo", mismatch.matchee)

    def test_mismatch_sets_expected(self):
        matcher = StartsWith("bar")
        mismatch = matcher.match("foo")
        self.assertEqual("bar", mismatch.expected)


class DoesNotEndWithTests(TestCase):

    def test_describe(self):
        mismatch = DoesNotEndWith("fo", "bo")
        self.assertEqual("'fo' does not end with 'bo'.", mismatch.describe())


class EndsWithTests(TestCase):

    def test_str(self):
        matcher = EndsWith("bar")
        self.assertEqual("Ends with 'bar'.", str(matcher))

    def test_match(self):
        matcher = EndsWith("arf")
        self.assertIs(None, matcher.match("barf"))

    def test_mismatch_returns_does_not_end_with(self):
        matcher = EndsWith("bar")
        self.assertIsInstance(matcher.match("foo"), DoesNotEndWith)

    def test_mismatch_sets_matchee(self):
        matcher = EndsWith("bar")
        mismatch = matcher.match("foo")
        self.assertEqual("foo", mismatch.matchee)

    def test_mismatch_sets_expected(self):
        matcher = EndsWith("bar")
        mismatch = matcher.match("foo")
        self.assertEqual("bar", mismatch.expected)


def test_suite():
    from unittest import TestLoader
    return TestLoader().loadTestsFromName(__name__)
