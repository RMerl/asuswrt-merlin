#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>

#define ROUNDUP(n, a)	((n + (a - 1)) & ~(a - 1))

#define TRX_MAGIC		0x30524448
#define TRX_MAX_OFFSET		4
#define TRX_MAX_LEN		((64 * 1024 * 1024) - ((256 + 128) * 1024))		// 64MB - (256K cfe + 128K cfg)

typedef struct {
	uint32_t magic;
	uint32_t length;
	uint32_t crc32;
	uint32_t flag_version;
	uint32_t offsets[TRX_MAX_OFFSET];
} trx_t;

char trx_version = 1;
int trx_max_offset = 3;

uint32_t *crc_table = NULL;
trx_t *trx = NULL;
int trx_final = 0;
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
		"Usage: -i <input> {output}\n"
		"Output:\n"
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

	if ((f = fopen(fname, "r")) == NULL) {
		perror(fname);
		exit(1);
	}

	if (fread((char *)trx, st.st_size, 1, f) != 1) {
		perror(fname);
		exit(1);
	}
	fclose(f);
}

void finalize_trx(void)
{
	uint32_t len;

	if (trx_final) return;
	trx_final = 1;

	len = trx->length;

	trx->magic = TRX_MAGIC;
	trx->flag_version = trx_version << 16;
	trx->crc32 = crc_calc(0xFFFFFFFF, (void *)&trx->flag_version,
		trx->length - (sizeof(*trx) - (sizeof(trx->flag_version) + sizeof(trx->offsets))));
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
	int o;
	unsigned l;

	printf("\n");
	
	if ((!crc_init()) || ((trx = calloc(1, TRX_MAX_LEN)) == NULL)) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	trx->length = trx_header_size();

	while ((o = getopt(argc, argv, "v:i:a:t:l:m:r:")) != -1) {
		switch (o) {
		case 'i':
			load_image(optarg);
			break;
		case 'r':	
			create_asus(optarg);
			break;
		default:
			help();
			return 1;
		}
	}

	finalize_trx();
	l = trx->length - trx_header_size();
	printf("\nTRX Image:\n");
	printf(" Total Size .... : %u (%.1f KB) (%.1f MB)\n", trx->length, trx->length / 1024.0, trx->length / 1024.0 / 1024.0);
	printf(" CRC-32 ........ : %8X\n", trx->crc32);
	printf("\n");

	return 0;
}
