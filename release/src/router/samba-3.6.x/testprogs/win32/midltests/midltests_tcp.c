/*
   MIDLTESTS client.

   Copyright (C) Stefan Metzmacher 2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <winsock.h>
#include "midltests.h"

#ifndef _M_AMD64
#error "please run 'vcvarsall.bat amd64' -midltests_tcp needs 64-bit support!"
#endif

#define MIDLTESTS_C_CODE 1
#include "midltests.idl"

#ifndef LISTEN_IP
#define LISTEN_IP "127.0.0.1"
#endif

#ifndef FORWARD_IP
#define FORWARD_IP "127.0.0.1"
#endif

#ifndef CONNECT_IP
#define CONNECT_IP "127.0.0.1"
#endif

struct NDRTcpThreadCtx;

struct NDRProxyThreadCtx {
	const struct NDRTcpThreadCtx *ctx;
	SOCKET InSocket;
	SOCKET OutSocket;
	DWORD dwThreadId;
	HANDLE hThread;
};

struct NDRTcpThreadCtx {
	const char *name;
	short listen_port;
	short client_port;
	BOOL ndr64;
	BOOL stop;
};

struct dcerpc_header {
	BYTE rpc_vers;		/* RPC version */
	BYTE rpc_vers_minor;	/* Minor version */
	BYTE ptype;		/* Packet type */
	BYTE pfc_flags; 	/* Fragmentation flags */
	BYTE drep[4];		/* NDR data representation */
	short frag_length;	/* Total length of fragment */
	short auth_length;	/* authenticator length */
	DWORD call_id;		/* Call identifier */
};

static void dump_packet(const char *ctx, const char *direction,
			const unsigned char *buf, int len)
{
	struct dcerpc_header *hdr = (struct dcerpc_header *)buf;

	if (len < sizeof(struct dcerpc_header)) {
		printf("%s:%s: invalid dcerpc pdu len(%d)\n",
		       ctx, direction, len);
		fflush(stdout);
		return;
	}

	if (hdr->rpc_vers != 5 || hdr->rpc_vers_minor != 0) {
		printf("%s:%s: invalid dcerpc pdu len(%d) ver:%d min:%d\n",
		       ctx, direction, len,
		       hdr->rpc_vers, hdr->rpc_vers_minor);
		fflush(stdout);
		return;
	}

	if (hdr->frag_length != len) {
		printf("%s:%s: invalid dcerpc pdu len(%d) should be (%d)\n",
		       ctx, direction, len, hdr->frag_length);
		fflush(stdout);
		return;
	}


	switch (hdr->ptype) {
	case 0: /* request */
		printf("%s:%s: ptype[request] flen[%d] plen[%d] ahint[%d]\n\n",
		      ctx, direction, hdr->frag_length,
		      len - 24, *(DWORD *)(&buf[0x10]));
		dump_data(buf + 24, len - 24);
		printf("\n");
		fflush(stdout);
		break;

	case 2: /* response */
		printf("\n%s:%s: ptype[response] flen[%d] plen[%d] ahint[%d]\n\n",
		       ctx, direction, hdr->frag_length,
		       len - 24, *(DWORD *)(&buf[0x10]));
		dump_data(buf + 24, len - 24);
		printf("\n");
		fflush(stdout);
		break;

	case 11: /* bind */
#if 0
		printf("%s:%s: ptype[bind] flen[%d] call[%d] contexts[%d]\n\n"
		       ctx, direction, hdr->frag_length, hdr->call_id,
		       buf[24]);
		dump_data(buf + 24, len - 24);
		printf("\n");
		fflush(stdout);
#endif
		break;

	case 12: /* bind ack */
#if 0
		printf("%s:%s: ptype[bind_ack] flen[%d] call[%d]\n\n",
		       ctx, direction, hdr->frag_length, hdr->call_id);
		fflush(stdout);
#endif
		break;

	case 14: /* alter_req */
#if 1
		printf("%s:%s: ptype[alter_req] flen[%d] call[%d] contexts[%d]\n\n",
			   ctx, direction, hdr->frag_length, hdr->call_id,
			   buf[24]);
		//dump_data(buf + 24, len - 24);
		printf("\n");
		fflush(stdout);
#endif
		break;

	case 15: /* alter_ack */
#if 1
		printf("%s:%s: ptype[alter_ack] flen[%d] call[%d]\n\n",
		       ctx, direction, hdr->frag_length, hdr->call_id);
		fflush(stdout);
#endif
		break;

	default:
		printf("%s:%s: ptype[%d] flen[%d] call[%d]\n\n",
		       ctx, direction, hdr->ptype, hdr->frag_length, hdr->call_id);
		fflush(stdout);
		break;
	}
}

static void change_packet(const char *ctx, BOOL ndr64,
			  unsigned char *buf, int len)
{
	struct dcerpc_header *hdr = (struct dcerpc_header *)buf;
	BOOL is_ndr64 = FALSE;
	const unsigned char ndr64_buf[] = {
		0x33, 0x05, 0x71, 0x71, 0xBA, 0xBE, 0x37, 0x49,
		0x83, 0x19, 0xB5, 0xDB, 0xEF, 0x9C, 0xCC, 0x36
	};

	if (len < sizeof(struct dcerpc_header)) {
		printf("%s: invalid dcerpc pdu len(%d)\n",
			   ctx, len);
		fflush(stdout);
		return;
	}

	if (hdr->rpc_vers != 5 || hdr->rpc_vers_minor != 0) {
		printf("%s: invalid dcerpc pdu len(%d) ver:%d min:%d\n",
		       ctx, len,
		       hdr->rpc_vers, hdr->rpc_vers_minor);
		fflush(stdout);
		return;
	}

	if (hdr->frag_length != len) {
		printf("%s: invalid dcerpc pdu len(%d) should be (%d)\n",
		       ctx, len, hdr->frag_length);
		fflush(stdout);
		return;
	}

	switch (hdr->ptype) {
	case 11: /* bind */
	case 14: /* alter_req */

		if (buf[24] >= 2) {
			int ret;

			ret = memcmp(&buf[0x60], ndr64_buf, 16);
			if (ret == 0) {
				is_ndr64 = TRUE;
			}
		}

		if (is_ndr64 && !ndr64) {
			buf[24+0x48] = 0xFF;
			memset(&buf[0x60], 0xFF, 16);
			printf("%s: disable NDR64\n\n", ctx);
		} else if (!is_ndr64 && ndr64) {
			printf("\n%s: got NDR32 downgrade\n\n", ctx);
#ifndef DONOT_FORCE_NDR64
			printf("\n\tERROR!!!\n\n");
			memset(&buf[0x34], 0xFF, 16);
			printf("You may need to run 'vcvarsall.bat amd64' before 'nmake tcp'\n");
#endif
			printf("\n");
		} else if (is_ndr64) {
			printf("%s: got NDR64\n\n", ctx);
		} else {
			printf("%s: got NDR32\n\n", ctx);
		}
		//printf("%s: bind with %u pres\n", ctx, buf[24]);
		fflush(stdout);
		break;
	}
}

static int sock_pending(SOCKET s)
{
	int ret, error;
	int value = 0;
	int len;

	ret = ioctlsocket(s, FIONREAD, &value);
	if (ret == -1) {
		return ret;
	}

	if (ret != 0) {
		/* this should not be reached */
		return -1;
	}

	if (value != 0) {
		return value;
	}

	error = 0;
	len = sizeof(error);

	/*
	 * if no data is available check if the socket is in error state. For
	 * dgram sockets it's the way to return ICMP error messages of
	 * connected sockets to the caller.
	 */
	ret = getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
	if (ret == -1) {
		return ret;
	}
	if (error != 0) {
		return -1;
	}
	return 0;
}

DWORD WINAPI NDRProxyThread(LPVOID lpParameter)
{
	struct NDRProxyThreadCtx *p = (struct NDRProxyThreadCtx *)lpParameter;

	while (!p->ctx->stop) {
		int r, s;
		int ret = -1;
		BYTE buf[5840];

		Sleep(250);

		ret = sock_pending(p->InSocket);
		if (ret == 0) {
			goto out;
		}

		r = recv(p->InSocket, buf, sizeof(buf), 0);
		if (r <= 0) {
			ret = WSAGetLastError();
			printf("%s: recv(in) failed[%d][%d]\n", p->ctx->name, r, ret);
			fflush(stdout);
			goto stop;
		}

		change_packet(p->ctx->name, p->ctx->ndr64, buf, r);
		fflush(stdout);

		dump_packet(p->ctx->name, "in => out", buf, r);
		fflush(stdout);

out:
		s = send(p->OutSocket, buf, r, 0);
		if (s <= 0) {
			ret = WSAGetLastError();
			printf("%s: send(out) failed[%d][%d]\n", p->ctx->name, s, ret);
			fflush(stdout);
			goto stop;
		}

		ret = sock_pending(p->OutSocket);
		if (ret == 0) {
			goto next;
		}

		r = recv(p->OutSocket, buf, sizeof(buf), 0);
		if (r <= 0) {
			ret = WSAGetLastError();
			printf("%s: recv(out) failed[%d][%d]\n", p->ctx->name, r, ret);
			fflush(stdout);
			goto stop;
		}

		dump_packet(p->ctx->name, "out => in", buf, r);
		fflush(stdout);

		s = send(p->InSocket, buf, r, 0);
		if (s <= 0) {
			ret = WSAGetLastError();
			printf("%s: send(in) failed[%d][%d]\n", p->ctx->name, s, ret);
			fflush(stdout);
			goto stop;
		}
next:
		continue;
	}
stop:
	closesocket(p->InSocket);
	closesocket(p->OutSocket);

	printf("NDRTcpThread[%s] stop\n", p->ctx->name);
	fflush(stdout);
	return 0;
}

DWORD WINAPI NDRTcpThread(LPVOID lpParameter)
{
	struct NDRTcpThreadCtx *ctx = (struct NDRTcpThreadCtx *)lpParameter;
	int ret = -1;
	SOCKET ListenSocket;
	struct sockaddr_in saServer;
	struct sockaddr_in saClient;

	//printf("NDRTcpThread[%s] start\n", ctx->name);
	fflush(stdout);

	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		ret = WSAGetLastError();
		printf("socket() failed[%d][%d]\n", ListenSocket, ret);
		fflush(stdout);
		goto failed;
	}

	saServer.sin_family = AF_INET;
	saServer.sin_addr.s_addr = inet_addr(LISTEN_IP);
	saServer.sin_port = htons(ctx->listen_port);

	saClient.sin_family = AF_INET;
	saClient.sin_addr.s_addr = inet_addr(FORWARD_IP);
	saClient.sin_port = htons(ctx->client_port);

	ret = bind(ListenSocket, (SOCKADDR*)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR) {
		ret = WSAGetLastError();
		printf("bind() failed[%d]\n", ret);
		fflush(stdout);
		goto failed;
	}

	ret = listen(ListenSocket, 10);
	if (ret == SOCKET_ERROR) {
		ret = WSAGetLastError();
		printf("listen() failed[%d]\n", ret);
		fflush(stdout);
		goto failed;
	}

	while (!ctx->stop) {
		struct sockaddr_in sa;
		int sa_len = sizeof(sa);
		struct NDRProxyThreadCtx *p = malloc(sizeof(*p));
		p->ctx = ctx;

		p->InSocket = accept(ListenSocket, (SOCKADDR *)&sa, &sa_len);
		if (p->InSocket == INVALID_SOCKET) {
			ret = WSAGetLastError();
			printf("%s: accept() failed[%d][%d]\n", p->ctx->name, p->InSocket, ret);
			fflush(stdout);
			continue;
		}

		p->OutSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (p->OutSocket == INVALID_SOCKET) {
			ret = WSAGetLastError();
			printf("%s: socket(out) failed[%d][%d]\n", p->ctx->name, p->OutSocket, ret);
			fflush(stdout);
			closesocket(p->InSocket);
			continue;
		}

		ret = connect(p->OutSocket, (SOCKADDR*)&saClient, sizeof(saClient));
		if (ret == SOCKET_ERROR) {
			ret = WSAGetLastError();
			printf("%s: connect() failed[%d]\n", p->ctx->name, ret);
			fflush(stdout);
			closesocket(p->InSocket);
			closesocket(p->OutSocket);
			continue;
		}

		p->hThread = CreateThread(
			NULL,           // default security attributes
			0,              // use default stack size
			NDRProxyThread, // thread function name
			p,              // argument to thread function
			0,              // use default creation flags
			&p->dwThreadId);// returns the thread identifier
		if (p->hThread == NULL) {
			printf("failed to create thread ndr32\n");
			fflush(stdout);
			return -1;
		}
	}

	//printf("NDRTcpThread[%s] stop\n", ctx->name);
	fflush(stdout);
	return 0;
failed:
	printf("NDRTcpThread[%s] failed[%d]\n", ctx->name, ret);
	fflush(stdout);
	return ret;
}

struct NDRRpcThreadCtx {
	const char *name;
	short listen_port;
};

DWORD WINAPI NDRRpcThread(LPVOID lpParameter)
{
	struct NDRRpcThreadCtx *ctx = (struct NDRRpcThreadCtx *)lpParameter;
	int ret = -1;
	RPC_STATUS status;
	RPC_BINDING_VECTOR *pBindingVector;

#define RPC_MIN_CALLS 1
#define RPC_MAX_CALLS 20

	//printf("NDRRpcThread[%s] start\n", ctx->name);
	fflush(stdout);
	status = RpcServerUseProtseqEp("ncacn_ip_tcp", RPC_MAX_CALLS, "5055", NULL);
	if (status) {
		printf("Failed to register ncacn_ip_tcp endpoint\n");
		fflush(stdout);
		return status;
	}

	status = RpcServerInqBindings(&pBindingVector);
	if (status) {
		printf("Failed RpcServerInqBindings\n");
		fflush(stdout);
		return status;
	}

#if 0
	status = RpcEpRegister(srv_midltests_v0_0_s_ifspec, pBindingVector, NULL, "midltests server");
	if (status) {
		printf("Failed RpcEpRegister\n");
		fflush(stdout);
		return status;
	}
#endif
	status = RpcServerRegisterIf(srv_midltests_v0_0_s_ifspec, NULL, NULL);
	if (status) {
		printf("Failed to register interface\n");
		fflush(stdout);
		return status;
	}

	status = RpcServerListen(RPC_MIN_CALLS, RPC_MAX_CALLS, FALSE);
	if (status) {
		printf("RpcServerListen returned error %d\n", status);
		fflush(stdout);
		return status;
	}

	printf("NDRRpcThread[%s] stop\n", ctx->name);
	fflush(stdout);
	return 0;
failed:
	printf("NDRRpcThread[%s] failed[%d]\n", ctx->name, ret);
	fflush(stdout);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	struct NDRTcpThreadCtx ctx_ndr32;
	struct NDRTcpThreadCtx ctx_ndr64;
	struct NDRRpcThreadCtx ctx_rpc;
	DWORD   dwThreadIdArray[3];
	HANDLE  hThreadArray[3];
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	char *binding;
	RPC_STATUS status;

	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed with error: %d\n", ret);
		fflush(stdout);
		return -1;
	}

	ctx_ndr32.name = "ndr32";
	ctx_ndr32.listen_port = 5032;
	ctx_ndr32.client_port = 5055;
	ctx_ndr32.ndr64 = FALSE;
	ctx_ndr32.stop = FALSE;
	hThreadArray[0] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size
		NDRTcpThread,           // thread function name
		&ctx_ndr32,             // argument to thread function
		0,                      // use default creation flags
		&dwThreadIdArray[0]);   // returns the thread identifier
	if (hThreadArray[0] == NULL) {
		printf("failed to create thread ndr32\n");
		fflush(stdout);
		return -1;
	}

	ctx_ndr64.name = "ndr64";
	ctx_ndr64.listen_port = 5064;
	ctx_ndr64.client_port = 5055;
	ctx_ndr64.ndr64 = TRUE;
	ctx_ndr64.stop = FALSE;
	hThreadArray[1] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size
		NDRTcpThread,           // thread function name
		&ctx_ndr64,             // argument to thread function
		0,                      // use default creation flags
		&dwThreadIdArray[1]);   // returns the thread identifier
	if (hThreadArray[1] == NULL) {
		printf("failed to create thread ndr64\n");
		fflush(stdout);
		return -1;
	}

	ctx_rpc.name = "rpc";
	ctx_rpc.listen_port = 5050;
	hThreadArray[2] = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size
		NDRRpcThread,           // thread function name
		&ctx_rpc,               // argument to thread function
		0,                      // use default creation flags
		&dwThreadIdArray[2]);   // returns the thread identifier
	if (hThreadArray[2] == NULL) {
		printf("failed to create thread rpc\n");
		fflush(stdout);
		return -1;
	}

	printf("Wait for setup of server threads\n");
	fflush(stdout);
	ret = WaitForMultipleObjects(3, hThreadArray, TRUE, 3000);
	if (ret == WAIT_TIMEOUT) {
		/* OK */
	} else {
		printf("Failed to setup of server threads %d:%d\n",
			ret, GetLastError());
		fflush(stdout);
		return -1;
	}
	ret = 0;

	printf("\nTest NDR32\n\n");
	fflush(stdout);
	binding = "ncacn_ip_tcp:" CONNECT_IP "[5032]";
	status = RpcBindingFromStringBinding(
			binding,
			&midltests_IfHandle);
	if (status) {
		printf("RpcBindingFromStringBinding returned %d\n", status);
		fflush(stdout);
		return status;
	}

	RpcTryExcept {
		midltests();
	} RpcExcept(1) {
		ret = RpcExceptionCode();
		printf("NDR32 Runtime error 0x%x\n", ret);
		fflush(stdout);
	} RpcEndExcept
	ctx_ndr32.stop = TRUE;

	Sleep(250);

	printf("\nTest NDR64\n\n");
	binding = "ncacn_ip_tcp:" CONNECT_IP "[5064]";
	status = RpcBindingFromStringBinding(
			binding,
			&midltests_IfHandle);
	if (status) {
		printf("RpcBindingFromStringBinding returned %d\n", status);
		fflush(stdout);
		return status;
	}

	RpcTryExcept {
		midltests();
	} RpcExcept(1) {
		ret = RpcExceptionCode();
		printf("Runtime error 0x%x\n", ret);
		fflush(stdout);
	} RpcEndExcept
	ctx_ndr64.stop = TRUE;

	WaitForMultipleObjects(3, hThreadArray, TRUE, 2000);

	if (ret == 0) {
		printf("\nTest OK\n");
		fflush(stdout);
	} else {
		printf("\nTest FAILED[%d]\n", ret);
		fflush(stdout);
	}

	return ret;
}
