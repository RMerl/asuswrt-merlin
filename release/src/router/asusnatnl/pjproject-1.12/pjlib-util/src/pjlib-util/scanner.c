/* $Id: scanner.c 4375 2013-02-27 09:28:31Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjlib-util/scanner.h>
#include <pj/ctype.h>
#include <pj/string.h>
#include <pj/except.h>
#include <pj/os.h>
#include <pj/errno.h>
#include <pj/assert.h>

#define PJ_SCAN_IS_SPACE(c)		((c)==' ' || (c)=='\t')
#define PJ_SCAN_IS_NEWLINE(c)		((c)=='\r' || (c)=='\n')
#define PJ_SCAN_IS_PROBABLY_SPACE(c)	((c) <= 32)
#define PJ_SCAN_CHECK_EOF(s)		(s != scanner->end)


#if defined(PJ_SCANNER_USE_BITWISE) && PJ_SCANNER_USE_BITWISE != 0
#  include "scanner_cis_bitwise.c"
#else
#  include "scanner_cis_uint.c"
#endif


static void pj_scan_syntax_err(pj_scanner *scanner)
{
    (*scanner->callback)(scanner);
}


PJ_DEF(void) pj_cis_add_range(pj_cis_t *cis, int cstart, int cend)
{
    /* Can not set zero. This is the requirement of the parser. */
    pj_assert(cstart > 0);

    while (cstart != cend) {
        PJ_CIS_SET(cis, cstart);
	++cstart;
    }
}

PJ_DEF(void) pj_cis_add_alpha(pj_cis_t *cis)
{
    pj_cis_add_range( cis, 'a', 'z'+1);
    pj_cis_add_range( cis, 'A', 'Z'+1);
}

PJ_DEF(void) pj_cis_add_num(pj_cis_t *cis)
{
    pj_cis_add_range( cis, '0', '9'+1);
}

PJ_DEF(void) pj_cis_add_str( pj_cis_t *cis, const char *str)
{
    while (*str) {
        PJ_CIS_SET(cis, *str);
	++str;
    }
}

PJ_DEF(void) pj_cis_add_cis( pj_cis_t *cis, const pj_cis_t *rhs)
{
    int i;
    for (i=0; i<256; ++i) {
	if (PJ_CIS_ISSET(rhs, i))
	    PJ_CIS_SET(cis, i);
    }
}

PJ_DEF(void) pj_cis_del_range( pj_cis_t *cis, int cstart, int cend)
{
    while (cstart != cend) {
        PJ_CIS_CLR(cis, cstart);
        cstart++;
    }
}

PJ_DEF(void) pj_cis_del_str( pj_cis_t *cis, const char *str)
{
    while (*str) {
        PJ_CIS_CLR(cis, *str);
	++str;
    }
}

PJ_DEF(void) pj_cis_invert( pj_cis_t *cis )
{
    unsigned i;
    /* Can not set zero. This is the requirement of the parser. */
    for (i=1; i<256; ++i) {
	if (PJ_CIS_ISSET(cis,i))
            PJ_CIS_CLR(cis,i);
        else
            PJ_CIS_SET(cis,i);
    }
}

PJ_DEF(void) pj_scan_init( int inst_id, pj_scanner *scanner, char *bufstart, int buflen, 
			   unsigned options, pj_syn_err_func_ptr callback )
{
    PJ_CHECK_STACK();

    scanner->begin = scanner->curptr = bufstart;
    scanner->end = bufstart + buflen;
    scanner->line = 1;
    scanner->start_line = scanner->begin;
    scanner->callback = callback;
    scanner->skip_ws = options;
	scanner->inst_id = inst_id;

    if (scanner->skip_ws) 
	pj_scan_skip_whitespace(scanner);
}


PJ_DEF(void) pj_scan_fini( pj_scanner *scanner )
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(scanner);
}

PJ_DEF(void) pj_scan_skip_whitespace( pj_scanner *scanner )
{
    register char *s = scanner->curptr;

    while (PJ_SCAN_IS_SPACE(*s)) {
	++s;
    }

    if (PJ_SCAN_IS_NEWLINE(*s) && (scanner->skip_ws & PJ_SCAN_AUTOSKIP_NEWLINE)) {
	for (;;) {
	    if (*s == '\r') {
		++s;
		if (*s == '\n') ++s;
		++scanner->line;
		scanner->curptr = scanner->start_line = s;
	    } else if (*s == '\n') {
		++s;
		++scanner->line;
		scanner->curptr = scanner->start_line = s;
	    } else if (PJ_SCAN_IS_SPACE(*s)) {
		do {
		    ++s;
		} while (PJ_SCAN_IS_SPACE(*s));
	    } else {
		break;
	    }
	}
    }

    if (PJ_SCAN_IS_NEWLINE(*s) && (scanner->skip_ws & PJ_SCAN_AUTOSKIP_WS_HEADER)==PJ_SCAN_AUTOSKIP_WS_HEADER) {
	/* Check for header continuation. */
	scanner->curptr = s;

	if (*s == '\r') {
	    ++s;
	}
	if (*s == '\n') {
	    ++s;
	}
	scanner->start_line = s;

	if (PJ_SCAN_IS_SPACE(*s)) {
	    register char *t = s;
	    do {
		++t;
	    } while (PJ_SCAN_IS_SPACE(*t));

	    ++scanner->line;
	    scanner->curptr = t;
	}
    } else {
	scanner->curptr = s;
    }
}

PJ_DEF(void) pj_scan_skip_line( pj_scanner *scanner )
{
    char *s = pj_ansi_strchr(scanner->curptr, '\n');
    if (!s) {
	scanner->curptr = scanner->end;
    } else {
	scanner->curptr = scanner->start_line = s+1;
	scanner->line++;
   }
}

PJ_DEF(int) pj_scan_peek( pj_scanner *scanner,
			  const pj_cis_t *spec, pj_str_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    /* Don't need to check EOF with PJ_SCAN_CHECK_EOF(s) */
    while (pj_cis_match(spec, *s))
	++s;

    pj_strset3(out, scanner->curptr, s);
    return *s;
}


PJ_DEF(int) pj_scan_peek_n( pj_scanner *scanner,
			     pj_size_t len, pj_str_t *out)
{
    char *endpos = scanner->curptr + len;

    if (endpos > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    pj_strset(out, scanner->curptr, len);
    return *endpos;
}


PJ_DEF(int) pj_scan_peek_until( pj_scanner *scanner,
				const pj_cis_t *spec, 
				pj_str_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    while (PJ_SCAN_CHECK_EOF(s) && !pj_cis_match( spec, *s))
	++s;

    pj_strset3(out, scanner->curptr, s);
    return *s;
}


PJ_DEF(void) pj_scan_get( pj_scanner *scanner,
			  const pj_cis_t *spec, pj_str_t *out)
{
    register char *s = scanner->curptr;

    pj_assert(pj_cis_match(spec,0)==0);

    /* EOF is detected implicitly */
    if (!pj_cis_match(spec, *s)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    do {
	++s;
    } while (pj_cis_match(spec, *s));
    /* No need to check EOF here (PJ_SCAN_CHECK_EOF(s)) because
     * buffer is NULL terminated and pj_cis_match(spec,0) should be
     * false.
     */

    pj_strset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);    
    }
}


PJ_DEF(void) pj_scan_get_unescape( pj_scanner *scanner,
				   const pj_cis_t *spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    char *dst = s;

    pj_assert(pj_cis_match(spec,0)==0);

    /* Must not match character '%' */
    pj_assert(pj_cis_match(spec,'%')==0);

    /* EOF is detected implicitly */
    if (!pj_cis_match(spec, *s) && *s != '%') {
	pj_scan_syntax_err(scanner);
	return;
    }

    out->ptr = s;
    do {
	if (*s == '%') {
	    if (s+3 <= scanner->end && pj_isxdigit(*(s+1)) && 
		pj_isxdigit(*(s+2))) 
	    {
		*dst = (pj_uint8_t) ((pj_hex_digit_to_val(*(s+1)) << 4) +
				      pj_hex_digit_to_val(*(s+2)));
		++dst;
		s += 3;
	    } else {
		*dst++ = *s++;
		*dst++ = *s++;
		break;
	    }
	}
	
	if (pj_cis_match(spec, *s)) {
	    char *start = s;
	    do {
		++s;
	    } while (pj_cis_match(spec, *s));

	    if (dst != start) pj_memmove(dst, start, s-start);
	    dst += (s-start);
	} 
	
    } while (*s == '%');

    scanner->curptr = s;
    out->slen = (dst - out->ptr);

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);    
    }
}


PJ_DEF(void) pj_scan_get_quote( pj_scanner *scanner,
				int begin_quote, int end_quote, 
				pj_str_t *out)
{
    char beg = (char)begin_quote;
    char end = (char)end_quote;
    pj_scan_get_quotes(scanner, &beg, &end, 1, out);
}

PJ_DEF(void) pj_scan_get_quotes(pj_scanner *scanner,
                                const char *begin_quote, const char *end_quote,
                                int qsize, pj_str_t *out)
{
    register char *s = scanner->curptr;
    int qpair = -1;
    int i;

    pj_assert(qsize > 0);

    /* Check and eat the begin_quote. */
    for (i = 0; i < qsize; ++i) {
	if (*s == begin_quote[i]) {
	    qpair = i;
	    break;
	}
    }
    if (qpair == -1) {
	pj_scan_syntax_err(scanner);
	return;
    }
    ++s;

    /* Loop until end_quote is found. 
     */
    do {
	/* loop until end_quote is found. */
	while (PJ_SCAN_CHECK_EOF(s) && *s != '\n' && *s != end_quote[qpair]) {
	    ++s;
	}

	/* check that no backslash character precedes the end_quote. */
	if (*s == end_quote[qpair]) {
	    if (*(s-1) == '\\') {
		if (s-2 == scanner->begin) {
		    break;
		} else {
		    char *q = s-2;
		    char *r = s-2;

		    while (r != scanner->begin && *r == '\\') {
			--r;
		    }
		    /* break from main loop if we have odd number of backslashes */
		    if (((unsigned)(q-r) & 0x01) == 1) {
			break;
		    }
		    ++s;
		}
	    } else {
		/* end_quote is not preceeded by backslash. break now. */
		break;
	    }
	} else {
	    /* loop ended by non-end_quote character. break now. */
	    break;
	}
    } while (1);

    /* Check and eat the end quote. */
    if (*s != end_quote[qpair]) {
	pj_scan_syntax_err(scanner);
	return;
    }
    ++s;

    pj_strset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_n( pj_scanner *scanner,
			    unsigned N, pj_str_t *out)
{
    if (scanner->curptr + N > scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    pj_strset(out, scanner->curptr, N);
    
    scanner->curptr += N;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(int) pj_scan_get_char( pj_scanner *scanner )
{
    int chr = *scanner->curptr;

    if (!chr) {
	pj_scan_syntax_err(scanner);
	return 0;
    }

    ++scanner->curptr;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
    return chr;
}


PJ_DEF(void) pj_scan_get_newline( pj_scanner *scanner )
{
    if (!PJ_SCAN_IS_NEWLINE(*scanner->curptr)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    if (*scanner->curptr == '\r') {
	++scanner->curptr;
    }
    if (*scanner->curptr == '\n') {
	++scanner->curptr;
    }

    ++scanner->line;
    scanner->start_line = scanner->curptr;

    /**
     * This probably is a bug, see PROTOS test #2480.
     * This would cause scanner to incorrectly eat two new lines, e.g.
     * when parsing:
     *   
     *	Content-Length: 120\r\n
     *	\r\n
     *	<space><space><space>...
     *
     * When pj_scan_get_newline() is called to parse the first newline
     * in the Content-Length header, it will eat the second newline
     * too because it thinks that it's a header continuation.
     *
     * if (PJ_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
     *    pj_scan_skip_whitespace(scanner);
     * }
     */
}


PJ_DEF(void) pj_scan_get_until( pj_scanner *scanner,
				const pj_cis_t *spec, pj_str_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    while (PJ_SCAN_CHECK_EOF(s) && !pj_cis_match(spec, *s)) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_until_ch( pj_scanner *scanner, 
				   int until_char, pj_str_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    while (PJ_SCAN_CHECK_EOF(s) && *s != until_char) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_until_chr( pj_scanner *scanner,
				     const char *until_spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    int speclen;

    if (s >= scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    speclen = strlen(until_spec);
    while (PJ_SCAN_CHECK_EOF(s) && !memchr(until_spec, *s, speclen)) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}

PJ_DEF(void) pj_scan_advance_n( pj_scanner *scanner,
				 unsigned N, pj_bool_t skip_ws)
{
    if (scanner->curptr + N > scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    scanner->curptr += N;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(int) pj_scan_strcmp( pj_scanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }
    return strncmp(scanner->curptr, s, len);
}


PJ_DEF(int) pj_scan_stricmp( pj_scanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }
    return pj_ansi_strnicmp(scanner->curptr, s, len);
}

PJ_DEF(int) pj_scan_stricmp_alnum( pj_scanner *scanner, const char *s, 
				   int len)
{
    if (scanner->curptr + len > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }
    return strnicmp_alnum(scanner->curptr, s, len);
}

PJ_DEF(void) pj_scan_save_state( const pj_scanner *scanner, 
				 pj_scan_state *state)
{
    state->curptr = scanner->curptr;
    state->line = scanner->line;
    state->start_line = scanner->start_line;
}


PJ_DEF(void) pj_scan_restore_state( pj_scanner *scanner, 
				    pj_scan_state *state)
{
    scanner->curptr = state->curptr;
    scanner->line = state->line;
    scanner->start_line = state->start_line;
}


