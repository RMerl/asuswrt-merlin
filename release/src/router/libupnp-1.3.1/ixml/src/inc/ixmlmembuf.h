///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _IXML_MEMBUF_H
#define _IXML_MEMBUF_H

#include <stdlib.h>
#include "ixml.h"

#define MINVAL( a, b ) ( (a) < (b) ? (a) : (b) )
#define MAXVAL( a, b ) ( (a) > (b) ? (a) : (b) )

#define	XINLINE inline

#define MEMBUF_DEF_SIZE_INC		20


typedef struct // ixml_membuf 
{
	char	*buf;
				
	size_t	length;
	size_t	capacity;
	size_t	size_inc;

} ixml_membuf;

//--------------------------------------------------
//////////////// functions /////////////////////////
//--------------------------------------------------
/*
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
*/

void ixml_membuf_init(INOUT ixml_membuf *m);
void ixml_membuf_destroy(INOUT ixml_membuf *m);
int ixml_membuf_assign(INOUT ixml_membuf *m, IN const void *buf, 
					 IN size_t buf_len );
int ixml_membuf_assign_str(INOUT ixml_membuf *m, IN const char *c_str );
int ixml_membuf_append(INOUT ixml_membuf *m, IN const void *buf);
int ixml_membuf_append_str(INOUT ixml_membuf *m, IN const char *c_str);
int ixml_membuf_insert(INOUT ixml_membuf *m, IN const void* buf, 
					 IN size_t buf_len, int index );

#endif // _IXML_MEMBUF_H
