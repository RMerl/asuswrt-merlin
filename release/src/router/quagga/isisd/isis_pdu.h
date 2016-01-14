/*
 * IS-IS Rout(e)ing protocol - isis_pdu.h
 *                             PDU processing
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ZEBRA_ISIS_PDU_H
#define _ZEBRA_ISIS_PDU_H

#ifdef __SUNPRO_C
#pragma pack(1)
#endif

/*
 *                    ISO 9542 - 7.5,7.6
 *
 *                       ES to IS Fixed Header
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |         Intradomain Routeing Protocol Discriminator           |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                       Length Indicator                        |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                  Version/Protocol ID extension                |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                         Reserved = 0                          |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |   0   |   0   |   0   |              PDU Type                 |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                         Holding Time                          | 2
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                          Checksum                             | 2
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 */

struct esis_fixed_hdr
{
  u_char idrp;
  u_char length;
  u_char version;
  u_char id_len;
  u_char pdu_type;
  u_int16_t holdtime;
  u_int16_t checksum;
} __attribute__ ((packed));

#define ESIS_FIXED_HDR_LEN   9

#define ESH_PDU              2
#define ISH_PDU              4
#define RD_PDU               5

/*
 *                       IS to IS Fixed Header
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |         Intradomain Routeing Protocol Discriminator           | 
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                       Length Indicator                        |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                  Version/Protocol ID extension                |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |   R   |   R   |   R   |              PDU Type                 |      
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                            Version                            |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                            Reserved                           |
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 *  |                       Maximum Area Addresses                  |      
 *  +-------+-------+-------+-------+-------+-------+-------+-------+
 */

struct isis_fixed_hdr
{
  u_char idrp;
  u_char length;
  u_char version1;
  u_char id_len;
  u_char pdu_type;
  u_char version2;
  u_char reserved;
  u_char max_area_addrs;
} __attribute__ ((packed));

#define ISIS_FIXED_HDR_LEN 8

/*
 * IS-IS PDU types.
 */

#define L1_LAN_HELLO         15
#define L2_LAN_HELLO         16
/*
 *              L1 and L2 LAN IS to IS Hello PDU header
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                       Reserved                | Circuit Type  | 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Source ID                              + id_len   
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        Holding  Time                          | 2     
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        PDU Length                             | 2    
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   R   |                Priority                               | 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        LAN ID                                 | id_len + 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 */
struct isis_lan_hello_hdr
{
  u_char circuit_t;
  u_char source_id[ISIS_SYS_ID_LEN];
  u_int16_t hold_time;
  u_int16_t pdu_len;
  u_char prio;
  u_char lan_id[ISIS_SYS_ID_LEN + 1];
} __attribute__ ((packed));
#define ISIS_LANHELLO_HDRLEN  19

#define P2P_HELLO            17
/*
 *           Point-to-point IS to IS hello PDU header
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        Reserved               | Circuit Type  | 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Source ID                              + id_len   
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Holding  Time                          + 2     
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        PDU Length                             + 2    
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        Local Circuit ID                       | 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 */
struct isis_p2p_hello_hdr
{
  u_char circuit_t;
  u_char source_id[ISIS_SYS_ID_LEN];
  u_int16_t hold_time;
  u_int16_t pdu_len;
  u_char local_id;
} __attribute__ ((packed));
#define ISIS_P2PHELLO_HDRLEN 12

#define L1_LINK_STATE        18
#define L2_LINK_STATE        20
/*
 *              L1 and L2 IS to IS link state PDU header
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        PDU Length                             + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Remaining Lifetime                     + 2 
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |                        LSP ID                                 | id_len + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Sequence Number                        + 4
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Checksum                               + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * |   P   |              ATT              |LSPDBOL|    ISTYPE     |
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 */
struct isis_link_state_hdr
{
  u_int16_t pdu_len;
  u_int16_t rem_lifetime;
  u_char lsp_id[ISIS_SYS_ID_LEN + 2];
  u_int32_t seq_num;
  u_int16_t checksum;
  u_int8_t lsp_bits;
} __attribute__ ((packed));
#define ISIS_LSP_HDR_LEN 19

/*
 * Since the length field of LSP Entries TLV is one byte long, and each LSP
 * entry is LSP_ENTRIES_LEN (16) bytes long, the maximum number of LSP entries
 * can be accomodated in a TLV is
 * 255 / 16 = 15.
 * 
 * Therefore, the maximum length of the LSP Entries TLV is
 * 16 * 15 + 2 (header) = 242 bytes.
 */
#define MAX_LSP_ENTRIES_TLV_SIZE 242

#define L1_COMPLETE_SEQ_NUM  24
#define L2_COMPLETE_SEQ_NUM  25
/*
 *      L1 and L2 IS to IS complete sequence numbers PDU header
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        PDU Length                             + 2    
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Source ID                              + id_len + 1
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Start LSP ID                           + id_len + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        End LSP ID                             + id_len + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 */
struct isis_complete_seqnum_hdr
{
  u_int16_t pdu_len;
  u_char source_id[ISIS_SYS_ID_LEN + 1];
  u_char start_lsp_id[ISIS_SYS_ID_LEN + 2];
  u_char stop_lsp_id[ISIS_SYS_ID_LEN + 2];
};
#define ISIS_CSNP_HDRLEN 25

#define L1_PARTIAL_SEQ_NUM   26
#define L2_PARTIAL_SEQ_NUM   27
/*
 *      L1 and L2 IS to IS partial sequence numbers PDU header
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        PDU Length                             + 2
 * +-------+-------+-------+-------+-------+-------+-------+-------+
 * +                        Source ID                              + id_len + 1
 * +---------------------------------------------------------------+
 */
struct isis_partial_seqnum_hdr
{
  u_int16_t pdu_len;
  u_char source_id[ISIS_SYS_ID_LEN + 1];
};
#define ISIS_PSNP_HDRLEN 9

#ifdef __SUNPRO_C
#pragma pack()
#endif

/*
 * Function for receiving IS-IS PDUs
 */
int isis_receive (struct thread *thread);

/*
 * calling arguments for snp_process ()
 */
#define ISIS_SNP_PSNP_FLAG 0
#define ISIS_SNP_CSNP_FLAG 1

#define ISIS_AUTH_MD5_SIZE       16U

/*
 * Sending functions
 */
int send_lan_l1_hello (struct thread *thread);
int send_lan_l2_hello (struct thread *thread);
int send_p2p_hello (struct thread *thread);
int send_csnp (struct isis_circuit *circuit, int level);
int send_l1_csnp (struct thread *thread);
int send_l2_csnp (struct thread *thread);
int send_l1_psnp (struct thread *thread);
int send_l2_psnp (struct thread *thread);
int send_lsp (struct thread *thread);
int ack_lsp (struct isis_link_state_hdr *hdr,
	     struct isis_circuit *circuit, int level);
void fill_fixed_hdr (struct isis_fixed_hdr *hdr, u_char pdu_type);
int send_hello (struct isis_circuit *circuit, int level);

#endif /* _ZEBRA_ISIS_PDU_H */
