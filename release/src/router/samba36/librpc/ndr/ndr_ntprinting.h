/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special ntprinting structures

   Copyright (C) Guenther Deschner 2010

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

#include "includes.h"
#include "../librpc/gen_ndr/ndr_ntprinting.h"

_PUBLIC_ uint32_t ndr_ntprinting_string_flags(uint32_t string_flags);

_PUBLIC_ enum ndr_err_code ndr_pull_ntprinting_printer(struct ndr_pull *ndr, int ndr_flags, struct ntprinting_printer *r);
