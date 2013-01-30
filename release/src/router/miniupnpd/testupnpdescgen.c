/* $Id: testupnpdescgen.c,v 1.18 2008/07/10 09:18:34 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2008 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "upnpdescgen.h"

char uuidvalue[] = "uuid:12345678-0000-0000-0000-00000000abcd";
char serialnumber[] = "12345678";
char modelnumber[] = "1";
char friendly_name[] = "ASUS Router";
char presentationurl[] = "http://192.168.0.1:8080/";

char * use_ext_ip_addr = NULL;
const char * ext_if_name = "eth0";

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
	printf("\n-------------\n");
	s = genWANIPCn(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
	s = genWANCfg(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
#ifdef ENABLE_L3F_SERVICE
	s = genL3F(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
#endif
#ifdef ENABLE_EVENTS
	s = getVarsWANIPCn(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
	s = getVarsWANCfg(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
#ifdef ENABLE_L3F_SERVICE
	s = getVarsL3F(&l);
	xml_pretty_print(s, l, stdout);
	free(s);
	printf("\n-------------\n");
#endif
#endif
/*
	stupid_test();
*/
	return 0;
}

