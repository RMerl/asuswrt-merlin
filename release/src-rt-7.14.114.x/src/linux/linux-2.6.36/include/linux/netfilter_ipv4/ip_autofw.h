/*
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ip_autofw.h,v 1.1 2008-10-02 03:42:40 $
 */

#ifndef _IP_AUTOFW_H
#define _IP_AUTOFW_H

#define AUTOFW_MASTER_TIMEOUT 600	/* 600 secs */

struct ip_autofw_info {
	u_int16_t proto;	/* Related protocol */
	u_int16_t dport[2];	/* Related destination port range */
	u_int16_t to[2];	/* Port range to map related destination port range to */
};

struct ip_autofw_expect {
	u_int16_t dport[2];	/* Related destination port range */
	u_int16_t to[2];	/* Port range to map related destination port range to */
};

#endif /* _IP_AUTOFW_H */
