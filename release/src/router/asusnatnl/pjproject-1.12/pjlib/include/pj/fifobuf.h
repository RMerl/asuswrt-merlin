/* $Id: fifobuf.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_FIFOBUF_H__
#define __PJ_FIFOBUF_H__

#include <pj/types.h>

PJ_BEGIN_DECL

typedef struct pj_fifobuf_t pj_fifobuf_t;
struct pj_fifobuf_t
{
    char *first, *last;
    char *ubegin, *uend;
    int full;
};

PJ_DECL(void)	     pj_fifobuf_init (pj_fifobuf_t *fb, void *buffer, unsigned size);
PJ_DECL(unsigned)    pj_fifobuf_max_size (pj_fifobuf_t *fb);
PJ_DECL(void*)	     pj_fifobuf_alloc (pj_fifobuf_t *fb, unsigned size);
PJ_DECL(pj_status_t) pj_fifobuf_unalloc (pj_fifobuf_t *fb, void *buf);
PJ_DECL(pj_status_t) pj_fifobuf_free (pj_fifobuf_t *fb, void *buf);

PJ_END_DECL

#endif	/* __PJ_FIFOBUF_H__ */

