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
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_cmds_basic.h"

/* sdparm_vpd.c : does mainly VPD page processing associated with the
 * INQUIRY SCSI command.
 */

/* Prints outs an abridged set of device identification designators
   selected by association, designator type and/or code set. */
static int
decode_dev_ids_quiet(unsigned char * buff, int len, int m_assoc,
                     int m_desig_type, int m_code_set)
{
    int m, p_id, c_set, piv, assoc, desig_type, i_len, is_sas;
    int naa, off, u, rtp;
    const unsigned char * ucp;
    const unsigned char * ip;
    unsigned char sas_tport_addr[8];

    rtp = 0;
    memset(sas_tport_addr, 0, sizeof(sas_tport_addr));
    off = -1;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        ucp = buff + off;
        i_len = ucp[3];
        if ((off + i_len + 4) > len) {
            fprintf(stderr, "    VPD page error: designator length longer "
                    "than\n     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        ip = ucp + 4;
        p_id = ((ucp[0] >> 4) & 0xf);
        c_set = (ucp[0] & 0xf);
        piv = ((ucp[1] & 0x80) ? 1 : 0);
        is_sas = (piv && (6 == p_id)) ? 1 : 0;
        assoc = ((ucp[1] >> 4) & 0x3);
        desig_type = (ucp[1] & 0xf);
        switch (desig_type) {
        case 0: /* vendor specific */
            break;
        case 1: /* T10 vendor identification */
            break;
        case 2: /* EUI-64 based */
            if ((8 != i_len) && (12 != i_len) && (16 != i_len))
                fprintf(stderr, "      << expect 8, 12 and 16 byte "
                        "EUI, got %d>>\n", i_len);
            printf("0x");
            for (m = 0; m < i_len; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        case 3: /* NAA */
            if (1 != c_set) {
                fprintf(stderr, "      << unexpected code set %d for "
                        "NAA>>\n", c_set);
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            naa = (ip[0] >> 4) & 0xff;
            if (! ((2 == naa) || (5 == naa) || (6 == naa))) {
                fprintf(stderr, "      << unexpected NAA [0x%x]>>\n", naa);
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            if (2 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 2 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                printf("0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            } else if (5 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 5 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                if ((0 == is_sas) || (1 != assoc)) {
                    printf("0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("\n");
                } else if (rtp) {
                    printf("0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf(",0x%x\n", rtp);
                    rtp = 0;
                } else {
                    if (sas_tport_addr[0]) {
                        printf("0x");
                        for (m = 0; m < 8; ++m)
                            printf("%02x", (unsigned int)sas_tport_addr[m]);
                        printf("\n");
                    }
                    memcpy(sas_tport_addr, ip, sizeof(sas_tport_addr));
                }
            } else if (6 == naa) {
                if (16 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 6 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                printf("0x");
                for (m = 0; m < 16; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            }
            break;
        case 4: /* Relative target port */
            if ((0 == is_sas) || (1 != c_set) || (1 != assoc) || (4 != i_len))
                break;
            rtp = ((ip[2] << 8) | ip[3]);
            if (sas_tport_addr[0]) {
                printf("0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)sas_tport_addr[m]);
                printf(",0x%x\n", rtp);
                memset(sas_tport_addr, 0, sizeof(sas_tport_addr));
                rtp = 0;
            }
            break;
        case 5: /* (primary) Target port group */
            break;
        case 6: /* Logical unit group */
            break;
        case 7: /* MD5 logical unit identifier */
            break;
        case 8: /* SCSI name string */
            if (3 != c_set) {
                fprintf(stderr, "      << expected UTF-8 code_set>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("%s\n", (const char *)ip);
            break;
        default: /* reserved */
            break;
        }
    }
    if (sas_tport_addr[0]) {
        printf("0x");
        for (m = 0; m < 8; ++m)
            printf("%02x", (unsigned int)sas_tport_addr[m]);
        printf("\n");
    }
    if (-2 == u) {
        fprintf(stderr, "VPD page error: short designator near "
                "offset %d\n", off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

/* Prints outs device identification designators selected by association,
   designator type and/or code set. */
static int
decode_dev_ids(const char * print_if_found, unsigned char * buff, int len,
               int m_assoc, int m_desig_type, int m_code_set, int long_out,
               int quiet)
{
    int m, p_id, c_set, piv, assoc, desig_type, i_len;
    int ci_off, c_id, d_id, naa, vsi, printed, off, u;
    unsigned long long vsei;
    unsigned long long id_ext;
    const unsigned char * ucp;
    const unsigned char * ip;

    if (quiet)
        return decode_dev_ids_quiet(buff, len, m_assoc, m_desig_type,
                                    m_code_set);
    off = -1;
    printed = 0;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        ucp = buff + off;
        i_len = ucp[3];
        if ((off + i_len + 4) > len) {
            fprintf(stderr, "    VPD page error: designator length longer "
                    "than\n     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        ip = ucp + 4;
        p_id = ((ucp[0] >> 4) & 0xf);
        c_set = (ucp[0] & 0xf);
        piv = ((ucp[1] & 0x80) ? 1 : 0);
        assoc = ((ucp[1] >> 4) & 0x3);
        desig_type = (ucp[1] & 0xf);
        if (print_if_found && (0 == printed)) {
            printed = 1;
            printf("  %s:\n", print_if_found);
        }
        if (NULL == print_if_found)
            printf("  %s:\n", sdparm_assoc_arr[assoc]);
        printf("    designator type: %s,  code set: %s\n",
               sdparm_desig_type_arr[desig_type], sdparm_code_set_arr[c_set]);
        if (piv && ((1 == assoc) || (2 == assoc)))
            printf("     transport: %s\n", sdparm_transport_proto_arr[p_id]);
        /* printf("    associated with the %s\n", sdparm_assoc_arr[assoc]); */
        switch (desig_type) {
        case 0: /* vendor specific */
            dStrHex((const char *)ip, i_len, 0);
            break;
        case 1: /* T10 vendor identification */
            printf("      vendor id: %.8s\n", ip);
            if (i_len > 8)
                printf("      vendor specific: %.*s\n", i_len - 8, ip + 8);
            break;
        case 2: /* EUI-64 based */
            if (! long_out) {
                if ((8 != i_len) && (12 != i_len) && (16 != i_len)) {
                    fprintf(stderr, "      << expect 8, 12 and 16 byte "
                            "EUI, got %d>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                printf("      0x");
                for (m = 0; m < i_len; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
                break;
            }
            printf("      EUI-64 based %d byte identifier\n", i_len);
            if (1 != c_set) {
                fprintf(stderr, "      << expected binary code_set (1)>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            ci_off = 0;
            if (16 == i_len) {
                ci_off = 8;
                id_ext = 0;
                for (m = 0; m < 8; ++m) {
                    if (m > 0)
                        id_ext <<= 8;
                    id_ext |= ip[m];
                }
                printf("      Identifier extension: 0x%" PRIx64 "\n", id_ext);
            } else if ((8 != i_len) && (12 != i_len)) {
                fprintf(stderr, "      << can only decode 8, 12 and 16 "
                        "byte ids>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            c_id = ((ip[ci_off] << 16) | (ip[ci_off + 1] << 8) |
                    ip[ci_off + 2]);
            printf("      IEEE Company_id: 0x%x\n", c_id);
            vsei = 0;
            for (m = 0; m < 5; ++m) {
                if (m > 0)
                    vsei <<= 8;
                vsei |= ip[ci_off + 3 + m];
            }
            printf("      Vendor Specific Extension Identifier: 0x%" PRIx64
                   "\n", vsei);
            if (12 == i_len) {
                d_id = ((ip[8] << 24) | (ip[9] << 16) | (ip[10] << 8) |
                        ip[11]);
                printf("      Directory ID: 0x%x\n", d_id);
            }
            break;
        case 3: /* NAA */
            if (1 != c_set) {
                fprintf(stderr, "      << unexpected code set %d for "
                        "NAA>>\n", c_set);
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            naa = (ip[0] >> 4) & 0xff;
            if (! ((2 == naa) || (5 == naa) || (6 == naa))) {
                fprintf(stderr, "      << unexpected NAA [0x%x]>>\n", naa);
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            if (2 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 2 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                d_id = (((ip[0] & 0xf) << 8) | ip[1]);
                c_id = ((ip[2] << 16) | (ip[3] << 8) | ip[4]);
                vsi = ((ip[5] << 16) | (ip[6] << 8) | ip[7]);
                if (long_out) {
                    printf("      NAA 2, vendor specific identifier A: "
                           "0x%x\n", d_id);
                    printf("      IEEE Company_id: 0x%x\n", c_id);
                    printf("      vendor specific identifier B: 0x%x\n", vsi);
                    printf("      [0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("]\n");
                }
                printf("      0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            } else if (5 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 5 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                c_id = (((ip[0] & 0xf) << 20) | (ip[1] << 12) | 
                        (ip[2] << 4) | ((ip[3] & 0xf0) >> 4));
                vsei = ip[3] & 0xf;
                for (m = 1; m < 5; ++m) {
                    vsei <<= 8;
                    vsei |= ip[3 + m];
                }
                if (long_out) {
                    printf("      NAA 5, IEEE Company_id: 0x%x\n", c_id);
                    printf("      Vendor Specific Identifier: 0x%" PRIx64
                           "\n", vsei);
                    printf("      [0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("]\n");
                } else {
                    printf("      0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("\n");
                }
            } else if (6 == naa) {
                if (16 != i_len) {
                    fprintf(stderr, "      << unexpected NAA 6 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                c_id = (((ip[0] & 0xf) << 20) | (ip[1] << 12) | 
                        (ip[2] << 4) | ((ip[3] & 0xf0) >> 4));
                vsei = ip[3] & 0xf;
                for (m = 1; m < 5; ++m) {
                    vsei <<= 8;
                    vsei |= ip[3 + m];
                }
                if (long_out) {
                    printf("      NAA 6, IEEE Company_id: 0x%x\n", c_id);
                    printf("      Vendor Specific Identifier: 0x%" PRIx64
                           "\n", vsei);
                    vsei = 0;
                    for (m = 0; m < 8; ++m) {
                        if (m > 0)
                            vsei <<= 8;
                        vsei |= ip[8 + m];
                    }
                    printf("      Vendor Specific Identifier Extension: "
                           "0x%" PRIx64 "\n", vsei);
                    printf("      [0x");
                    for (m = 0; m < 16; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("]\n");
                } else {
                    printf("      0x");
                    for (m = 0; m < 16; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("\n");
                }
            }
            break;
        case 4: /* Relative target port */
            if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, target "
                        "port association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Relative target port: 0x%x\n", d_id);
            break;
        case 5: /* (primary) Target port group */
            if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, target "
                        "port association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Target port group: 0x%x\n", d_id);
            break;
        case 6: /* Logical unit group */
            if ((1 != c_set) || (0 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, logical "
                        "unit association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Logical unit group: 0x%x\n", d_id);
            break;
        case 7: /* MD5 logical unit identifier */
            if ((1 != c_set) || (0 != assoc)) {
                printf("      << expected binary code_set, logical "
                       "unit association>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            printf("      MD5 logical unit identifier:\n");
            dStrHex((const char *)ip, i_len, 0);
            break;
        case 8: /* SCSI name string */
            if (3 != c_set) {
                fprintf(stderr, "      << expected UTF-8 code_set>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            printf("      SCSI name string:\n");
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("      %s\n", (const char *)ip);
            break;
        default: /* reserved */
            dStrHex((const char *)ip, i_len, 0);
            break;
        }
    }
    if (-2 == u) {
        fprintf(stderr, "VPD page error: short designator around "
                "offset %d\n", off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

static int
decode_mode_policy_vpd(unsigned char * buff, int len)
{
    int k, bump;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Mode page policy VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            fprintf(stderr, "Mode page policy VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        printf("  Policy page code: 0x%x", (ucp[0] & 0x3f));
        if (ucp[1])
            printf(",  subpage code: 0x%x\n", ucp[1]);
        else
            printf("\n");
        printf("    MLUS=%d,  Policy: %s\n", !!(ucp[2] & 0x80),
               sdparm_mode_page_policy_arr[ucp[2] & 0x3]);
    }
    return 0;
}

static int
decode_man_net_vpd(unsigned char * buff, int len)
{
    int k, bump, na_len;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Management network addresses VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        printf("  %s, Service type: %s\n",
               sdparm_assoc_arr[(ucp[0] >> 5) & 0x3],
               sdparm_network_service_type_arr[ucp[0] & 0x1f]);
        na_len = (ucp[2] << 8) + ucp[3];
        bump = 4 + na_len;
        if ((k + bump) > len) {
            fprintf(stderr, "Management network addresses VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (na_len > 0) {
            printf("    %s\n", ucp + 4);
        }
    }
    return 0;
}

static int
decode_proto_lu_vpd(unsigned char * buff, int len)
{
    int k, bump, rel_port, desc_len, proto;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Protocol-specific logical unit information VPD "
                "page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = (ucp[0] << 8) + ucp[1];
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = (ucp[6] << 8) + ucp[7];
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            fprintf(stderr, "Protocol-specific logical unit information VPD "
                    "page, short descriptor length=%d, left=%d\n", bump,
                    (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (desc_len > 0) {
            switch (proto) {
            case TPROTO_SAS:
                printf(" Protocol identifier: SAS\n");
                printf(" TLR control supported: %d\n", !!(ucp[8] & 0x1));
                break;
            default:
                fprintf(stderr, "Unexpected proto=%d\n", proto);
                dStrHex((const char *)ucp, bump, 1);
                break;
            }
        }
    }
    return 0;
}

static int
decode_proto_port_vpd(unsigned char * buff, int len)
{
    int k, bump, rel_port, desc_len, proto;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Protocol-specific port information VPD "
                "page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = (ucp[0] << 8) + ucp[1];
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = (ucp[6] << 8) + ucp[7];
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            fprintf(stderr, "Protocol-specific port information VPD "
                    "page, short descriptor length=%d, left=%d\n", bump,
                    (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (desc_len > 0) {
            switch (proto) {
            case TPROTO_SAS:
            default:
                fprintf(stderr, "Unexpected proto=%d\n", proto);
                dStrHex((const char *)ucp, bump, 1);
                break;
            }
        }
    }
    return 0;
}

static int
decode_scsi_ports_vpd(unsigned char * buff, int len, int long_out, int quiet)
{
    int k, bump, rel_port, ip_tid_len, tpd_len, res;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "SCSI Ports VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = (ucp[2] << 8) + ucp[3];
        printf("Relative port=%d\n", rel_port);
        ip_tid_len = (ucp[6] << 8) + ucp[7];
        bump = 8 + ip_tid_len;
        if ((k + bump) > len) {
            fprintf(stderr, "SCSI Ports VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (ip_tid_len > 0) {
            /* 
             * SCSI devices that are both target and initiator are rare.
             * Only target devices can receive this command, so if they
             * are also initiators then print out the "Initiator port
             * transport id" in hex. sg_inq in sg3_utils decodes it.
             */
            printf(" Initiator port transport id:\n");
            dStrHex((const char *)(ucp + 8), ip_tid_len, 1);
        }
        tpd_len = (ucp[bump + 2] << 8) + ucp[bump + 3];
        if ((k + bump + tpd_len + 4) > len) {
            fprintf(stderr, "SCSI Ports VPD page, short descriptor(tgt) "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (tpd_len > 0) {
            res = decode_dev_ids(" Target port descriptor(s)",
                                 ucp + bump + 4, tpd_len, VPD_ASSOC_TPORT,
                                 -1, -1, long_out, quiet);
            if (res)
                return res;
        }
        bump += tpd_len + 4;
    }
    return 0;
}

static int
decode_ext_inq_vpd(unsigned char * buff, int len, int quiet)
{
    if (len < 7) {
        fprintf(stderr, "Extended INQUIRY data VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    if (quiet) {
        printf("spt=%d\n", ((buff[4] >> 3) & 0x7));
        printf("grd_chk=%d\n", !!(buff[4] & 0x4));
        printf("app_chk=%d\n", !!(buff[4] & 0x2));
        printf("ref_chk=%d\n", !!(buff[4] & 0x1));
        printf("GRP_SUP=%d\n", !!(buff[5] & 0x10));
        printf("prior_sup=%d\n", !!(buff[5] & 0x8));
        printf("headsup=%d\n", !!(buff[5] & 0x4));
        printf("ordsup=%d\n", !!(buff[5] & 0x2));
        printf("simpsup=%d\n", !!(buff[5] & 0x1)); 
        printf("corr_d_sup=%d\n", !!(buff[6] & 0x4));
        printf("nv_sup=%d\n", !!(buff[6] & 0x2));
        printf("v_sup=%d\n", !!(buff[6] & 0x1));
        printf("luiclr=%d\n", !!(buff[7] & 0x1));
    } else {
        printf("  SPT: %d  GRD_CHK: %d  APP_CHK: %d  REF_CHK: %d\n",
               ((buff[4] >> 3) & 0x7), !!(buff[4] & 0x4), !!(buff[4] & 0x2),
               !!(buff[4] & 0x1));
        printf("  GRP_SUP: %d  PRIOR_SUP: %d  HEADSUP: %d  ORDSUP: %d  "
               "SIMPSUP: %d\n", !!(buff[5] & 0x10), !!(buff[5] & 0x8),
               !!(buff[5] & 0x4), !!(buff[5] & 0x2), !!(buff[5] & 0x1));
        printf("  CORR_D_SUP: %d  NV_SUP: %d  V_SUP: %d  LUICLR: %d\n",
               !!(buff[6] & 0x4), !!(buff[6] & 0x2), !!(buff[6] & 0x1),
               !!(buff[7] & 0x1));
    }
    return 0;
}

static int decode_ata_info_vpd(unsigned char * buff, int len, int long_out,
                               int do_hex)
{
    char b[80];
    int num, is_be;
    const char * cp;

    if (len < 36) {
        fprintf(stderr, "ATA information VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    memcpy(b, buff + 8, 8);
    b[8] = '\0';
    printf("  SAT Vendor identification: %s\n", b);
    memcpy(b, buff + 16, 16);
    b[16] = '\0';
    printf("  SAT Product identification: %s\n", b);
    memcpy(b, buff + 32, 4);
    b[4] = '\0';
    printf("  SAT Product revision level: %s\n", b);
    if (len < 56)
        return SG_LIB_CAT_MALFORMED;
    if (long_out) {
        printf("  Signature (Device to host FIS):\n");
        dStrHex((const char *)buff + 36, 20, 1);
    }
    if (len < 60)
        return SG_LIB_CAT_MALFORMED;
    is_be = sg_is_big_endian();
    if ((0xec == buff[56]) || (0xa1 == buff[56])) {
        cp = (0xa1 == buff[56]) ? "PACKET " : "";
        printf("  ATA command IDENTIFY %sDEVICE response summary:\n", cp);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 27, 20,
                               is_be, b);
        b[num] = '\0';
        printf("    model: %s\n", b);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 10, 10,
                               is_be, b);
        b[num] = '\0';
        printf("    serial number: %s\n", b);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 23, 4,
                               is_be, b);
        b[num] = '\0';
        printf("    firmware revision: %s\n", b);
        if (long_out)
            printf("  ATA command IDENTIFY %sDEVICE response in hex:\n", cp);
    } else if (long_out)
        printf("  ATA command 0x%x got following response:\n",
               (unsigned int)buff[56]);
    if (len < 572)
        return SG_LIB_CAT_MALFORMED;
    if (do_hex)
        dStrHex((const char *)(buff + 60), 512, 0);
    else if (long_out)
        dWordHex((const unsigned short *)(buff + 60), 256, 0, is_be);
    return 0;
}

static int
decode_block_limits_vpd(unsigned char * buff, int len)
{
    unsigned int u;

    if (len < 16) {
        fprintf(stderr, "Block limits VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    u = (buff[6] << 8) | buff[7];
    printf("  Optimal transfer length granularity: %u blocks\n", u);
    u = (buff[8] << 24) | (buff[9] << 16) | (buff[10] << 8) |
        buff[11];
    printf("  Maximum transfer length: %u blocks\n", u);
    u = (buff[12] << 24) | (buff[13] << 16) | (buff[14] << 8) |
        buff[15];
    printf("  Optimal transfer length: %u blocks\n", u);
    if (len > 19) {     /* added in sbc3r09 */
        u = (buff[16] << 24) | (buff[17] << 16) | (buff[18] << 8) |
            buff[19];
        printf("  Maximum prefetch, xdread, xdwrite transfer length: %u "
               "blocks\n", u);
    }
    return 0;
}

static int
decode_block_dev_chars_vpd(unsigned char * buff, int len)
{
    unsigned int u;

    if (len < 64) {
        fprintf(stderr, "Block device capabilities VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    u = (buff[4] << 8) | buff[5];
    if (0 == u)
        printf("  Medium rotation rate is not reported\n");
    else if (1 == u)
        printf("  Non-rotating medium (e.g. solid state)\n");
    else if ((u < 0x401) || (0xffff == u))
        printf("  Reserved [0x%x]\n", u);
    else
        printf("  Nominal rotation rate: %d rpm\n", u);
    return 0;
}

static int
decode_tape_dev_caps_vpd(unsigned char * buff, int len)
{
    if (len < 6) {
        fprintf(stderr, "Sequential access device capabilities VPD page "
                "length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Worm: %d\n", !!(buff[4] & 0x1));
    return 0;
}

static int
decode_tape_man_ass_sn_vpd(unsigned char * buff, int len)
{
    if (len < 64) {
        fprintf(stderr, "Manufacturer-assigned serial number VPD page "
                "length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Manufacturer-assigned serial number: %.*s\n",
                   len - 4, buff + 4);
    return 0;
}

static int
decode_tapealert_supported_vpd(unsigned char * b, int len)
{
    if (len < 12) {
        fprintf(stderr, "TapeAlert supported flags length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Flag01h: %d  02h: %d  03h: %d  04h: %d  05h: %d  06h: %d  "
           "07h: %d  08h: %d\n", !!(b[4] & 0x80), !!(b[4] & 0x40),
           !!(b[4] & 0x20), !!(b[4] & 0x10), !!(b[4] & 0x8), !!(b[4] & 0x4),
           !!(b[4] & 0x2), !!(b[4] & 0x1));
    printf("  Flag09h: %d  0ah: %d  0bh: %d  0ch: %d  0dh: %d  0eh: %d  "
           "0fh: %d  10h: %d\n", !!(b[5] & 0x80), !!(b[5] & 0x40),
           !!(b[5] & 0x20), !!(b[5] & 0x10), !!(b[5] & 0x8), !!(b[5] & 0x4),
           !!(b[5] & 0x2), !!(b[5] & 0x1));
    printf("  Flag11h: %d  12h: %d  13h: %d  14h: %d  15h: %d  16h: %d  "
           "17h: %d  18h: %d\n", !!(b[6] & 0x80), !!(b[6] & 0x40),
           !!(b[6] & 0x20), !!(b[6] & 0x10), !!(b[6] & 0x8), !!(b[6] & 0x4),
           !!(b[6] & 0x2), !!(b[6] & 0x1));
    printf("  Flag19h: %d  1ah: %d  1bh: %d  1ch: %d  1dh: %d  1eh: %d  "
           "1fh: %d  20h: %d\n", !!(b[7] & 0x80), !!(b[7] & 0x40),
           !!(b[7] & 0x20), !!(b[7] & 0x10), !!(b[7] & 0x8), !!(b[7] & 0x4),
           !!(b[7] & 0x2), !!(b[7] & 0x1));
    printf("  Flag21h: %d  22h: %d  23h: %d  24h: %d  25h: %d  26h: %d  "
           "27h: %d  28h: %d\n", !!(b[8] & 0x80), !!(b[8] & 0x40),
           !!(b[8] & 0x20), !!(b[8] & 0x10), !!(b[8] & 0x8), !!(b[8] & 0x4),
           !!(b[8] & 0x2), !!(b[8] & 0x1));
    printf("  Flag29h: %d  2ah: %d  2bh: %d  2ch: %d  2dh: %d  2eh: %d  "
           "2fh: %d  30h: %d\n", !!(b[9] & 0x80), !!(b[9] & 0x40),
           !!(b[9] & 0x20), !!(b[9] & 0x10), !!(b[9] & 0x8), !!(b[9] & 0x4),
           !!(b[9] & 0x2), !!(b[9] & 0x1));
    printf("  Flag31h: %d  32h: %d  33h: %d  34h: %d  35h: %d  36h: %d  "
           "37h: %d  38h: %d\n", !!(b[10] & 0x80), !!(b[10] & 0x40),
           !!(b[10] & 0x20), !!(b[10] & 0x10), !!(b[10] & 0x8),
           !!(b[10] & 0x4), !!(b[10] & 0x2), !!(b[10] & 0x1));
    printf("  Flag39h: %d  3ah: %d  3bh: %d  3ch: %d  3dh: %d  3eh: %d  "
           "3fh: %d  40h: %d\n", !!(b[11] & 0x80), !!(b[11] & 0x40),
           !!(b[11] & 0x20), !!(b[11] & 0x10), !!(b[11] & 0x8),
           !!(b[11] & 0x4), !!(b[11] & 0x2), !!(b[11] & 0x1));
    return 0;
}

/* Returns 0 if successful, else error */
int
sdp_process_vpd_page(int sg_fd, int pn, int spn,
                     const struct sdparm_opt_coll * opts, int req_pdt)
{
    int res, len, k, verb, dev_pdt, pdt;
    unsigned char b[VPD_ATA_INFO_RESP_LEN];
    int sz;
    unsigned char * up;
    const struct sdparm_vpd_page_t * vpp;

    verb = (opts->verbose > 0) ? opts->verbose - 1 : 0;
    sz = sizeof(b);
    memset(b, 0, sz);
    if (pn < 0) {
        if (opts->all)
            pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
        else
            pn = VPD_DEVICE_ID;  /* default to device identification page */
    }
    sz = (VPD_ATA_INFO == pn) ? VPD_ATA_INFO_RESP_LEN : DEF_INQ_RESP_LEN;
    res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verb);
    if (res) {
        fprintf(stderr, "INQUIRY fetching VPD page=0x%x failed\n", pn);
        return res;
    }
    dev_pdt = b[0] & 0x1f;
    if ((req_pdt >= 0) && (req_pdt != (dev_pdt))) {
        fprintf(stderr, "given peripheral device type [%d] differs "
                "from reported [%d]\n", req_pdt, dev_pdt);
        fprintf(stderr, "  start with given pdt\n");
        pdt = req_pdt;
    } else
        pdt = dev_pdt;
    switch (pn) {
    case VPD_SUPPORTED_VPDS:
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        printf("Supported VPD pages VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        if (len > 0) {
            for (k = 0; k < len; ++k) {
                vpp = sdp_get_vpd_detail(b[4 + k], -1, pdt);
                if (vpp) {
                    if (opts->long_out)
                        printf("  [0x%02x] %s [%s]\n", b[4 + k],
                               vpp->name, vpp->acron);
                    else
                        printf("  %s [%s]\n", vpp->name, vpp->acron);
                } else
                    printf("  0x%x\n", b[4 + k]);
            }
        } else
            printf("  <empty>\n");
        break;
    case VPD_ATA_INFO:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to ATA information VPD page "
                    "truncated\n");
            len = sz;
        }
        if (3 == opts->hex) {   /* output suitable for "hdparm --Istdin" */
            dWordHex((const unsigned short *)(b + 60), 256, -2,
                     sg_is_big_endian());
            return 0;
        }
        if (opts->long_out)
            printf("ATA information [0x89] VPD page:\n");
        else
            printf("ATA information VPD page:\n");
        if (opts->hex && (2 != opts->hex)) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ata_info_vpd(b, len + 4, opts->long_out, opts->hex);
        if (res)
            return res;
        break;
    case VPD_DEVICE_ID:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to device identification VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Device identification [0x83] VPD page:\n");
        else if (! opts->quiet)
            printf("Device identification VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = 0;
        if ((0 == spn) || (VPD_DI_SEL_LU & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_LU], b + 4, len,
                                 VPD_ASSOC_LU, -1, -1, opts->long_out,
                                 opts->quiet);
        if ((0 == spn) || (VPD_DI_SEL_TPORT & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TPORT], b + 4,
                                 len, VPD_ASSOC_TPORT, -1, -1,
                                 opts->long_out, opts->quiet);
        if ((0 == spn) || (VPD_DI_SEL_TARGET & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TDEVICE], b + 4,
                                 len, VPD_ASSOC_TDEVICE, -1, -1,
                                 opts->long_out, opts->quiet);
        if (VPD_DI_SEL_AS_IS & spn)
            res = decode_dev_ids(NULL, b + 4, len, -1, -1, -1,
                                 opts->long_out, opts->quiet);
        if (res)
            return res;
        break;
    case VPD_EXT_INQ:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Extended inquiry data VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Extended inquiry data [0x86] VPD page:\n");
        else if (! opts->quiet)
            printf("Extended inquiry data VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ext_inq_vpd(b, len + 4, opts->quiet);
        if (res)
            return res;
        break;
    case VPD_MAN_NET_ADDR:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Management network addresses VPD "
                    "page truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Management network addresses [0x85] VPD page:\n");
        else
            printf("Management network addresses VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_man_net_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_MODE_PG_POLICY:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Mode page policy VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Mode page policy [0x87] VPD page:\n");
        else
            printf("mode page policy VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_mode_policy_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_PROTO_LU:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Protocol-specific logical unit "
                    "information VPD page truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Protocol-specific logical unit information [0x90] VPD "
                   "page:\n");
        else
            printf("Protocol-specific logical unit information VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_proto_lu_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_PROTO_PORT:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Protocol-specific port "
                    "information VPD page truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Protocol-specific port information [0x91] VPD "
                   "page:\n");
        else
            printf("Protocol-specific port information VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_proto_port_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_SCSI_PORTS:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to SCSI Ports VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("SCSI Ports [0x88] VPD page:\n");
        else
            printf("SCSI Ports VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_scsi_ports_vpd(b, len + 4, opts->long_out, opts->quiet);
        if (res)
            return res;
        break;
    case VPD_SOFTW_INF_ID:
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Software interface identification "
                    "VPD page truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Software interface identification [0x84] VPD page:\n");
        else
            printf("Software interface identification VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        up = b + 4;
        for ( ; len > 5; len -= 6, up += 6) {
            printf("    ");
            for (k = 0; k < 6; ++k)
                printf("%02x", (unsigned int)up[k]);
            printf("\n");
        }
        break;
    case VPD_UNIT_SERIAL_NUM:
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        if (opts->long_out)
            printf("Unit serial number [0x80] VPD page:\n");
        else
            printf("Unit serial number VPD page:\n");
        if (opts->hex)
            dStrHex((const char *)b, len + 4, 0);
        else if (len > 0) {
            if (len + 4 < (int)sizeof(b))
                b[len + 4] = '\0';
            else
                b[sizeof(b) - 1] = '\0';
            printf("  %s\n", b + 4);
        } else
            printf("  <empty>\n");
        break;
    case 0xb0:          /* VPD page depends on pdt */
        {
            int osd = 0;
            int sbc = 0;
            int ssc = 0;
            const char * vpd_name;

            switch (pdt)
            {
            case 0: case 4: case 7:
                vpd_name = "Block limits";
                sbc = 1;
                break;
            case 1: case 8:
                vpd_name = "Sequential access device capabilities";
                ssc = 1;
                break;
            case 0x11:
                vpd_name = "OSD information";
                osd = 1;
                break;
            default:
                vpd_name = "unexpected pdt for B0h";
                break;
            }
            if (b[1] != pn)
                goto dumb_inq;
            len = (b[2] << 8) + b[3];
            if (len > sz) {
                fprintf(stderr, "Response to %s VPD page truncated\n",
                        vpd_name);
                len = sz;
            }
            if (opts->long_out)
                printf("%s [0xb0] VPD page:\n", vpd_name);
            else
                printf("%s VPD page:\n", vpd_name);
            if (opts->hex) {
                dStrHex((const char *)b, len + 4, 0);
                return 0;
            }
            if (ssc)
                res = decode_tape_dev_caps_vpd(b, len + 4);
            else if (sbc)
                res = decode_block_limits_vpd(b, len + 4);
            else
                dStrHex((const char *)b, len + 4, 0);
            if (res)
                return res;
        }
        break;
    case 0xb1:          /* VPD page depends on pdt */
        {
            int adc = 0;
            int osd = 0;
            int sbc = 0;
            int ssc = 0;
            const char * vpd_name;

            switch (pdt)
            {
            case 0: case 4: case 7:
                vpd_name = "Block device characteristics";
                sbc = 1;
                break;
            case 1: case 8:
                vpd_name = "Manufactured assigned serial number";
                ssc = 1;
                break;
            case 0x11:
                vpd_name = "Security token";
                osd = 1;
                break;
            case 0x12:
                vpd_name = "Manufactured assigned serial number";
                adc = 1;
                break;
            default:
                vpd_name = "unexpected pdt for B1h";
                break;
            }
            if (b[1] != pn)
                goto dumb_inq;
            len = (b[2] << 8) + b[3];
            if (len > sz) {
                fprintf(stderr, "Response to %s VPD page truncated\n",
                        vpd_name);
                len = sz;
            }
            if (opts->long_out)
                printf("%s [0xb1] VPD page:\n", vpd_name);
            else
                printf("%s VPD page:\n", vpd_name);
            if (opts->hex) {
                dStrHex((const char *)b, len + 4, 0);
                return 0;
            }
            if (ssc || adc)
                res = decode_tape_man_ass_sn_vpd(b, len + 4);
            else if (sbc)
                res = decode_block_dev_chars_vpd(b, len + 4);
            else
                dStrHex((const char *)b, len + 4, 0);
            if (res)
                return res;
        }
        break;
    case 0xb2:          /* VPD page depends on pdt, only VPD_TA_SUPPORTED */
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        if (opts->long_out)
            printf("TapeAlert supported flags [0xb2] VPD page:\n");
        else
            printf("TapeAlert supported flags VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_tapealert_supported_vpd(b, len + 4);
        if (res)
            return res;
        break;
    default:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3] + 4;
        vpp = sdp_get_vpd_detail(pn, -1, pdt);
        if (vpp)
            fprintf(stderr, "%s VPD page in hex:\n", vpp->name);
        else
            fprintf(stderr, "VPD page 0x%x in hex:\n", pn);
        if (len > (int)sizeof(b)) {
            if (opts->verbose)
                fprintf(stderr, "page length=%d too long, trim\n", len);
            len = sizeof(b);
        }
        dStrHex((const char *)b, len, 0);
        break;
    }
    return 0;

dumb_inq:
    fprintf(stderr, "malformed VPD response, VPD pages probably not "
            "supported\n");
    return SG_LIB_CAT_MALFORMED;
}
