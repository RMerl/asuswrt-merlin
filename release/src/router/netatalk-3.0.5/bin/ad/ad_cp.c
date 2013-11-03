/*
 * Copyright (c) 2010, Frank Lahm <franklahm@googlemail.com>
 * Copyright (c) 1988, 1993, 1994
 * The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize "to" and use fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in "to") to form the final target path.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <atalk/ftw.h>
#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/util.h>
#include <atalk/unix.h>
#include <atalk/volume.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/queue.h>

#include "ad.h"

#define STRIP_TRAILING_SLASH(p) {                                   \
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')  \
            *--(p).p_end = 0;                                       \
    }

static char emptystring[] = "";

PATH_T to = { to.p_path, emptystring, "" };
enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

int fflag, iflag, nflag, pflag, vflag;
mode_t mask;

cnid_t ppdid, pdid, did; /* current dir CNID and parent did*/

static afpvol_t svolume, dvolume;
static enum op type;
static int Rflag;
static volatile sig_atomic_t alarmed;
static int badcp, rval;
static int ftw_options = FTW_MOUNT | FTW_PHYS | FTW_ACTIONRETVAL;

/* Forward declarations */
static int copy(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);
static int ftw_copy_file(const struct FTW *, const char *, const struct stat *, int);
static int ftw_copy_link(const struct FTW *, const char *, const struct stat *, int);
static int setfile(const struct stat *, int);
// static int preserve_dir_acls(const struct stat *, char *, char *);
static int preserve_fd_acls(int, int);

static void upfunc(void)
{
    did = pdid;
    pdid = ppdid;
}

/*
  SIGNAL handling:
  catch SIGINT and SIGTERM which cause clean exit. Ignore anything else.
*/

static void sig_handler(int signo)
{
    alarmed = 1;
    return;
}

static void set_signal(void)
{
    struct sigaction sv;

    sv.sa_handler = sig_handler;
    sv.sa_flags = SA_RESTART;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGTERM, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGTERM): %s", strerror(errno));

    if (sigaction(SIGINT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGINT): %s", strerror(errno));

    memset(&sv, 0, sizeof(struct sigaction));
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask);

    if (sigaction(SIGABRT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGABRT): %s", strerror(errno));

    if (sigaction(SIGHUP, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGHUP): %s", strerror(errno));

    if (sigaction(SIGQUIT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGQUIT): %s", strerror(errno));
}

static void usage_cp(void)
{
    printf(
        "Usage: ad cp [-R] [-aipvf] <source_file> <target_file>\n"
        "       ad cp [-R] [-aipvfx] <source_file [source_file ...]> <target_directory>\n\n"
        "In the first synopsis form, the cp utility copies the contents of the source_file to the\n"
        "target_file.  In the second synopsis form, the contents of each named source_file is copied to the\n"
        "destination target_directory.  The names of the files themselves are not changed.  If cp detects an\n"
        "attempt to copy a file to itself, the copy will fail.\n\n"
        "Netatalk AFP volumes are detected by means of their \".AppleDesktop\" directory\n"
        "which is located in their volume root. When a copy targetting an AFP volume\n"
        "is detected, its CNID database daemon is connected and all copies will also\n"
        "go through the CNID database.\n"
        "AppleDouble files are also copied and created as needed when the target is\n"
        "an AFP volume.\n\n"
        "The following options are available:\n\n"
        "     -a    Archive mode.  Same as -Rp.\n\n"
        "     -f    For each existing destination pathname, remove it and create a new\n"
        "           file, without prompting for confirmation regardless of its permis-\n"
        "           sions.  (The -f option overrides any previous -i or -n options.)\n\n"
        "     -i    Cause cp to write a prompt to the standard error output before\n"
        "           copying a file that would overwrite an existing file.  If the\n"
        "           response from the standard input begins with the character 'y' or\n"
        "           'Y', the file copy is attempted.  (The -i option overrides any pre-\n"
        "           vious -f or -n options.)\n\n"
        "     -n    Do not overwrite an existing file.  (The -n option overrides any\n"
        "           previous -f or -i options.)\n\n"
        "     -p    Cause cp to preserve the following attributes of each source file\n"
        "           in the copy: modification time, access time, file flags, file mode,\n"
        "           user ID, and group ID, as allowed by permissions.\n"
        "           If the user ID and group ID cannot be preserved, no error message\n"
        "           is displayed and the exit value is not altered.\n\n"
        "     -R    If source_file designates a directory, cp copies the directory and\n"
        "           the entire subtree connected at that point.If the source_file\n"
        "           ends in a /, the contents of the directory are copied rather than\n"
        "           the directory itself.\n\n"
        "     -v    Cause cp to be verbose, showing files as they are copied.\n\n"
        "     -x    File system mount points are not traversed.\n\n"
        );
    exit(EXIT_FAILURE);
}

int ad_cp(int argc, char *argv[], AFPObj *obj)
{
    struct stat to_stat, tmp_stat;
    int r, ch, have_trailing_slash;
    char *target;
#if 0
    afpvol_t srcvol;
    afpvol_t dstvol;
#endif

    ppdid = pdid = htonl(1);
    did = htonl(2);

    while ((ch = getopt(argc, argv, "afinpRvx")) != -1)
        switch (ch) {
        case 'a':
            pflag = 1;
            Rflag = 1;
            break;
        case 'f':
            fflag = 1;
            iflag = nflag = 0;
            break;
        case 'i':
            iflag = 1;
            fflag = nflag = 0;
            break;
        case 'n':
            nflag = 1;
            fflag = iflag = 0;
            break;
        case 'p':
            pflag = 1;
            break;
        case 'R':
            Rflag = 1;
            break;
        case 'v':
            vflag = 1;
            break;
        case 'x':
            ftw_options |= FTW_MOUNT;
            break;
        default:
            usage_cp();
            break;
        }
    argc -= optind;
    argv += optind;

    if (argc < 2)
        usage_cp();

    set_signal();
    cnid_init();

    /* Save the target base in "to". */
    target = argv[--argc];
    if ((strlcpy(to.p_path, target, PATH_MAX)) >= PATH_MAX)
        ERROR("%s: name too long", target);

    to.p_end = to.p_path + strlen(to.p_path);
    if (to.p_path == to.p_end) {
        *to.p_end++ = '.';
        *to.p_end = 0;
    }
    have_trailing_slash = (to.p_end[-1] == '/');
    if (have_trailing_slash)
        STRIP_TRAILING_SLASH(to);
    to.target_end = to.p_end;

    /* Set end of argument list */
    argv[argc] = NULL;

    /*
     * Cp has two distinct cases:
     *
     * cp [-R] source target
     * cp [-R] source1 ... sourceN directory
     *
     * In both cases, source can be either a file or a directory.
     *
     * In (1), the target becomes a copy of the source. That is, if the
     * source is a file, the target will be a file, and likewise for
     * directories.
     *
     * In (2), the real target is not directory, but "directory/source".
     */
    r = stat(to.p_path, &to_stat);
    if (r == -1 && errno != ENOENT)
        ERROR("%s", to.p_path);
    if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
        /*
         * Case (1).  Target is not a directory.
         */
        if (argc > 1)
            ERROR("%s is not a directory", to.p_path);

        /*
         * Need to detect the case:
         *cp -R dir foo
         * Where dir is a directory and foo does not exist, where
         * we want pathname concatenations turned on but not for
         * the initial mkdir().
         */
        if (r == -1) {
            lstat(*argv, &tmp_stat);

            if (S_ISDIR(tmp_stat.st_mode) && Rflag)
                type = DIR_TO_DNE;
            else
                type = FILE_TO_FILE;
        } else
            type = FILE_TO_FILE;

        if (have_trailing_slash && type == FILE_TO_FILE) {
            if (r == -1)
                ERROR("directory %s does not exist", to.p_path);
            else
                ERROR("%s is not a directory", to.p_path);
        }
    } else
        /*
         * Case (2).  Target is a directory.
         */
        type = FILE_TO_DIR;

    /*
     * Keep an inverted copy of the umask, for use in correcting
     * permissions on created directories when not using -p.
     */
    mask = ~umask(0777);
    umask(~mask);

#if 0
    /* Inhereting perms in ad_mkdir etc requires this */
    ad_setfuid(0);
#endif

    /* Load .volinfo file for destination*/
    openvol(obj, to.p_path, &dvolume);

    for (int i = 0; argv[i] != NULL; i++) {
        /* Load .volinfo file for source */
        openvol(obj, argv[i], &svolume);

        if (nftw(argv[i], copy, upfunc, 20, ftw_options) == -1) {
            if (alarmed) {
                SLOG("...break");
            } else {
                SLOG("Error: %s: %s", argv[i], strerror(errno));
            }
            closevol(&svolume);
            closevol(&dvolume);
        }
    }
    return rval;
}

static int copy(const char *path,
                const struct stat *statp,
                int tflag,
                struct FTW *ftw)
{
    static int base = 0;

    struct stat to_stat;
    int dne;
    size_t nlen;
    const char *p;
    char *target_mid;

    if (alarmed)
        return -1;

    /* This currently doesn't work with "." */
    if (strcmp(path, ".") == 0) {
        ERROR("\".\" not supported");
    }
    const char *dir = strrchr(path, '/');
    if (dir == NULL)
        dir = path;
    else
        dir++;
    if (!dvolume.vol->vfs->vfs_validupath(dvolume.vol, dir))
        return FTW_SKIP_SUBTREE;

    /*
     * If we are in case (2) above, we need to append the
     * source name to the target name.
     */
    if (type != FILE_TO_FILE) {
        /*
         * Need to remember the roots of traversals to create
         * correct pathnames.  If there's a directory being
         * copied to a non-existent directory, e.g.
         *     cp -R a/dir noexist
         * the resulting path name should be noexist/foo, not
         * noexist/dir/foo (where foo is a file in dir), which
         * is the case where the target exists.
         *
         * Also, check for "..".  This is for correct path
         * concatenation for paths ending in "..", e.g.
         *     cp -R .. /tmp
         * Paths ending in ".." are changed to ".".  This is
         * tricky, but seems the easiest way to fix the problem.
         *
         * XXX
         * Since the first level MUST be FTS_ROOTLEVEL, base
         * is always initialized.
         */
        if (ftw->level == 0) {
            if (type != DIR_TO_DNE) {
                base = ftw->base;

                if (strcmp(&path[base], "..") == 0)
                    base += 1;
            } else
                base = strlen(path);
        }

        p = &path[base];
        nlen = strlen(path) - base;
        target_mid = to.target_end;
        if (*p != '/' && target_mid[-1] != '/')
            *target_mid++ = '/';
        *target_mid = 0;
        if (target_mid - to.p_path + nlen >= PATH_MAX) {
            SLOG("%s%s: name too long (not copied)", to.p_path, p);
            badcp = rval = 1;
            return 0;
        }
        (void)strncat(target_mid, p, nlen);
        to.p_end = target_mid + nlen;
        *to.p_end = 0;
        STRIP_TRAILING_SLASH(to);
    }

    /* Not an error but need to remember it happened */
    if (stat(to.p_path, &to_stat) == -1)
        dne = 1;
    else {
        if (to_stat.st_dev == statp->st_dev &&
            to_stat.st_ino == statp->st_ino) {
            SLOG("%s and %s are identical (not copied).", to.p_path, path);
            badcp = rval = 1;
            if (S_ISDIR(statp->st_mode))
                /* without using glibc extension FTW_ACTIONRETVAL cant handle this */
                return FTW_SKIP_SUBTREE;
            return 0;
        }
        if (!S_ISDIR(statp->st_mode) &&
            S_ISDIR(to_stat.st_mode)) {
            SLOG("cannot overwrite directory %s with "
                 "non-directory %s",
                 to.p_path, path);
            badcp = rval = 1;
            return 0;
        }
        dne = 0;
    }

    /* Convert basename to appropiate volume encoding */
    if (dvolume.vol->v_path) {
        if ((convert_dots_encoding(&svolume, &dvolume, to.p_path, MAXPATHLEN)) == -1) {
            SLOG("Error converting name for %s", to.p_path);
            badcp = rval = 1;
            return -1;
        }
    }

    switch (statp->st_mode & S_IFMT) {
    case S_IFLNK:
        if (ftw_copy_link(ftw, path, statp, !dne))
            badcp = rval = 1;
        break;
    case S_IFDIR:
        if (!Rflag) {
            SLOG("%s is a directory", path);
            badcp = rval = 1;
            return -1;
        }
        /*
         * If the directory doesn't exist, create the new
         * one with the from file mode plus owner RWX bits,
         * modified by the umask.  Trade-off between being
         * able to write the directory (if from directory is
         * 555) and not causing a permissions race.  If the
         * umask blocks owner writes, we fail..
         */
        if (dne) {
            if (mkdir(to.p_path, statp->st_mode | S_IRWXU) < 0)
                ERROR("mkdir: %s: %s", to.p_path, strerror(errno));
        } else if (!S_ISDIR(to_stat.st_mode)) {
            errno = ENOTDIR;
            ERROR("%s", to.p_path);
        }

        /* Create ad dir and copy ".Parent" */
        if (dvolume.vol->v_path && ADVOL_V2_OR_EA(dvolume.vol->v_adouble)) {
            mode_t omask = umask(0);
            if (dvolume.vol->v_adouble == AD_VERSION2) {
                /* Create ".AppleDouble" dir */
                bstring addir = bfromcstr(to.p_path);
                bcatcstr(addir, "/.AppleDouble");
                mkdir(cfrombstr(addir), 02777);
                bdestroy(addir);
            }

            if (svolume.vol->v_path && ADVOL_V2_OR_EA(svolume.vol->v_adouble)) {
                /* copy metadata file */
                if (dvolume.vol->vfs->vfs_copyfile(dvolume.vol, -1, path, to.p_path)) {
                    SLOG("Error copying adouble for %s -> %s", path, to.p_path);
                    badcp = rval = 1;
                    break;
                }
            }

            /* Get CNID of Parent and add new childir to CNID database */
            ppdid = pdid;
            if ((did = cnid_for_path(&dvolume, to.p_path, &pdid)) == CNID_INVALID) {
                SLOG("Error resolving CNID for %s", to.p_path);
                badcp = rval = 1;
                return -1;
            }

            struct adouble ad;
            struct stat st;
            if (lstat(to.p_path, &st) != 0) {
                badcp = rval = 1;
                break;
            }
            ad_init(&ad, dvolume.vol);
            if (ad_open(&ad, to.p_path, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666) != 0) {
                ERROR("Error opening adouble for: %s", to.p_path);
            }
            ad_setid( &ad, st.st_dev, st.st_ino, did, pdid, dvolume.db_stamp);
            if (dvolume.vol->v_adouble == AD_VERSION2)
                ad_setname(&ad, utompath(dvolume.vol, basename(to.p_path)));
            ad_setdate(&ad, AD_DATE_CREATE | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_MODIFY | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_ACCESS | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_BACKUP, AD_DATE_START);
            ad_flush(&ad);
            ad_close(&ad, ADFLAGS_HF);

            umask(omask);
        }

        if (pflag) {
            if (setfile(statp, -1))
                rval = 1;
#if 0
            if (preserve_dir_acls(statp, curr->fts_accpath, to.p_path) != 0)
                rval = 1;
#endif
        }
        break;

    case S_IFBLK:
    case S_IFCHR:
        SLOG("%s is a device file (not copied).", path);
        break;
    case S_IFSOCK:
        SLOG("%s is a socket (not copied).", path);
        break;
    case S_IFIFO:
        SLOG("%s is a FIFO (not copied).", path);
        break;
    default:
        if (ftw_copy_file(ftw, path, statp, dne))
            badcp = rval = 1;

        if (dvolume.vol->v_path && ADVOL_V2_OR_EA(dvolume.vol->v_adouble)) {

            mode_t omask = umask(0);
            if (svolume.vol->v_path && ADVOL_V2_OR_EA(svolume.vol->v_adouble)) {
                /* copy ad-file */
                if (dvolume.vol->vfs->vfs_copyfile(dvolume.vol, -1, path, to.p_path)) {
                    SLOG("Error copying adouble for %s -> %s", path, to.p_path);
                    badcp = rval = 1;
                    break;
                }
            }

            /* Get CNID of Parent and add new childir to CNID database */
            pdid = did;
            cnid_t cnid;
            if ((cnid = cnid_for_path(&dvolume, to.p_path, &did)) == CNID_INVALID) {
                SLOG("Error resolving CNID for %s", to.p_path);
                badcp = rval = 1;
                return -1;
            }

            struct adouble ad;
            struct stat st;
            if (lstat(to.p_path, &st) != 0) {
                badcp = rval = 1;
                break;
            }
            ad_init(&ad, dvolume.vol);
            if (ad_open(&ad, to.p_path, ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666) != 0) {
                ERROR("Error opening adouble for: %s", to.p_path);
            }
            ad_setid( &ad, st.st_dev, st.st_ino, cnid, did, dvolume.db_stamp);
            if (dvolume.vol->v_adouble == AD_VERSION2)
                ad_setname(&ad, utompath(dvolume.vol, basename(to.p_path)));
            ad_setdate(&ad, AD_DATE_CREATE | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_MODIFY | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_ACCESS | AD_DATE_UNIX, st.st_mtime);
            ad_setdate(&ad, AD_DATE_BACKUP, AD_DATE_START);
            ad_flush(&ad);
            ad_close(&ad, ADFLAGS_HF);
            umask(omask);
        }
        break;
    }
    if (vflag && !badcp)
        (void)printf("%s -> %s\n", path, to.p_path);

    return 0;
}

/* Memory strategy threshold, in pages: if physmem is larger then this, use a large buffer */
#define PHYSPAGES_THRESHOLD (32*1024)

/* Maximum buffer size in bytes - do not allow it to grow larger than this */
#define BUFSIZE_MAX (2*1024*1024)

/* Small (default) buffer size in bytes. It's inefficient for this to be smaller than MAXPHYS */
#define MAXPHYS (64 * 1024)
#define BUFSIZE_SMALL (MAXPHYS)

static int ftw_copy_file(const struct FTW *entp,
                         const char *spath,
                         const struct stat *sp,
                         int dne)
{
    static char *buf = NULL;
    static size_t bufsize;
    ssize_t wcount;
    size_t wresid;
    off_t wtotal;
    int ch, checkch, from_fd = 0, rcount, rval, to_fd = 0;
    char *bufp;
    char *p;

    if ((from_fd = open(spath, O_RDONLY, 0)) == -1) {
        SLOG("%s: %s", spath, strerror(errno));
        return (1);
    }

    /*
     * If the file exists and we're interactive, verify with the user.
     * If the file DNE, set the mode to be the from file, minus setuid
     * bits, modified by the umask; arguably wrong, but it makes copying
     * executables work right and it's been that way forever.  (The
     * other choice is 666 or'ed with the execute bits on the from file
     * modified by the umask.)
     */
    if (!dne) {
#define YESNO "(y/n [n]) "
        if (nflag) {
            if (vflag)
                printf("%s not overwritten\n", to.p_path);
            (void)close(from_fd);
            return (0);
        } else if (iflag) {
            (void)fprintf(stderr, "overwrite %s? %s",
                          to.p_path, YESNO);
            checkch = ch = getchar();
            while (ch != '\n' && ch != EOF)
                ch = getchar();
            if (checkch != 'y' && checkch != 'Y') {
                (void)close(from_fd);
                (void)fprintf(stderr, "not overwritten\n");
                return (1);
            }
        }

        if (fflag) {
            /* remove existing destination file name,
             * create a new file  */
            (void)unlink(to.p_path);
            (void)dvolume.vol->vfs->vfs_deletefile(dvolume.vol, -1, to.p_path);
            to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
                         sp->st_mode & ~(S_ISUID | S_ISGID));
        } else {
            /* overwrite existing destination file name */
            to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
        }
    } else {
        to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
                     sp->st_mode & ~(S_ISUID | S_ISGID));
    }

    if (to_fd == -1) {
        SLOG("%s: %s", to.p_path, strerror(errno));
        (void)close(from_fd);
        return (1);
    }

    rval = 0;

    /*
     * Mmap and write if less than 8M (the limit is so we don't totally
     * trash memory on big files.  This is really a minor hack, but it
     * wins some CPU back.
     * Some filesystems, such as smbnetfs, don't support mmap,
     * so this is a best-effort attempt.
     */

    if (S_ISREG(sp->st_mode) && sp->st_size > 0 &&
        sp->st_size <= 8 * 1024 * 1024 &&
        (p = mmap(NULL, (size_t)sp->st_size, PROT_READ,
                  MAP_SHARED, from_fd, (off_t)0)) != MAP_FAILED) {
        wtotal = 0;
        for (bufp = p, wresid = sp->st_size; ;
             bufp += wcount, wresid -= (size_t)wcount) {
            wcount = write(to_fd, bufp, wresid);
            if (wcount <= 0)
                break;
            wtotal += wcount;
            if (wcount >= (ssize_t)wresid)
                break;
        }
        if (wcount != (ssize_t)wresid) {
            SLOG("%s: %s", to.p_path, strerror(errno));
            rval = 1;
        }
        /* Some systems don't unmap on close(2). */
        if (munmap(p, sp->st_size) < 0) {
            SLOG("%s: %s", spath, strerror(errno));
            rval = 1;
        }
    } else {
        if (buf == NULL) {
            /*
             * Note that buf and bufsize are static. If
             * malloc() fails, it will fail at the start
             * and not copy only some files.
             */
            if (sysconf(_SC_PHYS_PAGES) >
                PHYSPAGES_THRESHOLD)
                bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
            else
                bufsize = BUFSIZE_SMALL;
            buf = malloc(bufsize);
            if (buf == NULL)
                ERROR("Not enough memory");

        }
        wtotal = 0;
        while ((rcount = read(from_fd, buf, bufsize)) > 0) {
            for (bufp = buf, wresid = rcount; ;
                 bufp += wcount, wresid -= wcount) {
                wcount = write(to_fd, bufp, wresid);
                if (wcount <= 0)
                    break;
                wtotal += wcount;
                if (wcount >= (ssize_t)wresid)
                    break;
            }
            if (wcount != (ssize_t)wresid) {
                SLOG("%s: %s", to.p_path, strerror(errno));
                rval = 1;
                break;
            }
        }
        if (rcount < 0) {
            SLOG("%s: %s", spath, strerror(errno));
            rval = 1;
        }
    }

    /*
     * Don't remove the target even after an error.  The target might
     * not be a regular file, or its attributes might be important,
     * or its contents might be irreplaceable.  It would only be safe
     * to remove it if we created it and its length is 0.
     */

    if (pflag && setfile(sp, to_fd))
        rval = 1;
    if (pflag && preserve_fd_acls(from_fd, to_fd) != 0)
        rval = 1;
    if (close(to_fd)) {
        SLOG("%s: %s", to.p_path, strerror(errno));
        rval = 1;
    }

    (void)close(from_fd);

    return (rval);
}

static int ftw_copy_link(const struct FTW *p,
                         const char *spath,
                         const struct stat *sstp,
                         int exists)
{
    int len;
    char llink[PATH_MAX];

    if ((len = readlink(spath, llink, sizeof(llink) - 1)) == -1) {
        SLOG("readlink: %s: %s", spath, strerror(errno));
        return (1);
    }
    llink[len] = '\0';
    if (exists && unlink(to.p_path)) {
        SLOG("unlink: %s: %s", to.p_path, strerror(errno));
        return (1);
    }
    if (symlink(llink, to.p_path)) {
        SLOG("symlink: %s: %s", llink, strerror(errno));
        return (1);
    }
    return (pflag ? setfile(sstp, -1) : 0);
}

static int setfile(const struct stat *fs, int fd)
{
    static struct timeval tv[2];
    struct stat ts;
    int rval, gotstat, islink, fdval;
    mode_t mode;

    rval = 0;
    fdval = fd != -1;
    islink = !fdval && S_ISLNK(fs->st_mode);
    mode = fs->st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);

#if defined(__FreeBSD__)
    TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atimespec);
    TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtimespec);
#else
    TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atim);
    TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtim);
#endif

    if (utimes(to.p_path, tv)) {
        SLOG("utimes: %s", to.p_path);
        rval = 1;
    }
    if (fdval ? fstat(fd, &ts) :
        (islink ? lstat(to.p_path, &ts) : stat(to.p_path, &ts)))
        gotstat = 0;
    else {
        gotstat = 1;
        ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX |
            S_IRWXU | S_IRWXG | S_IRWXO;
    }
    /*
     * Changing the ownership probably won't succeed, unless we're root
     * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
     * the mode; current BSD behavior is to remove all setuid bits on
     * chown.  If chown fails, lose setuid/setgid bits.
     */
    if (!gotstat || fs->st_uid != ts.st_uid || fs->st_gid != ts.st_gid)
        if (fdval ? fchown(fd, fs->st_uid, fs->st_gid) :
            (islink ? lchown(to.p_path, fs->st_uid, fs->st_gid) :
             chown(to.p_path, fs->st_uid, fs->st_gid))) {
            if (errno != EPERM) {
                SLOG("chown: %s: %s", to.p_path, strerror(errno));
                rval = 1;
            }
            mode &= ~(S_ISUID | S_ISGID);
        }

    if (!gotstat || mode != ts.st_mode)
        if (fdval ? fchmod(fd, mode) : chmod(to.p_path, mode)) {
            SLOG("chmod: %s: %s", to.p_path, strerror(errno));
            rval = 1;
        }

#ifdef HAVE_ST_FLAGS
    if (!gotstat || fs->st_flags != ts.st_flags)
        if (fdval ?
            fchflags(fd, fs->st_flags) :
            (islink ? lchflags(to.p_path, fs->st_flags) :
             chflags(to.p_path, fs->st_flags))) {
            SLOG("chflags: %s: %s", to.p_path, strerror(errno));
            rval = 1;
        }
#endif

    return (rval);
}

static int preserve_fd_acls(int source_fd, int dest_fd)
{
#if 0
    acl_t acl;
    acl_type_t acl_type;
    int acl_supported = 0, ret, trivial;

    ret = fpathconf(source_fd, _PC_ACL_NFS4);
    if (ret > 0 ) {
        acl_supported = 1;
        acl_type = ACL_TYPE_NFS4;
    } else if (ret < 0 && errno != EINVAL) {
        warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", to.p_path);
        return (1);
    }
    if (acl_supported == 0) {
        ret = fpathconf(source_fd, _PC_ACL_EXTENDED);
        if (ret > 0 ) {
            acl_supported = 1;
            acl_type = ACL_TYPE_ACCESS;
        } else if (ret < 0 && errno != EINVAL) {
            warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s",
                 to.p_path);
            return (1);
        }
    }
    if (acl_supported == 0)
        return (0);

    acl = acl_get_fd_np(source_fd, acl_type);
    if (acl == NULL) {
        warn("failed to get acl entries while setting %s", to.p_path);
        return (1);
    }
    if (acl_is_trivial_np(acl, &trivial)) {
        warn("acl_is_trivial() failed for %s", to.p_path);
        acl_free(acl);
        return (1);
    }
    if (trivial) {
        acl_free(acl);
        return (0);
    }
    if (acl_set_fd_np(dest_fd, acl, acl_type) < 0) {
        warn("failed to set acl entries for %s", to.p_path);
        acl_free(acl);
        return (1);
    }
    acl_free(acl);
#endif
    return (0);
}

#if 0
static int preserve_dir_acls(const struct stat *fs, char *source_dir, char *dest_dir)
{
    acl_t (*aclgetf)(const char *, acl_type_t);
    int (*aclsetf)(const char *, acl_type_t, acl_t);
    struct acl *aclp;
    acl_t acl;
    acl_type_t acl_type;
    int acl_supported = 0, ret, trivial;

    ret = pathconf(source_dir, _PC_ACL_NFS4);
    if (ret > 0) {
        acl_supported = 1;
        acl_type = ACL_TYPE_NFS4;
    } else if (ret < 0 && errno != EINVAL) {
        warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", source_dir);
        return (1);
    }
    if (acl_supported == 0) {
        ret = pathconf(source_dir, _PC_ACL_EXTENDED);
        if (ret > 0) {
            acl_supported = 1;
            acl_type = ACL_TYPE_ACCESS;
        } else if (ret < 0 && errno != EINVAL) {
            warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s",
                 source_dir);
            return (1);
        }
    }
    if (acl_supported == 0)
        return (0);

    /*
     * If the file is a link we will not follow it
     */
    if (S_ISLNK(fs->st_mode)) {
        aclgetf = acl_get_link_np;
        aclsetf = acl_set_link_np;
    } else {
        aclgetf = acl_get_file;
        aclsetf = acl_set_file;
    }
    if (acl_type == ACL_TYPE_ACCESS) {
        /*
         * Even if there is no ACL_TYPE_DEFAULT entry here, a zero
         * size ACL will be returned. So it is not safe to simply
         * check the pointer to see if the default ACL is present.
         */
        acl = aclgetf(source_dir, ACL_TYPE_DEFAULT);
        if (acl == NULL) {
            warn("failed to get default acl entries on %s",
                 source_dir);
            return (1);
        }
        aclp = &acl->ats_acl;
        if (aclp->acl_cnt != 0 && aclsetf(dest_dir,
                                          ACL_TYPE_DEFAULT, acl) < 0) {
            warn("failed to set default acl entries on %s",
                 dest_dir);
            acl_free(acl);
            return (1);
        }
        acl_free(acl);
    }
    acl = aclgetf(source_dir, acl_type);
    if (acl == NULL) {
        warn("failed to get acl entries on %s", source_dir);
        return (1);
    }
    if (acl_is_trivial_np(acl, &trivial)) {
        warn("acl_is_trivial() failed on %s", source_dir);
        acl_free(acl);
        return (1);
    }
    if (trivial) {
        acl_free(acl);
        return (0);
    }
    if (aclsetf(dest_dir, acl_type, acl) < 0) {
        warn("failed to set acl entries on %s", dest_dir);
        acl_free(acl);
        return (1);
    }
    acl_free(acl);
    return (0);
}
#endif
