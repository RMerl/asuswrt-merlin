
#include <stdio.h>	     
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/file.h>

#include "rtconfig.h"
#include "bcmnvram.h"
#include "shutils.h"


#ifdef RTCONFIG_USB_MODEM

static inline int Gobi_AtCommand(const char *cmd, const char *file)
{
	const char *modem_act_node;
	int fd;
	int ret = -1;
	const char *lock_file = "/tmp/at_cmd_lock";
	char buf[32];
	int owner = -1;

	if (cmd == NULL || file == NULL)
		return -1;

	while((fd = open(lock_file, O_CREAT | O_RDWR)) < 0 || flock(fd, LOCK_EX /*| LOCK_NB*/) < 0)
	{
		if(fd >= 0)
		{
			if(owner < 0)
			{
				int len = 0;
				if ((len = read(fd, buf, sizeof(buf)-1)) < 0)
					len = 0;
				buf[len] = '\0';
				owner = atoi(buf);
			}
			close(fd);
		}
		cprintf("# %s wait process(%d): fd(%d) pid(%d)!\n", __func__, owner, fd, getpid());
		sleep(1);
	}
	sprintf(buf, "%d", getpid());
	write(fd, buf, strlen(buf));

	unlink(file);

	if (nvram_match("usb_modem_act_type", "tty"))
		modem_act_node = nvram_get("usb_modem_act_bulk");
	else
		modem_act_node = nvram_get("usb_modem_act_int");

	if (modem_act_node != NULL)
	{
		ret = doSystem("chat -t 1 -e '' 'AT%s' OK >> /dev/%s < /dev/%s 2>%s", cmd, modem_act_node, modem_act_node, file);
	}

	flock(fd, LOCK_UN);
	close(fd);
	return ret;
}

static inline char * get_line_by_str(char *line, int size, const char *file, const char *check)
{
	FILE *fp;
	int chk_len;
	char *p = NULL;

	if(line == NULL || file == NULL || check == NULL)
		return NULL;

	chk_len = strlen(check);
	if ((fp = fopen(file, "r")) != NULL)
	{
		while(fgets(line, size, fp)!=NULL) {
			if(strncmp(line, check, chk_len) == 0)
			{
				p = line + chk_len;
				break;
			}
		}
		fclose(fp);
	}
	return p;
}

static inline char * get_line_by_num(char *line, int size, const char *file, int num)
{
	FILE *fp;
	int cnt;
	char *p = NULL;

	if(line == NULL || file == NULL || num <= 0)
		return NULL;

	cnt = 0;
	if ((fp = fopen(file, "r")) != NULL)
	{
		while(fgets(line, size, fp)!=NULL) {
			if (cnt == num)
			{
				p = line;
				break;
			}
			cnt++;
		}
		fclose(fp);
	}
	return p;
}

static inline char * find_field(const char *buf, const char sep, int num, char *value)
{
	int cnt;
	const char *p1, *p2;

	if (buf == NULL || sep == '\0' || num < 0)
		return NULL;

	cnt = 0;
	p1 = buf;
	while((p2 = strchr(p1, sep)))
	{
		if (cnt == num)
		{
			strncpy(value, p1, p2-p1);
			value[p2-p1] = '\0';
			return value;
		}
		p1 = p2+1;
		cnt++;
	}
	if(cnt == num)
	{
		strcpy(value, p1);
		return value;
	}
	return NULL;
}

#define skip_space(p)	{if(p != NULL){ while(isspace(*p)) p++;}}
#define cut_space(p)	{if(p != NULL){ int idx = strlen(p) -1; while(idx > 0 && isspace(p[idx])) p[idx--] = '\0';}}

// system("chat -t 1 -e '' 'AT+CPIN?' OK >> /dev/ttyACM0 < /dev/ttyACM0 2>/tmp/at_cpin; grep OK /tmp/at_cpin -q && v=PASS || v=FAIL; echo $v");
char *Gobi_SimCard(char *line, int size)
{
	const char *atCmd   = "+CPIN?";
	const char *tmpFile = "/tmp/at_cpin";
	char *p = NULL;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, size, tmpFile, "+CPIN: ")) != NULL)
	{
		skip_space(p);
	}
	return p;
}

int Gobi_SimCardReady(const char *status)
{
	if (status != NULL && (strncmp(status, "READY", 5) == 0 || strstr(status, "SIM PIN") != NULL || strstr(status, "SIM PUK") != NULL))
	{ // ready
		return 1;
	}
	return 0;
}


// system("chat -t 1 -e '' 'AT+CGSN' OK >> /dev/ttyACM0 < /dev/ttyACM0 2>/tmp/at_cgsn; [ `sed -n 4p /tmp/at_cgsn | wc -m ` = 16 ] && v=`sed -n 4p /tmp/at_cgsn` || v=FAIL; echo $v");
char *Gobi_IMEI(char *line, int size)
{
	const char *atCmd   = "+CGSN";
	const char *tmpFile = "/tmp/at_cgsn";
	char *p;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_num(line, size, tmpFile, 3)) != NULL)
	{
		int cnt;
		skip_space(p);
		cut_space(p);
		for(cnt = 0; cnt <= 15 && isdigit(p[cnt]); cnt++)
			;
		if (cnt == 15 && !isdigit(p[cnt]))
			return p;
	}
	return NULL;
}

// system("chat -t 1 -e '' 'AT+CGNWS' OK >> /dev/ttyACM0 < /dev/ttyACM0 2>/tmp/at_cgnws; grep +CGNWS: /tmp/at_cgnws | awk -F, '{print $4}' | grep 0x01 -q && v=`grep +CGNWS: /tmp/at_cgnws | awk -F, '{print $7}'` || v=FAIL; echo $v");
char *Gobi_ConnectISP(char *line, int size)
{
	const char *atCmd   = "+CGNWS";
	const char *tmpFile = "/tmp/at_cgnws";
	char str[64];
	char *p;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, size, tmpFile, "+CGNWS:")) != NULL)
	{
		char *f;
		skip_space(p);
		cut_space(p);
		if ((f = find_field(p, ',', 3, str)) != NULL)
		{
			if (strcmp(str, "0x01") == 0)
			{
				if((f = find_field(p, ',', 6, str)) != NULL && strcmp(str, "NULL")) // SPN
				{
					strcpy(line,str);
					return line;
				}
				else if((f = find_field(p, ',', 7, str)) != NULL) // long name
				{
					strcpy(line,str);
					return line;
				}
				else if((f = find_field(p, ',', 8, str)) != NULL) // short name
				{
					strcpy(line,str);
					return line;
				}
				
			}
		}
	}
	return NULL;
}

char *Gobi_ConnectStatus_Str(int status)
{
	switch (status) {
		case 0x01:	return "GPES";
		case 0x02:	return "EDGE";
		case 0x03:	return "HSDPA";
		case 0x04:	return "HSUPA";
		case 0x05:	return "WCDMA(3G)";
		case 0x06:	return "CDMA";
		case 0x07:	return "EV-DO REV 0";
		case 0x08:	return "EV-DO REV A";
		case 0x09:	return "GSM";
		case 0x0A:	return "EV-DO REV B";
		case 0x0B:	return "LTE";
		case 0x0C:	return "HSDPA+";
		case 0x0D:	return "DC-HSDPA+";
	}
	return NULL;
}

// system("chat -t 1 -e '' 'AT$CBEARER' OK >> /dev/ttyACM0 < /dev/ttyACM0 2>/tmp/at_cbearer; st=`grep '$CBEARER:' /tmp/at_cbearer | awk -F: '{print $2}'` ; echo $st | grep '0x0[1-9A-D]' -q && v=$st || v=FAIL; echo $v");
int Gobi_ConnectStatus_Int(void)
{
	const char *atCmd   = "$CBEARER";
	const char *tmpFile = "/tmp/at_cbearer";
	char line[128];
	char *p;
	int value = 0;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, sizeof(line), tmpFile, "$CBEARER:")) != NULL)
	{
		skip_space(p);
		cut_space(p);
		value = strtoul(p, NULL, 0);
	}
	return value;
}


int Gobi_SignalQuality_Percent(int value)
{
	int percent;
	if (value < 0 || value > 31)
		percent = -99;
	else
		percent = (value * 100) / 31;

	return percent;
}

// system("chat -t 1 -e '' 'AT+CSQ' OK >> /dev/ttyACM0 < /dev/ttyACM0 2>/tmp/at_csq; sg=`grep +CSQ: /tmp/at_csq | awk -F: '{print $2}'` ; echo $sg | grep ,99 -q && v=`echo $sg | awk -F, '{print $1}'` || v=FAIL; echo $v");
int Gobi_SignalQuality_Int(void)
{
	const char *atCmd   = "+CSQ";
	const char *tmpFile = "/tmp/at_csq";
	char line[128];
	char *p;
	int value = -1;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, sizeof(line), tmpFile, "+CSQ:")) != NULL)
	{
		char *p2;
		skip_space(p);
		cut_space(p);
		if ((p2 = strstr(p, ",99")) != NULL)
		{
			*p2 = '\0';
			value = strtoul(p, NULL, 0);
		}
	}
	return value;
}

int Gobi_SignalLevel_Int(void)
{
	const char *atCmd   = "$CSIGNAL";
	const char *tmpFile = "/tmp/at_csignal";
	char line[128];
	char *p;
	int value = 0;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, sizeof(line), tmpFile, "$CSIGNAL:")) != NULL)
	{
		skip_space(p);
		cut_space(p);
		if (*p == '-')
		{
			p++;
			value = - strtoul(p, NULL, 0);
		}
	}
	return value;
}

char * Gobi_FwVersion(char *line, int size)
{
	const char *atCmd   = "I";
	const char *tmpFile = "/tmp/ati";
	char *p = NULL;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, size, tmpFile, "WWLC")) != NULL)
	{
		skip_space(p);
		cut_space(p);
	}
	return p;
}

char * Gobi_QcnVersion(char *line, int size)
{
	const char *atCmd   = "$QCNVER";
	const char *tmpFile = "/tmp/at_qcnver";
	char *p = NULL;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_num(line, size, tmpFile, 3)) != NULL)
	{
		skip_space(p);
		cut_space(p);
	}
	return p;
}

char * Gobi_SelectBand(const char *band, char *line, int size)
{
	const char *atCmd1;
	const char *atCmd2 = "+CSETPREFNET=11";
	const char *atCmd3 = "+CFUN=1,1";
	const char *tmpFile1 = "/tmp/at_nv65633";
	const char *tmpFile2 = "/tmp/at_csetprefnet";
	const char *tmpFile3 = "/tmp/at_cfun";
	char *p = NULL;

	if (band == NULL)
	{
		atCmd1 = "$NV65633=0000002000080044";
		atCmd2 = "+CSETPREFNET=10";
	}
	else if (strcmp(band, "B3") == 0)
		atCmd1 = "$NV65633=0000000000000004";
	else if (strcmp(band, "B7") == 0)
		atCmd1 = "$NV65633=0000000000000040";
	else if (strcmp(band, "B20") == 0)
		atCmd1 = "$NV65633=0000000000080000";
	else if (strcmp(band, "B38") == 0)
		atCmd1 = "$NV65633=0000002000000000";
	else
		return NULL;

	if (Gobi_AtCommand(atCmd1, tmpFile1) >= 0
		&& Gobi_AtCommand(atCmd2, tmpFile2) >= 0
		&& Gobi_AtCommand(atCmd3, tmpFile3) >= 0
		&& (p = get_line_by_num(line, size, tmpFile2, 1)) != NULL)
	{
		skip_space(p);
		cut_space(p);
	}
	return p;
}

char * Gobi_BandChannel(char *line, int size)
{
	const char *atCmd   = "$CRFI";
	const char *tmpFile = "/tmp/at_crfi";
	char *p = NULL;

	if (Gobi_AtCommand(atCmd, tmpFile) >= 0
	    && (p = get_line_by_str(line, size, tmpFile, "$CRFI:")) != NULL)
	{
		skip_space(p);
		cut_space(p);
	}
	return p;
}

#endif	/* RTCONFIG_USB_MODEM */

