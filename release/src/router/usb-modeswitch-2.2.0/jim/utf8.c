/**
 * UTF-8 utility functions
 *
 * (c) 2010 Steve Bennett <steveb@workware.net.au>
 *
 * See LICENCE for licence details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "utf8.h"

/* This one is always implemented */
int utf8_fromunicode(char *p, unsigned short uc)
{
    if (uc <= 0x7f) {
        *p = uc;
        return 1;
    }
    else if (uc <= 0x7ff) {
        *p++ = 0xc0 | ((uc & 0x7c0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 2;
    }
    else {
        *p++ = 0xe0 | ((uc & 0xf000) >> 12);
        *p++ = 0x80 | ((uc & 0xfc0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 3;
    }
}

#if defined(JIM_UTF8) && !defined(JIM_BOOTSTRAP)
int utf8_charlen(int c)
{
    if ((c & 0x80) == 0) {
        return 1;
    }
    if ((c & 0xe0) == 0xc0) {
        return 2;
    }
    if ((c & 0xf0) == 0xe0) {
        return 3;
    }
    if ((c & 0xf8) == 0xf0) {
        return 4;
    }
    /* Invalid sequence */
    return -1;
}

int utf8_strlen(const char *str, int bytelen)
{
    int charlen = 0;
    if (bytelen < 0) {
        bytelen = strlen(str);
    }
    while (bytelen) {
        int c;
        int l = utf8_tounicode(str, &c);
        charlen++;
        str += l;
        bytelen -= l;
    }
    return charlen;
}

int utf8_index(const char *str, int index)
{
    const char *s = str;
    while (index--) {
        int c;
        s += utf8_tounicode(s, &c);
    }
    return s - str;
}

int utf8_charequal(const char *s1, const char *s2)
{
    int c1, c2;

    utf8_tounicode(s1, &c1);
    utf8_tounicode(s2, &c2);

    return c1 == c2;
}

int utf8_prev_len(const char *str, int len)
{
    int n = 1;

    assert(len > 0);

    /* Look up to len chars backward for a start-of-char byte */
    while (--len) {
        if ((str[-n] & 0x80) == 0) {
            /* Start of a 1-byte char */
            break;
        }
        if ((str[-n] & 0xc0) == 0xc0) {
            /* Start of a multi-byte char */
            break;
        }
        n++;
    }
    return n;
}

int utf8_tounicode(const char *str, int *uc)
{
    unsigned const char *s = (unsigned const char *)str;

    if (s[0] < 0xc0) {
        *uc = s[0];
        return 1;
    }
    if (s[0] < 0xe0) {
        if ((s[1] & 0xc0) == 0x80) {
            *uc = ((s[0] & ~0xc0) << 6) | (s[1] & ~0x80);
            return 2;
        }
    }
    else if (s[0] < 0xf0) {
        if (((str[1] & 0xc0) == 0x80) && ((str[2] & 0xc0) == 0x80)) {
            *uc = ((s[0] & ~0xe0) << 12) | ((s[1] & ~0x80) << 6) | (s[2] & ~0x80);
            return 3;
        }
    }

    /* Invalid sequence, so just return the byte */
    *uc = *s;
    return 1;
}

struct casemap {
    unsigned short code;    /* code point */
    signed char lowerdelta; /* add for lowercase, or if -128 use the ext table */
    signed char upperdelta; /* add for uppercase, or offset into the ext table */
};

/* Extended table for codepoints where |delta| > 127 */
struct caseextmap {
    unsigned short lower;
    unsigned short upper;
};

/* Generated mapping tables */
#include "_unicode_mapping.c"

#define NUMCASEMAP sizeof(unicode_case_mapping) / sizeof(*unicode_case_mapping)

static int cmp_casemap(const void *key, const void *cm)
{
    return *(int *)key - (int)((const struct casemap *)cm)->code;
}

static int utf8_map_case(int uc, int upper)
{
    const struct casemap *cm = bsearch(&uc, unicode_case_mapping, NUMCASEMAP, sizeof(*unicode_case_mapping), cmp_casemap);

    if (cm) {
        if (cm->lowerdelta == -128) {
            uc = upper ? unicode_extmap[cm->upperdelta].upper : unicode_extmap[cm->upperdelta].lower;
        }
        else {
            uc += upper ? cm->upperdelta : cm->lowerdelta;
        }
    }
    return uc;
}

int utf8_upper(int uc)
{
    if (isascii(uc)) {
        return toupper(uc);
    }
    return utf8_map_case(uc, 1);
}

int utf8_lower(int uc)
{
    if (isascii(uc)) {
        return tolower(uc);
    }

    return utf8_map_case(uc, 0);
}

#endif /* JIM_BOOTSTRAP */
