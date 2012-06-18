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
 * Copyright (c) 2005-2006 Douglas Gilbert.
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
 * Version 1.4 20070904
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sdparm.h"

/*
 * This is a maintenance program for checking the integrity of
 * data in the sdparm_data.c tables.
 *
 * Build with a line like:
 *      gcc -Wall -o chk_sdparm_data sdparm_data.o sdparm_data_vendor.o chk_sdparm_data.c
 *
 */

#define MAX_MP_LEN 1024
#define MAX_PDT  0x12

static unsigned char cl_pdt_arr[MAX_PDT + 1][MAX_MP_LEN];
static unsigned char cl_common_arr[MAX_MP_LEN];

static void clear_cl()
{
    memset(cl_pdt_arr, 0, sizeof(cl_pdt_arr));
    memset(cl_common_arr, 0, sizeof(cl_common_arr));
}

/* result: 0 -> good, 1 -> clash at given pdt, 2 -> clash
 *         other than given pdt, -1 -> bad input */
static int check_cl(int off, int pdt, unsigned char mask)
{
    int k;

    if (off >= MAX_MP_LEN)
        return -1;
    if (pdt < 0) {
        if (cl_common_arr[off] & mask)
            return 1;
        for (k = 0; k <= MAX_PDT; ++k) {
            if (cl_pdt_arr[k][off] & mask)
                return 2;
        }
        return 0;
    } else if (pdt <= MAX_PDT) {
        if (cl_pdt_arr[pdt][off] & mask)
            return 1;
        if (cl_common_arr[off] & mask)
            return 2;
        return 0;
    }
    return -1;
}

static void set_cl(int off, int pdt, unsigned char mask)
{
    if (off < MAX_MP_LEN) {
        if (pdt < 0)
            cl_common_arr[off] |= mask;
        else if (pdt <= MAX_PDT)
            cl_pdt_arr[pdt][off] |= mask;
    }
}

static void check(const struct sdparm_mode_page_item * mpi,
                  const struct sdparm_mode_page_item * mpi_b)
{
    unsigned char mask;
    const struct sdparm_mode_page_item * kp = mpi;
    const struct sdparm_mode_page_item * jp = mpi;
    const char * acron;
    int res, prev_mp, prev_msp, prev_pdt, sbyte, sbit, nbits;
    int second_k = 0;
    int second_j = 0;

    clear_cl();
    for (prev_mp = 0, prev_msp = 0, prev_pdt = -1; ; ++kp) {
        if (NULL == kp->acron) {
            if ((NULL == mpi_b) || second_k)
                break;
            prev_mp = 0;
            prev_msp = 0;
            kp = mpi_b;
            second_k = 1;
        }
        acron = kp->acron ? kp->acron : "?";
        if ((prev_mp != kp->page_num) || (prev_msp != kp->subpage_num)) {
            if (prev_mp > kp->page_num)
                printf("  mode page 0x%x,0x%x out of order\n", kp->page_num,
                        kp->subpage_num);
            if ((prev_mp == kp->page_num) && (prev_msp > kp->subpage_num))
                printf("  mode subpage 0x%x,0x%x out of order, previous msp "
                       "was 0x%x\n", kp->page_num, kp->subpage_num, prev_msp);
            prev_mp = kp->page_num;
            prev_msp = kp->subpage_num;
            prev_pdt = kp->pdt;
            clear_cl();
        } else if ((prev_pdt >= 0) && (prev_pdt != kp->pdt)) {
            if (prev_pdt > kp->pdt)
                printf("  mode page 0x%x,0x%x pdt out of order, pdt was "
                       "%d, now %d\n", kp->page_num, kp->subpage_num,
                       prev_pdt, kp->pdt);
            prev_pdt = kp->pdt;
        }
        for (jp = kp + 1, second_j = second_k; ; ++jp) {
            if (NULL == jp->acron) {
                if ((NULL == mpi_b) || second_j)
                    break;
                jp = mpi_b;
                second_j = 1;
            }
            if ((0 == strcmp(acron, jp->acron)) &&
                (! (jp->flags & MF_CLASH_OK)))
                printf("  acronym '%s' with this description: '%s'\n    "
                       "clashes with '%s'\n", acron, kp->description,
                       jp->description);
        }
        sbyte = kp->start_byte;
        if ((unsigned)sbyte + 8 > MAX_MP_LEN) {
            printf("  acronym: %s  start byte too large: %d\n", kp->acron,
                   sbyte);
            continue;
        }
        sbit = kp->start_bit;
        if ((unsigned)sbit > 7) {
            printf("  acronym: %s  start bit too large: %d\n", kp->acron,
                   sbit);
            continue;
        }
        nbits = kp->num_bits;
        if (nbits > 64) {
            printf("  acronym: %s  number of bits too large: %d\n",
                   kp->acron, nbits);
            continue;
        }
        if (nbits < 1) {
            printf("  acronym: %s  number of bits too small: %d\n",
                   kp->acron, nbits);
            continue;
        }
        mask = (1 << (sbit + 1)) - 1;
        if ((nbits - 1) < sbit)
            mask &= ~((1 << (sbit + 1 - nbits)) - 1);
        res = check_cl(sbyte, kp->pdt, mask);
        if (res) {
            if (1 == res)
                printf("  0x%x,0x%x: clash at start_byte: %d, bit: %d "
                       "[latest acron: %s, this pdt]\n", prev_mp, prev_msp,
                       sbyte, sbit, acron);
            else if (2 == res)
                printf("  0x%x,0x%x: clash at start_byte: %d, bit: %d "
                       "[latest acron: %s, another pdt]\n", prev_mp,
                       prev_msp, sbyte, sbit, acron);
            else
                printf("  0x%x,0x%x: clash, bad data at start_byte: %d, "
                       "bit: %d [latest acron: %s]\n", prev_mp,
                       prev_msp, sbyte, sbit, acron);
        }
        set_cl(sbyte, kp->pdt, mask); 
        if ((nbits - 1) > sbit) {
            nbits -= (sbit + 1);
            if ((nbits > 7) && (0 != (nbits % 8)))
                printf("  0x%x,0x%x: check nbits: %d, start_byte: %d, bit: "
                       "%d [acron: %s]\n", prev_mp, prev_msp, kp->num_bits,
                       sbyte, sbit, acron);
            do {
                ++sbyte;
                mask = 0xff;
                if (nbits > 7)
                    nbits -= 8;
                else {
                    mask &= ~((1 << (8 - nbits)) - 1);
                    nbits = 0;
                }
                res = check_cl(sbyte, kp->pdt, mask);
                if (res) {
                    if (1 == res)
                        printf("   0x%x,0x%x: clash at start_byte: %d, "
                               "bit: %d [latest acron: %s, this pdt]\n",
                               prev_mp, prev_msp, sbyte, sbit, acron);
                    else if (2 == res)
                        printf("   0x%x,0x%x: clash at start_byte: %d, "
                               "bit: %d [latest acron: %s, another pdt]\n",
                               prev_mp, prev_msp, sbyte, sbit, acron);
                    else
                        printf("   0x%x,0x%x: clash, bad at start_byte: "
                               "%d, bit: %d [latest acron: %s]\n",
                               prev_mp, prev_msp, sbyte, sbit, acron);
                }
                set_cl(sbyte, kp->pdt, mask); 
            } while (nbits > 0);
        }
    }
}

static const char * get_vendor_name(int vendor_num)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vendor_num == vnp->vendor_num)
            return vnp->name;
    }
    return NULL;
}

int main(int argc, char ** argv)
{
    int k;
    const struct sdparm_transport_pair * tp;
    const struct sdparm_vendor_pair * vp;
    const char * ccp;

    printf("    Check integrity of mode page item tables in sdparm\n");
    printf("    ==================================================\n\n");
    printf("Generic (i.e. non-transport specific) mode page items:\n");
    check(sdparm_mitem_arr, NULL);
    printf("\n");
    tp = sdparm_transport_mp;
    for (k = 0; k < 16; ++k, ++tp) {
        if (tp->mitem) {
            printf("%s mode page items:\n", sdparm_transport_id[k].name);
            check(tp->mitem, NULL);
            printf("\n");
        }
    }
    vp = sdparm_vendor_mp;
    for (k = 0; k < sdparm_vendor_mp_len; ++k, ++vp) {
        if (vp->mitem) {
            ccp = get_vendor_name(k);
            if (ccp)
                printf("%s mode page items:\n", ccp);
            else
                printf("0x%x mode page items:\n", k);
            check(vp->mitem, NULL);
            printf("\n");
        }
    }
    printf("Cross check Generic with SAS mode page items:\n");
    check(sdparm_mitem_arr, sdparm_transport_mp[6].mitem);
    return 0;
}
