/* vi: set sw=4 ts=4: */
/*
 * jffs2reader v0.0.18 A jffs2 image reader
 *
 * Copyright (c) 2001 Jari Kirma <Jari.Kirma@hut.fi>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 * not claim that you wrote the original software. If you use this
 * software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must
 * not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 *
 *********
 *  This code was altered September 2001
 *  Changes are Copyright (c) Erik Andersen <andersen@codepoet.org>
 *
 * In compliance with (2) above, this is hereby marked as an altered
 * version of this software.  It has been altered as follows:
 *      *) Listing a directory now mimics the behavior of 'ls -l'
 *      *) Support for recursive listing has been added
 *      *) Without options, does a recursive 'ls' on the whole filesystem
 *      *) option parsing now uses getopt()
 *      *) Now uses printf, and error messages go to stderr.
 *      *) The copyright notice has been cleaned up and reformatted
 *      *) The code has been reformatted
 *      *) Several twisty code paths have been fixed so I can understand them.
 *  -Erik, 1 September 2001
 *
 *      *) Made it show major/minor numbers for device nodes
 *      *) Made it show symlink targets
 *  -Erik, 13 September 2001
 */


/*
TODO:

- Add CRC checking code to places marked with XXX.
- Add support for other node compression types.

- Test with real life images.
- Maybe port into bootloader.
 */

/*
BUGS:

- Doesn't check CRC checksums.
 */

#define PROGRAM_NAME "jffs2reader"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <zlib.h>

#include "mtd/jffs2-user.h"
#include "common.h"

#define SCRATCH_SIZE (5*1024*1024)

/* macro to avoid "lvalue required as left operand of assignment" error */
#define ADD_BYTES(p, n)		((p) = (typeof(p))((char *)(p) + (n)))

#define DIRENT_INO(dirent) ((dirent) !=NULL ? je32_to_cpu((dirent)->ino) : 0)
#define DIRENT_PINO(dirent) ((dirent) !=NULL ? je32_to_cpu((dirent)->pino) : 0)

struct dir {
	struct dir *next;
	uint8_t type;
	uint8_t nsize;
	uint32_t ino;
	char name[256];
};

int target_endian = __BYTE_ORDER;

void putblock(char *, size_t, size_t *, struct jffs2_raw_inode *);
struct dir *putdir(struct dir *, struct jffs2_raw_dirent *);
void printdir(char *o, size_t size, struct dir *d, const char *path,
		int recurse, int want_ctime);
void freedir(struct dir *);

struct jffs2_raw_inode *find_raw_inode(char *o, size_t size, uint32_t ino);
struct jffs2_raw_dirent *resolvedirent(char *, size_t, uint32_t, uint32_t,
		char *, uint8_t);
struct jffs2_raw_dirent *resolvename(char *, size_t, uint32_t, char *, uint8_t);
struct jffs2_raw_dirent *resolveinode(char *, size_t, uint32_t);

struct jffs2_raw_dirent *resolvepath0(char *, size_t, uint32_t, const char *,
		uint32_t *, int);
struct jffs2_raw_dirent *resolvepath(char *, size_t, uint32_t, const char *,
		uint32_t *);

void lsdir(char *, size_t, const char *, int, int);
void catfile(char *, size_t, char *, char *, size_t, size_t *);

int main(int, char **);

/* writes file node into buffer, to the proper position. */
/* reading all valid nodes in version order reconstructs the file. */

/*
   b       - buffer
   bsize   - buffer size
   rsize   - result size
   n       - node
 */

void putblock(char *b, size_t bsize, size_t * rsize,
		struct jffs2_raw_inode *n)
{
	uLongf dlen = je32_to_cpu(n->dsize);

	if (je32_to_cpu(n->isize) > bsize || (je32_to_cpu(n->offset) + dlen) > bsize)
		errmsg_die("File does not fit into buffer!");

	if (*rsize < je32_to_cpu(n->isize))
		bzero(b + *rsize, je32_to_cpu(n->isize) - *rsize);

	switch (n->compr) {
		case JFFS2_COMPR_ZLIB:
			uncompress((Bytef *) b + je32_to_cpu(n->offset), &dlen,
					(Bytef *) ((char *) n) + sizeof(struct jffs2_raw_inode),
					(uLongf) je32_to_cpu(n->csize));
			break;

		case JFFS2_COMPR_NONE:
			memcpy(b + je32_to_cpu(n->offset),
					((char *) n) + sizeof(struct jffs2_raw_inode), dlen);
			break;

		case JFFS2_COMPR_ZERO:
			bzero(b + je32_to_cpu(n->offset), dlen);
			break;

			/* [DYN]RUBIN support required! */

		default:
			errmsg_die("Unsupported compression method!");
	}

	*rsize = je32_to_cpu(n->isize);
}

/* adds/removes directory node into dir struct. */
/* reading all valid nodes in version order reconstructs the directory. */

/*
   dd      - directory struct being processed
   n       - node

   return value: directory struct value replacing dd
 */

struct dir *putdir(struct dir *dd, struct jffs2_raw_dirent *n)
{
	struct dir *o, *d, *p;

	o = dd;

	if (je32_to_cpu(n->ino)) {
		if (dd == NULL) {
			d = xmalloc(sizeof(struct dir));
			d->type = n->type;
			memcpy(d->name, n->name, n->nsize);
			d->nsize = n->nsize;
			d->ino = je32_to_cpu(n->ino);
			d->next = NULL;

			return d;
		}

		while (1) {
			if (n->nsize == dd->nsize &&
					!memcmp(n->name, dd->name, n->nsize)) {
				dd->type = n->type;
				dd->ino = je32_to_cpu(n->ino);

				return o;
			}

			if (dd->next == NULL) {
				dd->next = xmalloc(sizeof(struct dir));
				dd->next->type = n->type;
				memcpy(dd->next->name, n->name, n->nsize);
				dd->next->nsize = n->nsize;
				dd->next->ino = je32_to_cpu(n->ino);
				dd->next->next = NULL;

				return o;
			}

			dd = dd->next;
		}
	} else {
		if (dd == NULL)
			return NULL;

		if (n->nsize == dd->nsize && !memcmp(n->name, dd->name, n->nsize)) {
			d = dd->next;
			free(dd);
			return d;
		}

		while (1) {
			p = dd;
			dd = dd->next;

			if (dd == NULL)
				return o;

			if (n->nsize == dd->nsize &&
					!memcmp(n->name, dd->name, n->nsize)) {
				p->next = dd->next;
				free(dd);

				return o;
			}
		}
	}
}


#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])

/* The special bits. If set, display SMODE0/1 instead of MODE0/1 */
static const mode_t SBIT[] = {
	0, 0, S_ISUID,
	0, 0, S_ISGID,
	0, 0, S_ISVTX
};

/* The 9 mode bits to test */
static const mode_t MBIT[] = {
	S_IRUSR, S_IWUSR, S_IXUSR,
	S_IRGRP, S_IWGRP, S_IXGRP,
	S_IROTH, S_IWOTH, S_IXOTH
};

static const char MODE1[] = "rwxrwxrwx";
static const char MODE0[] = "---------";
static const char SMODE1[] = "..s..s..t";
static const char SMODE0[] = "..S..S..T";

/*
 * Return the standard ls-like mode string from a file mode.
 * This is static and so is overwritten on each call.
 */
const char *mode_string(int mode)
{
	static char buf[12];

	int i;

	buf[0] = TYPECHAR(mode);
	for (i = 0; i < 9; i++) {
		if (mode & SBIT[i])
			buf[i + 1] = (mode & MBIT[i]) ? SMODE1[i] : SMODE0[i];
		else
			buf[i + 1] = (mode & MBIT[i]) ? MODE1[i] : MODE0[i];
	}
	return buf;
}

/* prints contents of directory structure */

/*
   d       - dir struct
 */

void printdir(char *o, size_t size, struct dir *d, const char *path, int recurse,
		int want_ctime)
{
	char m;
	char *filetime;
	time_t age;
	struct jffs2_raw_inode *ri;
	jint32_t mode;

	if (!path)
		return;
	if (strlen(path) == 1 && *path == '/')
		path++;

	while (d != NULL) {
		switch (d->type) {
			case DT_REG:
				m = ' ';
				break;

			case DT_FIFO:
				m = '|';
				break;

			case DT_CHR:
				m = ' ';
				break;

			case DT_BLK:
				m = ' ';
				break;

			case DT_DIR:
				m = '/';
				break;

			case DT_LNK:
				m = ' ';
				break;

			case DT_SOCK:
				m = '=';
				break;

			default:
				m = '?';
		}
		ri = find_raw_inode(o, size, d->ino);
		if (!ri) {
			warnmsg("bug: raw_inode missing!");
			d = d->next;
			continue;
		}

		filetime = ctime((const time_t *) &(ri->ctime));
		age = time(NULL) - je32_to_cpu(ri->ctime);
		mode.v32 = ri->mode.m;
		printf("%s %-4d %-8d %-8d ", mode_string(je32_to_cpu(mode)),
				1, je16_to_cpu(ri->uid), je16_to_cpu(ri->gid));
		if ( d->type==DT_BLK || d->type==DT_CHR ) {
			dev_t rdev;
			size_t devsize;
			putblock((char*)&rdev, sizeof(rdev), &devsize, ri);
			printf("%4d, %3d ", major(rdev), minor(rdev));
		} else {
			printf("%9ld ", (long)je32_to_cpu(ri->dsize));
		}
		d->name[d->nsize]='\0';
		if (want_ctime) {
			if (age < 3600L * 24 * 365 / 2 && age > -15 * 60)
				/* hh:mm if less than 6 months old */
				printf("%6.6s %5.5s ", filetime + 4, filetime + 11);
			else
				printf("%6.6s %4.4s ", filetime + 4, filetime + 20);
		}
		printf("%s/%s%c", path, d->name, m);
		if (d->type == DT_LNK) {
			char symbuf[1024];
			size_t symsize;
			putblock(symbuf, sizeof(symbuf), &symsize, ri);
			symbuf[symsize] = 0;
			printf(" -> %s", symbuf);
		}
		printf("\n");

		if (d->type == DT_DIR && recurse) {
			char *tmp;
			tmp = xmalloc(BUFSIZ);
			sprintf(tmp, "%s/%s", path, d->name);
			lsdir(o, size, tmp, recurse, want_ctime);	/* Go recursive */
			free(tmp);
		}

		d = d->next;
	}
}

/* frees memory used by directory structure */

/*
   d       - dir struct
 */

void freedir(struct dir *d)
{
	struct dir *t;

	while (d != NULL) {
		t = d->next;
		free(d);
		d = t;
	}
}

/* collects directory/file nodes in version order. */

/*
   f       - file flag.
   if zero, collect file, compare ino to inode
   otherwise, collect directory, compare ino to parent inode
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - inode to compare against. see f.

   return value: a jffs2_raw_inode that corresponds the the specified
   inode, or NULL
 */

struct jffs2_raw_inode *find_raw_inode(char *o, size_t size, uint32_t ino)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (o + size);
	union jffs2_node_union *lr;	/* last block position */
	union jffs2_node_union *mp = NULL;	/* minimum position */

	uint32_t vmin, vmint, vmaxt, vmax, vcur, v;

	vmin = 0;					/* next to read */
	vmax = ~((uint32_t) 0);		/* last to read */
	vmint = ~((uint32_t) 0);
	vmaxt = 0;					/* found maximum */
	vcur = 0;					/* XXX what is smallest version number used? */
	/* too low version number can easily result excess log rereading */

	n = (union jffs2_node_union *) o;
	lr = n;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_INODE &&
				je32_to_cpu(n->i.ino) == ino && (v = je32_to_cpu(n->i.version)) > vcur) {
				/* XXX crc check */

				if (vmaxt < v)
					vmaxt = v;
				if (vmint > v) {
					vmint = v;
					mp = n;
				}

				if (v == (vcur + 1))
					return (&(n->i));
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			n = (union jffs2_node_union *) o;	/* we're at the end, rewind to the beginning */

		if (lr == n) {			/* whole loop since last read */
			vmax = vmaxt;
			vmin = vmint;
			vmint = ~((uint32_t) 0);

			if (vcur < vmax && vcur < vmin)
				return (&(mp->i));
		}
	} while (vcur < vmax);

	return NULL;
}

/* collects dir struct for selected inode */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   pino    - inode of the specified directory
   d       - input directory structure

   return value: result directory structure, replaces d.
 */

struct dir *collectdir(char *o, size_t size, uint32_t ino, struct dir *d)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (o + size);
	union jffs2_node_union *lr;	/* last block position */
	union jffs2_node_union *mp = NULL;	/* minimum position */

	uint32_t vmin, vmint, vmaxt, vmax, vcur, v;

	vmin = 0;					/* next to read */
	vmax = ~((uint32_t) 0);		/* last to read */
	vmint = ~((uint32_t) 0);
	vmaxt = 0;					/* found maximum */
	vcur = 0;					/* XXX what is smallest version number used? */
	/* too low version number can easily result excess log rereading */

	n = (union jffs2_node_union *) o;
	lr = n;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT &&
				je32_to_cpu(n->d.pino) == ino && (v = je32_to_cpu(n->d.version)) > vcur) {
				/* XXX crc check */

				if (vmaxt < v)
					vmaxt = v;
				if (vmint > v) {
					vmint = v;
					mp = n;
				}

				if (v == (vcur + 1)) {
					d = putdir(d, &(n->d));

					lr = n;
					vcur++;
					vmint = ~((uint32_t) 0);
				}
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			n = (union jffs2_node_union *) o;	/* we're at the end, rewind to the beginning */

		if (lr == n) {			/* whole loop since last read */
			vmax = vmaxt;
			vmin = vmint;
			vmint = ~((uint32_t) 0);

			if (vcur < vmax && vcur < vmin) {
				d = putdir(d, &(mp->d));

				lr = n =
					(union jffs2_node_union *) (((char *) mp) +
							((je32_to_cpu(mp->u.totlen) + 3) & ~3));

				vcur = vmin;
			}
		}
	} while (vcur < vmax);

	return d;
}



/* resolve dirent based on criteria */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - if zero, ignore,
   otherwise compare against dirent inode
   pino    - if zero, ingore,
   otherwise compare against parent inode
   and use name and nsize as extra criteria
   name    - name of wanted dirent, used if pino!=0
   nsize   - length of name of wanted dirent, used if pino!=0

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolvedirent(char *o, size_t size,
		uint32_t ino, uint32_t pino,
		char *name, uint8_t nsize)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (o + size);

	struct jffs2_raw_dirent *dd = NULL;

	uint32_t vmax, v;

	if (!pino && ino <= 1)
		return dd;

	vmax = 0;

	n = (union jffs2_node_union *) o;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT &&
					(!ino || je32_to_cpu(n->d.ino) == ino) &&
					(v = je32_to_cpu(n->d.version)) > vmax &&
					(!pino || (je32_to_cpu(n->d.pino) == pino &&
							   nsize == n->d.nsize &&
							   !memcmp(name, n->d.name, nsize)))) {
				/* XXX crc check */

				if (vmax < v) {
					vmax = v;
					dd = &(n->d);
				}
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			return dd;
	} while (1);
}

/* resolve name under certain parent inode to dirent */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   pino    - requested parent inode
   name    - name of wanted dirent
   nsize   - length of name of wanted dirent

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolvename(char *o, size_t size, uint32_t pino,
		char *name, uint8_t nsize)
{
	return resolvedirent(o, size, 0, pino, name, nsize);
}

/* resolve inode to dirent */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - compare against dirent inode

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolveinode(char *o, size_t size, uint32_t ino)
{
	return resolvedirent(o, size, ino, 0, NULL, 0);
}

/* resolve slash-style path into dirent and inode.
   slash as first byte marks absolute path (root=inode 1).
   . and .. are resolved properly, and symlinks are followed.
 */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - root inode, used if path is relative
   p       - path to be resolved
   inos    - result inode, zero if failure
   recc    - recursion count, to detect symlink loops

   return value: pointer to dirent struct in file system image.
   note that root directory doesn't have dirent struct
   (return value is NULL), but it has inode (*inos=1)
 */

struct jffs2_raw_dirent *resolvepath0(char *o, size_t size, uint32_t ino,
		const char *p, uint32_t * inos, int recc)
{
	struct jffs2_raw_dirent *dir = NULL;

	int d = 1;
	uint32_t tino;

	char *next;

	char *path, *pp;

	char symbuf[1024];
	size_t symsize;

	if (recc > 16) {
		/* probably symlink loop */
		*inos = 0;
		return NULL;
	}

	pp = path = strdup(p);

	if (*path == '/') {
		path++;
		ino = 1;
	}

	if (ino > 1) {
		dir = resolveinode(o, size, ino);

		ino = DIRENT_INO(dir);
	}

	next = path - 1;

	while (ino && next != NULL && next[1] != 0 && d) {
		path = next + 1;
		next = strchr(path, '/');

		if (next != NULL)
			*next = 0;

		if (*path == '.' && path[1] == 0)
			continue;
		if (*path == '.' && path[1] == '.' && path[2] == 0) {
			if (DIRENT_PINO(dir) == 1) {
				ino = 1;
				dir = NULL;
			} else {
				dir = resolveinode(o, size, DIRENT_PINO(dir));
				ino = DIRENT_INO(dir);
			}

			continue;
		}

		dir = resolvename(o, size, ino, path, (uint8_t) strlen(path));

		if (DIRENT_INO(dir) == 0 ||
				(next != NULL &&
				 !(dir->type == DT_DIR || dir->type == DT_LNK))) {
			free(pp);

			*inos = 0;

			return NULL;
		}

		if (dir->type == DT_LNK) {
			struct jffs2_raw_inode *ri;
			ri = find_raw_inode(o, size, DIRENT_INO(dir));
			putblock(symbuf, sizeof(symbuf), &symsize, ri);
			symbuf[symsize] = 0;

			tino = ino;
			ino = 0;

			dir = resolvepath0(o, size, tino, symbuf, &ino, ++recc);

			if (dir != NULL && next != NULL &&
					!(dir->type == DT_DIR || dir->type == DT_LNK)) {
				free(pp);

				*inos = 0;
				return NULL;
			}
		}
		if (dir != NULL)
			ino = DIRENT_INO(dir);
	}

	free(pp);

	*inos = ino;

	return dir;
}

/* resolve slash-style path into dirent and inode.
   slash as first byte marks absolute path (root=inode 1).
   . and .. are resolved properly, and symlinks are followed.
 */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - root inode, used if path is relative
   p       - path to be resolved
   inos    - result inode, zero if failure

   return value: pointer to dirent struct in file system image.
   note that root directory doesn't have dirent struct
   (return value is NULL), but it has inode (*inos=1)
 */

struct jffs2_raw_dirent *resolvepath(char *o, size_t size, uint32_t ino,
		const char *p, uint32_t * inos)
{
	return resolvepath0(o, size, ino, p, inos, 0);
}

/* lists files on directory specified by path */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   p       - path to be resolved
 */

void lsdir(char *o, size_t size, const char *path, int recurse, int want_ctime)
{
	struct jffs2_raw_dirent *dd;
	struct dir *d = NULL;

	uint32_t ino;

	dd = resolvepath(o, size, 1, path, &ino);

	if (ino == 0 ||
			(dd == NULL && ino == 0) || (dd != NULL && dd->type != DT_DIR))
		errmsg_die("%s: No such file or directory", path);

	d = collectdir(o, size, ino, d);
	printdir(o, size, d, path, recurse, want_ctime);
	freedir(d);
}

/* writes file specified by path to the buffer */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   p       - path to be resolved
   b       - file buffer
   bsize   - file buffer size
   rsize   - file result size
 */

void catfile(char *o, size_t size, char *path, char *b, size_t bsize,
		size_t * rsize)
{
	struct jffs2_raw_dirent *dd;
	struct jffs2_raw_inode *ri;
	uint32_t ino;

	dd = resolvepath(o, size, 1, path, &ino);

	if (ino == 0)
		errmsg_die("%s: No such file or directory", path);

	if (dd == NULL || dd->type != DT_REG)
		errmsg_die("%s: Not a regular file", path);

	ri = find_raw_inode(o, size, ino);
	putblock(b, bsize, rsize, ri);

	write(1, b, *rsize);
}

/* usage example */

int main(int argc, char **argv)
{
	int fd, opt, recurse = 0, want_ctime = 0;
	struct stat st;

	char *scratch, *dir = NULL, *file = NULL;
	size_t ssize = 0;

	char *buf;

	while ((opt = getopt(argc, argv, "rd:f:t")) > 0) {
		switch (opt) {
			case 'd':
				dir = optarg;
				break;
			case 'f':
				file = optarg;
				break;
			case 'r':
				recurse++;
				break;
			case 't':
				want_ctime++;
				break;
			default:
				fprintf(stderr,
						"Usage: %s <image> [-d|-f] < path >\n",
						PROGRAM_NAME);
				exit(EXIT_FAILURE);
		}
	}

	fd = open(argv[optind], O_RDONLY);
	if (fd == -1)
		sys_errmsg_die("%s", argv[optind]);

	if (fstat(fd, &st))
		sys_errmsg_die("%s", argv[optind]);

	buf = xmalloc((size_t) st.st_size);

	if (read(fd, buf, st.st_size) != (ssize_t) st.st_size)
		sys_errmsg_die("%s", argv[optind]);

	if (dir)
		lsdir(buf, st.st_size, dir, recurse, want_ctime);

	if (file) {
		scratch = xmalloc(SCRATCH_SIZE);

		catfile(buf, st.st_size, file, scratch, SCRATCH_SIZE, &ssize);
		free(scratch);
	}

	if (!dir && !file)
		lsdir(buf, st.st_size, "/", 1, want_ctime);


	free(buf);
	exit(EXIT_SUCCESS);
}
