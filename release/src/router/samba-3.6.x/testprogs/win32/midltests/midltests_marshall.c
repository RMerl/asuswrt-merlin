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
#include "midltests.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
static void print_asc(const unsigned char *buf,int len)
{
        int i;
        for (i=0;i<len;i++)
                printf("%c", isprint(buf[i])?buf[i]:'.');
}

void dump_data(const unsigned char *buf1,int len)
{
        const unsigned char *buf = (const unsigned char *)buf1;
        int i=0;
        if (len<=0) return;

        printf("[%03X] ",i);
        for (i=0;i<len;) {
                printf("%02X ",(int)buf[i]);
                i++;
                if (i%8 == 0) printf(" ");
                if (i%16 == 0) {
                        print_asc(&buf[i-16],8); printf(" ");
                        print_asc(&buf[i-8],8); printf("\n");
                        if (i<len) printf("[%03X] ",i);
                }
        }
        if (i%16) {
                int n;
                n = 16 - (i%16);
                printf(" ");
                if (n>8) printf(" ");
                while (n--) printf("   ");
                n = MIN(8,i%16);
                print_asc(&buf[i-(i%16)],n); printf( " " );
                n = (i%16) - n;
                if (n>0) print_asc(&buf[i-n],n);
                printf("\n");
        }
}

#if _WIN32_WINNT < 0x600

void NdrGetBufferMarshall(PMIDL_STUB_MESSAGE stubmsg, unsigned long len, RPC_BINDING_HANDLE hnd)
{
	stubmsg->RpcMsg->Buffer = HeapAlloc(GetProcessHeap(), 0, len);
	memset(stubmsg->RpcMsg->Buffer, 0xef, len);
	stubmsg->RpcMsg->BufferLength = len;
	stubmsg->Buffer = stubmsg->RpcMsg->Buffer;
	stubmsg->BufferLength = stubmsg->RpcMsg->BufferLength;
	stubmsg->fBufferValid = TRUE;
}

void __RPC_STUB midltests_midltests_fn(PRPC_MESSAGE _pRpcMessage);

void NdrSendReceiveMarshall(PMIDL_STUB_MESSAGE StubMsg, unsigned char *buffer)
{
	unsigned long DataRepresentation;

	StubMsg->RpcMsg->BufferLength = buffer - (unsigned char *)StubMsg->RpcMsg->Buffer;

	printf("[in] Buffer[%d/%d]\n",
		StubMsg->RpcMsg->BufferLength, StubMsg->BufferLength);
	dump_data(StubMsg->RpcMsg->Buffer, StubMsg->RpcMsg->BufferLength);

	DataRepresentation = StubMsg->RpcMsg->DataRepresentation;
	StubMsg->RpcMsg->DataRepresentation = NDR_LOCAL_DATA_REPRESENTATION;
	midltests_midltests_fn(StubMsg->RpcMsg);
	StubMsg->RpcMsg->DataRepresentation = DataRepresentation;

	StubMsg->BufferLength = StubMsg->RpcMsg->BufferLength;
	StubMsg->BufferStart = StubMsg->RpcMsg->Buffer;
	StubMsg->BufferEnd = StubMsg->BufferStart + StubMsg->BufferLength;
	StubMsg->Buffer = StubMsg->BufferStart;

	printf("[out] Buffer[%d]\n",
		StubMsg->RpcMsg->BufferLength);
	dump_data(StubMsg->RpcMsg->Buffer, StubMsg->RpcMsg->BufferLength);
}

void NdrServerInitializeNewMarshall(PRPC_MESSAGE pRpcMsg,
				    PMIDL_STUB_MESSAGE pStubMsg,
				    PMIDL_STUB_DESC pStubDesc)
{
	memset(pStubMsg, 0, sizeof(*pStubMsg));
	pStubMsg->RpcMsg = pRpcMsg;
	pStubMsg->Buffer = pStubMsg->BufferStart = pRpcMsg->Buffer;
	pStubMsg->BufferEnd = pStubMsg->Buffer + pRpcMsg->BufferLength;
	pStubMsg->BufferLength = pRpcMsg->BufferLength;
	pStubMsg->pfnAllocate = pStubDesc->pfnAllocate;
	pStubMsg->pfnFree = pStubDesc->pfnFree;
	pStubMsg->StubDesc = pStubDesc;
	pStubMsg->dwDestContext = MSHCTX_DIFFERENTMACHINE;
}

RPC_STATUS WINAPI I_RpcGetBufferMarshall(PRPC_MESSAGE RpcMsg)
{
	RpcMsg->Buffer = HeapAlloc(GetProcessHeap(), 0, RpcMsg->BufferLength);
	memset(RpcMsg->Buffer, 0xcd, RpcMsg->BufferLength);
	return 0;
}

#endif /* _WIN32_WINNT < 0x600 */
