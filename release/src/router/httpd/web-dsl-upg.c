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
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 */

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <httpd.h>
#endif /* WEBS */
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <asm/types.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#include <iwlib.h>
#include <stapriv.h>
#include <ethutils.h>
#endif
#include <shared.h>
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#include "tc_ver.h"
#include "web-dsl-upg.h"

#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))



static int update_tc_fw = 0;

#define IH_NMLEN 32

typedef struct {
	unsigned int ih_magic;
	unsigned int ih_hcrc;
	unsigned int ih_time;
	unsigned int ih_size;
	unsigned int ih_load;
	unsigned int ih_ep;
	unsigned int ih_dcrc;
	unsigned char ih_os;
	unsigned char ih_arch;
	unsigned char ih_type;
	unsigned char ih_comp;
	unsigned char ih_name[IH_NMLEN];
} IMAGE_HEADER_TRX;

int separate_tc_fw_from_trx(char* trxpath)
{
	FILE* FpTrx;
	char TrxHdrBuf[512];
	char buf[4096];
	unsigned int* pTrxSize;
	unsigned int TrxSize;
	unsigned int filelen;
	unsigned int TcFwSize;	
	int RetVal = 0;

	FILE* fpSrc = NULL;
	FILE* fpDst = NULL;	

	FpTrx = fopen(trxpath,"rb");
	if (FpTrx == NULL) goto err;
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpTrx);	  
	fseek( FpTrx, 0, SEEK_END);
	filelen = ftell( FpTrx );
	fclose(FpTrx);

	pTrxSize = (unsigned int*)&TrxHdrBuf[12];
	TrxSize = SWAP_LONG(*pTrxSize) + 64;
	cprintf("trx size %x , file size %x\n",TrxSize,filelen);
	if (filelen > TrxSize)
	{
		TcFwSize = filelen - TrxSize;
		cprintf("tcfw size %x\n",TcFwSize);

		//
		// linux trx has several bytes garbage in the file tail
		// only save tc fw if the tc fw size is a valid number
		//
		if (TcFwSize > 0x100)
		{
			fpSrc = fopen(trxpath, "rb");
			if (fpSrc==NULL) goto err;
			fpDst = fopen("/tmp/tcfw.bin", "wb");
			if (fpDst==NULL) goto err;
			
			fseek(fpSrc,TrxSize,SEEK_SET);
			while (TcFwSize > 0)
			{
				if (TcFwSize > sizeof(buf))
				{
					fread(buf, 1, sizeof(buf), fpSrc);
					fwrite(buf, 1, sizeof(buf), fpDst);
					TcFwSize -= sizeof(buf);
				}
				else
				{
					fread(buf, 1, TcFwSize, fpSrc);
					fwrite(buf, 1, TcFwSize, fpDst);
					TcFwSize = 0;
				}
			}

			update_tc_fw = 1;
		}
		else {
			cprintf("no tc fw\n");
			goto err;
		}
	}
	else if (TrxSize == filelen)
	{
		// only trx, no tc fw
		cprintf("no tc fw\n");
		goto err;
	}

	RetVal = 1;
	
err:
	if(TrxSize)
		truncate(trxpath, TrxSize);

	if (fpSrc)
		fclose(fpSrc);

	if (fpDst)
		fclose(fpDst);

	return RetVal;		
}

static int crc_table_empty = 1;
static unsigned long crc_table[256];

static void make_crc_table()
{
  unsigned long c;
  int n, k;
  unsigned long poly;	    /* polynomial exclusive-or pattern */
  /* terms of polynomial defining this crc (except x^32): */
  static const unsigned char p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* make exclusive-or pattern from polynomial (0xedb88320L) */
  poly = 0L;
  for (n = 0; n < sizeof(p)/sizeof(unsigned char); n++)
    poly |= 1L << (31 - p[n]);

  for (n = 0; n < 256; n++)
  {
    c = (unsigned long)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? poly ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
  crc_table_empty = 0;
}


#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

unsigned long crc32_no_comp(unsigned long crc,const unsigned char* buf, int len)
{
    if (crc_table_empty)
      make_crc_table();

    while (len >= 8)
    {
      DO8(buf);
      len -= 8;
    }
    if (len) do {
      DO1(buf);
    } while (--len);

    return crc;
}
int check_tc_firmware_crc()
{
	FILE* fpSrc = NULL;
	FILE* fpDst = NULL;	
	unsigned long crc_value;
	unsigned long* crc_value_ptr;
	unsigned long calc_crc;
	long tcfilelen;
	long filelen, *filelenptr;
	int cmpHeaderErr = 0;
	int RetVal = 0;
	char buf[4096];
	int new_modem_trx_ver;
	int curr_modem_trx_ver;
	unsigned char bBuf[4096];
	unsigned char tag[] = {0x3C, 0x23, 0x24, 0x3E};	//<#$>
	int bBufsize = sizeof(bBuf);

	if (update_tc_fw == 0) return 0;
	

	fpSrc = fopen("/tmp/tcfw.bin", "rb");
	if (fpSrc==NULL)
	{
		cmpHeaderErr = 1;	
		goto exit;
	}
	fpDst = fopen("/tmp/ras.bin", "wb");
	if (fpDst==NULL)
	{
		cmpHeaderErr = 1;	
		goto exit;
	}

	fread(buf, 1, 0x100, fpSrc);

	cprintf("TC FW VER : %c%c%c , %s , %s\n",buf[0],buf[1],buf[2],TC_DSL_FW_VER,TC_DSL_FW_VER_FROM_MODEM);


	//read tcfw.bin and find the driver ras info, date ...
	fseek(fpSrc, 0xF000, SEEK_SET);
	fread(bBuf, 1, bBufsize, fpSrc);
	
	int read_idx = bBufsize;
	while (read_idx--) {
		if(bBuf[read_idx] == tag[3])
			if(bBuf[--read_idx] == tag[2])
				if(bBuf[--read_idx] == tag[1])
					if(bBuf[--read_idx] == tag[0])
						break;
					else
						continue;
				else
					continue;
			else
				continue;
		else
			continue;
	}
	cprintf("bin date: %s\n", (char*)bBuf+read_idx-14);
	cprintf("bin ras: %s\n", (char*)bBuf+read_idx+4);
	
	fseek(fpSrc, 0x0100, SEEK_SET);
	
	//check Annex mode
#ifdef RTCONFIG_DSL_ANNEX_B
	if(bBuf[read_idx+14] != 'B') {	//ASUS_Annex'B'_..
		if(bBuf[read_idx+9] != '1') {	//check for the old ras info ASUS_1020
			RetVal = -1;
			cprintf("check annex failed\n");
			goto exit;
		}
	}
#else
	if(bBuf[read_idx+14] != 'A') {	//ASUS_ANNEX'A'IJLM_..
		if(bBuf[read_idx+9] != '1') {	//check for the old ras info ASUS_1020
			RetVal = -1;
			cprintf("check annex failed\n");
			goto exit;
		}
	}
#endif
	//check driver release date
		// /tmp/adsl # cat tc_ver_info.txt
		// Bootbase:VTC_SPI_BR1.6 | 2010/7/30
		// RAS:ASUS_ANNEXAIJLM_20120423
		// System:3.6.18.0(BE.C3)3.16.18.0| 2011/10/31   20111031_v012  [Oct 31 2011 12:53:59]
	FILE *fpCur;
	char ver_info_buf[256];
	int line_idx = 3;
	int ver_idx = 0;
	fpCur = fopen("/tmp/adsl/tc_ver_info.txt", "r");
	if(fpCur != NULL) {
		while(line_idx--)
			fgets(ver_info_buf, 256, fpCur);
		fclose(fpCur);
		while(++ver_idx < 256) {
			if(ver_info_buf[ver_idx] == 0x7C) {
				*(ver_info_buf + ver_idx + 12) = '\0';
				cprintf("cur date: %s\n", ver_info_buf+ver_idx+2);
				if(!strncmp(ver_info_buf+ver_idx+2, (char*)bBuf+read_idx-14, 10))
					update_tc_fw = 0;
				else
					update_tc_fw = 1;
				break;
			}else
				continue;
		}
	}
	
	//check RAS
	fpCur = fopen("/tmp/adsl/tc_ras_ver.txt", "r");
	if(fpCur != NULL) {
		fgets(ver_info_buf, 256, fpCur);
		fclose(fpCur);
		cprintf("cur ras: %s\n", ver_info_buf);
#ifdef RTCONFIG_DSL_ANNEX_B
		if(!strncmp(ver_info_buf, (char*)bBuf+read_idx+4, 20))	//ASUS_AnnexB_20111031
#else
		if(!strncmp(ver_info_buf, (char*)bBuf+read_idx+4, 24))	//ASUS_ANNEXAIJLM_20120423
#endif		
			update_tc_fw = 0;
		else
			update_tc_fw = 1;
	}
	cprintf("update tc fw or not: %d (0 is not)\n", update_tc_fw);

	cprintf("tcfw magic : %x %x %x\n",buf[4],buf[5],buf[6]);

	if (strncmp(&buf[4], TC_DSL_MAGIC_NUMBER, 3) == 0)
	{
		// magic number OK
	}
	else
	{
		cmpHeaderErr = 1;
		goto exit;
	}
	

	filelenptr=(long*)(buf+22);
	filelen=*filelenptr;
	cprintf("tcfw Filelen: %ld\n", filelen);
	tcfilelen=filelen-0x100;

	crc_value_ptr=(unsigned long*)(buf+34);
	crc_value=*crc_value_ptr;
	crc_value=SWAP_LONG(crc_value);
	cprintf("tcfw crc: %lx\n", crc_value);

	calc_crc = 0xffffffff;

	while(tcfilelen>0)
	{
		if (tcfilelen > sizeof(buf))
		{
			fread(buf, 1, sizeof(buf), fpSrc);
			fwrite(buf, 1, sizeof(buf), fpDst);
			calc_crc = crc32_no_comp(calc_crc,buf,sizeof(buf));
			tcfilelen-=sizeof(buf);
		}
		else
		{
			fread(buf, 1, tcfilelen, fpSrc);
			fwrite(buf, 1, tcfilelen, fpDst);
			calc_crc = crc32_no_comp(calc_crc,buf,tcfilelen);
			tcfilelen=0;
		}
	}


	cprintf("tcfw calc crc is %lx\n", calc_crc);
	cprintf("tcfw done\n");


exit:

	if((calc_crc != crc_value) || cmpHeaderErr)
	{
		printf("header crc error\n");
		RetVal = -1;
	}


	if (fpSrc)
		fclose(fpSrc);

	if (fpDst)
		fclose(fpDst);

	return RetVal;		
}

#define ADSL_FW_IP_PREFIX "194.255.255."

void do_upgrade_adsldrv(void)
{
	char UpdateFwBuf[256];
	char PingBuf[256];
	char ipaddr[80];
	FILE* fp;
	int WaitCnt;
	int chk_image_err = 0;

	if (update_tc_fw == 0) return;	

	fp = fopen("/tmp/adsl/tc_ip_addr.txt","r");
	if (fp == NULL) chk_image_err = 1;
	fgets(ipaddr,sizeof(ipaddr),fp);
	fclose(fp);

	// if adsl fw IP address is different, user should update to a new router fw first
	if (strncmp(ipaddr,ADSL_FW_IP_PREFIX,sizeof(ADSL_FW_IP_PREFIX)-1)!=0) chk_image_err = 1;

	strcpy(UpdateFwBuf,"cd /tmp; tftp -p -l ras.bin ");
	strcat(UpdateFwBuf,ipaddr);

	cprintf("## upgrade tc fw\n");
	
	if (chk_image_err == 0)
	{
		cprintf("IP alias for ADSL firmware update\n");
		// this command will stop tp_init
		system("adslate waitadsl;adslate quitdrv");
		system("ifconfig eth2.1:0 194.255.255.1 netmask 255.255.255.0;ifconfig eth2.1:0 up");
		// wait if up
		strcpy(PingBuf,"ping ");
		strcat(PingBuf,ipaddr);
		strcat(PingBuf," -c 1");
		for (WaitCnt=0; WaitCnt<3; WaitCnt++)
		{
			system(PingBuf);
		}
		cprintf("Start to update\n");
		cprintf(UpdateFwBuf);
		system(UpdateFwBuf);
		printf("tftp done\n");
		//usleep(1000*1000*5);
		// wait a miniute and send set to default setting
		for (WaitCnt=0; WaitCnt<20; WaitCnt++)
		{
			system(PingBuf);
		}
		// ifconfig down also remove IP alias
		system("tp_init clear_modem_var");
		// wait tc flash write completed
		usleep(1000*1000*2);
		cprintf("\nupgrade done\n");
	}
}



// -1: different or no origial header, upgrade
// 0: same or no upgrade file, skip upgrade
int compare_linux_image(void)
{
	IMAGE_HEADER_TRX* TrxHdr;
	FILE* FpHdr;
	char TrxHdrBuf[512];
	unsigned int OrigTime;
	unsigned int NewTime;

	memset(TrxHdrBuf,0,sizeof(TrxHdrBuf));
	FpHdr = fopen("/tmp/trx_hdr.bin","rb");
	if (FpHdr == NULL) return -1;
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpHdr);	  
	fclose(FpHdr); 
	TrxHdr = (IMAGE_HEADER_TRX*)TrxHdrBuf;

	OrigTime = SWAP_LONG(TrxHdr->ih_time);

	memset(TrxHdrBuf,0,sizeof(TrxHdrBuf));
	FpHdr = fopen("/tmp/linux.trx","rb");
	if (FpHdr == NULL) return 0;
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpHdr);	  
	fclose(FpHdr); 
	TrxHdr = (IMAGE_HEADER_TRX*)TrxHdrBuf;

	NewTime = SWAP_LONG(TrxHdr->ih_time);

	fprintf(stderr, "Trx %x %x\n", NewTime, OrigTime);
	
	if (NewTime == OrigTime) 
	{
		fprintf(stderr, "trx same\n");
		return 0;
	}
	fprintf(stderr, "trx different\n");
	return -1;
}














