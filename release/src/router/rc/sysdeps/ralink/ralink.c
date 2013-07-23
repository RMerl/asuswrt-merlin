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

#include <rc.h>
#ifdef RTCONFIG_RALINK
#include <stdio.h>
#include <fcntl.h>		//	for restore175C() from Ralink src
#include <ralink.h>
#include <bcmnvram.h>
//#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if_arp.h>
#include <shutils.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
//#include <linux/if.h>
#include <iwlib.h>
#include <wps.h>
#include <stapriv.h>
#include "flash_mtd.h"

#define MAX_FRW 64
#define MACSIZE 12

#define	DEFAULT_SSID_2G	"ASUS"
#define	DEFAULT_SSID_5G	"ASUS_5G"

#define RTKSWITCH_DEV  "/dev/rtkswitch"

#define LED_CONTROL(led, flag) ralink_gpio_write_bit(led, flag)


typedef struct {
	unsigned int link[5];
	unsigned int speed[5];
} phyState;

int g_wsc_configured = 0;
int g_isEnrollee = 0;

int getCountryRegion5G(const char *countryCode, int *warning);

static void 
iwprivSet(const char *ifname, const char *name, const char *value)
{
	char tmpBuf[256];
	sprintf(tmpBuf, "%s=%s", name, value);
	_dprintf("[%s] %s (%s)\n", __func__, ifname, tmpBuf);
	eval("iwpriv", (char*) ifname, "set", tmpBuf);
}


char *
get_wscd_pidfile()
{
	int wps_band = nvram_get_int("wps_band");
	char tmpstr[32] = "/var/run/wscd.pid.";

	if (wps_band)
		strcat(tmpstr, WIF_5G);
	else
		strcat(tmpstr, WIF_2G);

	return strdup(tmpstr);
}

char *
get_wscd_pidfile_band(int wps_band)
{
	char tmpstr[32] = "/var/run/wscd.pid.";

	if (wps_band)
		strcat(tmpstr, WIF_5G);
	else
		strcat(tmpstr, WIF_5G);

	return strdup(tmpstr);
}

char *
get_wifname(int band)
{
	if (band)
		return WIF_5G;
	else
		return WIF_2G;
}

char *
get_wpsifname(void)
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
		return WIF_5G;
	else
		return WIF_2G;
}

#if 0
char *
get_non_wpsifname()
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
		return WIF_2G;
	else
		return WIF_5G;
}
#endif
int
getMAC_5G()
{
	unsigned char buffer[6];
	char macaddr[18];
	memset(buffer, 0, sizeof(buffer));
	memset(macaddr, 0, sizeof(macaddr));

	if (FRead(buffer, OFFSET_MAC_ADDR, 6)<0)
		dbg("READ MAC address: Out of scope\n");
	else
	{
		ether_etoa(buffer, macaddr);
		puts(macaddr);
	}
	return 0;
}

int
getMAC_2G()
{
	unsigned char buffer[6];
	char macaddr[18];
	memset(buffer, 0, sizeof(buffer));
	memset(macaddr, 0, sizeof(macaddr));

	if (FRead(buffer, OFFSET_MAC_ADDR_2G, 6)<0)
		dbg("READ MAC address 2G: Out of scope\n");
	else
	{
		ether_etoa(buffer, macaddr);
		puts(macaddr);
	}
	return 0;
}

#if defined(RTN14U)
int
eeprom_upgrade(const char *path, int is_bk)
{
#define FLASH_OFFSET	0x40000
	FILE *fp=NULL;
	char buf_org[512], buf_new[512];
	int ret=0;
	int len, offset;

	fp=fopen(path, "r");
	if (!fp) goto quit_out;
	len=fread(buf_new, 1, 512, fp);
	if (len!=512) goto quit_out;
	if (is_bk)
	{
		FRead(buf_org, FLASH_OFFSET, 512);
		offset=OFFSET_BOOT_VER-FLASH_OFFSET;
		memcpy(buf_new+offset,buf_org+offset,4);
		offset=OFFSET_COUNTRY_CODE-FLASH_OFFSET;
		memcpy(buf_new+offset,buf_org+offset,2);
		offset=OFFSET_MAC_ADDR-FLASH_OFFSET;
		memcpy(buf_new+offset,buf_org+offset,6);
		offset=OFFSET_PIN_CODE-FLASH_OFFSET;
		memcpy(buf_new+offset,buf_org+offset,8);
		offset=OFFSET_TXBF_PARA-FLASH_OFFSET;
		memcpy(buf_new+offset,buf_org+offset,33);
	}
	FWrite(buf_new, FLASH_OFFSET, 512);
	ret=1;
quit_out:
	if (fp) fclose(fp);
	if (ret)
		fprintf(stderr,"success!\n");
	else
		fprintf(stderr,"fail!\n");
	return ret;
}
#endif

int
setMAC_5G(const char *mac)
{
	char ea[ETHER_ADDR_LEN];

	if (mac==NULL || !isValidMacAddr(mac))
		return 0;
	if (ether_atoe(mac, ea))
	{
		FWrite(ea, OFFSET_MAC_ADDR, 6);
		FWrite(ea, OFFSET_MAC_GMAC0, 6);
		getMAC_5G();
	}
	return 1;
}

int
setMAC_2G(const char *mac)
{
	char ea[ETHER_ADDR_LEN];

	if (mac==NULL || !isValidMacAddr(mac))
		return 0;
	if (ether_atoe(mac, ea))
	{
		FWrite(ea, OFFSET_MAC_ADDR_2G, 6);
		FWrite(ea, OFFSET_MAC_GMAC2, 6);
		getMAC_2G();
	}
	return 1;
}

#ifdef RTCONFIG_DSL
// used by rc
void get_country_code_from_rc(char* country_code)
{
	unsigned char CC[3];
	memset(CC, 0, sizeof(CC));
	FRead(CC, OFFSET_COUNTRY_CODE, 2);

	if (CC[0] == 0xff && CC[1] == 0xff)
	{
		*country_code++ = 'T';
		*country_code++ = 'W';
		*country_code = 0;	
	}
	else
	{
		*country_code++ = CC[0];
		*country_code++ = CC[1];
		*country_code = 0;	
	}
}
#endif


int
isWifiPower1(const char *cc)
{
	if ((strcmp(cc, "US") == 0)
	 || (strcmp(cc, "TW") == 0))
		return 1;
	return 0;
}

int
isWifiPower2(const char *cc)
{
	if ((strcmp(cc, "CN") == 0)
	 || (strcmp(cc, "GB") == 0))
		return 1;
	return 0;
}

#if !defined(RTN14U) && !defined(RTAC52U)
//for specific power
int
isWifiPower3(const char *cc)
{
	if ((strcmp(cc, "DB") == 0)
	 || (strcmp(cc, "Z1") == 0)
	 || (strcmp(cc, "Z2") == 0)
	 || (strcmp(cc, "Z3") == 0)
	 || (strcmp(cc, "Z4") == 0))
		return 1;
	return 0;
}

void
modify_ralink_power_2g(const char *old_CC, const char *CC, unsigned char *flash_buf)
{
	const unsigned char power1Value[] = {0x66,0x66,0x66,0x66,0x66,0x66,0x77,0x66,0x55,0x33,0x77,0x66,0x55,0x11,0x77,0x66,0x55,0x33};
	const unsigned char power2Value[] = {0x33,0x33,0x33,0x33,0x33,0x33,0x44,0x44,0x33,0x33,0x44,0x44,0x33,0x11,0x44,0x44,0x33,0x33};
	const unsigned char power3Value[] = {0x77,0x77,0x77,0x77,0x77,0x66,0x77,0x66,0x55,0x33,0x77,0x66,0x55,0x11,0x77,0x66,0x55,0x33};
	const unsigned char *powerValue = NULL;
	int len;

	if(!isValidCountryCode(CC))
	{
	}
	else if(isWifiPower1(CC))
	{
		powerValue = power1Value;
		len = sizeof(power1Value);
	}
	else if(isWifiPower2(CC))
	{
		powerValue = power2Value;
		len = sizeof(power2Value);
	}
	//for specific power
	else if(isWifiPower3(CC))
	{
		printf("use power3\n");
		powerValue = power3Value;
		len = sizeof(power3Value);
	}
	memcpy(flash_buf + (OFFSET_POWER_2G - OFFSET_MTD_FACTORY), powerValue, len);
}

void
modify_ralink_power_5g(const char *old_CC, const char *CC, unsigned char *flash_buf)
{
	signed char modify;
	signed char oldP = 0, newP = 0;

	
	if(!isValidCountryCode(old_CC))
		oldP = 0;
	else if(isWifiPower1(old_CC))
		oldP = -7;
	else if(isWifiPower2(old_CC))
		oldP = 0;
	else if(isWifiPower3(old_CC))
	   	oldP = 0;

	if(!isValidCountryCode(CC))
		newP = 0;
	else if(isWifiPower1(CC))
		newP = -7;
	else if(isWifiPower2(CC))
		newP = 0;
	else if(isWifiPower3(CC))
	   	newP = 0;

	modify = newP - oldP;

	_dprintf("old(%s, %d) new(%s, %d) modify(%d)\n", old_CC, oldP, CC, newP, modify);
	if(modify != 0)
	{
		struct ralink_eeprom {unsigned int offset; int number;};
		const struct ralink_eeprom pa[] = {
			{OFFSET_POWER_5G_TX0_36_x6, 6},
			{OFFSET_POWER_5G_TX1_36_x6, 6},
			{OFFSET_POWER_5G_TX2_36_x6, 6},
		};
		int i,cnt;
		unsigned char *pPower;

		for(i =0; i < ARRAY_SIZE(pa); i++)
		{
			pPower = flash_buf + (pa[i].offset - OFFSET_MTD_FACTORY);
			for(cnt = 0; cnt < pa[i].number; cnt++)
			{
				pPower[cnt] += modify;
			}
		}
	}
}
#endif

#if defined(RTCONFIG_NEW_REGULATION_DOMAIN)
int getRegSpec(void)
{
	char value_str[MAX_REGSPEC_LEN+1];
	int i;

	memset(value_str, 0, sizeof(value_str));
	FRead(value_str, REGSPEC_ADDR, MAX_REGSPEC_LEN);
	for(i = 0; i < MAX_REGSPEC_LEN && value_str[i] != '\0'; i++) {
		if ((unsigned char)value_str[i] == 0xff)
		{
			value_str[i] = '\0';
			break;
		}
	}
	puts(value_str);
	return 0;
}

int getRegDomain_2G(void)
{
	char value_str[MAX_REGDOMAIN_LEN+1];
	int i;

	memset(value_str, 0, sizeof(value_str));
	FRead(value_str, REG2G_EEPROM_ADDR, MAX_REGDOMAIN_LEN);

	for(i=0; i<MAX_REGDOMAIN_LEN; ++i) {
		if ((value_str[i]==(char)0xFF) || (value_str[i]=='\0'))
			break;
		printf("%c", value_str[i]);
	}
	printf("\n");
	return 0;
}

int getRegDomain_5G(void)
{
	char value_str[MAX_REGDOMAIN_LEN+1];
	int i;

	memset(value_str, 0, sizeof(value_str));
	FRead(value_str, REG5G_EEPROM_ADDR, MAX_REGDOMAIN_LEN);

	for(i=0; i<MAX_REGDOMAIN_LEN; ++i) {
		if ((value_str[i]==(char)0xFF) || (value_str[i]=='\0'))
			break;
		printf("%c", value_str[i]);
	}
	printf("\n");
	return 0;
}


int setRegSpec(const char *regSpec)
{
	char REGSPEC[MAX_REGSPEC_LEN+1];
	int i;

	if (regSpec == NULL || regSpec[0] == '\0' || strlen(regSpec) > 3)
		return -1;
	if (!strcasecmp(regSpec, "CE")) ;
	else if (!strcasecmp(regSpec, "FCC")) ;
	else
		return -1;

	memset(REGSPEC, 0, sizeof(REGSPEC));
	for (i=0; regSpec[i]!='\0' ;i++)
		REGSPEC[i]=(char)toupper(regSpec[i]);
	FWrite(REGSPEC, REGSPEC_ADDR, MAX_REGSPEC_LEN);
	return 0;
}

int setRegDomain_2G(const char *cc)
{
	char CC[MAX_REGDOMAIN_LEN+1];
	int i;

	if (!strcasecmp(cc, "2G_CH11")) ;
	else if (!strcasecmp(cc, "2G_CH13")) ;
	else if (!strcasecmp(cc, "2G_CH14")) ;
	else
		return -1;

	memset(CC, 0x0, sizeof(CC));
	strcpy(CC, cc);
	for (i=0;CC[i]!='\0';i++)
		CC[i]=(char)toupper(CC[i]);
	FWrite(CC, REG2G_EEPROM_ADDR, MAX_REGDOMAIN_LEN);
	return 0;
}

int setRegDomain_5G(const char *cc)
{
	char CC[MAX_REGDOMAIN_LEN+1];
	int i;

	if (!strcasecmp(cc, "5G_ALL")) ;
	else if (!strcasecmp(cc, "5G_BAND14")) ;
	else if (!strcasecmp(cc, "5G_BAND24")) ;
	else if (!strcasecmp(cc, "5G_BAND1")) ;
	else if (!strcasecmp(cc, "5G_BAND4")) ;
	else if (!strcasecmp(cc, "5G_BAND123")) ;
	else
		return -1;

	memset(CC, 0x0, sizeof(CC));
	strcpy(CC, cc);
	for (i=0;CC[i]!='\0';i++)
		CC[i]=(char)toupper(CC[i]);
	FWrite(CC, REG5G_EEPROM_ADDR, MAX_REGDOMAIN_LEN);
	return 0;
}


#else	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */

int
getCountryCode_2G()
{
	unsigned char CC[3];

	memset(CC, 0, sizeof(CC));
	FRead(CC, OFFSET_COUNTRY_CODE, 2);
	if (CC[0] == 0xff && CC[1] == 0xff)	// 0xffff is default
		;
	else
		puts(CC);
	return 1;
}

int
setCountryCode_2G(const char *cc)
{
	char CC[3];

	if (cc==NULL || !isValidCountryCode(cc))
		return 0;
	/* Please refer to ISO3166 code list for other countries and can be found at
	 * http://www.iso.org/iso/en/prods-services/iso3166ma/02iso-3166-code-lists/list-en1.html#sz
	 */
	else if (!strcasecmp(cc, "AE")) ;
	else if (!strcasecmp(cc, "AL")) ;
	else if (!strcasecmp(cc, "AM")) ;
	else if (!strcasecmp(cc, "AR")) ;
	else if (!strcasecmp(cc, "AT")) ;
	else if (!strcasecmp(cc, "AU")) ;
	else if (!strcasecmp(cc, "AZ")) ;
	else if (!strcasecmp(cc, "BE")) ;
	else if (!strcasecmp(cc, "BG")) ;
	else if (!strcasecmp(cc, "BH")) ;
	else if (!strcasecmp(cc, "BN")) ;
	else if (!strcasecmp(cc, "BO")) ;
	else if (!strcasecmp(cc, "BR")) ;
	else if (!strcasecmp(cc, "BY")) ;
	else if (!strcasecmp(cc, "BZ")) ;
	else if (!strcasecmp(cc, "CA")) ;
	else if (!strcasecmp(cc, "CH")) ;
	else if (!strcasecmp(cc, "CL")) ;
	else if (!strcasecmp(cc, "CN")) ;
	else if (!strcasecmp(cc, "CO")) ;
	else if (!strcasecmp(cc, "CR")) ;
	else if (!strcasecmp(cc, "CY")) ;
	else if (!strcasecmp(cc, "CZ")) ;
	else if (!strcasecmp(cc, "DB")) ;
	else if (!strcasecmp(cc, "DE")) ;
	else if (!strcasecmp(cc, "DK")) ;
	else if (!strcasecmp(cc, "DO")) ;
	else if (!strcasecmp(cc, "DZ")) ;
	else if (!strcasecmp(cc, "EC")) ;
	else if (!strcasecmp(cc, "EE")) ;
	else if (!strcasecmp(cc, "EG")) ;
	else if (!strcasecmp(cc, "ES")) ;
	else if (!strcasecmp(cc, "FI")) ;
	else if (!strcasecmp(cc, "FR")) ;
	else if (!strcasecmp(cc, "GB")) ;
	else if (!strcasecmp(cc, "GE")) ;
#if defined(RTN14U) || defined(RTAC52U)
	else if (!strcasecmp(cc, "EU")) ;
#endif
	else if (!strcasecmp(cc, "GR")) ;
	else if (!strcasecmp(cc, "GT")) ;
	else if (!strcasecmp(cc, "HK")) ;
	else if (!strcasecmp(cc, "HN")) ;
	else if (!strcasecmp(cc, "HR")) ;
	else if (!strcasecmp(cc, "HU")) ;
	else if (!strcasecmp(cc, "ID")) ;
	else if (!strcasecmp(cc, "IE")) ;
	else if (!strcasecmp(cc, "IL")) ;
	else if (!strcasecmp(cc, "IN")) ;
	else if (!strcasecmp(cc, "IR")) ;
	else if (!strcasecmp(cc, "IS")) ;
	else if (!strcasecmp(cc, "IT")) ;
	else if (!strcasecmp(cc, "JO")) ;
	else if (!strcasecmp(cc, "JP")) ;
	else if (!strcasecmp(cc, "KP")) ;
	else if (!strcasecmp(cc, "KR")) ;
	else if (!strcasecmp(cc, "KW")) ;
	else if (!strcasecmp(cc, "KZ")) ;
	else if (!strcasecmp(cc, "LB")) ;
	else if (!strcasecmp(cc, "LI")) ;
	else if (!strcasecmp(cc, "LT")) ;
	else if (!strcasecmp(cc, "LU")) ;
	else if (!strcasecmp(cc, "LV")) ;
	else if (!strcasecmp(cc, "MA")) ;
	else if (!strcasecmp(cc, "MC")) ;
	else if (!strcasecmp(cc, "MK")) ;
	else if (!strcasecmp(cc, "MO")) ;
	else if (!strcasecmp(cc, "MX")) ;
	else if (!strcasecmp(cc, "MY")) ;
	else if (!strcasecmp(cc, "NL")) ;
	else if (!strcasecmp(cc, "NO")) ;
	else if (!strcasecmp(cc, "NZ")) ;
	else if (!strcasecmp(cc, "OM")) ;
	else if (!strcasecmp(cc, "PA")) ;
	else if (!strcasecmp(cc, "PE")) ;
	else if (!strcasecmp(cc, "PH")) ;
	else if (!strcasecmp(cc, "PK")) ;
	else if (!strcasecmp(cc, "PL")) ;
	else if (!strcasecmp(cc, "PR")) ;
	else if (!strcasecmp(cc, "PT")) ;
	else if (!strcasecmp(cc, "QA")) ;
	else if (!strcasecmp(cc, "RO")) ;
	else if (!strcasecmp(cc, "RU")) ;
	else if (!strcasecmp(cc, "SA")) ;
	else if (!strcasecmp(cc, "SE")) ;
	else if (!strcasecmp(cc, "SG")) ;
	else if (!strcasecmp(cc, "SI")) ;
	else if (!strcasecmp(cc, "SK")) ;
	else if (!strcasecmp(cc, "SV")) ;
	else if (!strcasecmp(cc, "SY")) ;
	else if (!strcasecmp(cc, "TH")) ;
	else if (!strcasecmp(cc, "TN")) ;
	else if (!strcasecmp(cc, "TR")) ;
	else if (!strcasecmp(cc, "TT")) ;
	else if (!strcasecmp(cc, "TW")) ;
	else if (!strcasecmp(cc, "UA")) ;
	else if (!strcasecmp(cc, "US")) ;
	else if (!strcasecmp(cc, "UY")) ;
	else if (!strcasecmp(cc, "UZ")) ;
	else if (!strcasecmp(cc, "VE")) ;
	else if (!strcasecmp(cc, "VN")) ;
	else if (!strcasecmp(cc, "YE")) ;
	else if (!strcasecmp(cc, "ZA")) ;
	else if (!strcasecmp(cc, "ZW")) ;
#if !defined(RTN14U) && !defined(RTAC52U)
	//for specific power
	else if (!strcasecmp(cc, "Z1")) ; //US
	else if (!strcasecmp(cc, "Z2")) ; //GB
	else if (!strcasecmp(cc, "Z3")) ; //TW
	else if (!strcasecmp(cc, "Z4")) ; //CN
#endif
	else
	{
		return 0;
	}

	memset(&CC[0], toupper(cc[0]), 1);
	memset(&CC[1], toupper(cc[1]), 1);
	memset(&CC[2], 0, 1);

#if defined(RTN14U)
	FWrite(CC, OFFSET_COUNTRY_CODE, 2);
#else
#define MTD_SIZE_FACTORY 0x10000
    {
	unsigned char *flash_buf;
	if((flash_buf = malloc(MTD_SIZE_FACTORY)) == NULL)
	{
		return 0;
	}
	FRead(flash_buf, OFFSET_MTD_FACTORY, MTD_SIZE_FACTORY);

#if defined(RTN65U) // adjust power for different regulation domain
	if(get_model() == MODEL_RTN65U)
	{
		char old_CC[3];
		FRead(old_CC, OFFSET_COUNTRY_CODE, 2);
		old_CC[2] = '\0';
		modify_ralink_power_2g(old_CC, CC, flash_buf);
		modify_ralink_power_5g(old_CC, CC, flash_buf);
	}
#endif // RTN65U
	memcpy(flash_buf + (OFFSET_COUNTRY_CODE - OFFSET_MTD_FACTORY), CC, 2);
	FWrite(flash_buf, OFFSET_MTD_FACTORY, MTD_SIZE_FACTORY);
	free(flash_buf);
    }
#endif

	puts(CC);
	return 1;
}
#endif	/* ! RTCONFIG_NEW_REGULATION_DOMAIN */

static unsigned char nibble_hex(char *c)
{
	int val;
	char tmpstr[3];

	tmpstr[2]='\0';
	memcpy(tmpstr,c,2);
	val= strtoul(tmpstr, NULL, 16);
	return val;
}

static int atoh(const char *a, unsigned char *e)
{
	char *c = (char *) a;
	int i = 0;

	memset(e, 0, MAX_FRW);
	for (i = 0; i < MAX_FRW; ++i, c += 3) {
		if (!isxdigit(*c) || !isxdigit(*(c+1)) || isxdigit(*(c+2))) // should be "AA:BB:CC:DD:..."
			break;
		e[i] = (unsigned char) nibble_hex(c);
	}

	return i;
}

char *
htoa(const unsigned char *e, char *a, int len)
{
	char *c = a;
	int i;

	for (i = 0; i < len; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", e[i] & 0xff);
	}
	return a;
}

int
FREAD(unsigned int addr_sa, int len)
{
	unsigned char buffer[MAX_FRW];
	char buffer_h[128];
	memset(buffer, 0, sizeof(buffer));
	memset(buffer_h, 0, sizeof(buffer_h));

	if (FRead(buffer, addr_sa, len)<0)
		dbg("FREAD: Out of scope\n");
	else
	{
		if (len > MAX_FRW)
			len = MAX_FRW;
		htoa(buffer, buffer_h, len);
		puts(buffer_h);
	}
	return 0;
}

/*
 * 	write str_hex to offset da
 *	console input:	FWRITE 0x45000 00:11:22:33:44:55:66:77
 *	console output:	00:11:22:33:44:55:66:77
 *
 */
int
FWRITE(const char *da, const char* str_hex)
{
	unsigned char ee[MAX_FRW];
	unsigned int addr_da;
	int len;

	addr_da = strtoul(da, NULL, 16);
	if (addr_da && (len = atoh(str_hex, ee)))
	{
		FWrite(ee, addr_da, len);
		FREAD(addr_da, len);
	}
	return 0;
}

int getSN(void)
{
	char sn[SERIAL_NUMBER_LENGTH +1];

	if (FRead(sn, OFFSET_SERIAL_NUMBER, SERIAL_NUMBER_LENGTH) < 0)
		dbg("READ Serial Number: Out of scope\n");
	else
	{
		sn[SERIAL_NUMBER_LENGTH] = '\0';
		puts(sn);
	}
	return 1;
}

int setSN(const char *SN)
{
	if (SN==NULL || !isValidSN(SN))
		return 0;

	if (FWrite(SN, OFFSET_SERIAL_NUMBER, SERIAL_NUMBER_LENGTH) < 0)
		return 0;

	getSN();
	return 1;
}
#ifdef RTCONFIG_ODMPID
int setMN(const char *MN)
{
	char modelname[16];

	if(MN==NULL || !is_valid_hostname(MN))
		return 0;

	memset(modelname, 0, sizeof(modelname));
	strncpy(modelname, MN, sizeof(modelname) -1);
	FWrite(modelname, OFFSET_ODMPID, sizeof(modelname));

	nvram_set("odmpid", modelname);
	puts(nvram_safe_get("odmpid"));
	return 1;
}

int getMN(void)
{
	puts(nvram_safe_get("odmpid"));
	return 1;
}	
#endif
int
setPIN(const char *pin)
{
	if (pincheck(pin))
	{
		FWrite(pin, OFFSET_PIN_CODE, 8);
		char PIN[9];
		memset(PIN, 0, 9);
		memcpy(PIN, pin, 8);
		puts(PIN);
		return 1;
	}
	return 0;	
}

int
getBootVer()
{
	unsigned char btv[5];
	char output_buf[32];
	memset(btv, 0, sizeof(btv));
	memset(output_buf, 0, sizeof(output_buf));
	FRead(btv, OFFSET_BOOT_VER, 4);
	sprintf(output_buf, "%s-%c.%c.%c.%c", nvram_safe_get("productid"), btv[0], btv[1], btv[2], btv[3]);
	puts(output_buf);

	return 0;
}

int
getPIN()
{
	unsigned char PIN[9];
	memset(PIN, 0, sizeof(PIN));
	FRead(PIN, OFFSET_PIN_CODE, 8);
	if (PIN[0]!=0xff)
		puts(PIN);
	return 0;
}

//Supports both RTL8367M and RTL8367R Realtek switch
/*
 * This function is used by factory only and always
 * think the LAN port next to WAN port as LAN1
 * even WebUI/case define another LAN port as LAN1.
 */
int
GetPhyStatus(void)
{
#if defined(RTN14U) || defined(RTAC52U)
	ATE_mt7620_esw_port_status();
	return 1;
#else
	int fd;
	char buf[32];
	int porder_56u[5] = {4,3,2,1,0};
	int *o = porder_56u;

#ifdef RTCONFIG_DSL
	int porder_dsl[5] = {0,1,2,3,4};

	o = porder_dsl;
#endif

	fd = open(RTKSWITCH_DEV, O_RDONLY);
	if (fd < 0) {
		perror(RTKSWITCH_DEV);
		return 0;
	}

	phyState pS;

	pS.link[0] = pS.link[1] = pS.link[2] = pS.link[3] = pS.link[4] = 0;
	pS.speed[0] = pS.speed[1] = pS.speed[2] = pS.speed[3] = pS.speed[4] = 0;

	if (ioctl(fd, 18, &pS) < 0)
	{
		sprintf(buf, "ioctl: %s", RTKSWITCH_DEV);
		perror(buf);
		close(fd);
		return 0;
	}

	close(fd);

	sprintf(buf, "W0=%C;L1=%C;L2=%C;L3=%C;L4=%C;",
		(pS.link[o[0]] == 1) ? (pS.speed[o[0]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[1]] == 1) ? (pS.speed[o[1]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[2]] == 1) ? (pS.speed[o[2]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[3]] == 1) ? (pS.speed[o[3]] == 2) ? 'G' : 'M': 'X',
		(pS.link[o[4]] == 1) ? (pS.speed[o[4]] == 2) ? 'G' : 'M': 'X');

	puts(buf);
	return 1;
#endif
}

int
setAllLedOn(void)
{
#ifdef RTCONFIG_DSL
  	LED_CONTROL(RA_LED_POWER, RA_LED_ON);
  	LED_CONTROL(RA_LED_WAN, RA_LED_ON);
#else
	led_control(LED_POWER, LED_ON);
	led_control(LED_WAN  , LED_ON);
	led_control(LED_LAN  , LED_ON);
	led_control(LED_USB  , LED_ON);
#if defined(RTN14U)
	led_control(LED_2G  , LED_ON);
#endif
#ifdef RTCONFIG_LED_ALL
	led_control(LED_ALL  , LED_ON);
#endif
#endif
	puts("1");
	return 0;
}

int
setAllLedOff(void)
{
#ifdef RTCONFIG_DSL
	LED_CONTROL(RA_LED_POWER, RA_LED_OFF);
	LED_CONTROL(RA_LED_WAN, RA_LED_OFF);
#else
	led_control(LED_POWER, LED_OFF);
	led_control(LED_WAN  , LED_OFF);
	led_control(LED_LAN  , LED_OFF);
	led_control(LED_USB  , LED_OFF);
#if defined(RTN14U)
	led_control(LED_2G  , LED_OFF);
#endif
#ifdef RTCONFIG_LED_ALL
	led_control(LED_ALL  , LED_OFF);
#endif
#endif
	puts("1");
	return 0;
}

int 
ResetDefault(void)
{
	eval("mtd-erase","-d","nvram");
	puts("1");
	return 0;
}

int
set40M_Channel_2G(char *channel)
{
	char chl_buf[12];
	if (channel==NULL || !isValidChannel(1, channel))
		return 0;
	sprintf(chl_buf,"Channel=%s", channel);
	eval("iwpriv", WIF_2G, "set", chl_buf);
	eval("iwpriv", WIF_2G, "set", "HtBw=1");
	puts("1");
	return 1;
}

int
set40M_Channel_5G(char *channel)
{
	char chl_buf[12];
	if (channel==NULL || !isValidChannel(0, channel))
		return 0;
	sprintf(chl_buf,"Channel=%s", channel);
	eval("iwpriv", WIF_5G, "set", chl_buf);
	eval("iwpriv", WIF_5G, "set", "HtBw=1");
	puts("1");
	return 1;
}


int Get_channel_list(int unit)
{
	unsigned char countryCode[3];
	char chList[256];

#ifdef RTCONFIG_NEW_REGULATION_DOMAIN
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	memset(countryCode, 0, sizeof(countryCode));
	strncpy(countryCode, nvram_safe_get(strcat_r(prefix, "country_code", tmp)), 2);
#else
	memset(countryCode, 0, sizeof(countryCode));
	FRead(countryCode, OFFSET_COUNTRY_CODE, 2);
#endif
	if(get_channel_list_via_driver(unit, chList, sizeof(chList)) > 0)
	{
		puts(chList);
	}
	else if (countryCode[0] != 0xff && countryCode[1] != 0xff)	// 0xffff is default
	{
		if(get_channel_list_via_country(unit, countryCode, chList, sizeof(chList)) > 0)
		{
			puts(chList);
		}
	}
	return 1;
}

int Get_ChannelList_2G(void)
{
	return Get_channel_list(0);
}

int Get_ChannelList_5G(void)
{
	return Get_channel_list(1);
}
//End of new ATE Command

int getCountryRegion2G(const char *countryCode)
{
	if (countryCode == NULL)
	{
		return 5;	// 1-14
	}
	else if((strcasecmp(countryCode, "CA") == 0) || (strcasecmp(countryCode, "CO") == 0) ||
		(strcasecmp(countryCode, "DO") == 0) || (strcasecmp(countryCode, "GT") == 0) ||
		(strcasecmp(countryCode, "MX") == 0) || (strcasecmp(countryCode, "NO") == 0) ||
		(strcasecmp(countryCode, "PA") == 0) || (strcasecmp(countryCode, "PR") == 0) ||
		(strcasecmp(countryCode, "TW") == 0) || (strcasecmp(countryCode, "US") == 0) ||
		(strcasecmp(countryCode, "UZ") == 0) || 
		(strcasecmp(countryCode, "Z1") == 0) || (strcasecmp(countryCode, "Z3") == 0)  
		)
	{
		return 0;	// 1-11
	}
	else if (strcasecmp(countryCode, "DB") == 0  || strcasecmp(countryCode, "") == 0)
	{
		return 5;	// 1-14
	}

	return 1;	// 1-13
}

int getChannelNumMax2G(int region)
{
	switch(region)
	{
		case 0: return 11;
		case 1: return 13;
		case 5: return 14;
	}
	return 14;
}

int getCountryRegion5G(const char *countryCode, int *warning)
{
	if (		(!strcasecmp(countryCode, "AE")) ||
			(!strcasecmp(countryCode, "AL")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "AR")) ||
#endif
			(!strcasecmp(countryCode, "AU")) ||
			(!strcasecmp(countryCode, "BH")) ||
			(!strcasecmp(countryCode, "BY")) ||
			(!strcasecmp(countryCode, "CA")) ||
			(!strcasecmp(countryCode, "CL")) ||
			(!strcasecmp(countryCode, "CO")) ||
			(!strcasecmp(countryCode, "CR")) ||
			(!strcasecmp(countryCode, "DO")) ||
			(!strcasecmp(countryCode, "DZ")) ||
			(!strcasecmp(countryCode, "EC")) ||
			(!strcasecmp(countryCode, "GT")) ||
			(!strcasecmp(countryCode, "HK")) ||
			(!strcasecmp(countryCode, "HN")) ||
			(!strcasecmp(countryCode, "IL")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "IN")) ||
#endif
			(!strcasecmp(countryCode, "JO")) ||
			(!strcasecmp(countryCode, "KW")) ||
			(!strcasecmp(countryCode, "KZ")) ||
			(!strcasecmp(countryCode, "LB")) ||
			(!strcasecmp(countryCode, "MA")) ||
			(!strcasecmp(countryCode, "MK")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "MO")) ||
			(!strcasecmp(countryCode, "MX")) ||
#endif
			(!strcasecmp(countryCode, "MY")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "NO")) ||
#endif
			(!strcasecmp(countryCode, "NZ")) ||
			(!strcasecmp(countryCode, "OM")) ||
			(!strcasecmp(countryCode, "PA")) ||
			(!strcasecmp(countryCode, "PK")) ||
			(!strcasecmp(countryCode, "PR")) ||
			(!strcasecmp(countryCode, "QA")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "RO")) ||
			(!strcasecmp(countryCode, "RU")) ||
#endif
			(!strcasecmp(countryCode, "SA")) ||
			(!strcasecmp(countryCode, "SG")) ||
			(!strcasecmp(countryCode, "SV")) ||
			(!strcasecmp(countryCode, "SY")) ||
			(!strcasecmp(countryCode, "TH")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "UA")) ||
#endif
			(!strcasecmp(countryCode, "US")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "UY")) ||
#endif
			(!strcasecmp(countryCode, "VN")) ||
			(!strcasecmp(countryCode, "YE")) ||
			(!strcasecmp(countryCode, "ZW")) ||
#if !defined(RTN14U) && !defined(RTAC52U)
			//for specific power
			(!strcasecmp(countryCode, "Z1")) 
#else
			0
#endif
	)
	{
		return 0;
	}
	else if (
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "AM")) ||
#endif
			(!strcasecmp(countryCode, "AT")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "AZ")) ||
#endif
			(!strcasecmp(countryCode, "BE")) ||
			(!strcasecmp(countryCode, "BG")) ||
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "BR")) ||
#endif
			(!strcasecmp(countryCode, "CH")) ||
			(!strcasecmp(countryCode, "CY")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "CZ")) ||
#endif
			(!strcasecmp(countryCode, "DE")) ||
			(!strcasecmp(countryCode, "DK")) ||
			(!strcasecmp(countryCode, "EE")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "EG")) ||
#endif
			(!strcasecmp(countryCode, "ES")) ||
			(!strcasecmp(countryCode, "FI")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "FR")) ||
#endif
			(!strcasecmp(countryCode, "GB")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "GE")) ||
#endif
			(!strcasecmp(countryCode, "GR")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "HR")) ||
#endif
			(!strcasecmp(countryCode, "HU")) ||
			(!strcasecmp(countryCode, "IE")) ||
			(!strcasecmp(countryCode, "IS")) ||
			(!strcasecmp(countryCode, "IT")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "JP")) ||
			(!strcasecmp(countryCode, "KP")) ||
			(!strcasecmp(countryCode, "KR")) ||
#endif
			(!strcasecmp(countryCode, "LI")) ||
			(!strcasecmp(countryCode, "LT")) ||
			(!strcasecmp(countryCode, "LU")) ||
			(!strcasecmp(countryCode, "LV")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "MC")) ||
#endif
			(!strcasecmp(countryCode, "NL")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "NO")) ||
#endif
			(!strcasecmp(countryCode, "PL")) ||
			(!strcasecmp(countryCode, "PT")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "RO")) ||
#endif
			(!strcasecmp(countryCode, "SE")) ||
			(!strcasecmp(countryCode, "SI")) ||
			(!strcasecmp(countryCode, "SK")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "TN")) ||
			(!strcasecmp(countryCode, "TR")) ||
			(!strcasecmp(countryCode, "TT")) ||
#endif
			(!strcasecmp(countryCode, "UZ")) ||
			(!strcasecmp(countryCode, "ZA")) ||
#if !defined(RTN14U) && !defined(RTAC52U)
			//for specific power
			(!strcasecmp(countryCode, "Z2"))
#else
			0
#endif
	)
	{
		return 1;
	}
	else if (
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "AM")) ||
			(!strcasecmp(countryCode, "AZ")) ||
			(!strcasecmp(countryCode, "CZ")) ||
			(!strcasecmp(countryCode, "EG")) ||
			(!strcasecmp(countryCode, "FR")) ||
			(!strcasecmp(countryCode, "GE")) ||
			(!strcasecmp(countryCode, "HR")) ||
			(!strcasecmp(countryCode, "MC")) ||
			(!strcasecmp(countryCode, "TN")) ||
			(!strcasecmp(countryCode, "TR")) ||
			(!strcasecmp(countryCode, "TT"))
#else
			(!strcasecmp(countryCode, "IN")) ||
			(!strcasecmp(countryCode, "MX"))
#endif
	)
	{
		return 2;
	}
	else if (
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "AR")) ||
#endif
			(!strcasecmp(countryCode, "TW")) ||
#if !defined(RTN14U) && !defined(RTAC52U)
			//for specific power
			(!strcasecmp(countryCode, "Z3")) 
#else
			0
#endif
	)
	{
		return 3;
	}
	else if (
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "BR")) ||
#endif
			(!strcasecmp(countryCode, "BZ")) ||
			(!strcasecmp(countryCode, "BO")) ||
			(!strcasecmp(countryCode, "BN")) ||
			(!strcasecmp(countryCode, "CN")) ||
			(!strcasecmp(countryCode, "ID")) ||
			(!strcasecmp(countryCode, "IR")) ||
#ifdef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "MO")) ||
#endif
			(!strcasecmp(countryCode, "PE")) ||
			(!strcasecmp(countryCode, "PH"))
#ifdef RTCONFIG_LOCALE2012
						 ||
			(!strcasecmp(countryCode, "VE"))
#endif
						 ||
#if !defined(RTN14U) && !defined(RTAC52U)
			//for specific power
			(!strcasecmp(countryCode, "Z4")) 
#else
			0
#endif
	)
	{
		return 4;
	}
#ifndef RTCONFIG_LOCALE2012
	else if (	(!strcasecmp(countryCode, "KP")) ||
			(!strcasecmp(countryCode, "KR")) ||
			(!strcasecmp(countryCode, "UY")) ||
			(!strcasecmp(countryCode, "VE"))
	)
	{
		return 5;
	}
#else
	else if (!strcasecmp(countryCode, "RU"))
	{
		return 6;
	}
#endif
	else if (!strcasecmp(countryCode, "DB"))
	{
		return 7;
	}
	else if (
#ifndef RTCONFIG_LOCALE2012
			(!strcasecmp(countryCode, "JP"))
#else
			(!strcasecmp(countryCode, "UA"))
#endif
	)
	{
		return 9;
	}
	else
	{
		if (warning)
			*warning = 2;
		return 7;
	}
}

//Ren.B
int check_macmode(const char *str)
{
	if((!str)||(!strcmp(str, ""))||(!strcmp(str, "disabled")))
	{
		return 0;
	}

	if(strcmp(str, "allow")==0)
	{
		return 1;
	}

	if(strcmp(str, "deny")==0)
	{
		return 2;
	}
	return 0;
}
//Ren.E

//Ren.B
void gen_macmode(int mac_filter[], int band)
{
	int i,j;
	char temp[128], prefix_mssid[] = "wlXXXXXXXXXX_mssid_";

	snprintf(temp, sizeof(temp), "wl%d_macmode", band);
	mac_filter[0] = check_macmode(nvram_get(temp));
	_dprintf("mac_filter[0] = %d\n", mac_filter[0]);
	if(mac_filter[0] == 0)
	{
		for(i = 1, j = 1; i < MAX_NO_MSSID; i++)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);
			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			mac_filter[j] = 0;
			_dprintf("mac_filter[%d] = %d\n", j, mac_filter[j]);
			j++;
		}
	}
	else
	{
		for(i = 1, j = 1; i < MAX_NO_MSSID; i++)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);
			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			if(nvram_match(strcat_r(prefix_mssid, "macmode", temp), "disabled"))
			{
				mac_filter[j] = 0;
			}
			else
			{
				mac_filter[j] = mac_filter[0]; //copy rule from "primary" WiFi interface.
			}
			_dprintf("mac_filter[%d] = %d\n", j, mac_filter[j]);
			j++;
		}
	}
}
//Ren.E

int gen_ralink_config(int band, int is_iNIC)
{
	FILE *fp;
	char *str = NULL;
	char *str2 = NULL;
	int  i;
	int ssid_num = 1;
	char wmm_enable[8];
	char wmm_noack[8];
	char list[2048];
	int flag_8021x = 0;
	int wsc_configure = 0;
	int warning = 0;
	int ChannelNumMax_2G = 11;
	char tmp[128], prefix[] = "wlXXXXXXX_";
	char temp[128], prefix_mssid[] = "wlXXXXXXXXXX_mssid_";
	char tmpstr[128];
	int j;
	char *nv, *nvp, *b;
	int wl_key_type[MAX_NO_MSSID];
	int mcast_phy, mcast_mcs;
	int mac_filter[MAX_NO_MSSID];
#if defined(RTAC52U)
	int VHTBW_MAX = 0;
#endif

	if (!is_iNIC)
	{
		_dprintf("gen ralink config\n");
		system("mkdir -p /etc/Wireless/RT2860");
		if (!(fp=fopen("/etc/Wireless/RT2860/RT2860.dat", "w+")))
			return 0;
	}
	else
	{
		_dprintf("gen ralink iNIC config\n");
		system("mkdir -p /etc/Wireless/iNIC");
		if (!(fp=fopen("/etc/Wireless/iNIC/iNIC_ap.dat", "w+")))
			return 0;
	}

	fprintf(fp, "#The word of \"Default\" must not be removed\n");
	fprintf(fp, "Default\n");

	snprintf(prefix, sizeof(prefix), "wl%d_", band);

	//CountryRegion
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	if (str && strlen(str))
	{
		int region;
		region = getCountryRegion2G(str);
		fprintf(fp, "CountryRegion=%d\n", region);
		ChannelNumMax_2G = getChannelNumMax2G(region);
	}
	else
	{
		warning = 1;
		fprintf(fp, "CountryRegion=%d\n", 5);
	}

	//CountryRegion for A band
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	if (str && strlen(str))
	{
		int region;
		region = getCountryRegion5G(str, &warning);
		fprintf(fp, "CountryRegionABand=%d\n", region);
	}
	else
	{
		warning = 3;
		fprintf(fp, "CountryRegionABand=%d\n", 7);
	}

	//CountryCode
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	if (str && strlen(str))
	{
		fprintf(fp, "CountryCode=%s\n", str);
	}
	else
	{
		warning = 4;
		fprintf(fp, "CountryCode=DB\n");
	}

	//SSID Num. [MSSID Only]



	if (!mssid_mac_validate(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp))))
	{
		dbG("Main BSSID is not multiple of 4s!");
		ssid_num = 1;
	}
	else
	for (i = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d.%d_", band, i);
		else
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d_", band);

		if ((!i) ||
			(i && (nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))))
		{
			if (nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "wpa"))
			{
				ssid_num = 1;
				break;
			}
			else if (nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "wpa2"))
			{
				ssid_num = 1;
				break;
			}
			else if (nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "wpawpa2"))
			{
				ssid_num = 1;
				break;
			}
			else if (nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "radius"))
			{
				ssid_num = 1;
				break;
			}

			if (i) ssid_num++;
		}
	}

	if ((ssid_num < 1) || (ssid_num > MAX_NO_MSSID))
	{
		warning = 0;
		ssid_num = 1;
	}

	fprintf(fp, "BssidNum=%d\n", ssid_num);

	//SSID
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);
			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		if (strlen(nvram_safe_get(strcat_r(prefix_mssid, "ssid", temp))))
			sprintf(tmpstr, "SSID%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "ssid", temp)));
		else
		{
			warning = 5;
			sprintf(tmpstr, "SSID%d=%s%d\n", j + 1, "ASUS", j + 1);
		}
		fprintf(fp, "%s", tmpstr);
	}

	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "SSID%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}
/*
	str = nvram_safe_get(strcat_r(prefix, "ssid", tmp));
	if (str && strlen(str))
		fprintf(fp, "SSID1=%s\n", str);
	else
	{
		warning = 5;
		fprintf(fp, "SSID1=%s\n", "default");
	}

	fprintf(fp, "SSID2=\n");
	fprintf(fp, "SSID3=\n");
	fprintf(fp, "SSID4=\n");
	fprintf(fp, "SSID5=\n");
	fprintf(fp, "SSID6=\n");
	fprintf(fp, "SSID7=\n");
	fprintf(fp, "SSID8=\n");
*/

	//Network Mode
	str = nvram_safe_get(strcat_r(prefix, "nmode_x", tmp));
	if (band)
	{
		if (str && strlen(str))
		{
			if (atoi(str) == 0)       // Auto
			{
#if defined(RTAC52U)
				fprintf(fp, "WirelessMode=%d\n", 14);	// A + AN + AC mixed
				VHTBW_MAX = 1;
#else
				fprintf(fp, "WirelessMode=%d\n", 8);	// A + AN mixed
#endif
			}
			else if (atoi(str) == 1)  // N Only or N + AC
			{
#if defined(RTAC52U)
				fprintf(fp, "WirelessMode=%d\n", 15);	// AN + AC mixed
				VHTBW_MAX = 1;
#else
				fprintf(fp, "WirelessMode=%d\n", 11);	// N in 5G
#endif
			}
			else if (atoi(str) == 2)  // A
				fprintf(fp, "WirelessMode=%d\n", 2);
			else			// A,N
			{
#if defined(RTAC52U)
				fprintf(fp, "WirelessMode=%d\n", 14);
				VHTBW_MAX = 1;
#else
				fprintf(fp, "WirelessMode=%d\n", 8);
#endif
			}
		}
		else
		{
			warning = 6;
#if defined(RTAC52U)
			fprintf(fp, "WirelessMode=%d\n", 14);		// A + AN + AC mixed
			VHTBW_MAX = 1;
#else
			fprintf(fp, "WirelessMode=%d\n", 8);		// A + AN mixed
#endif
		}
	}
	else
	{
		if (str && strlen(str))
		{
			if (atoi(str) == 0)       // B,G,N
				fprintf(fp, "WirelessMode=%d\n", 9);
			else if (atoi(str) == 2)  // B,G
				fprintf(fp, "WirelessMode=%d\n", 0);
			else if (atoi(str) == 1)  // N
				fprintf(fp, "WirelessMode=%d\n", 6);
/*
			else if (atoi(str) == 4)  // G
				fprintf(fp, "WirelessMode=%d\n", 4);
			else if (atoi(str) == 0)  // B
				fprintf(fp, "WirelessMode=%d\n", 1);
*/
			else			// B,G,N
				fprintf(fp, "WirelessMode=%d\n", 9);
		}
		else
		{
			warning = 7;
			fprintf(fp, "WirelessMode=%d\n", 9);
		}
	}

	fprintf(fp, "FixedTxMode=\n");
/*
	fprintf(fp, "TxRate=%d\n", 0);
*/
	if (ssid_num == 1)
		fprintf(fp, "TxRate=%d\n", 0);
	else
	{
		memset(tmpstr, 0x0, sizeof(tmpstr));

		for (i = 0; i < ssid_num; i++)
		{
			if (i)
				sprintf(tmpstr, "%s;", tmpstr);

			sprintf(tmpstr, "%s%s", tmpstr, "0");
		}
		fprintf(fp, "TxRate=%s\n", tmpstr);
	}

	//Channel
	str = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
//	if (nvram_invmatch("sw_mode", "2") && nvram_invmatch(strcat_r(prefix, "channel", tmp), "0"))
	{
		str = nvram_safe_get(strcat_r(prefix, "channel", tmp));

		if (str && strlen(str))
			fprintf(fp, "Channel=%d\n", atoi(str));
		else
		{
			warning = 8;
			fprintf(fp, "Channel=%d\n", 0);
		}
	}

	//BasicRate
	if (!band)
	{
	/*
 	 * not supported in 5G mode
	 */
		str = nvram_safe_get(strcat_r(prefix, "rateset", tmp));
		if (str && strlen(str))
		{
			if (!strcmp(str, "default"))	// 1, 2, 5.5, 11
				fprintf(fp, "BasicRate=%d\n", 15);
			else if (!strcmp(str, "all"))	// 1, 2, 5.5, 6, 11, 12, 24
				fprintf(fp, "BasicRate=%d\n", 351);
			else if (!strcmp(str, "12"))	// 1, 2
				fprintf(fp, "BasicRate=%d\n", 3);
			else
				fprintf(fp, "BasicRate=%d\n", 15);
		}
		else
		{
			warning = 9;
			fprintf(fp, "BasicRate=%d\n", 15);
		}
	}

	//BeaconPeriod
	str = nvram_safe_get(strcat_r(prefix, "bcn", tmp));
	if (str && strlen(str))
	{
		if (atoi(str) > 1000 || atoi(str) < 20)
		{
			nvram_set(strcat_r(prefix, "bcn", tmp), "100");
			fprintf(fp, "BeaconPeriod=%d\n", 100);
		}
		else
			fprintf(fp, "BeaconPeriod=%d\n", atoi(str));
	}
	else
	{
		warning = 10;
		fprintf(fp, "BeaconPeriod=%d\n", 100);
	}

	//DTIM Period
	str = nvram_safe_get(strcat_r(prefix, "dtim", tmp));
	if (str && strlen(str))
		fprintf(fp, "DtimPeriod=%d\n", atoi(str));
	else
	{
		warning = 11;
		fprintf(fp, "DtimPeriod=%d\n", 1);
	}

	//TxPower
	str = nvram_safe_get(strcat_r(prefix, "TxPower", tmp));
	if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
		fprintf(fp, "TxPower=%d\n", 0);
	else if (str && strlen(str))
		fprintf(fp, "TxPower=%d\n", atoi(str));
	else
	{
		warning = 12;
		fprintf(fp, "TxPower=%d\n", 100);
	}

	//DisableOLBC
	fprintf(fp, "DisableOLBC=%d\n", 0);

	//BGProtection
	str = nvram_safe_get(strcat_r(prefix, "gmode_protection", tmp));
	if (str && strlen(str))
	{
		if (!strcmp(str, "auto"))
			fprintf(fp, "BGProtection=%d\n", 0);
		else
			fprintf(fp, "BGProtection=%d\n", 2);
	}
	else
	{
		warning = 13;
		fprintf(fp, "BGProtection=%d\n", 0);
	}

	//TxAntenna
	fprintf(fp, "TxAntenna=\n");

	//RxAntenna
	fprintf(fp, "RxAntenna=\n");

	//TxPreamble
	str = nvram_safe_get(strcat_r(prefix, "plcphdr", tmp));
	if (str && strcmp(str, "long") == 0)
		fprintf(fp, "TxPreamble=%d\n", 0);
	else if (str && strcmp(str, "short") == 0)
		fprintf(fp, "TxPreamble=%d\n", 1);
	else
		fprintf(fp, "TxPreamble=%d\n", 2);

	//RTSThreshold  Default=2347
	str = nvram_safe_get(strcat_r(prefix, "rts", tmp));
	if (str && strlen(str))
		fprintf(fp, "RTSThreshold=%d\n", atoi(str));
	else
	{
		warning = 14;
		fprintf(fp, "RTSThreshold=%d\n", 2347);
	}

	//FragThreshold  Default=2346
	str = nvram_safe_get(strcat_r(prefix, "frag", tmp));
	if (str && strlen(str))
		fprintf(fp, "FragThreshold=%d\n", atoi(str));
	else
	{
		warning = 15;
		fprintf(fp, "FragThreshold=%d\n", 2346);
	}

	//TxBurst
	str = nvram_safe_get(strcat_r(prefix, "frameburst", tmp));
	if (str && strlen(str))
//		fprintf(fp, "TxBurst=%d\n", atoi(str));
		fprintf(fp, "TxBurst=%d\n", strcmp(str, "off") ? 1 : 0);
	else
	{
		warning = 16;
		fprintf(fp, "TxBurst=%d\n", 1);
	}

	//PktAggregate
	str = nvram_safe_get(strcat_r(prefix, "PktAggregate", tmp));
	if (str && strlen(str))
		fprintf(fp, "PktAggregate=%d\n", atoi(str));
	else
	{
		warning = 17;
		fprintf(fp, "PktAggregate=%d\n", 1);
	}

	fprintf(fp, "FreqDelta=%d\n", 0);
	fprintf(fp, "TurboRate=%d\n", 0);

	//WmmCapable
	memset(tmpstr, 0x0, sizeof(tmpstr));
	memset(wmm_enable, 0x0, sizeof(wmm_enable));

	str = nvram_safe_get(strcat_r(prefix, "nmode_x", tmp));
	if (str && atoi(str) == 1)	// always enable WMM in N only mode
		sprintf(wmm_enable+strlen(wmm_enable), "%d", 1);
	else
		sprintf(wmm_enable+strlen(wmm_enable), "%d", strcmp(nvram_safe_get(strcat_r(prefix, "wme", tmp)), "off") ? 1 : 0);

	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%s", tmpstr, wmm_enable);
	}
	fprintf(fp, "WmmCapable=%s\n", tmpstr);
/*
	bzero(wmm_enable, sizeof(char)*8);
//	for (i = 0; i < ssid_num; i++)
	{
		str = nvram_safe_get(strcat_r(prefix, "nmode_x", tmp));
		if (str && atoi(str) == 1)	// always enable WMM in N only mode
			sprintf(wmm_enable+strlen(wmm_enable), "%d", 1);
		else
			sprintf(wmm_enable+strlen(wmm_enable), "%d", strcmp(nvram_safe_get(strcat_r(prefix, "wme", tmp)), "off") ? 1 : 0);
//		sprintf(wmm_enable+strlen(wmm_enable), "%c", ';');
	}
//	wmm_enable[strlen(wmm_enable) - 1] = '\0';
	wmm_enable[1] = '\0';
	fprintf(fp, "WmmCapable=%s\n", wmm_enable);
*/

	fprintf(fp, "APAifsn=3;7;1;1\n");
	fprintf(fp, "APCwmin=4;4;3;2\n");
	fprintf(fp, "APCwmax=6;10;4;3\n");
	fprintf(fp, "APTxop=0;0;94;47\n");
	fprintf(fp, "APACM=0;0;0;0\n");
	fprintf(fp, "BSSAifsn=3;7;2;2\n");
	fprintf(fp, "BSSCwmin=4;4;3;2\n");
	fprintf(fp, "BSSCwmax=10;10;4;3\n");
	fprintf(fp, "BSSTxop=0;0;94;47\n");
	fprintf(fp, "BSSACM=0;0;0;0\n");

	//AckPolicy
	bzero(wmm_noack, sizeof(char)*8);
	for (i = 0; i < 4; i++)
	{
		sprintf(wmm_noack+strlen(wmm_noack), "%d", strcmp(nvram_safe_get(strcat_r(prefix, "wme_no_ack", tmp)), "on") ? 0 : 1);
		sprintf(wmm_noack+strlen(wmm_noack), "%c", ';');
	}
	wmm_noack[strlen(wmm_noack) - 1] = '\0';
	fprintf(fp, "AckPolicy=%s\n", wmm_noack);

	snprintf(prefix, sizeof(prefix), "wl%d_", band);
//	snprintf(prefix2, sizeof(prefix2), "wl%d_", band);

	//APSDCapable
	str = nvram_safe_get(strcat_r(prefix, "wme_apsd", tmp));
	if (str && strlen(str))
		fprintf(fp, "APSDCapable=%d\n", strcmp(str, "off") ? 1 : 0);
	else
	{
		warning = 18;
		fprintf(fp, "APSDCapable=%d\n", 1);
	}

	//DLSDCapable
	str = nvram_safe_get(strcat_r(prefix, "DLSCapable", tmp));
	if (str && strlen(str))
		fprintf(fp, "DLSCapable=%d\n", atoi(str));
	else
	{
		warning = 19;
		fprintf(fp, "DLSCapable=%d\n", 0);
	}

	//NoForwarding pre SSID & NoForwardingBTNBSSID
	memset(tmpstr, 0x0, sizeof(tmpstr));

	str = nvram_safe_get(strcat_r(prefix, "ap_isolate", tmp));
	if (str && strlen(str))
	{
		for (i = 0; i < ssid_num; i++)
		{
			if (i)
				sprintf(tmpstr, "%s;", tmpstr);

			sprintf(tmpstr, "%s%s", tmpstr, str);
		}
		fprintf(fp, "NoForwarding=%s\n", tmpstr);

		fprintf(fp, "NoForwardingBTNBSSID=%d\n", atoi(str));
	}
	else
	{
		warning = 20;

		for (i = 0; i < ssid_num; i++)
		{
			if (i)
				sprintf(tmpstr, "%s;", tmpstr);

			sprintf(tmpstr, "%s%s", tmpstr, "0");
		}
		fprintf(fp, "NoForwarding=%s\n", tmpstr);
		fprintf(fp, "NoForwardingBTNBSSID=%d\n", 0);
	}
/*
	str = nvram_safe_get(strcat_r(prefix, "ap_isolate", tmp));
	if (str && strlen(str))
	{
		fprintf(fp, "NoForwarding=%d\n", atoi(str));
		fprintf(fp, "NoForwardingBTNBSSID=%d\n", atoi(str));
	}
	else
	{
		warning = 20;
		fprintf(fp, "NoForwarding=%d\n", 0);
		fprintf(fp, "NoForwardingBTNBSSID=%d\n", 0);
	}
*/
	//HideSSID
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			sprintf(tmpstr, "%s;", tmpstr);
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		str = nvram_safe_get(strcat_r(prefix_mssid, "closed", temp));
		sprintf(tmpstr, "%s%s", tmpstr, str);
	}
	fprintf(fp, "HideSSID=%s\n", tmpstr);
/*
	fprintf(fp, "HideSSID=%s\n", nvram_safe_get(strcat_r(prefix, "closed", tmp)));
*/

	//ShortSlot
	fprintf(fp, "ShortSlot=%d\n", 1);

	//AutoChannelSelect
	{
		str = nvram_safe_get(strcat_r(prefix, "channel", tmp));
/*
		if (nvram_match("sw_mode", "2"))
			fprintf(fp, "AutoChannelSelect=%d\n", 1);
		else */if (str && strlen(str))
		{
			if (atoi(str) == 0)
			{
				fprintf(fp, "AutoChannelSelect=%d\n", 2);
				if(band && atoi(nvram_safe_get(strcat_r(prefix, "bw", tmp))) > 0)
					fprintf(fp, "AutoChannelSkipList=%d\n", 165); // skip 165 in A band when bw setting to 20/40Mhz or 40Mhz.
			}
			else
				fprintf(fp, "AutoChannelSelect=%d\n", 0);
		}
		else
		{
			warning = 21;
			fprintf(fp, "AutoChannelSelect=%d\n", 2);
		}
	}

	//IEEE8021X
	str = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (str && !strcmp(str, "radius") && ssid_num == 1)
		fprintf(fp, "IEEE8021X=%d\n", 1);
	else
	{
		memset(tmpstr, 0x0, sizeof(tmpstr));

		for (i = 0; i < ssid_num; i++)
		{
			if (i)
				sprintf(tmpstr, "%s;", tmpstr);

			sprintf(tmpstr, "%s%s", tmpstr, "0");
		}
		fprintf(fp, "IEEE8021X=%s\n", tmpstr);
	}
/*
	str = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (str && !strcmp(str, "radius"))
		fprintf(fp, "IEEE8021X=%d\n", 1);
	else
		fprintf(fp, "IEEE8021X=%d\n", 0);
*/
	if (str && !flag_8021x)
	{
		if (	!strcmp(str, "radius") ||
			!strcmp(str, "wpa") ||
			!strcmp(str, "wpa2") ||
			!strcmp(str, "wpawpa2")	)
			flag_8021x = 1;
	}

	fprintf(fp, "IEEE80211H=0\n");
#ifdef RTCONFIG_AP_CARRIER_DETECTION
	if (nvram_match(strcat_r(prefix, "country_code", tmp), "JP"))
	{
		fprintf(fp, "RDRegion=%s\n", "JAP");
		fprintf(fp, "CarrierDetect=%d\n", 1);
	}
	else
#endif
	{
		fprintf(fp, "RDRegion=\n");
		fprintf(fp, "CarrierDetect=%d\n", 0);
	}
//	if (band)
//	fprintf(fp, "ChannelGeography=%d\n", 2);
	fprintf(fp, "PreAntSwitch=\n");
	fprintf(fp, "PhyRateLimit=%d\n", 0);
	fprintf(fp, "DebugFlags=%d\n", 0);
	fprintf(fp, "FineAGC=%d\n", 0);
	if(band)
		fprintf(fp, "StreamMode=%d\n", 3);	// from RT3883_EVT.TXT for test 5G in factory. But the meaning of value is unknown.
	else
	fprintf(fp, "StreamMode=%d\n", 0);
	fprintf(fp, "StreamModeMac0=\n");
	fprintf(fp, "StreamModeMac1=\n");
	fprintf(fp, "StreamModeMac2=\n");
	fprintf(fp, "StreamModeMac3=\n");
	fprintf(fp, "StationKeepAlive=%d\n", 0);
	fprintf(fp, "CSPeriod=10\n");
	fprintf(fp, "DfsLowerLimit=%d\n", 0);
	fprintf(fp, "DfsUpperLimit=%d\n", 0);
	fprintf(fp, "DfsIndoor=%d\n", 0);
	fprintf(fp, "DFSParamFromConfig=%d\n", 0);
	fprintf(fp, "FCCParamCh0=\n");
	fprintf(fp, "FCCParamCh1=\n");
	fprintf(fp, "FCCParamCh2=\n");
	fprintf(fp, "FCCParamCh3=\n");
	fprintf(fp, "CEParamCh0=\n");
	fprintf(fp, "CEParamCh1=\n");
	fprintf(fp, "CEParamCh2=\n");
	fprintf(fp, "CEParamCh3=\n");
	fprintf(fp, "JAPParamCh0=\n");
	fprintf(fp, "JAPParamCh1=\n");
	fprintf(fp, "JAPParamCh2=\n");
	fprintf(fp, "JAPParamCh3=\n");
	fprintf(fp, "JAPW53ParamCh0=\n");
	fprintf(fp, "JAPW53ParamCh1=\n");
	fprintf(fp, "JAPW53ParamCh2=\n");
	fprintf(fp, "JAPW53ParamCh3=\n");
	fprintf(fp, "FixDfsLimit=%d\n", 0);
	fprintf(fp, "LongPulseRadarTh=%d\n", 0);
	fprintf(fp, "AvgRssiReq=%d\n", 0);
	fprintf(fp, "DFS_R66=%d\n", 0);
	fprintf(fp, "BlockCh=\n");

	//GreenAP
#if defined(RTN65U)
	{
		fprintf(fp, "GreenAP=%d\n", 1);
	}
#elif defined(RTN14U) || defined(RTAC52U)
	/// MT7620 GreenAP will impact TSSI, force to disable GreenAP here..
	//  MT7620 GreenAP cause bad site survey result on RTAC52 2G.
	{
		fprintf(fp, "GreenAP=%d\n", 0);
	}
#else
	str = nvram_safe_get(strcat_r(prefix, "GreenAP", tmp));
/*
	if (nvram_match("sw_mode", "2"))
		fprintf(fp, "GreenAP=%d\n", 0);
	else */if (str && strlen(str))
		fprintf(fp, "GreenAP=%d\n", atoi(str));
	else
	{
		warning = 22;
		fprintf(fp, "GreenAP=%d\n", 0);
	}
#endif

	//PreAuth
	memset(tmpstr, 0x0, sizeof(tmpstr));
	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d.%d_", band, i);
		else
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d_", band);

		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%s", tmpstr, strstr(nvram_safe_get(strcat_r(prefix_mssid, "auth_mode_x", temp)), "wpa") ? "1" : "0");
	}
	fprintf(fp, "PreAuth=%s\n", tmpstr);
/*
	fprintf(fp, "PreAuth=0\n");
*/

	//AuthMode
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			sprintf(tmpstr, "%s;", tmpstr);
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		str = nvram_safe_get(strcat_r(prefix_mssid, "auth_mode_x", temp));
		if (str && strlen(str))
		{
			if (!strcmp(str, "open"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "OPEN");
			}
			else if (!strcmp(str, "shared"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "SHARED");
			}
			else if (!strcmp(str, "psk"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPAPSK");
			}
			else if (!strcmp(str, "psk2"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPA2PSK");
			}
			else if (!strcmp(str, "pskpsk2"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPAPSKWPA2PSK");
			}
			else if (!strcmp(str, "wpa"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPA");
				break;
			}
			else if (!strcmp(str, "wpa2"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPA2");
				break;
			}
			else if (!strcmp(str, "wpawpa2"))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "WPA1WPA2");
				break;
			}
			else if ((!strcmp(str, "radius")))
			{
				sprintf(tmpstr, "%s%s", tmpstr, "OPEN");
				break;
			}
			else
			{
				warning = 23;
				sprintf(tmpstr, "%s%s", tmpstr, "OPEN");
			}
		}
		else
		{
			warning = 24;
			sprintf(tmpstr, "%s%s", tmpstr, "OPEN");
		}
	}
	fprintf(fp, "AuthMode=%s\n", tmpstr);
/*
	str = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (str && strlen(str))
	{
		if (!strcmp(str, "open"))
		{
			fprintf(fp, "AuthMode=%s\n", "OPEN");
		}
		else if (!strcmp(str, "shared"))
		{
			fprintf(fp, "AuthMode=%s\n", "SHARED");
		}
		else if (!strcmp(str, "psk"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPAPSK");
		}
		else if (!strcmp(str, "psk2"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPA2PSK");
		}
		else if (!strcmp(str, "pskpsk2"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPAPSKWPA2PSK");
		}
		else if (!strcmp(str, "wpa"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPA");
		}
		else if (!strcmp(str, "wpa2"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPA2");
		}
		else if (!strcmp(str, "wpawpa2"))
		{
			fprintf(fp, "AuthMode=%s\n", "WPA1WPA2");
		}
		else if (!strcmp(str, "radius"))
		{
			fprintf(fp, "AuthMode=%s\n", "OPEN");
		}
		else
		{
			fprintf(fp, "AuthMode=%s\n", "OPEN");
		}
	}
	else
	{
		warning = 24;
		fprintf(fp, "AuthMode=%s\n", "OPEN");
	}
*/

	//EncrypType
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			sprintf(tmpstr, "%s;", tmpstr);
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
			&& nvram_match(strcat_r(prefix_mssid, "wep_x", temp), "0")))
			sprintf(tmpstr, "%s%s", tmpstr, "NONE");
		else if ((nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "open")
			&& nvram_invmatch(strcat_r(prefix_mssid, "wep_x", temp), "0")) ||
				nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "shared") ||
				nvram_match(strcat_r(prefix_mssid, "auth_mode_x", temp), "radius"))
			sprintf(tmpstr, "%s%s", tmpstr, "WEP");
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip"))
		{
			sprintf(tmpstr, "%s%s", tmpstr, "TKIP");
		}
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "aes"))
		{
			sprintf(tmpstr, "%s%s", tmpstr, "AES");
		}
		else if (nvram_match(strcat_r(prefix_mssid, "crypto", temp), "tkip+aes"))
		{
			sprintf(tmpstr, "%s%s", tmpstr, "TKIPAES");
		}
		else
		{
			warning = 25;
			sprintf(tmpstr, "%s%s", tmpstr, "NONE");
		}
	}
	fprintf(fp, "EncrypType=%s\n", tmpstr);
#if 0
	if (	(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_match(strcat_r(prefix, "wep_x", tmp), "0")) /*||
		(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius") && nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))*/
	)
		fprintf(fp, "EncrypType=%s\n", "NONE");
	else if (       (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_invmatch(strcat_r(prefix, "wep_x", tmp), "0")) ||
			nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared") ||
			nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius")/* ||
			(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius") && nvram_invmatch(strcat_r(prefix, "wep_x", tmp), "0"))*/
	)
		fprintf(fp, "EncrypType=%s\n", "WEP");
	else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
		fprintf(fp, "EncrypType=%s\n", "TKIP");
	else if (nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
		fprintf(fp, "EncrypType=%s\n", "AES");
	else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip+aes"))
		fprintf(fp, "EncrypType=%s\n", "TKIPAES");
	else
		fprintf(fp, "EncrypType=%s\n", "NONE");
#endif

	fprintf(fp, "WapiPsk1=\n");
	fprintf(fp, "WapiPsk2=\n");
	fprintf(fp, "WapiPsk3=\n");
	fprintf(fp, "WapiPsk4=\n");
	fprintf(fp, "WapiPsk5=\n");
	fprintf(fp, "WapiPsk6=\n");
	fprintf(fp, "WapiPsk7=\n");
	fprintf(fp, "WapiPsk8=\n");
	fprintf(fp, "WapiPskType=\n");
	fprintf(fp, "Wapiifname=\n");
	fprintf(fp, "WapiAsCertPath=\n");
	fprintf(fp, "WapiUserCertPath=\n");
	fprintf(fp, "WapiAsIpAddr=\n");
	fprintf(fp, "WapiAsPort=\n");

	//RekeyInterval
	str = nvram_safe_get(strcat_r(prefix, "wpa_gtk_rekey", tmp));
	if (str && strlen(str))
	{
		if (atol(str) == 0)
			fprintf(fp, "RekeyMethod=%s\n", "DISABLE");
		else
			fprintf(fp, "RekeyMethod=TIME\n");

		fprintf(fp, "RekeyInterval=%ld\n", atol(str));
	}
	else
	{
		warning = 26;
		fprintf(fp, "RekeyMethod=%s\n", "DISABLE");
		fprintf(fp, "RekeyInterval=%d\n", 0);
	}

	//PMKCachePeriod (in minutes)
	str = nvram_safe_get(strcat_r(prefix, "pmk_cache", tmp));
	if (str && strlen(str))
		fprintf(fp, "PMKCachePeriod=%ld\n", atol(str));
	else
	fprintf(fp, "PMKCachePeriod=%d\n", 10);

	fprintf(fp, "MeshAutoLink=%d\n", 0);
	fprintf(fp, "MeshAuthMode=\n");
	fprintf(fp, "MeshEncrypType=\n");
	fprintf(fp, "MeshDefaultkey=%d\n", 0);
	fprintf(fp, "MeshWEPKEY=\n");
	fprintf(fp, "MeshWPAKEY=\n");
	fprintf(fp, "MeshId=\n");

	//WPAPSK
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		sprintf(tmpstr, "WPAPSK%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "wpa_psk", temp)));
		fprintf(fp, "%s", tmpstr);
	}
	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "WPAPSK%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}
/*
	fprintf(fp, "WPAPSK1=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));

	fprintf(fp, "WPAPSK2=\n");
	fprintf(fp, "WPAPSK3=\n");
	fprintf(fp, "WPAPSK4=\n");
	fprintf(fp, "WPAPSK5=\n");
	fprintf(fp, "WPAPSK6=\n");
	fprintf(fp, "WPAPSK7=\n");
	fprintf(fp, "WPAPSK8=\n");
*/

	//DefaultKeyID

	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			sprintf(tmpstr, "%s;", tmpstr);
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		str = nvram_safe_get(strcat_r(prefix_mssid, "key", temp));
		sprintf(tmpstr, "%s%s", tmpstr, str);
	}
	fprintf(fp, "DefaultKeyID=%s\n", tmpstr);

	list[0] = 0;
	list[1] = 0;
	memset(wl_key_type, 0x0, sizeof(wl_key_type));

	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d.%d_", band, i);
		else
			snprintf(prefix_mssid, sizeof(prefix_mssid), "wl%d_", band);

		if ((!i) ||
			(i && (nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))))
		{
			str = strcat_r(prefix_mssid, "key", temp);
			str2 = nvram_safe_get(str);
			sprintf(list, "%s%s", str, str2);

			if ((strlen(nvram_safe_get(list)) == 5) || (strlen(nvram_safe_get(list)) == 13))
			{
				wl_key_type[j] = 1;
				warning = 271;
			}
			else if ((strlen(nvram_safe_get(list)) == 10) || (strlen(nvram_safe_get(list)) == 26))
			{
				wl_key_type[j] = 0;
				warning = 272;
			}
			else if ((strlen(nvram_safe_get(list)) != 0))
			{
				warning = 273;
			}

			j++;
		}
	}

	// Key1Type(0 -> Hex, 1->Ascii)
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%d", tmpstr, wl_key_type[i]);
	}
	fprintf(fp, "Key1Type=%s\n", tmpstr);

	// Key1Str
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		sprintf(tmpstr, "Key1Str%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "key1", temp)));
		fprintf(fp, "%s", tmpstr);
	}
	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "Key1Str%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}

	// Key2Type
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%d", tmpstr, wl_key_type[i]);
	}
	fprintf(fp, "Key2Type=%s\n", tmpstr);

	// Key2Str
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		sprintf(tmpstr, "Key2Str%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "key2", temp)));
		fprintf(fp, "%s", tmpstr);
	}
	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "Key2Str%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}

	// Key3Type
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%d", tmpstr, wl_key_type[i]);
	}
	fprintf(fp, "Key3Type=%s\n", tmpstr);

	// Key3Str
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		sprintf(tmpstr, "Key3Str%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "key3", temp)));
		fprintf(fp, "%s", tmpstr);
	}
	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "Key3Str%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}

	// Key4Type
	memset(tmpstr, 0x0, sizeof(tmpstr));

	for (i = 0; i < ssid_num; i++)
	{
		if (i)
			sprintf(tmpstr, "%s;", tmpstr);

		sprintf(tmpstr, "%s%d", tmpstr, wl_key_type[i]);
	}
	fprintf(fp, "Key4Type=%s\n", tmpstr);

	// Key4Str
	for (i = 0, j = 0; i < MAX_NO_MSSID; i++)
	{
		if (i)
		{
			sprintf(prefix_mssid, "wl%d.%d_", band, i);

			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;
			else
				j++;
		}
		else
			sprintf(prefix_mssid, "wl%d_", band);

		sprintf(tmpstr, "Key4Str%d=%s\n", j + 1, nvram_safe_get(strcat_r(prefix_mssid, "key4", temp)));
		fprintf(fp, "%s", tmpstr);
	}
	for (i = ssid_num; i < 8; i++)
	{
		sprintf(tmpstr, "Key4Str%d=\n", i + 1);
		fprintf(fp, "%s", tmpstr);
	}

#if 0
	fprintf(fp, "DefaultKeyID=%s\n", nvram_safe_get(strcat_r(prefix, "key", tmp)));

	list[0] = 0;
	list[1] = 0;
//	sprintf(list, "wl%d_key%s", band, nvram_safe_get(strcat_r(prefix, "key", tmp)));
	sprintf(list, "%s%s", strcat_r(prefix, "key", tmp), nvram_safe_get(strcat_r(prefix2, "key", tmp2)));

	if ((strlen(nvram_safe_get(list)) == 5) || (strlen(nvram_safe_get(list)) == 13))
	{
		nvram_set(strcat_r(prefix, "key_type", tmp), "1");
		warning = 271;
	}
	else if ((strlen(nvram_safe_get(list)) == 10) || (strlen(nvram_safe_get(list)) == 26))
	{
		nvram_set(strcat_r(prefix, "key_type", tmp), "0");
		warning = 272;
	}
	else if ((strlen(nvram_safe_get(list)) != 0))
		warning = 273;

	//Key1Type(0 -> Hex, 1->Ascii)
	fprintf(fp, "Key1Type=%s\n", nvram_safe_get(strcat_r(prefix, "key_type", tmp)));
	//Key1Str
	fprintf(fp, "Key1Str1=%s\n", nvram_safe_get(strcat_r(prefix, "key1", tmp)));
	fprintf(fp, "Key1Str2=\n");
	fprintf(fp, "Key1Str3=\n");
	fprintf(fp, "Key1Str4=\n");
	fprintf(fp, "Key1Str5=\n");
	fprintf(fp, "Key1Str6=\n");
	fprintf(fp, "Key1Str7=\n");
	fprintf(fp, "Key1Str8=\n");

	//Key2Type
	fprintf(fp, "Key2Type=%s\n", nvram_safe_get(strcat_r(prefix, "key_type", tmp)));
	//Key2Str
	fprintf(fp, "Key2Str1=%s\n", nvram_safe_get(strcat_r(prefix, "key2", tmp)));
	fprintf(fp, "Key2Str2=\n");
	fprintf(fp, "Key2Str3=\n");
	fprintf(fp, "Key2Str4=\n");
	fprintf(fp, "Key2Str5=\n");
	fprintf(fp, "Key2Str6=\n");
	fprintf(fp, "Key2Str7=\n");
	fprintf(fp, "Key2Str8=\n");

	//Key3Type
	fprintf(fp, "Key3Type=%s\n", nvram_safe_get(strcat_r(prefix, "key_type", tmp)));
	//Key3Str
	fprintf(fp, "Key3Str1=%s\n", nvram_safe_get(strcat_r(prefix, "key3", tmp)));
	fprintf(fp, "Key3Str2=\n");
	fprintf(fp, "Key3Str3=\n");
	fprintf(fp, "Key3Str4=\n");
	fprintf(fp, "Key3Str5=\n");
	fprintf(fp, "Key3Str6=\n");
	fprintf(fp, "Key3Str7=\n");
	fprintf(fp, "Key3Str8=\n");

	//Key4Type
	fprintf(fp, "Key4Type=%s\n", nvram_safe_get(strcat_r(prefix, "key_type", tmp)));
	//Key4Str
	fprintf(fp, "Key4Str1=%s\n", nvram_safe_get(strcat_r(prefix, "key4", tmp)));
	fprintf(fp, "Key4Str2=\n");
	fprintf(fp, "Key4Str3=\n");
	fprintf(fp, "Key4Str4=\n");
	fprintf(fp, "Key4Str5=\n");
	fprintf(fp, "Key4Str6=\n");
	fprintf(fp, "Key4Str7=\n");
	fprintf(fp, "Key4Str8=\n");
#endif

/*
	fprintf(fp, "SecurityMode=%d\n", 0);
	fprintf(fp, "VLANEnable=%d\n", 0);
	fprintf(fp, "VLANName=\n");
	fprintf(fp, "VLANID=%d\n", 0);
	fprintf(fp, "VLANPriority=%d\n", 0);
*/
	fprintf(fp, "HSCounter=%d\n", 0);

	//HT_HTC
	str = nvram_safe_get(strcat_r(prefix, "HT_HTC", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_HTC=%d\n", atoi(str));
	else
	{
		warning = 28;
		fprintf(fp, "HT_HTC=%d\n", 1);
	}

	//HT_RDG
	str = nvram_safe_get(strcat_r(prefix, "HT_RDG", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_RDG=%d\n", atoi(str));
	else
	{
		warning = 29;
		fprintf(fp, "HT_RDG=%d\n", 0);
	}

	//HT_LinkAdapt
	str = nvram_safe_get(strcat_r(prefix, "HT_LinkAdapt", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_LinkAdapt=%d\n", atoi(str));
	else
	{
		warning = 30;
		fprintf(fp, "HT_LinkAdapt=%d\n", 1);
	}

	//HT_OpMode
//	str = nvram_safe_get(strcat_r(prefix, "HT_OpMode", tmp));
	str = nvram_safe_get(strcat_r(prefix, "mimo_preamble", tmp));
	int opmode = strcmp(str, "mm") ? 1 : 0;
	if (str && strlen(str))
//		fprintf(fp, "HT_OpMode=%d\n", atoi(str));
		fprintf(fp, "HT_OpMode=%d\n", opmode);
	else
	{
		warning = 31;
		fprintf(fp, "HT_OpMode=%d\n", 0);
	}

	//HT_MpduDensity
	str = nvram_safe_get(strcat_r(prefix, "HT_MpduDensity", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_MpduDensity=%d\n", atoi(str));
	else
	{
		warning = 32;
		fprintf(fp, "HT_MpduDensity=%d\n", 5);
	}

	int Channel = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	int EXTCHA = 0;
	int EXTCHA_MAX = 0;
	int HTBW_MAX = 1;

	if (band)
	{
		if (Channel != 0)
		{
			if ((Channel == 36) || (Channel == 44) || (Channel == 52) || (Channel == 60) || (Channel == 100) || (Channel == 108) ||
			    (Channel == 116) || (Channel == 124) || (Channel == 132) || (Channel == 149) || (Channel == 157))
			{
				EXTCHA = 1;
			}
			else if ((Channel == 40) || (Channel == 48) || (Channel == 56) || (Channel == 64) || (Channel == 104) || (Channel == 112) ||
					(Channel == 120) || (Channel == 128) || (Channel == 136) || (Channel == 153) || (Channel == 161))
			{
				EXTCHA = 0;
			}
			else
			{
				HTBW_MAX = 0;
			}
		}
	}
	else
	{
		if (Channel == 0)
			EXTCHA_MAX = 1;
		else if ((Channel >=1) && (Channel <= 4))
			EXTCHA_MAX = 1;
		else if ((Channel >= 5) && (Channel <= 7))
			EXTCHA_MAX = 1;
		else if ((Channel >= 8) && (Channel <= 14))
		{
			if ((ChannelNumMax_2G - Channel) < 4)
				EXTCHA_MAX = 0;
			else
				EXTCHA_MAX = 1;
		}
		else
			HTBW_MAX = 0;
	}

	//HT_EXTCHA
	if (band)
	{
		fprintf(fp, "HT_EXTCHA=%d\n", EXTCHA);
	}
	else
	{
//		str = nvram_safe_get(strcat_r(prefix, "HT_EXTCHA", tmp));
		str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));
		int extcha = strcmp(str, "lower") ? 0 : 1;
		if (str && strlen(str))
		{
			if ((Channel >=1) && (Channel <= 4))
				fprintf(fp, "HT_EXTCHA=%d\n", 1);	
//			else if (atoi(str) <= EXTCHA_MAX)
			else if (extcha <= EXTCHA_MAX)
//				fprintf(fp, "HT_EXTCHA=%d\n", atoi(str));
				fprintf(fp, "HT_EXTCHA=%d\n", extcha);
			else
				fprintf(fp, "HT_EXTCHA=%d\n", 0);
		}
		else
		{
			warning = 33;
			fprintf(fp, "HT_EXTCHA=%d\n", 0);
		}
	}

	//HT_BW
	str = nvram_safe_get(strcat_r(prefix, "bw", tmp));
/*
	if (nvram_match("sw_mode", "2"))
		fprintf(fp, "HT_BW=%d\n", 1);
	else */if ((atoi(str) > 0) && (HTBW_MAX == 1))
		fprintf(fp, "HT_BW=%d\n", 1);
	else
	{
//		warning = 34;
		fprintf(fp, "HT_BW=%d\n", 0);
	}

	//HT_BSSCoexistence
	if(str && (strcmp(str, "1") == 0))
		fprintf(fp, "HT_BSSCoexistence=%d\n", 1);	// 20 Only and 20/40 coexistence support
	else
		fprintf(fp, "HT_BSSCoexistence=%d\n", 0);	// 40 Only


	//HT_AutoBA
	str = nvram_safe_get(strcat_r(prefix, "HT_AutoBA", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_AutoBA=%d\n", atoi(str));
	else
	{
		warning = 35;
		fprintf(fp, "HT_AutoBA=%d\n", 1);
	}

	//HT_BADecline
	str = nvram_safe_get(strcat_r(prefix, "HT_BADecline", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_BADecline=%d\n", atoi(str));
	else
	{
		warning = 36;
		fprintf(fp, "HT_BADecline=%d\n", 0);
	}

	//HT_AMSDU
	str = nvram_safe_get(strcat_r(prefix, "HT_AMSDU", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_AMSDU=%d\n", atoi(str));
	else
	{
		warning = 37;
		fprintf(fp, "HT_AMSDU=%d\n", 0);
	}

	//HT_BAWinSize
	str = nvram_safe_get(strcat_r(prefix, "HT_BAWinSize", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_BAWinSize=%d\n", atoi(str));
	else
	{
		warning = 38;
		fprintf(fp, "HT_BAWinSize=%d\n", 64);
	}

	//HT_GI
	str = nvram_safe_get(strcat_r(prefix, "HT_GI", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_GI=%d\n", atoi(str));
	else
	{
		warning = 39;
		fprintf(fp, "HT_GI=%d\n", 1);
	}

	//HT_STBC
	str = nvram_safe_get(strcat_r(prefix, "HT_STBC", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_STBC=%d\n", atoi(str));
	else
	{
		warning = 40;
		fprintf(fp, "HT_STBC=%d\n", 1);
	}

	//HT_MCS
	str = nvram_safe_get(strcat_r(prefix, "HT_MCS", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_MCS=%d\n", atoi(str));
	else
	{
		warning = 41;
		fprintf(fp, "HT_MCS=%d\n", 33);
	}

	//HT_TxStream
	str = nvram_safe_get(strcat_r(prefix, "HT_TxStream", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_TxStream=%d\n", atoi(str));
	else
	{
		warning = 42;
		fprintf(fp, "HT_TxStream=%d\n", 2);
	}

	//HT_RxStream
	str = nvram_safe_get(strcat_r(prefix, "HT_RxStream", tmp));
	if (str && strlen(str))
		fprintf(fp, "HT_RxStream=%d\n", atoi(str));
	else
	{
		warning = 43;
		fprintf(fp, "HT_RxStream=%d\n", 3);
	}

	//HT_PROTECT
//	str = nvram_safe_get(strcat_r(prefix, "HT_PROTECT", tmp));
	str = nvram_safe_get(strcat_r(prefix, "nmode_protection", tmp));
	int nprotect = strcmp(str, "auto") ? 0 : 1;
	if (str && strlen(str))
//		fprintf(fp, "HT_PROTECT=%d\n", atoi(str));
		fprintf(fp, "HT_PROTECT=%d\n", nprotect);
	else
	{
		warning = 44;
		fprintf(fp, "HT_PROTECT=%d\n", 1);
	}

	//HT_DisallowTKIP
	fprintf(fp, "HT_DisallowTKIP=%d\n", 1);

#ifdef RTAC52U
	//VHT_BW, VHT_DisallowNonVHT
	str = nvram_safe_get(strcat_r(prefix, "bw", tmp));
	if(band != 1)
	{
	}
	else if(str && (strcmp(str, "1") == 0) && (HTBW_MAX == 1) && (VHTBW_MAX == 1))	// Auto
	{
		fprintf(fp, "VHT_BW=%d\n", 1);
		fprintf(fp, "VHT_DisallowNonVHT=%d\n", 0);
	}
	else if(str && (strcmp(str, "3") == 0) && (HTBW_MAX == 1) && (VHTBW_MAX == 1))	// 80 MHz Only
	{
		fprintf(fp, "VHT_BW=%d\n", 1);
		fprintf(fp, "VHT_DisallowNonVHT=%d\n", 1);
	}
	else
	{
		fprintf(fp, "VHT_BW=%d\n", 0);
		fprintf(fp, "VHT_DisallowNonVHT=%d\n", 0);
	}
#endif

	// TxBF
	if (band)
	{
		str = nvram_safe_get(strcat_r(prefix, "txbf", tmp));
		if ((atoi(str) > 0) && nvram_match(strcat_r(prefix, "txbf_en", tmp), "1"))
		{
			fprintf(fp, "ITxBfEn=%d\n", 1);
			fprintf(fp, "ETxBfEnCond=%d\n", 1);
		}
		else
		{
			fprintf(fp, "ITxBfEn=%d\n", 0);
			fprintf(fp, "ETxBfEnCond=%d\n", 0);
		}
	}

	//WscConfStatus
//	str = nvram_safe_get(strcat_r(prefix, "wsc_config_state", tmp));
//	if (str && strlen(str))
//		wsc_configure = atoi(str);
//	else
//	{
//		warning = 45;
//		wsc_configure = 0;
//	}

	str2 = nvram_safe_get("w_Setting");

	if (str2)
//		wsc_configure |= atoi(str2);
		wsc_configure = atoi(str2);

	if (wsc_configure == 0)
	{
		fprintf(fp, "WscConfMode=%d\n", 0);
		fprintf(fp, "WscConfStatus=%d\n", 1);
		g_wsc_configured = 0;						// AP is unconfigured
		nvram_set(strcat_r(prefix, "wsc_config_state", tmp), "0");
	}
	else
	{
		fprintf(fp, "WscConfMode=%d\n", 0);
		fprintf(fp, "WscConfStatus=%d\n", 2);
		g_wsc_configured = 1;						// AP is configured
		nvram_set(strcat_r(prefix, "wsc_config_state", tmp), "1");
	}

	fprintf(fp, "WscVendorPinCode=%s\n", nvram_safe_get("secret_code"));
//	fprintf(fp, "ApCliWscPinCode=%s\n", nvram_safe_get("secret_code"));	// removed from SDK 3.3.0.0

	//AccessPolicy0
	gen_macmode(mac_filter, band); //Ren
	str = nvram_safe_get(strcat_r(prefix, "macmode", tmp));

	if (str && strlen(str))
	{
		#if 1 //Ren.B
		for (i = 0; i < ssid_num; i++)
		{
			fprintf(fp, "AccessPolicy%d=%d\n", i, mac_filter[i]);
		}
		#else
		if (!strcmp(str, "disabled"))
		{
			for (i = 0; i < ssid_num; i++)
				fprintf(fp, "AccessPolicy%d=%d\n", i, 0);
		}
		else if (!strcmp(str, "allow"))
		{
			for (i = 0; i < ssid_num; i++)
				fprintf(fp, "AccessPolicy%d=%d\n", i, 1);
		}
		else if (!strcmp(str, "deny"))
		{
			for (i = 0; i < ssid_num; i++)
				fprintf(fp, "AccessPolicy%d=%d\n", i, 2);
		}
		else
		{
			warning = 46;
			for (i = 0; i < ssid_num; i++)
				fprintf(fp, "AccessPolicy%d=%d\n", i, 0);
		}
		#endif //Ren.E
	}
	else
	{
		warning = 47;
		for (i = 0; i < ssid_num; i++)
			fprintf(fp, "AccessPolicy%d=%d\n", i, 0);
	}

	list[0]=0;
	list[1]=0;
	if (nvram_invmatch(strcat_r(prefix, "macmode", tmp), "disabled"))
	{
		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b)==0) continue;
				if (list[0]==0)
					sprintf(list, "%s", b);
				else
					sprintf(list, "%s;%s", list, b);
			}
			free(nv);
		}
	}

	// AccessControlLis0~3
	for (i = 0; i < ssid_num; i++)
		if(mac_filter[i] != 0)
			fprintf(fp, "AccessControlList%d=%s\n", i, list);
		else
			fprintf(fp, "AccessControlList%d=%s\n", i, "");

	if (!nvram_match("sw_mode", "2") && !nvram_match(strcat_r(prefix, "mode_x", tmp), "0"))
	{
	//WdsEnable
	str = nvram_safe_get(strcat_r(prefix, "mode_x", tmp));
	if (str && strlen(str))
	{
		if (	(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") ||
			(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2") && nvram_match(strcat_r(prefix, "crypto", tmp), "aes")))
		)
		{
			if (atoi(str) == 0)
				fprintf(fp, "WdsEnable=%d\n", 0);
			else if (atoi(str) == 1)
				fprintf(fp, "WdsEnable=%d\n", 2);
			else if (atoi(str) == 2)
			{
				if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "0"))
					fprintf(fp, "WdsEnable=%d\n", 4);
				else
					fprintf(fp, "WdsEnable=%d\n", 3);
			}
		}
		else
			fprintf(fp, "WdsEnable=%d\n", 0);
	}
	else
	{
		warning = 49;
		fprintf(fp, "WdsEnable=%d\n", 0);
	}

	//WdsPhyMode
	str = nvram_safe_get(strcat_r(prefix, "mode_x", tmp));
	if (str && strlen(str))
	{
		{
			if ((atoi(nvram_safe_get(strcat_r(prefix, "nmode_x", tmp))) == 0))
				fprintf(fp, "WdsPhyMode=%s\n", "HTMIX");
			else if ((atoi(nvram_safe_get(strcat_r(prefix, "nmode_x", tmp))) == 1))
				fprintf(fp, "WdsPhyMode=%s\n", "GREENFIELD");
			else
				fprintf(fp, "WdsPhyMode=\n");
		}
	}

	//WdsEncrypType
	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
		fprintf(fp, "WdsEncrypType=%s\n", "NONE;NONE;NONE;NONE");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_invmatch(strcat_r(prefix, "wep_x", tmp), "0"))
		fprintf(fp, "WdsEncrypType=%s\n", "WEP;WEP;WEP;WEP");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2") && nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
		fprintf(fp, "WdsEncrypType=%s\n", "AES;AES;AES;AES");
	else
		fprintf(fp, "WdsEncrypType=%s\n", "NONE;NONE;NONE;NONE");

	list[0]=0;
	list[1]=0;
	if (	(nvram_match(strcat_r(prefix, "mode_x", tmp), "1") || (nvram_match(strcat_r(prefix, "mode_x", tmp), "2") && nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "1"))) &&
		(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") ||
		(nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2") && nvram_match(strcat_r(prefix, "crypto", tmp), "aes")))
	)
	{
#if 0
		num = atoi(nvram_safe_get(strcat_r(prefix, "wdsnum_x", tmp)));
		for (i=0;i<num;i++)
			sprintf(list, "%s;%s", list, mac_conv(strcat_r(prefix, "wdslist_x", tmp), i, macbuf));
#else
		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "wdslist", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b)==0) continue;
				if (list[0]==0)
					sprintf(list, "%s", b);
				else
					sprintf(list, "%s;%s", list, b);
			}
			free(nv);
		}
#endif
	}

	//WdsList
	fprintf(fp, "WdsList=%s\n", list);

	//WdsKey
	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_match(strcat_r(prefix, "wep_x", tmp), "0"))
	{
		fprintf(fp, "WdsDefaultKeyID=\n");
		fprintf(fp, "Wds0Key=\n");
		fprintf(fp, "Wds1Key=\n");
		fprintf(fp, "Wds2Key=\n");
		fprintf(fp, "Wds3Key=\n");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "open") && nvram_invmatch(strcat_r(prefix, "wep_x", tmp), "0"))
	{
		fprintf(fp, "WdsDefaultKeyID=%s;%s;%s;%s\n", nvram_safe_get(strcat_r(prefix, "key", tmp)), nvram_safe_get(strcat_r(prefix, "key", tmp)), nvram_safe_get(strcat_r(prefix, "key", tmp)), nvram_safe_get(strcat_r(prefix, "key", tmp)));
//		sprintf(list, "wl%d_key%s", band, nvram_safe_get(strcat_r(prefix, "key", tmp)));
		str = strcat_r(prefix, "key", tmp);
		str2 = nvram_safe_get(str);
		sprintf(list, "%s%s", str, str2);

		fprintf(fp, "Wds0Key=%s\n", nvram_safe_get(list));
		fprintf(fp, "Wds1Key=%s\n", nvram_safe_get(list));
		fprintf(fp, "Wds2Key=%s\n", nvram_safe_get(list));
		fprintf(fp, "Wds3Key=%s\n", nvram_safe_get(list));
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2") && nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
	{
		fprintf(fp, "WdsKey=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		fprintf(fp, "Wds0Key=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		fprintf(fp, "Wds1Key=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		fprintf(fp, "Wds2Key=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		fprintf(fp, "Wds3Key=%s\n", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
	}

	} // sw_mode

//	fprintf(fp, "WirelessEvent=%d\n", 0);

	//RADIUS_Server
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)), ""))
//		fprintf(fp, "RADIUS_Server=0;0;0;0;0;0;0;0\n");
		fprintf(fp, "RADIUS_Server=\n");
	else
//		fprintf(fp, "RADIUS_Server=%s;0;0;0;0;0;0;0\n", nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)));
		fprintf(fp, "RADIUS_Server=%s\n", nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)));

	//RADIUS_Port
	str = nvram_safe_get(strcat_r(prefix, "radius_port", tmp));
	if (str && strlen(str))
/*		
		fprintf(fp, "RADIUS_Port=%d;%d;%d;%d;%d;%d;%d;%d\n",	atoi(str),
									atoi(str),
									atoi(str),
									atoi(str),
									atoi(str),
									atoi(str),
									atoi(str),
									atoi(str));
*/
		fprintf(fp, "RADIUS_Port=%d\n",	atoi(str));
	else
	{
		warning = 50;
/*
		fprintf(fp, "RADIUS_Port=%d;%d;%d;%d;%d;%d;%d;%d\n", 1812, 1812, 1812, 1812, 1812, 1812, 1812, 1812);
*/
		fprintf(fp, "RADIUS_Port=%d\n", 1812);
	}

	//RADIUS_Key
	fprintf(fp, "RADIUS_Key1=%s\n", nvram_safe_get(strcat_r(prefix, "radius_key", tmp)));
	fprintf(fp, "RADIUS_Key2=\n");
	fprintf(fp, "RADIUS_Key3=\n");
	fprintf(fp, "RADIUS_Key4=\n");
	fprintf(fp, "RADIUS_Key5=\n");
	fprintf(fp, "RADIUS_Key6=\n");
	fprintf(fp, "RADIUS_Key7=\n");
	fprintf(fp, "RADIUS_Key8=\n");

	fprintf(fp, "RADIUS_Acct_Server=\n");
	fprintf(fp, "RADIUS_Acct_Port=%d\n", 1813);
	fprintf(fp, "RADIUS_Acct_Key=\n");

	//own_ip_addr
	if (flag_8021x == 1)
	{
		fprintf(fp, "own_ip_addr=%s\n", nvram_safe_get("lan_ipaddr"));
		fprintf(fp, "Ethifname=\n");
		fprintf(fp, "EAPifname=%s\n", "br0");
		fprintf(fp, "PreAuthifname=%s\n", "br0");
	}
	else
	{
		fprintf(fp, "own_ip_addr=\n");
		fprintf(fp, "Ethifname=\n");
		fprintf(fp, "EAPifname=\n");
		fprintf(fp, "PreAuthifname=\n");
	}

	fprintf(fp, "session_timeout_interval=%d\n", 0);
	fprintf(fp, "idle_timeout_interval=%d\n", 0);

	//WiFiTest
	fprintf(fp, "WiFiTest=0\n");

	//TGnWifiTest
	fprintf(fp, "TGnWifiTest=0\n");

/*
	if (nvram_match("sw_mode", "2") && nvram_invmatch("sta_ssid", ""))
	{
		int flag_wep;

		fprintf(fp, "ApCliEnable=1\n");
		fprintf(fp, "ApCliSsid=%s\n", nvram_safe_get("sta_ssid"));
		fprintf(fp, "ApCliBssid=\n");

		str = nvram_safe_get("sta_auth_mode");
		if (str && strlen(str))
		{
			if (!strcmp(str, "open") && nvram_match("sta_wep_x", "0"))
			{
				fprintf(fp, "ApCliAuthMode=%s\n", "OPEN");
				fprintf(fp, "ApCliEncrypType=%s\n", "NONE");
			}
			else if (!strcmp(str, "open") || !strcmp(str, "shared"))
			{
				flag_wep = 1;
				fprintf(fp, "ApCliAuthMode=%s\n", "WEPAUTO");
				fprintf(fp, "ApCliEncrypType=%s\n", "WEP");
			}
			else if (!strcmp(str, "psk") || !strcmp(str, "psk2"))
			{
				if (!strcmp(str, "psk"))
					fprintf(fp, "ApCliAuthMode=%s\n", "WPAPSK");
				else
					fprintf(fp, "ApCliAuthMode=%s\n", "WPA2PSK");

				//EncrypType
				if (nvram_match("sta_crypto", "tkip"))
					fprintf(fp, "ApCliEncrypType=%s\n", "TKIP");
				else if (nvram_match("sta_crypto", "aes"))
					fprintf(fp, "ApCliEncrypType=%s\n", "AES");

				//WPAPSK
				fprintf(fp, "ApCliWPAPSK=%s\n", nvram_safe_get("sta_wpa_psk"));
			}
			else
			{
				fprintf(fp, "ApCliAuthMode=%s\n", "OPEN");
				fprintf(fp, "ApCliEncrypType=%s\n", "NONE");
			}
		}
		else
		{
			fprintf(fp, "ApCliAuthMode=%s\n", "OPEN");
			fprintf(fp, "ApCliEncrypType=%s\n", "NONE");
		}

		//EncrypType
		if (flag_wep)
		{
			//DefaultKeyID
			fprintf(fp, "ApCliDefaultKeyID=%s\n", nvram_safe_get("sta_key"));

			//KeyType (0 -> Hex, 1->Ascii)
			fprintf(fp, "ApCliKey1Type=%s\n", nvram_safe_get("sta_key_type"));
			fprintf(fp, "ApCliKey2Type=%s\n", nvram_safe_get("sta_key_type"));
			fprintf(fp, "ApCliKey3Type=%s\n", nvram_safe_get("sta_key_type"));
			fprintf(fp, "ApCliKey4Type=%s\n", nvram_safe_get("sta_key_type"));

			//KeyStr
			fprintf(fp, "ApCliKey1Str=%s\n", nvram_safe_get("sta_key1"));
			fprintf(fp, "ApCliKey2Str=%s\n", nvram_safe_get("sta_key2"));
			fprintf(fp, "ApCliKey3Str=%s\n", nvram_safe_get("sta_key3"));
			fprintf(fp, "ApCliKey4Str=%s\n", nvram_safe_get("sta_key4"));
		}
		else
		{
			fprintf(fp, "ApCliDefaultKeyID=0\n");
			fprintf(fp, "ApCliKey1Type=0\n");
			fprintf(fp, "ApCliKey1Str=\n");
			fprintf(fp, "ApCliKey2Type=0\n");
			fprintf(fp, "ApCliKey2Str=\n");
			fprintf(fp, "ApCliKey3Type=0\n");
			fprintf(fp, "ApCliKey3Str=\n");
			fprintf(fp, "ApCliKey4Type=0\n");
			fprintf(fp, "ApCliKey4Str=\n");
		}
	}
	else
*/
	{
		fprintf(fp, "ApCliEnable=0\n");
		fprintf(fp, "ApCliSsid=\n");
		fprintf(fp, "ApCliBssid=\n");
		fprintf(fp, "ApCliAuthMode=\n");
		fprintf(fp, "ApCliEncrypType=\n");
		fprintf(fp, "ApCliWPAPSK=\n");
		fprintf(fp, "ApCliDefaultKeyID=0\n");
		fprintf(fp, "ApCliKey1Type=0\n");
		fprintf(fp, "ApCliKey1Str=\n");
		fprintf(fp, "ApCliKey2Type=0\n");
		fprintf(fp, "ApCliKey2Str=\n");
		fprintf(fp, "ApCliKey3Type=0\n");
		fprintf(fp, "ApCliKey3Str=\n");
		fprintf(fp, "ApCliKey4Type=0\n");
		fprintf(fp, "ApCliKey4Str=\n");
	}

	//RadioOn
	str = nvram_safe_get(strcat_r(prefix, "radio", tmp));
	if (str && strlen(str))
		fprintf(fp, "RadioOn=%d\n", atoi(str));
	else
	{
		warning = 51;
		fprintf(fp, "RadioOn=%d\n", 1);
	}

	fprintf(fp, "SSID=\n");
	fprintf(fp, "WPAPSK=\n");
	fprintf(fp, "Key1Str=\n");
	fprintf(fp, "Key2Str=\n");
	fprintf(fp, "Key3Str=\n");
	fprintf(fp, "Key4Str=\n");

	/* Wireless IGMP Snooping */
	fprintf(fp, "IgmpSnEnable=%d\n",
		nvram_get_int(strcat_r(prefix, "igs", tmp)) ? 1 : 0);

	/*	McastPhyMode, PHY mode for Multicast frames
	 *	McastMcs, MCS for Multicast frames
	 *
	 *	MODE=1, MCS=0: Legacy CCK 1Mbps
	 *	MODE=1, MCS=1: Legacy CCK 2Mbps
	 *	MODE=1, MCS=2: Legacy CCK 5.5Mbps
	 *	MODE=1, MCS=3: Legacy CCK 11Mbps
	 *	MODE=2, MCS=0: Legacy OFDM 6Mbps
	 *	MODE=2, MCS=1: Legacy OFDM 9Mbps
	 *	MODE=2, MCS=2: Legacy OFDM 12Mbps
	 *	MODE=2, MCS=3: Legacy OFDM 18Mbps
	 *	MODE=2, MCS=4: Legacy OFDM 24Mbps
	 * 	MODE=2, MCS=5: Legacy OFDM 36Mbps
	 *	MODE=2, MCS=6: Legacy OFDM 48Mbps
	 *	MODE=2, MCS=7: Legacy OFDM 54Mbps
	 *	MODE=3, MCS=0: HTMIX 6.5/15Mbps
	 *	MODE=3, MCS=1: HTMIX 13/30Mbps
	 *	MODE=3, MCS=2: HTMIX 19.5/45Mbps
	 *	MODE=3, MCS=7: HTMIX 65/150Mbps
	 *	MODE=3, MCS=8: HTMIX 13/30Mbps 2S
	 *	MODE=3, MCS=9: HTMIX 26/60Mbps 2S
	 *	MODE=3, MCS=10: HTMIX 39/90Mbps 2S
	 *	MODE=3, MCS=15: HTMIX 130/300Mbps 2S
	 **/
	i = nvram_get_int(strcat_r(prefix, "mrate_x", tmp));
next_mrate:
	switch (i++) {
	case 0: /* HTMIX 65/150Mbps */
		mcast_phy = 3, mcast_mcs = 7;
		break;
	case 1: /* Legacy CCK 1Mbps */
		mcast_phy = 1, mcast_mcs = 0;
		break;
	case 2: /* Legacy CCK 2Mbps */
		mcast_phy = 1, mcast_mcs = 1;
		break;
	case 3: /* Legacy CCK 5.5Mbps */
		mcast_phy = 1, mcast_mcs = 2;
		break;
	case 4: /* Legacy OFDM 6Mbps */
		mcast_phy = 2, mcast_mcs = 0;
		break;
	case 5: /* Legacy OFDM 9Mbps */
		mcast_phy = 2, mcast_mcs = 1;
		break;
	case 6: /* Legacy CCK 11Mbps */
		mcast_phy = 1, mcast_mcs = 3;
		break;
	case 7: /* Legacy OFDM 12Mbps */
		mcast_phy = 2, mcast_mcs = 2;
		break;
	case 8: /* Legacy OFDM 18Mbps */
		mcast_phy = 2, mcast_mcs = 3;
		break;
	case 9: /* Legacy OFDM 24Mbps */
		mcast_phy = 2, mcast_mcs = 4;
		break;
	case 10: /* Legacy OFDM 36Mbps */
		mcast_phy = 2, mcast_mcs = 5;
		break;
	case 11: /* Legacy OFDM 48Mbps */
		mcast_phy = 2, mcast_mcs = 6;
		break;
	case 12: /* Legacy OFDM 54Mbps */
		mcast_phy = 2, mcast_mcs = 7;
		break;
	case 13: /* HTMIX 130/300Mbps 2S */
		mcast_phy = 3, mcast_mcs = 15;
		break;
	case 14: /* HTMIX 6.5/15Mbps */
		mcast_phy = 3, mcast_mcs = 0;
		break;
	case 15: /* HTMIX 13/30Mbps */
		mcast_phy = 3, mcast_mcs = 1;
		break;
	case 16: /* HTMIX 19.5/45Mbps */
		mcast_phy = 3, mcast_mcs = 2;
		break;
	case 17: /* HTMIX 13/30Mbps 2S */
		mcast_phy = 3, mcast_mcs = 8;
		break;
	case 18: /* HTMIX 26/60Mbps 2S */
		mcast_phy = 3, mcast_mcs = 9;
		break;
	case 19: /* HTMIX 39/90Mbps 2S */
		mcast_phy = 3, mcast_mcs = 10;
		break;
	default: /* Disable */
		mcast_phy = 0, mcast_mcs = 0;
		break;
	}
	/* No CCK for 5Ghz band */
	if (band && mcast_phy == 1)
		goto next_mrate;
	fprintf(fp, "McastPhyMode=%d\n", mcast_phy);
	fprintf(fp, "McastMcs=%d\n", mcast_mcs);

	/* Set WSC/WPS variables */
	if(band)
		str = " (5G)";
	else
		str = "";
	fprintf(fp, "WscManufacturer=%s\n", "ASUSTeK Computer Inc.");
	fprintf(fp, "WscModelName=%s%s\n", "WPS Router", str);
	fprintf(fp, "WscDeviceName=%s%s\n", "ASUS WPS Router", str);
	fprintf(fp, "WscModelNumber=%s\n", get_productid());
	fprintf(fp, "WscSerialNumber=%s\n", "00000000");

	fprintf(fp, "WscV2Support=%d\n", 0);	// WPS/WSC v2 feature allows client to create connection via the PinCode of AP.
						// Disable this feature causes longer detection time (click AP till show dialog box) on WIN7 client when WSC disabled either.


#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	if (is_iNIC)
		fprintf(fp, "ExtEEPROM=%d\n", 1);

	if (is_iNIC && nvram_get_int("sw_mode") == SW_MODE_ROUTER)	// Only limite access of Guest network in Router mode
	{
		int vlan_id_1st = INIC_VLAN_ID_START;
		int vlan_id;
		char buf1[32], buf2[32], buf3[32];
		char *p1, *p2, *p3;

		p1 = buf1;
		p2 = buf2;
		p3 = buf3;
		memset(buf1, 0, sizeof(buf1));
		memset(buf2, 0, sizeof(buf2));
		memset(buf3, 0, sizeof(buf3));

		for (i = 1; i < MAX_NO_MSSID; i++)
		{
			vlan_id = 0;	// vlan id 1 for LAN access

			sprintf(prefix_mssid, "wl%d.%d_", band, i);
			if (!nvram_match(strcat_r(prefix_mssid, "bss_enabled", temp), "1"))
				continue;

			if (nvram_match(strcat_r(prefix_mssid, "lanaccess", temp), "off"))
				vlan_id = vlan_id_1st++;	// vlan id for no LAN access

			p1 += sprintf(p1, ";%d", vlan_id);
			p2 += sprintf(p2, ";%d", 0);
			p3 += sprintf(p3, ";%d", 0);
		}

		if(vlan_id_1st != INIC_VLAN_ID_START)
		{ //has vlan-based MBSSID for LAN access control but would make the wifi QoS field fixed at 0 (Best Effort).
			fprintf(fp, "VLAN_ID=0%s\n", buf1);
			fprintf(fp, "VLAN_TAG=0%s\n", buf2);
			fprintf(fp, "VLAN_Priority=0%s\n", buf3);
			fprintf(fp, "SwitchRemoveTag=1;1;1;1;1;0;0\n");
		}
	}
#endif

	if (warning)
	{
		printf("warning: %d!!!!\n", warning);
		printf("Miss some configuration, please check!!!!\n");
	}

	fclose(fp);
	return 0;
}


PAIR_CHANNEL_FREQ_ENTRY ChannelFreqTable[] = {
	//channel Frequency
	{1,     2412000},
	{2,     2417000},
	{3,     2422000},
	{4,     2427000},
	{5,     2432000},
	{6,     2437000},
	{7,     2442000},
	{8,     2447000},
	{9,     2452000},
	{10,    2457000},
	{11,    2462000},
	{12,    2467000},
	{13,    2472000},
	{14,    2484000},
	{34,    5170000},
	{36,    5180000},
	{38,    5190000},
	{40,    5200000},
	{42,    5210000},
	{44,    5220000},
	{46,    5230000},
	{48,    5240000},
	{52,    5260000},
	{56,    5280000},
	{60,    5300000},
	{64,    5320000},
	{100,   5500000},
	{104,   5520000},
	{108,   5540000},
	{112,   5560000},
	{116,   5580000},
	{120,   5600000},
	{124,   5620000},
	{128,   5640000},
	{132,   5660000},
	{136,   5680000},
	{140,   5700000},
	{149,   5745000},
	{153,   5765000},
	{157,   5785000},
	{161,   5805000},
};

char G_bRadio = 1;
int G_nChanFreqCount = sizeof (ChannelFreqTable) / sizeof(PAIR_CHANNEL_FREQ_ENTRY);

/************************ CONSTANTS & MACROS ************************/

/*
 * Constants fof WE-9->15
 */
#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

/****************************** TYPES ******************************/

/*
 *	Struct iw_range up to WE-15
 */
struct	iw15_range
{
	__u32		throughput;
	__u32		min_nwid;
	__u32		max_nwid;
	__u16		num_channels;
	__u8		num_frequency;
	struct iw_freq	freq[IW15_MAX_FREQUENCIES];
	__s32		sensitivity;
	struct iw_quality	max_qual;
	__u8		num_bitrates;
	__s32		bitrate[IW15_MAX_BITRATES];
	__s32		min_rts;
	__s32		max_rts;
	__s32		min_frag;
	__s32		max_frag;
	__s32		min_pmp;
	__s32		max_pmp;
	__s32		min_pmt;
	__s32		max_pmt;
	__u16		pmp_flags;
	__u16		pmt_flags;
	__u16		pm_capa;
	__u16		encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8		num_encoding_sizes;
	__u8		max_encoding_tokens;
	__u16		txpower_capa;
	__u8		num_txpower;
	__s32		txpower[IW15_MAX_TXPOWER];
	__u8		we_version_compiled;
	__u8		we_version_source;
	__u16		retry_capa;
	__u16		retry_flags;
	__u16		r_time_flags;
	__s32		min_retry;
	__s32		max_retry;
	__s32		min_r_time;
	__s32		max_r_time;
	struct iw_quality	avg_qual;
};

/*
 * Union for all the versions of iwrange.
 * Fortunately, I mostly only add fields at the end, and big-bang
 * reorganisations are few.
 */
union	iw_range_raw
{
	struct iw15_range	range15;	/* WE 9->15 */
	struct iw_range		range;		/* WE 16->current */
};

/*
 * Offsets in iw_range struct
 */
#define iwr15_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define iwr_off(f)	( ((char *) &(((struct iw_range *) NULL)->f)) - \
			  (char *) NULL)

/* Disable runtime version warning in ralink_get_range_info() */
int iw_ignore_version_sp = 0;

/*------------------------------------------------------------------*/
/*
 * Get the range information out of the driver
 */
int
ralink_get_range_info(iwrange *	range, char* buffer, int length)
{
  union iw_range_raw *	range_raw;

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if (length < 300)
    {
      /* That's v10 or earlier. Ouch ! Let's make a guess...*/
      range_raw->range.we_version_compiled = 9;
    }

  /* Check how it needs to be processed */
  if (range_raw->range.we_version_compiled > 15)
    {
      /* This is our native format, that's easy... */
      /* Copy stuff at the right place, ignore extra */
      memcpy((char *) range, buffer, sizeof(iwrange));
    }
  else
    {
      /* Zero unknown fields */
      bzero((char *) range, sizeof(struct iw_range));

      /* Initial part unmoved */
      memcpy((char *) range,
	     buffer,
	     iwr15_off(num_channels));
      /* Frequencies pushed futher down towards the end */
      memcpy((char *) range + iwr_off(num_channels),
	     buffer + iwr15_off(num_channels),
	     iwr15_off(sensitivity) - iwr15_off(num_channels));
      /* This one moved up */
      memcpy((char *) range + iwr_off(sensitivity),
	     buffer + iwr15_off(sensitivity),
	     iwr15_off(num_bitrates) - iwr15_off(sensitivity));
      /* This one goes after avg_qual */
      memcpy((char *) range + iwr_off(num_bitrates),
	     buffer + iwr15_off(num_bitrates),
	     iwr15_off(min_rts) - iwr15_off(num_bitrates));
      /* Number of bitrates has changed, put it after */
      memcpy((char *) range + iwr_off(min_rts),
	     buffer + iwr15_off(min_rts),
	     iwr15_off(txpower_capa) - iwr15_off(min_rts));
      /* Added encoding_login_index, put it after */
      memcpy((char *) range + iwr_off(txpower_capa),
	     buffer + iwr15_off(txpower_capa),
	     iwr15_off(txpower) - iwr15_off(txpower_capa));
      /* Hum... That's an unexpected glitch. Bummer. */
      memcpy((char *) range + iwr_off(txpower),
	     buffer + iwr15_off(txpower),
	     iwr15_off(avg_qual) - iwr15_off(txpower));
      /* Avg qual moved up next to max_qual */
      memcpy((char *) range + iwr_off(avg_qual),
	     buffer + iwr15_off(avg_qual),
	     sizeof(struct iw_quality));
    }

  /* We are now checking much less than we used to do, because we can
   * accomodate more WE version. But, there are still cases where things
   * will break... */
  if (!iw_ignore_version_sp)
    {
      /* We don't like very old version (unfortunately kernel 2.2.X) */
      if (range->we_version_compiled <= 10)
	{
	  dbg("Warning: Driver for the device has been compiled with an ancient version\n");
	  dbg("of Wireless Extension, while this program support version 11 and later.\n");
	  dbg("Some things may be broken...\n\n");
	}

      /* We don't like future versions of WE, because we can't cope with
       * the unknown */
      if (range->we_version_compiled > WE_MAX_VERSION)
	{
	  dbg("Warning: Driver for the device has been compiled with version %d\n", range->we_version_compiled);
	  dbg("of Wireless Extension, while this program supports up to version %d.\n", WE_VERSION);
	  dbg("Some things may be broken...\n\n");
	}

      /* Driver version verification */
      if ((range->we_version_compiled > 10) &&
	 (range->we_version_compiled < range->we_version_source))
	{
	  dbg("Warning: Driver for the device recommend version %d of Wireless Extension,\n", range->we_version_source);
	  dbg("but has been compiled with version %d, therefore some driver features\n", range->we_version_compiled);
	  dbg("may not be available...\n\n");
	}
      /* Note : we are only trying to catch compile difference, not source.
       * If the driver source has not been updated to the latest, it doesn't
       * matter because the new fields are set to zero */
    }

  /* Don't complain twice.
   * In theory, the test apply to each individual driver, but usually
   * all drivers are compiled from the same kernel. */
  iw_ignore_version_sp = 1;

  return (0);
}

int
getSSID(int band)
{
	struct iwreq wrq;
	wrq.u.data.flags = 0;
	char buffer[33];
	bzero(buffer, sizeof(buffer));
	wrq.u.essid.pointer = (caddr_t) buffer;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;

	if (wl_ioctl(get_wifname(band), SIOCGIWESSID, &wrq) < 0)
	{
		dbg("!!!\n");
		return 0;
	}

	if (wrq.u.essid.length>0)
	{
		unsigned char SSID[33];
		memset(SSID, 0, sizeof(SSID));
		memcpy(SSID, wrq.u.essid.pointer, wrq.u.essid.length);
		puts(SSID);
	}

	return 0;
}

int
getChannel(int band)
{
	int channel;
	struct iw_range	range;
	double freq;
	struct iwreq wrq1;
	struct iwreq wrq2;
	char ch_str[3];

	if (wl_ioctl(get_wifname(band), SIOCGIWFREQ, &wrq1) < 0)
		return 0;

	char buffer[sizeof(iwrange) * 2];
	bzero(buffer, sizeof(buffer));
	wrq2.u.data.pointer = (caddr_t) buffer;
	wrq2.u.data.length = sizeof(buffer);
	wrq2.u.data.flags = 0;

	if (wl_ioctl(get_wifname(band), SIOCGIWRANGE, &wrq2) < 0)
		return 0;

	if (ralink_get_range_info(&range, buffer, wrq2.u.data.length) < 0)
		return 0;

	freq = iw_freq2float(&(wrq1.u.freq));
	if (freq < KILO)
		channel = (int) freq;
	else
	{
		channel = iw_freq_to_channel(freq, &range);
		if (channel < 0)
			return 0;
	}

	memset(ch_str, 0, sizeof(ch_str));
	sprintf(ch_str, "%d", channel);
	puts(ch_str);
	return 0;
}

int
getSiteSurvey(int band)
{
	int i = 0, apCount = 0;
	char data[8192];
	char header[128];
	struct iwreq wrq;
	SSA *ssap;
	int lock;

	memset(data, 0x00, 255);
	strcpy(data, "SiteSurvey=1");
	wrq.u.data.length = strlen(data)+1;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;

	lock = file_lock("sitesurvey");
	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_SET, &wrq) < 0)
	{
		file_unlock(lock);

		dbg("Site Survey fails\n");
		return 0;
	}
	file_unlock(lock);

	dbg("Please wait");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");

	memset(data, 0, 8192);
	strcpy(data, "");
	wrq.u.data.length = 8192;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0)
	{
		dbg("errors in getting site survey result\n");
		return 0;
	}

	memset(header, 0, sizeof(header));
	//sprintf(header, "%-3s%-33s%-18s%-8s%-15s%-9s%-8s%-2s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "NT");
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");
	dbg("\n%s", header);

	if (wrq.u.data.length > 0)
	{
		ssap=(SSA *)(wrq.u.data.pointer+strlen(header)+1);
		int len = strlen(wrq.u.data.pointer+strlen(header))-1;
		char *sp, *op;
 		op = sp = wrq.u.data.pointer+strlen(header)+1;

		while (*sp && ((len - (sp-op)) >= 0))
		{
			ssap->SiteSurvey[i].channel[3] = '\0';
			ssap->SiteSurvey[i].ssid[32] = '\0';
			ssap->SiteSurvey[i].bssid[17] = '\0';
			ssap->SiteSurvey[i].encryption[8] = '\0';
			ssap->SiteSurvey[i].authmode[15] = '\0';
			ssap->SiteSurvey[i].signal[8] = '\0';
			ssap->SiteSurvey[i].wmode[7] = '\0';
//			ssap->SiteSurvey[i].bsstype[2] = '\0';
//			ssap->SiteSurvey[i].centralchannel[2] = '\0';

			sp+=strlen(header);
			apCount=++i;
		}

		for (i=0;i<apCount;i++)
		{
			dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
				ssap->SiteSurvey[i].channel,
				(char*)ssap->SiteSurvey[i].ssid,
				ssap->SiteSurvey[i].bssid,
				ssap->SiteSurvey[i].encryption,
				ssap->SiteSurvey[i].authmode,
				ssap->SiteSurvey[i].signal,
				ssap->SiteSurvey[i].wmode
//				ssap->SiteSurvey[i].bsstype,
//				ssap->SiteSurvey[i].centralchannel
			);
		}
		dbg("\n");
	}

	return 0;
}

int getBSSID(int band)	// get AP's BSSID
{
	unsigned char data[MACSIZE];
	char macaddr[18];
	struct iwreq wrq;

	memset(data, 0x00, MACSIZE);
	wrq.u.data.length = MACSIZE;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = OID_802_11_BSSID;

	if (wl_ioctl(get_wifname(band), RT_PRIV_IOCTL, &wrq) < 0)
	{
		dbg("errors in getting bssid!\n");
		return -1;
	}
	else
	{
		ether_etoa(data, macaddr);
		puts(macaddr);
		return 0;
	}
}

int
get_channel(int band)
{
	int channel;
	struct iw_range	range;
	double freq;
	struct iwreq wrq1;
	struct iwreq wrq2;

	if (wl_ioctl(get_wifname(band), SIOCGIWFREQ, &wrq1) < 0)
		return 0;

	char buffer[sizeof(iwrange) * 2];
	bzero(buffer, sizeof(buffer));
	wrq2.u.data.pointer = (caddr_t) buffer;
	wrq2.u.data.length = sizeof(buffer);
	wrq2.u.data.flags = 0;

	if (wl_ioctl(get_wifname(band), SIOCGIWRANGE, &wrq2) < 0)
		return 0;

	if (ralink_get_range_info(&range, buffer, wrq2.u.data.length) < 0)
		return 0;

	freq = iw_freq2float(&(wrq1.u.freq));
	if (freq < KILO)
		channel = (int) freq;
	else
	{
		channel = iw_freq_to_channel(freq, &range);
		if (channel < 0)
			return 0;
	}

	return channel;
}

int
asuscfe(const char *PwqV, const char *IF)
{
	if (strcmp(PwqV, "stat") == 0)
	{
		eval("iwpriv", (char*) IF, "stat");
	}
	else if (strstr(PwqV, "=") && strstr(PwqV, "=")!=PwqV)
	{
		eval("iwpriv", (char*) IF, "set", (char*) PwqV);
		puts("1");
	}
	return 0;
}

int
need_to_start_wps_5g()
{
	if (	nvram_match("wl1_auth_mode_x", "shared") ||
		nvram_match("wl1_auth_mode_x", "wpa") ||
		nvram_match("wl1_auth_mode_x", "wpa2") ||
		nvram_match("wl1_auth_mode_x", "wpawpa2") ||
		nvram_match("wl1_auth_mode_x", "radius") ||
		nvram_match("wl1_radio", "0") ||
		!nvram_match("sw_mode", "1")	)
		return 0;

	return 1;
}

int
need_to_start_wps_2g()
{
	if (	nvram_match("wl0_auth_mode_x", "shared") ||
		nvram_match("wl0_auth_mode_x", "wpa") ||
		nvram_match("wl0_auth_mode_x", "wpa2") ||
		nvram_match("wl0_auth_mode_x", "wpawpa2") ||
		nvram_match("wl0_auth_mode_x", "radius") ||
		nvram_match("wl0_radio", "0") ||
		!nvram_match("sw_mode", "1")	)
		return 0;

	return 1;
}

#if defined (W7_LOGO) || defined (wifi_LOGO)
int
wps_pin(int pincode)
{
	int wps_band = nvram_get_int("wps_band");
	char *wscd_pidfile;

	if (nvram_match("lan_ipaddr", ""))
		return 0;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return 0;
	}
	else
	{
		if (!need_to_start_wps_2g()) return 0;
	}

	system("route delete 239.255.255.250 1>/dev/null 2>&1");
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}
	free(wscd_pidfile);

	dbg("start wsc %s()\n", __func__);

	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);		// Enrollee + Proxy + Registrar

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());

	dbg("WPS: PIN\n");					// PIN method
	doSystem("iwpriv %s set WscMode=1", get_wpsifname());

	if (pincode == 0)
	{
		g_isEnrollee = 1;
		doSystem("iwpriv %s set WscPinCode=%d", get_wpsifname(), 0);
		doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);	// Trigger WPS AP to do simple config with WPS Client
	}
	else
	{
		doSystem("iwpriv %s set WscPinCode=%08d", get_wpsifname(), pincode);
		doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);	// Trigger WPS AP to do simple config with WPS Client
	}

	return 0;
}

int
wps_pbc()
{
	int wps_band = nvram_get_int("wps_band");
	char *wscd_pidfile;

	if (nvram_match("lan_ipaddr", ""))
		return 0;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return 0;
	}
	else
	{
		if (!need_to_start_wps_2g()) return 0;
	}

	system("route delete 239.255.255.250 1>/dev/null 2>&1");
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}
	free(wscd_pidfile);

	dbg("start wsc %s()\n", __func__);

	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);		// Enrollee + Proxy + Registrar

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());

//	dbg("WPS: PBC\n");
	g_isEnrollee = 1;
	doSystem("iwpriv %s set WscMode=%d", get_wpsifname(), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);		// Trigger WPS AP to do simple config with WPS Client

	return 0;
}

int
wps_pbc_both()
{
	char *wscd_pidfile_band;

	if (nvram_match("lan_ipaddr", ""))
		return 0;

	if (!need_to_start_wps_5g() || !need_to_start_wps_2g()) return 0;

	system("route delete 239.255.255.250 1>/dev/null 2>&1");

	wscd_pidfile_band = get_wscd_pidfile_band(1);
	if (check_if_file_exist(wscd_pidfile_band))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile_band);
		unlink(wscd_pidfile_band);
	}
	free(wscd_pidfile_band);

	wscd_pidfile_band = get_wscd_pidfile_band(0);
	if (check_if_file_exist(wscd_pidfile_band))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile_band);
		doSystem("rm -f /var/run/wscd.pid.%s", wscd_pidfile_band);
	}
	free(wscd_pidfile_band);

	dbg("start wsc %s()\n", __func__);

	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 7);		// Enrollee + Proxy + Registrar

	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 7);		// Enrollee + Proxy + Registrar

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(1));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(0));

//	dbg("WPS: PBC\n");
	g_isEnrollee = 1;
	doSystem("iwpriv %s set WscMode=%d", get_wifname(1), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(1), 1);		// Trigger WPS AP to do simple config with WPS Client

	doSystem("iwpriv %s set WscMode=%d", get_wifname(0), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(0), 1);		// Trigger WPS AP to do simple config with WPS Client

	return 0;
}
#else
int
wps_pin(int pincode)
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return 0;
	}
	else
	{
		if (!need_to_start_wps_2g()) return 0;
	}

//	dbg("WPS: PIN\n");
	doSystem("iwpriv %s set WscMode=1", get_wpsifname());

	if (pincode == 0)
		doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);	// Trigger WPS AP to do simple config with WPS Client
	else
		doSystem("iwpriv %s set WscPinCode=%08d", get_wpsifname(), pincode);

	return 0;
}

int
wps_pbc()
{
	int wps_band = nvram_get_int("wps_band");

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return 0;
	}
	else
	{
		if (!need_to_start_wps_2g()) return 0;
	}

//	dbg("WPS: PBC\n");
	g_isEnrollee = 1;
	doSystem("iwpriv %s set WscMode=%d", get_wpsifname(), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);		// Trigger WPS AP to do simple config with WPS Client

	return 0;
}

int
wps_pbc_both()
{
	if (!need_to_start_wps_5g() || !need_to_start_wps_2g()) return 0;

//	dbg("WPS: PBC\n");
	g_isEnrollee = 1;

	doSystem("iwpriv %s set WscMode=%d", get_wifname(1), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(1), 1);		// Trigger WPS AP to do simple config with WPS Client

	doSystem("iwpriv %s set WscMode=%d", get_wifname(0), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(0), 1);		// Trigger WPS AP to do simple config with WPS Client

	return 0;
}
#endif

extern struct nvram_tuple router_defaults[];

static void
wl_default_wps(int unit)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXX_", tmp[100];

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	for (t = router_defaults; t->name; t++) {
		if(strncmp(t->name, "wl_", 3) &&
			strncmp(t->name, "wl0_", 4) &&
			strncmp(t->name, "wl1_", 4))
			continue;

		if(!strncmp(t->name, "wl_", 3))
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
		else if (!strncmp(t->name, prefix, 4))
			nvram_set(t->name, t->value);
	}
}

void
wps_oob()
{
	int wps_band = nvram_get_int("wps_band");

	if (nvram_match("lan_ipaddr", ""))
		return;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return;
	}
	else
	{
		if (!need_to_start_wps_2g()) return;
	}

	nvram_set("w_Setting", "0");
	if (!wps_band)
		nvram_set("wl0_wsc_config_state", "0");
	else
		nvram_set("wl1_wsc_config_state", "0");

	wl_default_wps(wps_band);

	nvram_commit();

#if defined (W7_LOGO) || defined (wifi_LOGO)
	doSystem("iwpriv %s set AuthMode=%s", get_wpsifname(), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wpsifname(), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wpsifname(), 0);
	if (strlen(nvram_safe_get("wl1_key1")))
		iwprivSet(get_wpsifname(), "Key1", nvram_safe_get("wl1_key1"));
	if (strlen(nvram_safe_get("wl1_key2")))
		iwprivSet(get_wpsifname(), "Key2", nvram_safe_get("wl1_key2"));
	if (strlen(nvram_safe_get("wl1_key3")))
		iwprivSet(get_wpsifname(), "Key3", nvram_safe_get("wl1_key3"));
	if (strlen(nvram_safe_get("wl1_key4")))
		iwprivSet(get_wpsifname(), "Key4", nvram_safe_get("wl1_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wpsifname(), nvram_safe_get("wl1_key"));
	iwprivSet(get_wpsifname(), "SSID", nvram_safe_get("wl1_ssid"));

	system("route delete 239.255.255.250 1>/dev/null 2>&1");
	char *wscd_pidfile;
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}		
	free(wscd_pidfile);

	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);		// Enrollee + Proxy + Registrar
	doSystem("iwpriv %s set WscConfStatus=%d", get_wpsifname(), 1);		// AP is unconfigured
	g_wsc_configured = 0;

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());

	doSystem("iwpriv %s set WscMode=1", get_wpsifname());			// PIN method
//	doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);		// Trigger WPS AP to do simple config with WPS Client
#else
	doSystem("iwpriv %s set AuthMode=%s", get_wpsifname(), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wpsifname(), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wpsifname(), 0);
	if (strlen(nvram_safe_get("wl1_key1")))
		iwprivSet(get_wpsifname(), "Key1", nvram_safe_get("wl1_key1"));
	if (strlen(nvram_safe_get("wl1_key2")))
		iwprivSet(get_wpsifname(), "Key2", nvram_safe_get("wl1_key2"));
	if (strlen(nvram_safe_get("wl1_key3")))
		iwprivSet(get_wpsifname(), "Key3", nvram_safe_get("wl1_key3"));
	if (strlen(nvram_safe_get("wl1_key4")))
		iwprivSet(get_wpsifname(), "Key4", nvram_safe_get("wl1_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wpsifname(), nvram_safe_get("wl1_key"));
	iwprivSet(get_wpsifname(), "SSID", nvram_safe_get("wl1_ssid"));
	doSystem("iwpriv %s set WscConfStatus=%d", get_wpsifname(), 1);		// AP is unconfigured
#endif
	g_isEnrollee = 0;
}

void
wps_oob_both(void)
{
#if defined (W7_LOGO) || defined (wifi_LOGO)
	char *wscd_pidfile_band;
#endif

	if (nvram_match("lan_ipaddr", ""))
		return;

//	if (!need_to_start_wps_5g() || !need_to_start_wps_2g()) return;

	nvram_set("w_Setting", "0");
//	nvram_set("wl0_wsc_config_state", "0");
//	nvram_set("wl1_wsc_config_state", "0");
	wl_defaults_wps();

	nvram_commit();

#if defined (W7_LOGO) || defined (wifi_LOGO)
	doSystem("iwpriv %s set AuthMode=%s", get_wifname(1), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wifname(1), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wifname(1), 0);
	if (strlen(nvram_safe_get("wl1_key1")))
		iwprivSet(get_wifname(1), "Key1", nvram_safe_get("wl1_key1"));
	if (strlen(nvram_safe_get("wl1_key2")))
		iwprivSet(get_wifname(1), "Key2", nvram_safe_get("wl1_key2"));
	if (strlen(nvram_safe_get("wl1_key3")))
		iwprivSet(get_wifname(1), "Key3", nvram_safe_get("wl1_key3"));
	if (strlen(nvram_safe_get("wl1_key4")))
		iwprivSet(get_wifname(1), "Key4", nvram_safe_get("wl1_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wifname(1), nvram_safe_get("wl1_key"));
	iwprivSet(get_wifname(1), "SSID", nvram_safe_get("wl1_ssid"));

	doSystem("iwpriv %s set AuthMode=%s", get_wifname(0), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wifname(0), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wifname(0), 0);
	if (strlen(nvram_safe_get("wl0_key1")))
		iwprivSet(get_wifname(0), "Key1", nvram_safe_get("wl0_key1"));
	if (strlen(nvram_safe_get("wl0_key2")))
		iwprivSet(get_wifname(0), "Key2", nvram_safe_get("wl0_key2"));
	if (strlen(nvram_safe_get("wl0_key3")))
		iwprivSet(get_wifname(0), "Key3", nvram_safe_get("wl0_key3"));
	if (strlen(nvram_safe_get("wl0_key4")))
		iwprivSet(get_wifname(0), "Key4", nvram_safe_get("wl0_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wifname(0), nvram_safe_get("wl0_key"));
	iwprivSet(get_wifname(0), "SSID", nvram_safe_get("wl0_ssid"));

	system("route delete 239.255.255.250 1>/dev/null 2>&1");

	wscd_pidfile_band = get_wscd_pidfile_band(1);
	if (check_if_file_exist(wscd_pidfile_band))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile_band);
		unlink(wscd_pidfile_band);
	}
	free(wscd_pidfile_band);

	wscd_pidfile_band = get_wscd_pidfile_band(0);
	if (check_if_file_exist(wscd_pidfile_band))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile_band);
		unlink(wscd_pidfile_band);
	}
	free(wscd_pidfile_band);

	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 7);		// Enrollee + Proxy + Registrar
	doSystem("iwpriv %s set WscConfStatus=%d", get_wifname(1), 1);		// AP is unconfigured

	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 0);		// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 7);		// Enrollee + Proxy + Registrar
	doSystem("iwpriv %s set WscConfStatus=%d", get_wifname(0), 1);		// AP is unconfigured

	g_wsc_configured = 0;

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");

	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));

	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(1));

	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(0));

	doSystem("iwpriv %s set WscMode=1", get_wifname(1));			// PIN method

	doSystem("iwpriv %s set WscMode=1", get_wifname(0));			// PIN method

//	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(1), 1);		// Trigger WPS AP to do simple config with WPS Client
#else
	doSystem("iwpriv %s set AuthMode=%s", get_wifname(1), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wifname(1), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wifname(1), 0);
	if (strlen(nvram_safe_get("wl1_key1")))
		iwprivSet(get_wifname(1), "Key1", nvram_safe_get("wl1_key1"));
	if (strlen(nvram_safe_get("wl1_key2")))
		iwprivSet(get_wifname(1), "Key2", nvram_safe_get("wl1_key2"));
	if (strlen(nvram_safe_get("wl1_key3")))
		iwprivSet(get_wifname(1), "Key3", nvram_safe_get("wl1_key3"));
	if (strlen(nvram_safe_get("wl1_key4")))
		iwprivSet(get_wifname(1), "Key4", nvram_safe_get("wl1_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wifname(1), nvram_safe_get("wl1_key"));
	iwprivSet(get_wifname(1), "SSID", nvram_safe_get("wl1_ssid"));
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 0);		// WPS disabled. Force WPS status to change
	doSystem("iwpriv %s set WscConfStatus=%d", get_wifname(1), 1);		// AP is unconfigured
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 1);		// WPS enabled. Force WPS status to change

	doSystem("iwpriv %s set AuthMode=%s", get_wifname(0), "OPEN");
	doSystem("iwpriv %s set EncrypType=%s", get_wifname(0), "NONE");
	doSystem("iwpriv %s set IEEE8021X=%d", get_wifname(0), 0);
	if (strlen(nvram_safe_get("wl0_key1")))
		iwprivSet(get_wifname(0), "Key1", nvram_safe_get("wl0_key1"));
	if (strlen(nvram_safe_get("wl0_key2")))
		iwprivSet(get_wifname(0), "Key2", nvram_safe_get("wl0_key2"));
	if (strlen(nvram_safe_get("wl0_key3")))
		iwprivSet(get_wifname(0), "Key3", nvram_safe_get("wl0_key3"));
	if (strlen(nvram_safe_get("wl0_key4")))
		iwprivSet(get_wifname(0), "Key4", nvram_safe_get("wl0_key4"));
	doSystem("iwpriv %s set DefaultKeyID=%s", get_wifname(0), nvram_safe_get("wl0_key"));
	iwprivSet(get_wifname(0), "SSID", nvram_safe_get("wl0_ssid"));
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 0);		// WPS disabled. Force WPS status to change
	doSystem("iwpriv %s set WscConfStatus=%d", get_wifname(0), 1);		// AP is unconfigured
	doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 1);		// WPS enabled. Force WPS status to change
#endif
	g_isEnrollee = 0;
}

void
start_wsc(void)
{
	int wps_band = nvram_get_int("wps_band");
	char *wps_sta_pin = nvram_safe_get("wps_sta_pin");
	char *wscd_pidfile;

	if (nvram_match("lan_ipaddr", ""))
		return;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return;
	}
	else
	{
		if (!need_to_start_wps_2g()) return;
	}

	system("route delete 239.255.255.250 1>/dev/null 2>&1");
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}
	free(wscd_pidfile);

	dbg("start wsc %s()\n", __func__);

	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);	// WPS disabled
	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);	// Enrollee + Proxy + Registrar

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());

//#if defined (W7_LOGO) || defined (wifi_LOGO)
	if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000") && (wl_wpsPincheck(wps_sta_pin) == 0))
	{
		dbg("WPS: PIN\n");					// PIN method
		g_isEnrollee = 0;
		doSystem("iwpriv %s set WscMode=1", get_wpsifname());
		doSystem("iwpriv %s set WscPinCode=%s", get_wpsifname(), wps_sta_pin);
	}
	else
	{
		dbg("WPS: PBC\n");					// PBC method
		g_isEnrollee = 1;
		doSystem("iwpriv %s set WscMode=2", get_wpsifname());
//		doSystem("iwpriv %s set WscPinCode=%s", get_wpsifname(), "00000000");
	}

	doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);	// Trigger WPS AP to do simple config with WPS Client
//#endif
}
#if 0
void
start_wsc_pbc()
{
	int wps_band = nvram_get_int("wps_band");

	if (nvram_match("lan_ipaddr", ""))
		return;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return;
	}
	else
	{
		if (!need_to_start_wps_2g()) return;
	}

	dbg("start wsc %s()\n", __func__);
	if (nvram_match("wps_enable", "0"))
	{
		char *wscd_pidfile;
		system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
		wscd_pidfile = get_wscd_pidfile();
		if (check_if_file_exist(wscd_pidfile))
		{
			doSystem("kill -9 `cat %s`", wscd_pidfile);
			unlink(wscd_pidfile);
		}
		free(wscd_pidfile);
		
		char str_lan_ipaddr[16];
		strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
		doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());
		doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);	// Enrollee + Proxy + Registrar
	}

	dbg("WPS: PBC\n");
	doSystem("iwpriv %s set WscMode=%d", get_wpsifname(), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wpsifname(), 1);		// Trigger WPS AP to do simple config with WPS Client

	nvram_set("wps_enable", "1");
}

void
start_wsc_pbc_both()
{
	char *wscd_pidfile_band;

	if (nvram_match("lan_ipaddr", ""))
		return;

	if (!need_to_start_wps_5g() || !need_to_start_wps_2g()) return;

	dbg("start wsc %s()\n", __func__);
//	if (nvram_match("wps_enable", "0"))
	{
		system("route delete 239.255.255.250 1>/dev/null 2>&1");

		wscd_pidfile_band = get_wscd_pidfile_band(1);
		if (check_if_file_exist(wscd_pidfile_band))
		{
			doSystem("kill -9 `cat %s`", wscd_pidfile_band);
			unlink(wscd_pidfile_band);
		}
		free(wscd_pidfile_band);

		wscd_pidfile_band = get_wscd_pidfile_band(0);
		if (check_if_file_exist(wscd_pidfile_band))
		{
			doSystem("kill -9 `cat %s`", wscd_pidfile_band);
			unlink(wscd_pidfile_band);
		}
		free(wscd_pidfile_band);

		system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");

		char str_lan_ipaddr[16];
		strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));

		doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(1));
		doSystem("iwpriv %s set WscConfMode=%d", get_wifname(1), 7);	// Enrollee + Proxy + Registrar

		doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wifname(0));
		doSystem("iwpriv %s set WscConfMode=%d", get_wifname(0), 7);	// Enrollee + Proxy + Registrar
	}

	dbg("WPS: PBC\n");

	doSystem("iwpriv %s set WscMode=%d", get_wifname(1), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(1), 1);		// Trigger WPS AP to do simple config with WPS Client

	doSystem("iwpriv %s set WscMode=%d", get_wifname(0), 2);		// PBC method
	doSystem("iwpriv %s set WscGetConf=%d", get_wifname(0), 1);		// Trigger WPS AP to do simple config with WPS Client

	nvram_set("wps_enable", "1");
}
#endif

void
start_wsc_pin_enrollee(void)
{
	int wps_band = nvram_get_int("wps_band");
	char *wscd_pidfile;

	if (nvram_match("lan_ipaddr", ""))
	{
		nvram_set("wps_enable", "0");
		return;
	}

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return;
	}
	else
	{
		if (!need_to_start_wps_2g()) return;
	}

	system("route add -host 239.255.255.250 dev br0 1>/dev/null 2>&1");
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}
	free(wscd_pidfile);

	dbg("start wsc %s()\n", __func__);

	char str_lan_ipaddr[16];
	strcpy(str_lan_ipaddr, nvram_safe_get("lan_ipaddr"));
	doSystem("wscd -m 1 -a %s -i %s &", str_lan_ipaddr, get_wpsifname());
	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 7);		// Enrollee + Proxy + Registrar	

	dbg("WPS: PIN\n");
	doSystem("iwpriv %s set WscMode=1", get_wpsifname());

//	nvram_set("wps_start_flag", "1");
}

void
stop_wsc(void)
{
	int wps_band = nvram_get_int("wps_band");
	char *wscd_pidfile;

	if (wps_band)
	{
		if (!need_to_start_wps_5g()) return;
	}
	else
	{
		if (!need_to_start_wps_2g()) return;
	}

	system("route delete 239.255.255.250 1>/dev/null 2>&1");
	wscd_pidfile = get_wscd_pidfile();
	if (check_if_file_exist(wscd_pidfile))
	{
		doSystem("kill -9 `cat %s`", wscd_pidfile);
		unlink(wscd_pidfile);
	}
	free(wscd_pidfile);

	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);		// WPS disabled
	doSystem("iwpriv %s set WscStatus=%d", get_wpsifname(), 0);		// Not Used
}

int getWscStatus(void)
{
	int data = 0;
	struct iwreq wrq;
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = RT_OID_WSC_QUERY_STATUS;

	if (wl_ioctl(get_wpsifname(), RT_PRIV_IOCTL, &wrq) < 0)
		dbg("errors in getting WSC status\n");

	return data;
}

int getWscProfile(char *interface, WSC_CONFIGURED_VALUE *data, int len)
{
	int socket_id;
	struct iwreq wrq;

	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy((char *)data, "get_wsc_profile");
	strcpy(wrq.ifr_name, interface);
	wrq.u.data.length = len;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = 0;
	ioctl(socket_id, RTPRIV_IOCTL_WSC_PROFILE, &wrq);
	close(socket_id);
	return 0;
}

int
stainfo(int band)
{
	char data[2048];
	struct iwreq wrq;

	memset(data, 0x00, 2048);
	wrq.u.data.length = 2048;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GSTAINFO;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0)
	{
		dbg("errors in getting STAINFO result\n");
		return 0;
	}

	if (wrq.u.data.length > 0)
	{
		puts(wrq.u.data.pointer);
	}

	return 0;
}

int
getstat(int band)
{
	char data[4096];
	struct iwreq wrq;

	memset(data, 0x00, 4096);
	wrq.u.data.length = 4096;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GSTAT;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0)
	{
		dbg("errors in getting STAT result\n");
		return 0;
	}

	if (wrq.u.data.length > 0)
	{
		puts(wrq.u.data.pointer);
	}

	return 0;
}

#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
void getRSSI(const char *start, char **end, char *rssi)
{
	char *pS, *pV, *pN;

	if(start == NULL || rssi == NULL)
		return;

	rssi[0] = '\0';
	if((pS = strstr(start, "RSSI")) == NULL)
		return;

	if((pN = strchr(pS, '\n')) == NULL)
		return;

	if(end)
		*end = pN;

	if((pV = strchr(pS, '=')) == NULL || pV > pN)
		return;

	pV++;
	while(pV[0] == ' ' || pV[0] == '\t')
		pV++;

	while(pN > pV && isspace(pN[-1]))
		pN--;

	if(pN-pV > 0)
	{
		memcpy(rssi, pV, pN-pV);
		rssi[pN-pV] = '\0';
	}
}
#endif	/* RTCONFIG_WLMODULE_RT3352_INIC_MII */

int
getrssi(int band)
{
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
#define RTPRIV_IOCTL_STATISTICS		(SIOCIWFIRSTPRIV + 0x09)
	char data[1024];
	struct iwreq wrq;

	memset(data, 0x00, sizeof(data));
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = 0;

	if(wl_ioctl(get_wifname(band), RTPRIV_IOCTL_STATISTICS, &wrq) < 0)
	{
		dbg("errors in getting STATISTICS result\n");
		return 0;
	}

	if(wrq.u.data.length > 0)
	{
		char *start;
		char rssi1[8], rssi2[8], rssi3[8];
		start = wrq.u.data.pointer;
		getRSSI(start, &start, rssi1);
		getRSSI(start, &start, rssi2);
		getRSSI(start, &start, rssi3);
		printf("%s,%s,%s\n", rssi1, rssi2, rssi3);
	}

	return 0;
#else	/* !RTCONFIG_WLMODULE_RT3352_INIC_MII */
	char data[32];
	struct iwreq wrq;

	memset(data, 0x00, 32);
	wrq.u.data.length = 32;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = ASUS_SUBCMD_GRSSI;

	if (wl_ioctl(get_wifname(band), RTPRIV_IOCTL_ASUSCMD, &wrq) < 0)
	{
		dbg("errors in getting RSSI result\n");
		return 0;
	}

	if(wrq.u.data.length > 0)
	{
		puts(wrq.u.data.pointer);
	}

	return 0;
#endif	/* RTCONFIG_WLMODULE_RT3352_INIC_MII */
}

void
wsc_user_commit()
{
	int flag_wep;

	if (nvram_match("wl0_wsc_config_state", "2"))
	{
		flag_wep = 0;

		if (nvram_match("wl0_auth_mode_x", "open") && nvram_match("wl0_wep_x", "0"))
		{
			doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "NONE");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_2G, 0);
		}
		else if (nvram_match("wl0_auth_mode_x", "open"))
		{
			flag_wep = 1;
			doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "WEP");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_2G, 0);
		}
		else if (nvram_match("wl0_auth_mode_x", "shared"))
		{
			flag_wep = 1;
			doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "SHARED");
			doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "WEP");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_2G, 0);
		}
		else if (nvram_match("wl0_auth_mode_x", "psk") || nvram_match("wl0_auth_mode_x", "psk2") || nvram_match("wl0_auth_mode_x", "pskpsk2"))
		{
			if (nvram_match("wl0_auth_mode_x", "pskpsk2"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "WPAPSKWPA2PSK");
			else if (nvram_match("wl0_auth_mode_x", "psk"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "WPAPSK");
			else if (nvram_match("wl0_auth_mode_x", "psk2"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "WPA2PSK");

			//EncrypType
			if (nvram_match("wl0_crypto", "tkip"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "TKIP");
			else if (nvram_match("wl0_crypto", "aes"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "AES");
			else if (nvram_match("wl0_crypto", "tkip+aes"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "TKIPAES");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_2G, 0);

			iwprivSet(WIF_2G, "SSID", nvram_safe_get("wl0_ssid"));

			//WPAPSK
			iwprivSet(WIF_2G, "WPAPSK", nvram_safe_get("wl0_wpa_psk"));

			doSystem("iwpriv %s set DefaultKeyID=%s", WIF_2G, "2");
		}
		else
		{
			doSystem("iwpriv %s set AuthMode=%s", WIF_2G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_2G, "NONE");
		}

//		if (nvram_invmatch("wl0_wep_x", "0"))
		if (flag_wep)
		{
			//KeyStr
			if (strlen(nvram_safe_get("wl0_key1")))
				iwprivSet(WIF_2G, "Key1", nvram_safe_get("wl0_key1"));
			if (strlen(nvram_safe_get("wl0_key2")))
				iwprivSet(WIF_2G, "Key2", nvram_safe_get("wl0_key2"));
			if (strlen(nvram_safe_get("wl0_key3")))
				iwprivSet(WIF_2G, "Key3", nvram_safe_get("wl0_key3"));
			if (strlen(nvram_safe_get("wl0_key4")))
				iwprivSet(WIF_2G, "Key4", nvram_safe_get("wl0_key4"));

			//DefaultKeyID
			doSystem("iwpriv %s set DefaultKeyID=%s", WIF_2G, nvram_safe_get("wl0_key"));
		}

		iwprivSet(WIF_2G, "SSID", nvram_safe_get("wl0_ssid"));

		nvram_set("wl0_wsc_config_state", "1");
		doSystem("iwpriv %s set WscConfStatus=%d", WIF_2G, 2);	// AP is configured
	}

	if (nvram_match("wl1_wsc_config_state", "2"))
	{
		flag_wep = 0;

		if (nvram_match("wl1_auth_mode_x", "open") && nvram_match("wl1_wep_x", "0"))
		{
			doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "NONE");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_5G, 0);
		}
		else if (nvram_match("wl1_auth_mode_x", "open"))
		{
			flag_wep = 1;
			doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "WEP");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_5G, 0);
		}
		else if (nvram_match("wl1_auth_mode_x", "shared"))
		{
			flag_wep = 1;
			doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "SHARED");
			doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "WEP");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_5G, 0);
		}
		else if (nvram_match("wl1_auth_mode_x", "psk") || nvram_match("wl1_auth_mode_x", "psk2") || nvram_match("wl1_auth_mode_x", "pskpsk2"))
		{
			if (nvram_match("wl1_auth_mode_x", "pskpsk2"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "WPAPSKWPA2PSK");
			else if (nvram_match("wl1_auth_mode_x", "psk"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "WPAPSK");
			else if (nvram_match("wl1_auth_mode_x", "psk2"))
				doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "WPA2PSK");

			//EncrypType
			if (nvram_match("wl1_crypto", "tkip"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "TKIP");
			else if (nvram_match("wl1_crypto", "aes"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "AES");
			else if (nvram_match("wl1_crypto", "tkip+aes"))
				doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "TKIPAES");

			doSystem("iwpriv %s set IEEE8021X=%d", WIF_5G, 0);

			iwprivSet(WIF_5G, "SSID", nvram_safe_get("wl1_ssid"));

			//WPAPSK
			iwprivSet(WIF_5G, "WPAPSK", nvram_safe_get("wl1_wpa_psk"));

			doSystem("iwpriv %s set DefaultKeyID=%s", WIF_5G, "2");
		}
		else
		{
			doSystem("iwpriv %s set AuthMode=%s", WIF_5G, "OPEN");
			doSystem("iwpriv %s set EncrypType=%s", WIF_5G, "NONE");
		}

//		if (nvram_invmatch("wl1_wep_x", "0"))
		if (flag_wep)
		{
			//KeyStr
			if (strlen(nvram_safe_get("wl1_key1")))
				iwprivSet(WIF_5G, "Key1", nvram_safe_get("wl1_key1"));
			if (strlen(nvram_safe_get("wl1_key2")))
				iwprivSet(WIF_5G, "Key2", nvram_safe_get("wl1_key2"));
			if (strlen(nvram_safe_get("wl1_key3")))
				iwprivSet(WIF_5G, "Key3", nvram_safe_get("wl1_key3"));
			if (strlen(nvram_safe_get("wl1_key4")))
				iwprivSet(WIF_5G, "Key4", nvram_safe_get("wl1_key4"));

			//DefaultKeyID
			doSystem("iwpriv %s set DefaultKeyID=%s", WIF_5G, nvram_safe_get("wl1_key"));
		}

		iwprivSet(WIF_5G, "SSID", nvram_safe_get("wl1_ssid"));

		nvram_set("wl1_wsc_config_state", "1");
		doSystem("iwpriv %s set WscConfStatus=%d", WIF_5G, 2);	// AP is configured
	}

//	doSystem("iwpriv %s set WscConfMode=%d", get_non_wpsifname(), 7);	// trigger Windows OS to give a popup about WPS PBC AP
}

int
wl_WscConfigured(int unit)
{
	WSC_CONFIGURED_VALUE result;
	struct iwreq wrq;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	wrq.u.data.length = sizeof(WSC_CONFIGURED_VALUE);
	wrq.u.data.pointer = (caddr_t) &result;
	wrq.u.data.flags = 0;
	strcpy((char *)&result, "get_wsc_profile");

	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_WSC_PROFILE, &wrq) < 0)
	{
		fprintf(stderr, "errors in getting WSC profile\n");
		return -1;
	}

	if (result.WscConfigured == 2)
		return 1;
	else
		return 0;
}

#define FAIL_LOG_MAX 100

struct FAIL_LOG
{
	unsigned char num;
	unsigned char bits[15];
};

void
Get_fail_log(char *buf, int size, unsigned int offset)
{
	struct FAIL_LOG fail_log, *log = &fail_log;
	char *p = buf;
	int x, y;

	memset(buf, 0, size);
	FRead((char*) &fail_log, offset, sizeof(fail_log));
	if(log->num == 0 || log->num > FAIL_LOG_MAX)
	{
		return;
	}
	for(x = 0; x < (FAIL_LOG_MAX >> 3); x++)
	{
		for(y = 0; log->bits[x] != 0 && y < 7; y++)
		{
			if(log->bits[x] & (1 << y))
			{
				p += snprintf(p, size - (p - buf), "%d,", (x << 3) + y);
			}
		}
	}
}

void
Gen_fail_log(const char *logStr, int max, struct FAIL_LOG *log)
{
	const char *p = logStr;
	char *next;
	int num;
	int x,y;

	memset(log, 0, sizeof(struct FAIL_LOG));
	if(max > FAIL_LOG_MAX)
		log->num = FAIL_LOG_MAX;
	else
		log->num = max;

	if(logStr == NULL)
		return;

	while(*p != '\0')
	{
		while(*p != '\0' && !isdigit(*p))
			p++;
		if(*p == '\0')
			break;
		num = strtoul(p, &next, 0);
		if(num > FAIL_LOG_MAX)
			break;
		x = num >> 3;
		y = num & 0x7;
		log->bits[x] |= (1 << y);
		p = next;
	}
}

void
Get_fail_ret(void)
{
	unsigned char str[ OFFSET_FAIL_BOOT_LOG - OFFSET_FAIL_RET ];
	FRead(str, OFFSET_FAIL_RET, sizeof(str));
	if(str[0] == 0 || str[0] == 0xff)
		return;
	str[sizeof(str) -1] = '\0';
	puts(str);
}

void
Get_fail_reboot_log(void)
{
	char str[512];
	Get_fail_log(str, sizeof(str), OFFSET_FAIL_BOOT_LOG);
	puts(str);
}

void
Get_fail_dev_log(void)
{
	char str[512];
	Get_fail_log(str, sizeof(str), OFFSET_FAIL_DEV_LOG);
	puts(str);
}

void
ate_commit_bootlog(char *err_code)
{
	unsigned char fail_buffer[ OFFSET_SERIAL_NUMBER - OFFSET_FAIL_RET ];

	nvram_set("Ate_power_on_off_enable", err_code);
	nvram_commit();

	memset(fail_buffer, 0, sizeof(fail_buffer));
	strncpy(fail_buffer, err_code, OFFSET_FAIL_BOOT_LOG - OFFSET_FAIL_RET -1);
	Gen_fail_log(nvram_get("Ate_reboot_log"), nvram_get_int("Ate_boot_check"), (struct FAIL_LOG *) &fail_buffer[ OFFSET_FAIL_BOOT_LOG - OFFSET_FAIL_RET ]);
	Gen_fail_log(nvram_get("Ate_dev_log"), nvram_get_int("Ate_boot_check"), (struct FAIL_LOG *) &fail_buffer[ OFFSET_FAIL_DEV_LOG  - OFFSET_FAIL_RET ]);

	FWrite(fail_buffer, OFFSET_FAIL_RET, sizeof(fail_buffer));
}

#if defined(RTN65U)
void
ate_run_in(void)
{
	unsigned char ateTxFreqOffset;
	char tmpbuf[32];
	char *wl_ifnames;
	int rai0 = 0;
	int ra0  = 0;

	wl_ifnames = nvram_safe_get("wl_ifnames");
	if(strstr(wl_ifnames, "rai0") != NULL)
		rai0 = 1;
	if(strstr(wl_ifnames, "ra0" ) != NULL)
		ra0  = 1;

	if(rai0)
	{
		eval("iwpriv", "rai0", "set", "ATE=ATESTART");
		eval("iwpriv", "rai0", "set", "ATECHANNEL=6");
		eval("iwpriv", "rai0", "set", "ATETXANT=0");
		eval("iwpriv", "rai0", "set", "ATETXMODE=1");
		eval("iwpriv", "rai0", "set", "ATETXMCS=7");
		eval("iwpriv", "rai0", "set", "ATETXBW=0");
		eval("iwpriv", "rai0", "set", "ATETXGI=0");
		eval("iwpriv", "rai0", "set", "ATETXLEN=1024");
		FRead(&ateTxFreqOffset, 0x4803A, 1);
		sprintf(tmpbuf, "ATETXFREQOFFSET=%u", (unsigned char) ateTxFreqOffset);
		eval("iwpriv", "rai0", "set", tmpbuf);
		eval("iwpriv", "rai0", "set", "ATETXPOW0=0");
		eval("iwpriv", "rai0", "set", "ATETXPOW1=0");
		eval("iwpriv", "rai0", "set", "ATEAUTOALC=1");
		eval("iwpriv", "rai0", "set", "ATEIPG=200");
		eval("iwpriv", "rai0", "set", "ATETXCNT=1000000000000000");
		eval("iwpriv", "rai0", "set", "ATE=TXFRAME");
	}

	if(ra0)
	{
		eval("iwpriv", "ra0", "set", "ATE=ATESTART");
		eval("iwpriv", "ra0", "set", "ATECHANNEL=48");
		eval("iwpriv", "ra0", "set", "ATETXANT=0");
		eval("iwpriv", "ra0", "set", "ATETXMODE=1");
		eval("iwpriv", "ra0", "set", "ATETXMCS=7");
		eval("iwpriv", "ra0", "set", "ATETXBW=0");
		eval("iwpriv", "ra0", "set", "ATETXGI=0");
		eval("iwpriv", "ra0", "set", "ATETXLEN=1024");
		FRead(&ateTxFreqOffset, 0x40044, 1);
		sprintf(tmpbuf, "ATETXFREQOFFSET=%u", (unsigned char) ateTxFreqOffset);
		eval("iwpriv", "ra0", "set", tmpbuf);
		eval("iwpriv", "ra0", "set", "ATETXPOW0=0");
		eval("iwpriv", "ra0", "set", "ATETXPOW1=0");
		eval("iwpriv", "ra0", "set", "ATETXPOW2=0");
		eval("iwpriv", "ra0", "set", "ATEAUTOALC=1");
		eval("iwpriv", "ra0", "set", "ATEIPG=200");
		eval("iwpriv", "ra0", "set", "ATETXCNT=1000000000000000");
		eval("iwpriv", "ra0", "set", "ATE=TXFRAME");
		eval("iwpriv", "ra0", "mac", "102C=40000000");	//set LED on
	}

	if(ra0 && rai0)
	{ //stop and restart 2.4G after setting 5G
		eval("iwpriv", "rai0", "set", "ATE=ATESTART");
		eval("iwpriv", "rai0", "set", "ATE=TXFRAME");
	}
}
#endif // RTN65U

#if !defined(RTN14U) && !defined(RTAC52U)
int Set_SwitchPort_LEDs(const char *group, const char *action)
{
	int groupNo;
	int actionNo;

	if((groupNo = strtol(group, NULL, 0)) < 0 || groupNo > 2)
		return -1;

	if (strcmp(action, "normal") == 0)
		actionNo = 0;
	else if (strcmp(action, "blink") == 0)
		actionNo = 1;
	else if (strcmp(action, "off") == 0)
		actionNo = 2;
	else if (strcmp(action, "on") == 0)
		actionNo = 3;
	else
		return -1;

	return rtkswitch_ioctl(100, (groupNo << 16) | (actionNo));
}
#endif

#define DEV_FLAGS_MAGIC "FL"
struct device_flags
{
	char magic[2];
	union {
		__u16 value;
		__u16 reserve:15,
		      has_thermal_pad:1;
	} u;
};

int Get_Device_Flags(void)
{
	struct device_flags dev_flags;
	int ret = -1;
	if( FRead((char *)&dev_flags, OFFSET_DEV_FLAGS, 4) < 0)
		dbg("READ DEV Flags: Out of scope\n");
	else if (memcmp(&dev_flags.magic, DEV_FLAGS_MAGIC, sizeof(dev_flags.magic)) != 0)
		dbg("READ DEV Flags: no contents !\n");
	else
	{
		char buf[128];
		char *p = buf;
		if(dev_flags.u.has_thermal_pad)
			p += sprintf(p, " Has Thermal Pad.");
		printf("Flags: 0x%04x\n%s\n", (__u16)dev_flags.u.value, buf);
		ret = 0;
	}
	return ret;
}

int Set_Device_Flags(const char *flags_str)
{
	struct device_flags dev_flags;

	if(flags_str == NULL || strlen(flags_str) != 6 || strncmp(flags_str, "0x", 2) != 0)
		return -1;

	memset(&dev_flags, 0, sizeof(dev_flags));
	memcpy(&dev_flags.magic, DEV_FLAGS_MAGIC, sizeof(dev_flags.magic));
	dev_flags.u.value = strtoul(flags_str, NULL, 16);
	FWrite((const char *)&dev_flags, OFFSET_DEV_FLAGS, 4);
	return Get_Device_Flags();
}

#endif

