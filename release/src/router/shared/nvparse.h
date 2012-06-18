/*
 * Routines for managing persistent storage of port mappings, etc.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: nvparse.h 241398 2011-02-18 03:46:33Z stakita $
 */

#ifndef _nvparse_h_
#define _nvparse_h_

/* 256 entries per list */
#if defined(MAX_NVPARSE)
#undef MAX_NVPARSE
#define MAX_NVPARSE 256
#endif

/* 
 * Automatic (application specific) port forwards are described by a
 * netconf_app_t structure. A specific outbound connection triggers
 * the expectation of one or more inbound connections which may be
 * optionally remapped to a different port range.
 */
extern bool valid_autofw_port(const netconf_app_t *app);
extern bool get_autofw_port(int which, netconf_app_t *app);
extern bool set_autofw_port(int which, const netconf_app_t *app);
extern bool del_autofw_port(int which);

/* 
 * Persistent (static) port forwards are described by a netconf_nat_t
 * structure. On Linux, a netconf_filter_t that matches the target
 * parameters of the netconf_nat_t should also be added to the INPUT
 * and FORWARD tables to ACCEPT the forwarded connection.
 */
extern bool valid_forward_port(const netconf_nat_t *nat);
extern bool get_forward_port(int which, netconf_nat_t *nat);
extern bool set_forward_port(int which, const netconf_nat_t *nat);
extern bool del_forward_port(int which);

/* 
 * Client filters are described by two netconf_filter_t structures that
 * differ in match.src.ipaddr alone (to support IP address ranges)
 */
extern bool valid_filter_client(const netconf_filter_t *start, const netconf_filter_t *end);
extern bool get_filter_client(int which, netconf_filter_t *start, netconf_filter_t *end);
extern bool set_filter_client(int which, const netconf_filter_t *start,
                              const netconf_filter_t *end);
extern bool del_filter_client(int which);

#ifdef __CONFIG_URLFILTER__
/* 
 * URL filters are described by two netconf_urlfilter_t structures that
 * differ in match.src.ipaddr alone (to support IP address ranges)
 */
extern bool valid_filter_url(const netconf_urlfilter_t *start, const netconf_urlfilter_t *end);
extern bool get_filter_url(int which, netconf_urlfilter_t *start, netconf_urlfilter_t *end);
extern bool set_filter_url(int which, const netconf_urlfilter_t *start,
                              const netconf_urlfilter_t *end);
extern bool del_filter_url(int which);
#endif /* __CONFIG_URLFILTER__ */

/*
* WPA/WDS per link configuration. Parameters after 'auth' are
* authentication algorithm dependant:
*
* When auth is "psk", the parameter list is:
*
* 	bool get_wds_wsec(int unit, int which, char *mac, char *role,
*		char *crypto, char *auth, char *ssid, char *passphrase);
*/
extern bool get_wds_wsec(int unit, int which, unsigned char *mac, char *role,
                         char *crypto, char *auth, ...);
extern bool set_wds_wsec(int unit, int which, unsigned char *mac, char *role,
                         char *crypto, char *auth, ...);
extern bool del_wds_wsec(int unit, int which);

/* Conversion routine for deprecated variables */
extern void convert_deprecated(void);

#endif /* _nvparse_h_ */
