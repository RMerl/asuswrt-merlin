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

import dns.exception
import dns.tokenizer

Token = dns.tokenizer.Token

class TokenizerTestCase(unittest.TestCase):

    def testQuotedString1(self):
        tok = dns.tokenizer.Tokenizer(r'"foo"')
        token = tok.get()
        self.failUnless(token == Token(dns.tokenizer.QUOTED_STRING, 'foo'))

    def testQuotedString2(self):
        tok = dns.tokenizer.Tokenizer(r'""')
        token = tok.get()
        self.failUnless(token == Token(dns.tokenizer.QUOTED_STRING, ''))

    def testQuotedString3(self):
        tok = dns.tokenizer.Tokenizer(r'"\"foo\""')
        token = tok.get()
        self.failUnless(token == Token(dns.tokenizer.QUOTED_STRING, '"foo"'))

    def testQuotedString4(self):
        tok = dns.tokenizer.Tokenizer(r'"foo\010bar"')
        token = tok.get()
        self.failUnless(token == Token(dns.tokenizer.QUOTED_STRING, 'foo\x0abar'))

    def testQuotedString5(self):
        def bad():
            tok = dns.tokenizer.Tokenizer(r'"foo')
            token = tok.get()
        self.failUnlessRaises(dns.exception.UnexpectedEnd, bad)

    def testQuotedString6(self):
        def bad():
            tok = dns.tokenizer.Tokenizer(r'"foo\01')
            token = tok.get()
        self.failUnlessRaises(dns.exception.SyntaxError, bad)

    def testQuotedString7(self):
        def bad():
            tok = dns.tokenizer.Tokenizer('"foo\nbar"')
            token = tok.get()
        self.failUnlessRaises(dns.exception.SyntaxError, bad)

    def testEmpty1(self):
        tok = dns.tokenizer.Tokenizer('')
        token = tok.get()
        self.failUnless(token.is_eof())

    def testEmpty2(self):
        tok = dns.tokenizer.Tokenizer('')
        token1 = tok.get()
        token2 = tok.get()
        self.failUnless(token1.is_eof() and token2.is_eof())

    def testEOL(self):
        tok = dns.tokenizer.Tokenizer('\n')
        token1 = tok.get()
        token2 = tok.get()
        self.failUnless(token1.is_eol() and token2.is_eof())

    def testWS1(self):
        tok = dns.tokenizer.Tokenizer(' \n')
        token1 = tok.get()
        self.failUnless(token1.is_eol())

    def testWS2(self):
        tok = dns.tokenizer.Tokenizer(' \n')
        token1 = tok.get(want_leading=True)
        self.failUnless(token1.is_whitespace())

    def testComment1(self):
        tok = dns.tokenizer.Tokenizer(' ;foo\n')
        token1 = tok.get()
        self.failUnless(token1.is_eol())

    def testComment2(self):
        tok = dns.tokenizer.Tokenizer(' ;foo\n')
        token1 = tok.get(want_comment = True)
        token2 = tok.get()
        self.failUnless(token1 == Token(dns.tokenizer.COMMENT, 'foo') and
                        token2.is_eol())

    def testComment3(self):
        tok = dns.tokenizer.Tokenizer(' ;foo bar\n')
        token1 = tok.get(want_comment = True)
        token2 = tok.get()
        self.failUnless(token1 == Token(dns.tokenizer.COMMENT, 'foo bar') and
                        token2.is_eol())

    def testMultiline1(self):
        tok = dns.tokenizer.Tokenizer('( foo\n\n bar\n)')
        tokens = list(iter(tok))
        self.failUnless(tokens == [Token(dns.tokenizer.IDENTIFIER, 'foo'),
                                   Token(dns.tokenizer.IDENTIFIER, 'bar')])

    def testMultiline2(self):
        tok = dns.tokenizer.Tokenizer('( foo\n\n bar\n)\n')
        tokens = list(iter(tok))
        self.failUnless(tokens == [Token(dns.tokenizer.IDENTIFIER, 'foo'),
                                   Token(dns.tokenizer.IDENTIFIER, 'bar'),
                                   Token(dns.tokenizer.EOL, '\n')])
    def testMultiline3(self):
        def bad():
            tok = dns.tokenizer.Tokenizer('foo)')
            tokens = list(iter(tok))
        self.failUnlessRaises(dns.exception.SyntaxError, bad)

    def testMultiline4(self):
        def bad():
            tok = dns.tokenizer.Tokenizer('((foo)')
            tokens = list(iter(tok))
        self.failUnlessRaises(dns.exception.SyntaxError, bad)

    def testUnget1(self):
        tok = dns.tokenizer.Tokenizer('foo')
        t1 = tok.get()
        tok.unget(t1)
        t2 = tok.get()
        self.failUnless(t1 == t2 and t1.ttype == dns.tokenizer.IDENTIFIER and \
                        t1.value == 'foo')

    def testUnget2(self):
        def bad():
            tok = dns.tokenizer.Tokenizer('foo')
            t1 = tok.get()
            tok.unget(t1)
            tok.unget(t1)
        self.failUnlessRaises(dns.tokenizer.UngetBufferFull, bad)

    def testGetEOL1(self):
        tok = dns.tokenizer.Tokenizer('\n')
        t = tok.get_eol()
        self.failUnless(t == '\n')

    def testGetEOL2(self):
        tok = dns.tokenizer.Tokenizer('')
        t = tok.get_eol()
        self.failUnless(t == '')

    def testEscapedDelimiter1(self):
        tok = dns.tokenizer.Tokenizer(r'ch\ ld')
        t = tok.get()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == r'ch\ ld')

    def testEscapedDelimiter2(self):
        tok = dns.tokenizer.Tokenizer(r'ch\032ld')
        t = tok.get()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == r'ch\032ld')

    def testEscapedDelimiter3(self):
        tok = dns.tokenizer.Tokenizer(r'ch\ild')
        t = tok.get()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == r'ch\ild')

    def testEscapedDelimiter1u(self):
        tok = dns.tokenizer.Tokenizer(r'ch\ ld')
        t = tok.get().unescape()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == r'ch ld')

    def testEscapedDelimiter2u(self):
        tok = dns.tokenizer.Tokenizer(r'ch\032ld')
        t = tok.get().unescape()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == 'ch ld')

    def testEscapedDelimiter3u(self):
        tok = dns.tokenizer.Tokenizer(r'ch\ild')
        t = tok.get().unescape()
        self.failUnless(t.ttype == dns.tokenizer.IDENTIFIER and t.value == r'child')

if __name__ == '__main__':
    unittest.main()
