/* $Id: xml.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"


#if INCLUDE_XML_TEST

#include <pjlib-util/xml.h>
#include <pjlib.h>

#define THIS_FILE   "xml_test"

static const char *xml_doc[] =
{
"   <?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"   <p:pidf-full xmlns=\"urn:ietf:params:xml:ns:pidf\"\n"
"          xmlns:p=\"urn:ietf:params:xml:ns:pidf-diff\"\n"
"          xmlns:r=\"urn:ietf:params:xml:ns:pidf:rpid\"\n"
"          xmlns:c=\"urn:ietf:params:xml:ns:pidf:caps\"\n"
"          entity=\"pres:someone@example.com\"\n"
"          version=\"567\">\n"
"\n"
"    <tuple id=\"sg89ae\">\n"
"     <status>\n"
"      <basic>open</basic>\n"
"      <r:relationship>assistant</r:relationship>\n"
"     </status>\n"
"     <c:servcaps>\n"
"      <c:audio>true</c:audio>\n"
"      <c:video>false</c:video>\n"
"      <c:message>true</c:message>\n"
"     </c:servcaps>\n"
"     <contact priority=\"0.8\">tel:09012345678</contact>\n"
"    </tuple>\n"
"\n"
"    <tuple id=\"cg231jcr\">\n"
"     <status>\n"
"      <basic>open</basic>\n"
"     </status>\n"
"     <contact priority=\"1.0\">im:pep@example.com</contact>\n"
"    </tuple>\n"
"\n"
"    <tuple id=\"r1230d\">\n"
"     <status>\n"
"      <basic>closed</basic>\n"
"      <r:activity>meeting</r:activity>\n"
"     </status>\n"
"     <r:homepage>http://example.com/~pep/</r:homepage>\n"
"     <r:icon>http://example.com/~pep/icon.gif</r:icon>\n"
"     <r:card>http://example.com/~pep/card.vcd</r:card>\n"
"     <contact priority=\"0.9\">sip:pep@example.com</contact>\n"
"    </tuple>\n"
"\n"
"    <note xml:lang=\"en\">Full state presence document</note>\n"
"\n"
"    <r:person>\n"
"     <r:status>\n"
"      <r:activities>\n"
"       <r:on-the-phone/>\n"
"       <r:busy/>\n"
"      </r:activities>\n"
"     </r:status>\n"
"    </r:person>\n"
"\n"
"    <r:device id=\"urn:esn:600b40c7\">\n"
"     <r:status>\n"
"      <c:devcaps>\n"
"       <c:mobility>\n"
"        <c:supported>\n"
"         <c:mobile/>\n"
"        </c:supported>\n"
"       </c:mobility>\n"
"      </c:devcaps>\n"
"     </r:status>\n"
"    </r:device>\n"
"\n"
"   </p:pidf-full>\n"
}
;

static int xml_parse_print_test(const char *doc)
{
    pj_str_t msg;
    pj_pool_t *pool;
    pj_xml_node *root;
    char *output;
    int output_len;

    pool = pj_pool_create(mem, "xml", 4096, 1024, NULL);
    pj_strdup2(pool, &msg, doc);
    root = pj_xml_parse(0, pool, msg.ptr, msg.slen);
    if (!root) {
	PJ_LOG(1, (THIS_FILE, "  Error: unable to parse XML"));
	return -10;
    }

    output = (char*)pj_pool_zalloc(pool, msg.slen + 512);
    output_len = pj_xml_print(root, output, msg.slen+512, PJ_TRUE);
    if (output_len < 1) {
	PJ_LOG(1, (THIS_FILE, "  Error: buffer too small to print XML file"));
	return -20;
    }
    output[output_len] = '\0';


    pj_pool_release(pool);
    return 0;
}

int xml_test()
{
    unsigned i;
    for (i=0; i<sizeof(xml_doc)/sizeof(xml_doc[0]); ++i) {
	int status;
	if ((status=xml_parse_print_test(xml_doc[i])) != 0)
	    return status;
    }
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_xml_test;
#endif	/* INCLUDE_XML_TEST */


