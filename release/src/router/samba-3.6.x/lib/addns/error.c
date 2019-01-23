/*
  Linux DNS client library implementation
  Copyright (C) 2010 Guenther Deschner

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "dns.h"
#include "dnserr.h"

typedef struct {
	const char *dns_errstr;
	DNS_ERROR dns_errcode;
} dns_err_code_struct;

static const dns_err_code_struct dns_errs[] =
{
	{ "ERROR_DNS_SUCCESS", ERROR_DNS_SUCCESS },
	{ "ERROR_DNS_RECORD_NOT_FOUND", ERROR_DNS_RECORD_NOT_FOUND },
	{ "ERROR_DNS_BAD_RESPONSE", ERROR_DNS_BAD_RESPONSE },
	{ "ERROR_DNS_INVALID_PARAMETER", ERROR_DNS_INVALID_PARAMETER },
	{ "ERROR_DNS_NO_MEMORY", ERROR_DNS_NO_MEMORY },
	{ "ERROR_DNS_INVALID_NAME_SERVER", ERROR_DNS_INVALID_NAME_SERVER },
	{ "ERROR_DNS_CONNECTION_FAILED", ERROR_DNS_CONNECTION_FAILED },
	{ "ERROR_DNS_GSS_ERROR", ERROR_DNS_GSS_ERROR },
	{ "ERROR_DNS_INVALID_NAME", ERROR_DNS_INVALID_NAME },
	{ "ERROR_DNS_INVALID_MESSAGE", ERROR_DNS_INVALID_MESSAGE },
	{ "ERROR_DNS_SOCKET_ERROR", ERROR_DNS_SOCKET_ERROR },
	{ "ERROR_DNS_UPDATE_FAILED", ERROR_DNS_UPDATE_FAILED },
	{ NULL, ERROR_DNS_SUCCESS },
};

const char *dns_errstr(DNS_ERROR err)
{
	int i;

	for (i=0; dns_errs[i].dns_errstr != NULL; i++) {
		if (ERR_DNS_EQUAL(err, dns_errs[i].dns_errcode)) {
			return dns_errs[i].dns_errstr;
		}
	}

	return NULL;
}
