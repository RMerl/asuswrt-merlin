/*
 *  GSSAPI Security Extensions
 *  Krb5 helpers
 *  Copyright (C) Simo Sorce 2010.
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

#ifndef _GSE_KRB5_H_
#define _GSE_KRB5_H_

#ifdef HAVE_KRB5

krb5_error_code gse_krb5_get_server_keytab(krb5_context krbctx,
					   krb5_keytab *keytab);

#endif /* HAVE_KRB5 */

#endif /* _GSE_KRB5_H_ */
