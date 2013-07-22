/*
 * Dump JFFS filesystem.
 * Useful when it buggers up.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/types.h>
#include <asm/byteorder.h>

#include "common.h"

#define BLOCK_SIZE 1024
#define JFFS_MAGIC 0x34383931 /* "1984" */
#define JFFS_MAX_NAME_LEN 256
#define JFFS_MIN_INO 1
#define JFFS_TRACE_INDENT 4
#define JFFS_ALIGN_SIZE 4
#define MAX_CHUNK_SIZE 32768

/* How many padding bytes should be inserted between two chunks of data
   on the flash?  */
#define JFFS_GET_PAD_BYTES(size) ((JFFS_ALIGN_SIZE                     \
			- ((uint32_t)(size) % JFFS_ALIGN_SIZE)) \
		% JFFS_ALIGN_SIZE)

#define JFFS_EMPTY_BITMASK 0xffffffff
#define JFFS_MAGIC_BITMASK 0x34383931
#define JFFS_DIRTY_BITMASK 0x00000000

struct jffs_raw_inode
{
	uint32_t magic;    /* A constant magic number.  */
	uint32_t ino;      /* Inode number.  */
	uint32_t pino;     /* Parent's inode number.  */
	uint32_t version;  /* Version number.  */
	uint32_t mode;     /* file_type, mode  */
	uint16_t uid;
	uint16_t gid;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
	uint32_t offset;     /* Where to begin to write.  */
	uint32_t dsize;      /* Size of the file data.  */
	uint32_t rsize;      /* How much are going to be replaced?  */
	uint8_t nsize;       /* Name length.  */
	uint8_t nlink;       /* Number of links.  */
	uint8_t spare : 6;   /* For future use.  */
	uint8_t rename : 1;  /* Is this a special rename?  */
	uint8_t deleted : 1; /* Has this file been deleted?  */
	uint8_t accurate;    /* The inode is obsolete if accurate == 0.  */
	uint32_t dchksum;    /* Checksum for the data.  */
	uint16_t nchksum;    /* Checksum for the name.  */
	uint16_t chksum;     /* Checksum for the raw_inode.  */
};


struct jffs_file
{
	struct jffs_raw_inode inode;
	char *name;
	unsigned char *data;
};


char *root_directory_name = NULL;
int fs_pos = 0;
int verbose = 0;

#define ENDIAN_HOST   0
#define ENDIAN_BIG    1
#define ENDIAN_LITTLE 2
int endian = ENDIAN_HOST;

static uint32_t jffs_checksum(void *data, int size);
void jffs_print_trace(const char *path, int depth);
int make_root_dir(FILE *fs, int first_ino, const char *root_dir_path,
		int depth);
void write_file(struct jffs_file *f, FILE *fs, struct stat st);
void read_data(struct jffs_file *f, const char *path, int offset);
int mkfs(FILE *fs, const char *path, int ino, int parent, int depth);


	static uint32_t
jffs_checksum(void *data, int size)
{
	uint32_t sum = 0;
	uint8_t *ptr = (uint8_t *)data;

	while (size-- > 0)
	{
		sum += *ptr++;
	}

	return sum;
}


	void
jffs_print_trace(const char *path, int depth)
{
	int path_len = strlen(path);
	int out_pos = depth * JFFS_TRACE_INDENT;
	int pos = path_len - 1;
	char *out = (char *)alloca(depth * JFFS_TRACE_INDENT + path_len + 1);

	if (verbose >= 2)
	{
		fprintf(stderr, "jffs_print_trace(): path: \"%s\"\n", path);
	}

	if (!out) {
		fprintf(stderr, "jffs_print_trace(): Allocation failed.\n");
		fprintf(stderr, " path: \"%s\"\n", path);
		fprintf(stderr, "depth: %d\n", depth);
		exit(1);
	}

	memset(out, ' ', depth * JFFS_TRACE_INDENT);

	if (path[pos] == '/')
	{
		pos--;
	}
	while (path[pos] && (path[pos] != '/'))
	{
		pos--;
	}
	for (pos++; path[pos] && (path[pos] != '/'); pos++)
	{
		out[out_pos++] = path[pos];
	}
	out[out_pos] = '\0';
	fprintf(stderr, "%s\n", out);
}


/* Print the contents of a raw inode.  */
	void
jffs_print_raw_inode(struct jffs_raw_inode *raw_inode)
{
	fprintf(stdout, "jffs_raw_inode: inode number: %u, version %u\n", raw_inode->ino, raw_inode->version);
	fprintf(stdout, "{\n");
	fprintf(stdout, "        0x%08x, /* magic  */\n", raw_inode->magic);
	fprintf(stdout, "        0x%08x, /* ino  */\n", raw_inode->ino);
	fprintf(stdout, "        0x%08x, /* pino  */\n", raw_inode->pino);
	fprintf(stdout, "        0x%08x, /* version  */\n", raw_inode->version);
	fprintf(stdout, "        0x%08x, /* mode  */\n", raw_inode->mode);
	fprintf(stdout, "        0x%04x,     /* uid  */\n", raw_inode->uid);
	fprintf(stdout, "        0x%04x,     /* gid  */\n", raw_inode->gid);
	fprintf(stdout, "        0x%08x, /* atime  */\n", raw_inode->atime);
	fprintf(stdout, "        0x%08x, /* mtime  */\n", raw_inode->mtime);
	fprintf(stdout, "        0x%08x, /* ctime  */\n", raw_inode->ctime);
	fprintf(stdout, "        0x%08x, /* offset  */\n", raw_inode->offset);
	fprintf(stdout, "        0x%08x, /* dsize  */\n", raw_inode->dsize);
	fprintf(stdout, "        0x%08x, /* rsize  */\n", raw_inode->rsize);
	fprintf(stdout, "        0x%02x,       /* nsize  */\n", raw_inode->nsize);
	fprintf(stdout, "        0x%02x,       /* nlink  */\n", raw_inode->nlink);
	fprintf(stdout, "        0x%02x,       /* spare  */\n",
			raw_inode->spare);
	fprintf(stdout, "        %u,          /* rename  */\n",
			raw_inode->rename);
	fprintf(stdout, "        %u,          /* deleted  */\n",
			raw_inode->deleted);
	fprintf(stdout, "        0x%02x,       /* accurate  */\n",
			raw_inode->accurate);
	fprintf(stdout, "        0x%08x, /* dchksum  */\n", raw_inode->dchksum);
	fprintf(stdout, "        0x%04x,     /* nchksum  */\n", raw_inode->nchksum);
	fprintf(stdout, "        0x%04x,     /* chksum  */\n", raw_inode->chksum);
	fprintf(stdout, "}\n");
}

static void write_val32(uint32_t *adr, uint32_t val)
{
	switch(endian) {
		case ENDIAN_HOST:
			*adr = val;
			break;
		case ENDIAN_LITTLE:
			*adr = __cpu_to_le32(val);
			break;
		case ENDIAN_BIG:
			*adr = __cpu_to_be32(val);
			break;
	}
}

static void write_val16(uint16_t *adr, uint16_t val)
{
	switch(endian) {
		case ENDIAN_HOST:
			*adr = val;
			break;
		case ENDIAN_LITTLE:
			*adr = __cpu_to_le16(val);
			break;
		case ENDIAN_BIG:
			*adr = __cpu_to_be16(val);
			break;
	}
}

static uint32_t read_val32(uint32_t *adr)
{
	uint32_t val;

	switch(endian) {
		case ENDIAN_HOST:
			val = *adr;
			break;
		case ENDIAN_LITTLE:
			val = __le32_to_cpu(*adr);
			break;
		case ENDIAN_BIG:
			val = __be32_to_cpu(*adr);
			break;
	}
	return val;
}

static uint16_t read_val16(uint16_t *adr)
{
	uint16_t val;

	switch(endian) {
		case ENDIAN_HOST:
			val = *adr;
			break;
		case ENDIAN_LITTLE:
			val = __le16_to_cpu(*adr);
			break;
		case ENDIAN_BIG:
			val = __be16_to_cpu(*adr);
			break;
	}
	return val;
}

	int
main(int argc, char **argv)
{
	int fs;
	struct stat sb;
	uint32_t wordbuf;
	off_t pos = 0;
	off_t end;
	struct jffs_raw_inode ino;
	unsigned char namebuf[4096];
	int myino = -1;

	if (argc < 2) {
		printf("no filesystem given\n");
		exit(1);
	}

	fs = open(argv[1], O_RDONLY);
	if (fs < 0) {
		perror("open");
		exit(1);
	}

	if (argc > 2) {
		myino = atol(argv[2]);
		printf("Printing ino #%d\n" , myino);
	}

	if (fstat(fs, &sb) < 0) {
		perror("stat");
		close(fs);
		exit(1);
	}
	end = sb.st_size;

	while (pos < end) {
		if (pread(fs, &wordbuf, 4, pos) < 0) {
			perror("pread");
			exit(1);
		}

		switch(wordbuf) {
			case JFFS_EMPTY_BITMASK:
				//			printf("0xff started at 0x%lx\n", pos);
				for (; pos < end && wordbuf == JFFS_EMPTY_BITMASK; pos += 4) {
					if (pread(fs, &wordbuf, 4, pos) < 0) {
						perror("pread");
						exit(1);
					}
				}
				if (pos < end)
					pos -= 4;
				//			printf("0xff ended at 0x%lx\n", pos);
				continue;

			case JFFS_DIRTY_BITMASK:
				//			printf("0x00 started at 0x%lx\n", pos);
				for (; pos < end && wordbuf == JFFS_DIRTY_BITMASK; pos += 4) {
					if (pread(fs, &wordbuf, 4, pos) < 0) {
						perror("pread");
						exit(1);
					}
				}
				if (pos < end)
					pos -=4;
				//			printf("0x00 ended at 0x%lx\n", pos);
				continue;

			default:
				printf("Argh. Dirty memory at 0x%lx\n", pos);
				//			file_hexdump(fs, pos, 128);
				for (pos += 4; pos < end; pos += 4) {
					if (pread(fs, &wordbuf, 4, pos) < 0) {
						perror("pread");
						exit(1);
					}
					if (wordbuf == JFFS_MAGIC_BITMASK)
						break;
				}

			case JFFS_MAGIC_BITMASK:
				if (pread(fs, &ino, sizeof(ino), pos) < 0) {
					perror("pread");
					exit(1);
				}
				if (myino == -1 || ino.ino == myino) {
					printf("Magic found at 0x%lx\n", pos);
					jffs_print_raw_inode(&ino);
				}
				pos += sizeof(ino);

				if (myino == -1 || ino.ino == myino) {
					if (ino.nsize) {
						if (pread(fs, namebuf, min(ino.nsize, 4095), pos) < 0) {
							perror("pread");
							exit(1);
						}
						if (ino.nsize < 4095)
							namebuf[ino.nsize] = 0;
						else
							namebuf[4095] = 0;
						printf("Name: \"%s\"\n", namebuf);
					} else {
						printf("No Name\n");
					}
				}
				pos += (ino.nsize + 3) & ~3;

				pos += (ino.dsize + 3) & ~3;
		}



	}
}
