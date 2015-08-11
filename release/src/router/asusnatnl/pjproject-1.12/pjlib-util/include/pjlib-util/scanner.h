/* $Id: scanner.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_SCANNER_H__
#define __PJ_SCANNER_H__

/**
 * @file scanner.h
 * @brief Text Scanning.
 */

#include <pjlib-util/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_SCAN Fast Text Scanning
 * @ingroup PJLIB_TEXT
 * @brief Text scanning utility.
 *
 * This module describes a fast text scanning functions.
 *
 * @{
 */
#if defined(PJ_SCANNER_USE_BITWISE) && PJ_SCANNER_USE_BITWISE != 0
#  include <pjlib-util/scanner_cis_bitwise.h>
#else
#  include <pjlib-util/scanner_cis_uint.h>
#endif

/**
 * Initialize scanner input specification buffer.
 *
 * @param cs_buf    The scanner character specification.
 */
PJ_DECL(void) pj_cis_buf_init(pj_cis_buf_t *cs_buf);

/**
 * Create a new input specification.
 *
 * @param cs_buf    Specification buffer.
 * @param cis       Character input specification to be initialized.
 *
 * @return          PJ_SUCCESS if new specification has been successfully
 *                  created, or PJ_ETOOMANY if there are already too many
 *                  specifications in the buffer.
 */
PJ_DECL(pj_status_t) pj_cis_init(pj_cis_buf_t *cs_buf, pj_cis_t *cis);

/**
 * Create a new input specification based on an existing specification.
 *
 * @param new_cis   The new specification to be initialized.
 * @param existing  The existing specification, from which the input
 *                  bitmask will be copied to the new specification.
 *
 * @return          PJ_SUCCESS if new specification has been successfully
 *                  created, or PJ_ETOOMANY if there are already too many
 *                  specifications in the buffer.
 */
PJ_DECL(pj_status_t) pj_cis_dup(pj_cis_t *new_cis, pj_cis_t *existing);

/**
 * Add the characters in the specified range '[cstart, cend)' to the 
 * specification (the last character itself ('cend') is not added).
 *
 * @param cis       The scanner character specification.
 * @param cstart    The first character in the range.
 * @param cend      The next character after the last character in the range.
 */
PJ_DECL(void) pj_cis_add_range( pj_cis_t *cis, int cstart, int cend);

/**
 * Add alphabetic characters to the specification.
 *
 * @param cis       The scanner character specification.
 */
PJ_DECL(void) pj_cis_add_alpha( pj_cis_t *cis);

/**
 * Add numeric characters to the specification.
 *
 * @param cis       The scanner character specification.
 */
PJ_DECL(void) pj_cis_add_num( pj_cis_t *cis);

/**
 * Add the characters in the string to the specification.
 *
 * @param cis       The scanner character specification.
 * @param str       The string.
 */
PJ_DECL(void) pj_cis_add_str( pj_cis_t *cis, const char *str);

/**
 * Add specification from another specification.
 *
 * @param cis	    The specification is to be set.
 * @param rhs	    The specification to be copied.
 */
PJ_DECL(void) pj_cis_add_cis( pj_cis_t *cis, const pj_cis_t *rhs);

/**
 * Delete characters in the specified range from the specification.
 *
 * @param cis       The scanner character specification.
 * @param cstart    The first character in the range.
 * @param cend      The next character after the last character in the range.
 */
PJ_DECL(void) pj_cis_del_range( pj_cis_t *cis, int cstart, int cend);

/**
 * Delete characters in the specified string from the specification.
 *
 * @param cis       The scanner character specification.
 * @param str       The string.
 */
PJ_DECL(void) pj_cis_del_str( pj_cis_t *cis, const char *str);

/**
 * Invert specification.
 *
 * @param cis       The scanner character specification.
 */
PJ_DECL(void) pj_cis_invert( pj_cis_t *cis );

/**
 * Check whether the specified character belongs to the specification.
 *
 * @param cis       The scanner character specification.
 * @param c         The character to check for matching.
 *
 * @return	    Non-zero if match (not necessarily one).
 */
PJ_INLINE(int) pj_cis_match( const pj_cis_t *cis, pj_uint8_t c )
{
    return PJ_CIS_ISSET(cis, c);
}


/**
 * Flags for scanner.
 */
enum
{
    /** This flags specifies that the scanner should automatically skip
	whitespaces 
     */
    PJ_SCAN_AUTOSKIP_WS = 1,

    /** This flags specifies that the scanner should automatically skip
        SIP header continuation. This flag implies PJ_SCAN_AUTOSKIP_WS.
     */
    PJ_SCAN_AUTOSKIP_WS_HEADER = 3,

    /** Auto-skip new lines.
     */
    PJ_SCAN_AUTOSKIP_NEWLINE = 4
};


/* Forward decl. */
struct pj_scanner;


/**
 * The callback function type to be called by the scanner when it encounters
 * syntax error.
 *
 * @param scanner       The scanner instance that calls the callback .
 */
typedef void (*pj_syn_err_func_ptr)(struct pj_scanner *scanner);


/**
 * The text scanner structure.
 */
typedef struct pj_scanner
{
    char *begin;        /**< Start of input buffer.	*/
    char *end;          /**< End of input buffer.	*/
    char *curptr;       /**< Current pointer.		*/
    int   line;         /**< Current line.		*/
    char *start_line;   /**< Where current line starts.	*/
    int   skip_ws;      /**< Skip whitespace flag.	*/
	int   inst_id;
    pj_syn_err_func_ptr callback;   /**< Syntax error callback. */
} pj_scanner;


/**
 * This structure can be used by application to store the state of the parser,
 * so that the scanner state can be rollback to this state when necessary.
 */
typedef struct pj_scan_state
{
    char *curptr;       /**< Current scanner's pointer. */
    int   line;         /**< Current line.		*/
    char *start_line;   /**< Start of current line.	*/
} pj_scan_state;


/**
 * Initialize the scanner. Note that the input string buffer must have
 * length at least buflen+1 because the scanner will NULL terminate the
 * string during initialization.
 *
 * @param scanner   The scanner to be initialized.
 * @param bufstart  The input buffer to scan. Note that buffer[buflen] will be 
 *		    filled with NULL char until scanner is destroyed, so
 *		    the actual buffer length must be at least buflen+1.
 * @param buflen    The length of the input buffer, which normally is
 *		    strlen(bufstart).
 * @param options   Zero, or combination of PJ_SCAN_AUTOSKIP_WS or
 *		    PJ_SCAN_AUTOSKIP_WS_HEADER
 * @param callback  Callback to be called when the scanner encounters syntax
 *		    error condition.
 */
PJ_DECL(void) pj_scan_init( int inst_id, 
				pj_scanner *scanner, char *bufstart, int buflen, 
			    unsigned options,
			    pj_syn_err_func_ptr callback );


/** 
 * Call this function when application has finished using the scanner.
 *
 * @param scanner   The scanner.
 */
PJ_DECL(void) pj_scan_fini( pj_scanner *scanner );


/** 
 * Determine whether the EOF condition for the scanner has been met.
 *
 * @param scanner   The scanner.
 *
 * @return Non-zero if scanner is EOF.
 */
PJ_INLINE(int) pj_scan_is_eof( const pj_scanner *scanner)
{
    return scanner->curptr >= scanner->end;
}


/** 
 * Peek strings in current position according to parameter spec, and return
 * the strings in parameter out. The current scanner position will not be
 * moved. If the scanner is already in EOF state, syntax error callback will
 * be called thrown.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position or zero if there's
 *	   no more characters.
 */
PJ_DECL(int) pj_scan_peek( pj_scanner *scanner,
			   const pj_cis_t *spec, pj_str_t *out);


/** 
 * Peek len characters in current position, and return them in out parameter.
 * Note that whitespaces or newlines will be returned as it is, regardless
 * of PJ_SCAN_AUTOSKIP_WS settings. If the character left is less than len, 
 * syntax error callback will be called.
 *
 * @param scanner   The scanner.
 * @param len	    Length to peek.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position or zero if there's
 *	   no more characters.
 */
PJ_DECL(int) pj_scan_peek_n( pj_scanner *scanner,
			     pj_size_t len, pj_str_t *out);


/** 
 * Peek strings in current position until spec is matched, and return
 * the strings in parameter out. The current scanner position will not be
 * moved. If the scanner is already in EOF state, syntax error callback will
 * be called.
 *
 * @param scanner   The scanner.
 * @param spec	    The peeking will stop when the input match this spec.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position.
 */
PJ_DECL(int) pj_scan_peek_until( pj_scanner *scanner,
				 const pj_cis_t *spec, 
				 pj_str_t *out);


/** 
 * Get characters from the buffer according to the spec, and return them
 * in out parameter. The scanner will attempt to get as many characters as
 * possible as long as the spec matches. If the first character doesn't
 * match the spec, or scanner is already in EOF when this function is called,
 * an exception will be thrown.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 */
PJ_DECL(void) pj_scan_get( pj_scanner *scanner,
			   const pj_cis_t *spec, pj_str_t *out);


/** 
 * Just like #pj_scan_get(), but additionally performs unescaping when
 * escaped ('%') character is found. The input spec MUST NOT contain the
 * specification for '%' characted.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 */
PJ_DECL(void) pj_scan_get_unescape( pj_scanner *scanner,
				    const pj_cis_t *spec, pj_str_t *out);


/** 
 * Get characters between quotes. If current input doesn't match begin_quote,
 * syntax error will be thrown. Note that the resulting string will contain
 * the enclosing quote.
 *
 * @param scanner	The scanner.
 * @param begin_quote	The character to begin the quote.
 * @param end_quote	The character to end the quote.
 * @param out		String to store the result.
 */
PJ_DECL(void) pj_scan_get_quote( pj_scanner *scanner,
				 int begin_quote, int end_quote, 
				 pj_str_t *out);

/** 
 * Get characters between quotes. If current input doesn't match begin_quote,
 * syntax error will be thrown. Note that the resulting string will contain
 * the enclosing quote.
 *
 * @param scanner	The scanner.
 * @param begin_quotes  The character array to begin the quotes. For example,
 *                      the two characters " and '.
 * @param end_quotes    The character array to end the quotes. The position
 *                      found in the begin_quotes array will be used to match
 *                      the end quotes. So if the begin_quotes was the array
 *                      of "'< the end_quotes should be "'>. If begin_array
 *                      matched the ' then the end_quotes will look for ' to
 *                      match at the end.
 * @param qsize         The size of the begin_quotes and end_quotes arrays.
 * @param out           String to store the result.
 */
PJ_DECL(void) pj_scan_get_quotes(pj_scanner *scanner,
                                 const char *begin_quotes,
                                 const char *end_quotes, int qsize,
                                 pj_str_t *out);


/**
 * Get N characters from the scanner.
 *
 * @param scanner   The scanner.
 * @param N	    Number of characters to get.
 * @param out	    String to store the result.
 */
PJ_DECL(void) pj_scan_get_n( pj_scanner *scanner,
			     unsigned N, pj_str_t *out);


/** 
 * Get one character from the scanner.
 *
 * @param scanner   The scanner.
 *
 * @return The character.
 */
PJ_DECL(int) pj_scan_get_char( pj_scanner *scanner );


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches the spec.
 *
 * @param scanner   The scanner.
 * @param spec	    Get until the input match this spec.
 * @param out	    String to store the result.
 */
PJ_DECL(void) pj_scan_get_until( pj_scanner *scanner,
				 const pj_cis_t *spec, pj_str_t *out);


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches until_char.
 *
 * @param scanner	The scanner.
 * @param until_char    Get until the input match this character.
 * @param out		String to store the result.
 */
PJ_DECL(void) pj_scan_get_until_ch( pj_scanner *scanner, 
				    int until_char, pj_str_t *out);


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches until_char.
 *
 * @param scanner	The scanner.
 * @param until_spec	Get until the input match any of these characters.
 * @param out		String to store the result.
 */
PJ_DECL(void) pj_scan_get_until_chr( pj_scanner *scanner,
				     const char *until_spec, pj_str_t *out);

/** 
 * Advance the scanner N characters, and skip whitespace
 * if necessary.
 *
 * @param scanner   The scanner.
 * @param N	    Number of characters to skip.
 * @param skip	    Flag to specify whether whitespace should be skipped
 *		    after skipping the characters.
 */
PJ_DECL(void) pj_scan_advance_n( pj_scanner *scanner,
				 unsigned N, pj_bool_t skip);


/** 
 * Compare string in current position with the specified string.
 * 
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare.
 *
 * @return zero, <0, or >0 (just like strcmp()).
 */
PJ_DECL(int) pj_scan_strcmp( pj_scanner *scanner, const char *s, int len);


/** 
 * Case-less string comparison of current position with the specified
 * string.
 *
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare with.
 *
 * @return zero, <0, or >0 (just like strcmp()).
 */
PJ_DECL(int) pj_scan_stricmp( pj_scanner *scanner, const char *s, int len);

/**
 * Perform case insensitive string comparison of string in current position,
 * knowing that the string to compare only consists of alphanumeric
 * characters.
 *
 * Note that unlike #pj_scan_stricmp, this function can only return zero or
 * -1.
 *
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare with.
 *
 * @return	    zero if equal or -1.
 *
 * @see strnicmp_alnum, pj_stricmp_alnum
 */
PJ_DECL(int) pj_scan_stricmp_alnum( pj_scanner *scanner, const char *s, 
				    int len);


/** 
 * Get a newline from the scanner. A newline is defined as '\\n', or '\\r', or
 * "\\r\\n". If current input is not newline, syntax error will be thrown.
 *
 * @param scanner   The scanner.
 */
PJ_DECL(void) pj_scan_get_newline( pj_scanner *scanner );


/** 
 * Manually skip whitespaces according to flag that was specified when
 * the scanner was initialized.
 *
 * @param scanner   The scanner.
 */
PJ_DECL(void) pj_scan_skip_whitespace( pj_scanner *scanner );


/**
 * Skip current line.
 *
 * @param scanner   The scanner.
 */
PJ_DECL(void) pj_scan_skip_line( pj_scanner *scanner );

/** 
 * Save the full scanner state.
 *
 * @param scanner   The scanner.
 * @param state	    Variable to store scanner's state.
 */
PJ_DECL(void) pj_scan_save_state( const pj_scanner *scanner, 
				  pj_scan_state *state);


/** 
 * Restore the full scanner state.
 * Note that this would not restore the string if application has modified
 * it. This will only restore the scanner scanning position.
 *
 * @param scanner   The scanner.
 * @param state	    State of the scanner.
 */
PJ_DECL(void) pj_scan_restore_state( pj_scanner *scanner, 
				     pj_scan_state *state);

/**
 * Get current column position.
 *
 * @param scanner   The scanner.
 *
 * @return	    The column position.
 */
PJ_INLINE(int) pj_scan_get_col( const pj_scanner *scanner )
{
    return (int)(scanner->curptr - scanner->start_line);
}

/**
 * @}
 */


PJ_END_DECL

#endif

