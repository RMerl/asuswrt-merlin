/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications Inc                   **
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
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <qcsapi_rpc_common/common/rpc_raw.h>

int qrpc_set_filter(const int sock)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD + BPF_H + BPF_ABS, ETH_ALEN << 1),	/* read packet type id */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,
			QRPC_RAW_SOCK_PROT, 0, 1),			/* if qtn packet */
		BPF_STMT(BPF_RET + BPF_K, ETH_FRAME_LEN),		/* accept packet */
		BPF_STMT(BPF_RET + BPF_K, 0)				/* else  ignore packet */
	};
	struct sock_fprog fp;

	fp.filter = filter;
	fp.len = ARRAY_SIZE(filter);

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &fp, sizeof(fp)) < 0) {
		printf("Cannot set rpc packet filter\n");
		return -1;
	}

	return 0;
}

int str_to_mac(const char *txt_mac, uint8_t *mac)
{
	uint32_t mac_buf[ETH_ALEN];
	int ret;

	if (!txt_mac || !mac)
		return -1;

	ret = sscanf(txt_mac, "%02x:%02x:%02x:%02x:%02x:%02x", &mac_buf[0], &mac_buf[1],
			&mac_buf[2], &mac_buf[3], &mac_buf[4], &mac_buf[5]);

	if (ret != ETH_ALEN)
		return -1;

	while (ret) {
		mac[ret - 1] = (uint8_t)mac_buf[ret - 1];
		--ret;
	}

	return 0;
}

