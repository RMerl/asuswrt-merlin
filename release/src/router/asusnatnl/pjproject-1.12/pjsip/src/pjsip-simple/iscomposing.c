/* $Id: iscomposing.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/iscomposing.h>
#include <pjsip-simple/errno.h>
#include <pjsip/sip_msg.h>
#include <pjlib-util/errno.h>
#include <pj/pool.h>
#include <pj/string.h>


/* MIME */
static const pj_str_t STR_MIME_TYPE = { "application", 11 };
static const pj_str_t STR_MIME_SUBTYPE = { "im-iscomposing+xml", 18 };


/* XML node constants. */
static const pj_str_t STR_ISCOMPOSING	= { "isComposing", 11 };
static const pj_str_t STR_STATE		= { "state", 5 };
static const pj_str_t STR_ACTIVE	= { "active", 6 };
static const pj_str_t STR_IDLE		= { "idle", 4 };
static const pj_str_t STR_LASTACTIVE	= { "lastactive", 10 };
static const pj_str_t STR_CONTENTTYPE	= { "contenttype", 11 };
static const pj_str_t STR_REFRESH	= { "refresh", 7 };


/* XML attributes constants */
static const pj_str_t STR_XMLNS_NAME =     { "xmlns", 5 };
static const pj_str_t STR_XMLNS_VAL =      { "urn:ietf:params:xml:ns:im-iscomposing", 37 };
static const pj_str_t STR_XMLNS_XSI_NAME = { "xmlns:xsi", 9 };
static const pj_str_t STR_XMLNS_XSI_VAL  = { "http://www.w3.org/2001/XMLSchema-instance", 41 };
static const pj_str_t STR_XSI_SLOC_NAME =  { "xsi:schemaLocation", 18 };
static const pj_str_t STR_XSI_SLOC_VAL =   { "urn:ietf:params:xml:ns:im-composing iscomposing.xsd", 51 };


PJ_DEF(pj_xml_node*) pjsip_iscomposing_create_xml( pj_pool_t *pool,
						   pj_bool_t is_composing,
						   const pj_time_val *lst_actv,
						   const pj_str_t *content_tp,
						   int refresh)
{
    pj_xml_node *doc, *node;
    pj_xml_attr *attr;

    /* Root document. */
    doc = pj_xml_node_new(pool, &STR_ISCOMPOSING);

    /* Add attributes */
    attr = pj_xml_attr_new(pool, &STR_XMLNS_NAME, &STR_XMLNS_VAL);
    pj_xml_add_attr(doc, attr);

    attr = pj_xml_attr_new(pool, &STR_XMLNS_XSI_NAME, &STR_XMLNS_XSI_VAL);
    pj_xml_add_attr(doc, attr);

    attr = pj_xml_attr_new(pool, &STR_XSI_SLOC_NAME, &STR_XSI_SLOC_VAL);
    pj_xml_add_attr(doc, attr);


    /* Add state. */
    node = pj_xml_node_new(pool, &STR_STATE);
    if (is_composing)
	node->content = STR_ACTIVE;
    else
	node->content = STR_IDLE;
    pj_xml_add_node(doc, node);

    /* Add lastactive, if any. */
    PJ_UNUSED_ARG(lst_actv);
    //if (!is_composing && lst_actv) {
    //	PJ_TODO(IMPLEMENT_LAST_ACTIVE_ATTRIBUTE);
    //}

    /* Add contenttype, if any. */
    if (content_tp) {
	node = pj_xml_node_new(pool, &STR_CONTENTTYPE);
	pj_strdup(pool, &node->content, content_tp);
	pj_xml_add_node(doc, node);
    }

    /* Add refresh, if any. */
    if (is_composing && refresh > 1 && refresh < 3601) {
	node = pj_xml_node_new(pool, &STR_REFRESH);
	node->content.ptr = (char*) pj_pool_alloc(pool, 10);
	node->content.slen = pj_utoa(refresh, node->content.ptr);
	pj_xml_add_node(doc, node);
    }

    /* Done! */

    return doc;
}



/*
 * Function to print XML message body.
 */
static int xml_print_body( struct pjsip_msg_body *msg_body, 
			   char *buf, pj_size_t size)
{
    return pj_xml_print((const pj_xml_node*)msg_body->data, buf, size, 
    			PJ_TRUE);
}


/*
 * Function to clone XML document.
 */
static void* xml_clone_data(pj_pool_t *pool, const void *data, unsigned len)
{
    PJ_UNUSED_ARG(len);
    return pj_xml_clone( pool, (const pj_xml_node*)data);
}



PJ_DEF(pjsip_msg_body*) pjsip_iscomposing_create_body( pj_pool_t *pool,
						   pj_bool_t is_composing,
						   const pj_time_val *lst_actv,
						   const pj_str_t *content_tp,
						   int refresh)
{
    pj_xml_node *doc;
    pjsip_msg_body *body;

    doc = pjsip_iscomposing_create_xml( pool, is_composing, lst_actv,
					content_tp, refresh);
    if (doc == NULL)
	return NULL;


    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    body->content_type.type = STR_MIME_TYPE;
    body->content_type.subtype = STR_MIME_SUBTYPE;

    body->data = doc;
    body->len = 0;

    body->print_body = &xml_print_body;
    body->clone_data = &xml_clone_data;

    return body;
}


PJ_DEF(pj_status_t) pjsip_iscomposing_parse( int inst_id, 
						 pj_pool_t *pool,
					     char *msg,
					     pj_size_t len,
					     pj_bool_t *p_is_composing,
					     pj_str_t **p_last_active,
					     pj_str_t **p_content_type,
					     int *p_refresh )
{
    pj_xml_node *doc, *node;

    /* Set defaults: */
    if (p_is_composing) *p_is_composing = PJ_FALSE;
    if (p_last_active) *p_last_active = NULL; 
    if (p_content_type) *p_content_type = NULL;

    /* Parse XML */
    doc = pj_xml_parse( inst_id, pool, msg, len);
    if (!doc)
	return PJLIB_UTIL_EINXML;

    /* Root document must be "isComposing" */
    if (pj_stricmp(&doc->name, &STR_ISCOMPOSING) != 0)
	return PJSIP_SIMPLE_EBADISCOMPOSE;

    /* Get the status. */
    if (p_is_composing) {
	node = pj_xml_find_node(doc, &STR_STATE);
	if (node == NULL)
	    return PJSIP_SIMPLE_EBADISCOMPOSE;
	*p_is_composing = (pj_stricmp(&node->content, &STR_ACTIVE)==0);
    }

    /* Get last active. */
    if (p_last_active) {
	node = pj_xml_find_node(doc, &STR_LASTACTIVE);
	if (node)
	    *p_last_active = &node->content;
    }

    /* Get content type */
    if (p_content_type) {
	node = pj_xml_find_node(doc, &STR_CONTENTTYPE);
	if (node)
	    *p_content_type = &node->content;
    }

    /* Get refresh */
    if (p_refresh) {
	node = pj_xml_find_node(doc, &STR_REFRESH);
	if (node)
	    *p_refresh = pj_strtoul(&node->content);
    }

    return PJ_SUCCESS;
}


