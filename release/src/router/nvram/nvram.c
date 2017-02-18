/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Frontend command-line utility for Linux NVRAM layer
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: nvram.c,v 1.1 2007/06/08 10:22:42 arthur Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>	// for sleep()
#include <typedefs.h>

#include <rtconfig.h>
#include <bcmnvram.h>

#define PROTECT_CHAR	'x'

/*******************************************************************
* NAME: _secure_romfile
* AUTHOR: Andy Chiu
* CREATE DATE: 2015/06/08
* DESCRIPTION: replace account /password by PROTECT_CHAR with the same string length
* INPUT:  path: the rom file path
* OUTPUT:
* RETURN:  0: success, -1:failed
* NOTE: Andy Chiu, 2015/12/18. Add new tokens.
*           Andy Chiu, 2015/12/24. Add new tokens.
*	     Andy Chiu, 2016/02/18. Add new token, wtf_username.
*******************************************************************/
static int _secure_conf(char* buf)
{
	char name[128], *item;
	int i, flag;
	const char *keyword_token[] = {"http_username", "passwd", "password", ""};	//Andy Chiu, 2015/12/18
	
	const char *token1[] = {"wan_pppoe_passwd", "modem_pass", "modem_pincode",
		"http_passwd", "wan0_pppoe_passwd", "dslx_pppoe_passwd", "ddns_passwd_x",
		"wl_wpa_psk",	"wlc_wpa_psk",  "wlc_wep_key",
		"wl0_wpa_psk", "wl0.1_wpa_psk", "wl0.2_wpa_psk", "wl0.3_wpa_psk",
		"wl1_wpa_psk", "wl1.1_wpa_psk", "wl1.2_wpa_psk", "wl1.3_wpa_psk",
		"wl0.1_key1", "wl0.1_key2", "wl0.1_key3", "wl0.1_key4",
		"wl0.2_key1", "wl0.2_key2", "wl0.2_key3", "wl0.2_key4",
		"wl0.3_key1", "wl0.3_key2", "wl0.3_key3", "wl0.3_key4",
		"wl0_key1", "wl0_key2", "wl0_key3", "wl0_key4",
		"wl1.1_key1", "wl1.1_key2", "wl1.1_key3", "wl1.1_key4",
		"wl1.2_key1", "wl1.2_key2", "wl1.2_key3", "wl1.2_key4",
		"wl1.3_key1", "wl1.3_key2", "wl1.3_key3", "wl1.3_key4",
		"wl_key1", "wl_key2", "wl_key3", "wl_key4",
		"wl1_key1", "wl1_key2", "wl1_key3", "wl1_key4",
		"wl0_phrase_x", "wl0.1_phrase_x", "wl0.2_phrase_x", "wl0.3_phrase_x",
		"wl1_phrase_x", "wl1.1_phrase_x", "wl1.2_phrase_x", "wl1.3_phrase_x",
		"wl_phrase_x", "vpnc_openvpn_pwd", "PM_SMTP_AUTH_USER", "PM_MY_EMAIL",
		"PM_SMTP_AUTH_PASS", "wtf_username", "ddns_hostname_x", "ddns_username_x", ""};

	const char *token2[] = {"acc_list", "pptpd_clientlist", "vpn_serverx_clientlist", ""};
	//admin>99999<Family>99999999<aaaaa>9999999<bbbbb>999999
	//pptpd_clientlist=<aaaaaaaaa>999999999<bbbbbbbbbb>9999999999

	const char vpnc_token[] = "vpnc_clientlist";
//	qwrety>PPTP>asdf>aaaaaaaaaa>999999999999<qe3rtuio>L2TP>waesrdio>bbbbbbbbb>9999999999999<wqertuio>OpenVPN>1>ccccccccc>999999999999

	const char cloud_token[] = "cloud_sync";
//	0>aaaaaaaaaaa>9999999999>none>0>/tmp/mnt/SANDISK_32G/aaa>1

	const char pppoe_username_token[] = "pppoe_username";
	//replace xxx@aaa.bbb

	if(!buf)
		return -1;

	//fprintf(stderr, "[%s, %d]\n", __FUNCTION__, __LINE__);
	for (item = buf; *item; item += strlen(item) + 1)
	{
		//get name of item
		char *ptr = strchr(item, '=');
		if(!ptr)	//invalid item
			continue;
		memset(name, 0, sizeof(name));
		strncpy(name, item, ptr - item);
		flag = 0;
		//skip '='
		++ptr;
		//fprintf(stderr, "[%s, %d]item(%s), name(%s), val(%s)\n", __FUNCTION__, __LINE__, item, name, ptr);

		//check the password keyword token
		for(i = 0; strlen(keyword_token[i]) > 0; ++i)
		{
			if(strstr(name, keyword_token[i]))
			{
				//replace the value
				memset(ptr, PROTECT_CHAR, strlen(ptr));
				//fprintf(stderr, "[%s, %d]<%s>replace(%s)\n", __FUNCTION__, __LINE__, name, ptr);
				flag = 1;
				break;
			}
		}
		if(flag)
			continue;
		
		//check the first token group
		for(i = 0; strlen(token1[i]) > 0; ++i)
		{
			if(!strcmp(name, token1[i]))
			{
				//replace the value
				memset(ptr, PROTECT_CHAR, strlen(ptr));
				//fprintf(stderr, "[%s, %d]<%s>replace(%s)\n", __FUNCTION__, __LINE__, name, ptr);
				flag = 1;
				break;
			}
		}
		if(flag)
			continue;

		//check the 2nd token group
		//admin>99999<Family>99999999<aaaaa>9999999<bbbbb>999999
		for(i = 0; strlen(token2[i]) > 0; ++i)
		{
			if(!strcmp(name, token2[i]))
			{
				//replace the value
				char *b = ptr, *e;
				do
				{
					b = strchr(b, '>');
					if(b)
						++b;
					else
						break;
					e = strchr(b, '<');

					if(e)
					{
						memset(b, PROTECT_CHAR, e-b);
						b = e + 1;
					}
					else
						memset(b, PROTECT_CHAR, strlen(b));

				}while(b);
				//fprintf(stderr, "[%s, %d]<%s>replace(%s)\n", __FUNCTION__, __LINE__, name, ptr);
				flag = 1;
				break;
			}
		}
		if(flag)
			continue;

		//check vpnc token
		//qwrety>PPTP>asdf>aaaaaaaaaa>999999999999<qe3rtuio>L2TP>waesrdio>bbbbbbbbb>9999999999999<wqertuio>OpenVPN>1>ccccccccc>999999999999
		if(!strcmp(name, vpnc_token))
		{
			char *b = ptr, *e;
			if(*b == '<')
				++b;
			do{
				int j;
				for(j = 0; j < 4; ++j)
				{
					b = strchr(b, '>');
					if(b)
						++b;
					else
						break;
				}

				if(b)
				{
					e = strchr(b, '<');
					if(e)
					{
						memset(b, PROTECT_CHAR, e - b);
						b = e + 1;
					}
					else
					{
						memset(b, PROTECT_CHAR, strlen(b));
						b = NULL;
					}
				}
			}while(b);
			//fprintf(stderr, "[%s, %d]<%s>replace(%s)\n", __FUNCTION__, __LINE__, name, ptr);
		}

		//check cloud sync token
		//0>aaaaaaaaaaa>9999999999>none>0>/tmp/mnt/SANDISK_32G/aaa>1
		if(!strcmp(name, cloud_token))
		{
			char *b = ptr, *e;
			if(*b == '<')
				++b;
			do{
				int j;
				for(j = 0; j < 2; ++j)
				{
					b = strchr(b, '>');
					if(b)
						++b;
					else
						break;
				}

				if(b)
				{
					e = strchr(b, '>');
					if(e)
					{
						memset(b, PROTECT_CHAR, e - b);
						b = strchr(e, '<');
					}
					else	//invalid
					{
						b = NULL;
					}
				}
			}while(b);
			//fprintf(stderr, "[%s, %d]<%s>replace(%s)\n", __FUNCTION__, __LINE__, name, ptr);
		}

		if(strstr(name, pppoe_username_token))
		{
			int len = strlen(ptr);
			for(i = 0; i < len; ++i)
			{
				if(ptr[i] == '@')
					break;
				ptr[i] = PROTECT_CHAR;
			}
		}
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: nvram [get name] [set name=value] [unset name] [show] [save file] [restore file] [fb_save file]\n");
	exit(0);
}

#define PROFILE_HEADER		"HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW      "N55U"
#else
#ifdef RTCONFIG_QCA
#define PROFILE_HEADER_NEW      "AC55U"
#else
#define PROFILE_HEADER_NEW      "HDR2"
#endif
#endif


// save nvram to file
int nvram_save(char *file, char *buf)
{
	FILE *fp;
	char *name;
	unsigned long count, filelen, i;

   	if ((fp=fopen(file, "w"))==NULL) return -1;

	count = 0;
	for (name=buf;*name;name+=strlen(name)+1)
	{	
		puts(name);
		count = count+strlen(name)+1;
	}
   	
   	filelen = count + (1024 - count%1024);	
   	fwrite(PROFILE_HEADER, 1, 4, fp);
   	fwrite(&filelen, 1, 4, fp);
   	fwrite(buf, 1, count, fp);
   	for (i=count;i<filelen;i++) fwrite(name, 1, 1, fp);   	
   	fclose(fp);
	return 0;
}

unsigned char get_rand()
{
	unsigned char buf[1];
	FILE *fp;
//	size_t rc;

	fp = fopen("/dev/urandom", "r");
	if (fp == NULL) {
#ifdef ASUS_DEBUG
		fprintf(stderr, "Could not open /dev/urandom.\n");
#endif
		return 0;
	}
	fread(buf, 1, 1, fp);
	fclose(fp);

	return buf[0];
}

int nvram_save_new(char *file, char *buf)
{
	FILE *fp;
	char *name;
	unsigned long count, filelen, i;
	unsigned char rand = 0, temp;

	if ((fp = fopen(file, "w")) == NULL) return -1;

	count = 0;
	for (name = buf; *name; name += strlen(name) + 1)
	{
#ifdef ASUS_DEBUG
		puts(name);
#endif
		count = count + strlen(name) + 1;
	}

	filelen = count + (1024 - count % 1024);
	rand = get_rand() % 30;
#ifdef ASUS_DEBUG
	fprintf(stderr, "random number: %x\n", rand);
#endif
	fwrite(PROFILE_HEADER_NEW, 1, 4, fp);
	fwrite(&filelen, 1, 3, fp);
	fwrite(&rand, 1, 1, fp);
#ifdef ASUS_DEBUG
	for (i = 0; i < 4; i++)
	{
		fprintf(stderr, "%2x ", PROFILE_HEADER_NEW[i]);
	}
	for (i = 0; i < 3; i++)
	{
		fprintf(stderr, "%2x ", ((char *)&filelen)[i]);
	}
	fprintf(stderr, "%2x ", ((char *)&rand)[0]);
#endif
	for (i = 0; i < count; i++)
	{
		if (buf[i] == 0x0)
			buf[i] = 0xfd + get_rand() % 3;
		else
			buf[i] = 0xff - buf[i] + rand;
	}
	fwrite(buf, 1, count, fp);
#ifdef ASUS_DEBUG
	for (i = 0; i < count; i++)
	{
		if (i % 16 == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%2x ", (unsigned char) buf[i]);
	}
#endif
	for (i = count; i < filelen; i++)
	{
		temp = 0xfd + get_rand() % 3;
		fwrite(&temp, 1, 1, fp);
#ifdef ASUS_DEBUG
		if (i % 16 == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%2x ", (unsigned char) temp);
#endif
	}
	fclose(fp);
	return 0;
}

int issyspara(char *p)
{
	struct nvram_tuple *t/*eric--, *u*/;
	extern struct nvram_tuple router_defaults[];
	
	// skip checking for wl[]_, wan[], lan[]_
	if(strstr(p, "wl") || strstr(p, "wan") || strstr(p, "lan")) return 1;

	for (t = router_defaults; t->name; t++)
	{
		if (strstr(p, t->name))
			break;
	
	}

	if (t->name) return 1;
	else return 0;
}

// restore nvram from file
int nvram_restore(char *file, char *buf)
{
   	FILE *fp;
   	char header[8], *p, *v;
  	unsigned long count, *filelen;

   	if ((fp=fopen(file, "r+"))==NULL) return -1;
    	   
   	count = fread(header, 1, 8, fp);
   	if (count>=8 && strncmp(header, PROFILE_HEADER, 4)==0)
   	{  
	    filelen = (unsigned long *)(header + 4);
   	    count = fread(buf, 1, *filelen, fp);
   	}   
   	fclose(fp);

   	p = buf;       		   
   
   	while (*p)
   	{       
		//printf("nv:%s\n", p);     	     			     	
       		v = strchr(p, '=');

		if (v!=NULL)
		{	
			*v++ = '\0' /*eric--NULL*/;

			if (issyspara(p))
			{
				nvram_set(p, v);
			}

       			p = v + strlen(v) + 1;			
		}
		else 
		{
			nvram_unset(p);
			p = p + 1;
		}
   	}

	nvram_set("x_Setting", "1");
	return 0;
}

int nvram_restore_new(char *file, char *buf)
{
	FILE *fp;
	char header[8], *p, *v;
	unsigned long count, filelen, *filelenptr, i;
	unsigned char rand, *randptr;

	if ((fp = fopen(file, "r+")) == NULL) return -1;

	count = fread(header, 1, 8, fp);
	if (count>=8 && strncmp(header, PROFILE_HEADER, 4) == 0)
	{
		filelenptr = (unsigned long *)(header + 4);
#ifdef ASUS_DEBUG
		fprintf(stderr, "restoring original text cfg of length %x\n", *filelenptr);
#endif
		fread(buf, 1, *filelenptr, fp);
	}
	else if (count>=8 && strncmp(header, PROFILE_HEADER_NEW, 4) == 0)
	{
		filelenptr = (unsigned long *)(header + 4);
		filelen = *filelenptr & 0xffffff;
#ifdef ASUS_DEBUG
		fprintf(stderr, "restoring non-text cfg of length %x\n", filelen);
#endif
		randptr = (unsigned char *)(header + 7);
		rand = *randptr;
#ifdef ASUS_DEBUG
		fprintf(stderr, "non-text cfg random number %x\n", rand);
#endif
		count = fread(buf, 1, filelen, fp);
#ifdef ASUS_DEBUG
		fprintf(stderr, "non-text cfg count %x\n", count);
#endif
		for (i = 0; i < count; i++)
		{
			if ((unsigned char) buf[i] > ( 0xfd - 0x1)){
				/* e.g.: to skip the case: 0x61 0x62 0x63 0x00 0x00 0x61 0x62 0x63 */
				if(i > 0 && buf[i-1] != 0x0)
					buf[i] = 0x0;
			}
			else
				buf[i] = 0xff + rand - buf[i];
		}
#ifdef ASUS_DEBUG
		for (i = 0; i < count; i++)
		{
			if (i % 16 == 0) fprintf(stderr, "\n");
			fprintf(stderr, "%2x ", (unsigned char) buf[i]);
		}

		for (i = 0; i < count; i++)
		{
			if (i % 16 == 0) fprintf(stderr, "\n");
			fprintf(stderr, "%c", buf[i]);
		}
#endif
	}
	else
	{
		fclose(fp);
		return 0;
	}
	fclose(fp);

	p = buf;

	while (*p)
	{
#if 1
		/* e.g.: to skip the case: 00 2e 30 2e 32 38 00 ff 77 61 6e */
		if (!isprint(*p)) {
			p = p + 1;
			continue;
		}
#endif
		v = strchr(p, '=');

		if (v != NULL)
		{
			*v++ = '\0';

			if (issyspara(p))
				nvram_set(p, v);

			p = v + strlen(v) + 1;
		}
		else
		{
			nvram_unset(p);
			p = p + 1;
		}
	}
	return 0;
}

/* NVRAM utility */
int
main(int argc, char **argv)
{
	char *name, *value, *buf;
	int size;
	char *tmpbuf;	//Andy Chiu, 2015/06/09
#ifdef RTCONFIG_CFE_NVRAM_CHK
	int ret = 0;
	FILE *fp;
#endif

	/* Skip program name */
	--argc;
	++argv;

	if (!*argv) 
		usage();
	
	buf = malloc (MAX_NVRAM_SPACE);
	if (buf == NULL)	{
		perror ("Out of memory!\n");
		return -1;
	}
	
	/* Process the remaining arguments. */
	for (; *argv; argv++) {
		if (!strncmp(*argv, "get", 3)) {
			if (*++argv) {
				if ((value = nvram_get(*argv)))
					puts(value);
			}
		}
		else if (!strncmp(*argv, "set", 3)) {
			if (*++argv) {
				strncpy(value = buf, *argv, MAX_NVRAM_SPACE);
				name = strsep(&value, "=");
				
#ifdef RTCONFIG_CFE_NVRAM_CHK
				ret = nvram_set(name, value);
				if(ret == 2) {
					fp = fopen("/var/log/cfecommit_ret", "w");
					if(fp!=NULL) {
		                                fprintf(fp,"Illegal nvram\n");
						fclose(fp);
					}
					puts("Illegal nvram format!");
				}
#else
				nvram_set(name, value);
#endif
			}
		}
		else if (!strncmp(*argv, "unset", 5)) {
			if (*++argv)
				nvram_unset(*argv);
		}
		else if (!strncmp(*argv, "commit", 5)) {
			nvram_commit();
		}
		else if (!strncmp(*argv, "save", 4)) 
		{
			if (*++argv) 
			{
				nvram_getall(buf, MAX_NVRAM_SPACE);
				nvram_save_new(*argv, buf);
			}
			
		}
		//Andy Chiu, 2015/06/09
		else if (!strncmp(*argv, "fb_save", 7))
		{
			if (*++argv)
			{
				tmpbuf = malloc(MAX_NVRAM_SPACE);
				if(!tmpbuf)
				{
					fprintf(stderr, "Can NOT alloc memory!!!");
					return 0;
				}
				nvram_getall(buf, MAX_NVRAM_SPACE);
				memcpy(tmpbuf, buf, MAX_NVRAM_SPACE);
				_secure_conf(tmpbuf);
#if 0
				FILE *fp = fopen("/tmp/var/fb_conf.test", "w");
				if(fp)
				{
					fwrite(tmpbuf, 1, MAX_NVRAM_SPACE, fp);
					fclose(fp);
				}
#endif
				nvram_save_new(*argv, tmpbuf);
				free(tmpbuf);
			}
		}
		else if (!strncmp(*argv, "restore", 7)) 
		{
			if (*++argv) 
			{
				nvram_restore_new(*argv, buf);
			}
			
		}
		else if (!strncmp(*argv, "show", 4) || !strncmp(*argv, "getall", 6)) {
			nvram_getall(buf, MAX_NVRAM_SPACE);
			for (name = buf; *name; name += strlen(name) + 1)
				puts(name);
			size = sizeof(struct nvram_header) + (int) name - (int) buf;
			fprintf(stderr, "size: %d bytes (%d left)\n", size, MAX_NVRAM_SPACE - size);
		}
		if (!*argv)
			break;
	}

	if (buf != NULL)
		free (buf);
	return 0;
}	
