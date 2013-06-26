/*
  Linux DNS client library implementation

  Copyright (C) 2006 Krishna Ganugapati <krishnag@centeris.com>
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

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
#include <ctype.h>

static DNS_ERROR LabelList( TALLOC_CTX *mem_ctx,
			    const char *name,
			    struct dns_domain_label **presult )
{
	struct dns_domain_label *result;
	const char *dot;

	for (dot = name; *dot != '\0'; dot += 1) {
		char c = *dot;

		if (c == '.')
			break;

		if (c == '-') continue;
		if ((c >= 'a') && (c <= 'z')) continue;
		if ((c >= 'A') && (c <= 'Z')) continue;
		if ((c >= '0') && (c <= '9')) continue;

		return ERROR_DNS_INVALID_NAME;
	}

	if ((dot - name) > 63) {
		/*
		 * DNS labels can only be 63 chars long
		 */
		return ERROR_DNS_INVALID_NAME;
	}

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct dns_domain_label))) {
		return ERROR_DNS_NO_MEMORY;
	}

	if (*dot == '\0') {
		/*
		 * No dot around, so this is the last component
		 */

		if (!(result->label = talloc_strdup(result, name))) {
			TALLOC_FREE(result);
			return ERROR_DNS_NO_MEMORY;
		}
		result->len = strlen(result->label);
		*presult = result;
		return ERROR_DNS_SUCCESS;
	}

	if (dot[1] == '.') {
		/*
		 * Two dots in a row, reject
		 */

		TALLOC_FREE(result);
		return ERROR_DNS_INVALID_NAME;
	}

	if (dot[1] != '\0') {
		/*
		 * Something follows, get the rest
		 */

		DNS_ERROR err = LabelList(result, dot+1, &result->next);

		if (!ERR_DNS_IS_OK(err)) {
			TALLOC_FREE(result);
			return err;
		}
	}

	result->len = (dot - name);

	if (!(result->label = talloc_strndup(result, name, result->len))) {
		TALLOC_FREE(result);
		return ERROR_DNS_NO_MEMORY;
	}

	*presult = result;
	return ERROR_DNS_SUCCESS;
}

DNS_ERROR dns_domain_name_from_string( TALLOC_CTX *mem_ctx,
				       const char *pszDomainName,
				       struct dns_domain_name **presult )
{
	struct dns_domain_name *result;
	DNS_ERROR err;

	if (!(result = talloc(mem_ctx, struct dns_domain_name))) {
		return ERROR_DNS_NO_MEMORY;
	}

	err = LabelList( result, pszDomainName, &result->pLabelList );
	if (!ERR_DNS_IS_OK(err)) {
		TALLOC_FREE(result);
		return err;
	}

	*presult = result;
	return ERROR_DNS_SUCCESS;
}

/*********************************************************************
*********************************************************************/

char *dns_generate_keyname( TALLOC_CTX *mem_ctx )
{
	char *result = NULL;
#if defined(WITH_DNS_UPDATES)

	uuid_t uuid;

	/*
	 * uuid_unparse gives 36 bytes plus '\0'
	 */
	if (!(result = TALLOC_ARRAY(mem_ctx, char, 37))) {
		return NULL;
	}

	uuid_generate( uuid );
	uuid_unparse( uuid, result );

#endif

	return result;
}
