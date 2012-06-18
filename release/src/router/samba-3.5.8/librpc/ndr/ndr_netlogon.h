/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special netlogon types

   Copyright (C) Guenther Deschner 2008

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

_PUBLIC_ enum ndr_err_code ndr_push_netr_SamDatabaseID8Bit(struct ndr_push *ndr, int ndr_flags, enum netr_SamDatabaseID8Bit r);
_PUBLIC_ enum ndr_err_code ndr_pull_netr_SamDatabaseID8Bit(struct ndr_pull *ndr, int ndr_flags, enum netr_SamDatabaseID8Bit *r);
_PUBLIC_ void ndr_print_netr_SamDatabaseID8Bit(struct ndr_print *ndr, const char *name, enum netr_SamDatabaseID8Bit r);

_PUBLIC_ enum ndr_err_code ndr_push_netr_DeltaEnum8Bit(struct ndr_push *ndr, int ndr_flags, enum netr_DeltaEnum8Bit r);
_PUBLIC_ enum ndr_err_code ndr_pull_netr_DeltaEnum8Bit(struct ndr_pull *ndr, int ndr_flags, enum netr_DeltaEnum8Bit *r);
_PUBLIC_ void ndr_print_netr_DeltaEnum8Bit(struct ndr_print *ndr, const char *name, enum netr_DeltaEnum8Bit r);
