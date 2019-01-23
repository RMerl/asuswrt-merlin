import unittest

from testtools import (
    TestCase,
    content,
    content_type,
    )
from testtools.helpers import try_import
from testtools.tests.helpers import (
    ExtendedTestResult,
    )

fixtures = try_import('fixtures')
LoggingFixture = try_import('fixtures.tests.helpers.LoggingFixture')


class TestFixtureSupport(TestCase):

    def setUp(self):
        super(TestFixtureSupport, self).setUp()
        if fixtures is None or LoggingFixture is None:
            self.skipTest("Need fixtures")

    def test_useFixture(self):
        fixture = LoggingFixture()
        class SimpleTest(TestCase):
            def test_foo(self):
                self.useFixture(fixture)
        result = unittest.TestResult()
        SimpleTest('test_foo').run(result)
        self.assertTrue(result.wasSuccessful())
        self.assertEqual(['setUp', 'cleanUp'], fixture.calls)

    def test_useFixture_cleanups_raise_caught(self):
        calls = []
        def raiser(ignored):
            calls.append('called')
            raise Exception('foo')
        fixture = fixtures.FunctionFixture(lambda:None, raiser)
        class SimpleTest(TestCase):
            def test_foo(self):
                self.useFixture(fixture)
        result = unittest.TestResult()
        SimpleTest('test_foo').run(result)
        self.assertFalse(result.wasSuccessful())
        self.assertEqual(['called'], calls)

    def test_useFixture_details_captured(self):
        class DetailsFixture(fixtures.Fixture):
            def setUp(self):
                fixtures.Fixture.setUp(self)
                self.addCleanup(delattr, self, 'content')
                self.content = ['content available until cleanUp']
                self.addDetail('content',
                    content.Content(content_type.UTF8_TEXT, self.get_content))
            def get_content(self):
                return self.content
        fixture = DetailsFixture()
        class SimpleTest(TestCase):
            def test_foo(self):
                self.useFixture(fixture)
                # Add a colliding detail (both should show up)
                self.addDetail('content',
                    content.Content(content_type.UTF8_TEXT, lambda:['foo']))
        result = ExtendedTestResult()
        SimpleTest('test_foo').run(result)
        self.assertEqual('addSuccess', result._events[-2][0])
        details = result._events[-2][2]
        self.assertEqual(['content', 'content-1'], sorted(details.keys()))
        self.assertEqual('foo', ''.join(details['content'].iter_text()))
        self.assertEqual('content available until cleanUp',
            ''.join(details['content-1'].iter_text()))


def test_suite():
    from unittest import TestLoader
    return TestLoader().loadTestsFromName(__name__)
