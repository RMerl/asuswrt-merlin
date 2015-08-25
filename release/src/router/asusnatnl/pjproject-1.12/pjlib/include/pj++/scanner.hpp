/* $Id: scanner.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJPP_SCANNER_HPP__
#define __PJPP_SCANNER_HPP__

#include <pjlib-util/scanner.h>
#include <pj++/string.hpp>

class Pj_Cis;
class Pj_Cis_Buffer;
class Pj_Scanner;

class Pj_Cis_Buffer
{
    friend class Pj_Cis;

public:
    Pj_Cis_Buffer() 
    { 
	pj_cis_buf_init(&buf_); 
    }

private:
    pj_cis_buf_t buf_;
};


class Pj_Cis
{
    friend class Pj_Scanner;

public:
    Pj_Cis(Pj_Cis_Buffer *buf)
    {
	pj_cis_init(&buf->buf_, &cis_);
    }

    Pj_Cis(const Pj_Cis &rhs)
    {
	pj_cis_dup(&cis_, (pj_cis_t*)&rhs.cis_);
    }

    void add_range(int start, int end)
    {
	pj_cis_add_range(&cis_, start, end);
    }

    void add_alpha()
    {
	pj_cis_add_alpha(&cis_);
    }

    void add_num()
    {
	pj_cis_add_num(&cis_);
    }

    void add_str(const char *str)
    {
	pj_cis_add_str(&cis_, str);
    }

    void add_cis(const Pj_Cis &rhs)
    {
	pj_cis_add_cis(&cis_, &rhs.cis_);
    }

    void del_range(int start, int end)
    {
	pj_cis_del_range(&cis_, start, end);
    }

    void del_str(const char *str)
    {
	pj_cis_del_str(&cis_, str);
    }

    void invert()
    {
	pj_cis_invert(&cis_);
    }

    bool match(int c) const
    {
	return pj_cis_match(&cis_, c) != 0;
    }

private:
    pj_cis_t cis_;
};



class Pj_Scanner
{
public:
    Pj_Scanner() {}

    enum
    {
	SYNTAX_ERROR = 101
    };
    static void syntax_error_handler_throw_pj(pj_scanner *);

    typedef pj_scan_state State;

    void init(char *buf, int len, unsigned options=PJ_SCAN_AUTOSKIP_WS, 
	      pj_syn_err_func_ptr callback = &syntax_error_handler_throw_pj)
    {
	pj_scan_init(&scanner_, buf, len, options, callback);
    }

    void fini()
    {
	pj_scan_fini(&scanner_);
    }

    int eof() const
    {
	return pj_scan_is_eof(&scanner_);
    }

    int peek_char() const
    {
	return *scanner_.curptr;
    }

    int peek(const Pj_Cis *cis, Pj_String *out)
    {
	return pj_scan_peek(&scanner_,  &cis->cis_, out);
    }

    int peek_n(pj_size_t len, Pj_String *out)
    {
	return pj_scan_peek_n(&scanner_, len, out);
    }

    int peek_until(const Pj_Cis *cis, Pj_String *out)
    {
	return pj_scan_peek_until(&scanner_, &cis->cis_, out);
    }

    void get(const Pj_Cis *cis, Pj_String *out)
    {
	pj_scan_get(&scanner_, &cis->cis_, out);
    }

    void get_n(unsigned N, Pj_String *out)
    {
	pj_scan_get_n(&scanner_, N, out);
    }

    int get_char()
    {
	return pj_scan_get_char(&scanner_);
    }

    void get_quote(int begin_quote, int end_quote, Pj_String *out)
    {
	pj_scan_get_quote(&scanner_, begin_quote, end_quote, out);
    }

    void get_newline()
    {
	pj_scan_get_newline(&scanner_);
    }

    void get_until(const Pj_Cis *cis, Pj_String *out)
    {
	pj_scan_get_until(&scanner_, &cis->cis_, out);
    }

    void get_until_ch(int until_ch, Pj_String *out)
    {
	pj_scan_get_until_ch(&scanner_, until_ch, out);
    }

    void get_until_chr(const char *spec, Pj_String *out)
    {
	pj_scan_get_until_chr(&scanner_, spec, out);
    }

    void advance_n(unsigned N, bool skip_ws=true)
    {
	pj_scan_advance_n(&scanner_, N, skip_ws);
    }

    int strcmp(const char *s, int len)
    {
	return pj_scan_strcmp(&scanner_, s, len);
    }

    int stricmp(const char *s, int len)
    {
	return pj_scan_stricmp(&scanner_, s, len);
    }

    void skip_ws()
    {
	pj_scan_skip_whitespace(&scanner_);
    }

    void save_state(State *state) const
    {
	pj_scan_save_state(&scanner_, state);
    }

    void restore_state(State *state)
    {
	pj_scan_restore_state(&scanner_, state);
    }

    int get_pos_line() const
    {
	return scanner_.line;
    }

    int get_pos_col() const
    {
	return pj_scan_get_col(&scanner_);
    }


private:
    pj_scanner scanner_;
};

#endif	/* __PJPP_SCANNER_HPP__ */

