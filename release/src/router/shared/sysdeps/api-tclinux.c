#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "shutils.h"
#include "shared.h"

static int crc_table_empty = 1;
static unsigned long crc_table[256];

#define SWAP_LONG(x) \
	((u_int32_t)( \
		(((u_int32_t)(x) & (u_int32_t)0x000000ffUL) << 24) | \
		(((u_int32_t)(x) & (u_int32_t)0x0000ff00UL) <<  8) | \
		(((u_int32_t)(x) & (u_int32_t)0x00ff0000UL) >>  8) | \
		(((u_int32_t)(x) & (u_int32_t)0xff000000UL) >> 24) ))

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

#define TC_HDR_LEN	0x100

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

int separate_tc_fw_from_trx(char* trxpath)
{
	FILE* FpTrx;
	char TrxHdrBuf[512];
	char buf[4096];
	unsigned int *pTrxSize;
	unsigned int TrxSize = 0;
	unsigned int filelen;
	unsigned int TcFwSize;
	int RetVal = 0;

	FILE* fpSrc = NULL;
	FILE* fpDst = NULL;

	FpTrx = fopen(trxpath, "rb");
	if (FpTrx == NULL) goto err;
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpTrx);
	fseek( FpTrx, 0, SEEK_END);
	filelen = ftell( FpTrx );
	fclose(FpTrx);

	pTrxSize = (unsigned int*)(TrxHdrBuf+4);	//bcm
	TrxSize = *pTrxSize;
	cprintf("trx size %u, file size %u\n", TrxSize, filelen);
	if (filelen > TrxSize)
	{
		TcFwSize = filelen - TrxSize;
		cprintf("tcfw size %u\n",TcFwSize);

		if (TcFwSize > TC_HDR_LEN)
		{
			fpSrc = fopen(trxpath, "rb");
			if (fpSrc==NULL) goto err;
			fpDst = fopen("/tmp/tcfw.bin", "wb");
			if (fpDst==NULL) goto err;

			fseek(fpSrc, TrxSize, SEEK_SET);

			fread(buf, 1, TC_HDR_LEN, fpSrc);
			fwrite(buf, 1, TC_HDR_LEN, fpDst);
			TcFwSize -= TC_HDR_LEN;

			cprintf("tcfw magic : %c%c%c%c\n",buf[0],buf[1],buf[2],buf[3]);
			if (strncmp(&buf[0], "2RDH", 4))
				goto err;

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
		}
		else {
			cprintf("not tc fw\n");
			goto err;
		}
	}
	else if (TrxSize == filelen)
	{
		// only trx, no tc fw
		printf("no tc fw\n");
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

int check_tc_firmware_crc()
{
	FILE* fpSrc = NULL;
	FILE* fpDst = NULL;
	unsigned long* crc_value_ptr = NULL;
	unsigned long ulfw_crc = 0, calc_crc = 0;
	unsigned long trxlen, *trxlenptr;
	int skip_flag = 0;
	int RetVal = 0;
	char buf[4096] = {0};

	fpSrc = fopen("/tmp/tcfw.bin", "rb");
	if (fpSrc==NULL)
	{
		skip_flag = 1;
		goto exit;
	}
	fpDst = fopen("/tmp/tclinux.bin", "wb");
	if (fpDst==NULL)
	{
		skip_flag = 1;
		goto exit;
	}

	fread(buf, 1, 0x100, fpSrc);

	trxlenptr=(unsigned long*)(buf+8);
	trxlen=*trxlenptr;
	trxlen=SWAP_LONG(trxlen);
	cprintf("tcfw len: %lu\n", trxlen);

	fwrite(buf, 1, 0x100, fpDst);
	trxlen -= 0x100;

	crc_value_ptr=(unsigned long*)(buf+12);
	ulfw_crc=*crc_value_ptr;
	ulfw_crc=SWAP_LONG(ulfw_crc);
	cprintf("ulfw crc: %lx\n", ulfw_crc);

	calc_crc = 0xffffffff;

	while(trxlen>0)
	{
		if (trxlen > sizeof(buf))
		{
			fread(buf, 1, sizeof(buf), fpSrc);
			fwrite(buf, 1, sizeof(buf), fpDst);
			calc_crc = crc32_no_comp(calc_crc, (unsigned char*)buf, sizeof(buf));
			trxlen-=sizeof(buf);
		}
		else
		{
			fread(buf, 1, trxlen, fpSrc);
			fwrite(buf, 1, trxlen, fpDst);
			calc_crc = crc32_no_comp(calc_crc, (unsigned char*)buf, trxlen);
			trxlen=0;
		}
	}

	cprintf("calc crc: %lx\n", calc_crc);

	if (ulfw_crc != calc_crc)
			skip_flag = 1;
#ifdef RTCONFIG_BRCM_NAND_JFFS2
	else {
		cprintf("backup tclinux\n");
		eval("cp", "-f", "/tmp/tclinux.bin", "/jffs/tclinux.bin");
	}
#endif

	unlink("/tmp/tcfw.bin");


	cprintf("check tcfw done\n");

exit:

	if(skip_flag)
		RetVal = -1;

	if (fpSrc)
		fclose(fpSrc);

	if (fpDst)
		fclose(fpDst);

	return RetVal;
}

void do_upgrade_adsldrv(void)
{
	char UpdateFwBuf[256];
	char ipaddr[80];
	FILE* fp;
	int ret;
	int skip_flag = 0, force_flag = 0;
	unsigned long old_crc = 0, new_crc = 0;

	cprintf("%s\n", __FUNCTION__);

	if(!check_if_file_exist("/tmp/tclinux.bin"))
		return;

	fp = fopen("/tmp/adsl/tc_ip_addr.txt","r");
	if (fp) {
		fgets(ipaddr,sizeof(ipaddr),fp);
		fclose(fp);
	}
	else {
		snprintf(ipaddr, sizeof(ipaddr), "169.254.0.1");
		force_flag = 1;
	}

	strcpy(UpdateFwBuf,"cd /tmp; tftp -p -l tclinux.bin ");
	strcat(UpdateFwBuf,ipaddr);

	fp = fopen("/tmp/adsl/tc_hdr.bin", "rb");
	if (fp)
	{
		fseek(fp, 12L, SEEK_SET);
		fread(&old_crc, 1, sizeof(unsigned long), fp);
		old_crc = SWAP_LONG(old_crc);
		cprintf("old crc: %lx\n", old_crc);
		fclose(fp);
	}
	fp = fopen("/tmp/tclinux.bin", "rb");
	if (fp)
	{
		fseek(fp, 12L, SEEK_SET);
		fread(&new_crc, 1, sizeof(unsigned long), fp);
		new_crc = SWAP_LONG(new_crc);
		cprintf("new crc: %lx\n", new_crc);
		fclose(fp);
	}

	if (old_crc && new_crc && new_crc == old_crc)
		skip_flag = 1;

	if (!skip_flag)
	{
		if(force_flag) {
			killall("tp_init", SIGKILL);
		}
		else {
			eval("adslate", "waitadsl");
			eval("adslate", "quitdrv");
		}
		eval("iptables", "-I", "INPUT", "-p", "udp", "-j", "ACCEPT");
		eval("ifconfig", "vlan2", "169.254.10.10");	//tmp
		eval("route", "add", "-host", ipaddr, "vlan2");

		cprintf("Start to upgrade tc fw\n");
		printf(UpdateFwBuf);
		ret = system(UpdateFwBuf);
		if(ret)
			cprintf("tftp failed\n");
		else
			cprintf("tftp done\n");

		eval("iptables", "-D", "INPUT", "-p", "udp", "-j", "ACCEPT");
	}
	else
		cprintf("Skip upgrade tc firmware\n");
}
