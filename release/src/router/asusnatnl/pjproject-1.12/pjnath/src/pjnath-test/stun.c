/* $Id: stun.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "stun.c"

static pj_stun_msg* create1(pj_pool_t*);
static int verify1(pj_stun_msg*);
static int verify2(pj_stun_msg*);
static int verify5(pj_stun_msg*);

static struct test
{
    const char    *title;
    char	      *pdu;
    unsigned       pdu_len;
    pj_stun_msg* (*create)(pj_pool_t*);
    pj_status_t    expected_status;
    int          (*verify)(pj_stun_msg*);
} tests[] = 
{
    {
	"Invalid message type",
	"\x11\x01\x00\x00\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	20,
	NULL,
	PJNATH_EINSTUNMSGTYPE,
	NULL
    },
    {
	"Short message (1) (partial header)",
	"\x00\x01",
	2,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (2) (partial header)",
	"\x00\x01\x00\x00\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	16,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (3), (missing attribute)",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	20,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (4), (partial attribute header)",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28",
	22,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (5), (partial attribute header)",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00",
	23,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (6), (partial attribute header)",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04",
	24,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Short message (7), (partial attribute body)",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00",
	27,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Message length in header is too long",
	"\x00\x01\xff\xff\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00",
	27,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Message length in header is shorter",
	"\x00\x01\x00\x04\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00\x00",
	28,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Invalid magic",
	"\x00\x01\x00\x08\x00\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00\x00",
	28,
	NULL,
	PJ_SUCCESS,
	NULL
    },
    {
	"Character beyond message",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00\x00\x0a",
	29,
	NULL,
	PJNATH_EINSTUNMSGLEN,
	NULL
    },
    {
	"Respond unknown mandatory attribute with 420 and "
	"UNKNOWN-ATTRIBUTES attribute",
	NULL,
	0,
	&create1,
	0,
	&verify1
    },
    {
	"Unknown but non-mandatory should be okay",
	"\x00\x01\x00\x08\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\xff\x00\x03\x00\x00\x00\x00",
	28,
	NULL,
	PJ_SUCCESS,
	&verify2
    },
    {
	"String attr length larger than message",
	"\x00\x01\x00\x08\x00\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x06\x00\xff\x00\x00\x00\x00",
	28,
	NULL,
	PJNATH_ESTUNINATTRLEN,
	NULL
    },
    {
	"Attribute other than FINGERPRINT after MESSAGE-INTEGRITY is allowed",
	"\x00\x01\x00\x20\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x08\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // M-I
	"\x80\x24\x00\x04\x00\x00\x00\x00",  // REFRESH-INTERVAL
	52,
	NULL,
	PJ_SUCCESS,
	NULL
    },
    {
	"Attribute between MESSAGE-INTEGRITY and FINGERPRINT is allowed", 
	"\x00\x01\x00\x28\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x08\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // M-I
	"\x80\x24\x00\x04\x00\x00\x00\x00"  // REFRESH-INTERVAL
	"\x80\x28\x00\x04\xc7\xde\xdd\x65", // FINGERPRINT
	60,
	NULL,
	PJ_SUCCESS,
	&verify5
    },
    {
	"Attribute past FINGERPRINT is not allowed", 
	"\x00\x01\x00\x10\x21\x12\xa4\x42"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x80\x28\x00\x04\x00\x00\x00\x00"
	"\x80\x24\x00\x04\x00\x00\x00\x00",
	36,
	NULL,
	PJNATH_ESTUNFINGERPOS,
	NULL
    }
};

static const char *err(pj_status_t status)
{
    static char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    return errmsg;
}

static const pj_str_t USERNAME = {"user", 4};
static const pj_str_t PASSWORD = {"password", 8};

static int decode_test(void)
{
    unsigned i;
    pj_pool_t *pool;
    int rc = 0;
    
    pool = pj_pool_create(mem, "decode_test", 1024, 1024, NULL);

    PJ_LOG(3,(THIS_FILE, "  STUN decode test"));

    for (i=0; i<PJ_ARRAY_SIZE(tests); ++i) {
	struct test *t = &tests[i];
	pj_stun_msg *msg, *msg2;
	pj_uint8_t buf[1500];
	pj_str_t key;
	pj_size_t len;
	pj_status_t status;

	PJ_LOG(3,(THIS_FILE, "   %s", t->title));

	if (t->pdu) {
	    status = pj_stun_msg_decode(pool, (pj_uint8_t*)t->pdu, t->pdu_len,
				        PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, 
					&msg, NULL, NULL);

	    /* Check expected decode result */
	    if (t->expected_status != status) {
		PJ_LOG(1,(THIS_FILE, "    expecting status %d, got %d",
		          t->expected_status, status));
		rc = -10;
		goto on_return;
	    }

	} else {
	    msg = t->create(pool);
	    status = PJ_SUCCESS;
	}

	if (status != PJ_SUCCESS)
	    continue;

	/* Try to encode message */
	pj_stun_create_key(pool, &key, NULL, &USERNAME, PJ_STUN_PASSWD_PLAIN, &PASSWORD);
	status = pj_stun_msg_encode(msg, buf, sizeof(buf), 0, &key, &len);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "    encode error: %s", err(status)));
	    rc = -40;
	    goto on_return;
	}

	/* Try to decode it once more */
	status = pj_stun_msg_decode(pool, buf, len, 
				    PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, 
				    &msg2, NULL, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "    subsequent decoding failed: %s", err(status)));
	    rc = -50;
	    goto on_return;
	}

	/* Verify */
	if (t->verify) {
	    rc = t->verify(msg);
	    if (rc != 0) {
		goto on_return;
	    }
	}
    }

on_return:
    pj_pool_release(pool);
    if (rc == 0)
	PJ_LOG(3,(THIS_FILE, "...success!"));
    return rc;
}

/* Create 420 response */
static pj_stun_msg* create1(pj_pool_t *pool)
{
    char *pdu = "\x00\x01\x00\x08\x21\x12\xa4\x42"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\xff\x00\x04\x00\x00\x00\x00";
    unsigned pdu_len = 28;
    pj_stun_msg *msg, *res;
    pj_status_t status;

    status = pj_stun_msg_decode(pool, (pj_uint8_t*)pdu, pdu_len,
				PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
				&msg, NULL, &res);
    pj_assert(status != PJ_SUCCESS);
    pj_assert(res != NULL);

    return res;
}

/* Error response MUST have ERROR-CODE attribute */
/* 420 response MUST contain UNKNOWN-ATTRIBUTES */
static int verify1(pj_stun_msg *msg)
{
    pj_stun_errcode_attr *aerr;
    pj_stun_unknown_attr *aunk;

    if (!PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type)) {
	PJ_LOG(1,(THIS_FILE, "    expecting error message"));
	return -100;
    }

    aerr = (pj_stun_errcode_attr*)
	   pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ERROR_CODE, 0);
    if (aerr == NULL) {
	PJ_LOG(1,(THIS_FILE, "    missing ERROR-CODE attribute"));
	return -110;
    }

    if (aerr->err_code != 420) {
	PJ_LOG(1,(THIS_FILE, "    expecting 420 error"));
	return -120;
    }

    aunk = (pj_stun_unknown_attr*)
	   pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES, 0);
    if (aunk == NULL) {
	PJ_LOG(1,(THIS_FILE, "    missing UNKNOWN-ATTRIBUTE attribute"));
	return -130;
    }

    if (aunk->attr_count != 1) {
	PJ_LOG(1,(THIS_FILE, "    expecting one unknown attribute"));
	return -140;
    }

    if (aunk->attrs[0] != 0xff) {
	PJ_LOG(1,(THIS_FILE, "    expecting 0xff as unknown attribute"));
	return -150;
    }

    return 0;
}

/* Attribute count should be zero since unknown attribute is not parsed */
static int verify2(pj_stun_msg *msg)
{
    pj_stun_binary_attr *bin_attr;

    if (msg->attr_count != 1) {
	PJ_LOG(1,(THIS_FILE, "    expecting one attribute count"));
	return -200;
    }

    bin_attr = (pj_stun_binary_attr*)msg->attr[0];
    if (bin_attr->hdr.type != 0x80ff) {
	PJ_LOG(1,(THIS_FILE, "    expecting attribute type 0x80ff"));
	return -210;
    }
    if (bin_attr->hdr.length != 3) {
	PJ_LOG(1,(THIS_FILE, "    expecting attribute length = 4"));
	return -220;
    }
    if (bin_attr->magic != PJ_STUN_MAGIC) {
	PJ_LOG(1,(THIS_FILE, "    expecting PJ_STUN_MAGIC for unknown attr"));
	return -230;
    }
    if (bin_attr->length != 3) {
	PJ_LOG(1,(THIS_FILE, "    expecting data length 4"));
	return -240;
    }

    return 0;
}


/* Attribute between MESSAGE-INTEGRITY and FINGERPRINT is allowed */
static int verify5(pj_stun_msg *msg)
{
    if (msg->attr_count != 3) {
	PJ_LOG(1,(THIS_FILE, "    expecting 3 attribute count"));
	return -500;
    }

    if (msg->attr[0]->type != PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	PJ_LOG(1,(THIS_FILE, "    expecting MESSAGE-INTEGRITY"));
	return -510;
    }
    if (msg->attr[1]->type != PJ_STUN_ATTR_REFRESH_INTERVAL) {
	PJ_LOG(1,(THIS_FILE, "    expecting REFRESH-INTERVAL"));
	return -520;
    }
    if (msg->attr[2]->type != PJ_STUN_ATTR_FINGERPRINT) {
	PJ_LOG(1,(THIS_FILE, "    expecting FINGERPRINT"));
	return -530;
    }

    return 0;
}


static int decode_verify(void)
{
    /* Decode all attribute types */
    return 0;
}

/*
 * Test vectors, from:
 * http://tools.ietf.org/html/draft-denis-behave-rfc3489bis-test-vectors-02
 */
typedef struct test_vector test_vector;

static pj_stun_msg* create_msgint1(pj_pool_t *pool, test_vector *v);
static pj_stun_msg* create_msgint2(pj_pool_t *pool, test_vector *v);
static pj_stun_msg* create_msgint3(pj_pool_t *pool, test_vector *v);

enum
{
    USE_MESSAGE_INTEGRITY   = 1,
    USE_FINGERPRINT	    = 2
};

static struct test_vector
{
    unsigned	   msg_type;
    char	  *tsx_id;
    char	  *pdu;
    unsigned 	   pdu_len;
    unsigned	   options;
    char	  *username;
    char	  *password;
    char	  *realm;
    char	  *nonce;
    pj_stun_msg* (*create)(pj_pool_t*, test_vector*);
} test_vectors[] = 
{
    {
	PJ_STUN_BINDING_REQUEST,
	"\xb7\xe7\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae",
	"\x00\x01\x00\x44\x21\x12\xa4\x42\xb7\xe7"
	"\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae"
	"\x00\x24\x00\x04\x6e\x00\x01\xff\x80\x29"
	"\x00\x08\x93\x2f\xf9\xb1\x51\x26\x3b\x36"
	"\x00\x06\x00\x09\x65\x76\x74\x6a\x3a\x68"
	"\x36\x76\x59\x20\x20\x20\x00\x08\x00\x14"
	"\x62\x4e\xeb\xdc\x3c\xc9\x2d\xd8\x4b\x74"
	"\xbf\x85\xd1\xc0\xf5\xde\x36\x87\xbd\x33"
	"\x80\x28\x00\x04\xad\x8a\x85\xff",
	88,
	USE_MESSAGE_INTEGRITY | USE_FINGERPRINT,
	"evtj:h6vY",
	"VOkJxbRl1RmTxUk/WvJxBt",
	"",
	"",
	&create_msgint1
    }
    /* disabled: see http://trac.pjsip.org/repos/ticket/960
    ,
    {
	PJ_STUN_BINDING_RESPONSE,
	"\xb7\xe7\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae",
	"\x01\x01\x00\x3c"
	"\x21\x12\xa4\x42"
	"\xb7\xe7\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae"
	"\x80\x22\x00\x0b"
	"\x74\x65\x73\x74\x20\x76\x65\x63\x74\x6f\x72\x20"
	"\x00\x20\x00\x08"
	"\x00\x01\xa1\x47\xe1\x12\xa6\x43"
	"\x00\x08\x00\x14"
	"\x2b\x91\xf5\x99\xfd\x9e\x90\xc3\x8c\x74\x89\xf9"
	"\x2a\xf9\xba\x53\xf0\x6b\xe7\xd7"
	"\x80\x28\x00\x04"
	"\xc0\x7d\x4c\x96",
	80,
	USE_MESSAGE_INTEGRITY | USE_FINGERPRINT,
	"evtj:h6vY",
	"VOkJxbRl1RmTxUk/WvJxBt",
	"",
	"",
	&create_msgint2
    }
    */

    /* disabled: see http://trac.pjsip.org/repos/ticket/960
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    ,
    {
	PJ_STUN_BINDING_RESPONSE,
	"\xb7\xe7\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae",
	"\x01\x01\x00\x48" //     Response type and message length
        "\x21\x12\xa4\x42" //     Message cookie
        "\xb7\xe7\xa7\x01" //  }
        "\xbc\x34\xd6\x86" //  }  Transaction ID
        "\xfa\x87\xdf\xae" //  }

        "\x80\x22\x00\x0b" // SOFTWARE, length=11
        "\x74\x65\x73\x74"
        "\x20\x76\x65\x63"
        "\x74\x6f\x72\x20"
        "\x00\x20\x00\x14" // XOR-MAPPED-ADDRESS
        "\x00\x02\xa1\x47"
        "\x01\x13\xa9\xfa"
        "\xa5\xd3\xf1\x79"
        "\xbc\x25\xf4\xb5"
        "\xbe\xd2\xb9\xd9"
        "\x00\x08\x00\x14" // MESSAGE-INTEGRITY attribute header
        "\xa3\x82\x95\x4e" // }
        "\x4b\xe6\x7b\xf1" // }
        "\x17\x84\xc9\x7c" // }  HMAC-SHA1 fingerprint
        "\x82\x92\xc2\x75" // }
        "\xbf\xe3\xed\x41" // }
        "\x80\x28\x00\x04" //    FINGERPRINT attribute header
        "\xc8\xfb\x0b\x4c" //    CRC32 fingerprint
	,
	92,
	USE_MESSAGE_INTEGRITY | USE_FINGERPRINT,
	"evtj:h6vY",
	"VOkJxbRl1RmTxUk/WvJxBt",
	"",
	"",
	&create_msgint3
    }
#endif
    */
};


static char* print_binary(const pj_uint8_t *data, unsigned data_len)
{
    static char buf[1500];
    unsigned length = sizeof(buf);
    char *p = buf;
    unsigned i;

    for (i=0; i<data_len;) {
	unsigned j;

	pj_ansi_snprintf(p, 1500-(p-buf), 
			 "%04d-%04d   ",
			 i, (i+20 < data_len) ? i+20 : data_len);
	p += 12;

	for (j=0; j<20 && i<data_len && p<(buf+length-10); ++j, ++i) {
	    pj_ansi_sprintf(p, "%02x ", (*data) & 0xFF);
	    p += 3;
	    data++;
	}

	pj_ansi_sprintf(p, "\n");
	p++;
    }

    return buf;
}

static int cmp_buf(const pj_uint8_t *s1, const pj_uint8_t *s2, unsigned len)
{
    unsigned i;
    for (i=0; i<len; ++i) {
	if (s1[i] != s2[i])
	    return i;
    }

    return -1;
}

static int fingerprint_test_vector()
{
    pj_pool_t *pool;
    pj_status_t status;
    unsigned i;
    int rc = 0;

    /* To avoid function not referenced warnings */
    (void)create_msgint2;
    (void)create_msgint3;

    PJ_LOG(3,(THIS_FILE, "  draft-denis-behave-rfc3489bis-test-vectors-02"));

    pool = pj_pool_create(mem, "fingerprint", 1024, 1024, NULL);

    for (i=0; i<PJ_ARRAY_SIZE(test_vectors); ++i) {
	struct test_vector *v;
	pj_stun_msg *ref_msg, *msg;
	pj_size_t parsed_len;
	pj_size_t len;
	unsigned pos;
	pj_uint8_t buf[1500];
	char print[1500];
	pj_str_t key;

	PJ_LOG(3,(THIS_FILE, "    Running test %d/%d", i, 
	          PJ_ARRAY_SIZE(test_vectors)));

	v = &test_vectors[i];

	/* Print reference message */
	PJ_LOG(4,(THIS_FILE, "Reference message PDU:\n%s",
	          print_binary((pj_uint8_t*)v->pdu, v->pdu_len)));

	/* Try to parse the reference message first */
	status = pj_stun_msg_decode(pool, (pj_uint8_t*)v->pdu, v->pdu_len,
				    PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, 
				    &ref_msg, &parsed_len, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "    Error decoding reference message"));
	    rc = -1010;
	    goto on_return;
	}

	if (parsed_len != v->pdu_len) {
	    PJ_LOG(1,(THIS_FILE, "    Parsed len error"));
	    rc = -1020;
	    goto on_return;
	}

	/* Print the reference message */
	pj_stun_msg_dump(ref_msg, print, sizeof(print), NULL);
	PJ_LOG(4,(THIS_FILE, "Reference message:\n%s", print));

	/* Create our message */
	msg = v->create(pool, v);
	if (msg == NULL) {
	    PJ_LOG(1,(THIS_FILE, "    Error creating stun message"));
	    rc = -1030;
	    goto on_return;
	}

	/* Encode message */
	if (v->options & USE_MESSAGE_INTEGRITY) {
	    pj_str_t s1, s2, r;

	    pj_stun_create_key(pool, &key, pj_cstr(&r, v->realm), 
			       pj_cstr(&s1, v->username), 
			       PJ_STUN_PASSWD_PLAIN, 
			       pj_cstr(&s2, v->password));
	    pj_stun_msg_encode(msg, buf, sizeof(buf), 0, &key, &len);

	} else {
	    pj_stun_msg_encode(msg, buf, sizeof(buf), 0, NULL, &len);
	}

	/* Print our raw message */
	PJ_LOG(4,(THIS_FILE, "Message PDU:\n%s",
	          print_binary((pj_uint8_t*)buf, len)));

	/* Print our message */
	pj_stun_msg_dump(msg, print, sizeof(print), NULL);
	PJ_LOG(4,(THIS_FILE, "Message is:\n%s", print));

	/* Compare message length */
	if (len != v->pdu_len) {
	    PJ_LOG(1,(THIS_FILE, "    Message length mismatch"));
	    rc = -1050;
	    goto on_return;
	}

	pos = cmp_buf(buf, (const pj_uint8_t*)v->pdu, len);
	if (pos != (unsigned)-1) {
	    PJ_LOG(1,(THIS_FILE, "    Message mismatch at byte %d", pos));
	    rc = -1060;
	    goto on_return;
	}

	/* Authenticate the request/response */
	if (v->options & USE_MESSAGE_INTEGRITY) {
	    if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {
		pj_stun_auth_cred cred;
		pj_status_t status;

		pj_bzero(&cred, sizeof(cred));
		cred.type = PJ_STUN_AUTH_CRED_STATIC;
		cred.data.static_cred.realm = pj_str(v->realm);
		cred.data.static_cred.username = pj_str(v->username);
		cred.data.static_cred.data = pj_str(v->password);
		cred.data.static_cred.nonce = pj_str(v->nonce);

		status = pj_stun_authenticate_request(buf, len, msg, 
						      &cred, pool, NULL, NULL);
		if (status != PJ_SUCCESS) {
		    char errmsg[PJ_ERR_MSG_SIZE];
		    pj_strerror(status, errmsg, sizeof(errmsg));
		    PJ_LOG(1,(THIS_FILE, 
			      "    Request authentication failed: %s",
			      errmsg));
		    rc = -1070;
		    goto on_return;
		}

	    } else if (PJ_STUN_IS_RESPONSE(msg->hdr.type)) {
		pj_status_t status;
		status = pj_stun_authenticate_response(buf, len, msg, &key);
		if (status != PJ_SUCCESS) {
		    char errmsg[PJ_ERR_MSG_SIZE];
		    pj_strerror(status, errmsg, sizeof(errmsg));
		    PJ_LOG(1,(THIS_FILE, 
			      "    Response authentication failed: %s",
			      errmsg));
		    rc = -1080;
		    goto on_return;
		}
	    }
	}	
    }


on_return:
    pj_pool_release(pool);
    return rc;
}

static pj_stun_msg* create_msgint1(pj_pool_t *pool, test_vector *v)
{
    pj_stun_msg *msg;
    pj_timestamp u64;
    pj_str_t s1;
    pj_status_t status;

    status = pj_stun_msg_create(pool, v->msg_type, PJ_STUN_MAGIC,
				(pj_uint8_t*)v->tsx_id, &msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_uint_attr(pool, msg, PJ_STUN_ATTR_PRIORITY, 
				       0x6e0001ff);
    if (status != PJ_SUCCESS)
	goto on_error;

    u64.u32.hi = 0x932ff9b1;
    u64.u32.lo = 0x51263b36;
    status = pj_stun_msg_add_uint64_attr(pool, msg, 
					 PJ_STUN_ATTR_ICE_CONTROLLED, &u64);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_USERNAME, 
					 pj_cstr(&s1, v->username));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_msgint_attr(pool, msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_uint_attr(pool, msg, PJ_STUN_ATTR_FINGERPRINT, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    return msg;

on_error:
    app_perror("    error: create_msgint1()", status);
    return NULL;
}

static pj_stun_msg* create_msgint2(pj_pool_t *pool, test_vector *v)
{
    pj_stun_msg *msg;
    pj_sockaddr_in mapped_addr;
    pj_str_t s1;
    pj_status_t status;

    status = pj_stun_msg_create(pool, v->msg_type, PJ_STUN_MAGIC,
				(pj_uint8_t*)v->tsx_id, &msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_SOFTWARE, 
					 pj_cstr(&s1, "test vector"));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_sockaddr_in_init(&mapped_addr, pj_cstr(&s1, "192.0.2.1"), 
				 32853);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_sockaddr_attr(pool, msg, 
					   PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					   PJ_TRUE, &mapped_addr, 
					   sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_msgint_attr(pool, msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_uint_attr(pool, msg, PJ_STUN_ATTR_FINGERPRINT, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    return msg;

on_error:
    app_perror("    error: create_msgint2()", status);
    return NULL;
}


static pj_stun_msg* create_msgint3(pj_pool_t *pool, test_vector *v)
{
    pj_stun_msg *msg;
    pj_sockaddr mapped_addr;
    pj_str_t s1;
    pj_status_t status;

    status = pj_stun_msg_create(pool, v->msg_type, PJ_STUN_MAGIC,
				(pj_uint8_t*)v->tsx_id, &msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_SOFTWARE, 
					 pj_cstr(&s1, "test vector"));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_sockaddr_init(pj_AF_INET6(), &mapped_addr,
		      pj_cstr(&s1, "2001:db8:1234:5678:11:2233:4455:6677"),
		      32853);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_sockaddr_attr(pool, msg, 
					   PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					   PJ_TRUE, &mapped_addr, 
					   sizeof(pj_sockaddr));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_msgint_attr(pool, msg);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_msg_add_uint_attr(pool, msg, PJ_STUN_ATTR_FINGERPRINT, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    return msg;

on_error:
    app_perror("    error: create_msgint3()", status);
    return NULL;
}


/* Compare two messages */
static int cmp_msg(const pj_stun_msg *msg1, const pj_stun_msg *msg2)
{
    unsigned i;

    if (msg1->hdr.type != msg2->hdr.type)
	return -10;
    if (msg1->hdr.length != msg2->hdr.length)
	return -20;
    if (msg1->hdr.magic != msg2->hdr.magic)
	return -30;
    if (pj_memcmp(msg1->hdr.tsx_id, msg2->hdr.tsx_id, sizeof(msg1->hdr.tsx_id)))
	return -40;
    if (msg1->attr_count != msg2->attr_count)
	return -50;

    for (i=0; i<msg1->attr_count; ++i) {
	const pj_stun_attr_hdr *a1 = msg1->attr[i];
	const pj_stun_attr_hdr *a2 = msg2->attr[i];

	if (a1->type != a2->type)
	    return -60;
	if (a1->length != a2->length)
	    return -70;
    }

    return 0;
}

/* Decode and authenticate message with unknown non-mandatory attribute */
static int handle_unknown_non_mandatory(void)
{
    pj_pool_t *pool = pj_pool_create(mem, NULL, 1000, 1000, NULL);
    pj_stun_msg *msg0, *msg1, *msg2;
    pj_uint8_t data[] = { 1, 2, 3, 4, 5, 6};
    pj_uint8_t packet[500];
    pj_stun_auth_cred cred;
    pj_size_t len;
    pj_status_t rc;

    PJ_LOG(3,(THIS_FILE, "  handling unknown non-mandatory attr"));

    PJ_LOG(3,(THIS_FILE, "    encoding"));
    rc = pj_stun_msg_create(pool, PJ_STUN_BINDING_REQUEST, PJ_STUN_MAGIC, NULL, &msg0);
    rc += pj_stun_msg_add_string_attr(pool, msg0, PJ_STUN_ATTR_USERNAME, &USERNAME);
    rc += pj_stun_msg_add_binary_attr(pool, msg0, 0x80ff, data, sizeof(data));
    rc += pj_stun_msg_add_msgint_attr(pool, msg0);
    rc += pj_stun_msg_encode(msg0, packet, sizeof(packet), 0, &PASSWORD, &len);

#if 0
    if (1) {
	unsigned i;
	puts("");
	printf("{ ");
	for (i=0; i<len; ++i) printf("0x%02x, ", packet[i]);
	puts(" }");
    }
#endif

    PJ_LOG(3,(THIS_FILE, "    decoding"));
    rc += pj_stun_msg_decode(pool, packet, len, PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
			     &msg1, NULL, NULL);

    rc += cmp_msg(msg0, msg1);

    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    cred.data.static_cred.username = USERNAME;
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    cred.data.static_cred.data = PASSWORD;

    PJ_LOG(3,(THIS_FILE, "    authenticating"));
    rc += pj_stun_authenticate_request(packet, len, msg1, &cred, pool, NULL, NULL);

    PJ_LOG(3,(THIS_FILE, "    clone"));
    msg2 = pj_stun_msg_clone(pool, msg1);
    rc += cmp_msg(msg0, msg2);

    pj_pool_release(pool);

    return rc==0 ? 0 : -4410;
}


int stun_test(void)
{
    int pad, rc;

    pad = pj_stun_set_padding_char(32);

    rc = decode_test();
    if (rc != 0)
	goto on_return;

    rc = decode_verify();
    if (rc != 0)
	goto on_return;

    rc = fingerprint_test_vector();
    if (rc != 0)
	goto on_return;

    rc = handle_unknown_non_mandatory();
    if (rc != 0)
	goto on_return;

on_return:
    pj_stun_set_padding_char(pad);
    return rc;
}

