#
#  subunit: extensions to python unittest to get test results from subprocesses.
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

from cStringIO import StringIO
import unittest

import subunit.chunked


def test_suite():
    loader = subunit.tests.TestUtil.TestLoader()
    result = loader.loadTestsFromName(__name__)
    return result


class TestDecode(unittest.TestCase):

    def setUp(self):
        unittest.TestCase.setUp(self)
        self.output = StringIO()
        self.decoder = subunit.chunked.Decoder(self.output)

    def test_close_read_length_short_errors(self):
        self.assertRaises(ValueError, self.decoder.close)

    def test_close_body_short_errors(self):
        self.assertEqual(None, self.decoder.write('2\r\na'))
        self.assertRaises(ValueError, self.decoder.close)

    def test_close_body_buffered_data_errors(self):
        self.assertEqual(None, self.decoder.write('2\r'))
        self.assertRaises(ValueError, self.decoder.close)

    def test_close_after_finished_stream_safe(self):
        self.assertEqual(None, self.decoder.write('2\r\nab'))
        self.assertEqual('', self.decoder.write('0\r\n'))
        self.decoder.close()

    def test_decode_nothing(self):
        self.assertEqual('', self.decoder.write('0\r\n'))
        self.assertEqual('', self.output.getvalue())

    def test_decode_serialised_form(self):
        self.assertEqual(None, self.decoder.write("F\r\n"))
        self.assertEqual(None, self.decoder.write("serialised\n"))
        self.assertEqual('', self.decoder.write("form0\r\n"))

    def test_decode_short(self):
        self.assertEqual('', self.decoder.write('3\r\nabc0\r\n'))
        self.assertEqual('abc', self.output.getvalue())

    def test_decode_combines_short(self):
        self.assertEqual('', self.decoder.write('6\r\nabcdef0\r\n'))
        self.assertEqual('abcdef', self.output.getvalue())

    def test_decode_excess_bytes_from_write(self):
        self.assertEqual('1234', self.decoder.write('3\r\nabc0\r\n1234'))
        self.assertEqual('abc', self.output.getvalue())

    def test_decode_write_after_finished_errors(self):
        self.assertEqual('1234', self.decoder.write('3\r\nabc0\r\n1234'))
        self.assertRaises(ValueError, self.decoder.write, '')

    def test_decode_hex(self):
        self.assertEqual('', self.decoder.write('A\r\n12345678900\r\n'))
        self.assertEqual('1234567890', self.output.getvalue())

    def test_decode_long_ranges(self):
        self.assertEqual(None, self.decoder.write('10000\r\n'))
        self.assertEqual(None, self.decoder.write('1' * 65536))
        self.assertEqual(None, self.decoder.write('10000\r\n'))
        self.assertEqual(None, self.decoder.write('2' * 65536))
        self.assertEqual('', self.decoder.write('0\r\n'))
        self.assertEqual('1' * 65536 + '2' * 65536, self.output.getvalue())


class TestEncode(unittest.TestCase):

    def setUp(self):
        unittest.TestCase.setUp(self)
        self.output = StringIO()
        self.encoder = subunit.chunked.Encoder(self.output)

    def test_encode_nothing(self):
        self.encoder.close()
        self.assertEqual('0\r\n', self.output.getvalue())

    def test_encode_empty(self):
        self.encoder.write('')
        self.encoder.close()
        self.assertEqual('0\r\n', self.output.getvalue())

    def test_encode_short(self):
        self.encoder.write('abc')
        self.encoder.close()
        self.assertEqual('3\r\nabc0\r\n', self.output.getvalue())

    def test_encode_combines_short(self):
        self.encoder.write('abc')
        self.encoder.write('def')
        self.encoder.close()
        self.assertEqual('6\r\nabcdef0\r\n', self.output.getvalue())

    def test_encode_over_9_is_in_hex(self):
        self.encoder.write('1234567890')
        self.encoder.close()
        self.assertEqual('A\r\n12345678900\r\n', self.output.getvalue())

    def test_encode_long_ranges_not_combined(self):
        self.encoder.write('1' * 65536)
        self.encoder.write('2' * 65536)
        self.encoder.close()
        self.assertEqual('10000\r\n' + '1' * 65536 + '10000\r\n' +
            '2' * 65536 + '0\r\n', self.output.getvalue())
