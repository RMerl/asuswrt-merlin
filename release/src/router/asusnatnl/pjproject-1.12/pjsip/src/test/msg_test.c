/* $Id: msg_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define POOL_SIZE	8000
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
#   define LOOP		10000
#else
#   define LOOP		100000
#endif
#define AVERAGE_MSG_LEN	800
#define THIS_FILE	"msg_test.c"

static pjsip_msg *create_msg0(pj_pool_t *pool);
static pjsip_msg *create_msg1(pj_pool_t *pool);

#define STATUS_PARTIAL		1
#define STATUS_SYNTAX_ERROR	2

#define FLAG_DETECT_ONLY	1
#define FLAG_PARSE_ONLY		4
#define FLAG_PRINT_ONLY		8

struct test_msg
{
    char	 msg[1024];
    pjsip_msg *(*creator)(pj_pool_t *pool);
    pj_size_t	 len;
    int		 expected_status;
} test_array[] = 
{
{
    /* 'Normal' message with all headers. */
    "INVITE sip:user@foo SIP/2.0\n"
    "from: Hi I'm Joe <sip:joe.user@bar.otherdomain.com>;tag=123457890123456\r"
    "To: Fellow User <sip:user@foo.bar.domain.com>\r\n"
    "Call-ID: 12345678901234567890@bar\r\n"
    "Content-Length: 0\r\n"
    "CSeq: 123456 INVITE\n"
    "Contact: <sip:joe@bar> ; q=0.5;expires=3600,sip:user@host;q=0.500\r"
    "  ,sip:user2@host2\n"
    "Content-Type: text/html ; charset=ISO-8859-4\r"
    "Route: <sip:bigbox3.site3.atlanta.com;lr>,\r\n"
    "  <sip:server10.biloxi.com;lr>\r"
    "Record-Route: <sip:server10.biloxi.com>,\r\n" /* multiple routes+folding*/
    "  <sip:bigbox3.site3.atlanta.com;lr>\n"
    "v: SIP/2.0/SCTP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c230\n"
    "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\n" /* folding. */
    " ;received=192.0.2.1\r\n"
    "Via: SIP/2.0/UDP 10.2.1.1, SIP/2.0/TCP 192.168.1.1\n"
    "Organization: \r"
    "Max-Forwards: 70\n"
    "X-Header: \r\n"	    /* empty header */
    "P-Associated-URI:\r\n" /* empty header without space */
    "\r\n",
    &create_msg0,
    0,
    PJ_SUCCESS
},
{
    /* Typical response message. */
    "SIP/2.0 200 OK\r\n"
    "Via: SIP/2.0/SCTP server10.biloxi.com;branch=z9hG4bKnashds8;rport;received=192.0.2.1\r\n"
    "Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1;received=192.0.2.2\r\n"
    "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds ;received=192.0.2.3\r\n"
    "Route: <sip:proxy.sipprovider.com>\r\n"
    "Route: <sip:proxy.supersip.com:5060>\r\n"
    "Max-Forwards: 70\r\n"
    "To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
    "From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
    "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n"
    "CSeq: 314159 INVITE\r\n"
    "Contact: <sips:bob@192.0.2.4>\r\n"
    "Content-Type: application/sdp\r\n"
    "Content-Length: 150\r\n"
    "\r\n"
    "v=0\r\n"
    "o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "c=IN IP4 pc33.atlanta.com\r\n"
    "m=audio 3456 RTP/AVP 0 1 3 99\r\n"
    "a=rtpmap:0 PCMU/8000\r\n",
    &create_msg1,
    0,
    PJ_SUCCESS
},
{
    /* Torture message from RFC 4475
     * 3.1.1.1 A short tortuous INVITE
     */
    "INVITE sip:vivekg@chair-dnrc.example.com;unknownparam SIP/2.0\n"
    "TO :\n"
    " sip:vivekg@chair-dnrc.example.com ;   tag    = 1918181833n\n"
    "from   : \"J Rosenberg \\\\\\\"\"       <sip:jdrosen@example.com>\n"
    "  ;\n"
    "  tag = 98asjd8\n"
    "MaX-fOrWaRdS: 0068\n"
    "Call-ID: wsinv.ndaksdj@192.0.2.1\n"
    "Content-Length   : 150\n"
    "cseq: 0009\n"
    "  INVITE\n"
    "Via  : SIP  /   2.0\n"
    " /UDP\n"
    "    192.0.2.2;rport;branch=390skdjuw\n"
    "s :\n"
    "NewFangledHeader:   newfangled value\n"
    " continued newfangled value\n"
    "UnknownHeaderWithUnusualValue: ;;,,;;,;\n"
    "Content-Type: application/sdp\n"
    "Route:\n"
    " <sip:services.example.com;lr;unknownwith=value;unknown-no-value>\n"
    "v:  SIP  / 2.0  / TCP     spindle.example.com   ;\n"
    "  branch  =   z9hG4bK9ikj8  ,\n"
    " SIP  /    2.0   / UDP  192.168.255.111   ; branch=\n"
    " z9hG4bK30239\n"
    "m:\"Quoted string \\\"\\\"\" <sip:jdrosen@example.com> ; newparam =\n"
    "      newvalue ;\n"
    "  secondparam ; q = 0.33\r\n"
    "\r\n"
    "v=0\r\n"
    "o=mhandley 29739 7272939 IN IP4 192.0.2.3\r\n"
    "s=-\r\n"
    "c=IN IP4 192.0.2.4\r\n"
    "t=0 0\r\n"
    "m=audio 49217 RTP/AVP 0 12\r\n"
    "m=video 3227 RTP/AVP 31\r\n"
    "a=rtpmap:31 LPC\r\n",
    NULL,
    0,
    PJ_SUCCESS
},
{
    /* Torture message from RFC 4475
     * 3.1.1.2 Wide Range of Valid Characters
     */
    "!interesting-Method0123456789_*+`.%indeed'~ sip:1_unusual.URI~(to-be!sure)&isn't+it$/crazy?,/;;*:&it+has=1,weird!*pas$wo~d_too.(doesn't-it)@example.com SIP/2.0\n"
    "Via: SIP/2.0/UDP host1.example.com;rport;branch=z9hG4bK-.!%66*_+`'~\n"
    "To: \"BEL:\\\x07 NUL:\\\x00 DEL:\\\x7F\" <sip:1_unusual.URI~(to-be!sure)&isn't+it$/crazy?,/;;*@example.com>\n"
    "From: token1~` token2'+_ token3*%!.- <sip:mundane@example.com> ;fromParam''~+*_!.-%=\"\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xB0\xD1\x8E\xD1\x89\xD0\xB8\xD0\xB9\";tag=_token~1'+`*%!-.\n"
    "Call-ID: intmeth.word%ZK-!.*_+'@word`~)(><:\\/\"][?}{\n"
    "CSeq: 139122385 !interesting-Method0123456789_*+`.%indeed'~\n"
    "Max-Forwards: 255\n"
    "extensionHeader-!.%*+_`'~: \xEF\xBB\xBF\xE5\xA4\xA7\xE5\x81\x9C\xE9\x9B\xBB\n"
    "Content-Length: 0\r\n\r\n",
    NULL,
    641,
    PJ_SUCCESS
},
{
    /* Torture message from RFC 4475
     * 3.1.1.3 Valid Use of the % Escaping Mechanism
     */
    "INVITE sip:sips%3Auser%40example.com@example.net SIP/2.0\n"
    "To: sip:%75se%72@example.com\n"
    "From: <sip:I%20have%20spaces@example.net>;tag=1234\n"
    "Max-Forwards: 87\n"
    "i: esc01.239409asdfakjkn23onasd0-3234\n"
    "CSeq: 234234 INVITE\n"
    "Via: SIP/2.0/UDP host5.example.net;rport;branch=z9hG4bKkdjuw\n"
    "C: application/sdp\n"
    "Contact:\n"
    "  <sip:cal%6Cer@192.168.0.2:5060;%6C%72;n%61me=v%61lue%25%34%31>\n"
    "Content-Length: 150\r\n"
    "\r\n"
    "v=0\r\n"
    "o=mhandley 29739 7272939 IN IP4 192.0.2.1\r\n"
    "s=-\r\n"
    "c=IN IP4 192.0.2.1\r\n"
    "t=0 0\r\n"
    "m=audio 49217 RTP/AVP 0 12\r\n"
    "m=video 3227 RTP/AVP 31\r\n"
    "a=rtpmap:31 LPC\r\n",
    NULL,
    0,
    PJ_SUCCESS
},
{
    /* Torture message from RFC 4475
     * 3.1.1.4 Escaped Nulls in URIs
     */
    "REGISTER sip:example.com SIP/2.0\r\n"
    "To: sip:null-%00-null@example.com\r\n"
    "From: sip:null-%00-null@example.com;tag=839923423\r\n"
    "Max-Forwards: 70\r\n"
    "Call-ID: escnull.39203ndfvkjdasfkq3w4otrq0adsfdfnavd\r\n"
    "CSeq: 14398234 REGISTER\r\n"
    "Via: SIP/2.0/UDP host5.example.com;rport;branch=z9hG4bKkdjuw\r\n"
    "Contact: <sip:%00@host5.example.com>\r\n"
    "Contact: <sip:%00%00@host5.example.com>\r\n"
    "L:0\r\n"
    "\r\n",
    NULL,
    0,
    PJ_SUCCESS
},
{
    /* Torture message from RFC 4475
     * 3.1.1.5 Use of % When It Is Not an Escape
     */
    "RE%47IST%45R sip:registrar.example.com SIP/2.0\r\n"
    "To: \"%Z%45\" <sip:resource@example.com>\r\n"
    "From: \"%Z%45\" <sip:resource@example.com>;tag=f232jadfj23\r\n"
    "Call-ID: esc02.asdfnqwo34rq23i34jrjasdcnl23nrlknsdf\r\n"
    "Via: SIP/2.0/TCP host.example.com;rport;branch=z9hG4bK209%fzsnel234\r\n"
    "CSeq: 29344 RE%47IST%45R\r\n"
    "Max-Forwards: 70\r\n"
    "Contact: <sip:alias1@host1.example.com>\r\n"
    "C%6Fntact: <sip:alias2@host2.example.com>\r\n"
    "Contact: <sip:alias3@host3.example.com>\r\n"
    "l: 0\r\n"
    "\r\n",
    NULL,
    0,
    PJ_SUCCESS
}
};

static struct
{
    int flag;
    pj_highprec_t detect_len, parse_len, print_len;
    pj_timestamp  detect_time, parse_time, print_time;
} var;

static pj_status_t test_entry( pj_pool_t *pool, struct test_msg *entry )
{
    pjsip_msg *parsed_msg, *ref_msg = NULL;
    static pjsip_msg *print_msg;
    pj_status_t status = PJ_SUCCESS;
    int len;
    pj_str_t str1, str2;
    pjsip_hdr *hdr1, *hdr2;
    pj_timestamp t1, t2;
    pjsip_parser_err_report err_list;
    pj_size_t msg_size;
    char msgbuf1[PJSIP_MAX_PKT_LEN];
    char msgbuf2[PJSIP_MAX_PKT_LEN];
    enum { BUFLEN = 512 };

    if (entry->len==0)
	entry->len = pj_ansi_strlen(entry->msg);

    if (var.flag & FLAG_PARSE_ONLY)
	goto parse_msg;

    if (var.flag & FLAG_PRINT_ONLY) {
	if (print_msg == NULL)
	    print_msg = entry->creator(pool);
	goto print_msg;
    }

    /* Detect message. */
    var.detect_len = var.detect_len + entry->len;
    pj_get_timestamp(&t1);
    status = pjsip_find_msg(0, entry->msg, entry->len, PJ_FALSE, &msg_size);
    if (status != PJ_SUCCESS) {
	if (status!=PJSIP_EPARTIALMSG || 
	    entry->expected_status!=STATUS_PARTIAL)
	{
	    app_perror("   error: unable to detect message", status);
	    return -5;
	}
    }
    if (msg_size != entry->len) {
	PJ_LOG(3,(THIS_FILE, "   error: size mismatch"));
	return -6;
    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&var.detect_time, &t2);

    if (var.flag & FLAG_DETECT_ONLY)
	return PJ_SUCCESS;
    
    /* Parse message. */
parse_msg:
    var.parse_len = var.parse_len + entry->len;
    pj_get_timestamp(&t1);
    pj_list_init(&err_list);
    parsed_msg = pjsip_parse_msg(0, pool, entry->msg, entry->len, &err_list);
    if (parsed_msg == NULL) {
	if (entry->expected_status != STATUS_SYNTAX_ERROR) {
	    status = -10;
	    if (err_list.next != &err_list) {
		PJ_LOG(3,(THIS_FILE, "   Syntax error in line %d col %d",
			      err_list.next->line, err_list.next->col));
	    }
	    goto on_return;
	}
    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&var.parse_time, &t2);

    if ((var.flag & FLAG_PARSE_ONLY) || entry->creator==NULL)
	return PJ_SUCCESS;

    /* Create reference message. */
    ref_msg = entry->creator(pool);

    /* Create buffer for comparison. */
    str1.ptr = (char*)pj_pool_alloc(pool, BUFLEN);
    str2.ptr = (char*)pj_pool_alloc(pool, BUFLEN);

    /* Compare message type. */
    if (parsed_msg->type != ref_msg->type) {
	status = -20;
	goto on_return;
    }

    /* Compare request or status line. */
    if (parsed_msg->type == PJSIP_REQUEST_MSG) {
	pjsip_method *m1 = &parsed_msg->line.req.method;
	pjsip_method *m2 = &ref_msg->line.req.method;

	if (pjsip_method_cmp(m1, m2) != 0) {
	    status = -30;
	    goto on_return;
	}
	status = pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI,
			       parsed_msg->line.req.uri, 
			       ref_msg->line.req.uri);
	if (status != PJ_SUCCESS) {
	    app_perror("   error: request URI mismatch", status);
	    status = -31;
	    goto on_return;
	}
    } else {
	if (parsed_msg->line.status.code != ref_msg->line.status.code) {
	    PJ_LOG(3,(THIS_FILE, "   error: status code mismatch"));
	    status = -32;
	    goto on_return;
	}
	if (pj_strcmp(&parsed_msg->line.status.reason, 
		      &ref_msg->line.status.reason) != 0) 
	{
	    PJ_LOG(3,(THIS_FILE, "   error: status text mismatch"));
	    status = -33;
	    goto on_return;
	}
    }

    /* Compare headers. */
    hdr1 = parsed_msg->hdr.next;
    hdr2 = ref_msg->hdr.next;

    while (hdr1 != &parsed_msg->hdr && hdr2 != &ref_msg->hdr) {
	len = pjsip_hdr_print_on(hdr1, str1.ptr, BUFLEN);
	if (len < 0) {
	    status = -40;
	    goto on_return;
	}
	str1.ptr[len] = '\0';
	str1.slen = len;

	len = pjsip_hdr_print_on(hdr2, str2.ptr, BUFLEN);
	if (len < 0) {
	    status = -50;
	    goto on_return;
	}
	str2.ptr[len] = '\0';
	str2.slen = len;

	if (pj_strcmp(&str1, &str2) != 0) {
	    status = -60;
	    PJ_LOG(3,(THIS_FILE, "   error: header string mismatch:\n"
		          "   h1='%s'\n"
			  "   h2='%s'\n",
			  str1.ptr, str2.ptr));
	    goto on_return;
	}

	hdr1 = hdr1->next;
	hdr2 = hdr2->next;
    }

    if (hdr1 != &parsed_msg->hdr || hdr2 != &ref_msg->hdr) {
	status = -70;
	goto on_return;
    }

    /* Compare body? */
    if (parsed_msg->body==NULL && ref_msg->body==NULL)
	goto print_msg;

    /* Compare msg body length. */
    if (parsed_msg->body->len != ref_msg->body->len) {
	status = -80;
	goto on_return;
    }

    /* Compare msg body content type. */
    if (pj_strcmp(&parsed_msg->body->content_type.type,
	          &ref_msg->body->content_type.type) != 0) {
	status = -90;
	goto on_return;
    }
    if (pj_strcmp(&parsed_msg->body->content_type.subtype,
	          &ref_msg->body->content_type.subtype) != 0) {
	status = -100;
	goto on_return;
    }

    /* Compare body content. */
    str1.slen = parsed_msg->body->print_body(parsed_msg->body,
					     msgbuf1, sizeof(msgbuf1));
    if (str1.slen < 1) {
	status = -110;
	goto on_return;
    }
    str1.ptr = msgbuf1;

    str2.slen = ref_msg->body->print_body(ref_msg->body,
					  msgbuf2, sizeof(msgbuf2));
    if (str2.slen < 1) {
	status = -120;
	goto on_return;
    }
    str2.ptr = msgbuf2;

    if (pj_strcmp(&str1, &str2) != 0) {
	status = -140;
	goto on_return;
    }
    
    /* Print message. */
print_msg:
    var.print_len = var.print_len + entry->len;
    pj_get_timestamp(&t1);
    if (var.flag && FLAG_PRINT_ONLY)
	ref_msg = print_msg;
    len = pjsip_msg_print(ref_msg, msgbuf1, PJSIP_MAX_PKT_LEN);
    if (len < 1) {
	status = -150;
	goto on_return;
    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&var.print_time, &t2);


    status = PJ_SUCCESS;

on_return:
    return status;
}


static pjsip_msg *create_msg0(pj_pool_t *pool)
{

    pjsip_msg *msg;
    pjsip_name_addr *name_addr;
    pjsip_sip_uri *url;
    pjsip_fromto_hdr *fromto;
    pjsip_cid_hdr *cid;
    pjsip_clen_hdr *clen;
    pjsip_cseq_hdr *cseq;
    pjsip_contact_hdr *contact;
    pjsip_ctype_hdr *ctype;
    pjsip_routing_hdr *routing;
    pjsip_via_hdr *via;
    pjsip_generic_string_hdr *generic;
    pjsip_param *prm;
    pj_str_t str;

    msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);

    /* "INVITE sip:user@foo SIP/2.0\n" */
    pjsip_method_set(&msg->line.req.method, PJSIP_INVITE_METHOD);
    url = pjsip_sip_uri_create(pool, 0);
    msg->line.req.uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "user");
    pj_strdup2(pool, &url->host, "foo");

    /* "From: Hi I'm Joe <sip:joe.user@bar.otherdomain.com>;tag=123457890123456\r" */
    fromto = pjsip_from_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)fromto);
    pj_strdup2(pool, &fromto->tag, "123457890123456");
    name_addr = pjsip_name_addr_create(pool);
    fromto->uri = (pjsip_uri*)name_addr;
    pj_strdup2(pool, &name_addr->display, "Hi I'm Joe");
    url = pjsip_sip_uri_create(pool, 0);
    name_addr->uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "joe.user");
    pj_strdup2(pool, &url->host, "bar.otherdomain.com");

    /* "To: Fellow User <sip:user@foo.bar.domain.com>\r\n" */
    fromto = pjsip_to_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)fromto);
    name_addr = pjsip_name_addr_create(pool);
    fromto->uri = (pjsip_uri*)name_addr;
    pj_strdup2(pool, &name_addr->display, "Fellow User");
    url = pjsip_sip_uri_create(pool, 0);
    name_addr->uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "user");
    pj_strdup2(pool, &url->host, "foo.bar.domain.com");

    /* "Call-ID: 12345678901234567890@bar\r\n" */
    cid = pjsip_cid_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)cid);
    pj_strdup2(pool, &cid->id, "12345678901234567890@bar");

    /* "Content-Length: 0\r\n" */
    clen = pjsip_clen_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)clen);
    clen->len = 0;

    /* "CSeq: 123456 INVITE\n" */
    cseq = pjsip_cseq_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)cseq);
    cseq->cseq = 123456;
    pjsip_method_set(&cseq->method, PJSIP_INVITE_METHOD);

    /* "Contact: <sip:joe@bar>;q=0.5;expires=3600*/
    contact = pjsip_contact_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)contact);
    contact->q1000 = 500;
    contact->expires = 3600;
    name_addr = pjsip_name_addr_create(pool);
    contact->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, 0);
    name_addr->uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "joe");
    pj_strdup2(pool, &url->host, "bar");

    /*, sip:user@host;q=0.500\r" */
    contact = pjsip_contact_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)contact);
    contact->q1000 = 500;
    name_addr = pjsip_name_addr_create(pool);
    contact->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, 0);
    name_addr->uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "user");
    pj_strdup2(pool, &url->host, "host");

    /* "  ,sip:user2@host2\n" */
    contact = pjsip_contact_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)contact);
    name_addr = pjsip_name_addr_create(pool);
    contact->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, 0);
    name_addr->uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->user, "user2");
    pj_strdup2(pool, &url->host, "host2");

    /* "Content-Type: text/html; charset=ISO-8859-4\r" */
    ctype = pjsip_ctype_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)ctype);
    pj_strdup2(pool, &ctype->media.type, "text");
    pj_strdup2(pool, &ctype->media.subtype, "html");
    prm = PJ_POOL_ALLOC_T(pool, pjsip_param);
    prm->name = pj_str("charset");
    prm->value = pj_str("ISO-8859-4");
    pj_list_push_back(&ctype->media.param, prm);

    /* "Route: <sip:bigbox3.site3.atlanta.com;lr>,\r\n" */
    routing = pjsip_route_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)routing);
    url = pjsip_sip_uri_create(pool, 0);
    routing->name_addr.uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->host, "bigbox3.site3.atlanta.com");
    url->lr_param = 1;

    /* "  <sip:server10.biloxi.com;lr>\r" */
    routing = pjsip_route_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)routing);
    url = pjsip_sip_uri_create(pool, 0);
    routing->name_addr.uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->host, "server10.biloxi.com");
    url->lr_param = 1;

    /* "Record-Route: <sip:server10.biloxi.com>,\r\n" */
    routing = pjsip_rr_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)routing);
    url = pjsip_sip_uri_create(pool, 0);
    routing->name_addr.uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->host, "server10.biloxi.com");
    url->lr_param = 0;

    /* "  <sip:bigbox3.site3.atlanta.com;lr>\n" */
    routing = pjsip_rr_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)routing);
    url = pjsip_sip_uri_create(pool, 0);
    routing->name_addr.uri = (pjsip_uri*)url;
    pj_strdup2(pool, &url->host, "bigbox3.site3.atlanta.com");
    url->lr_param = 1;

    /* "Via: SIP/2.0/SCTP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c230\n" */
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    pj_strdup2(pool, &via->transport, "SCTP");
    pj_strdup2(pool, &via->sent_by.host, "bigbox3.site3.atlanta.com");
    pj_strdup2(pool, &via->branch_param, "z9hG4bK77ef4c230");

    /* "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\n"
	" ;received=192.0.2.1\r\n" */
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    pj_strdup2(pool, &via->transport, "UDP");
    pj_strdup2(pool, &via->sent_by.host, "pc33.atlanta.com");
    pj_strdup2(pool, &via->branch_param, "z9hG4bKnashds8");
    pj_strdup2(pool, &via->recvd_param, "192.0.2.1");


    /* "Via: SIP/2.0/UDP 10.2.1.1, */ 
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    pj_strdup2(pool, &via->transport, "UDP");
    pj_strdup2(pool, &via->sent_by.host, "10.2.1.1");
    
    
    /*SIP/2.0/TCP 192.168.1.1\n" */
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    pj_strdup2(pool, &via->transport, "TCP");
    pj_strdup2(pool, &via->sent_by.host, "192.168.1.1");

    /* "Organization: \r" */
    str.ptr = "Organization";
    str.slen = 12;
    generic = pjsip_generic_string_hdr_create(pool, &str, NULL);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)generic);
    generic->hvalue.ptr = NULL;
    generic->hvalue.slen = 0;

    /* "Max-Forwards: 70\n" */
    str.ptr = "Max-Forwards";
    str.slen = 12;
    generic = pjsip_generic_string_hdr_create(pool, &str, NULL);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)generic);
    str.ptr = "70";
    str.slen = 2;
    generic->hvalue = str;

    /* "X-Header: \r\n" */
    str.ptr = "X-Header";
    str.slen = 8;
    generic = pjsip_generic_string_hdr_create(pool, &str, NULL);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)generic);
    str.ptr = NULL;
    str.slen = 0;
    generic->hvalue = str;

    /* P-Associated-URI:\r\n */
    str.ptr = "P-Associated-URI";
    str.slen = 16;
    generic = pjsip_generic_string_hdr_create(pool, &str, NULL);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)generic);
    str.ptr = NULL;
    str.slen = 0;
    generic->hvalue = str;

    return msg;
}

static pjsip_msg *create_msg1(pj_pool_t *pool)
{
    pjsip_via_hdr *via;
    pjsip_route_hdr *route;
    pjsip_name_addr *name_addr;
    pjsip_sip_uri *url;
    pjsip_max_fwd_hdr *max_fwd;
    pjsip_to_hdr *to;
    pjsip_from_hdr *from;
    pjsip_contact_hdr *contact;
    pjsip_ctype_hdr *ctype;
    pjsip_cid_hdr *cid;
    pjsip_clen_hdr *clen;
    pjsip_cseq_hdr *cseq;
    pjsip_msg *msg = pjsip_msg_create(pool, PJSIP_RESPONSE_MSG);
    pjsip_msg_body *body;

    //"SIP/2.0 200 OK\r\n"
    msg->line.status.code = 200;
    msg->line.status.reason = pj_str("OK");

    //"Via: SIP/2.0/SCTP server10.biloxi.com;branch=z9hG4bKnashds8;rport;received=192.0.2.1\r\n"
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    via->transport = pj_str("SCTP");
    via->sent_by.host = pj_str("server10.biloxi.com");
    via->branch_param = pj_str("z9hG4bKnashds8");
    via->rport_param = 0;
    via->recvd_param = pj_str("192.0.2.1");

    //"Via: SIP/2.0/UDP bigbox3.site3.atlanta.com;branch=z9hG4bK77ef4c2312983.1;received=192.0.2.2\r\n"
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    via->transport = pj_str("UDP");
    via->sent_by.host = pj_str("bigbox3.site3.atlanta.com");
    via->branch_param = pj_str("z9hG4bK77ef4c2312983.1");
    via->recvd_param = pj_str("192.0.2.2");

    //"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds ;received=192.0.2.3\r\n"
    via = pjsip_via_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)via);
    via->transport = pj_str("UDP");
    via->sent_by.host = pj_str("pc33.atlanta.com");
    via->branch_param = pj_str("z9hG4bK776asdhds");
    via->recvd_param = pj_str("192.0.2.3");

    //"Route: <sip:proxy.sipprovider.com>\r\n"
    route = pjsip_route_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)route);
    url = pjsip_sip_uri_create(pool, PJ_FALSE);
    route->name_addr.uri = (pjsip_uri*)url;
    url->host = pj_str("proxy.sipprovider.com");
    
    //"Route: <sip:proxy.supersip.com:5060>\r\n"
    route = pjsip_route_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)route);
    url = pjsip_sip_uri_create(pool, PJ_FALSE);
    route->name_addr.uri = (pjsip_uri*)url;
    url->host = pj_str("proxy.supersip.com");
    url->port = 5060;

    //"Max-Forwards: 70\r\n"
    max_fwd = pjsip_max_fwd_hdr_create(pool, 70);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)max_fwd);

    //"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
    to = pjsip_to_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)to);
    name_addr = pjsip_name_addr_create(pool);
    name_addr->display = pj_str("Bob");
    to->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, PJ_FALSE);
    name_addr->uri = (pjsip_uri*)url;
    url->user = pj_str("bob");
    url->host = pj_str("biloxi.com");
    to->tag = pj_str("a6c85cf");

    //"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
    from = pjsip_from_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)from);
    name_addr = pjsip_name_addr_create(pool);
    name_addr->display = pj_str("Alice");
    from->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, PJ_FALSE);
    name_addr->uri = (pjsip_uri*)url;
    url->user = pj_str("alice");
    url->host = pj_str("atlanta.com");
    from->tag = pj_str("1928301774");

    //"Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n"
    cid = pjsip_cid_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)cid);
    cid->id = pj_str("a84b4c76e66710@pc33.atlanta.com");

    //"CSeq: 314159 INVITE\r\n"
    cseq = pjsip_cseq_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)cseq);
    cseq->cseq = 314159;
    pjsip_method_set(&cseq->method, PJSIP_INVITE_METHOD);

    //"Contact: <sips:bob@192.0.2.4>\r\n"
    contact = pjsip_contact_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)contact);
    name_addr = pjsip_name_addr_create(pool);
    contact->uri = (pjsip_uri*)name_addr;
    url = pjsip_sip_uri_create(pool, PJ_TRUE);
    name_addr->uri = (pjsip_uri*)url;
    url->user = pj_str("bob");
    url->host = pj_str("192.0.2.4");

    //"Content-Type: application/sdp\r\n"
    ctype = pjsip_ctype_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)ctype);
    ctype->media.type = pj_str("application");
    ctype->media.subtype = pj_str("sdp");

    //"Content-Length: 150\r\n"
    clen = pjsip_clen_hdr_create(pool);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)clen);
    clen->len = 150;

    // Body
    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    msg->body = body;
    body->content_type.type = pj_str("application");
    body->content_type.subtype = pj_str("sdp");
    body->data = (void*)
	"v=0\r\n"
	"o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
	"s=-\r\n"
	"t=0 0\r\n"
	"c=IN IP4 pc33.atlanta.com\r\n"
	"m=audio 3456 RTP/AVP 0 1 3 99\r\n"
	"a=rtpmap:0 PCMU/8000\r\n";
    body->len = pj_ansi_strlen((const char*) body->data);
    body->print_body = &pjsip_print_text_body;

    return msg;
}

/*****************************************************************************/

static pj_status_t simple_test(void)
{
    char stbuf[] = "SIP/2.0 180 Ringing like it never rings before";
    unsigned i;
    pjsip_status_line st_line;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  simple test.."));
    
    status = pjsip_parse_status_line(0, stbuf, pj_ansi_strlen(stbuf), &st_line);
    if (status != PJ_SUCCESS)
	return status;

    for (i=0; i<PJ_ARRAY_SIZE(test_array); ++i) {
	pj_pool_t *pool;
	pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
	status = test_entry( pool, &test_array[i] );
	pjsip_endpt_release_pool(endpt, pool);

	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}


#if INCLUDE_BENCHMARKS
static int msg_benchmark(unsigned *p_detect, unsigned *p_parse, 
			 unsigned *p_print)
{
    pj_pool_t *pool;
    int i, loop;
    pj_timestamp zero;
    pj_time_val elapsed;
    pj_highprec_t avg_detect, avg_parse, avg_print, kbytes;
    pj_status_t status = PJ_SUCCESS;

    pj_bzero(&var, sizeof(var));
    zero.u64 = 0;

    for (loop=0; loop<LOOP; ++loop) {
	for (i=0; i<(int)PJ_ARRAY_SIZE(test_array); ++i) {
	    pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
	    status = test_entry( pool, &test_array[i] );
	    pjsip_endpt_release_pool(endpt, pool);

	    if (status != PJ_SUCCESS)
		return status;
	}
    }

    kbytes = var.detect_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &var.detect_time);
    avg_detect = pj_elapsed_usec(&zero, &var.detect_time);
    pj_highprec_mul(avg_detect, AVERAGE_MSG_LEN);
    pj_highprec_div(avg_detect, var.detect_len);
    avg_detect = 1000000 / avg_detect;

    PJ_LOG(3,(THIS_FILE, 
	      "    %u.%u MB detected in %d.%03ds (avg=%d msg detection/sec)", 
	      (unsigned)(var.detect_len/1000000), (unsigned)kbytes,
	      elapsed.sec, elapsed.msec,
	      (unsigned)avg_detect));
    *p_detect = (unsigned)avg_detect;

    kbytes = var.parse_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &var.parse_time);
    avg_parse = pj_elapsed_usec(&zero, &var.parse_time);
    pj_highprec_mul(avg_parse, AVERAGE_MSG_LEN);
    pj_highprec_div(avg_parse, var.parse_len);
    avg_parse = 1000000 / avg_parse;

    PJ_LOG(3,(THIS_FILE, 
	      "    %u.%u MB parsed in %d.%03ds (avg=%d msg parsing/sec)", 
	      (unsigned)(var.parse_len/1000000), (unsigned)kbytes,
	      elapsed.sec, elapsed.msec,
	      (unsigned)avg_parse));
    *p_parse = (unsigned)avg_parse;

    kbytes = var.print_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &var.print_time);
    avg_print = pj_elapsed_usec(&zero, &var.print_time);
    pj_highprec_mul(avg_print, AVERAGE_MSG_LEN);
    pj_highprec_div(avg_print, var.print_len);
    avg_print = 1000000 / avg_print;

    PJ_LOG(3,(THIS_FILE, 
	      "    %u.%u MB printed in %d.%03ds (avg=%d msg print/sec)", 
	      (unsigned)(var.print_len/1000000), (unsigned)kbytes,
	      elapsed.sec, elapsed.msec,
	      (unsigned)avg_print));

    *p_print = (unsigned)avg_print;
    return status;
}
#endif	/* INCLUDE_BENCHMARKS */

/*****************************************************************************/
/* Test various header parsing and production */
static int hdr_test_success(pjsip_hdr *h);
static int hdr_test_accept0(pjsip_hdr *h);
static int hdr_test_accept1(pjsip_hdr *h);
static int hdr_test_accept2(pjsip_hdr *h);
static int hdr_test_allow0(pjsip_hdr *h);
static int hdr_test_authorization(pjsip_hdr *h);
static int hdr_test_cid(pjsip_hdr *h);
static int hdr_test_contact0(pjsip_hdr *h);
static int hdr_test_contact1(pjsip_hdr *h);
static int hdr_test_contact_q0(pjsip_hdr *h);
static int hdr_test_contact_q1(pjsip_hdr *h);
static int hdr_test_contact_q2(pjsip_hdr *h);
static int hdr_test_contact_q3(pjsip_hdr *h);
static int hdr_test_contact_q4(pjsip_hdr *h);
static int hdr_test_content_length(pjsip_hdr *h);
static int hdr_test_content_type(pjsip_hdr *h);
static int hdr_test_from(pjsip_hdr *h);
static int hdr_test_proxy_authenticate(pjsip_hdr *h);
static int hdr_test_record_route(pjsip_hdr *h);
static int hdr_test_supported(pjsip_hdr *h);
static int hdr_test_to(pjsip_hdr *h);
static int hdr_test_via(pjsip_hdr *h);
static int hdr_test_via_ipv6_1(pjsip_hdr *h);
static int hdr_test_via_ipv6_2(pjsip_hdr *h);
static int hdr_test_via_ipv6_3(pjsip_hdr *h);
static int hdr_test_retry_after1(pjsip_hdr *h);
static int hdr_test_subject_utf(pjsip_hdr *h);


#define GENERIC_PARAM	     "p0=a;p1=\"ab:;cd\";p2=ab%3acd;p3"
#define GENERIC_PARAM_PARSED "p0=a;p1=\"ab:;cd\";p2=ab:cd;p3"
#define PARAM_CHAR	     "][/:&+$"
#define SIMPLE_ADDR_SPEC     "sip:host"
#define ADDR_SPEC	     SIMPLE_ADDR_SPEC ";"PARAM_CHAR"="PARAM_CHAR ";p1=\";\""
#define NAME_ADDR	     "<" ADDR_SPEC ">"

#define HDR_FLAG_PARSE_FAIL 1
#define HDR_FLAG_DONT_PRINT 2

struct hdr_test_t
{
    char *hname;
    char *hshort_name;
    char *hcontent;
    int  (*test)(pjsip_hdr*);
    unsigned flags;
} hdr_test_data[] =
{
    {
	/* Empty Accept */
	"Accept", NULL,
	"",
	&hdr_test_accept0
    },

    {
	/* Overflowing generic string header */
	"Accept", NULL,
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, " \
	"a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a",
	&hdr_test_success,
	HDR_FLAG_PARSE_FAIL
    },

    {
	/* Normal Accept */
	"Accept", NULL,
	"application/*, text/plain",
	&hdr_test_accept1
    },

    {
	/* Accept with params */
	"Accept", NULL,
	"application/*;p1=v1, text/plain",
	&hdr_test_accept2
    },

    {
	/* Empty Allow */
	"Allow", NULL,
	"",
	&hdr_test_allow0,
    },

    {
	/* Authorization, testing which params should be quoted */
	"Authorization", NULL,
	"Digest username=\"username\", realm=\"realm\", nonce=\"nonce\", "  \
		"uri=\"sip:domain\", response=\"RESPONSE\", algorithm=MD5, "    \
		"cnonce=\"CNONCE\", opaque=\"OPAQUE\", qop=auth, nc=00000001",
	&hdr_test_authorization
    },

    {
	/* Call ID */
	"Call-ID", "i",
	"-.!%*_+`'~()<>:\\\"/[]?{}",
	&hdr_test_cid,
    },

    {
	/* Parameter belong to hparam */
	"Contact", "m",
	SIMPLE_ADDR_SPEC ";p1=v1",
	&hdr_test_contact0,
	HDR_FLAG_DONT_PRINT
    },

    {
	/* generic-param in Contact header */
	"Contact", "m",
	NAME_ADDR ";" GENERIC_PARAM,
	&hdr_test_contact1
    },

    {
	/* q=0 parameter in Contact header */
	"Contact", "m",
	NAME_ADDR ";q=0",
	&hdr_test_contact_q0,
	HDR_FLAG_DONT_PRINT
    },

    {
	/* q=0.5 parameter in Contact header */
	"Contact", "m",
	NAME_ADDR ";q=0.5",
	&hdr_test_contact_q1
    },

    {
	/* q=1 parameter in Contact header */
	"Contact", "m",
	NAME_ADDR ";q=1",
	&hdr_test_contact_q2
    },

    {
	/* q=1.0 parameter in Contact header */
	"Contact", "m",
	NAME_ADDR ";q=1.0",
	&hdr_test_contact_q3,
	HDR_FLAG_DONT_PRINT
    },

    {
	/* q=1.1 parameter in Contact header */
	"Contact", "m",
	NAME_ADDR ";q=1.15",
	&hdr_test_contact_q4
    },

    {
	/* Content-Length */
	"Content-Length", "l",
	"10",
	&hdr_test_content_length
    },

    {
	/* Content-Type, with generic-param */
	"Content-Type", "c",
	"application/sdp" ";" GENERIC_PARAM,
	&hdr_test_content_type,
	HDR_FLAG_DONT_PRINT
    },

    {
	/* From, testing parameters and generic-param */
	"From", "f",
	NAME_ADDR ";" GENERIC_PARAM,
	&hdr_test_from
    },

    {
	/* Proxy-Authenticate, testing which params should be quoted */
	"Proxy-Authenticate", NULL,
	"Digest  realm=\"realm\",domain=\"sip:domain\",nonce=\"nonce\","  \
	        "opaque=\"opaque\",stale=true,algorithm=MD5,qop=\"auth\"",
	&hdr_test_proxy_authenticate
    },

    {
	/* Record-Route, param belong to header */
	"Record-Route", NULL,
	NAME_ADDR ";" GENERIC_PARAM,
	&hdr_test_record_route
    },

    {
	/* Empty Supported */
	"Supported", "k",
	"",
	&hdr_test_supported,
    },

    {
	/* To */
	"To", "t",
	NAME_ADDR ";" GENERIC_PARAM,
	&hdr_test_to
    },

    {
	/* Via */
	"Via", "v",
	"SIP/2.0/XYZ host" ";" GENERIC_PARAM,
	&hdr_test_via
    },

    {
	/* Via with IPv6 */
	"Via", "v",
	"SIP/2.0/UDP [::1]",
	&hdr_test_via_ipv6_1
    },

    {
	/* Via with IPv6 */
	"Via", "v",
	"SIP/2.0/UDP [::1]:5061",
	&hdr_test_via_ipv6_2
    },

    {
	/* Via with IPv6 */
	"Via", "v",
	"SIP/2.0/UDP [::1];rport=5061;received=::2",
	&hdr_test_via_ipv6_3
    },

    {
	/* Retry-After header with comment */
	"Retry-After", NULL,
	"10(Already Pending Register)",
	&hdr_test_retry_after1
    },

    {
	/* Non-ASCII UTF-8 characters in Subject */
	"Subject", NULL,
	"\xC0\x81",
	&hdr_test_subject_utf
    }
};

static int hdr_test_success(pjsip_hdr *h)
{
    PJ_UNUSED_ARG(h);
    return 0;
}

/* "" */
static int hdr_test_accept0(pjsip_hdr *h)
{
    pjsip_accept_hdr *hdr = (pjsip_accept_hdr*)h;

    if (h->type != PJSIP_H_ACCEPT)
	return -1010;

    if (hdr->count != 0)
	return -1020;

    return 0;
}

/* "application/ *, text/plain\r\n" */
static int hdr_test_accept1(pjsip_hdr *h)
{
    pjsip_accept_hdr *hdr = (pjsip_accept_hdr*)h;

    if (h->type != PJSIP_H_ACCEPT)
	return -1110;

    if (hdr->count != 2)
	return -1120;

    if (pj_strcmp2(&hdr->values[0], "application/*"))
	return -1130;

    if (pj_strcmp2(&hdr->values[1], "text/plain"))
	return -1140;

    return 0;
}

/* "application/ *;p1=v1, text/plain\r\n" */
static int hdr_test_accept2(pjsip_hdr *h)
{
    pjsip_accept_hdr *hdr = (pjsip_accept_hdr*)h;

    if (h->type != PJSIP_H_ACCEPT)
	return -1210;

    if (hdr->count != 2)
	return -1220;

    if (pj_strcmp2(&hdr->values[0], "application/*;p1=v1"))
	return -1230;

    if (pj_strcmp2(&hdr->values[1], "text/plain"))
	return -1240;

    return 0;
}

/* "" */
static int hdr_test_allow0(pjsip_hdr *h)
{
    pjsip_allow_hdr *hdr = (pjsip_allow_hdr*)h;

    if (h->type != PJSIP_H_ALLOW)
	return -1310;

    if (hdr->count != 0)
	return -1320;

    return 0;

}


/*
	"Digest username=\"username\", realm=\"realm\", nonce=\"nonce\", "  \
		"uri=\"sip:domain\", response=\"RESPONSE\", algorithm=MD5, "    \
		"cnonce=\"CNONCE\", opaque=\"OPAQUE\", qop=auth, nc=00000001",
 */
static int hdr_test_authorization(pjsip_hdr *h)
{
    pjsip_authorization_hdr *hdr = (pjsip_authorization_hdr*)h;

    if (h->type != PJSIP_H_AUTHORIZATION)
	return -1410;

    if (pj_strcmp2(&hdr->scheme, "Digest"))
	return -1420;

    if (pj_strcmp2(&hdr->credential.digest.username, "username"))
	return -1421;

    if (pj_strcmp2(&hdr->credential.digest.realm, "realm"))
	return -1422;

    if (pj_strcmp2(&hdr->credential.digest.nonce, "nonce"))
	return -1423;

    if (pj_strcmp2(&hdr->credential.digest.uri, "sip:domain"))
	return -1424;

    if (pj_strcmp2(&hdr->credential.digest.response, "RESPONSE"))
	return -1425;

    if (pj_strcmp2(&hdr->credential.digest.algorithm, "MD5"))
	return -1426;

    if (pj_strcmp2(&hdr->credential.digest.cnonce, "CNONCE"))
	return -1427;

    if (pj_strcmp2(&hdr->credential.digest.opaque, "OPAQUE"))
	return -1428;

    if (pj_strcmp2(&hdr->credential.digest.qop, "auth"))
	return -1429;

    if (pj_strcmp2(&hdr->credential.digest.nc, "00000001"))
	return -1430;

    return 0;
}


/*
    "-.!%*_+`'~()<>:\\\"/[]?{}\r\n"
 */
static int hdr_test_cid(pjsip_hdr *h)
{
    pjsip_cid_hdr *hdr = (pjsip_cid_hdr*)h;

    if (h->type != PJSIP_H_CALL_ID)
	return -1510;

    if (pj_strcmp2(&hdr->id, "-.!%*_+`'~()<>:\\\"/[]?{}"))
	return -1520;

    return 0;
}

/*
 #define SIMPLE_ADDR_SPEC    "sip:host"
 */
static int test_simple_addr_spec(pjsip_uri *uri)
{
    pjsip_sip_uri *sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(uri);

    if (!PJSIP_URI_SCHEME_IS_SIP(uri))
	return -900;

    if (pj_strcmp2(&sip_uri->host, "host"))
	return -910;

    if (sip_uri->port != 0)
	return -920;

    return 0;
}

/* 
#define PARAM_CHAR	    "][/:&+$"
#define SIMPLE_ADDR_SPEC    "sip:host"
#define ADDR_SPEC	     SIMPLE_ADDR_SPEC ";"PARAM_CHAR"="PARAM_CHAR ";p1=\";\""
#define NAME_ADDR	    "<" ADDR_SPEC ">"
 */
static int nameaddr_test(void *uri)
{
    pjsip_sip_uri *sip_uri=(pjsip_sip_uri *)pjsip_uri_get_uri((pjsip_uri*)uri);
    pjsip_param *param;
    int rc;

    if (!PJSIP_URI_SCHEME_IS_SIP(uri))
	return -930;

    rc = test_simple_addr_spec((pjsip_uri*)sip_uri);
    if (rc != 0)
	return rc;

    if (pj_list_size(&sip_uri->other_param) != 2)
	return -940;

    param = sip_uri->other_param.next;

    if (pj_strcmp2(&param->name, PARAM_CHAR))
	return -942;

    if (pj_strcmp2(&param->value, PARAM_CHAR))
	return -943;

    param = param->next;
    if (pj_strcmp2(&param->name, "p1"))
	return -942;
    if (pj_strcmp2(&param->value, "\";\""))
	return -943;

    return 0;
}

/*
#define GENERIC_PARAM  "p0=a;p1=\"ab:;cd\";p2=ab%3acd;p3"
 */
static int generic_param_test(pjsip_param *param_head)
{
    pjsip_param *param;

    if (pj_list_size(param_head) != 4)
	return -950;

    param = param_head->next;

    if (pj_strcmp2(&param->name, "p0"))
	return -952;
    if (pj_strcmp2(&param->value, "a"))
	return -953;

    param = param->next;
    if (pj_strcmp2(&param->name, "p1"))
	return -954;
    if (pj_strcmp2(&param->value, "\"ab:;cd\""))
	return -955;

    param = param->next;
    if (pj_strcmp2(&param->name, "p2"))
	return -956;
    if (pj_strcmp2(&param->value, "ab:cd"))
	return -957;

    param = param->next;
    if (pj_strcmp2(&param->name, "p3"))
	return -958;
    if (pj_strcmp2(&param->value, ""))
	return -959;

    return 0;
}



/*
    SIMPLE_ADDR_SPEC ";p1=v1\r\n"
 */
static int hdr_test_contact0(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    pjsip_param *param;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1610;

    rc = test_simple_addr_spec(hdr->uri);
    if (rc != 0)
	return rc;

    if (pj_list_size(&hdr->other_param) != 1)
	return -1620;

    param = hdr->other_param.next;

    if (pj_strcmp2(&param->name, "p1"))
	return -1630;

    if (pj_strcmp2(&param->value, "v1"))
	return -1640;

    return 0;
}

/*
    NAME_ADDR GENERIC_PARAM "\r\n",    
 */
static int hdr_test_contact1(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    rc = generic_param_test(&hdr->other_param);
    if (rc != 0)
	return rc;

    return 0;
}

/*
    NAME_ADDR ";q=0"
 */
static int hdr_test_contact_q0(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    if (hdr->q1000 != 0)
	return -1711;

    return 0;
}

/*
    NAME_ADDR ";q=0.5"
 */
static int hdr_test_contact_q1(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    if (hdr->q1000 != 500)
	return -1712;

    return 0;
}

/*
    NAME_ADDR ";q=1"
 */
static int hdr_test_contact_q2(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    if (hdr->q1000 != 1000)
	return -1713;

    return 0;
}

/*
    NAME_ADDR ";q=1.0"
 */
static int hdr_test_contact_q3(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    if (hdr->q1000 != 1000)
	return -1714;

    return 0;
}

/*
    NAME_ADDR ";q=1.15"
 */
static int hdr_test_contact_q4(pjsip_hdr *h)
{
    pjsip_contact_hdr *hdr = (pjsip_contact_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_CONTACT)
	return -1710;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    if (hdr->q1000 != 1150)
	return -1715;

    return 0;
}

/*
    "10"
 */
static int hdr_test_content_length(pjsip_hdr *h)
{
    pjsip_clen_hdr *hdr = (pjsip_clen_hdr*)h;

    if (h->type != PJSIP_H_CONTENT_LENGTH)
	return -1810;

    if (hdr->len != 10)
	return -1820;

    return 0;
}

/*
    "application/sdp" GENERIC_PARAM,
 */
static int hdr_test_content_type(pjsip_hdr *h)
{
    pjsip_ctype_hdr *hdr = (pjsip_ctype_hdr*)h;
    const pjsip_param *prm;

    if (h->type != PJSIP_H_CONTENT_TYPE)
	return -1910;

    if (pj_strcmp2(&hdr->media.type, "application"))
	return -1920;

    if (pj_strcmp2(&hdr->media.subtype, "sdp"))
	return -1930;

    /* Currently, if the media parameter contains escaped characters,
     * pjsip will print the parameter unescaped.
     */
    prm = hdr->media.param.next;
    if (prm == &hdr->media.param) return -1940;
    if (pj_strcmp2(&prm->name, "p0")) return -1941;
    if (pj_strcmp2(&prm->value, "a")) return -1942;

    prm = prm->next;
    if (prm == &hdr->media.param) return -1950;
    if (pj_strcmp2(&prm->name, "p1")) { PJ_LOG(3,("", "%.*s", (int)prm->name.slen, prm->name.ptr)); return -1951; }
    if (pj_strcmp2(&prm->value, "\"ab:;cd\"")) { PJ_LOG(3,("", "%.*s", (int)prm->value.slen, prm->value.ptr)); return -1952; }

    prm = prm->next;
    if (prm == &hdr->media.param) return -1960;
    if (pj_strcmp2(&prm->name, "p2")) return -1961;
    if (pj_strcmp2(&prm->value, "ab:cd")) return -1962;

    prm = prm->next;
    if (prm == &hdr->media.param) return -1970;
    if (pj_strcmp2(&prm->name, "p3")) return -1971;
    if (pj_strcmp2(&prm->value, "")) return -1972;

    return 0;
}

/*
    NAME_ADDR GENERIC_PARAM,
 */
static int hdr_test_from(pjsip_hdr *h)
{
    pjsip_from_hdr *hdr = (pjsip_from_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_FROM)
	return -2010;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    rc = generic_param_test(&hdr->other_param);
    if (rc != 0)
	return rc;

    return 0;
}

/*
	"Digest realm=\"realm\", domain=\"sip:domain\", nonce=\"nonce\", "  \
	        "opaque=\"opaque\", stale=true, algorithm=MD5, qop=\"auth\"",
 */
static int hdr_test_proxy_authenticate(pjsip_hdr *h)
{
    pjsip_proxy_authenticate_hdr *hdr = (pjsip_proxy_authenticate_hdr*)h;

    if (h->type != PJSIP_H_PROXY_AUTHENTICATE)
	return -2110;

    if (pj_strcmp2(&hdr->scheme, "Digest"))
	return -2120;

    if (pj_strcmp2(&hdr->challenge.digest.realm, "realm"))
	return -2130;

    if (pj_strcmp2(&hdr->challenge.digest.domain, "sip:domain"))
	return -2140;

    if (pj_strcmp2(&hdr->challenge.digest.nonce, "nonce"))
	return -2150;

    if (pj_strcmp2(&hdr->challenge.digest.opaque, "opaque"))
	return -2160;

    if (hdr->challenge.digest.stale != 1)
	return -2170;

    if (pj_strcmp2(&hdr->challenge.digest.algorithm, "MD5"))
	return -2180;

    if (pj_strcmp2(&hdr->challenge.digest.qop, "auth"))
	return -2190;

    return 0;
}

/*
    NAME_ADDR GENERIC_PARAM,
 */
static int hdr_test_record_route(pjsip_hdr *h)
{
    pjsip_rr_hdr *hdr = (pjsip_rr_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_RECORD_ROUTE)
	return -2210;

    rc = nameaddr_test(&hdr->name_addr);
    if (rc != 0)
	return rc;

    rc = generic_param_test(&hdr->other_param);
    if (rc != 0)
	return rc;

    return 0;

}

/*
    " \r\n"
 */
static int hdr_test_supported(pjsip_hdr *h)
{
    pjsip_supported_hdr *hdr = (pjsip_supported_hdr*)h;

    if (h->type != PJSIP_H_SUPPORTED)
	return -2310;

    if (hdr->count != 0)
	return -2320;

    return 0;
}

/*
    NAME_ADDR GENERIC_PARAM,
 */
static int hdr_test_to(pjsip_hdr *h)
{
    pjsip_to_hdr *hdr = (pjsip_to_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_TO)
	return -2410;

    rc = nameaddr_test(hdr->uri);
    if (rc != 0)
	return rc;

    rc = generic_param_test(&hdr->other_param);
    if (rc != 0)
	return rc;

    return 0;
}

/*
    "SIP/2.0 host" GENERIC_PARAM
 */
static int hdr_test_via(pjsip_hdr *h)
{
    pjsip_via_hdr *hdr = (pjsip_via_hdr*)h;
    int rc;

    if (h->type != PJSIP_H_VIA)
	return -2510;

    if (pj_strcmp2(&hdr->transport, "XYZ"))
	return -2515;

    if (pj_strcmp2(&hdr->sent_by.host, "host"))
	return -2520;

    if (hdr->sent_by.port != 0)
	return -2530;

    rc = generic_param_test(&hdr->other_param);
    if (rc != 0)
	return rc;

    return 0;
}


/*
    "SIP/2.0/UDP [::1]"
 */
static int hdr_test_via_ipv6_1(pjsip_hdr *h)
{
    pjsip_via_hdr *hdr = (pjsip_via_hdr*)h;

    if (h->type != PJSIP_H_VIA)
	return -2610;

    if (pj_strcmp2(&hdr->transport, "UDP"))
	return -2615;

    if (pj_strcmp2(&hdr->sent_by.host, "::1"))
	return -2620;

    if (hdr->sent_by.port != 0)
	return -2630;

    return 0;
}

/* "SIP/2.0/UDP [::1]:5061" */
static int hdr_test_via_ipv6_2(pjsip_hdr *h)
{
    pjsip_via_hdr *hdr = (pjsip_via_hdr*)h;

    if (h->type != PJSIP_H_VIA)
	return -2710;

    if (pj_strcmp2(&hdr->transport, "UDP"))
	return -2715;

    if (pj_strcmp2(&hdr->sent_by.host, "::1"))
	return -2720;

    if (hdr->sent_by.port != 5061)
	return -2730;

    return 0;
}

/* "SIP/2.0/UDP [::1];rport=5061;received=::2" */
static int hdr_test_via_ipv6_3(pjsip_hdr *h)
{
    pjsip_via_hdr *hdr = (pjsip_via_hdr*)h;

    if (h->type != PJSIP_H_VIA)
	return -2810;

    if (pj_strcmp2(&hdr->transport, "UDP"))
	return -2815;

    if (pj_strcmp2(&hdr->sent_by.host, "::1"))
	return -2820;

    if (hdr->sent_by.port != 0)
	return -2830;

    if (pj_strcmp2(&hdr->recvd_param, "::2"))
	return -2840;

    if (hdr->rport_param != 5061)
	return -2850;

    return 0;
}

/* "10(Already Pending Register)" */
static int hdr_test_retry_after1(pjsip_hdr *h)
{
    pjsip_retry_after_hdr *hdr = (pjsip_retry_after_hdr*)h;

    if (h->type != PJSIP_H_RETRY_AFTER)
	return -2910;

    if (hdr->ivalue != 10)
	return -2920;
    
    if (pj_strcmp2(&hdr->comment, "Already Pending Register"))
	return -2930;

    return 0;
}

/* Subject: \xC0\x81 */
static int hdr_test_subject_utf(pjsip_hdr *h)
{
    pjsip_subject_hdr *hdr = (pjsip_subject_hdr*)h;

    if (pj_strcmp2(&h->name, "Subject"))
	return -2950;

    if (pj_strcmp2(&hdr->hvalue, "\xC0\x81"))
	return -2960;

    return 0;
}

static int hdr_test(void)
{
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "  testing header parsing.."));

    for (i=0; i<PJ_ARRAY_SIZE(hdr_test_data); ++i) {
	struct hdr_test_t  *test = &hdr_test_data[i];
	pj_str_t hname;
	int len, parsed_len;
	pj_pool_t *pool;
	pjsip_hdr *parsed_hdr1=NULL, *parsed_hdr2=NULL;
	char *input, *output;
#if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
	static char hcontent[1024];
#else
	char *hcontent;
#endif
	int rc;

	pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);

	/* Parse the header */
	hname = pj_str(test->hname);
	len = strlen(test->hcontent);
#if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
	PJ_ASSERT_RETURN(len < sizeof(hcontent), PJSIP_EMSGTOOLONG);
	strcpy(hcontent, test->hcontent);
#else
	hcontent = test->hcontent;
#endif
	
	parsed_hdr1 = (pjsip_hdr*) pjsip_parse_hdr(0, pool, &hname, 
						   hcontent, len, 
						   &parsed_len);
	if (parsed_hdr1 == NULL) {
	    if (test->flags & HDR_FLAG_PARSE_FAIL) {
		pj_pool_release(pool);
		continue;
	    }
	    PJ_LOG(3,(THIS_FILE, "    error parsing header %s: %s", test->hname, test->hcontent));
	    return -500;
	}

	/* Test the parsing result */
	if (test->test && (rc=test->test(parsed_hdr1)) != 0) {
	    PJ_LOG(3,(THIS_FILE, "    validation failed for header %s: %s", test->hname, test->hcontent));
	    PJ_LOG(3,(THIS_FILE, "    error code is %d", rc));
	    return -502;
	}

#if 1
	/* Parse with hshortname, if present */
	if (test->hshort_name) {
	    hname = pj_str(test->hshort_name);
	    len = strlen(test->hcontent);
#if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
	    PJ_ASSERT_RETURN(len < sizeof(hcontent), PJSIP_EMSGTOOLONG);
	    strcpy(hcontent, test->hcontent);
#else
	    hcontent = test->hcontent;
#endif

	    parsed_hdr2 = (pjsip_hdr*) pjsip_parse_hdr(0, pool, &hname, hcontent, len, &parsed_len);
	    if (parsed_hdr2 == NULL) {
		PJ_LOG(3,(THIS_FILE, "    error parsing header %s: %s", test->hshort_name, test->hcontent));
		return -510;
	    }
	}
#endif

	if (test->flags & HDR_FLAG_DONT_PRINT) {
	    pj_pool_release(pool);
	    continue;
	}

	/* Print the original header */
	input = (char*) pj_pool_alloc(pool, 1024);
	len = pj_ansi_snprintf(input, 1024, "%s: %s", test->hname, test->hcontent);
	if (len < 1 || len >= 1024)
	    return -520;

	/* Print the parsed header*/
	output = (char*) pj_pool_alloc(pool, 1024);
	len = pjsip_hdr_print_on(parsed_hdr1, output, 1024);
	if (len < 0 || len >= 1024) {
	    PJ_LOG(3,(THIS_FILE, "    header too long: %s: %s", test->hname, test->hcontent));
	    return -530;
	}
	output[len] = 0;

	if (strcmp(input, output) != 0) {
	    PJ_LOG(3,(THIS_FILE, "    header character by character comparison failed."));
	    PJ_LOG(3,(THIS_FILE, "    original header=|%s|", input));
	    PJ_LOG(3,(THIS_FILE, "    parsed header  =|%s|", output));
	    return -540;
	}

	pj_pool_release(pool);
    }

    return 0;
}


/*****************************************************************************/

int msg_test(void)
{
    enum { COUNT = 1, DETECT=0, PARSE=1, PRINT=2 };
    struct {
	unsigned detect;
	unsigned parse;
	unsigned print;
    } run[COUNT];
    unsigned i, max, avg_len;
    char desc[250];
    pj_status_t status;

    status = hdr_test();
    if (status != 0)
	return status;

    status = simple_test();
    if (status != PJ_SUCCESS)
	return status;

#if INCLUDE_BENCHMARKS
    for (i=0; i<COUNT; ++i) {
	PJ_LOG(3,(THIS_FILE, "  benchmarking (%d of %d)..", i+1, COUNT));
	status = msg_benchmark(&run[i].detect, &run[i].parse, &run[i].print);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Calculate average message length */
    for (i=0, avg_len=0; i<PJ_ARRAY_SIZE(test_array); ++i) {
	avg_len += test_array[i].len;
    }
    avg_len /= PJ_ARRAY_SIZE(test_array);


    /* Print maximum detect/sec */
    for (i=0, max=0; i<COUNT; ++i)
	if (run[i].detect > max) max = run[i].detect;

    PJ_LOG(3,("", "  Maximum message detection/sec=%u", max));

    pj_ansi_sprintf(desc, "Number of SIP messages "
			  "can be pre-parse by <tt>pjsip_find_msg()</tt> "
			  "per second (tested with %d message sets with "
			  "average message length of "
			  "%d bytes)", (int)PJ_ARRAY_SIZE(test_array), avg_len);
    report_ival("msg-detect-per-sec", max, "msg/sec", desc);

    /* Print maximum parse/sec */
    for (i=0, max=0; i<COUNT; ++i)
	if (run[i].parse > max) max = run[i].parse;

    PJ_LOG(3,("", "  Maximum message parsing/sec=%u", max));

    pj_ansi_sprintf(desc, "Number of SIP messages "
			  "can be <b>parsed</b> by <tt>pjsip_parse_msg()</tt> "
			  "per second (tested with %d message sets with "
			  "average message length of "
			  "%d bytes)", (int)PJ_ARRAY_SIZE(test_array), avg_len);
    report_ival("msg-parse-per-sec", max, "msg/sec", desc);

    /* Msg parsing bandwidth */
    report_ival("msg-parse-bandwidth-mb", avg_len*max/1000000, "MB/sec",
	        "Message parsing bandwidth in megabytes (number of megabytes"
		" worth of SIP messages that can be parsed per second). "
		"The value is derived from msg-parse-per-sec above.");


    /* Print maximum print/sec */
    for (i=0, max=0; i<COUNT; ++i)
	if (run[i].print > max) max = run[i].print;

    PJ_LOG(3,("", "  Maximum message print/sec=%u", max));

    pj_ansi_sprintf(desc, "Number of SIP messages "
			  "can be <b>printed</b> by <tt>pjsip_msg_print()</tt>"
			  " per second (tested with %d message sets with "
			  "average message length of "
			  "%d bytes)", (int)PJ_ARRAY_SIZE(test_array), avg_len);

    report_ival("msg-print-per-sec", max, "msg/sec", desc);

    /* Msg print bandwidth */
    report_ival("msg-printed-bandwidth-mb", avg_len*max/1000000, "MB/sec",
	        "Message print bandwidth in megabytes (total size of "
		"SIP messages printed per second). "
		"The value is derived from msg-print-per-sec above.");

#endif	/* INCLUDE_BENCHMARKS */

    return PJ_SUCCESS;
}




