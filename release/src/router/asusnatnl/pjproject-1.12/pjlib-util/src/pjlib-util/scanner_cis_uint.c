/* $Id: scanner_cis_uint.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/*
 * THIS FILE IS INCLUDED BY scanner.c.
 * DO NOT COMPILE THIS FILE ALONE!
 */


PJ_DEF(void) pj_cis_buf_init( pj_cis_buf_t *cis_buf)
{
    /* Do nothing. */
    PJ_UNUSED_ARG(cis_buf);
}

PJ_DEF(pj_status_t) pj_cis_init(pj_cis_buf_t *cis_buf, pj_cis_t *cis)
{
    PJ_UNUSED_ARG(cis_buf);
    pj_bzero(cis->cis_buf, sizeof(cis->cis_buf));
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_cis_dup( pj_cis_t *new_cis, pj_cis_t *existing)
{
    pj_memcpy(new_cis, existing, sizeof(pj_cis_t));
    return PJ_SUCCESS;
}


