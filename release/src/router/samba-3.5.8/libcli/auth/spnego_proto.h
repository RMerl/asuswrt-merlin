/*
   Unix SMB/CIFS implementation.

   RFC2478 Compliant SPNEGO implementation

   Copyright (C) Jim McDonough <jmcd@us.ibm.com>   2003

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

ssize_t spnego_read_data(TALLOC_CTX *mem_ctx, DATA_BLOB data, struct spnego_data *token);
ssize_t spnego_write_data(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, struct spnego_data *spnego);
bool spnego_free_data(struct spnego_data *spnego);
bool spnego_write_mech_types(TALLOC_CTX *mem_ctx,
			     const char **mech_types,
			     DATA_BLOB *blob);
