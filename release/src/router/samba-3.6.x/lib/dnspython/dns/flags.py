# Copyright (C) 2001-2007, 2009, 2010 Nominum, Inc.
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

"""DNS Message Flags."""

# Standard DNS flags

QR = 0x8000
AA = 0x0400
TC = 0x0200
RD = 0x0100
RA = 0x0080
AD = 0x0020
CD = 0x0010

# EDNS flags

DO = 0x8000

_by_text = {
    'QR' : QR,
    'AA' : AA,
    'TC' : TC,
    'RD' : RD,
    'RA' : RA,
    'AD' : AD,
    'CD' : CD
}

_edns_by_text = {
    'DO' : DO
}


# We construct the inverse mappings programmatically to ensure that we
# cannot make any mistakes (e.g. omissions, cut-and-paste errors) that
# would cause the mappings not to be true inverses.

_by_value = dict([(y, x) for x, y in _by_text.iteritems()])

_edns_by_value = dict([(y, x) for x, y in _edns_by_text.iteritems()])

def _order_flags(table):
    order = list(table.iteritems())
    order.sort()
    order.reverse()
    return order

_flags_order = _order_flags(_by_value)

_edns_flags_order = _order_flags(_edns_by_value)

def _from_text(text, table):
    flags = 0
    tokens = text.split()
    for t in tokens:
        flags = flags | table[t.upper()]
    return flags

def _to_text(flags, table, order):
    text_flags = []
    for k, v in order:
        if flags & k != 0:
            text_flags.append(v)
    return ' '.join(text_flags)

def from_text(text):
    """Convert a space-separated list of flag text values into a flags
    value.
    @rtype: int"""

    return _from_text(text, _by_text)

def to_text(flags):
    """Convert a flags value into a space-separated list of flag text
    values.
    @rtype: string"""

    return _to_text(flags, _by_value, _flags_order)
    

def edns_from_text(text):
    """Convert a space-separated list of EDNS flag text values into a EDNS
    flags value.
    @rtype: int"""

    return _from_text(text, _edns_by_text)

def edns_to_text(flags):
    """Convert an EDNS flags value into a space-separated list of EDNS flag
    text values.
    @rtype: string"""

    return _to_text(flags, _edns_by_value, _edns_flags_order)
