/* $Id: pidf.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_PIDF_H__
#define __PJSIP_SIMPLE_PIDF_H__

/**
 * @file pidf.h
 * @brief PIDF/Presence Information Data Format (RFC 3863)
 */
#include <pjsip-simple/types.h>
#include <pjlib-util/xml.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_PIDF PIDF/Presence Information Data Format (RFC 3863)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for PIDF/Presence Information Data Format (RFC 3863)
 * @{
 *
 * This file provides tools for manipulating Presence Information Data 
 * Format (PIDF) as described in RFC 3863.
 */
typedef struct pj_xml_node pjpidf_pres;
typedef struct pj_xml_node pjpidf_tuple;
typedef struct pj_xml_node pjpidf_status;
typedef struct pj_xml_node pjpidf_note;

typedef struct pjpidf_status_op
{
    void	    (*construct)(pj_pool_t*, pjpidf_status*);
    pj_bool_t	    (*is_basic_open)(const pjpidf_status*);
    void	    (*set_basic_open)(pjpidf_status*, pj_bool_t);
} pjpidf_status_op;

typedef struct pjpidf_tuple_op
{
    void	    (*construct)(pj_pool_t*, pjpidf_tuple*, const pj_str_t*);

    const pj_str_t* (*get_id)(const pjpidf_tuple* );
    void	    (*set_id)(pj_pool_t*, pjpidf_tuple *, const pj_str_t*);

    pjpidf_status*  (*get_status)(pjpidf_tuple* );

    const pj_str_t* (*get_contact)(const pjpidf_tuple*);
    void	    (*set_contact)(pj_pool_t*, pjpidf_tuple*, const pj_str_t*);
    void	    (*set_contact_prio)(pj_pool_t*, pjpidf_tuple*, const pj_str_t*);
    const pj_str_t* (*get_contact_prio)(const pjpidf_tuple*);

    pjpidf_note*    (*add_note)(pj_pool_t*, pjpidf_tuple*, const pj_str_t*);
    pjpidf_note*    (*get_first_note)(pjpidf_tuple*);
    pjpidf_note*    (*get_next_note)(pjpidf_tuple*, pjpidf_note*);

    const pj_str_t* (*get_timestamp)(const pjpidf_tuple*);
    void	    (*set_timestamp)(pj_pool_t*, pjpidf_tuple*, const pj_str_t*);
    void	    (*set_timestamp_np)(pj_pool_t*,pjpidf_tuple*, pj_str_t*);

} pjpidf_tuple_op;

typedef struct pjpidf_pres_op
{
    void	    (*construct)(pj_pool_t*, pjpidf_pres*, const pj_str_t*);

    pjpidf_tuple*   (*add_tuple)(pj_pool_t*, pjpidf_pres*, const pj_str_t*);
    pjpidf_tuple*   (*get_first_tuple)(pjpidf_pres*);
    pjpidf_tuple*   (*get_next_tuple)(pjpidf_pres*, pjpidf_tuple*);
    pjpidf_tuple*   (*find_tuple)(pjpidf_pres*, const pj_str_t*);
    void	    (*remove_tuple)(pjpidf_pres*, pjpidf_tuple*);

    pjpidf_note*    (*add_note)(pj_pool_t*, pjpidf_pres*, const pj_str_t*);
    pjpidf_note*    (*get_first_note)(pjpidf_pres*);
    pjpidf_note*    (*get_next_note)(pjpidf_pres*, pjpidf_note*);

} pjpidf_pres_op;


extern struct pjpidf_op_desc
{
    pjpidf_pres_op	pres;
    pjpidf_tuple_op	tuple;
    pjpidf_status_op	status;
} pjpidf_op;


/******************************************************************************
 * Top level API for managing presence document. 
 *****************************************************************************/
PJ_DECL(pjpidf_pres*)    pjpidf_create(pj_pool_t *pool, const pj_str_t *entity);
PJ_DECL(pjpidf_pres*)	 pjpidf_parse(int inst_id, pj_pool_t *pool, char *text, int len);
PJ_DECL(int)		 pjpidf_print(const pjpidf_pres* pres, char *buf, int len);


/******************************************************************************
 * API for managing Presence node.
 *****************************************************************************/
PJ_DECL(void)		 pjpidf_pres_construct(pj_pool_t *pool, pjpidf_pres *pres,
					       const pj_str_t *entity);
PJ_DECL(pjpidf_tuple*)	 pjpidf_pres_add_tuple(pj_pool_t *pool, pjpidf_pres *pres, 
					       const pj_str_t *id);
PJ_DECL(pjpidf_tuple*)	 pjpidf_pres_get_first_tuple(pjpidf_pres *pres);
PJ_DECL(pjpidf_tuple*)	 pjpidf_pres_get_next_tuple(pjpidf_pres *pres, 
						    pjpidf_tuple *t);
PJ_DECL(pjpidf_tuple*)	 pjpidf_pres_find_tuple(pjpidf_pres *pres, 
						const pj_str_t *id);
PJ_DECL(void)		 pjpidf_pres_remove_tuple(pjpidf_pres *pres, 
						  pjpidf_tuple*);

PJ_DECL(pjpidf_note*)	 pjpidf_pres_add_note(pj_pool_t *pool, pjpidf_pres *pres, 
					      const pj_str_t *text);
PJ_DECL(pjpidf_note*)	 pjpidf_pres_get_first_note(pjpidf_pres *pres);
PJ_DECL(pjpidf_note*)	 pjpidf_pres_get_next_note(pjpidf_pres*, pjpidf_note*);


/******************************************************************************
 * API for managing Tuple node.
 *****************************************************************************/
PJ_DECL(void)		 pjpidf_tuple_construct(pj_pool_t *pool, pjpidf_tuple *t,
						const pj_str_t *id);
PJ_DECL(const pj_str_t*) pjpidf_tuple_get_id(const pjpidf_tuple *t );
PJ_DECL(void)		 pjpidf_tuple_set_id(pj_pool_t *pool, pjpidf_tuple *t, 
					     const pj_str_t *id);

PJ_DECL(pjpidf_status*)  pjpidf_tuple_get_status(pjpidf_tuple *t);

PJ_DECL(const pj_str_t*) pjpidf_tuple_get_contact(const pjpidf_tuple *t);
PJ_DECL(void)		 pjpidf_tuple_set_contact(pj_pool_t *pool, pjpidf_tuple *t, 
						  const pj_str_t *contact);
PJ_DECL(void)		 pjpidf_tuple_set_contact_prio(pj_pool_t *pool, pjpidf_tuple *t, 
						       const pj_str_t *prio);
PJ_DECL(const pj_str_t*) pjpidf_tuple_get_contact_prio(const pjpidf_tuple *t);

PJ_DECL(pjpidf_note*)	 pjpidf_tuple_add_note(pj_pool_t *pool, pjpidf_tuple *t,
					       const pj_str_t *text);
PJ_DECL(pjpidf_note*)	 pjpidf_tuple_get_first_note(pjpidf_tuple *t);
PJ_DECL(pjpidf_note*)	 pjpidf_tuple_get_next_note(pjpidf_tuple *t, pjpidf_note *n);

PJ_DECL(const pj_str_t*) pjpidf_tuple_get_timestamp(const pjpidf_tuple *t);
PJ_DECL(void)		 pjpidf_tuple_set_timestamp(pj_pool_t *pool, pjpidf_tuple *t,
						    const pj_str_t *ts);
PJ_DECL(void)		 pjpidf_tuple_set_timestamp_np(	pj_pool_t*, pjpidf_tuple *t,
							pj_str_t *ts);


/******************************************************************************
 * API for managing Status node.
 *****************************************************************************/
PJ_DECL(void)		 pjpidf_status_construct(pj_pool_t*, pjpidf_status*);
PJ_DECL(pj_bool_t)	 pjpidf_status_is_basic_open(const pjpidf_status*);
PJ_DECL(void)		 pjpidf_status_set_basic_open(pjpidf_status*, pj_bool_t);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_PIDF_H__ */
