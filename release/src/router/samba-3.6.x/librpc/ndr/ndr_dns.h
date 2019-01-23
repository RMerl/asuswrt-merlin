/*
   Unix SMB/CIFS implementation.

   manipulate dns name structures

   Copyright (C) 2010 Kai Blin  <kai@samba.org>

   Heavily based on nbtname.c which is:

   Copyright (C) Andrew Tridgell 2005

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

void ndr_print_dns_string(struct ndr_print *ndr,
			  const char *name,
			  const char *s);
enum ndr_err_code ndr_pull_dns_string(struct ndr_pull *ndr,
				      int ndr_flags,
				      const char **s);
enum ndr_err_code ndr_push_dns_string(struct ndr_push *ndr,
				      int ndr_flags,
				      const char *s);
enum ndr_err_code ndr_push_dns_res_rec(struct ndr_push *ndr,
				       int ndr_flags,
				       const struct dns_res_rec *r);
enum ndr_err_code ndr_pull_dns_res_rec(struct ndr_pull *ndr,
				       int ndr_flags,
				       struct dns_res_rec *r);
