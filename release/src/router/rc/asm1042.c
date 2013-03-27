
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <shutils.h>	//_eval()
#include <shared.h>	//_dprintf()
#include <bcmnvram.h>	//nvram_match()


#define ASM1042_DIR		"/asm1042"
#define ASM1042_TOOL		"104xfwdl"
#define ASM1042_FW		"130125_00_02_00.bin"
#define ASM1042_OUT		"/tmp/asm1042_log"
#define ASM1042_VER_HEAD	"Version :"

extern void asm1042_upgrade(int skip_usb_dev);


/* chk_usb_devices
 *
 * < 0: error
 * = 0: no usb devices
 * > 0: has usb devices
 */
static int chk_usb_devices(void)
{
	char buf[512];
	int lev = 0;
	FILE *fp;
	char *pHead, *pLev;

	if((fp = fopen("/proc/bus/usb/devices", "rb")) == NULL)
		return -1;

	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		if((pHead = strstr(buf, "Lev=")) == NULL)
			continue;

		pLev = pHead + 4;
		lev = atoi(pLev);
		if(lev > 0)
			break;
	}
	fclose(fp);
	return lev;
}


/* chk_asm1042_fw_ver:
 *
 * < 0: error
 * = 0: same
 * > 0: different and need upgrade
 */
static int chk_asm1042_fw_ver(void)
{
	char * const argv[] = { ASM1042_DIR "/" ASM1042_TOOL, "-D", NULL };
	int ret;
	FILE *fp;
	char buf[512];
	char *pHead, *pVer;

	ret = _eval(argv, ">" ASM1042_OUT, 0, NULL);
	if(ret)
	{
		_dprintf(ASM1042_TOOL " -D: ret(%d)\n", ret);
		return -1;
	}

	if((fp = fopen(ASM1042_OUT, "rb")) == NULL)
	{
		_dprintf("fopen(%s) fail: %s\n", ASM1042_OUT, strerror(errno));
		return -1;
	}

	ret = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);

	if(ret <= 0)
	{
		_dprintf("fread() fail ret(%d)\n", ret);
		return -1;
	}

	if((pHead = strstr(buf, ASM1042_VER_HEAD)) == NULL)
	{
		_dprintf("\"%s\" not found\n", ASM1042_VER_HEAD);
		return -1;
	}

	pVer = pHead + strlen(ASM1042_VER_HEAD);
	while(*pVer != '\0' && isspace(*pVer))
		pVer++;

	if(strncmp(pVer, ASM1042_FW, strlen(ASM1042_FW) - 4) == 0)
		return 0;

	_dprintf("== ASM1042 FW NEED UPGRADE ==\n");
	return 1;
}

static void do_asm1042_fw_upgrade(void)
{
	char * const argv[] = { ASM1042_DIR "/" ASM1042_TOOL, "-U", ASM1042_DIR "/" ASM1042_FW, NULL };
	int ret;

	ret = _eval(argv, ">>" ASM1042_OUT, 0, NULL);
	if(ret)
	{
		_dprintf(ASM1042_TOOL " -U " ASM1042_FW ": fail %d, %s\n", ret, strerror(ret));
		return;
	}
	_dprintf("== ASM1042 FW UPGRADE SUCCESS ==\n");
}

void asm1042_upgrade(int skip_usb_dev)
{
	if(nvram_match("skip_asm1042_upgrade", "1"))
		return;
	if(skip_usb_dev == 0 && chk_usb_devices() != 0)
		return;
	if(chk_asm1042_fw_ver() <= 0)
		return;
	do_asm1042_fw_upgrade();
}


