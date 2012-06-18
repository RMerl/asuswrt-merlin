/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006-2008, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "upnpdescgen.h"

char uuidvalue[] = "uuid:12345678-0000-0000-0000-00000000abcd";
char friendly_name[] = "localhost: system_type";
char serialnumber[] = "12345678";
char modelname[] = "MiniDLNA";
char modelnumber[] = "1";
char presentationurl[] = "http://192.168.0.1:8080/";
unsigned int updateID = 0;
#if PNPX
char pnpx_hwid[] = "VEN_01F2&amp;DEV_0101&amp;REV_01 VEN_0033&amp;DEV_0001&amp;REV_01";
#endif

int getifaddr(const char * ifname, char * buf, int len)
{
	strncpy(buf, "1.2.3.4", len);
	return 0;
}

int upnp_get_portmapping_number_of_entries()
{
	return 42;
}

/* To be improved */
int
xml_pretty_print(const char * s, int len, FILE * f)
{
	int n = 0, i;
	int elt_close = 0;
	int c, indent = 0;
	while(len > 0)
	{
		c = *(s++);	len--;
		switch(c)
		{
		case '<':
			if(len>0 && *s == '/')
				elt_close++;
			else if(len>0 && *s == '?')
				elt_close = 1;
			else
				elt_close = 0;
			if(elt_close!=1)
			{
				if(elt_close > 1)
					indent--;
				fputc('\n', f); n++;
				for(i=indent; i>0; i--)
					fputc(' ', f);
				n += indent;
			}
			fputc(c, f); n++;
			break;
		case '>':
			fputc(c, f); n++;
			if(elt_close==1)
			{
				/*fputc('\n', f); n++; */
				//elt_close = 0;
				if(indent > 0)
					indent--;
			}
			else if(elt_close == 0)
				indent++;
			break;
		default:
			fputc(c, f); n++;
		}
	}
	return n;
}

/* stupid test */
const char * str1 = "Prefix123String";
const char * str2 = "123String";

void stupid_test()
{
	printf("str1:'%s' str2:'%s'\n", str1, str2);
	printf("str1:%p str2:%p str2-str1:%ld\n", str1, str2, (long)(str2-str1));
}

/* main */

int
main(int argc, char * * argv)
{
	char * rootDesc;
	int rootDescLen;
	char * s;
	int l;
	rootDesc = genRootDesc(&rootDescLen);
	xml_pretty_print(rootDesc, rootDescLen, stdout);
	free(rootDesc);
	printf("\n----------------\n");
	printf("ContentDirectory\n");
	printf("----------------\n");
	s = genContentDirectory(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n----------------\n");
	printf("ConnectionManager\n");
	printf("----------------\n");
	s = genConnectionManager(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n----------------\n");
	printf("X_MS_MRR\n");
	printf("----------------\n");
	s = genX_MS_MediaReceiverRegistrar(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
/*
	stupid_test();
*/
	return 0;
}

