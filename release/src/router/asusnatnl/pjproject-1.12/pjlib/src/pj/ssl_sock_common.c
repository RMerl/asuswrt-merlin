/* $Id: ssl_sock_common.c 3942 2012-01-16 05:05:47Z nanang $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>

/*
 * Initialize the SSL socket configuration with the default values.
 */
PJ_DEF(void) pj_ssl_sock_param_default(pj_ssl_sock_param *param)
{
    pj_bzero(param, sizeof(*param));

    /* Socket config */
    param->sock_af = PJ_AF_INET;
    param->sock_type = pj_SOCK_STREAM();
    param->async_cnt = 1;
    param->concurrency = -1;
    param->whole_data = PJ_TRUE;
    param->send_buffer_size = 8192;
#if !defined(PJ_SYMBIAN) || PJ_SYMBIAN==0
    param->read_buffer_size = 1500;
#endif
    param->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    param->qos_ignore_error = PJ_TRUE;

    /* Security config */
    param->proto = PJ_SSL_SOCK_PROTO_DEFAULT;

	/* TCP connection timeout value */
	param->tcp_timeout = SIP_TCP_TIMEOUT_SEC;
}


PJ_DEF(pj_status_t) pj_ssl_cert_get_verify_status_strings(
						pj_uint32_t verify_status, 
						const char *error_strings[],
						unsigned *count)
{
    unsigned i = 0, shift_idx = 0;
    unsigned unknown = 0;
    pj_uint32_t errs;

    PJ_ASSERT_RETURN(error_strings && count, PJ_EINVAL);

    if (verify_status == PJ_SSL_CERT_ESUCCESS && *count) {
	error_strings[0] = "OK";
	*count = 1;
	return PJ_SUCCESS;
    }

    errs = verify_status;

    while (errs && i < *count) {
	pj_uint32_t err;
	const char *p = NULL;

	if ((errs & 1) == 0) {
	    shift_idx++;
	    errs >>= 1;
	    continue;
	}

	err = (1 << shift_idx);

	switch (err) {
	case PJ_SSL_CERT_EISSUER_NOT_FOUND:
	    p = "The issuer certificate cannot be found";
	    break;
	case PJ_SSL_CERT_EUNTRUSTED:
	    p = "The certificate is untrusted";
	    break;
	case PJ_SSL_CERT_EVALIDITY_PERIOD:
	    p = "The certificate has expired or not yet valid";
	    break;
	case PJ_SSL_CERT_EINVALID_FORMAT:
	    p = "One or more fields of the certificate cannot be decoded "
		"due to invalid format";
	    break;
	case PJ_SSL_CERT_EISSUER_MISMATCH:
	    p = "The issuer info in the certificate does not match to the "
		"(candidate) issuer certificate";
	    break;
	case PJ_SSL_CERT_ECRL_FAILURE:
	    p = "The CRL certificate cannot be found or cannot be read "
		"properly";
	    break;
	case PJ_SSL_CERT_EREVOKED:
	    p = "The certificate has been revoked";
	    break;
	case PJ_SSL_CERT_EINVALID_PURPOSE:
	    p = "The certificate or CA certificate cannot be used for the "
		"specified purpose";
	    break;
	case PJ_SSL_CERT_ECHAIN_TOO_LONG:
	    p = "The certificate chain length is too long";
	    break;
	case PJ_SSL_CERT_EIDENTITY_NOT_MATCH:
	    p = "The server identity does not match to any identities "
		"specified in the certificate";
	    break;
	case PJ_SSL_CERT_EUNKNOWN:
	default:
	    unknown++;
	    break;
	}
	
	/* Set error string */
	if (p)
	    error_strings[i++] = p;

	/* Next */
	shift_idx++;
	errs >>= 1;
    }

    /* Unknown error */
    if (unknown && i < *count)
	error_strings[i++] = "Unknown verification error";

    *count = i;

    return PJ_SUCCESS;
}
