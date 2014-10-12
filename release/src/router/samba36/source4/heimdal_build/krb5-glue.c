/* 
   Unix SMB/CIFS implementation.

   provide glue functions between heimdal and samba

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

#include "heimdal/lib/krb5/krb5_locl.h"

const krb5_cc_ops krb5_scc_ops = {
    KRB5_CC_OPS_VERSION,
    "_NOTSUPPORTED_SDB",
    NULL, /* scc_retrieve */
    NULL, /* scc_get_principal */
    NULL, /* scc_get_first */
    NULL, /* scc_get_next */
    NULL, /* scc_end_get */
    NULL, /* scc_remove_cred */
    NULL, /* scc_set_flags */
    NULL,
    NULL, /* scc_get_cache_first */
    NULL, /* scc_get_cache_next */
    NULL, /* scc_end_cache_get */
    NULL, /* scc_move */
    NULL, /* scc_get_default_name */
    NULL  /* scc_set_default */
};
