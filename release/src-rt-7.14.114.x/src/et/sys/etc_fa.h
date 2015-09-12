/*
 * Flow Accelerator setup functions
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

#ifndef _ETC_FA_H_
#define _ETC_FA_H_

#include <bcmutils.h>
#include <siutils.h>
#include <proto/bcmip.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <ctf/hndctf.h>

typedef struct {
	union {
#ifdef BIG_ENDIAN
		struct {
			uint32_t	op_code		:3; /* 31:29 */
			uint32_t	reserved	:5; /* 28:24 */
			uint32_t	cl_id		:8; /* 23:16 */
			uint32_t	reason_code	:8; /* 15:8  */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	src_pid		:5; /* 4:0   */
		} oc0;
		struct {
			uint32_t	op_code		:3; /* 31:29 */
			uint32_t	reserved	:2; /* 28:27 */
			uint32_t	all_bkts_full	:1; /* 26    */
			uint32_t	bkt_id		:2; /* 25:24 */
			uint32_t	napt_flow_id	:8; /* 23:16 */
			uint32_t	hdr_chk_result	:8; /* 15:8  */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	src_pid		:5; /* 4:0   */
		} oc10;
#else
		struct {
			uint32_t	src_pid		:5; /* 4:0   */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	reason_code	:8; /* 15:8  */
			uint32_t	cl_id		:8; /* 23:16 */
			uint32_t	reserved	:5; /* 28:24 */
			uint32_t	op_code		:3; /* 31:29 */
		} oc0;
		struct {
			uint32_t	src_pid		:5; /* 4:0   */
			uint32_t	tc		:3; /* 7:5   */
			uint32_t	hdr_chk_result	:8; /* 15:8  */
			uint32_t	napt_flow_id	:8; /* 23:16 */
			uint32_t	bkt_id		:2; /* 25:24 */
			uint32_t	all_bkts_full	:1; /* 26    */
			uint32_t	reserved	:2; /* 28:27 */
			uint32_t	op_code		:3; /* 31:29 */
		} oc10;
#endif /* BIG_ENDIAN */
		uint32_t word;
	};
} bcm_hdr_t;

#define FA_777WAR_ENABLED	0x01
#define FA_BCM_HDR_RX		0x02
#define FA_BCM_HDR_TX		0x04
#define FA_FA_DEV		0x08
#define FA_AUX_DEV		0x10

#define FA_TX_BCM_HDR(fa)	((fa) && ((fa)->flags & FA_BCM_HDR_TX))
#define FA_RX_BCM_HDR(fa)	((fa) && ((fa)->flags & FA_BCM_HDR_RX))
#define FA_IS_FA_DEV(fa)	((fa) && ((fa)->flags & FA_FA_DEV))
#define FA_IS_AUX_DEV(fa)	((fa) && ((fa)->flags & FA_AUX_DEV))

#define FA_CTF_CAPABLE_DEV(fa)	!FA_IS_AUX_DEV(fa)

typedef struct fa_pub {
	uint32	flags;
} fa_t;

extern fa_t *fa_attach(si_t *sih, void *et, char *vars, uint coreunit, void *robo);
extern void fa_detach(fa_t *fa);
extern int fa_enable_device(fa_t *fa);
extern void *fa_process_tx(fa_t *fa, void *p);
extern void fa_process_rx(fa_t *fa, void *p);
extern uint32 fa_get_nid_rx(osl_t *osh, void *p, int bhdroff);
extern int32 fa_napt_add(fa_t *fa, ctf_ipc_t *ipc, bool v6);
extern int32 fa_napt_del(fa_t *fa, ctf_ipc_t *ipc, bool v6);
extern void fa_napt_live(fa_t *fa, ctf_ipc_t *ipc, bool v6);
extern void fa_conntrack(fa_t *fa, ctf_ipc_t *ipc, bool v6);
extern void fa_et_up(fa_t *fa);
extern void fa_et_down(fa_t *fa);
extern void fa_set_name(fa_t *fa, char *name);
extern void fa_set_aux_unit(si_t *sih, uint unit);
extern char *fa_get_macaddr(si_t *sih, char *vars, uint unit);
extern int fa_read_proc(char *buffer, char **start, off_t offset, int length,
	int *eof, void *data);
extern void fa_dump(fa_t *fai, struct bcmstrbuf *b, bool all);
extern void fa_regs_show(fa_t *fai, struct bcmstrbuf *b);
extern uint fa_core2unit(si_t *sih, uint coreunit);

#endif /* _ETC_FA_H_ */
