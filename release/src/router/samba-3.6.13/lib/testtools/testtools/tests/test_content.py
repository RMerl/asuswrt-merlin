# Copyright (c) 2008-2010 Jonathan M. Lange. See LICENSE for details.

import unittest
from testtools import TestCase
from testtools.compat import _b, _u
from testtools.content import Content, TracebackContent, text_content
from testtools.content_type import ContentType, UTF8_TEXT
from testtools.matchers import MatchesException, Raises
from testtools.tests.helpers import an_exc_info


raises_value_error = Raises(MatchesException(ValueError))


class TestContent(TestCase):

    def test___init___None_errors(self):
        self.assertThat(lambda:Content(None, None), raises_value_error)
        self.assertThat(lambda:Content(None, lambda: ["traceback"]),
            raises_value_error)
        self.assertThat(lambda:Content(ContentType("text", "traceback"), None),
            raises_value_error)

    def test___init___sets_ivars(self):
        content_type = ContentType("foo", "bar")
        content = Content(content_type, lambda: ["bytes"])
        self.assertEqual(content_type, content.content_type)
        self.assertEqual(["bytes"], list(content.iter_bytes()))

    def test___eq__(self):
        content_type = ContentType("foo", "bar")
        one_chunk = lambda: [_b("bytes")]
        two_chunk = lambda: [_b("by"), _b("tes")]
        content1 = Content(content_type, one_chunk)
        content2 = Content(content_type, one_chunk)
        content3 = Content(content_type, two_chunk)
        content4 = Content(content_type, lambda: [_b("by"), _b("te")])
        content5 = Content(ContentType("f", "b"), two_chunk)
        self.assertEqual(content1, content2)
        self.assertEqual(content1, content3)
        self.assertNotEqual(content1, content4)
        self.assertNotEqual(content1, content5)

    def test___repr__(self):
        content = Content(ContentType("application", "octet-stream"),
            lambda: [_b("\x00bin"), _b("ary\xff")])
        self.assertIn("\\x00binary\\xff", repr(content))

    def test_iter_text_not_text_errors(self):
        content_type = ContentType("foo", "bar")
        content = Content(content_type, lambda: ["bytes"])
        self.assertThat(content.iter_text, raises_value_error)

    def test_iter_text_decodes(self):
        content_type = ContentType("text", "strange", {"charset": "utf8"})
        content = Content(
            content_type, lambda: [_u("bytes\xea").encode("utf8")])
        self.assertEqual([_u("bytes\xea")], list(content.iter_text()))

    def test_iter_text_default_charset_iso_8859_1(self):
        content_type = ContentType("text", "strange")
        text = _u("bytes\xea")
        iso_version = text.encode("ISO-8859-1")
        content = Content(content_type, lambda: [iso_version])
        self.assertEqual([text], list(content.iter_text()))


class TestTracebackContent(TestCase):

    def test___init___None_errors(self):
        self.assertThat(lambda:TracebackContent(None, None),
            raises_value_error) 

    def test___init___sets_ivars(self):
        content = TracebackContent(an_exc_info, self)
        content_type = ContentType("text", "x-traceback",
            {"language": "python", "charset": "utf8"})
        self.assertEqual(content_type, content.content_type)
        result = unittest.TestResult()
        expected = result._exc_info_to_string(an_exc_info, self)
        self.assertEqual(expected, ''.join(list(content.iter_text())))


class TestBytesContent(TestCase):

    def test_bytes(self):
        data = _u("some data")
        expected = Content(UTF8_TEXT, lambda: [data.encode('utf8')])
        self.assertEqual(expected, text_content(data))


def test_suite():
    from unittest import TestLoader
    return TestLoader().loadTestsFromName(__name__)
