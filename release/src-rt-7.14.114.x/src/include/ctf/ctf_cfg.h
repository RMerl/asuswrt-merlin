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
 * $Id: $
 */

#ifndef _CTF_CFG_H_
#define _CTF_CFG_H_

#define NETLINK_CTF   20

#define SUCCESS                     0
#define FAILURE                     -1

#define CTFCFG_MAX_SIZE		    sizeof(ctf_cfg_request_t)
#define CTFCFG_MAX_ARG_SIZE         1024

#define CTFCFG_CMD_SUSPEND	    1
#define CTFCFG_CMD_RESUME	    2
#define CTFCFG_CMD_TUPLE_VALID	    3
#define CTFCFG_CMD_DEFAULT_FWD_GET  4
#define CTFCFG_CMD_DEFAULT_FWD_SET  5
#define CTFCFG_CMD_UPD_MARK	    6
#define CTFCFG_CMD_CHANGE_TXIF_TO_BR	7

#define CTFCFG_STATUS_SUCCESS			1
#define CTFCFG_STATUS_FAILURE			2
#define CTFCFG_STATUS_TUPLE_INVALID		3
#define CTFCFG_STATUS_FLOW_ALREADY_SUSPENDED	4
#define CTFCFG_STATUS_FLOW_NOT_SUSPENDED	5
#define CTFCFG_STATUS_DEFAULT_FWD_INVALID	6
#define CTFCFG_STATUS_PROTOCOL_NOT_SUPPORTED	7

typedef union
{
	struct in_addr ip_v4;
	struct in6_addr ip_v6;
} ip_address_t;

typedef struct
{
	int family;

	ip_address_t src_addr;
	ip_address_t dst_addr;

	uint16_t src_port;
	uint16_t dst_port;

	uint8_t protocol;
} ctf_tuple_t;

typedef enum {
	CTF_FWD_FASTPATH,
	CTF_FWD_HOST,	/* i.e. send to network stack */
} ctf_fwd_t;

typedef struct ctf_cfg_request
{
	uint32_t command_id;
	uint32_t status;		/* Command status */
	uint32_t size;			/* Size of the argument */
	uint8_t	 arg[CTFCFG_MAX_ARG_SIZE];
} ctf_cfg_request_t;

#endif /* _CTF_CFG_H_ */
