/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications, Inc.                 **
**                                                                           **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH0*/
#ifndef RPC_RAW_H
#define RPC_RAW_H
#include <rpc/rpc.h>
#include <linux/if_ether.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif

#define QRPC_RAW_SOCK_PROT       0x88B5

struct qrpc_raw_ethhdr {
	struct ethhdr	eth_hdr;
	uint16_t	seq;	/* Sequence number */
} __attribute__ ((packed));

struct qrpc_raw_ethpkt {
	struct qrpc_raw_ethhdr	hdr;
	char			payload[ETH_FRAME_LEN - sizeof(struct qrpc_raw_ethhdr)];
} __attribute__ ((packed));

extern CLIENT *qrpc_clnt_raw_create(u_long prog, u_long vers,
	const char *const srcif_name, const uint8_t * dmac_addr);
extern SVCXPRT *qrpc_svc_raw_create(int sock, const char *const bind_interface);
extern int qrpc_set_filter(const int sock);
extern int str_to_mac(const char *txt_mac, uint8_t * mac);

#endif
