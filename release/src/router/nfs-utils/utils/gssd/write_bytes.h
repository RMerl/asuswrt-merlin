/*
  Copyright (c) 2004 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _WRITE_BYTES_H_
#define _WRITE_BYTES_H_

#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>		/* for ntohl */

inline static int
write_bytes(char **ptr, const char *end, const void *arg, int arg_len)
{
	char *p = *ptr, *arg_end;

	arg_end = p + arg_len;
	if (arg_end > end || arg_end < p)
		return -1;
	memcpy(p, arg, arg_len);
	*ptr = arg_end;
	return 0;
}

#define WRITE_BYTES(p, end, arg) write_bytes(p, end, &arg, sizeof(arg))

inline static int
write_buffer(char **p, char *end, gss_buffer_desc *arg)
{
	int len = (int)arg->length;		/* make an int out of size_t */
	if (WRITE_BYTES(p, end, len))
		return -1;
	if (*p + len > end)
		return -1;
	memcpy(*p, arg->value, len);
	*p += len;
	return 0;
}

inline static int
write_oid(char **p, char *end, gss_OID_desc *arg)
{
	int len = (int)arg->length;		/* make an int out of size_t */
	if (WRITE_BYTES(p, end, len))
		return -1;
	if (*p + arg->length > end)
		return -1;
	memcpy(*p, arg->elements, len);
	*p += len;
	return 0;
}

static inline int
get_bytes(char **ptr, const char *end, void *res, int len)
{
	char *p, *q;
	p = *ptr;
	q = p + len;
	if (q > end || q < p)
		return -1;
	memcpy(res, p, len);
	*ptr = q;
	return 0;
}

static inline int
get_buffer(char **ptr, const char *end, gss_buffer_desc *res)
{
	char *p, *q;
	p = *ptr;
	int len;
	if (get_bytes(&p, end, &len, sizeof(len)))
		return -1;
	res->length = len;		/* promote to size_t if necessary */
	q = p + res->length;
	if (q > end || q < p)
		return -1;
	if (!(res->value = malloc(res->length)))
		return -1;
	memcpy(res->value, p, res->length);
	*ptr = q;
	return 0;
}

static inline int
xdr_get_u32(u_int32_t **ptr, const u_int32_t *end, u_int32_t *res)
{
	if (get_bytes((char **)ptr, (char *)end, res, sizeof(res)))
		return -1;
	*res = ntohl(*res);
	return 0;
}

static inline int
xdr_get_buffer(u_int32_t **ptr, const u_int32_t *end, gss_buffer_desc *res)
{
	u_int32_t *p, *q;
	u_int32_t len;
	p = *ptr;
	if (xdr_get_u32(&p, end, &len))
		return -1;
	res->length = len;
	q = p + ((res->length + 3) >> 2);
	if (q > end || q < p)
		return -1;
	if (!(res->value = malloc(res->length)))
		return -1;
	memcpy(res->value, p, res->length);
	*ptr = q;
	return 0;
}

static inline int
xdr_write_u32(u_int32_t **ptr, const u_int32_t *end, u_int32_t arg)
{
	u_int32_t tmp;

	tmp = htonl(arg);
	return WRITE_BYTES((char **)ptr, (char *)end, tmp);
}

static inline int
xdr_write_buffer(u_int32_t **ptr, const u_int32_t *end, gss_buffer_desc *arg)
{
	int len = arg->length;
	if (xdr_write_u32(ptr, end, len))
		return -1;
	return write_bytes((char **)ptr, (char *)end, arg->value,
			   (arg->length + 3) & ~3);
}

#endif /* _WRITE_BYTES_H_ */
