/* $Id: presence_body.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/presence.h>
#include <pjsip-simple/errno.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_transport.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE   "presence_body.c"


static const pj_str_t STR_APPLICATION = { "application", 11 };
static const pj_str_t STR_PIDF_XML =	{ "pidf+xml", 8 };
static const pj_str_t STR_XPIDF_XML =	{ "xpidf+xml", 9 };




/*
 * Function to print XML message body.
 */
static int pres_print_body(struct pjsip_msg_body *msg_body, 
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
    return pj_xml_clone( pool, (const pj_xml_node*) data);
}


/*
 * This is a utility function to create PIDF message body from PJSIP
 * presence status (pjsip_pres_status).
 */
PJ_DEF(pj_status_t) pjsip_pres_create_pidf( pj_pool_t *pool,
					    const pjsip_pres_status *status,
					    const pj_str_t *entity,
					    pjsip_msg_body **p_body )
{
    pjpidf_pres *pidf;
    pjsip_msg_body *body;
    unsigned i;

    /* Create <presence>. */
    pidf = pjpidf_create(pool, entity);

    /* Create <tuple> */
    for (i=0; i<status->info_cnt; ++i) {

	pjpidf_tuple *pidf_tuple;
	pjpidf_status *pidf_status;
	pj_str_t id;

	/* Add tuple id. */
	if (status->info[i].id.slen == 0) {
	    /* xs:ID must start with letter */
	    //pj_create_unique_string(pool, &id);
	    id.ptr = (char*)pj_pool_alloc(pool, PJ_GUID_STRING_LENGTH+2);
	    id.ptr += 2;
	    pj_generate_unique_string(pool->factory->inst_id, &id);
	    id.ptr -= 2;
	    id.ptr[0] = 'p';
	    id.ptr[1] = 'j';
	    id.slen += 2;
	} else {
	    id = status->info[i].id;
	}

	pidf_tuple = pjpidf_pres_add_tuple(pool, pidf, &id);

	/* Set <contact> */
	if (status->info[i].contact.slen)
	    pjpidf_tuple_set_contact(pool, pidf_tuple, 
				     &status->info[i].contact);


	/* Set basic status */
	pidf_status = pjpidf_tuple_get_status(pidf_tuple);
	pjpidf_status_set_basic_open(pidf_status, 
				     status->info[i].basic_open);

	/* Add <timestamp> if configured */
#if defined(PJSIP_PRES_PIDF_ADD_TIMESTAMP) && PJSIP_PRES_PIDF_ADD_TIMESTAMP
	if (PJSIP_PRES_PIDF_ADD_TIMESTAMP) {
	  char buf[50];
	  int tslen = 0;
	  pj_time_val tv;
	  pj_parsed_time pt;

	  pj_gettimeofday(&tv);
	  /* TODO: convert time to GMT! (unsupported by pjlib) */
	  pj_time_decode( &tv, &pt);

	  tslen = pj_ansi_snprintf(buf, sizeof(buf),
				   "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
				   pt.year, pt.mon+1, pt.day, 
				   pt.hour, pt.min, pt.sec, pt.msec);
	  if (tslen > 0 && tslen < (int)sizeof(buf)) {
	      pj_str_t time = pj_str(buf);
	      pjpidf_tuple_set_timestamp(pool, pidf_tuple, &time);
	  }
	}
#endif
    }

    /* Create <person> (RPID) */
    if (status->info_cnt) {
	pjrpid_add_element(pidf, pool, 0, &status->info[0].rpid);
    }

    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    body->data = pidf;
    body->content_type.type = STR_APPLICATION;
    body->content_type.subtype = STR_PIDF_XML;
    body->print_body = &pres_print_body;
    body->clone_data = &xml_clone_data;

    *p_body = body;

    return PJ_SUCCESS;    
}


/*
 * This is a utility function to create X-PIDF message body from PJSIP
 * presence status (pjsip_pres_status).
 */
PJ_DEF(pj_status_t) pjsip_pres_create_xpidf( pj_pool_t *pool,
					     const pjsip_pres_status *status,
					     const pj_str_t *entity,
					     pjsip_msg_body **p_body )
{
    /* Note: PJSIP implementation of XPIDF is not complete!
     */
    pjxpidf_pres *xpidf;
    pjsip_msg_body *body;

    PJ_LOG(4,(THIS_FILE, "Warning: XPIDF format is not fully supported "
			 "by PJSIP"));

    /* Create XPIDF document. */
    xpidf = pjxpidf_create(pool, entity);

    /* Set basic status. */
    if (status->info_cnt > 0)
	pjxpidf_set_status( xpidf, status->info[0].basic_open);
    else
	pjxpidf_set_status( xpidf, PJ_FALSE);

    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    body->data = xpidf;
    body->content_type.type = STR_APPLICATION;
    body->content_type.subtype = STR_XPIDF_XML;
    body->print_body = &pres_print_body;
    body->clone_data = &xml_clone_data;

    *p_body = body;

    return PJ_SUCCESS;
}



/*
 * This is a utility function to parse PIDF body into PJSIP presence status.
 */
PJ_DEF(pj_status_t) pjsip_pres_parse_pidf( int inst_id, pjsip_rx_data *rdata,
					   pj_pool_t *pool,
					   pjsip_pres_status *pres_status)
{
    return pjsip_pres_parse_pidf2(inst_id, (char*)rdata->msg_info.msg->body->data,
				  rdata->msg_info.msg->body->len,
				  pool, pres_status);
}

PJ_DEF(pj_status_t) pjsip_pres_parse_pidf2(int inst_id, char *body, unsigned body_len,
					   pj_pool_t *pool,
					   pjsip_pres_status *pres_status)
{
    pjpidf_pres *pidf;
    pjpidf_tuple *pidf_tuple;

    pidf = pjpidf_parse(inst_id, pool, body, body_len);
    if (pidf == NULL)
	return PJSIP_SIMPLE_EBADPIDF;

    pres_status->info_cnt = 0;

    pidf_tuple = pjpidf_pres_get_first_tuple(pidf);
    while (pidf_tuple && pres_status->info_cnt < PJSIP_PRES_STATUS_MAX_INFO) {
	pjpidf_status *pidf_status;

	pres_status->info[pres_status->info_cnt].tuple_node = 
	    pj_xml_clone(pool, pidf_tuple);

	pj_strdup(pool, 
		  &pres_status->info[pres_status->info_cnt].id,
		  pjpidf_tuple_get_id(pidf_tuple));

	pj_strdup(pool, 
		  &pres_status->info[pres_status->info_cnt].contact,
		  pjpidf_tuple_get_contact(pidf_tuple));

	pidf_status = pjpidf_tuple_get_status(pidf_tuple);
	if (pidf_status) {
	    pres_status->info[pres_status->info_cnt].basic_open = 
		pjpidf_status_is_basic_open(pidf_status);
	} else {
	    pres_status->info[pres_status->info_cnt].basic_open = PJ_FALSE;
	}

	pidf_tuple = pjpidf_pres_get_next_tuple( pidf, pidf_tuple );
	pres_status->info_cnt++;
    }

    /* Parse <person> (RPID) */
    pjrpid_get_element(pidf, pool, &pres_status->info[0].rpid);

    return PJ_SUCCESS;
}


/*
 * This is a utility function to parse X-PIDF body into PJSIP presence status.
 */
PJ_DEF(pj_status_t) pjsip_pres_parse_xpidf(int inst_id, pjsip_rx_data *rdata,
					   pj_pool_t *pool,
					   pjsip_pres_status *pres_status)
{
    return pjsip_pres_parse_xpidf2(inst_id, (char*)rdata->msg_info.msg->body->data,
				   rdata->msg_info.msg->body->len,
				   pool, pres_status);
}

PJ_DEF(pj_status_t) pjsip_pres_parse_xpidf2(int inst_id, char *body, unsigned body_len,
					    pj_pool_t *pool,
					    pjsip_pres_status *pres_status)
{
    pjxpidf_pres *xpidf;

    xpidf = pjxpidf_parse(inst_id, pool, body, body_len);
    if (xpidf == NULL)
	return PJSIP_SIMPLE_EBADXPIDF;

    pres_status->info_cnt = 1;
    
    pj_strdup(pool,
	      &pres_status->info[0].contact,
	      pjxpidf_get_uri(xpidf));
    pres_status->info[0].basic_open = pjxpidf_get_status(xpidf);
    pres_status->info[0].id.slen = 0;
    pres_status->info[0].tuple_node = NULL;

    return PJ_SUCCESS;
}


