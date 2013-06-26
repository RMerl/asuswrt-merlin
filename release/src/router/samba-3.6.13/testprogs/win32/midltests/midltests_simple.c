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

#define MIDLTESTS_C_CODE 1
#include "midltests.idl"

int main(int argc, char **argv)
{
	int ret;

	midltests_IfHandle = NULL;

	RpcTryExcept {
		midltests();
	} RpcExcept(1) {
		ret = RpcExceptionCode();
		printf("Runtime error 0x%x\n", ret);
	} RpcEndExcept

	return ret;
}
