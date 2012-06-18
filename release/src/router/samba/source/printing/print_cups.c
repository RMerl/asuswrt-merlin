/*
 * Support code for the Common UNIX Printing System ("CUPS")
 *
 * Copyright 1999 by Easy Software Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "smb.h"

#ifdef HAVE_LIBCUPS
#include <cups/cups.h>
#include <cups/language.h>

extern int DEBUGLEVEL;


/*
 * 'cups_printer_fn()' - Call a function for every printer known to the
 *                       system.
 */

void cups_printer_fn(void (*fn)(char *, char *))
{
	http_t		*http;		/* HTTP connection to server */
	ipp_t		*request,	/* IPP Request */
			*response;	/* IPP Response */
	ipp_attribute_t	*attr;		/* Current attribute */
	cups_lang_t	*language;	/* Default language */
	char		*name,		/* printer-name attribute */
			*make_model;	/* make_model attribute */


       /*
	* Try to connect to the server...
	*/

	if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
		return;

       /*
	* Build a CUPS_GET_PRINTERS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*/

	request = ippNew();

	request->request.op.operation_id = CUPS_GET_PRINTERS;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, cupsLangEncoding(language));

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL)
	{
		httpClose(http);
		return;
	}

	for (attr = response->attrs; attr != NULL;)
	{
	       /*
		* Skip leading attributes until we hit a printer...
		*/

		while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
			attr = attr->next;

		if (attr == NULL)
        		break;

	       /*
		* Pull the needed attributes from this printer...
		*/

		name       = NULL;
		make_model = NULL;

		while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
		{
        		if (strcmp(attr->name, "printer-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME)
				name = attr->values[0].string.text;

        		if (strcmp(attr->name, "printer-make-and-model") == 0 &&
			    attr->value_tag == IPP_TAG_TEXT)
				make_model = attr->values[0].string.text;

        		attr = attr->next;
		}

	       /*
		* See if we have everything needed...
		*/

		if (name == NULL)
			break;

		(*fn)(name, make_model);
	}

	ippDelete(response);
	httpClose(http);
}


/*
 * provide the equivalent of pcap_printername_ok() for SVID/XPG4 conforming
 * systems.
 */
int cups_printername_ok(char *name)
{
	http_t		*http;		/* HTTP connection to server */
	ipp_t		*request,	/* IPP Request */
			*response;	/* IPP Response */
	ipp_attribute_t	*attr;		/* Current attribute */
	cups_lang_t	*language;	/* Default language */
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */

       /*
	* Try to connect to the server...
	*/

	if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
		return (0);

       /*
	* Build a IPP_GET_PRINTER_ATTRS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    printer-uri
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, cupsLangEncoding(language));

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

	snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", name);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL)
	{
		httpClose(http);
		return (0);
	}

	httpClose(http);

	if (response->request.status.status_code >= IPP_OK_CONFLICT)
	{
		ippDelete(response);
		return (0);
	}
	else
	{
		ippDelete(response);
		return (1);
	}
}

#else
 /* this keeps fussy compilers happy */
 void print_cups_dummy(void) {}
#endif /* HAVE_LIBCUPS */
