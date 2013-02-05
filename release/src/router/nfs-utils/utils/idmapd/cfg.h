/* $OpenBSD: conf.h,v 1.30 2004/06/25 20:25:34 hshoexer Exp $	 */
/* $EOM: conf.h,v 1.13 2000/09/18 00:01:47 ho Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000, 2003 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _CONF_H_
#define _CONF_H_

#include "queue.h"

struct conf_list_node {
	TAILQ_ENTRY(conf_list_node) link;
	char	*field;
};

struct conf_list {
	size_t	cnt;
	TAILQ_HEAD(conf_list_fields_head, conf_list_node) fields;
};

extern char    *conf_path;

extern int      conf_begin(void);
extern int      conf_decode_base64(u_int8_t *, u_int32_t *, u_char *);
extern int      conf_end(int, int);
extern void     conf_free_list(struct conf_list *);
extern struct sockaddr *conf_get_address(char *, char *);
extern struct conf_list *conf_get_list(char *, char *);
extern struct conf_list *conf_get_tag_list(char *);
extern int      conf_get_num(char *, char *, int);
extern char    *conf_get_str(char *, char *);
extern void     conf_init(void);
extern int      conf_match_num(char *, char *, int);
extern void     conf_reinit(void);
extern int      conf_remove(int, char *, char *);
extern int      conf_remove_section(int, char *);
extern int      conf_set(int, char *, char *, char *, int, int);
extern void     conf_report(void);

#endif				/* _CONF_H_ */
