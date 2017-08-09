/*
   Unix SMB/CIFS implementation.

   tstream based generic authentication interface

   Copyright (c) 2010 Stefan Metzmacher
   Copyright (c) 2010 Andreas Schneider <asn@redhat.com>

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

#ifndef _GENSEC_TSTREAM_H_
#define _GENSEC_TSTREAM_H_

struct gensec_context;
struct tstream_context;

NTSTATUS _gensec_create_tstream(TALLOC_CTX *mem_ctx,
				struct gensec_security *gensec_security,
				struct tstream_context *plain_tstream,
				struct tstream_context **gensec_tstream,
				const char *location);
#define gensec_create_tstream(mem_ctx, gensec_security, \
			      plain_tstream, gensec_tstream) \
	_gensec_create_tstream(mem_ctx, gensec_security, \
			       plain_tstream, gensec_tstream, \
			       __location__)

#endif /* _GENSEC_TSTREAM_H_ */
