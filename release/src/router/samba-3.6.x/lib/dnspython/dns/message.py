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

"""DNS Messages"""

import cStringIO
import random
import struct
import sys
import time

import dns.edns
import dns.exception
import dns.flags
import dns.name
import dns.opcode
import dns.entropy
import dns.rcode
import dns.rdata
import dns.rdataclass
import dns.rdatatype
import dns.rrset
import dns.renderer
import dns.tsig

class ShortHeader(dns.exception.FormError):
    """Raised if the DNS packet passed to from_wire() is too short."""
    pass

class TrailingJunk(dns.exception.FormError):
    """Raised if the DNS packet passed to from_wire() has extra junk
    at the end of it."""
    pass

class UnknownHeaderField(dns.exception.DNSException):
    """Raised if a header field name is not recognized when converting from
    text into a message."""
    pass

class BadEDNS(dns.exception.FormError):
    """Raised if an OPT record occurs somewhere other than the start of
    the additional data section."""
    pass

class BadTSIG(dns.exception.FormError):
    """Raised if a TSIG record occurs somewhere other than the end of
    the additional data section."""
    pass

class UnknownTSIGKey(dns.exception.DNSException):
    """Raised if we got a TSIG but don't know the key."""
    pass

class Message(object):
    """A DNS message.

    @ivar id: The query id; the default is a randomly chosen id.
    @type id: int
    @ivar flags: The DNS flags of the message.  @see: RFC 1035 for an
    explanation of these flags.
    @type flags: int
    @ivar question: The question section.
    @type question: list of dns.rrset.RRset objects
    @ivar answer: The answer section.
    @type answer: list of dns.rrset.RRset objects
    @ivar authority: The authority section.
    @type authority: list of dns.rrset.RRset objects
    @ivar additional: The additional data section.
    @type additional: list of dns.rrset.RRset objects
    @ivar edns: The EDNS level to use.  The default is -1, no Edns.
    @type edns: int
    @ivar ednsflags: The EDNS flags
    @type ednsflags: long
    @ivar payload: The EDNS payload size.  The default is 0.
    @type payload: int
    @ivar options: The EDNS options
    @type options: list of dns.edns.Option objects
    @ivar request_payload: The associated request's EDNS payload size.
    @type request_payload: int
    @ivar keyring: The TSIG keyring to use.  The default is None.
    @type keyring: dict
    @ivar keyname: The TSIG keyname to use.  The default is None.
    @type keyname: dns.name.Name object
    @ivar keyalgorithm: The TSIG algorithm to use; defaults to
    dns.tsig.default_algorithm.  Constants for TSIG algorithms are defined
    in dns.tsig, and the currently implemented algorithms are
    HMAC_MD5, HMAC_SHA1, HMAC_SHA224, HMAC_SHA256, HMAC_SHA384, and
    HMAC_SHA512.
    @type keyalgorithm: string
    @ivar request_mac: The TSIG MAC of the request message associated with
    this message; used when validating TSIG signatures.   @see: RFC 2845 for
    more information on TSIG fields.
    @type request_mac: string
    @ivar fudge: TSIG time fudge; default is 300 seconds.
    @type fudge: int
    @ivar original_id: TSIG original id; defaults to the message's id
    @type original_id: int
    @ivar tsig_error: TSIG error code; default is 0.
    @type tsig_error: int
    @ivar other_data: TSIG other data.
    @type other_data: string
    @ivar mac: The TSIG MAC for this message.
    @type mac: string
    @ivar xfr: Is the message being used to contain the results of a DNS
    zone transfer?  The default is False.
    @type xfr: bool
    @ivar origin: The origin of the zone in messages which are used for
    zone transfers or for DNS dynamic updates.  The default is None.
    @type origin: dns.name.Name object
    @ivar tsig_ctx: The TSIG signature context associated with this
    message.  The default is None.
    @type tsig_ctx: hmac.HMAC object
    @ivar had_tsig: Did the message decoded from wire format have a TSIG
    signature?
    @type had_tsig: bool
    @ivar multi: Is this message part of a multi-message sequence?  The
    default is false.  This variable is used when validating TSIG signatures
    on messages which are part of a zone transfer.
    @type multi: bool
    @ivar first: Is this message standalone, or the first of a multi
    message sequence?  This variable is used when validating TSIG signatures
    on messages which are part of a zone transfer.
    @type first: bool
    @ivar index: An index of rrsets in the message.  The index key is
    (section, name, rdclass, rdtype, covers, deleting).  Indexing can be
    disabled by setting the index to None.
    @type index: dict
    """

    def __init__(self, id=None):
        if id is None:
            self.id = dns.entropy.random_16()
        else:
            self.id = id
        self.flags = 0
        self.question = []
        self.answer = []
        self.authority = []
        self.additional = []
        self.edns = -1
        self.ednsflags = 0
        self.payload = 0
        self.options = []
        self.request_payload = 0
        self.keyring = None
        self.keyname = None
        self.keyalgorithm = dns.tsig.default_algorithm
        self.request_mac = ''
        self.other_data = ''
        self.tsig_error = 0
        self.fudge = 300
        self.original_id = self.id
        self.mac = ''
        self.xfr = False
        self.origin = None
        self.tsig_ctx = None
        self.had_tsig = False
        self.multi = False
        self.first = True
        self.index = {}

    def __repr__(self):
        return '<DNS message, ID ' + `self.id` + '>'

    def __str__(self):
        return self.to_text()

    def to_text(self,  origin=None, relativize=True, **kw):
        """Convert the message to text.

        The I{origin}, I{relativize}, and any other keyword
        arguments are passed to the rrset to_wire() method.

        @rtype: string
        """

        s = cStringIO.StringIO()
        print >> s, 'id %d' % self.id
        print >> s, 'opcode %s' % \
              dns.opcode.to_text(dns.opcode.from_flags(self.flags))
        rc = dns.rcode.from_flags(self.flags, self.ednsflags)
        print >> s, 'rcode %s' % dns.rcode.to_text(rc)
        print >> s, 'flags %s' % dns.flags.to_text(self.flags)
        if self.edns >= 0:
            print >> s, 'edns %s' % self.edns
            if self.ednsflags != 0:
                print >> s, 'eflags %s' % \
                      dns.flags.edns_to_text(self.ednsflags)
            print >> s, 'payload', self.payload
        is_update = dns.opcode.is_update(self.flags)
        if is_update:
            print >> s, ';ZONE'
        else:
            print >> s, ';QUESTION'
        for rrset in self.question:
            print >> s, rrset.to_text(origin, relativize, **kw)
        if is_update:
            print >> s, ';PREREQ'
        else:
            print >> s, ';ANSWER'
        for rrset in self.answer:
            print >> s, rrset.to_text(origin, relativize, **kw)
        if is_update:
            print >> s, ';UPDATE'
        else:
            print >> s, ';AUTHORITY'
        for rrset in self.authority:
            print >> s, rrset.to_text(origin, relativize, **kw)
        print >> s, ';ADDITIONAL'
        for rrset in self.additional:
            print >> s, rrset.to_text(origin, relativize, **kw)
        #
        # We strip off the final \n so the caller can print the result without
        # doing weird things to get around eccentricities in Python print
        # formatting
        #
        return s.getvalue()[:-1]

    def __eq__(self, other):
        """Two messages are equal if they have the same content in the
        header, question, answer, and authority sections.
        @rtype: bool"""
        if not isinstance(other, Message):
            return False
        if self.id != other.id:
            return False
        if self.flags != other.flags:
            return False
        for n in self.question:
            if n not in other.question:
                return False
        for n in other.question:
            if n not in self.question:
                return False
        for n in self.answer:
            if n not in other.answer:
                return False
        for n in other.answer:
            if n not in self.answer:
                return False
        for n in self.authority:
            if n not in other.authority:
                return False
        for n in other.authority:
            if n not in self.authority:
                return False
        return True

    def __ne__(self, other):
        """Are two messages not equal?
        @rtype: bool"""
        return not self.__eq__(other)

    def is_response(self, other):
        """Is other a response to self?
        @rtype: bool"""
        if other.flags & dns.flags.QR == 0 or \
           self.id != other.id or \
           dns.opcode.from_flags(self.flags) != \
           dns.opcode.from_flags(other.flags):
            return False
        if dns.rcode.from_flags(other.flags, other.ednsflags) != \
               dns.rcode.NOERROR:
            return True
        if dns.opcode.is_update(self.flags):
            return True
        for n in self.question:
            if n not in other.question:
                return False
        for n in other.question:
            if n not in self.question:
                return False
        return True

    def section_number(self, section):
        if section is self.question:
            return 0
        elif section is self.answer:
            return 1
        elif section is self.authority:
            return 2
        elif section is self.additional:
            return 3
        else:
            raise ValueError('unknown section')

    def find_rrset(self, section, name, rdclass, rdtype,
                   covers=dns.rdatatype.NONE, deleting=None, create=False,
                   force_unique=False):
        """Find the RRset with the given attributes in the specified section.

        @param section: the section of the message to look in, e.g.
        self.answer.
        @type section: list of dns.rrset.RRset objects
        @param name: the name of the RRset
        @type name: dns.name.Name object
        @param rdclass: the class of the RRset
        @type rdclass: int
        @param rdtype: the type of the RRset
        @type rdtype: int
        @param covers: the covers value of the RRset
        @type covers: int
        @param deleting: the deleting value of the RRset
        @type deleting: int
        @param create: If True, create the RRset if it is not found.
        The created RRset is appended to I{section}.
        @type create: bool
        @param force_unique: If True and create is also True, create a
        new RRset regardless of whether a matching RRset exists already.
        @type force_unique: bool
        @raises KeyError: the RRset was not found and create was False
        @rtype: dns.rrset.RRset object"""

        key = (self.section_number(section),
               name, rdclass, rdtype, covers, deleting)
        if not force_unique:
            if not self.index is None:
                rrset = self.index.get(key)
                if not rrset is None:
                    return rrset
            else:
                for rrset in section:
                    if rrset.match(name, rdclass, rdtype, covers, deleting):
                        return rrset
        if not create:
            raise KeyError
        rrset = dns.rrset.RRset(name, rdclass, rdtype, covers, deleting)
        section.append(rrset)
        if not self.index is None:
            self.index[key] = rrset
        return rrset

    def get_rrset(self, section, name, rdclass, rdtype,
                  covers=dns.rdatatype.NONE, deleting=None, create=False,
                  force_unique=False):
        """Get the RRset with the given attributes in the specified section.

        If the RRset is not found, None is returned.

        @param section: the section of the message to look in, e.g.
        self.answer.
        @type section: list of dns.rrset.RRset objects
        @param name: the name of the RRset
        @type name: dns.name.Name object
        @param rdclass: the class of the RRset
        @type rdclass: int
        @param rdtype: the type of the RRset
        @type rdtype: int
        @param covers: the covers value of the RRset
        @type covers: int
        @param deleting: the deleting value of the RRset
        @type deleting: int
        @param create: If True, create the RRset if it is not found.
        The created RRset is appended to I{section}.
        @type create: bool
        @param force_unique: If True and create is also True, create a
        new RRset regardless of whether a matching RRset exists already.
        @type force_unique: bool
        @rtype: dns.rrset.RRset object or None"""

        try:
            rrset = self.find_rrset(section, name, rdclass, rdtype, covers,
                                    deleting, create, force_unique)
        except KeyError:
            rrset = None
        return rrset

    def to_wire(self, origin=None, max_size=0, **kw):
        """Return a string containing the message in DNS compressed wire
        format.

        Additional keyword arguments are passed to the rrset to_wire()
        method.

        @param origin: The origin to be appended to any relative names.
        @type origin: dns.name.Name object
        @param max_size: The maximum size of the wire format output; default
        is 0, which means 'the message's request payload, if nonzero, or
        65536'.
        @type max_size: int
        @raises dns.exception.TooBig: max_size was exceeded
        @rtype: string
        """

        if max_size == 0:
            if self.request_payload != 0:
                max_size = self.request_payload
            else:
                max_size = 65535
        if max_size < 512:
            max_size = 512
        elif max_size > 65535:
            max_size = 65535
        r = dns.renderer.Renderer(self.id, self.flags, max_size, origin)
        for rrset in self.question:
            r.add_question(rrset.name, rrset.rdtype, rrset.rdclass)
        for rrset in self.answer:
            r.add_rrset(dns.renderer.ANSWER, rrset, **kw)
        for rrset in self.authority:
            r.add_rrset(dns.renderer.AUTHORITY, rrset, **kw)
        if self.edns >= 0:
            r.add_edns(self.edns, self.ednsflags, self.payload, self.options)
        for rrset in self.additional:
            r.add_rrset(dns.renderer.ADDITIONAL, rrset, **kw)
        r.write_header()
        if not self.keyname is None:
            r.add_tsig(self.keyname, self.keyring[self.keyname],
                       self.fudge, self.original_id, self.tsig_error,
                       self.other_data, self.request_mac,
                       self.keyalgorithm)
            self.mac = r.mac
        return r.get_wire()

    def use_tsig(self, keyring, keyname=None, fudge=300,
                 original_id=None, tsig_error=0, other_data='',
                 algorithm=dns.tsig.default_algorithm):
        """When sending, a TSIG signature using the specified keyring
        and keyname should be added.

        @param keyring: The TSIG keyring to use; defaults to None.
        @type keyring: dict
        @param keyname: The name of the TSIG key to use; defaults to None.
        The key must be defined in the keyring.  If a keyring is specified
        but a keyname is not, then the key used will be the first key in the
        keyring.  Note that the order of keys in a dictionary is not defined,
        so applications should supply a keyname when a keyring is used, unless
        they know the keyring contains only one key.
        @type keyname: dns.name.Name or string
        @param fudge: TSIG time fudge; default is 300 seconds.
        @type fudge: int
        @param original_id: TSIG original id; defaults to the message's id
        @type original_id: int
        @param tsig_error: TSIG error code; default is 0.
        @type tsig_error: int
        @param other_data: TSIG other data.
        @type other_data: string
        @param algorithm: The TSIG algorithm to use; defaults to
        dns.tsig.default_algorithm
        """

        self.keyring = keyring
        if keyname is None:
            self.keyname = self.keyring.keys()[0]
        else:
            if isinstance(keyname, (str, unicode)):
                keyname = dns.name.from_text(keyname)
            self.keyname = keyname
        self.keyalgorithm = algorithm
        self.fudge = fudge
        if original_id is None:
            self.original_id = self.id
        else:
            self.original_id = original_id
        self.tsig_error = tsig_error
        self.other_data = other_data

    def use_edns(self, edns=0, ednsflags=0, payload=1280, request_payload=None, options=None):
        """Configure EDNS behavior.
        @param edns: The EDNS level to use.  Specifying None, False, or -1
        means 'do not use EDNS', and in this case the other parameters are
        ignored.  Specifying True is equivalent to specifying 0, i.e. 'use
        EDNS0'.
        @type edns: int or bool or None
        @param ednsflags: EDNS flag values.
        @type ednsflags: int
        @param payload: The EDNS sender's payload field, which is the maximum
        size of UDP datagram the sender can handle.
        @type payload: int
        @param request_payload: The EDNS payload size to use when sending
        this message.  If not specified, defaults to the value of payload.
        @type request_payload: int or None
        @param options: The EDNS options
        @type options: None or list of dns.edns.Option objects
        @see: RFC 2671
        """
        if edns is None or edns is False:
            edns = -1
        if edns is True:
            edns = 0
        if request_payload is None:
            request_payload = payload
        if edns < 0:
            ednsflags = 0
            payload = 0
            request_payload = 0
            options = []
        else:
            # make sure the EDNS version in ednsflags agrees with edns
            ednsflags &= 0xFF00FFFFL
            ednsflags |= (edns << 16)
            if options is None:
                options = []
        self.edns = edns
        self.ednsflags = ednsflags
        self.payload = payload
        self.options = options
        self.request_payload = request_payload

    def want_dnssec(self, wanted=True):
        """Enable or disable 'DNSSEC desired' flag in requests.
        @param wanted: Is DNSSEC desired?  If True, EDNS is enabled if
        required, and then the DO bit is set.  If False, the DO bit is
        cleared if EDNS is enabled.
        @type wanted: bool
        """
        if wanted:
            if self.edns < 0:
                self.use_edns()
            self.ednsflags |= dns.flags.DO
        elif self.edns >= 0:
            self.ednsflags &= ~dns.flags.DO

    def rcode(self):
        """Return the rcode.
        @rtype: int
        """
        return dns.rcode.from_flags(self.flags, self.ednsflags)

    def set_rcode(self, rcode):
        """Set the rcode.
        @param rcode: the rcode
        @type rcode: int
        """
        (value, evalue) = dns.rcode.to_flags(rcode)
        self.flags &= 0xFFF0
        self.flags |= value
        self.ednsflags &= 0x00FFFFFFL
        self.ednsflags |= evalue
        if self.ednsflags != 0 and self.edns < 0:
            self.edns = 0

    def opcode(self):
        """Return the opcode.
        @rtype: int
        """
        return dns.opcode.from_flags(self.flags)

    def set_opcode(self, opcode):
        """Set the opcode.
        @param opcode: the opcode
        @type opcode: int
        """
        self.flags &= 0x87FF
        self.flags |= dns.opcode.to_flags(opcode)

class _WireReader(object):
    """Wire format reader.

    @ivar wire: the wire-format message.
    @type wire: string
    @ivar message: The message object being built
    @type message: dns.message.Message object
    @ivar current: When building a message object from wire format, this
    variable contains the offset from the beginning of wire of the next octet
    to be read.
    @type current: int
    @ivar updating: Is the message a dynamic update?
    @type updating: bool
    @ivar one_rr_per_rrset: Put each RR into its own RRset?
    @type one_rr_per_rrset: bool
    @ivar zone_rdclass: The class of the zone in messages which are
    DNS dynamic updates.
    @type zone_rdclass: int
    """

    def __init__(self, wire, message, question_only=False,
                 one_rr_per_rrset=False):
        self.wire = wire
        self.message = message
        self.current = 0
        self.updating = False
        self.zone_rdclass = dns.rdataclass.IN
        self.question_only = question_only
        self.one_rr_per_rrset = one_rr_per_rrset

    def _get_question(self, qcount):
        """Read the next I{qcount} records from the wire data and add them to
        the question section.
        @param qcount: the number of questions in the message
        @type qcount: int"""

        if self.updating and qcount > 1:
            raise dns.exception.FormError

        for i in xrange(0, qcount):
            (qname, used) = dns.name.from_wire(self.wire, self.current)
            if not self.message.origin is None:
                qname = qname.relativize(self.message.origin)
            self.current = self.current + used
            (rdtype, rdclass) = \
                     struct.unpack('!HH',
                                   self.wire[self.current:self.current + 4])
            self.current = self.current + 4
            self.message.find_rrset(self.message.question, qname,
                                    rdclass, rdtype, create=True,
                                    force_unique=True)
            if self.updating:
                self.zone_rdclass = rdclass

    def _get_section(self, section, count):
        """Read the next I{count} records from the wire data and add them to
        the specified section.
        @param section: the section of the message to which to add records
        @type section: list of dns.rrset.RRset objects
        @param count: the number of records to read
        @type count: int"""

        if self.updating or self.one_rr_per_rrset:
            force_unique = True
        else:
            force_unique = False
        seen_opt = False
        for i in xrange(0, count):
            rr_start = self.current
            (name, used) = dns.name.from_wire(self.wire, self.current)
            absolute_name = name
            if not self.message.origin is None:
                name = name.relativize(self.message.origin)
            self.current = self.current + used
            (rdtype, rdclass, ttl, rdlen) = \
                     struct.unpack('!HHIH',
                                   self.wire[self.current:self.current + 10])
            self.current = self.current + 10
            if rdtype == dns.rdatatype.OPT:
                if not section is self.message.additional or seen_opt:
                    raise BadEDNS
                self.message.payload = rdclass
                self.message.ednsflags = ttl
                self.message.edns = (ttl & 0xff0000) >> 16
                self.message.options = []
                current = self.current
                optslen = rdlen
                while optslen > 0:
                    (otype, olen) = \
                            struct.unpack('!HH',
                                          self.wire[current:current + 4])
                    current = current + 4
                    opt = dns.edns.option_from_wire(otype, self.wire, current, olen)
                    self.message.options.append(opt)
                    current = current + olen
                    optslen = optslen - 4 - olen
                seen_opt = True
            elif rdtype == dns.rdatatype.TSIG:
                if not (section is self.message.additional and
                        i == (count - 1)):
                    raise BadTSIG
                if self.message.keyring is None:
                    raise UnknownTSIGKey('got signed message without keyring')
                secret = self.message.keyring.get(absolute_name)
                if secret is None:
                    raise UnknownTSIGKey("key '%s' unknown" % name)
                self.message.tsig_ctx = \
                                      dns.tsig.validate(self.wire,
                                          absolute_name,
                                          secret,
                                          int(time.time()),
                                          self.message.request_mac,
                                          rr_start,
                                          self.current,
                                          rdlen,
                                          self.message.tsig_ctx,
                                          self.message.multi,
                                          self.message.first)
                self.message.had_tsig = True
            else:
                if ttl < 0:
                    ttl = 0
                if self.updating and \
                   (rdclass == dns.rdataclass.ANY or
                    rdclass == dns.rdataclass.NONE):
                    deleting = rdclass
                    rdclass = self.zone_rdclass
                else:
                    deleting = None
                if deleting == dns.rdataclass.ANY or \
                   (deleting == dns.rdataclass.NONE and \
                    section is self.message.answer):
                    covers = dns.rdatatype.NONE
                    rd = None
                else:
                    rd = dns.rdata.from_wire(rdclass, rdtype, self.wire,
                                             self.current, rdlen,
                                             self.message.origin)
                    covers = rd.covers()
                if self.message.xfr and rdtype == dns.rdatatype.SOA:
                    force_unique = True
                rrset = self.message.find_rrset(section, name,
                                                rdclass, rdtype, covers,
                                                deleting, True, force_unique)
                if not rd is None:
                    rrset.add(rd, ttl)
            self.current = self.current + rdlen

    def read(self):
        """Read a wire format DNS message and build a dns.message.Message
        object."""

        l = len(self.wire)
        if l < 12:
            raise ShortHeader
        (self.message.id, self.message.flags, qcount, ancount,
         aucount, adcount) = struct.unpack('!HHHHHH', self.wire[:12])
        self.current = 12
        if dns.opcode.is_update(self.message.flags):
            self.updating = True
        self._get_question(qcount)
        if self.question_only:
            return
        self._get_section(self.message.answer, ancount)
        self._get_section(self.message.authority, aucount)
        self._get_section(self.message.additional, adcount)
        if self.current != l:
            raise TrailingJunk
        if self.message.multi and self.message.tsig_ctx and \
               not self.message.had_tsig:
            self.message.tsig_ctx.update(self.wire)


def from_wire(wire, keyring=None, request_mac='', xfr=False, origin=None,
              tsig_ctx = None, multi = False, first = True,
              question_only = False, one_rr_per_rrset = False):
    """Convert a DNS wire format message into a message
    object.

    @param keyring: The keyring to use if the message is signed.
    @type keyring: dict
    @param request_mac: If the message is a response to a TSIG-signed request,
    I{request_mac} should be set to the MAC of that request.
    @type request_mac: string
    @param xfr: Is this message part of a zone transfer?
    @type xfr: bool
    @param origin: If the message is part of a zone transfer, I{origin}
    should be the origin name of the zone.
    @type origin: dns.name.Name object
    @param tsig_ctx: The ongoing TSIG context, used when validating zone
    transfers.
    @type tsig_ctx: hmac.HMAC object
    @param multi: Is this message part of a multiple message sequence?
    @type multi: bool
    @param first: Is this message standalone, or the first of a multi
    message sequence?
    @type first: bool
    @param question_only: Read only up to the end of the question section?
    @type question_only: bool
    @param one_rr_per_rrset: Put each RR into its own RRset
    @type one_rr_per_rrset: bool
    @raises ShortHeader: The message is less than 12 octets long.
    @raises TrailingJunk: There were octets in the message past the end
    of the proper DNS message.
    @raises BadEDNS: An OPT record was in the wrong section, or occurred more
    than once.
    @raises BadTSIG: A TSIG record was not the last record of the additional
    data section.
    @rtype: dns.message.Message object"""

    m = Message(id=0)
    m.keyring = keyring
    m.request_mac = request_mac
    m.xfr = xfr
    m.origin = origin
    m.tsig_ctx = tsig_ctx
    m.multi = multi
    m.first = first

    reader = _WireReader(wire, m, question_only, one_rr_per_rrset)
    reader.read()

    return m


class _TextReader(object):
    """Text format reader.

    @ivar tok: the tokenizer
    @type tok: dns.tokenizer.Tokenizer object
    @ivar message: The message object being built
    @type message: dns.message.Message object
    @ivar updating: Is the message a dynamic update?
    @type updating: bool
    @ivar zone_rdclass: The class of the zone in messages which are
    DNS dynamic updates.
    @type zone_rdclass: int
    @ivar last_name: The most recently read name when building a message object
    from text format.
    @type last_name: dns.name.Name object
    """

    def __init__(self, text, message):
        self.message = message
        self.tok = dns.tokenizer.Tokenizer(text)
        self.last_name = None
        self.zone_rdclass = dns.rdataclass.IN
        self.updating = False

    def _header_line(self, section):
        """Process one line from the text format header section."""

        token = self.tok.get()
        what = token.value
        if what == 'id':
            self.message.id = self.tok.get_int()
        elif what == 'flags':
            while True:
                token = self.tok.get()
                if not token.is_identifier():
                    self.tok.unget(token)
                    break
                self.message.flags = self.message.flags | \
                                     dns.flags.from_text(token.value)
            if dns.opcode.is_update(self.message.flags):
                self.updating = True
        elif what == 'edns':
            self.message.edns = self.tok.get_int()
            self.message.ednsflags = self.message.ednsflags | \
                                     (self.message.edns << 16)
        elif what == 'eflags':
            if self.message.edns < 0:
                self.message.edns = 0
            while True:
                token = self.tok.get()
                if not token.is_identifier():
                    self.tok.unget(token)
                    break
                self.message.ednsflags = self.message.ednsflags | \
                              dns.flags.edns_from_text(token.value)
        elif what == 'payload':
            self.message.payload = self.tok.get_int()
            if self.message.edns < 0:
                self.message.edns = 0
        elif what == 'opcode':
            text = self.tok.get_string()
            self.message.flags = self.message.flags | \
                      dns.opcode.to_flags(dns.opcode.from_text(text))
        elif what == 'rcode':
            text = self.tok.get_string()
            self.message.set_rcode(dns.rcode.from_text(text))
        else:
            raise UnknownHeaderField
        self.tok.get_eol()

    def _question_line(self, section):
        """Process one line from the text format question section."""

        token = self.tok.get(want_leading = True)
        if not token.is_whitespace():
            self.last_name = dns.name.from_text(token.value, None)
        name = self.last_name
        token = self.tok.get()
        if not token.is_identifier():
            raise dns.exception.SyntaxError
        # Class
        try:
            rdclass = dns.rdataclass.from_text(token.value)
            token = self.tok.get()
            if not token.is_identifier():
                raise dns.exception.SyntaxError
        except dns.exception.SyntaxError:
            raise dns.exception.SyntaxError
        except:
            rdclass = dns.rdataclass.IN
        # Type
        rdtype = dns.rdatatype.from_text(token.value)
        self.message.find_rrset(self.message.question, name,
                                rdclass, rdtype, create=True,
                                force_unique=True)
        if self.updating:
            self.zone_rdclass = rdclass
        self.tok.get_eol()

    def _rr_line(self, section):
        """Process one line from the text format answer, authority, or
        additional data sections.
        """

        deleting = None
        # Name
        token = self.tok.get(want_leading = True)
        if not token.is_whitespace():
            self.last_name = dns.name.from_text(token.value, None)
        name = self.last_name
        token = self.tok.get()
        if not token.is_identifier():
            raise dns.exception.SyntaxError
        # TTL
        try:
            ttl = int(token.value, 0)
            token = self.tok.get()
            if not token.is_identifier():
                raise dns.exception.SyntaxError
        except dns.exception.SyntaxError:
            raise dns.exception.SyntaxError
        except:
            ttl = 0
        # Class
        try:
            rdclass = dns.rdataclass.from_text(token.value)
            token = self.tok.get()
            if not token.is_identifier():
                raise dns.exception.SyntaxError
            if rdclass == dns.rdataclass.ANY or rdclass == dns.rdataclass.NONE:
                deleting = rdclass
                rdclass = self.zone_rdclass
        except dns.exception.SyntaxError:
            raise dns.exception.SyntaxError
        except:
            rdclass = dns.rdataclass.IN
        # Type
        rdtype = dns.rdatatype.from_text(token.value)
        token = self.tok.get()
        if not token.is_eol_or_eof():
            self.tok.unget(token)
            rd = dns.rdata.from_text(rdclass, rdtype, self.tok, None)
            covers = rd.covers()
        else:
            rd = None
            covers = dns.rdatatype.NONE
        rrset = self.message.find_rrset(section, name,
                                        rdclass, rdtype, covers,
                                        deleting, True, self.updating)
        if not rd is None:
            rrset.add(rd, ttl)

    def read(self):
        """Read a text format DNS message and build a dns.message.Message
        object."""

        line_method = self._header_line
        section = None
        while 1:
            token = self.tok.get(True, True)
            if token.is_eol_or_eof():
                break
            if token.is_comment():
                u = token.value.upper()
                if u == 'HEADER':
                    line_method = self._header_line
                elif u == 'QUESTION' or u == 'ZONE':
                    line_method = self._question_line
                    section = self.message.question
                elif u == 'ANSWER' or u == 'PREREQ':
                    line_method = self._rr_line
                    section = self.message.answer
                elif u == 'AUTHORITY' or u == 'UPDATE':
                    line_method = self._rr_line
                    section = self.message.authority
                elif u == 'ADDITIONAL':
                    line_method = self._rr_line
                    section = self.message.additional
                self.tok.get_eol()
                continue
            self.tok.unget(token)
            line_method(section)


def from_text(text):
    """Convert the text format message into a message object.

    @param text: The text format message.
    @type text: string
    @raises UnknownHeaderField:
    @raises dns.exception.SyntaxError:
    @rtype: dns.message.Message object"""

    # 'text' can also be a file, but we don't publish that fact
    # since it's an implementation detail.  The official file
    # interface is from_file().

    m = Message()

    reader = _TextReader(text, m)
    reader.read()

    return m

def from_file(f):
    """Read the next text format message from the specified file.

    @param f: file or string.  If I{f} is a string, it is treated
    as the name of a file to open.
    @raises UnknownHeaderField:
    @raises dns.exception.SyntaxError:
    @rtype: dns.message.Message object"""

    if sys.hexversion >= 0x02030000:
        # allow Unicode filenames; turn on universal newline support
        str_type = basestring
        opts = 'rU'
    else:
        str_type = str
        opts = 'r'
    if isinstance(f, str_type):
        f = file(f, opts)
        want_close = True
    else:
        want_close = False

    try:
        m = from_text(f)
    finally:
        if want_close:
            f.close()
    return m

def make_query(qname, rdtype, rdclass = dns.rdataclass.IN, use_edns=None,
               want_dnssec=False):
    """Make a query message.

    The query name, type, and class may all be specified either
    as objects of the appropriate type, or as strings.

    The query will have a randomly choosen query id, and its DNS flags
    will be set to dns.flags.RD.

    @param qname: The query name.
    @type qname: dns.name.Name object or string
    @param rdtype: The desired rdata type.
    @type rdtype: int
    @param rdclass: The desired rdata class; the default is class IN.
    @type rdclass: int
    @param use_edns: The EDNS level to use; the default is None (no EDNS).
    See the description of dns.message.Message.use_edns() for the possible
    values for use_edns and their meanings.
    @type use_edns: int or bool or None
    @param want_dnssec: Should the query indicate that DNSSEC is desired?
    @type want_dnssec: bool
    @rtype: dns.message.Message object"""

    if isinstance(qname, (str, unicode)):
        qname = dns.name.from_text(qname)
    if isinstance(rdtype, (str, unicode)):
        rdtype = dns.rdatatype.from_text(rdtype)
    if isinstance(rdclass, (str, unicode)):
        rdclass = dns.rdataclass.from_text(rdclass)
    m = Message()
    m.flags |= dns.flags.RD
    m.find_rrset(m.question, qname, rdclass, rdtype, create=True,
                 force_unique=True)
    m.use_edns(use_edns)
    m.want_dnssec(want_dnssec)
    return m

def make_response(query, recursion_available=False, our_payload=8192):
    """Make a message which is a response for the specified query.
    The message returned is really a response skeleton; it has all
    of the infrastructure required of a response, but none of the
    content.

    The response's question section is a shallow copy of the query's
    question section, so the query's question RRsets should not be
    changed.

    @param query: the query to respond to
    @type query: dns.message.Message object
    @param recursion_available: should RA be set in the response?
    @type recursion_available: bool
    @param our_payload: payload size to advertise in EDNS responses; default
    is 8192.
    @type our_payload: int
    @rtype: dns.message.Message object"""

    if query.flags & dns.flags.QR:
        raise dns.exception.FormError('specified query message is not a query')
    response = dns.message.Message(query.id)
    response.flags = dns.flags.QR | (query.flags & dns.flags.RD)
    if recursion_available:
        response.flags |= dns.flags.RA
    response.set_opcode(query.opcode())
    response.question = list(query.question)
    if query.edns >= 0:
        response.use_edns(0, 0, our_payload, query.payload)
    if not query.keyname is None:
        response.keyname = query.keyname
        response.keyring = query.keyring
        response.request_mac = query.mac
    return response
