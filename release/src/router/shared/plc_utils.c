#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <bcmnvram.h>

#include <rtconfig.h>
#include <flash_mtd.h>
#include <shutils.h>
#include <shared.h>
#include <plc_utils.h>

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

/*
 * Convert PLC Key (e.g. NMK, DAK) string representation to binary data
 * @param	a	string in xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx notation
 * @param	e	binary data
 * @return	TRUE if conversion was successful and FALSE otherwise
 */
int
key_atoe(const char *a, unsigned char *e)
{
	char *c = (char *) a;
	int i = 0;

	memset(e, 0, PLC_KEY_LEN);
	for (;;) {
		e[i++] = (unsigned char) strtoul(c, &c, 16);
		if (!*c++ || i == PLC_KEY_LEN)
			break;
	}
	return (i == PLC_KEY_LEN);
}

/*
 * Convert PLC Key binary data to string representation
 * @param	e	binary data
 * @param	a	string in xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx notation
 * @return	a
 */
char *
key_etoa(const unsigned char *e, char *a)
{
	char *c = a;
	int i;

	for (i = 0; i < PLC_KEY_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", e[i] & 0xff);
	}
	return a;
}

/*
 * increase n to mac last 24 bits and handle a carry problem
 */
static void inc_mac(unsigned char n, unsigned char *mac)
{
	int c = 0;

	//dbg("MAC + %u\n", n);

	if (mac[5] >= (0xff - n + 1))
		c = 1;
	else
		c = 0;
	mac[5] += n;

	if (c == 1) {
		if (mac[4] >= 0xff)
			c = 1;
		else
			c = 0;
		mac[4] += 1;

		if (c == 1)
			mac[3] += 1;
	}
}

/*
 * check PLC MAC/Key
 * reference isValidMacAddr() in rc/ate.c
 */
int isValidPara(const char* str, int len)
{
	#define MULTICAST_BIT	0x0001
	#define UNIQUE_OUI_BIT	0x0002
	int sec_byte;
	int i = 0, s = 0;

	if (strlen(str) != ((len * 2) + len - 1))
		return 0;

	while (*str && i < (len * 2)) {
		if (isxdigit(*str)) {
			if (i == 1 && len == ETHER_ADDR_LEN) {
				sec_byte = strtol(str, NULL, 16);
				if ((sec_byte & MULTICAST_BIT) || (sec_byte & UNIQUE_OUI_BIT))
					break;
			}
			i++;
		}
		else if (*str == ':') {
			if (i == 0 || i/2-1 != s)
				break;
			++s;
		}
		++str;
	}
	return (i == (len * 2) && s == (len - 1));
}

static int __getPLC_para(char *ebuf, int addr)
{
	int len;

	if (addr == OFFSET_PLC_MAC)
		len = ETHER_ADDR_LEN;
	else if (addr == OFFSET_PLC_NMK)
		len = PLC_KEY_LEN;
	else
		return 0;

	memset(ebuf, 0, sizeof(ebuf));

	if (FRead(ebuf, addr, len) < 0) {
		dbg("READ PLC parameter: Out of scope\n");
		return 0;
	}

	return 1;
}

/*
 * get PLC MAC from factory partition
 */
int getPLC_MAC(char *abuf)
{
	unsigned char ebuf[ETHER_ADDR_LEN];

	if (__getPLC_para(ebuf, OFFSET_PLC_MAC)) {
		memset(abuf, 0, sizeof(abuf));
		if (ether_etoa(ebuf, abuf))
			return 1;
	}

	return 0;
}

/*
 * get PLC NMK from factory partition
 */
int getPLC_NMK(char *abuf)
{
	unsigned char ebuf[PLC_KEY_LEN];

	if (__getPLC_para(ebuf, OFFSET_PLC_NMK)) {
		memset(abuf, 0, sizeof(abuf));
		if (key_etoa(ebuf, abuf))
			return 1;
	}

	return 0;
}

/*
 * ATE get PLC password from MAC
 */
static int __getPLC_PWD(unsigned char *emac, char *pwd)
{
	FILE *fp;
	int len;
	char cmd[64], buf[32];

	inc_mac(2, emac);
	sprintf(cmd, "/usr/local/bin/mac2pw -q %02x%02x%02x%02x%02x%02x", emac[0], emac[1], emac[2], emac[3], emac[4], emac[5]);
	fp = popen(cmd, "r");
	if (fp) {
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len - 1] = '\0';
			strcpy(pwd, buf);
		}
		else
			return 0;
	}
	else
		return 0;

	return 1;
}

int getPLC_PWD(void)
{
	unsigned char ebuf[ETHER_ADDR_LEN];
	char pwd[32];

	if (__getPLC_para(ebuf, OFFSET_PLC_MAC)) {
		memset(pwd, 0, sizeof(pwd));
		if (!__getPLC_PWD(ebuf, pwd))
			return 0;

		puts(pwd);
	}
	else
		return 0;

	return 1;
}

/*
 * ATE get/set PLC parameter from/to factory partition, e.g. MAC, NMK
 */
int getPLC_para(int addr)
{
	char abuf[64], ebuf[16];


	if (__getPLC_para(ebuf, addr)) {
		memset(abuf, 0, sizeof(abuf));
		if (addr == OFFSET_PLC_MAC)
			ether_etoa(ebuf, abuf);
		else
			key_etoa(ebuf, abuf);
		puts(abuf);
	}
	else
		return 0;

	return 1;
}

int setPLC_para(const char *abuf, int addr)
{
	unsigned char ebuf[32];
	int len, ret;

	if (abuf == NULL)
		return 0;

	memset(ebuf, 0, sizeof(ebuf));

	if (addr == OFFSET_PLC_MAC) {
		len = ETHER_ADDR_LEN;
		if (!isValidPara(abuf, len))
			return 0;

		ret = ether_atoe(abuf, ebuf);
	} else if (addr == OFFSET_PLC_NMK) {
		len = PLC_KEY_LEN;
		if (!isValidPara(abuf, len))
			return 0;

		ret = key_atoe(abuf, ebuf);
	} else
		return 0;

	if (ret) {
		FWrite(ebuf, addr, len);
		getPLC_para(addr);
		return 1;
	}
	else
		return 0;
}

/*
 * modify the value of specific offset
 */
static int modify_pib_byte(unsigned int offset, unsigned int value)
{
	return doSystem("/usr/local/bin/setpib -x %s 0x%x data %02x", BOOT_PIB_PATH, offset, value);
}

/*
 * disable all LED event of PLC except Power event
 */
void ate_ctl_plc_led(void)
{
	int i = 0;

	for (i = 0; i < 18; i++) {
#if defined(RTCONFIG_AR7420)
		if (i == 7) /* Power event */
			continue;
		modify_pib_byte((0x1B13 + (8 * i)), 0x1);
#elif defined(RTCONFIG_QCA7500)
		if (i == 11) /* Power event */
			continue;
		modify_pib_byte((0x21A7 + (8 * i)), 0x1);
#endif
	}
}

/*
 * ATE turn on/off all LED of PLC
 */
int set_plc_all_led_onoff(int on)
{
	if (on) {
#if defined(RTCONFIG_AR7420)
		modify_pib_byte(0x1B49, 0x61);	/* GPIO 0, 5, 6 */
#elif defined(RTCONFIG_QCA7500)
		modify_pib_byte(0x21FD, 0xC0);	/* GPIO 6, 7 */
		modify_pib_byte(0x21FE, 0x0);
#endif
	}
	else {
#if defined(RTCONFIG_AR7420)
		modify_pib_byte(0x1B49, 0x0);
#elif defined(RTCONFIG_QCA7500)
		modify_pib_byte(0x21FD, 0x0);
		modify_pib_byte(0x21FE, 0x02);	/* GPIO 9 */
#endif
	}

	system("/usr/local/bin/plctool -i br0 -R -e > /dev/null");

	return 0;
}


/*
 * backup user's .nvm and .pib mechanism
 */
#define PLC_MAGIC		0x27051956	/* PLC Image Magic Number */
#define DEFAULT_NVM_PATH	"/usr/local/bin/asus.nvm"
#define DEFAULT_PIB_PATH	"/usr/local/bin/asus.pib"
#define USER_NVM_PATH		"/tmp/user.nvm"
#define USER_PIB_PATH		"/tmp/user.pib"
#define HDR_PATH		"/tmp/plc.hdr"
#define IMAGE_PATH		"/tmp/plc.img"
#define RD_SIZE			65536

#define PLC_MTD_NAME		"plc"
#define PLC_MTD_DEV		"mtd5"

#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

typedef struct _plc_image_header {
	unsigned int	magic;		/* PLC Image Header Magic Number */
	unsigned int	hdr_crc;	/* PLC Image Header crc checksum */
	unsigned int	nvm_size;	/* PLC .nvm size */
	unsigned int	nvm_crc;	/* PLC .nvm crc checksum */
	unsigned int	pib_size;	/* PLC .pib size */
	unsigned int	pib_crc;	/* PLC .pib crc checksum */
} plc_image_header;

plc_image_header header;

/*
 * check crc of header
 *
 * : input,  file name
 *
 * return
 * 1: match
 * 0: mismatch or fail
 */
static int hdr_match_crc(plc_image_header *hdr)
{
	unsigned int checksum, checksum_org;

	checksum_org = hdr->hdr_crc;
	hdr->hdr_crc = 0;
	checksum = crc_calc(0, (const char *)hdr, sizeof(plc_image_header));
	fprintf(stderr, "%s: crc %x/%x of header\n", __func__, checksum_org, checksum);

	if (checksum != checksum_org) {
		fprintf(stderr, "%s: header crc mismatch!\n", __func__);
		return 0;
	}

	return 1;
}

/*
 * get file length and crc
 *
 * fname: input,  file name
 * len:   output, file length
 * crc:   output, file crc
 */
static int get_crc(char* fname, unsigned int *len, unsigned int *crc)
{
	int fd, ret = -1;
	struct stat fs;
	unsigned char *ptr = NULL;

	if ((fd = open(fname, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, fname);
		goto open_fail;
	}

	if (fstat(fd, &fs) < 0) {
		fprintf(stderr, "%s: Can't stat %s\n", __func__, fname);
		goto checkcrc_fail;
	}
	*len = fs.st_size;

	ptr = (unsigned char *)mmap(0, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (ptr == (unsigned char *)MAP_FAILED) {
		fprintf(stderr, "%s: Can't map %s\n", __func__, fname);
		goto checkcrc_fail;
	}
	*crc = crc_calc(0, (const char *)ptr, fs.st_size);

	ret = 0;

checkcrc_fail:
	if (ptr != NULL)
		munmap(ptr, fs.st_size);
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void)fdatasync(fd);
#else
	(void)fsync(fd);
#endif
	close(fd);

open_fail:
	return ret;
}

/*
 * check crc of file and header record
 *
 * fname: input,  file name
 * crc:   input,  header record
 *
 * return
 * 1: match
 * 0: mismatch or fail
 */
static int match_crc(char *fname, unsigned int crc)
{
	unsigned int len, checksum;

	if (get_crc(fname, &len, &checksum)) {
		fprintf(stderr, "%s: Can't check crc of %s\n", __func__, fname);
		return 0;
	}
	fprintf(stderr, "%s: crc %x/%x of %s\n", __func__, crc, checksum, fname);

	if (checksum != crc) {
		fprintf(stderr, "%s: %s crc mismatch!\n", __func__, fname);
		return 0;
	}

	return 1;
}

/*
 * read .nvm and .pib from flash
 */
static int plc_read_from_flash(char *nvm_path, char *pib_path)
{
	int rfd, wfd, ret = -1;
	unsigned int rlen, wlen;
	plc_image_header *hdr = &header;
	char cmd[32], buf[RD_SIZE];

	sprintf(cmd, "cat /dev/%s > %s", PLC_MTD_DEV, IMAGE_PATH);
	system(cmd);

	memset(hdr, 0, sizeof(plc_image_header));
	// header
	if ((rfd = open(IMAGE_PATH, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, IMAGE_PATH);
		goto open_fail;
	}
	rlen = read(rfd, hdr, sizeof(plc_image_header));
	// check header crc
	if (hdr_match_crc(hdr) == 0)
		goto mismatch;

	memset(buf, 0, sizeof(buf));
	// .nvm
	wlen = hdr->nvm_size;
	if ((wfd = open(nvm_path, O_RDWR|O_CREAT, 0666)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, nvm_path);
		goto mismatch;
	}
	while (wlen > 0) {
		if (wlen > RD_SIZE)
			rlen = read(rfd, buf, RD_SIZE);
		else
			rlen = read(rfd, buf, wlen);
		write(wfd, buf, rlen);
		wlen -= rlen;
	}
	close(wfd);
	// check .nvm crc
	if (match_crc(nvm_path, hdr->nvm_crc) == 0)
		goto mismatch;

	// .pib
	wlen = hdr->pib_size;
	if ((wfd = open(pib_path, O_RDWR|O_CREAT, 0666)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, pib_path);
		goto mismatch;
	}
	while (wlen > 0) {
		if (wlen > RD_SIZE)
			rlen = read(rfd, buf, RD_SIZE);
		else
			rlen = read(rfd, buf, wlen);
		write(wfd, buf, rlen);
		wlen -= rlen;
	}
	close(wfd);
	// check .pib crc
	if (match_crc(pib_path, hdr->pib_crc) == 0)
		goto mismatch;

	ret = 0;

mismatch:
	close(rfd);

open_fail:
	unlink(IMAGE_PATH);

	return ret;
}

/*
 * write .nvm and .pib to flash
 */
static int plc_write_to_flash(char *nvm_path, char *pib_path)
{
	int fd;
	plc_image_header *hdr = &header;
	char cmd[128];

	memset(hdr, 0, sizeof(plc_image_header));
	hdr->magic = PLC_MAGIC;

	// get length and crc of .nvm
	if (get_crc(nvm_path, &hdr->nvm_size, &hdr->nvm_crc)) {
		fprintf(stderr, "%s: Can't check crc of %s\n", __func__, nvm_path);
		return -1;
	}

	// get length and crc of .pib
	if (get_crc(pib_path, &hdr->pib_size, &hdr->pib_crc)) {
		fprintf(stderr, "%s: Can't check crc of %s\n", __func__, pib_path);
		return -1;
	}

	// create header
	hdr->hdr_crc = crc_calc(0, (const char *)hdr, sizeof(plc_image_header));
	if ((fd = open(HDR_PATH, O_RDWR|O_CREAT, 0666)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, HDR_PATH);
		return -1;
	}
	write(fd, hdr, sizeof(plc_image_header));
	close(fd);

	// write to plc partition
	sprintf(cmd, "cat %s %s %s > %s", HDR_PATH, nvm_path, pib_path, IMAGE_PATH);
	system(cmd);
	sprintf(cmd, "mtd-write -i %s -d %s", IMAGE_PATH, PLC_MTD_NAME);
	system(cmd);

	unlink(HDR_PATH);
	unlink(IMAGE_PATH);

	return 0;
}

/*
 * check .nvm and .pib of plc partition
 *
 * return
 * 1: vaild
 * 0: invaild or fail
 */
int vaild_plc_partition(void)
{
	int fd, ret = 0;
	plc_image_header *hdr = &header;
	char cmd[32];

	sprintf(cmd, "cat /dev/%s > %s", PLC_MTD_DEV, IMAGE_PATH);
	system(cmd);

	memset(hdr, 0, sizeof(plc_image_header));
	// header
	if ((fd = open(IMAGE_PATH, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Can't open %s\n", __func__, IMAGE_PATH);
		goto open_fail;
	}
	read(fd, hdr, sizeof(plc_image_header));

	// check header crc
	if (hdr_match_crc(hdr) == 0)
		goto mismatch;

	if (hdr->magic == PLC_MAGIC)
		ret = 1;

mismatch:
	close(fd);

open_fail:
	unlink(IMAGE_PATH);

	return ret;
}

/*
 * write default .nvm and .pib to flash, if plc partition is empty.
 * reload .nvm and .pib to /tmp from flash for plchost utility
 */
int load_plc_setting(void)
{
	if (vaild_plc_partition() == 0) {
		FILE *fp;
		int len, i;
		char cmd[64], buf[64];
		char mac[18], dak[48], nmk[48];
		unsigned char emac[ETHER_ADDR_LEN], enmk[PLC_KEY_LEN];

		doSystem("cp %s %s", DEFAULT_NVM_PATH, BOOT_NVM_PATH);
		doSystem("cp %s %s", DEFAULT_PIB_PATH, BOOT_PIB_PATH);

		// modify .pib
		// MAC
		if (!__getPLC_para(emac, OFFSET_PLC_MAC)) {
			_dprintf("READ PLC MAC: Out of scope\n");
		}
		else {
			if (emac[0] != 0xff) {
				if (ether_etoa(emac, mac))
					doSystem("/usr/local/bin/modpib %s -M %s", BOOT_PIB_PATH, mac);

				// DAK
				if (__getPLC_PWD(emac, buf)) {
					sprintf(cmd, "/usr/local/bin/hpavkey -D %s", buf);
					fp = popen(cmd, "r");
					if (fp) {
						len = fread(buf, 1, sizeof(buf), fp);
						pclose(fp);
						if (len > 1) {
							buf[len - 1] = '\0';

							for (i = 0; i < PLC_KEY_LEN; i++) {
								if (i == 0)
									sprintf(dak, "%c%c", buf[0], buf[1]);
								else
									sprintf(dak, "%s:%c%c", dak, buf[i*2], buf[i*2+1]);
							}
							doSystem("/usr/local/bin/modpib %s -D %s", BOOT_PIB_PATH, dak);
						}
					}
				}
			}
		}

		// NMK
		if (!__getPLC_para(enmk, OFFSET_PLC_NMK))
			_dprintf("READ PLC NMK: Out of scope\n");
		else {
			if (enmk[0] != 0xff && enmk[1] != 0xff && enmk[2] != 0xff) {
				if (key_etoa(enmk, nmk))
					doSystem("/usr/local/bin/modpib %s -N %s", BOOT_PIB_PATH, nmk);
			}
		}

		if (plc_write_to_flash(BOOT_NVM_PATH, BOOT_PIB_PATH))
			return -1;
	}
	else {
		if (plc_read_from_flash(BOOT_NVM_PATH, BOOT_PIB_PATH))
			return -1;
	}

	return 0;
}

/*
 * set plc_flag nvram for save_plc_setting() function
 *
 * because of plc-utils/plc/plchost.c cannot include bcmnvram.h
 * 	error: conflicting types for 'bool' from
 * 		src-qca/include/typedefs.h
 * 		plc-utils/tools/types.h
 */
void set_plc_flag(int flag)
{
	nvram_set_int("plc_flag", flag);
}

/*
 * write user .nvm or .pib to flash
 * case0: reset to default
 * case1: backup .nvm
 * case2: backup .pib
 * case3: backup .nvm and .pib
 */
void save_plc_setting(void)
{
	switch (atoi(nvram_safe_get("plc_flag"))) {
	case 0:
		plc_write_to_flash(DEFAULT_NVM_PATH, DEFAULT_PIB_PATH);
		break;
	case 1:
		plc_write_to_flash(USER_NVM_PATH, BOOT_PIB_PATH);
		break;
	case 2:
		_dprintf("sleep 10 second for wait pairing done!\n");
		sleep(10);
		plc_write_to_flash(BOOT_NVM_PATH, USER_PIB_PATH);
		break;
	case 3:
		plc_write_to_flash(USER_NVM_PATH, USER_PIB_PATH);
		break;
	default:
		fprintf(stderr, "%s: wrong flag!", __func__);
	}

	set_plc_flag(-1);
}

void turn_led_pwr_off(void)
{
	if (!nvram_match("asus_mfg", "0"))
		return;

	nvram_set("plc_ready", "1");

#if (defined(PLN12) || defined(PLAC56))
	led_control(LED_POWER_RED, LED_OFF);
#endif
}
