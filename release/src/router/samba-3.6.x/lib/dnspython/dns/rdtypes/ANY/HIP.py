# Copyright (C) 2010 Nominum, Inc.
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

import cStringIO
import string
import struct

import dns.exception
import dns.rdata
import dns.rdatatype

class HIP(dns.rdata.Rdata):
    """HIP record

    @ivar hit: the host identity tag
    @type hit: string
    @ivar algorithm: the public key cryptographic algorithm
    @type algorithm: int
    @ivar key: the public key
    @type key: string
    @ivar servers: the rendezvous servers
    @type servers: list of dns.name.Name objects
    @see: RFC 5205"""

    __slots__ = ['hit', 'algorithm', 'key', 'servers']

    def __init__(self, rdclass, rdtype, hit, algorithm, key, servers):
        super(HIP, self).__init__(rdclass, rdtype)
        self.hit = hit
        self.algorithm = algorithm
        self.key = key
        self.servers = servers

    def to_text(self, origin=None, relativize=True, **kw):
        hit = self.hit.encode('hex-codec')
        key = self.key.encode('base64-codec').replace('\n', '')
        text = ''
        servers = []
        for server in self.servers:
            servers.append(str(server.choose_relativity(origin, relativize)))
        if len(servers) > 0:
            text += (' ' + ' '.join(servers))
        return '%u %s %s%s' % (self.algorithm, hit, key, text)

    def from_text(cls, rdclass, rdtype, tok, origin = None, relativize = True):
        algorithm = tok.get_uint8()
        hit = tok.get_string().decode('hex-codec')
        if len(hit) > 255:
            raise dns.exception.SyntaxError("HIT too long")
        key = tok.get_string().decode('base64-codec')
        servers = []
        while 1:
            token = tok.get()
            if token.is_eol_or_eof():
                break
            server = dns.name.from_text(token.value, origin)
            server.choose_relativity(origin, relativize)
            servers.append(server)
        return cls(rdclass, rdtype, hit, algorithm, key, servers)

    from_text = classmethod(from_text)

    def to_wire(self, file, compress = None, origin = None):
        lh = len(self.hit)
        lk = len(self.key)
        file.write(struct.pack("!BBH", lh, self.algorithm, lk))
        file.write(self.hit)
        file.write(self.key)
        for server in self.servers:
            server.to_wire(file, None, origin)

    def from_wire(cls, rdclass, rdtype, wire, current, rdlen, origin = None):
        (lh, algorithm, lk) = struct.unpack('!BBH',
                                            wire[current : current + 4])
        current += 4
        rdlen -= 4
        hit = wire[current : current + lh]
        current += lh
        rdlen -= lh
        key = wire[current : current + lk]
        current += lk
        rdlen -= lk
        servers = []
        while rdlen > 0:
            (server, cused) = dns.name.from_wire(wire[: current + rdlen],
                                                 current)
            current += cused
            rdlen -= cused
            if not origin is None:
                server = server.relativize(origin)
            servers.append(server)
        return cls(rdclass, rdtype, hit, algorithm, key, servers)

    from_wire = classmethod(from_wire)

    def choose_relativity(self, origin = None, relativize = True):
        servers = []
        for server in self.servers:
            server = server.choose_relativity(origin, relativize)
            servers.append(server)
        self.servers = servers

    def _cmp(self, other):
        b1 = cStringIO.StringIO()
        lh = len(self.hit)
        lk = len(self.key)
        b1.write(struct.pack("!BBH", lh, self.algorithm, lk))
        b1.write(self.hit)
        b1.write(self.key)
        b2 = cStringIO.StringIO()
        lh = len(other.hit)
        lk = len(other.key)
        b2.write(struct.pack("!BBH", lh, other.algorithm, lk))
        b2.write(other.hit)
        b2.write(other.key)
        v = cmp(b1.getvalue(), b2.getvalue())
        if v != 0:
            return v
        ls = len(self.servers)
        lo = len(other.servers)
        count = min(ls, lo)
        i = 0
        while i < count:
            v = cmp(self.servers[i], other.servers[i])
            if v != 0:
                return v
            i += 1
        return ls - lo
