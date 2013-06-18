/*
 * message.c --- print e2fsck messages (with compression)
 *
 * Copyright 1996, 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 *
 * print_e2fsck_message() prints a message to the user, using
 * compression techniques and expansions of abbreviations.
 *
 * The following % expansions are supported:
 *
 * 	%b	<blk>			block number
 * 	%B	<blkcount>		interpret blkcount as blkcount
 * 	%c	<blk2>			block number
 * 	%Di	<dirent>->ino		inode number
 * 	%Dn	<dirent>->name		string
 * 	%Dr	<dirent>->rec_len
 * 	%Dl	<dirent>->name_len
 * 	%Dt	<dirent>->filetype
 * 	%d	<dir> 			inode number
 * 	%g	<group>			integer
 * 	%i	<ino>			inode number
 * 	%Is	<inode> -> i_size
 * 	%IS	<inode> -> i_extra_isize
 * 	%Ib	<inode> -> i_blocks
 * 	%Il	<inode> -> i_links_count
 * 	%Im	<inode> -> i_mode
 * 	%IM	<inode> -> i_mtime
 * 	%IF	<inode> -> i_faddr
 * 	%If	<inode> -> i_file_acl
 * 	%Id	<inode> -> i_dir_acl
 * 	%Iu	<inode> -> i_uid
 * 	%Ig	<inode> -> i_gid
 *	%It	<inode type>
 * 	%j	<ino2>			inode number
 * 	%m	<com_err error message>
 * 	%N	<num>
 *	%p	ext2fs_get_pathname of directory <ino>
 * 	%P	ext2fs_get_pathname of <dirent>->ino with <ino2> as
 * 			the containing directory.  (If dirent is NULL
 * 			then return the pathname of directory <ino2>)
 * 	%q	ext2fs_get_pathname of directory <dir>
 * 	%Q	ext2fs_get_pathname of directory <ino> with <dir> as
 * 			the containing directory.
 * 	%r	<blkcount>		interpret blkcount as refcount
 * 	%s	<str>			miscellaneous string
 * 	%S	backup superblock
 * 	%X	<num> hexadecimal format
 *
 * The following '@' expansions are supported:
 *
 * 	@a	extended attribute
 * 	@A	error allocating
 * 	@b	block
 * 	@B	bitmap
 * 	@c	compress
 * 	@C	conflicts with some other fs block
 * 	@D	deleted
 * 	@d	directory
 * 	@e	entry
 * 	@E	Entry '%Dn' in %p (%i)
 * 	@f	filesystem
 * 	@F	for @i %i (%Q) is
 * 	@g	group
 * 	@h	HTREE directory inode
 * 	@i	inode
 * 	@I	illegal
 * 	@j	journal
 * 	@l	lost+found
 * 	@L	is a link
 *	@m	multiply-claimed
 *	@n	invalid
 * 	@o	orphaned
 * 	@p	problem in
 *	@q	quota
 * 	@r	root inode
 * 	@s	should be
 * 	@S	superblock
 * 	@u	unattached
 * 	@v	device
 *	@x	extent
 * 	@z	zero-length
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include "e2fsck.h"

#include "problem.h"

#ifdef __GNUC__
#define _INLINE_ __inline__
#else
#define _INLINE_
#endif

/*
 * This structure defines the abbreviations used by the text strings
 * below.  The first character in the string is the index letter.  An
 * abbreviation of the form '@<i>' is expanded by looking up the index
 * letter <i> in the table below.
 */
static const char *abbrevs[] = {
	N_("aextended attribute"),
	N_("Aerror allocating"),
	N_("bblock"),
	N_("Bbitmap"),
	N_("ccompress"),
	N_("Cconflicts with some other fs @b"),
	N_("iinode"),
	N_("Iillegal"),
	N_("jjournal"),
	N_("Ddeleted"),
	N_("ddirectory"),
	N_("eentry"),
	N_("E@e '%Dn' in %p (%i)"),
	N_("ffilesystem"),
	N_("Ffor @i %i (%Q) is"),
	N_("ggroup"),
	N_("hHTREE @d @i"),
	N_("llost+found"),
	N_("Lis a link"),
	N_("mmultiply-claimed"),
	N_("ninvalid"),
	N_("oorphaned"),
	N_("pproblem in"),
	N_("qquota"),
	N_("rroot @i"),
	N_("sshould be"),
	N_("Ssuper@b"),
	N_("uunattached"),
	N_("vdevice"),
	N_("xextent"),
	N_("zzero-length"),
	"@@",
	0
	};

/*
 * Give more user friendly names to the "special" inodes.
 */
#define num_special_inodes	11
static const char *special_inode_name[] =
{
	N_("<The NULL inode>"),			/* 0 */
	N_("<The bad blocks inode>"),		/* 1 */
	"/",					/* 2 */
	N_("<The user quota inode>"),		/* 3 */
	N_("<The group quota inode>"),		/* 4 */
	N_("<The boot loader inode>"),		/* 5 */
	N_("<The undelete directory inode>"),	/* 6 */
	N_("<The group descriptor inode>"),	/* 7 */
	N_("<The journal inode>"),		/* 8 */
	N_("<Reserved inode 9>"),		/* 9 */
	N_("<Reserved inode 10>"),		/* 10 */
};

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 */
static void safe_print(FILE *f, const char *cp, int len)
{
	unsigned char	ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (ch > 128) {
			fputs("M-", f);
			ch -= 128;
		}
		if ((ch < 32) || (ch == 0x7f)) {
			fputc('^', f);
			ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */
		}
		fputc(ch, f);
	}
}


/*
 * This function prints a pathname, using the ext2fs_get_pathname
 * function
 */
static void print_pathname(FILE *f, ext2_filsys fs, ext2_ino_t dir,
			   ext2_ino_t ino)
{
	errcode_t	retval = 0;
	char		*path;

	if (!dir && (ino < num_special_inodes)) {
		fputs(_(special_inode_name[ino]), f);
		return;
	}

	if (fs)
		retval = ext2fs_get_pathname(fs, dir, ino, &path);
	if (!fs || retval)
		fputs("???", f);
	else {
		safe_print(f, path, -1);
		ext2fs_free_mem(&path);
	}
}

static void print_time(FILE *f, time_t t)
{
	const char *		time_str;
	static int		do_gmt = -1;

#ifdef __dietlibc__
		/* The diet libc doesn't respect the TZ environemnt variable */
		if (do_gmt == -1) {
			time_str = getenv("TZ");
			if (!time_str)
				time_str = "";
			do_gmt = !strcmp(time_str, "GMT0");
		}
#endif
		time_str = asctime((do_gmt > 0) ? gmtime(&t) : localtime(&t));
		fprintf(f, "%.24s", time_str);
}

/*
 * This function handles the '@' expansion.  We allow recursive
 * expansion; an @ expression can contain further '@' and '%'
 * expressions.
 */
static _INLINE_ void expand_at_expression(FILE *f, e2fsck_t ctx, char ch,
					  struct problem_context *pctx,
					  int *first, int recurse)
{
	const char **cpp, *str;

	/* Search for the abbreviation */
	for (cpp = abbrevs; *cpp; cpp++) {
		if (ch == *cpp[0])
			break;
	}
	if (*cpp && recurse < 10) {
		str = _(*cpp) + 1;
		if (*first && islower(*str)) {
			*first = 0;
			fputc(toupper(*str++), f);
		}
		print_e2fsck_message(f, ctx, str, pctx, *first, recurse+1);
	} else
		fprintf(f, "@%c", ch);
}

/*
 * This function expands '%IX' expressions
 */
static _INLINE_ void expand_inode_expression(FILE *f, ext2_filsys fs, char ch,
					     struct problem_context *ctx)
{
	struct ext2_inode	*inode;
	struct ext2_inode_large	*large_inode;

	if (!ctx || !ctx->inode)
		goto no_inode;

	inode = ctx->inode;
	large_inode = (struct ext2_inode_large *) inode;

	switch (ch) {
	case 's':
		if (LINUX_S_ISDIR(inode->i_mode))
			fprintf(f, "%u", inode->i_size);
		else {
#ifdef EXT2_NO_64_TYPE
			if (inode->i_size_high)
				fprintf(f, "0x%x%08x", inode->i_size_high,
					inode->i_size);
			else
				fprintf(f, "%u", inode->i_size);
#else
			fprintf(f, "%llu", EXT2_I_SIZE(inode));
#endif
		}
		break;
	case 'S':
		fprintf(f, "%u", large_inode->i_extra_isize);
		break;
	case 'b':
		if (fs->super->s_feature_ro_compat &
		    EXT4_FEATURE_RO_COMPAT_HUGE_FILE) 
			fprintf(f, "%llu", inode->i_blocks +
				(((long long) inode->osd2.linux2.l_i_blocks_hi)
				 << 32));
		else
			fprintf(f, "%u", inode->i_blocks);
		break;
	case 'l':
		fprintf(f, "%d", inode->i_links_count);
		break;
	case 'm':
		fprintf(f, "0%o", inode->i_mode);
		break;
	case 'M':
		print_time(f, inode->i_mtime);
		break;
	case 'F':
		fprintf(f, "%u", inode->i_faddr);
		break;
	case 'f':
		fprintf(f, "%llu", ext2fs_file_acl_block(fs, inode));
		break;
	case 'd':
		fprintf(f, "%u", (LINUX_S_ISDIR(inode->i_mode) ?
				  inode->i_dir_acl : 0));
		break;
	case 'u':
		fprintf(f, "%d", inode_uid(*inode));
		break;
	case 'g':
		fprintf(f, "%d", inode_gid(*inode));
		break;
	case 't':
		if (LINUX_S_ISREG(inode->i_mode))
			fputs(_("regular file"), f);
		else if (LINUX_S_ISDIR(inode->i_mode))
			fputs(_("directory"), f);
		else if (LINUX_S_ISCHR(inode->i_mode))
			fputs(_("character device"), f);
		else if (LINUX_S_ISBLK(inode->i_mode))
			fputs(_("block device"), f);
		else if (LINUX_S_ISFIFO(inode->i_mode))
			fputs(_("named pipe"), f);
		else if (LINUX_S_ISLNK(inode->i_mode))
			fputs(_("symbolic link"), f);
		else if (LINUX_S_ISSOCK(inode->i_mode))
			fputs(_("socket"), f);
		else
			fprintf(f, _("unknown file type with mode 0%o"),
				inode->i_mode);
		break;
	default:
	no_inode:
		fprintf(f, "%%I%c", ch);
		break;
	}
}

/*
 * This function expands '%dX' expressions
 */
static _INLINE_ void expand_dirent_expression(FILE *f, ext2_filsys fs, char ch,
					      struct problem_context *ctx)
{
	struct ext2_dir_entry	*dirent;
	unsigned int rec_len;
	int	len;

	if (!ctx || !ctx->dirent)
		goto no_dirent;

	dirent = ctx->dirent;

	switch (ch) {
	case 'i':
		fprintf(f, "%u", dirent->inode);
		break;
	case 'n':
		len = dirent->name_len & 0xFF;
		if ((ext2fs_get_rec_len(fs, dirent, &rec_len) == 0) &&
		    (len > rec_len))
			len = rec_len;
		safe_print(f, dirent->name, len);
		break;
	case 'r':
		(void) ext2fs_get_rec_len(fs, dirent, &rec_len);
		fprintf(f, "%u", rec_len);
		break;
	case 'l':
		fprintf(f, "%u", dirent->name_len & 0xFF);
		break;
	case 't':
		fprintf(f, "%u", dirent->name_len >> 8);
		break;
	default:
	no_dirent:
		fprintf(f, "%%D%c", ch);
		break;
	}
}

static _INLINE_ void expand_percent_expression(FILE *f, ext2_filsys fs,
					       char ch, int width, int *first,
					       struct problem_context *ctx)
{
	e2fsck_t e2fsck_ctx = fs ? (e2fsck_t) fs->priv_data : NULL;
	const char *m;

	if (!ctx)
		goto no_context;

	switch (ch) {
	case '%':
		fputc('%', f);
		break;
	case 'b':
#ifdef EXT2_NO_64_TYPE
		fprintf(f, "%*u", width, (unsigned long) ctx->blk);
#else
		fprintf(f, "%*llu", width, (unsigned long long) ctx->blk);
#endif
		break;
	case 'B':
		if (ctx->blkcount == BLOCK_COUNT_IND)
			m = _("indirect block");
		else if (ctx->blkcount == BLOCK_COUNT_DIND)
			m = _("double indirect block");
		else if (ctx->blkcount == BLOCK_COUNT_TIND)
			m = _("triple indirect block");
		else if (ctx->blkcount == BLOCK_COUNT_TRANSLATOR)
			m = _("translator block");
		else
			m = _("block #");
		if (*first && islower(m[0]))
			fputc(toupper(*m++), f);
		fputs(m, f);
		if (ctx->blkcount >= 0) {
#ifdef EXT2_NO_64_TYPE
			fprintf(f, "%d", ctx->blkcount);
#else
			fprintf(f, "%lld", (long long) ctx->blkcount);
#endif
		}
		break;
	case 'c':
#ifdef EXT2_NO_64_TYPE
		fprintf(f, "%*u", width, (unsigned long) ctx->blk2);
#else
		fprintf(f, "%*llu", width, (unsigned long long) ctx->blk2);
#endif
		break;
	case 'd':
		fprintf(f, "%*u", width, ctx->dir);
		break;
	case 'g':
		fprintf(f, "%*d", width, ctx->group);
		break;
	case 'i':
		fprintf(f, "%*u", width, ctx->ino);
		break;
	case 'j':
		fprintf(f, "%*u", width, ctx->ino2);
		break;
	case 'm':
		fprintf(f, "%*s", width, error_message(ctx->errcode));
		break;
	case 'N':
#ifdef EXT2_NO_64_TYPE
		fprintf(f, "%*u", width, ctx->num);
#else
		fprintf(f, "%*llu", width, (long long)ctx->num);
#endif
		break;
	case 'p':
		print_pathname(f, fs, ctx->ino, 0);
		break;
	case 'P':
		print_pathname(f, fs, ctx->ino2,
			       ctx->dirent ? ctx->dirent->inode : 0);
		break;
	case 'q':
		print_pathname(f, fs, ctx->dir, 0);
		break;
	case 'Q':
		print_pathname(f, fs, ctx->dir, ctx->ino);
		break;
	case 'r':
#ifdef EXT2_NO_64_TYPE
		fprintf(f, "%*d", width, ctx->blkcount);
#else
		fprintf(f, "%*lld", width, (long long) ctx->blkcount);
#endif
		break;
	case 'S':
		fprintf(f, "%u", get_backup_sb(NULL, fs, NULL, NULL));
		break;
	case 's':
		fprintf(f, "%*s", width, ctx->str ? ctx->str : "NULL");
		break;
	case 't':
		print_time(f, (time_t) ctx->num);
		break;
	case 'T':
		print_time(f, e2fsck_ctx ? e2fsck_ctx->now : time(0));
		break;
	case 'x':
		fprintf(f, "0x%0*x", width, ctx->csum1);
		break;
	case 'X':
#ifdef EXT2_NO_64_TYPE
		fprintf(f, "0x%0*x", width, ctx->num);
#else
		fprintf(f, "0x%0*llx", width, (long long)ctx->num);
#endif
		break;
	case 'y':
		fprintf(f, "0x%0*x", width, ctx->csum2);
		break;
	default:
	no_context:
		fprintf(f, "%%%c", ch);
		break;
	}
}

void print_e2fsck_message(FILE *f, e2fsck_t ctx, const char *msg,
			  struct problem_context *pctx, int first,
			  int recurse)
{
	ext2_filsys fs = ctx->fs;
	const char *	cp;
	int		i, width;

	e2fsck_clear_progbar(ctx);
	for (cp = msg; *cp; cp++) {
		if (cp[0] == '@') {
			cp++;
			expand_at_expression(f, ctx, *cp, pctx, &first,
					     recurse);
		} else if (cp[0] == '%') {
			cp++;
			width = 0;
			while (isdigit(cp[0])) {
				width = (width * 10) + cp[0] - '0';
				cp++;
			}
			if (cp[0] == 'I') {
				cp++;
				expand_inode_expression(f, fs, *cp, pctx);
			} else if (cp[0] == 'D') {
				cp++;
				expand_dirent_expression(f, fs, *cp, pctx);
			} else {
				expand_percent_expression(f, fs, *cp, width,
							  &first, pctx);
			}
		} else {
			for (i=0; cp[i]; i++)
				if ((cp[i] == '@') || cp[i] == '%')
					break;
			fprintf(f, "%.*s", i, cp);
			cp += i-1;
		}
		first = 0;
	}
}
