/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef PACKETIO_H
#define PACKETIO_H

extern void packetio_recv_handler(int fd, void *state);

extern void packetio_tx_hello(void);
extern void packetio_tx_queryresp(void);
extern void packetio_tx_flat(void);
extern void packetio_tx_emitee(topo_emitee_desc_t *ed);
extern void packetio_tx_ack(uint16_t thisSeqNum);
extern void packetio_tx_qltlvResp(uint16_t thisSeqNum, tlv_desc_t *tlvDescr, size_t LtlvOffset);

extern void packetio_invalidate_retxbuf(void);

// ------ for QOS ------ //
extern void  tx_write(uint8_t *buf, size_t nbytes);
extern void* fmt_base(uint8_t *buf, const etheraddr_t *srchw, const etheraddr_t *dsthw, lld2_tos_t tos,
	              topo_opcode_t g_opcode, uint16_t seqnum, bool_t use_broadcast);

#endif /* PACKETIO_H */
