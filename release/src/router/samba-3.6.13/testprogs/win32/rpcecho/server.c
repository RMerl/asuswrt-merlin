/* 
   RPC echo server.

   Copyright (C) Tim Potter 2003
   
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

#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rpcecho.h"

#define RPC_MIN_CALLS 1
#define RPC_MAX_CALLS 20
#define RPC_ENDPOINT "\\pipe\\rpcecho"

void AddOne(int in_data, __RPC_FAR int *out_data)
{
	printf("AddOne: got in_data = %d\n", in_data);
	*out_data = in_data + 1;
}

void EchoData(int len, unsigned char __RPC_FAR in_data[],
	unsigned char __RPC_FAR out_data[])
{
	printf("EchoData: got len = %d\n", len);
	
	memcpy(out_data, in_data, len);
}

void SinkData(int len, unsigned char __RPC_FAR in_data[  ])
{
	printf("SinkData: got len = %d\n", len);
}

void SourceData(int len, unsigned char __RPC_FAR out_data[  ])
{
	int i;

	printf("SourceData: got len = %d\n", len);

	for (i = 0; i < len; i++)
		out_data[i] = i & 0xff;
}

void TestCall(wchar_t **s1, wchar_t **s2)
{
	if (*s1) {
		printf("s1='%S'\n", *s1);
	} else {
		printf("s1=NULL\n");
	}
	*s2 = L"test string";
}

long TestCall2(short level, echo_Info **info)
{
	static echo_Info i;

	printf("TestCall2 level %d\n", level);

	*info = &i;

	switch (level) {
	case 1:
		i.info1.v = 10;
		break;
	case 2:
		i.info2.v = 20;
		break;
	case 3:
		i.info3.v = 30;
		break;
	case 4:
		i.info4.v = 40;
		break;
	case 5:
		i.info5.v1 = 50;
		i.info5.v2 = 51;
		break;
	case 6:
		i.info6.v1 = 60;
		i.info6.info1.v = 61;
		break;
	case 7:
		i.info7.v1 = 70;
		i.info7.info4.v = 71;
		break;
	default:
		return -1;
	}
	return 0;
}

#if 0
void TestSleep(PRPC_ASYNC_STATE pAsync, long seconds)
{
	long ret;
	printf("async Sleeping for %d seconds\n", seconds);
	Sleep(1000 * seconds);
	ret = seconds;
	RpcAsyncCompleteCall(pAsync, &ret);
}
#else
long TestSleep(long seconds)
{
	printf("non-async Sleeping for %d seconds\n", seconds);
	Sleep(1000 * seconds);
	return seconds;
}
#endif


void echo_TestEnum(echo_Enum1 *foo1,
		   echo_Enum2 *foo2,
		   echo_Enum3 *foo3)
{
	foo2->e1 = ECHO_ENUM2;
}

void echo_TestSurrounding(echo_Surrounding *data)
{
	printf("Incoming array of size %d\n", data->x);
	data->x *= 2;
}

short echo_TestDoublePointer(short ***data)
{
	if (!*data) {
		printf("WARNING: *data == NULL\n");
		return 0;
	}
	if (!**data) {
		printf("WARNING: **data == NULL\n");
		return 0;
	}
	printf("Incoming double pointer: %d\n", ***data);
	return ***data;
}

void main(int argc, char **argv)
{
	RPC_STATUS status;
	RPC_BINDING_VECTOR *pBindingVector;

	if (argc != 1) {
		printf("Usage: rpcechosrv\n");
		exit(0);
	}

	status = RpcServerUseProtseqEp("ncacn_np", RPC_MAX_CALLS, "\\pipe\\rpcecho", NULL);
	if (status) {
		printf("Failed to register ncacn_np endpoint\n");
		exit(status);
	}

	status = RpcServerUseProtseqEp("ncacn_ip_tcp", RPC_MAX_CALLS, "1234", NULL);
	if (status) {
		printf("Failed to register ncacn_ip_tcp endpoint\n");
		exit(status);
	}

	status = RpcServerInqBindings(&pBindingVector);
	if (status) {
		printf("Failed RpcServerInqBindings\n");
		exit(status);
	}

	status = RpcEpRegister(rpcecho_v1_0_s_ifspec, pBindingVector, NULL, "rpcecho server");
	if (status) {
		printf("Failed RpcEpRegister\n");
		exit(status);
	}

	status = RpcServerRegisterIf(rpcecho_v1_0_s_ifspec, NULL, NULL);

	if (status) {
		printf("Failed to register interface\n");
		exit(status);
	}

	status = RpcServerRegisterAuthInfo(NULL, RPC_C_AUTHN_GSS_NEGOTIATE, NULL, NULL);
	if (status) {
		printf("Failed to setup auth info\n");
	}
		
	status = RpcServerListen(RPC_MIN_CALLS, RPC_MAX_CALLS, FALSE);

	if (status) {
		printf("RpcServerListen returned error %d\n", status);
		exit(status);
	}
}

