/*
   Unix SMB/CIFS implementation.

   Manually parsed structures found in the DCERPC protocol

   Copyright (C) Stefan Metzmacher 2014
   Copyright (C) Gregor Beck 2014

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

enum ndr_err_code ndr_pop_dcerpc_sec_verification_trailer(
	struct ndr_pull *ndr, TALLOC_CTX *mem_ctx,
	struct dcerpc_sec_verification_trailer **_r);
