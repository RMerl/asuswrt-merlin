# Copyright (C) 2006, 2007, 2009, 2010 Nominum, Inc.
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

"""TXT-like base class."""

import dns.exception
import dns.rdata
import dns.tokenizer

class TXTBase(dns.rdata.Rdata):
    """Base class for rdata that is like a TXT record

    @ivar strings: the text strings
    @type strings: list of string
    @see: RFC 1035"""

    __slots__ = ['strings']

    def __init__(self, rdclass, rdtype, strings):
        super(TXTBase, self).__init__(rdclass, rdtype)
        if isinstance(strings, str):
            strings = [ strings ]
        self.strings = strings[:]

    def to_text(self, origin=None, relativize=True, **kw):
        txt = ''
        prefix = ''
        for s in self.strings:
            txt += '%s"%s"' % (prefix, dns.rdata._escapify(s))
            prefix = ' '
        return txt

    def from_text(cls, rdclass, rdtype, tok, origin = None, relativize = True):
        strings = []
        while 1:
            token = tok.get().unescape()
            if token.is_eol_or_eof():
                break
            if not (token.is_quoted_string() or token.is_identifier()):
                raise dns.exception.SyntaxError("expected a string")
            if len(token.value) > 255:
                raise dns.exception.SyntaxError("string too long")
            strings.append(token.value)
        if len(strings) == 0:
            raise dns.exception.UnexpectedEnd
        return cls(rdclass, rdtype, strings)

    from_text = classmethod(from_text)

    def to_wire(self, file, compress = None, origin = None):
        for s in self.strings:
            l = len(s)
            assert l < 256
            byte = chr(l)
            file.write(byte)
            file.write(s)

    def from_wire(cls, rdclass, rdtype, wire, current, rdlen, origin = None):
        strings = []
        while rdlen > 0:
            l = ord(wire[current])
            current += 1
            rdlen -= 1
            if l > rdlen:
                raise dns.exception.FormError
            s = wire[current : current + l]
            current += l
            rdlen -= l
            strings.append(s)
        return cls(rdclass, rdtype, strings)

    from_wire = classmethod(from_wire)

    def _cmp(self, other):
        return cmp(self.strings, other.strings)
