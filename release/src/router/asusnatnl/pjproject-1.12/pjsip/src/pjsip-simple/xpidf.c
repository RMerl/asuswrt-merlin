/* $Id: xpidf.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/xpidf.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/string.h>

static pj_str_t STR_PRESENCE = { "presence", 8 };
static pj_str_t STR_STATUS = { "status", 6 };
static pj_str_t STR_OPEN = { "open", 4 };
static pj_str_t STR_CLOSED = { "closed", 6 };
static pj_str_t STR_URI = { "uri", 3 };
static pj_str_t STR_ATOM = { "atom", 4 };
static pj_str_t STR_ATOMID = { "atomid", 6 };
static pj_str_t STR_ID = { "id", 2 };
static pj_str_t STR_ADDRESS = { "address", 7 };
static pj_str_t STR_SUBSCRIBE_PARAM = { ";method=SUBSCRIBE", 17 };
static pj_str_t STR_PRESENTITY = { "presentity", 10 };
static pj_str_t STR_EMPTY_STRING = { NULL, 0 };

static pj_xml_node* xml_create_node(pj_pool_t *pool, 
				    pj_str_t *name, const pj_str_t *value)
{
    pj_xml_node *node;

    node = PJ_POOL_ALLOC_T(pool, pj_xml_node);
    pj_list_init(&node->attr_head);
    pj_list_init(&node->node_head);
    node->name = *name;
    if (value) pj_strdup(pool, &node->content, value);
    else node->content.ptr=NULL, node->content.slen=0;

    return node;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name,
				    const pj_str_t *value)
{
    pj_xml_attr *attr = PJ_POOL_ALLOC_T(pool, pj_xml_attr);
    attr->name = *name;
    pj_strdup(pool, &attr->value, value);
    return attr;
}


PJ_DEF(pjxpidf_pres*) pjxpidf_create(pj_pool_t *pool, const pj_str_t *uri_cstr)
{
    pjxpidf_pres *pres;
    pj_xml_node *presentity;
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;
    pj_str_t uri;
    pj_str_t tmp;

    /* <presence> */
    pres = xml_create_node(pool, &STR_PRESENCE, NULL);

    /* <presentity> */
    presentity = xml_create_node(pool, &STR_PRESENTITY, NULL);
    pj_xml_add_node(pres, presentity);

    /* uri attribute */
    uri.ptr = (char*) pj_pool_alloc(pool, uri_cstr->slen + 
    					   STR_SUBSCRIBE_PARAM.slen);
    pj_strcpy( &uri, uri_cstr);
    pj_strcat( &uri, &STR_SUBSCRIBE_PARAM);
    attr = xml_create_attr(pool, &STR_URI, &uri);
    pj_xml_add_attr(presentity, attr);

    /* <atom> */
    atom = xml_create_node(pool, &STR_ATOM, NULL);
    pj_xml_add_node(pres, atom);

    /* atom id */
    pj_create_unique_string(pool, &tmp);
    attr = xml_create_attr(pool, &STR_ATOMID, &tmp);
    pj_xml_add_attr(atom, attr);

    /* address */
    addr = xml_create_node(pool, &STR_ADDRESS, NULL);
    pj_xml_add_node(atom, addr);

    /* address'es uri */
    attr = xml_create_attr(pool, &STR_URI, uri_cstr);
    pj_xml_add_attr(addr, attr);

    /* status */
    status = xml_create_node(pool, &STR_STATUS, NULL);
    pj_xml_add_node(addr, status);

    /* status attr */
    attr = xml_create_attr(pool, &STR_STATUS, &STR_OPEN);
    pj_xml_add_attr(status, attr);

    return pres;
}   



PJ_DEF(pjxpidf_pres*) pjxpidf_parse(int inst_id, pj_pool_t *pool, char *text, pj_size_t len)
{
    pjxpidf_pres *pres;
    pj_xml_node *node;

    pres = pj_xml_parse(inst_id, pool, text, len);
    if (!pres)
	return NULL;

    /* Validate <presence> */
    if (pj_stricmp(&pres->name, &STR_PRESENCE) != 0)
	return NULL;

    /* Validate <presentity> */
    node = pj_xml_find_node(pres, &STR_PRESENTITY);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &STR_URI, NULL) == NULL)
	return NULL;

    /* Validate <atom> */
    node = pj_xml_find_node(pres, &STR_ATOM);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &STR_ATOMID, NULL) == NULL && 
	pj_xml_find_attr(node, &STR_ID, NULL) == NULL)
    {
	return NULL;
    }

    /* Address */
    node = pj_xml_find_node(node, &STR_ADDRESS);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &STR_URI, NULL) == NULL)
	return NULL;


    /* Status */
    node = pj_xml_find_node(node, &STR_STATUS);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &STR_STATUS, NULL) == NULL)
	return NULL;

    return pres;
}


PJ_DEF(int) pjxpidf_print( pjxpidf_pres *pres, char *text, pj_size_t len)
{
    return pj_xml_print(pres, text, len, PJ_TRUE);
}


PJ_DEF(pj_str_t*) pjxpidf_get_uri(pjxpidf_pres *pres)
{
    pj_xml_node *presentity;
    pj_xml_attr *attr;

    presentity = pj_xml_find_node(pres, &STR_PRESENTITY);
    if (!presentity)
	return &STR_EMPTY_STRING;

    attr = pj_xml_find_attr(presentity, &STR_URI, NULL);
    if (!attr)
	return &STR_EMPTY_STRING;

    return &attr->value;
}


PJ_DEF(pj_status_t) pjxpidf_set_uri(pj_pool_t *pool, pjxpidf_pres *pres, 
				    const pj_str_t *uri)
{
    pj_xml_node *presentity;
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_attr *attr;
    pj_str_t dup_uri;

    presentity = pj_xml_find_node(pres, &STR_PRESENTITY);
    if (!presentity) {
	pj_assert(0);
	return -1;
    }
    atom = pj_xml_find_node(pres, &STR_ATOM);
    if (!atom) {
	pj_assert(0);
	return -1;
    }
    addr = pj_xml_find_node(atom, &STR_ADDRESS);
    if (!addr) {
	pj_assert(0);
	return -1;
    }

    /* Set uri in presentity */
    attr = pj_xml_find_attr(presentity, &STR_URI, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }
    pj_strdup(pool, &dup_uri, uri);
    attr->value = dup_uri;

    /* Set uri in address. */
    attr = pj_xml_find_attr(addr, &STR_URI, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }
    attr->value = dup_uri;

    return 0;
}


PJ_DEF(pj_bool_t) pjxpidf_get_status(pjxpidf_pres *pres)
{
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;

    atom = pj_xml_find_node(pres, &STR_ATOM);
    if (!atom) {
	pj_assert(0);
	return PJ_FALSE;
    }
    addr = pj_xml_find_node(atom, &STR_ADDRESS);
    if (!addr) {
	pj_assert(0);
	return PJ_FALSE;
    }
    status = pj_xml_find_node(addr, &STR_STATUS);
    if (!status) {
	pj_assert(0);
	return PJ_FALSE;
    }
    attr = pj_xml_find_attr(status, &STR_STATUS, NULL);
    if (!attr) {
	pj_assert(0);
	return PJ_FALSE;
    }

    return pj_stricmp(&attr->value, &STR_OPEN)==0 ? PJ_TRUE : PJ_FALSE;
}


PJ_DEF(pj_status_t) pjxpidf_set_status(pjxpidf_pres *pres, pj_bool_t online_status)
{
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;

    atom = pj_xml_find_node(pres, &STR_ATOM);
    if (!atom) {
	pj_assert(0);
	return -1;
    }
    addr = pj_xml_find_node(atom, &STR_ADDRESS);
    if (!addr) {
	pj_assert(0);
	return -1;
    }
    status = pj_xml_find_node(addr, &STR_STATUS);
    if (!status) {
	pj_assert(0);
	return -1;
    }
    attr = pj_xml_find_attr(status, &STR_STATUS, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }

    attr->value = ( online_status ? STR_OPEN : STR_CLOSED );
    return 0;
}

