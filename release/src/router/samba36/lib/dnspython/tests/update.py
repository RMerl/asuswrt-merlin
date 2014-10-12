# Copyright (C) 2003-2007, 2009, 2010 Nominum, Inc.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose with or without fee is hereby granted,
# provided that the above copyright notice and this permission notice
# appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND NOMINUM DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL NOMINUM BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import unittest

import dns.update
import dns.rdata
import dns.rdataset

goodhex = '0001 2800 0001 0005 0007 0000' \
          '076578616d706c6500 0006 0001' \
          '03666f6fc00c 00ff 00ff 00000000 0000' \
          'c019 0001 00ff 00000000 0000' \
          '03626172c00c 0001 0001 00000000 0004 0a000005' \
          '05626c617a32c00c 00ff 00fe 00000000 0000' \
          'c049 0001 00fe 00000000 0000' \
          'c019 0001 00ff 00000000 0000' \
          'c019 0001 0001 0000012c 0004 0a000001' \
          'c019 0001 0001 0000012c 0004 0a000002' \
          'c035 0001 0001 0000012c 0004 0a000003' \
          'c035 0001 00fe 00000000 0004 0a000004' \
          '04626c617ac00c 0001 00ff 00000000 0000' \
          'c049 00ff 00ff 00000000 0000'

goodwire = goodhex.replace(' ', '').decode('hex_codec')

update_text="""id 1
opcode UPDATE
rcode NOERROR
;ZONE
example. IN SOA
;PREREQ
foo ANY ANY
foo ANY A
bar 0 IN A 10.0.0.5
blaz2 NONE ANY
blaz2 NONE A
;UPDATE
foo ANY A
foo 300 IN A 10.0.0.1
foo 300 IN A 10.0.0.2
bar 300 IN A 10.0.0.3
bar 0 NONE A 10.0.0.4
blaz ANY A
blaz2 ANY ANY
"""

class UpdateTestCase(unittest.TestCase):

    def test_to_wire1(self):
        update = dns.update.Update('example')
        update.id = 1
        update.present('foo')
        update.present('foo', 'a')
        update.present('bar', 'a', '10.0.0.5')
        update.absent('blaz2')
        update.absent('blaz2', 'a')
        update.replace('foo', 300, 'a', '10.0.0.1', '10.0.0.2')
        update.add('bar', 300, 'a', '10.0.0.3')
        update.delete('bar', 'a', '10.0.0.4')
        update.delete('blaz','a')
        update.delete('blaz2')
        self.failUnless(update.to_wire() == goodwire)

    def test_to_wire2(self):
        update = dns.update.Update('example')
        update.id = 1
        update.present('foo')
        update.present('foo', 'a')
        update.present('bar', 'a', '10.0.0.5')
        update.absent('blaz2')
        update.absent('blaz2', 'a')
        update.replace('foo', 300, 'a', '10.0.0.1', '10.0.0.2')
        update.add('bar', 300, dns.rdata.from_text(1, 1, '10.0.0.3'))
        update.delete('bar', 'a', '10.0.0.4')
        update.delete('blaz','a')
        update.delete('blaz2')
        self.failUnless(update.to_wire() == goodwire)

    def test_to_wire3(self):
        update = dns.update.Update('example')
        update.id = 1
        update.present('foo')
        update.present('foo', 'a')
        update.present('bar', 'a', '10.0.0.5')
        update.absent('blaz2')
        update.absent('blaz2', 'a')
        update.replace('foo', 300, 'a', '10.0.0.1', '10.0.0.2')
        update.add('bar', dns.rdataset.from_text(1, 1, 300, '10.0.0.3'))
        update.delete('bar', 'a', '10.0.0.4')
        update.delete('blaz','a')
        update.delete('blaz2')
        self.failUnless(update.to_wire() == goodwire)

    def test_from_text1(self):
        update = dns.message.from_text(update_text)
        w = update.to_wire(origin=dns.name.from_text('example'),
                           want_shuffle=False)
        self.failUnless(w == goodwire)

if __name__ == '__main__':
    unittest.main()
