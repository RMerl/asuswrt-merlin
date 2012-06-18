/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Copyright (c) 2005-2007 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "sdparm.h"

/* sdparm_access.c : helpers for sdparm to access tables in
 * sdparm_data.c
 */

int
sdp_get_mp_len(unsigned char * mp)
{
    return (mp[0] & 0x40) ? ((mp[2] << 8) + mp[3] + 4) : (mp[1] + 2);
}

const struct sdparm_mode_page_t *
sdp_get_mode_detail(int page_num, int subpage_num, int pdt, int transp_proto,
                    int vendor_num)
{
    const struct sdparm_mode_page_t * mpp;

    if (vendor_num >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_num);
        mpp = (vpp ? vpp->mpage : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mpp = sdparm_transport_mp[transp_proto].mpage;
    else
        mpp = sdparm_gen_mode_pg;
    if (NULL == mpp)
        return NULL;

    for ( ; mpp->acron; ++mpp) {
        if ((page_num == mpp->page) && (subpage_num == mpp->subpage)) {
            if ((pdt < 0) || (mpp->pdt < 0) || (mpp->pdt == pdt))
                return mpp;
        }
    }
    return NULL;
}

const struct sdparm_mode_page_t *
sdp_get_mpage_name(int page_num, int subpage_num, int pdt, int transp_proto,
                   int vendor_num, int plus_acron, int hex, char * bp,
                   int max_b_len)
{
    int len = max_b_len - 1;
    const struct sdparm_mode_page_t * mpp = NULL;
    const char * cp;

    if (len < 0)
        return mpp;
    bp[len] = '\0';
    mpp = sdp_get_mode_detail(page_num, subpage_num, pdt, transp_proto,
                              vendor_num);
    if (NULL == mpp)
        mpp = sdp_get_mode_detail(page_num, subpage_num, -1, transp_proto,
                                  vendor_num);
    if (mpp && mpp->name) {
        cp = mpp->acron;
        if (NULL == cp)
            cp = "";
        if (hex) {
            if (0 == subpage_num) {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x]", mpp->name, cp,
                             page_num);
                else
                    snprintf(bp, len, "%s [0x%x]", mpp->name, page_num);
            } else {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x,0x%x]", mpp->name, cp,
                             page_num, subpage_num);
                else
                    snprintf(bp, len, "%s [0x%x,0x%x]", mpp->name, page_num,
                             subpage_num);
            }
        } else {
            if (plus_acron)
                snprintf(bp, len, "%s [%s]", mpp->name, cp);
            else
                snprintf(bp, len, "%s", mpp->name);
        }
    } else {
        if (0 == subpage_num)
            snprintf(bp, len, "[0x%x]", page_num);
        else
            snprintf(bp, len, "[0x%x,0x%x]", page_num, subpage_num);
    }
    return mpp;
}

const struct sdparm_mode_page_t *
sdp_find_mp_by_acron(const char * ap, int transp_proto, int vendor_num)
{
    const struct sdparm_mode_page_t * mpp;

    if (vendor_num >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_num);
        mpp = (vpp ? vpp->mpage : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mpp = sdparm_transport_mp[transp_proto].mpage;
    else
        mpp = sdparm_gen_mode_pg;
    if (NULL == mpp)
        return NULL;

    for ( ; mpp->acron; ++mpp) {
        if (0 == strcmp(mpp->acron, ap))
            return mpp;
    }
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_get_vpd_detail(int page_num, int subvalue, int pdt)
{
    const struct sdparm_vpd_page_t * vpp;
    int sv, ty;

    sv = (subvalue < 0) ? 1 : 0;
    ty = (pdt < 0) ? 1 : 0;
    for (vpp = sdparm_vpd_pg; vpp->acron; ++vpp) {
        if ((page_num == vpp->vpd_num) &&
            (sv || (subvalue == vpp->subvalue)) &&
            (ty || (pdt == vpp->pdt)))
            return vpp;
    }
    if (! ty)
        return sdp_get_vpd_detail(page_num, subvalue, -1);
    if (! sv)
        return sdp_get_vpd_detail(page_num, -1, -1);
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_find_vpd_by_acron(const char * ap)
{
    const struct sdparm_vpd_page_t * vpp;

    for (vpp = sdparm_vpd_pg; vpp->acron; ++vpp) {
        if (0 == strcmp(vpp->acron, ap))
            return vpp;
    }
    return NULL;
}

const char *
sdp_get_transport_name(int proto_num)
{
    const struct sdparm_transport_id_t * tip;

    for (tip = sdparm_transport_id; tip->acron; ++tip) {
        if (proto_num == tip->proto_num)
            return tip->name;
    }
    return NULL;
}

const struct sdparm_transport_id_t *
sdp_find_transport_by_acron(const char * ap)
{
    const struct sdparm_transport_id_t * tip;

    for (tip = sdparm_transport_id; tip->acron; ++tip) {
        if (0 == strcmp(tip->acron, ap))
            return tip;
    }
    return NULL;
}

const char *
sdp_get_vendor_name(int vendor_num)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vendor_num == vnp->vendor_num)
            return vnp->name;
    }
    return NULL;
}

const struct sdparm_vendor_name_t *
sdp_find_vendor_by_acron(const char * ap)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (0 == strcmp(vnp->acron, ap))
            return vnp;
    }
    return NULL;
}

const struct sdparm_vendor_pair *
sdp_get_vendor_pair(int vendor_num)
{
     return ((vendor_num >= 0) && (vendor_num < sdparm_vendor_mp_len))
            ? (sdparm_vendor_mp + vendor_num) : NULL;
}

const struct sdparm_mode_page_item *
sdp_find_mitem_by_acron(const char * ap, int * from, int transp_proto,
                        int vendor_num)
{
    int k = 0;
    const struct sdparm_mode_page_item * mpi;

    if (from) {
        k = *from;
        if (k < 0)
            k = 0;
    }
    if (vendor_num >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_num);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mpi = sdparm_transport_mp[transp_proto].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return NULL;

    for (mpi += k; mpi->acron; ++k, ++mpi) {
        if (0 == strcmp(mpi->acron, ap))
            break;
    }
    if (NULL == mpi->acron)
        mpi = NULL;
    if (from)
        *from = (mpi ? (k + 1) : k);
    return mpi;
}

unsigned long long
sdp_get_big_endian(const unsigned char * from, int start_bit, int num_bits)
{
    unsigned long long res;
    int sbit_o1 = start_bit + 1;

    res = (*from++ & ((1 << sbit_o1) - 1));
    num_bits -= sbit_o1;
    while (num_bits > 0) {
        res <<= 8;
        res |= *from++;
        num_bits -= 8;
    }
    if (num_bits < 0)
        res >>= (-num_bits);
    return res;
}

void
sdp_set_big_endian(unsigned long long val, unsigned char * to, int start_bit,
                   int num_bits)
{
    int sbit_o1 = start_bit + 1;
    int mask, num, k, x;

    mask = (8 != sbit_o1) ? ((1 << sbit_o1) - 1) : 0xff;
    k = start_bit - ((num_bits - 1) % 8);
    if (0 != k)
        val <<= ((k > 0) ? k : (8 + k));
    num = (num_bits + 15 - sbit_o1) / 8;
    for (k = 0; k < num; ++k) {
        if ((sbit_o1 - num_bits) > 0)
            mask &= ~((1 << (sbit_o1 - num_bits)) - 1);
        if (k < (num - 1))
            x = (val >> ((num - k - 1) * 8)) & 0xff;
        else
            x = val & 0xff;
        to[k] = (to[k] & ~mask) | (x & mask); 
        mask = 0xff;
        num_bits -= sbit_o1;
        sbit_o1 = 8;
    }
}

unsigned long long
sdp_mp_get_value(const struct sdparm_mode_page_item *mpi,
                 const unsigned char * mp)
{
    return sdp_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                              mpi->num_bits);
}

unsigned long long
sdp_mp_get_value_check(const struct sdparm_mode_page_item *mpi,
                       const unsigned char * mp, int * all_set)
{
    unsigned long long res;

    res = sdp_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                             mpi->num_bits);
    if (all_set) {
        if ((16 == mpi->num_bits) && (0xffff == res))
            *all_set = 1;
        else if ((32 == mpi->num_bits) && (0xffffffff == res))
            *all_set = 1;
        else if ((64 == mpi->num_bits) && (0xffffffffffffffffULL == res))
            *all_set = 1;
        else
            *all_set = 0;
    }
    return res;
}

void
sdp_mp_set_value(unsigned long long val,
                 const struct sdparm_mode_page_item * mpi, unsigned char * mp)
{
    sdp_set_big_endian(val, mp + mpi->start_byte, mpi->start_bit,
                       mpi->num_bits);
}
 
char *
sdp_get_ansi_version_str(int version, int buff_len, char * buff)
{
    version &= 0x7;
    buff[buff_len - 1] = '\0';
    strncpy(buff, sdparm_ansi_version_arr[version], buff_len - 1);
    return buff;
}

char *
sdp_get_pdt_doc_str(int pdt, int buff_len, char * buff)
{
    if ((pdt < -1) || (pdt > 31))
        snprintf(buff, buff_len, "bad pdt");
    else if (-1 == pdt)
        snprintf(buff, buff_len, "SPC-4");
    else
        snprintf(buff, buff_len, "%s", sdparm_pdt_doc_strs[pdt]);
    return buff;
}
