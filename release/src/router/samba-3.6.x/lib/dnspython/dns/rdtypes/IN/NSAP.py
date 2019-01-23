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

import dns.exception
import dns.rdata
import dns.tokenizer

class NSAP(dns.rdata.Rdata):
    """NSAP record.

    @ivar address: a NASP
    @type address: string
    @see: RFC 1706"""

    __slots__ = ['address']

    def __init__(self, rdclass, rdtype, address):
        super(NSAP, self).__init__(rdclass, rdtype)
        self.address = address

    def to_text(self, origin=None, relativize=True, **kw):
        return "0x%s" % self.address.encode('hex_codec')

    def from_text(cls, rdclass, rdtype, tok, origin = None, relativize = True):
        address = tok.get_string()
        t = tok.get_eol()
        if address[0:2] != '0x':
            raise dns.exception.SyntaxError('string does not start with 0x')
        address = address[2:].replace('.', '')
        if len(address) % 2 != 0:
            raise dns.exception.SyntaxError('hexstring has odd length')
        address = address.decode('hex_codec')
        return cls(rdclass, rdtype, address)

    from_text = classmethod(from_text)

    def to_wire(self, file, compress = None, origin = None):
        file.write(self.address)

    def from_wire(cls, rdclass, rdtype, wire, current, rdlen, origin = None):
        address = wire[current : current + rdlen]
        return cls(rdclass, rdtype, address)

    from_wire = classmethod(from_wire)

    def _cmp(self, other):
        return cmp(self.address, other.address)
