/* 
   String handling tests
   Copyright (C) 2001-2007, 2009, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h> /* for the ENOENT definitions in str_errors */
#endif

#include "ne_string.h"
#include "ne_utils.h"

#include "tests.h"

#undef ONCMP
#define ONCMP(a,b) ONV(strcmp(a, b), \
		       ("result was [%s] not [%s]", a, b))

static int simple(void) {
    ne_buffer *s = ne_buffer_create();
    ON(s == NULL);
    ne_buffer_zappend(s, "abcde");
    ONCMP(s->data, "abcde");
    ON(ne_buffer_size(s) != 5);
    ne_buffer_destroy(s);
    return OK;
}

static int buf_concat(void)
{
    ne_buffer *s = ne_buffer_create();
    ON(s == NULL);
    ne_buffer_concat(s, "a", "b", "c", "d", "e", "f", "g", NULL);
    ONCMP(s->data, "abcdefg");
    ON(ne_buffer_size(s) != 7);
    ne_buffer_destroy(s);
    return OK;
}

static int buf_concat2(void)
{
#define RES "alphabetagammadeltaepsilonetatheta"
    ne_buffer *s = ne_buffer_create();
    ON(s == NULL);
    ne_buffer_concat(s, "alpha", "beta", "gamma", "delta", "epsilon", 
		     "eta", "theta", NULL);
    ONCMP(s->data, RES);
    ON(ne_buffer_size(s) != strlen(RES));
    ne_buffer_destroy(s);
    return OK;
}

static int buf_concat3(void)
{
    ne_buffer *s = ne_buffer_create();
    ON(s == NULL);
    ne_buffer_zappend(s, "foobar");
    ne_buffer_concat(s, "norman", NULL);
    ONCMP(s->data, "foobarnorman");
    ON(ne_buffer_size(s) != 12);
    ne_buffer_destroy(s);
    return OK;
}

static int append(void)
{
    ne_buffer *s = ne_buffer_create();
    ON(s == NULL);
    ne_buffer_append(s, "a", 1);
    ne_buffer_append(s, "b", 1);
    ne_buffer_append(s, "c", 1);
    ONCMP(s->data, "abc");
    ON(ne_buffer_size(s) != 3);
    ne_buffer_zappend(s, "hello");
    ONCMP(s->data, "abchello");
    ne_buffer_czappend(s, "world");
    ONCMP(s->data, "abchelloworld");
    ON(ne_buffer_size(s) != 13);
    ne_buffer_destroy(s);
    return OK;
}    

static int grow(void)
{
    ne_buffer *s = ne_buffer_ncreate(2);
    ON(s == NULL);
    ne_buffer_append(s, "a", 1);
    ne_buffer_grow(s, 4);
    ONCMP(s->data, "a");
    ne_buffer_destroy(s);
    return OK;
}

static int alter(void) {
    ne_buffer *s = ne_buffer_create();
    char *d;
    ON(s == NULL);
    ne_buffer_zappend(s, "abcdefg");
    d = s->data;
    ON(d == NULL);
    d[2] = '\0';
    ne_buffer_altered(s);
    ONCMP(s->data, "ab");
    ON(ne_buffer_size(s) != 2);
    ne_buffer_zappend(s, "hijkl");
    ONCMP(s->data, "abhijkl");
    ne_buffer_destroy(s);
    return OK;
}

/* Macros for testing ne_token. */

#define TEST(res) do { \
  char *tok = ne_token(&pnt, ','); \
  ONN(res ": return", tok == NULL); \
  ONN(res ": compare", strcmp(tok, (res))); \
  ONN(res ": modify", pnt == NULL); \
} while (0)

#define LASTTEST(res) do { \
  char *tok = ne_token(&pnt, ','); \
  ONN(res ": last return", tok == NULL); \
  ONN(res ": last compare", strcmp(tok, (res))); \
  ONN(res ": last modify", pnt != NULL); \
} while (0)

#define QTEST(res) do { \
  char *tok = ne_qtoken(&pnt, ',', QUOTES); \
  ONN(res ": return", tok == NULL); \
  ONN(res ": compare", strcmp(tok, (res))); \
  ONN(res ": modify", pnt == NULL); \
} while (0)

#define QLASTTEST(res) do { \
  char *tok = ne_qtoken(&pnt, ',', QUOTES); \
  ONN(res ": last return", tok == NULL); \
  ONN(res ": last compare", strcmp(tok, (res))); \
  ONN(res ": last modify", pnt != NULL); \
} while (0)

static int token1(void)
{
    char *str = ne_strdup("a,b,c,d"), *pnt = str;

    TEST("a"); TEST("b"); TEST("c"); LASTTEST("d");
    
    ne_free(str);
    return OK;
}

static int token2(void)
{
    char *str = ne_strdup("norman,fishing, elsewhere"), *pnt = str;
    
    TEST("norman"); TEST("fishing"); LASTTEST(" elsewhere");

    ne_free(str);
    return OK;
}

static int nulls(void)
{
    char *str = ne_strdup("alpha,,gamma"), *pnt = str;

    TEST("alpha"); TEST(""); LASTTEST("gamma");
    ne_free(str);
    
    pnt = str = ne_strdup(",,,wooo");
    TEST(""); TEST(""); TEST(""); LASTTEST("wooo");
    ne_free(str);

    pnt = str = ne_strdup("wooo,,,");
    TEST("wooo"); TEST(""); TEST(""); LASTTEST("");
    ne_free(str);    

    return OK;
}

static int empty(void)
{
    char *str = ne_strdup(""), *pnt = str;

    LASTTEST("");
    ne_free(str);

    return OK;
}

#undef QUOTES
#define QUOTES "'"

static int quoted(void)
{
    char *str = 
	ne_strdup("alpha,'beta, a fish called HELLO!?',sandwiches");
    char *pnt = str;
    
    QTEST("alpha");
    QTEST("'beta, a fish called HELLO!?'");
    QLASTTEST("sandwiches");
    
    ne_free(str);
    return OK;
}

static int badquotes(void)
{
    char *str = ne_strdup("alpha,'blah"), *pnt = str;
    
    QTEST("alpha");
    ON(ne_qtoken(&pnt, ',', QUOTES) != NULL);
    
    ne_free(str);
    return OK;
}

/* for testing ne_shave. */
#undef TEST
#define TEST(str, ws, res) do { \
  char *s = ne_strdup((str)); \
  char *r = ne_shave(s, (ws)); \
  ONN("[" str "]", strcmp(r, (res))); \
  ne_free(s); \
} while (0)

static int shave(void)
{
    TEST(" b ", " ", "b");
    TEST("b", " ", "b");
    TEST("   b    ", " ", "b");
    TEST("--bbb-----", "-", "bbb");
    TEST("hello, world    ", " ", "hello, world");
    TEST("<<><<<><<this is foo<<><<<<><<", "<>", "this is foo");
    TEST("09809812342347I once saw an helicopter0012312312398", "0123456789",
	 "I once saw an helicopter");
    return OK;
}

/* Regression test for ne_shave call which should produce an empty
 * string. */
static int shave_regress(void)
{
    TEST("\"\"", "\"", "");
    return OK;
}

/* Test the ne_token/ne_shave combination. */

#undef TEST
#undef LASTTEST

#define TEST(res) do { \
  char *tok = ne_token(&pnt, ','); \
  ONN(res ": return", tok == NULL); \
  tok = ne_shave(tok, " "); \
  ONN(res ": shave", tok == NULL); \
  ONN(res ": compare", strcmp(tok, (res))); \
  ONN(res ": modify", pnt == NULL); \
} while (0)


#define LASTTEST(res) do { \
  char *tok = ne_token(&pnt, ','); \
  ONN(res ": last return", tok == NULL); \
  tok = ne_shave(tok, " "); \
  ONN(res ": last shave", tok == NULL); \
  ONN(res ": last compare", strcmp(tok, (res))); \
  ONN(res ": last modify", pnt != NULL); \
} while (0)

/* traditional use of ne_token/ne_shave. */
static int combo(void)
{
    char *str = ne_strdup("  fred , mary, jim , alice, david"), *pnt = str;
    
    TEST("fred"); TEST("mary"); TEST("jim"); TEST("alice");
    LASTTEST("david");
    
    ne_free(str);
    return 0;
}

static int concat(void)
{
#define CAT(res, args) do { char *str = ne_concat args; \
ONCMP(str, res); \
ne_free(str); } while (0)
    CAT("alphabeta", ("alpha", "beta", NULL));
    CAT("alpha", ("alpha", "", "", NULL));
    CAT("", ("", NULL));
    CAT("", ("", "", "", NULL));
    CAT("alpha", ("", "a", "lph", "", "a", NULL));
    return OK;    
}

static int str_errors(void)
{
    char expect[200], actual[200];
    
    strncpy(expect, strerror(ENOENT), sizeof(expect));
    ONN("ne_strerror did not return passed-in buffer",
	ne_strerror(ENOENT, actual, sizeof(actual)) != actual);
    
    ONV(strcmp(expect, actual), 
	("error from ENOENT was `%s' not `%s'", actual, expect));

    /* Test truncated error string is still NUL-terminated. */
    ne_strerror(ENOENT, actual, 6);
    NE_DEBUG(NE_DBG_HTTP, "error: %s\n", actual);
    ONN("truncated string had wrong length", strlen(actual) != 5);

    ne_strerror(-1, actual, 6);
    ONN("truncated string for bad error had wrong length", 
        strlen(actual) != 5);

    return OK;
}

static int strnzcpy(void)
{
    char buf[5];

    ne_strnzcpy(buf, "abcdefghi", sizeof buf);
    ONV(strcmp(buf, "abcd"), ("result was `%s' not `abcd'", buf));
    
    ne_strnzcpy(buf, "ab", sizeof buf);
    ONV(strcmp(buf, "ab"), ("result was `%s' not `ab'", buf));    

    return OK;    
}

#define FOX_STRING "The quick brown fox jumped over the lazy dog"
#define PUNC_STRING "<>,.;'#:@~[]{}!\"$%^&*()_+-="

static int cleaner(void)
{
    static const char *strings[] = {
        "alpha", "alpha",
        "pretty\033[41mcolours", "pretty [41mcolours",
        "beta\n", "beta ",
        "del\rt\na", "del t a",
        FOX_STRING, FOX_STRING,
        "0123456789", "0123456789",
        PUNC_STRING, PUNC_STRING,
        "\01blah blee\05bloo", " blah blee bloo",
        NULL,
    };
    unsigned int n;
    
    for (n = 0; strings[n]; n+=2) {
        char *act = ne_strclean(ne_strdup(strings[n]));
        
        ONV(strcmp(act, strings[n+1]), 
            ("cleansed to `%s' not `%s'", act, strings[n+1]));
        
        ne_free(act);
    }

    return OK;
}

/* Check that raw data 'raw', of length 'len', has base64 encoding
 * of 'expected'. */
static int b64_check(const unsigned char *raw, size_t len,
                     const char *expected)
{
    char *encoded = ne_base64(raw, len);
    unsigned char *decoded;
    size_t dlen;
    
    ONV(strcmp(encoded, expected), 
        ("base64(\"%s\") gave \"%s\" not \"%s\"", raw, encoded, expected));
    
    dlen = ne_unbase64(encoded, &decoded);
    ONV(dlen != len, 
        ("decoded `%s' length was %" NE_FMT_SIZE_T " not %" NE_FMT_SIZE_T,
         expected, dlen, len));

    ONV(memcmp(raw, decoded, dlen), 
        ("decoded `%s' as `%.*s' not `%.*s'",
         expected, (int)dlen, decoded, (int)dlen, raw));

    ne_free(decoded);
    ne_free(encoded);
    return OK;
}

/* ALLBITS: base64 encoding of "\0..\377" */
#define ALLBITS \
"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKiss" \
"LS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZ" \
"WltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWG" \
"h4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKz" \
"tLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g" \
"4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w=="

static int base64(void)
{
    unsigned char bits[256];
    size_t n;

#define B64B(x, l, y) CALL(b64_check((unsigned char *)x, l, y))
#define B64(x, y) B64B(x, strlen(x), y)

    /* invent these with 
     *  $ printf "string" | uuencode -m blah
     */
    B64("a", "YQ==");
    B64("bb", "YmI=");
    B64("ccc", "Y2Nj");
    B64("Hello, world", "SGVsbG8sIHdvcmxk");
    B64("Aladdin:open sesame", "QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
    B64("I once saw a dog called norman.\n", 
	"SSBvbmNlIHNhdyBhIGRvZyBjYWxsZWQgbm9ybWFuLgo=");
    B64("The quick brown fox jumped over the lazy dog", 
	"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c=");

    /* binary data..
     *   $ printf "string" | wc -c # get the length
     *   $ printf "string" | uuencode -m blah # get the base64
     */
    B64B("\0\0\0\0\0\n", 6, "AAAAAAAK");
    B64B("I once wished \0 upon a \0 fish.", 30, 
	 "SSBvbmNlIHdpc2hlZCAAIHVwb24gYSAAIGZpc2gu");
    B64B("\201\202\203\204", 4, "gYKDhA==");

    for (n = 0; n < sizeof bits; n++)
        bits[n] = (unsigned char)n;
    CALL(b64_check(bits, sizeof bits, ALLBITS));

#undef B64
#undef B64B
    return OK;
}

static int unbase64(void)
{
    static const char *ts[] = {
        "", "a", "ab", "abc", 
        "}bcd", "a}cd", "ab}d", "abc}", "    ",
        "^bcd", "a^cd", "ab^d", "abc^",
        "====", "=bcd", "a=cd", "ab=d", "a==d", "a=c=",
        NULL
    };
    size_t n;

    for (n = 0; ts[n]; n++) {
        unsigned char *tmp;
        ONV(ne_unbase64(ts[n], &tmp) != 0,
            ("invalid string `%s' was decoded", ts[n]));
    }

    return OK;
}

static int printing(void)
{
    struct {
        const char *in, *out;
        size_t pass, ret;
    } ts[] = {
        { "alpha", "alpha", 10, 5 },
        { "alpha", "alph", 5, 4 },
        { "foobar", "", 1, 0 },
        { NULL, NULL, 0, 0}
    };
    size_t n;

    for (n = 0; ts[n].in; n++) {
        char buf[512];
        size_t ret;

        memset(buf, 'A', sizeof buf);

        ret = ne_snprintf(buf, ts[n].pass, "%s", ts[n].in);
        
        ONCMP(buf, ts[n].out);
        ONV(ret != ts[n].ret, 
            ("got return value %" NE_FMT_SIZE_T " not %" NE_FMT_SIZE_T,
             ret, ts[n].ret));

        /* byte past the NUL must still be 'A' */
        ONN("buffer over-ran!", buf[ret + 1] != 'A');
    }
    
    return OK;
}

static int casecmp(void)
{
    static const struct {
        const char *left, *right;
        int expect;
    } ts[] = {
        { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0 },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", 0 },
        { "foo", "bar", 1 },
        { "!#:[@\377", "!#:[@\377", 0 },
        { "bar", "foo", -1 },
        { "foop", "foo", 1 },
        { "foo", "foop", -1 },
        { NULL, NULL, 0 }
    };
    size_t n;
    
    for (n = 0; ts[n].left; n++) {
        int actual;

        actual = ne_strcasecmp(ts[n].left, ts[n].right);
        
        ONV(ts[n].expect == 0 && actual != 0,
            ("strcasecmp(%s, %s) gave %d, expected 0",
             ts[n].left, ts[n].right, actual));

        ONV(ts[n].expect > 0 && actual <= 0,
            ("strcasecmp(%s, %s) gave %d, expected > 0",
             ts[n].left, ts[n].right, actual));

        ONV(ts[n].expect < 0 && actual >= 0,
            ("strcasecmp(%s, %s) gave %d, expected < 0",
             ts[n].left, ts[n].right, actual));
    }

    ONN("comparison of identical pointers did not give zero",
        ne_strcasecmp(ts[0].left, ts[0].left) != 0);

    return OK;
}

static int casencmp(void)
{
    static const struct {
        const char *left, *right;
        size_t n;
        int expect;
    } ts[] = {
        { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 30, 0 },
        { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 10, 0 }, 
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", 0, 0 },
        { "foo", "bar", 3, 1 },
        { "bar", "foo", 4, -1 },
        { "bar", "foo", 3, -1 },
        { "foop", "foo", 4, 1 },
        { "foo", "foop", 4, -1 },
        { "bee", "bar", 0, 0},
        { NULL, NULL, 0, 0 }
    };
    size_t n;
    
    for (n = 0; ts[n].left; n++) {
        int actual;

        actual = ne_strncasecmp(ts[n].left, ts[n].right, ts[n].n);
        
        ONV(ts[n].expect == 0 && actual != 0,
            ("strncasecmp(%s, %s, %" NE_FMT_SIZE_T ") gave %d, expected 0",
             ts[n].left, ts[n].right, ts[n].n, actual));

        ONV(ts[n].expect > 0 && actual <= 0,
            ("strncasecmp(%s, %s, %" NE_FMT_SIZE_T ") gave %d, expected > 0",
             ts[n].left, ts[n].right, ts[n].n, actual));

        ONV(ts[n].expect < 0 && actual >= 0,
            ("strncasecmp(%s, %s, %" NE_FMT_SIZE_T ") gave %d, expected < 0",
             ts[n].left, ts[n].right, ts[n].n, actual));
    }

    ONN("comparison of identical pointers did not give zero",
        ne_strncasecmp(ts[0].left, ts[0].left, 5) != 0);

    return OK;
}

static int buf_print(void)
{
    ne_buffer *buf = ne_buffer_create();

    ne_buffer_czappend(buf, "foo-");
    ne_buffer_snprintf(buf, 20, "bar-%s-asda", "norman");
    ne_buffer_czappend(buf, "-bloo");
    ONN("snprintf return value", ne_buffer_snprintf(buf, 2, "---") != 1);
    
    ONCMP(buf->data, "foo-bar-norman-asda-bloo-");

    ne_buffer_destroy(buf);
    
    return OK;
}

static int qappend(void)
{
    static const struct {
        const char *in;
        size_t inlen;
        const char *out;
    } ts[] = {
        { "", 0, "" },
        { "a", 1, "a" },
        { "b", 2, "b\\x00" },
        { "alpha\0alpha", 11, "alpha\\x00alpha" },
        { "a\tb", 3, "a\\x09b" },
        { NULL }
    };
    unsigned n;

    for (n = 0; ts[n].in; n++) {
        ne_buffer *buf = ne_buffer_create();
        char *s;
        const unsigned char *in = (const unsigned char *)ts[n].in;

        ne_buffer_qappend(buf, in, ts[n].inlen);

        ONCMP(buf->data, ts[n].out);

        ONV(strlen(buf->data) + 1 != buf->used,
            ("bad buffer length for '%s': %" NE_FMT_SIZE_T, 
             ts[n].out, buf->used));
        
        s = ne_strnqdup(in, ts[n].inlen);
        
        ONCMP(s, ts[n].out);

        ne_free(s);
        ne_buffer_destroy(buf);
    }

    return OK;
}

ne_test tests[] = {
    T(simple),
    T(buf_concat),
    T(buf_concat2),
    T(buf_concat3),
    T(append),
    T(grow),
    T(alter),
    T(token1),
    T(token2),
    T(nulls),
    T(empty),
    T(quoted),
    T(badquotes),
    T(shave),
    T(shave_regress),
    T(combo),
    T(concat),
    T(str_errors),
    T(strnzcpy),
    T(cleaner),
    T(base64),
    T(unbase64),
    T(printing),
    T(casecmp),
    T(casencmp),
    T(buf_print),
    T(qappend),
    T(NULL)
};

