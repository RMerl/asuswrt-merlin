/* $Id: multipart_test.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE	""

/*
 * multipart tests
 */
typedef pj_status_t (*verify_ptr)(pj_pool_t*,pjsip_msg_body*);

static pj_status_t verify1(pj_pool_t *pool, pjsip_msg_body *body);

static struct test_t
{
    char *ctype;
    char *csubtype;
    char *boundary;
    const char *msg;
    verify_ptr  verify;
} p_tests[] =
{
	{
		/* Content-type */
		"multipart", "mixed", "12345",

		/* Body: */
		"This is the prolog, which should be ignored.\r\n"
		"--12345\r\n"
		"Content-Type: my/text\r\n"
		"\r\n"
		"Header and body\r\n"
		"--12345 \t\r\n"
		"Content-Type: hello/world\r\n"
		"Content-Length: 0\r\n"
		"\r\n"
		"--12345\r\n"
		"\r\n"
		"Body only\r\n"
		"--12345\r\n"
		"Content-Type: multipart/mixed;boundary=6789\r\n"
		"\r\n"
		"Prolog of the subbody, should be ignored\r\n"
		"--6789\r\n"
		"\r\n"
		"Subbody\r\n"
		"--6789--\r\n"
		"Epilogue of the subbody, should be ignored\r\n"
		"--12345--\r\n"
		"This is epilogue, which should be ignored too",

		&verify1
	}
};

static void init_media_type(pjsip_media_type *mt,
			    char *type, char *subtype, char *boundary)
{
    static pjsip_param prm;

    pjsip_media_type_init(mt, NULL, NULL);
    if (type) mt->type = pj_str(type);
    if (subtype) mt->subtype = pj_str(subtype);
    if (boundary) {
	pj_list_init(&prm);
	prm.name = pj_str("boundary");
	prm.value = pj_str(boundary);
	pj_list_push_back(&mt->param, &prm);
    }
}

static int verify_part(pjsip_multipart_part *part,
		       char *h_content_type,
		       char *h_content_subtype,
		       char *boundary,
		       int h_content_length,
		       const char *body)
{
    pjsip_ctype_hdr *ctype_hdr = NULL;
    pjsip_clen_hdr *clen_hdr = NULL;
    pjsip_hdr *hdr;
    pj_str_t the_body;

    hdr = part->hdr.next;
    while (hdr != &part->hdr) {
	if (hdr->type == PJSIP_H_CONTENT_TYPE)
	    ctype_hdr = (pjsip_ctype_hdr*)hdr;
	else if (hdr->type == PJSIP_H_CONTENT_LENGTH)
	    clen_hdr = (pjsip_clen_hdr*)hdr;
	hdr = hdr->next;
    }

    if (h_content_type) {
	pjsip_media_type mt;

	if (ctype_hdr == NULL)
	    return -10;

	init_media_type(&mt, h_content_type, h_content_subtype, boundary);

	if (pjsip_media_type_cmp(&ctype_hdr->media, &mt, 2) != 0)
	    return -20;

    } else {
	if (ctype_hdr)
	    return -30;
    }

    if (h_content_length >= 0) {
	if (clen_hdr == NULL)
	    return -50;
	if (clen_hdr->len != h_content_length)
	    return -60;
    } else {
	if (clen_hdr)
	    return -70;
    }

    the_body.ptr = (char*)part->body->data;
    the_body.slen = part->body->len;

    if (pj_strcmp2(&the_body, body) != 0)
	return -90;

    return 0;
}

static pj_status_t verify1(pj_pool_t *pool, pjsip_msg_body *body)
{
    pjsip_media_type mt;
    pjsip_multipart_part *part;
    int rc;

    PJ_UNUSED_ARG(pool);

    /* Check content-type: "multipart/mixed;boundary=12345" */
    init_media_type(&mt, "multipart", "mixed", "12345");
    if (pjsip_media_type_cmp(&body->content_type, &mt, 2) != 0)
	return -200;

    /* First part:
		"Content-Type: my/text\r\n"
		"\r\n"
		"Header and body\r\n"
     */
    part = pjsip_multipart_get_first_part(body);
    if (!part)
	return -210;
    if (verify_part(part, "my", "text", NULL, -1, "Header and body"))
	return -220;

    /* Next part:
		"Content-Type: hello/world\r\n"
		"Content-Length: 0\r\n"
		"\r\n"
     */
    part = pjsip_multipart_get_next_part(body, part);
    if (!part)
	return -230;
    if ((rc=verify_part(part, "hello", "world", NULL, 0, ""))!=0) {
	PJ_LOG(3,(THIS_FILE, "   err: verify_part rc=%d", rc));
	return -240;
    }

    /* Next part:
		"\r\n"
		"Body only\r\n"
     */
    part = pjsip_multipart_get_next_part(body, part);
    if (!part)
	return -260;
    if (verify_part(part, NULL, NULL, NULL, -1, "Body only"))
	return -270;

    /* Next part:
		"Content-Type: multipart/mixed;boundary=6789\r\n"
		"\r\n"
		"Prolog of the subbody, should be ignored\r\n"
		"--6789\r\n"
		"\r\n"
		"Subbody\r\n"
		"--6789--\r\n"
		"Epilogue of the subbody, should be ignored\r\n"

     */
    part = pjsip_multipart_get_next_part(body, part);
    if (!part)
	return -280;
    if ((rc=verify_part(part, "multipart", "mixed", "6789", -1,
	        "Prolog of the subbody, should be ignored\r\n"
		"--6789\r\n"
		"\r\n"
		"Subbody\r\n"
		"--6789--\r\n"
		"Epilogue of the subbody, should be ignored"))!=0) {
	PJ_LOG(3,(THIS_FILE, "   err: verify_part rc=%d", rc));
	return -290;
    }

    return 0;
}

static int parse_test(void)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(p_tests); ++i) {
	pj_pool_t *pool;
	pjsip_media_type ctype;
	pjsip_msg_body *body;
	pj_str_t str;
	int rc;

	pool = pjsip_endpt_create_pool(endpt, NULL, 512, 512);

	init_media_type(&ctype, p_tests[i].ctype, p_tests[i].csubtype,
			p_tests[i].boundary);

	pj_strdup2_with_null(pool, &str, p_tests[i].msg);
	body = pjsip_multipart_parse(0, pool, str.ptr, str.slen, &ctype, 0);
	if (!body)
	    return -100;

	if (p_tests[i].verify) {
	    rc = p_tests[i].verify(pool, body);
	} else {
	    rc = 0;
	}

	pj_pool_release(pool);
	if (rc)
	    return rc;
    }

    return 0;
}

int multipart_test(void)
{
    int rc;

    rc = parse_test();
    if (rc)
	return rc;

    return rc;
}

