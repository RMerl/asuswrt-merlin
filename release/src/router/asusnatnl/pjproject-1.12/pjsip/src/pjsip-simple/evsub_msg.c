/* $Id: evsub_msg.c 3565 2011-05-15 12:13:24Z bennylp $ */
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
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_parser.h>
#include <pjlib-util/string.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/except.h>

/*
 * Event header.
 */
static int pjsip_event_hdr_print( pjsip_event_hdr *hdr, 
				  char *buf, pj_size_t size);
static pjsip_event_hdr* pjsip_event_hdr_clone( pj_pool_t *pool, 
					       const pjsip_event_hdr *hdr);
static pjsip_event_hdr* pjsip_event_hdr_shallow_clone( pj_pool_t *pool,
						       const pjsip_event_hdr*);

static pjsip_hdr_vptr event_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &pjsip_event_hdr_clone,
    (pjsip_hdr_clone_fptr) &pjsip_event_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &pjsip_event_hdr_print,
};


PJ_DEF(pjsip_event_hdr*) pjsip_event_hdr_create(pj_pool_t *pool)
{
    pjsip_event_hdr *hdr = PJ_POOL_ZALLOC_T(pool, pjsip_event_hdr);
    hdr->type = PJSIP_H_OTHER;
    hdr->name.ptr = "Event";
    hdr->name.slen = 5;
    hdr->sname.ptr = "o";
    hdr->sname.slen = 1;
    hdr->vptr = &event_hdr_vptr;
    pj_list_init(hdr);
    pj_list_init(&hdr->other_param);
    return hdr;
}

static int pjsip_event_hdr_print( pjsip_event_hdr *hdr, 
				  char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    copy_advance(p, hdr->event_type);
    copy_advance_pair(p, ";id=", 4, hdr->id_param);
    
    printed = pjsip_param_print_on(&hdr->other_param, p, endbuf-p,
				   &pc->pjsip_TOKEN_SPEC, 
				   &pc->pjsip_TOKEN_SPEC, ';');
    if (printed < 0)
	return printed;

    p += printed;
    return p - buf;
}

static pjsip_event_hdr* pjsip_event_hdr_clone( pj_pool_t *pool, 
					       const pjsip_event_hdr *rhs)
{
    pjsip_event_hdr *hdr = pjsip_event_hdr_create(pool);
    pj_strdup(pool, &hdr->event_type, &rhs->event_type);
    pj_strdup(pool, &hdr->id_param, &rhs->id_param);
    pjsip_param_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}

static pjsip_event_hdr* 
pjsip_event_hdr_shallow_clone( pj_pool_t *pool,
			       const pjsip_event_hdr *rhs )
{
    pjsip_event_hdr *hdr = PJ_POOL_ALLOC_T(pool, pjsip_event_hdr);
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    pjsip_param_shallow_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}


/*
 * Allow-Events header.
 */
PJ_DEF(pjsip_allow_events_hdr*) pjsip_allow_events_hdr_create(pj_pool_t *pool)
{
    const pj_str_t STR_ALLOW_EVENTS = { "Allow-Events", 12};
    pjsip_allow_events_hdr *hdr;

    hdr = pjsip_generic_array_hdr_create(pool, &STR_ALLOW_EVENTS);

    if (hdr) {
	hdr->sname.ptr = "u";
	hdr->sname.slen = 1;
    }

    return hdr;
}


/*
 * Subscription-State header.
 */
static int pjsip_sub_state_hdr_print(pjsip_sub_state_hdr *hdr, 
				     char *buf, pj_size_t size);
static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_clone(pj_pool_t *pool, 
			  const pjsip_sub_state_hdr *hdr);
static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_shallow_clone(pj_pool_t *pool,
				  const pjsip_sub_state_hdr*);

static pjsip_hdr_vptr sub_state_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &pjsip_sub_state_hdr_clone,
    (pjsip_hdr_clone_fptr) &pjsip_sub_state_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &pjsip_sub_state_hdr_print,
};


PJ_DEF(pjsip_sub_state_hdr*) pjsip_sub_state_hdr_create(pj_pool_t *pool)
{
    pj_str_t sub_state = { "Subscription-State", 18 };
    pjsip_sub_state_hdr *hdr = PJ_POOL_ZALLOC_T(pool, pjsip_sub_state_hdr);
    hdr->type = PJSIP_H_OTHER;
    hdr->name = hdr->sname = sub_state;
    hdr->vptr = &sub_state_hdr_vptr;
    hdr->expires_param = -1;
    hdr->retry_after = -1;
    pj_list_init(hdr);
    pj_list_init(&hdr->other_param);
    return hdr;
}

static int pjsip_sub_state_hdr_print(pjsip_sub_state_hdr *hdr, 
				     char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    copy_advance_escape(p, hdr->sub_state, pc->pjsip_TOKEN_SPEC);
    copy_advance_pair_escape(p, ";reason=", 8, hdr->reason_param,
			     pc->pjsip_TOKEN_SPEC);
    if (hdr->expires_param >= 0) {
	pj_memcpy(p, ";expires=", 9);
	p += 9;
	printed = pj_utoa(hdr->expires_param, p);
	p += printed;
    }
    if (hdr->retry_after >= 0) {
	pj_memcpy(p, ";retry-after=", 13);
	p += 9;
	printed = pj_utoa(hdr->retry_after, p);
	p += printed;
    }
    
    printed = pjsip_param_print_on( &hdr->other_param, p, endbuf-p, 
				    &pc->pjsip_TOKEN_SPEC,
				    &pc->pjsip_TOKEN_SPEC,
				    ';');
    if (printed < 0)
	return printed;

    p += printed;

    return p - buf;
}

static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_clone(pj_pool_t *pool, 
			  const pjsip_sub_state_hdr *rhs)
{
    pjsip_sub_state_hdr *hdr = pjsip_sub_state_hdr_create(pool);
    pj_strdup(pool, &hdr->sub_state, &rhs->sub_state);
    pj_strdup(pool, &hdr->reason_param, &rhs->reason_param);
    hdr->retry_after = rhs->retry_after;
    hdr->expires_param = rhs->expires_param;
    pjsip_param_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}

static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_shallow_clone(pj_pool_t *pool,
				  const pjsip_sub_state_hdr *rhs)
{
    pjsip_sub_state_hdr *hdr = PJ_POOL_ALLOC_T(pool, pjsip_sub_state_hdr);
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    pjsip_param_shallow_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}


/*
 * Parse Event header.
 */
static pjsip_hdr *parse_hdr_event(pjsip_parse_ctx *ctx)
{
    pjsip_event_hdr *hdr = pjsip_event_hdr_create(ctx->pool);
    const pj_str_t id_param = { "id", 2 };
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    pj_scan_get(ctx->scanner, &pc->pjsip_TOKEN_SPEC, &hdr->event_type);

    while (*ctx->scanner->curptr == ';') {
	pj_str_t pname, pvalue;

	pj_scan_get_char(ctx->scanner);
	pjsip_parse_param_imp(ctx->scanner, ctx->pool, &pname, &pvalue, 0);

	if (pj_stricmp(&pname, &id_param)==0) {
	    hdr->id_param = pvalue;
	} else {
	    pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
	    param->name = pname;
	    param->value = pvalue;
	    pj_list_push_back(&hdr->other_param, param);
	}
    }
    pjsip_parse_end_hdr_imp( ctx->scanner );
    return (pjsip_hdr*)hdr;
}

/*
 * Parse Subscription-State header.
 */
static pjsip_hdr* parse_hdr_sub_state( pjsip_parse_ctx *ctx )
{
    pjsip_sub_state_hdr *hdr = pjsip_sub_state_hdr_create(ctx->pool);
    const pj_str_t reason = { "reason", 6 },
		   expires = { "expires", 7 },
		   retry_after = { "retry-after", 11 };
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    pj_scan_get(ctx->scanner, &pc->pjsip_TOKEN_SPEC, &hdr->sub_state);

    while (*ctx->scanner->curptr == ';') {
	pj_str_t pname, pvalue;

	pj_scan_get_char(ctx->scanner);
	pjsip_parse_param_imp(ctx->scanner, ctx->pool, &pname, &pvalue, 0);

	if (pj_stricmp(&pname, &reason) == 0) {
	    hdr->reason_param = pvalue;

	} else if (pj_stricmp(&pname, &expires) == 0) {
	    hdr->expires_param = pj_strtoul(&pvalue);

	} else if (pj_stricmp(&pname, &retry_after) == 0) {
	    hdr->retry_after = pj_strtoul(&pvalue);

	} else {
	    pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
	    param->name = pname;
	    param->value = pvalue;
	    pj_list_push_back(&hdr->other_param, param);
	}
    }

    pjsip_parse_end_hdr_imp( ctx->scanner );
    return (pjsip_hdr*)hdr;
}

/*
 * Register header parsers.
 */
PJ_DEF(void) pjsip_evsub_init_parser(int inst_id)
{
    pjsip_register_hdr_parser( inst_id, "Event", "o",
			       &parse_hdr_event);

    pjsip_register_hdr_parser( inst_id, "Subscription-State", NULL, 
			       &parse_hdr_sub_state);
}

