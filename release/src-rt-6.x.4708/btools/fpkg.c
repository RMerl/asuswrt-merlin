/*

	fpkg - Package a firmware
	Copyright (C) 2007 Jonathan Zarate

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define ROUNDUP(n, a)	((n + (a - 1)) & ~(a - 1))

#define TRX_MAGIC		0x30524448
#define TRX_MAX_OFFSET		4
#define TRX_MAX_LEN		((32 * 1024 * 1024) - ((256 + 128) * 1024))		// 32MB - (256K cfe + 128K cfg)

typedef struct {
	uint32_t magic;
	uint32_t length;
	uint32_t crc32;
	uint32_t flag_version;
	uint32_t offsets[TRX_MAX_OFFSET];
} trx_t;

char names[TRX_MAX_OFFSET][80];

char trx_version = 1;
int trx_max_offset = 3;


typedef struct {
	uint32_t crc32;
	uint32_t magic;
} moto_t;

typedef struct {
	uint8_t magic[4];
	uint8_t extra1[4];
	uint8_t date[3];
	uint8_t version[3];
	uint8_t u2nd[4];
	uint8_t hardware;
	uint8_t serial;
	uint16_t flags;
	uint8_t extra2[10];
} cytan_t;

uint32_t *crc_table = NULL;
trx_t *trx = NULL;
int trx_count = 0;
int trx_final = 0;
int trx_padding;
time_t max_time = 0;

inline size_t trx_header_size(void)
{
	return sizeof(*trx) - sizeof(trx->offsets) + (trx_max_offset * sizeof(trx->offsets[0]));
}

int crc_init(void)
{
	uint32_t c;
	int i, j;

	if (crc_table == NULL) {
		if ((crc_table = malloc(sizeof(uint32_t) * 256)) == NULL) return 0;
		for (i = 255; i >= 0; --i) {
			c = i;
			for (j = 8; j > 0; --j) {
				if (c & 1) c = (c >> 1) ^ 0xEDB88320L;
					else c >>= 1;
			}
			crc_table[i] = c;
		}
	}
	return 1;
}

void crc_done(void)
{
	free(crc_table);
	crc_table = NULL;
}

uint32_t crc_calc(uint32_t crc, uint8_t *buf, int len)
{
	while (len-- > 0) {
		crc = crc_table[(crc ^ *buf) & 0xFF] ^ (crc >> 8);
		++buf;
	}
	return crc;
}

void help(void)
{
	fprintf(stderr,
		"fpkg - Package a firmware\n"
		"Copyright (C) 2007 Jonathan Zarate\n"
		"\n"
		"Usage: [-v <trx version>] -i <input> [-a <align>] [-i <input>] [-a <align>] {output}\n"
		"Output:\n"
		" TRX:            -t <output file>\n"
		" Linksys/Cisco:  -l <id>,<output file>\n"
		"   W54G WRT54G / WRT54GL\n"
		"   W54U WRTSL54GS\n"
		"   W54S WRTS54GS\n"
		"   W54s WRT54GS v4\n"
		"   N160 WRT160N v3\n"
		"   EWCB WRT300N v1\n"
		"   310N WRT310N v1/v2\n"
		"   320N WRT320N\n"
		"   610N WRT610N v2\n"
		"   32XN E2000\n"
		"   61XN E3000\n"
		" Motorola:       -m <id>,<output file>\n"
		"   0x10577000 WE800\n"
		"   0x10577040 WA840\n"
		"   0x10577050 WR850\n"
		" ASUS: 	  -r <id>,<v1>,<v2>,<v3>,<v4>,<output file>\n"
		"\n"
	);
	exit(1);
}

void load_image(const char *fname)
{
	struct stat st;
	FILE *f;
	long rsize;
	char *p;

	if (trx_final) {
		fprintf(stderr, "Cannot load another image if an output has already been written.\n");
		exit(1);
	}
	if (trx_count >= trx_max_offset) {
		fprintf(stderr, "Too many input files.\n");
		exit(1);
	}

	if (stat(fname, &st) != 0) {
		perror(fname);
		exit(1);
	}
	if (st.st_ctime > max_time) max_time = st.st_ctime;

	rsize = ROUNDUP(st.st_size, 4);
	if ((trx->length + rsize) > TRX_MAX_LEN) {
		fprintf(stderr, "Total size %lu (%.1f KB) is too big. Maximum is %lu (%.1f KB).\n",
			(trx->length + rsize), (trx->length + rsize) / 1024.0,
			(long unsigned int) TRX_MAX_LEN, TRX_MAX_LEN / 1024.0);
		exit(1);
	}

	p = (char *)trx + trx->length;
	if ((f = fopen(fname, "r")) == NULL) {
		perror(fname);
		exit(1);
	}
	if (fread((char *)trx + trx->length, st.st_size, 1, f) != 1) {
		perror(fname);
		exit(1);
	}
	fclose(f);
	strncpy(names[trx_count], fname, sizeof(names[0]) -1);
	trx->offsets[trx_count++] = trx->length;
	trx->length += rsize;
}

void align_trx(const char *align)
{
	uint32_t len;
	size_t n;
	char *e;

	errno = 0;
	n = strtoul(align, &e, 0);
	if (errno || (e == align) || *e) {
		fprintf(stderr, "Illegal numeric string\n");
		help();
	}

	if (trx_final) {
		fprintf(stderr, "Cannot align if an output has already been written.\n");
		exit(1);
	}

	len = ROUNDUP(trx->length, n);
	if (len > TRX_MAX_LEN) {
		fprintf(stderr, "Total size %u (%.1f KB) is too big. Maximum is %lu (%.1f KB).\n",
			len, len / 1024.0, (long unsigned int) TRX_MAX_LEN, TRX_MAX_LEN / 1024.0);
		exit(1);
	}
	trx->length = len;
}

void set_trx_version(const char *ver)
{
	int n;
	char *e;

	errno = 0;
	n = strtoul(ver, &e, 0);
	if (errno || (e == ver) || *e) {
		fprintf(stderr, "Illegal numeric string\n");
		help();
	}

	if (trx_count > 0) {
		fprintf(stderr, "Cannot change trx version after images have already been loaded.\n");
		exit(1);
	}

	if (n < 1 || n > 2) {
		fprintf(stderr, "TRX version %d is not supported.\n", n);
		exit(1);
	}

	trx_version = (char)n;
	switch (trx_version) {
	case 2:
		trx_max_offset = 4;
		break;
	default:
		trx_max_offset = 3;
		break;
	}
	trx->length = trx_header_size();
}

void finalize_trx(void)
{
	uint32_t len;

	if (trx_count == 0) {
		fprintf(stderr, "No image was loaded.\n");
		exit(1);
	}

	if (trx_final) return;
	trx_final = 1;

	len = trx->length;

	trx->length = ROUNDUP(len, 4096);
	trx->magic = TRX_MAGIC;
	trx->flag_version = trx_version << 16;
	trx->crc32 = crc_calc(0xFFFFFFFF, (void *)&trx->flag_version,
		trx->length - (sizeof(*trx) - (sizeof(trx->flag_version) + sizeof(trx->offsets))));

	trx_padding = trx->length - len;
}

void create_trx(const char *fname)
{
	FILE *f;

	finalize_trx();

	printf("Creating TRX: %s\n", fname);

	if (((f = fopen(fname, "w")) == NULL) ||
		(fwrite(trx, trx->length, 1, f) != 1)) {
		perror(fname);
		exit(1);
	}
	fclose(f);
}

void create_cytan(const char *fname, const char *pattern)
{
	FILE *f;
	cytan_t h;
	char padding[1024 - sizeof(h)];
	struct tm *tm;

	if (strlen(pattern) != 4) {
		fprintf(stderr, "Linksys signature must be 4 characters. \"%s\" is invalid.\n", pattern);
		exit(1);
	}

	finalize_trx();

	printf("Creating Linksys %s: %s\n", pattern, fname);

	memset(&h, 0, sizeof(h));
	memcpy(h.magic, pattern, 4);
	memcpy(h.u2nd, "U2ND", 4);
	h.version[0] = 4;		// 4.0.0	should be >= *_VERSION_FROM defined in code_pattern.h
	h.flags = 0xFF;
	tm = localtime(&max_time);
	h.date[0] = tm->tm_year - 100;
	h.date[1] = tm->tm_mon + 1;
	h.date[2] = tm->tm_mday;

	memset(padding, 0, sizeof(padding));

	if (((f = fopen(fname, "w")) == NULL) ||
		(fwrite(&h, sizeof(h), 1, f) != 1) ||
		(fwrite(trx, trx->length, 1, f) != 1) ||
		(fwrite(padding, sizeof(padding), 1, f) != 1)) {
		perror(fname);
		exit(1);
	}
	fclose(f);
}

void create_moto(const char *fname, const char *signature)
{
	FILE *f;
	moto_t h;
	char *p;

	h.magic = strtoul(signature, &p, 0);
	if (*p != 0) help();

	finalize_trx();

	printf("Creating Motorola 0x%08X: %s\n", h.magic, fname);

	h.magic = htonl(h.magic);
	h.crc32 = crc_calc(0xFFFFFFFF, (void *)&h.magic, sizeof(h.magic));
	h.crc32 = htonl(crc_calc(h.crc32, (void *)trx, trx->length));

	if (((f = fopen(fname, "w")) == NULL) ||
		(fwrite(&h, sizeof(h), 1, f) != 1) ||
		(fwrite(trx, trx->length, 1, f) != 1)) {
		perror(fname);
		exit(1);
	}
	fclose(f);
}

#define MAX_STRING 12
#define MAX_VER 4	

typedef struct {
	uint8_t major;
	uint8_t minor;
} version_t;

typedef struct {
	version_t kernel;
	version_t fs;
	char 	  productid[MAX_STRING];
	version_t hw[MAX_VER*2];
	char	  pad[32];
} TAIL;

/* usage:
 * -r <productid>,<version>,<output file>
 *
 */
int create_asus(const char *optarg)
{
	FILE *f;
	char value[320];
	char *next, *pid, *ver, *fname, *p;
	TAIL asus_tail;
	uint32_t len;
	uint32_t v1, v2, v3, v4;

	memset(&asus_tail, 0, sizeof(TAIL));

	strncpy(value, optarg, sizeof(value));
	next = value;
	pid = strsep(&next, ",");
	if(!pid) return 0;
	
	strncpy(&asus_tail.productid[0], pid, MAX_STRING);

	ver = strsep(&next, ",");
	if(!ver) return 0;	
	
	sscanf(ver, "%d.%d.%d.%d", &v1, &v2, &v3, &v4);
	asus_tail.kernel.major = (uint8_t)v1;
	asus_tail.kernel.minor = (uint8_t)v2;
	asus_tail.fs.major = (uint8_t)v3;
	asus_tail.fs.minor = (uint8_t)v4;

	fname = strsep(&next, ",");
	if(!fname) return 0;

	// append version information into the latest offset
	trx->length += sizeof(TAIL);
	len = trx->length;
	trx->length = ROUNDUP(len, 4096);

	p = (char *)trx+trx->length-sizeof(TAIL);
	memcpy(p, &asus_tail, sizeof(TAIL));

	finalize_trx();

	printf("Creating ASUS %s firmware to %s\n", asus_tail.productid, fname);

	if (((f = fopen(fname, "w")) == NULL) ||
		(fwrite(trx, trx->length, 1, f) != 1)) {
		perror(fname);
		exit(1);
	}
	fclose(f);

	return 1;
}

int main(int argc, char **argv)
{
	char s[256];
	char *p;
	int o;
	unsigned l, j;

	printf("\n");
	
	if ((!crc_init()) || ((trx = calloc(1, TRX_MAX_LEN)) == NULL)) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	trx->length = trx_header_size();

	while ((o = getopt(argc, argv, "v:i:a:t:l:m:r:")) != -1) {
		switch (o) {
		case 'v':
			set_trx_version(optarg);
			break;
		case 'i':
			load_image(optarg);
			break;
		case 'a':
			align_trx(optarg);
			break;
		case 't':
			create_trx(optarg);
			break;
		case 'l':
		case 'm':
			if (strlen(optarg) >= sizeof(s)) help();
			strcpy(s, optarg);
			if ((p = strchr(s, ',')) == NULL) help();
			*p = 0;
			++p;
			if (o == 'l') create_cytan(p, s);
				else create_moto(p, s);
			break;
		case 'r':	
			create_asus(optarg);
			break;
		default:
			help();
			return 1;
		}
	}

	if (trx_count == 0) {
		help();
	}
	else {
		finalize_trx();
		l = trx->length - trx_padding - trx_header_size();
		printf("\nTRX Image:\n");
		printf(" Total Size .... : %u (%.1f KB) (%.1f MB)\n", trx->length, trx->length / 1024.0, trx->length / 1024.0 / 1024.0);
		printf("   Images ...... : %u (0x%08x)\n", l , l);
		printf("   Padding ..... : %d\n", trx_padding);

		printf(" Avail. for jffs :\n");

		/* Reserved: 2 EBs for pmon, 1 EB for nvram. */
		l = trx->length;
		if (l < (4 * 1024 * 1024) - (3 * 64 * 1024))
			j = (4 * 1024 * 1024) - (3 * 64 * 1024) - l;
		else
			j = 0;
		printf("   4MB, 128K CFE : %d EBs + %d\n", j / (64*1024), j % (64*1024));

		/* Reserved: 4 EBs for pmon, 1 EB for nvram. */
		if (l < (4 * 1024 * 1024) - (5 * 64 * 1024))
			j = (4 * 1024 * 1024) - (5 * 64 * 1024) - l;
		else
			j = 0;
		printf("   4MB, 256K CFE : %d EBs + %d\n", j / (64*1024), j % (64*1024));

		if (l < (8 * 1024 * 1024) - (5 * 64 * 1024))
			j = (8 * 1024 * 1024) - (5 * 64 * 1024) - l;
		else
			j = 0;
		printf("   8MB, 256K CFE : %d EBs + %d\n", j / (64*1024), j % (64*1024));

		if (l < (32 * 1024 * 1024) - (5 * 64 * 1024))
			j = (32 * 1024 * 1024) - (5 * 64 * 1024) - l;
		else
			j = 0;
		printf("  32MB, 256K CFE : %d EBs + %d\n", j / (64*1024), j % (64*1024));

		printf(" CRC-32 ........ : %8X\n", trx->crc32);
		l = (ROUNDUP(trx->length, (128 * 1024)) / (128 * 1024));
		printf(" 128K Blocks ... : %u (0x%08X)\n", l, l);
		l = (ROUNDUP(trx->length, (64 * 1024)) / (64 * 1024));
		printf("  64K Blocks ... : %u (0x%08X)\n", l, l);
		printf(" Offsets:\n");
		for (o = 0; o < trx_max_offset; ++o) {
		   printf("   %d: 0x%08X  %s\n", o, trx->offsets[o], names[o]);
		}
	}
	printf("\n");
	return 0;
}
