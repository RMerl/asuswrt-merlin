/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (c) Andreas Schneider            2010.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NT_PRINTING_MIGRATE_H_
#define _NT_PRINTING_MIGRATE_H_

NTSTATUS printing_tdb_migrate_form(TALLOC_CTX *mem_ctx,
				   struct rpc_pipe_client *winreg_pipe,
				   const char *key_name,
				   unsigned char *data,
				   size_t length);
NTSTATUS printing_tdb_migrate_driver(TALLOC_CTX *mem_ctx,
				     struct rpc_pipe_client *winreg_pipe,
				     const char *key_name,
				     unsigned char *data,
				     size_t length,
				     bool do_string_conversion);
NTSTATUS printing_tdb_migrate_printer(TALLOC_CTX *mem_ctx,
				      struct rpc_pipe_client *winreg_pipe,
				      const char *key_name,
				      unsigned char *data,
				      size_t length,
				      bool do_string_conversion);
NTSTATUS printing_tdb_migrate_secdesc(TALLOC_CTX *mem_ctx,
				      struct rpc_pipe_client *winreg_pipe,
				      const char *key_name,
				      unsigned char *data,
				      size_t length);

#endif /* _NT_PRINTING_MIGRATE_H_ */
