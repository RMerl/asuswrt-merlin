/* $Id: rpid.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/rpid.h>
#include <pjsip-simple/errno.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/string.h>


static const pj_str_t DM_NAME = {"xmlns:dm", 8};
static const pj_str_t DM_VAL = {"urn:ietf:params:xml:ns:pidf:data-model", 38};
static const pj_str_t RPID_NAME = {"xmlns:rpid", 10};
static const pj_str_t RPID_VAL = {"urn:ietf:params:xml:ns:pidf:rpid", 32};

static const pj_str_t DM_NOTE = {"dm:note", 7};
static const pj_str_t DM_PERSON = {"dm:person", 9};
static const pj_str_t ID = {"id", 2};
static const pj_str_t NOTE = {"note", 4};
static const pj_str_t RPID_ACTIVITIES = {"rpid:activities", 15};
static const pj_str_t RPID_AWAY = {"rpid:away", 9};
static const pj_str_t RPID_BUSY = {"rpid:busy", 9};
static const pj_str_t RPID_UNKNOWN = {"rpid:unknown", 12};


/* Duplicate RPID element */
PJ_DEF(void) pjrpid_element_dup(pj_pool_t *pool, pjrpid_element *dst,
				const pjrpid_element *src)
{
    pj_memcpy(dst, src, sizeof(pjrpid_element));
    pj_strdup(pool, &dst->id, &src->id);
    pj_strdup(pool, &dst->note, &src->note);
}


/* Update RPID namespaces. */
static void update_namespaces(pjpidf_pres *pres,
			      pj_pool_t *pool)
{
    /* Check if namespace is already present. */
    if (pj_xml_find_attr(pres, &DM_NAME, NULL) != NULL)
	return;

    pj_xml_add_attr(pres, pj_xml_attr_new(pool, &DM_NAME, &DM_VAL));
    pj_xml_add_attr(pres, pj_xml_attr_new(pool, &RPID_NAME, &RPID_VAL));
}


/* Comparison function to find node name substring */
static pj_bool_t substring_match(const pj_xml_node *node, 
				 const char *part_name,
				 int part_len)
{
    pj_str_t end_name;

    if (part_len < 1)
	part_len = pj_ansi_strlen(part_name);

    if (node->name.slen < part_len)
	return PJ_FALSE;

    end_name.ptr = node->name.ptr + (node->name.slen - part_len);
    end_name.slen = part_len;

    return pj_strnicmp2(&end_name, part_name, part_len)==0;
}

/* Util to find child node with the specified substring */
static pj_xml_node *find_node(const pj_xml_node *parent, 
			      const char *part_name)
{
    const pj_xml_node *node = parent->node_head.next, 
		      *head = (pj_xml_node*) &parent->node_head;
    int part_len = pj_ansi_strlen(part_name);

    while (node != head) {
	if (substring_match(node, part_name, part_len))
	    return (pj_xml_node*) node;

	node = node->next;
    }

    return NULL;
}

/*
 * Add RPID element into existing PIDF document.
 */
PJ_DEF(pj_status_t) pjrpid_add_element(pjpidf_pres *pres, 
				       pj_pool_t *pool,
				       unsigned options,
				       const pjrpid_element *elem)
{
    pj_xml_node *nd_person, *nd_activities, *nd_activity, *nd_note;
    pj_xml_attr *attr;

    PJ_ASSERT_RETURN(pres && pool && options==0 && elem, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    /* Check if we need to add RPID information into the PIDF document. */
    if (elem->id.slen==0 && 
	elem->activity==PJRPID_ACTIVITY_UNKNOWN &&
	elem->note.slen==0)
    {
	/* No RPID information to be added. */
	return PJ_SUCCESS;
    }

    /* Add <note> to <tuple> */
    if (elem->note.slen != 0) {
	pj_xml_node *nd_tuple;

	nd_tuple = find_node(pres, "tuple");

	if (nd_tuple) {
	    nd_note = pj_xml_node_new(pool, &NOTE);
	    pj_strdup(pool, &nd_note->content, &elem->note);
	    pj_xml_add_node(nd_tuple, nd_note);
	    nd_note = NULL;
	}
    }

    /* Update namespace */
    update_namespaces(pres, pool);

    /* Add <person> */
    nd_person = pj_xml_node_new(pool, &DM_PERSON);
    if (elem->id.slen != 0) {
	attr = pj_xml_attr_new(pool, &ID, &elem->id);
    } else {
	pj_str_t person_id;
	/* xs:ID must start with letter */
	//pj_create_unique_string(pool, &person_id);
	person_id.ptr = (char*)pj_pool_alloc(pool, PJ_GUID_STRING_LENGTH+2);
	person_id.ptr += 2;
	pj_generate_unique_string(pool->factory->inst_id, &person_id);
	person_id.ptr -= 2;
	person_id.ptr[0] = 'p';
	person_id.ptr[1] = 'j';
	person_id.slen += 2;

	attr = pj_xml_attr_new(pool, &ID, &person_id);
    }
    pj_xml_add_attr(nd_person, attr);
    pj_xml_add_node(pres, nd_person);

    /* Add <activities> */
    nd_activities = pj_xml_node_new(pool, &RPID_ACTIVITIES);
    pj_xml_add_node(nd_person, nd_activities);

    /* Add the activity */
    switch (elem->activity) {
    case PJRPID_ACTIVITY_AWAY:
	nd_activity = pj_xml_node_new(pool, &RPID_AWAY);
	break;
    case PJRPID_ACTIVITY_BUSY:
	nd_activity = pj_xml_node_new(pool, &RPID_BUSY);
	break;
    case PJRPID_ACTIVITY_UNKNOWN:
    default:
	nd_activity = pj_xml_node_new(pool, &RPID_UNKNOWN);
	break;
    }
    pj_xml_add_node(nd_activities, nd_activity);

    /* Add custom text if required. */
    if (elem->note.slen != 0) {
	nd_note = pj_xml_node_new(pool, &DM_NOTE);
	pj_strdup(pool, &nd_note->content, &elem->note);
	pj_xml_add_node(nd_person, nd_note);
    }

    /* Done */
    return PJ_SUCCESS;
}


/* Get <note> element from PIDF <tuple> element */
static pj_status_t get_tuple_note(const pjpidf_pres *pres,
				  pj_pool_t *pool,
				  pjrpid_element *elem)
{
    const pj_xml_node *nd_tuple, *nd_note;

    nd_tuple = find_node(pres, "tuple");
    if (!nd_tuple)
	return PJSIP_SIMPLE_EBADRPID;

    nd_note = find_node(pres, "note");
    if (nd_note) {
	pj_strdup(pool, &elem->note, &nd_note->content);
	return PJ_SUCCESS;
    }

    return PJSIP_SIMPLE_EBADRPID;
}

/*
 * Get RPID element from PIDF document, if any.
 */
PJ_DEF(pj_status_t) pjrpid_get_element(const pjpidf_pres *pres,
				       pj_pool_t *pool,
				       pjrpid_element *elem)
{
    const pj_xml_node *nd_person, *nd_activities, *nd_note = NULL;
    const pj_xml_attr *attr;

    /* Reset */
    pj_bzero(elem, sizeof(*elem));
    elem->activity = PJRPID_ACTIVITY_UNKNOWN;

    /* Find <person> */
    nd_person = find_node(pres, "person");
    if (!nd_person) {
	/* <person> not found, try to get <note> from <tuple> */
	return get_tuple_note(pres, pool, elem);
    }

    /* Get element id attribute */
    attr = pj_xml_find_attr((pj_xml_node*)nd_person, &ID, NULL);
    if (attr)
	pj_strdup(pool, &elem->id, &attr->value);

    /* Get <activities> */
    nd_activities = find_node(nd_person, "activities");
    if (nd_activities) {
	const pj_xml_node *nd_activity;

	/* Try to get <note> from <activities> */
	nd_note = find_node(nd_activities, "note");

	/* Get the activity */
	nd_activity = nd_activities->node_head.next;
	if (nd_activity == nd_note)
	    nd_activity = nd_activity->next;

	if (nd_activity != (pj_xml_node*) &nd_activities->node_head) {
	    if (substring_match(nd_activity, "busy", -1))
		elem->activity = PJRPID_ACTIVITY_BUSY;
	    else if (substring_match(nd_activity, "away", -1))
		elem->activity = PJRPID_ACTIVITY_AWAY;
	    else
		elem->activity = PJRPID_ACTIVITY_UNKNOWN;

	}
    }

    /* If <note> is not found, get <note> from <person> */
    if (nd_note == NULL)
	nd_note = find_node(nd_person, "note");

    if (nd_note) {
	pj_strdup(pool, &elem->note, &nd_note->content);
    } else {
	get_tuple_note(pres, pool, elem);
    }

    return PJ_SUCCESS;
}


