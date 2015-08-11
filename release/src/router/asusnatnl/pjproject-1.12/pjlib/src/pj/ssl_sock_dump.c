/* $Id: ssl_sock_dump.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/string.h>


/* Only build when PJ_HAS_SSL_SOCK is enabled */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK!=0

#define THIS_FILE	"ssl_sock_dump.c"

#define CHECK_BUF_LEN()						\
    if ((len < 0) || (len >= end-p)) {				\
	*p = '\0';						\
	return -1;						\
    }								\
    p += len;

PJ_DEF(pj_ssize_t) pj_ssl_cert_info_dump(const pj_ssl_cert_info *ci,
					 const char *indent,
					 char *buf,
					 pj_size_t buf_size)
{
    const char *wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    pj_parsed_time pt1;
    pj_parsed_time pt2;
    unsigned i;
    int len = 0;
    char *p, *end;

    p = buf;
    end = buf + buf_size;

    pj_time_decode(&ci->validity.start, &pt1);
    pj_time_decode(&ci->validity.end, &pt2);

    /* Version */
    len = pj_ansi_snprintf(p, end-p, "%sVersion    : v%d\n", 
			   indent, ci->version);
    CHECK_BUF_LEN();
    
    /* Serial number */
    len = pj_ansi_snprintf(p, end-p, "%sSerial     : ", indent);
    CHECK_BUF_LEN();

    for (i = 0; i < sizeof(ci->serial_no) && !ci->serial_no[i]; ++i);
    for (; i < sizeof(ci->serial_no); ++i) {
	len = pj_ansi_snprintf(p, end-p, "%02X ", ci->serial_no[i]);
	CHECK_BUF_LEN();
    }
    *(p-1) = '\n';
    
    /* Subject */
    len = pj_ansi_snprintf( p, end-p, "%sSubject    : %.*s\n", indent,
			    (int)ci->subject.cn.slen,
			    ci->subject.cn.ptr);
    CHECK_BUF_LEN();
    len = pj_ansi_snprintf( p, end-p, "%s             %.*s\n", indent,
			    (int)ci->subject.info.slen,
			    ci->subject.info.ptr);
    CHECK_BUF_LEN();

    /* Issuer */
    len = pj_ansi_snprintf( p, end-p, "%sIssuer     : %.*s\n", indent,
			    (int)ci->issuer.cn.slen,
			    ci->issuer.cn.ptr);
    CHECK_BUF_LEN();
    len = pj_ansi_snprintf( p, end-p, "%s             %.*s\n", indent,
			    (int)ci->issuer.info.slen,
			    ci->issuer.info.ptr);
    CHECK_BUF_LEN();

    /* Validity period */
    len = pj_ansi_snprintf( p, end-p, "%sValid from : %s %4d-%02d-%02d "
			    "%02d:%02d:%02d.%03d %s\n", indent,
			    wdays[pt1.wday], pt1.year, pt1.mon+1, pt1.day,
			    pt1.hour, pt1.min, pt1.sec, pt1.msec,
			    (ci->validity.gmt? "GMT":""));
    CHECK_BUF_LEN();

    len = pj_ansi_snprintf( p, end-p, "%sValid to   : %s %4d-%02d-%02d "
			    "%02d:%02d:%02d.%03d %s\n", indent,
			    wdays[pt2.wday], pt2.year, pt2.mon+1, pt2.day,
			    pt2.hour, pt2.min, pt2.sec, pt2.msec,
			    (ci->validity.gmt? "GMT":""));
    CHECK_BUF_LEN();

    /* Subject alternative name extension */
    if (ci->subj_alt_name.cnt) {
	unsigned i;

	len = pj_ansi_snprintf(p, end-p, "%ssubjectAltName extension\n", 
			       indent);
	CHECK_BUF_LEN();

	for (i = 0; i < ci->subj_alt_name.cnt; ++i) {
	    const char *type = NULL;

	    switch(ci->subj_alt_name.entry[i].type) {
	    case PJ_SSL_CERT_NAME_RFC822:
		type = "MAIL";
		break;
	    case PJ_SSL_CERT_NAME_DNS:
		type = " DNS";
		break;
	    case PJ_SSL_CERT_NAME_URI:
		type = " URI";
		break;
	    case PJ_SSL_CERT_NAME_IP:
		type = "  IP";
		break;
	    default:
		break;
	    }
	    if (type) {
		len = pj_ansi_snprintf( p, end-p, "%s      %s : %.*s\n", indent, 
					type, 
					(int)ci->subj_alt_name.entry[i].name.slen,
					ci->subj_alt_name.entry[i].name.ptr);
		CHECK_BUF_LEN();
	    }
	}
    }

    return (p-buf);
}


#endif  /* PJ_HAS_SSL_SOCK */

