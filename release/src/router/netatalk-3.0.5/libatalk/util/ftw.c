/* File tree walker functions.
   Copyright (C) 1996-2004, 2006-2008, 2010 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1996.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if __GNUC__
# define alloca __builtin_alloca
#else
#  include <alloca.h>
#endif

#include <dirent.h>
#define NAMLEN(dirent) strlen ((dirent)->d_name)
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <atalk/ftw.h>

#ifndef HAVE_MEMPCPY
#define mempcpy(D, S, N) ((void *) ((char *) memcpy (D, S, N) + (N)))
#endif

#define NDEBUG 1
#include <assert.h>

#ifndef _LIBC
# undef __chdir
# define __chdir chdir
# undef __closedir
# define __closedir closedir
# undef __fchdir
# define __fchdir fchdir
# undef __getcwd
# define __getcwd(P, N) xgetcwd ()
# undef __mempcpy
# define __mempcpy mempcpy
# undef __opendir
# define __opendir opendir
# undef __readdir64
# define __readdir64 readdir
# undef __tdestroy
# define __tdestroy tdestroy
# undef __tfind
# define __tfind tfind
# undef __tsearch
# define __tsearch tsearch
# undef internal_function
# define internal_function /* empty */
# undef dirent64
# define dirent64 dirent
# undef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

/* Support for the LFS API version.  */
#ifndef FTW_NAME
# define FTW_NAME ftw
# define NFTW_NAME nftw
# define NFTW_OLD_NAME __old_nftw
# define NFTW_NEW_NAME __new_nftw
# define INO_T ino_t
# define STAT stat
# define LXSTAT(V,f,sb) lstat (f,sb)
# define XSTAT(V,f,sb) stat (f,sb)
# define FXSTATAT(V,d,f,sb,m) fstatat (d, f, sb, m)

#endif

/* We define PATH_MAX if the system does not provide a definition.
   This does not artificially limit any operation.  PATH_MAX is simply
   used as a guesstimate for the expected maximal path length.
   Buffers will be enlarged if necessary.  */
#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

struct dir_data
{
    DIR *stream;
    int streamfd;
    char *content;
};

struct known_object
{
    dev_t dev;
    INO_T ino;
};

struct ftw_data
{
    /* Array with pointers to open directory streams.  */
    struct dir_data **dirstreams;
    size_t actdir;
    size_t maxdir;

    /* Buffer containing name of currently processed object.  */
    char *dirbuf;
    size_t dirbufsize;

    /* Passed as fourth argument to `nftw' callback.  The `base' member
       tracks the content of the `dirbuf'.  */
    struct FTW ftw;

    /* Flags passed to `nftw' function.  0 for `ftw'.  */
    int flags;

    /* Conversion array for flag values.  It is the identity mapping for
       `nftw' calls, otherwise it maps the values to those known by
       `ftw'.  */
    const int *cvt_arr;

    /* Callback function.  We always use the `nftw' form.  */
    NFTW_FUNC_T func;

    /* Device of starting point.  Needed for FTW_MOUNT.  */
    dev_t dev;

    /* Data structure for keeping fingerprints of already processed
       object.  This is needed when not using FTW_PHYS.  */
    void *known_objects;
};


/* Internally we use the FTW_* constants used for `nftw'.  When invoked
   as `ftw', map each flag to the subset of values used by `ftw'.  */
static const int nftw_arr[] =
{
    FTW_F, FTW_D, FTW_DNR, FTW_NS, FTW_SL, FTW_DP, FTW_SLN
};

static const int ftw_arr[] =
{
    FTW_F, FTW_D, FTW_DNR, FTW_NS, FTW_F, FTW_D, FTW_NS
};


static dir_notification_func_t upfunc;

/* Forward declarations of local functions.  */
static int ftw_dir (struct ftw_data *data, struct STAT *st,
                    struct dir_data *old_dir) internal_function;

typedef void (*__free_fn_t) (void *__nodep);
typedef struct node_t {
    const void *key;
    struct node_t *left;
    struct node_t *right;
    unsigned int red:1;
} *node;

static void tdestroy_recurse (node root, __free_fn_t freefct)
{
    if (root->left != NULL)
        tdestroy_recurse (root->left, freefct);
    if (root->right != NULL)
        tdestroy_recurse (root->right, freefct);
    (*freefct) ((void *) root->key);
    /* Free the node itself.  */
    free (root);
}

static void mytdestroy (void *vroot, __free_fn_t freefct)
{
    node root = (node) vroot;

    if (root != NULL)
        tdestroy_recurse (root, freefct);
}

static char *mystpcpy(char *a, const char *b)
{
    strcpy(a, b);
    return (a + strlen(a));
}

static char *xgetcwd(void)
{
    char *cwd;
    char *ret;
    unsigned path_max;

    errno = 0;
    path_max = (unsigned) PATH_MAX;
    path_max += 2;        /* The getcwd docs say to do this. */

    cwd = malloc (path_max);
    errno = 0;
    while ((ret = getcwd (cwd, path_max)) == NULL && errno == ERANGE) {
        path_max += 512;
        cwd = realloc (cwd, path_max);
        errno = 0;
    }

    if (ret == NULL) {
        int save_errno = errno;
        free (cwd);
        errno = save_errno;
        return NULL;
    }
    return cwd;
}

static int
object_compare (const void *p1, const void *p2)
{
    /* We don't need a sophisticated and useful comparison.  We are only
       interested in equality.  However, we must be careful not to
       accidentally compare `holes' in the structure.  */
    const struct known_object *kp1 = p1, *kp2 = p2;
    int cmp1;
    cmp1 = (kp1->ino > kp2->ino) - (kp1->ino < kp2->ino);
    if (cmp1 != 0)
        return cmp1;
    return (kp1->dev > kp2->dev) - (kp1->dev < kp2->dev);
}


static int
add_object (struct ftw_data *data, struct STAT *st)
{
    struct known_object *newp = malloc (sizeof (struct known_object));
    if (newp == NULL)
        return -1;
    newp->dev = st->st_dev;
    newp->ino = st->st_ino;
    return __tsearch (newp, &data->known_objects, object_compare) ? 0 : -1;
}


static inline int
find_object (struct ftw_data *data, struct STAT *st)
{
    struct known_object obj;
    obj.dev = st->st_dev;
    obj.ino = st->st_ino;
    return __tfind (&obj, &data->known_objects, object_compare) != NULL;
}


static inline int
open_dir_stream (int *dfdp, struct ftw_data *data, struct dir_data *dirp)
{
    int result = 0;

    if (data->dirstreams[data->actdir] != NULL)
    {
        /* Oh, oh.  We must close this stream.  Get all remaining
           entries and store them as a list in the `content' member of
           the `struct dir_data' variable.  */
        size_t bufsize = 1024;
        char *buf = malloc (bufsize);

        if (buf == NULL)
            result = -1;
        else
        {
            DIR *st = data->dirstreams[data->actdir]->stream;
            struct dirent64 *d;
            size_t actsize = 0;

            while ((d = __readdir64 (st)) != NULL)
            {
                size_t this_len = NAMLEN (d);
                if (actsize + this_len + 2 >= bufsize)
                {
                    char *newp;
                    bufsize += MAX (1024, 2 * this_len);
                    newp = (char *) realloc (buf, bufsize);
                    if (newp == NULL)
                    {
                        /* No more memory.  */
                        int save_err = errno;
                        free (buf);
                        __set_errno (save_err);
                        return -1;
                    }
                    buf = newp;
                }

                *((char *) __mempcpy (buf + actsize, d->d_name, this_len))
                    = '\0';
                actsize += this_len + 1;
            }

            /* Terminate the list with an additional NUL byte.  */
            buf[actsize++] = '\0';

            /* Shrink the buffer to what we actually need.  */
            data->dirstreams[data->actdir]->content = realloc (buf, actsize);
            if (data->dirstreams[data->actdir]->content == NULL)
            {
                int save_err = errno;
                free (buf);
                __set_errno (save_err);
                result = -1;
            }
            else
            {
                __closedir (st);
                data->dirstreams[data->actdir]->stream = NULL;
                data->dirstreams[data->actdir]->streamfd = -1;
                data->dirstreams[data->actdir] = NULL;
            }
        }
    }

    /* Open the new stream.  */
    if (result == 0)
    {
        assert (data->dirstreams[data->actdir] == NULL);

        if (dfdp != NULL && *dfdp != -1)
        {
            int fd = openat(*dfdp, data->dirbuf + data->ftw.base, O_RDONLY);
            dirp->stream = NULL;
            if (fd != -1 && (dirp->stream = fdopendir (fd)) == NULL)
                close(fd);
        }
        else
        {
            const char *name;

            if (data->flags & FTW_CHDIR)
            {
                name = data->dirbuf + data->ftw.base;
                if (name[0] == '\0')
                    name = ".";
            }
            else
                name = data->dirbuf;

            dirp->stream = __opendir (name);
        }

        if (dirp->stream == NULL)
            result = -1;
        else
        {
            dirp->streamfd = dirfd (dirp->stream);
            dirp->content = NULL;
            data->dirstreams[data->actdir] = dirp;

            if (++data->actdir == data->maxdir)
                data->actdir = 0;
        }
    }

    return result;
}


static int
process_entry (struct ftw_data *data, struct dir_data *dir, const char *name, size_t namlen)
{
    struct STAT st;
    int result = 0;
    int flag = 0;
    size_t new_buflen;

    if (name[0] == '.' && (name[1] == '\0'
                           || (name[1] == '.' && name[2] == '\0')))
        /* Don't process the "." and ".." entries.  */
        return 0;

    new_buflen = data->ftw.base + namlen + 2;
    if (data->dirbufsize < new_buflen)
    {
        /* Enlarge the buffer.  */
        char *newp;

        data->dirbufsize = 2 * new_buflen;
        newp = (char *) realloc (data->dirbuf, data->dirbufsize);
        if (newp == NULL)
            return -1;
        data->dirbuf = newp;
    }

    *((char *) __mempcpy (data->dirbuf + data->ftw.base, name, namlen)) = '\0';

    int statres;
    if (dir->streamfd != -1)
        statres = FXSTATAT (_STAT_VER, dir->streamfd, name, &st,
                            (data->flags & FTW_PHYS) ? AT_SYMLINK_NOFOLLOW : 0);
    else
    {
        if ((data->flags & FTW_CHDIR) == 0)
            name = data->dirbuf;

        statres = ((data->flags & FTW_PHYS)
                   ? LXSTAT (_STAT_VER, name, &st)
                   : XSTAT (_STAT_VER, name, &st));
    }

    if (statres < 0)
    {
        if (errno != EACCES && errno != ENOENT)
            result = -1;
        else if (data->flags & FTW_PHYS)
            flag = FTW_NS;
        else
        {
            if (dir->streamfd != -1)
                statres = FXSTATAT (_STAT_VER, dir->streamfd, name, &st,
                                    AT_SYMLINK_NOFOLLOW);
            else
                statres = LXSTAT (_STAT_VER, name, &st);
            if (statres == 0 && S_ISLNK (st.st_mode))
                flag = FTW_SLN;
            else
                flag = FTW_NS;
        }
    }
    else
    {
        if (S_ISDIR (st.st_mode))
            flag = FTW_D;
        else if (S_ISLNK (st.st_mode))
            flag = FTW_SL;
        else
            flag = FTW_F;
    }

    if (result == 0
        && (flag == FTW_NS
            || !(data->flags & FTW_MOUNT) || st.st_dev == data->dev))
    {
        if (flag == FTW_D)
        {
            if ((data->flags & FTW_PHYS)
                || (!find_object (data, &st)
                    /* Remember the object.  */
                    && (result = add_object (data, &st)) == 0))
                result = ftw_dir (data, &st, dir);
        }
        else
            result = (*data->func) (data->dirbuf, &st, data->cvt_arr[flag],
                                    &data->ftw);
    }

    if ((data->flags & FTW_ACTIONRETVAL) && result == FTW_SKIP_SUBTREE)
        result = 0;

    return result;
}


static int
ftw_dir (struct ftw_data *data, struct STAT *st, struct dir_data *old_dir)
{
    struct dir_data dir;
    struct dirent64 *d;
    int previous_base = data->ftw.base;
    int result;
    char *startp;

    /* Open the stream for this directory.  This might require that
       another stream has to be closed.  */
    result = open_dir_stream (old_dir == NULL ? NULL : &old_dir->streamfd,
                              data, &dir);
    if (result != 0)
    {
        if (errno == EACCES)
            /* We cannot read the directory.  Signal this with a special flag.  */
            result = (*data->func) (data->dirbuf, st, FTW_DNR, &data->ftw);

        return result;
    }

    /* First, report the directory (if not depth-first).  */
    if (!(data->flags & FTW_DEPTH))
    {
        result = (*data->func) (data->dirbuf, st, FTW_D, &data->ftw);
        if (result != 0)
        {
            int save_err;
        fail:
            save_err = errno;
            __closedir (dir.stream);
            dir.streamfd = -1;
            __set_errno (save_err);

            if (data->actdir-- == 0)
                data->actdir = data->maxdir - 1;
            data->dirstreams[data->actdir] = NULL;
            return result;
        }
    }

    /* If necessary, change to this directory.  */
    if (data->flags & FTW_CHDIR)
    {
        if (__fchdir (dirfd (dir.stream)) < 0)
        {
            result = -1;
            goto fail;
        }
    }

    /* Next, update the `struct FTW' information.  */
    ++data->ftw.level;
    startp = data->dirbuf + strlen(data->dirbuf);
    /* There always must be a directory name.  */
    assert (startp != data->dirbuf);
    if (startp[-1] != '/')
        *startp++ = '/';
    data->ftw.base = startp - data->dirbuf;

    while (dir.stream != NULL && (d = __readdir64 (dir.stream)) != NULL)
    {
        result = process_entry (data, &dir, d->d_name, NAMLEN (d));
        if (result != 0)
            break;
    }

    if (dir.stream != NULL)
    {
        /* The stream is still open.  I.e., we did not need more
           descriptors.  Simply close the stream now.  */
        int save_err = errno;

        assert (dir.content == NULL);

        __closedir (dir.stream);
        dir.streamfd = -1;
        __set_errno (save_err);

        if (data->actdir-- == 0)
            data->actdir = data->maxdir - 1;
        data->dirstreams[data->actdir] = NULL;
    }
    else
    {
        int save_err;
        char *runp = dir.content;

        while (result == 0 && *runp != '\0')
        {
            char *endp = strchr (runp, '\0');

            // XXX Should store the d_type values as well?!
            result = process_entry (data, &dir, runp, endp - runp);

            runp = endp + 1;
        }

        save_err = errno;
        free (dir.content);
        __set_errno (save_err);
    }

    if ((data->flags & FTW_ACTIONRETVAL) && result == FTW_SKIP_SIBLINGS)
        result = 0;

    /* Prepare the return, revert the `struct FTW' information.  */
    data->dirbuf[data->ftw.base - 1] = '\0';
    --data->ftw.level;
    if (upfunc)
        (*upfunc)();
    data->ftw.base = previous_base;

    /* Finally, if we process depth-first report the directory.  */
    if (result == 0 && (data->flags & FTW_DEPTH))
        result = (*data->func) (data->dirbuf, st, FTW_DP, &data->ftw);

    if (old_dir
        && (data->flags & FTW_CHDIR)
        && (result == 0
            || ((data->flags & FTW_ACTIONRETVAL)
                && (result != -1 && result != FTW_STOP))))
    {
        /* Change back to the parent directory.  */
        int done = 0;
        if (old_dir->stream != NULL)
            if (__fchdir (dirfd (old_dir->stream)) == 0)
                done = 1;

        if (!done)
        {
            if (data->ftw.base == 1)
            {
                if (__chdir ("/") < 0)
                    result = -1;
            }
            else
                if (__chdir ("..") < 0)
                    result = -1;
        }
    }

    return result;
}


static int ftw_startup (const char *dir,
                        int is_nftw,
                        void *func,
                        dir_notification_func_t up,
                        int descriptors,
                        int flags)
{
    struct ftw_data data;
    struct STAT st;
    int result = 0;
    int save_err;
    int cwdfd = -1;
    char *cwd = NULL;
    char *cp;

    upfunc = up;

    /* First make sure the parameters are reasonable.  */
    if (dir[0] == '\0')
    {
        __set_errno (ENOENT);
        return -1;
    }

    data.maxdir = descriptors < 1 ? 1 : descriptors;
    data.actdir = 0;
    data.dirstreams = (struct dir_data **) alloca (data.maxdir
                                                   * sizeof (struct dir_data *));
    memset (data.dirstreams, '\0', data.maxdir * sizeof (struct dir_data *));

    /* PATH_MAX is always defined when we get here.  */
    data.dirbufsize = MAX (2 * strlen (dir), PATH_MAX);
    data.dirbuf = (char *) malloc (data.dirbufsize);
    if (data.dirbuf == NULL)
        return -1;
    cp = mystpcpy (data.dirbuf, dir);
    /* Strip trailing slashes.  */
    while (cp > data.dirbuf + 1 && cp[-1] == '/')
        --cp;
    *cp = '\0';

    data.ftw.level = 0;

    /* Find basename.  */
    while (cp > data.dirbuf && cp[-1] != '/')
        --cp;
    data.ftw.base = cp - data.dirbuf;

    data.flags = flags;

    /* This assignment might seem to be strange but it is what we want.
       The trick is that the first three arguments to the `ftw' and
       `nftw' callback functions are equal.  Therefore we can call in
       every case the callback using the format of the `nftw' version
       and get the correct result since the stack layout for a function
       call in C allows this.  */
    data.func = (NFTW_FUNC_T) func;

    /* Since we internally use the complete set of FTW_* values we need
       to reduce the value range before calling a `ftw' callback.  */
    data.cvt_arr = is_nftw ? nftw_arr : ftw_arr;

    /* No object known so far.  */
    data.known_objects = NULL;

    /* Now go to the directory containing the initial file/directory.  */
    if (flags & FTW_CHDIR)
    {
        /* We have to be able to go back to the current working
           directory.  The best way to do this is to use a file
           descriptor.  */
        cwdfd = open (".", O_RDONLY);
        if (cwdfd == -1)
        {
            /* Try getting the directory name.  This can be needed if
               the current directory is executable but not readable.  */
            if (errno == EACCES)
                /* GNU extension ahead.  */
                cwd = __getcwd(NULL, 0);

            if (cwd == NULL)
                goto out_fail;
        }
        else if (data.maxdir > 1)
            /* Account for the file descriptor we use here.  */
            --data.maxdir;

        if (data.ftw.base > 0)
        {
            /* Change to the directory the file is in.  In data.dirbuf
               we have a writable copy of the file name.  Just NUL
               terminate it for now and change the directory.  */
            if (data.ftw.base == 1)
                /* I.e., the file is in the root directory.  */
                result = __chdir ("/");
            else
            {
                char ch = data.dirbuf[data.ftw.base - 1];
                data.dirbuf[data.ftw.base - 1] = '\0';
                result = __chdir (data.dirbuf);
                data.dirbuf[data.ftw.base - 1] = ch;
            }
        }
    }

    /* Get stat info for start directory.  */
    if (result == 0)
    {
        const char *name;

        if (data.flags & FTW_CHDIR)
        {
            name = data.dirbuf + data.ftw.base;
            if (name[0] == '\0')
                name = ".";
        }
        else
            name = data.dirbuf;

        if (((flags & FTW_PHYS)
             ? LXSTAT (_STAT_VER, name, &st)
             : XSTAT (_STAT_VER, name, &st)) < 0)
        {
            if (!(flags & FTW_PHYS)
                && errno == ENOENT
                && LXSTAT (_STAT_VER, name, &st) == 0
                && S_ISLNK (st.st_mode))
                result = (*data.func) (data.dirbuf, &st, data.cvt_arr[FTW_SLN],
                                       &data.ftw);
            else
                /* No need to call the callback since we cannot say anything
                   about the object.  */
                result = -1;
        }
        else
        {
            if (S_ISDIR (st.st_mode))
            {
                /* Remember the device of the initial directory in case
                   FTW_MOUNT is given.  */
                data.dev = st.st_dev;

                /* We know this directory now.  */
                if (!(flags & FTW_PHYS))
                    result = add_object (&data, &st);

                if (result == 0)
                    result = ftw_dir (&data, &st, NULL);
            }
            else
            {
                int flag = S_ISLNK (st.st_mode) ? FTW_SL : FTW_F;

                result = (*data.func) (data.dirbuf, &st, data.cvt_arr[flag],
                                       &data.ftw);
            }
        }

        if ((flags & FTW_ACTIONRETVAL)
            && (result == FTW_SKIP_SUBTREE || result == FTW_SKIP_SIBLINGS))
            result = 0;
    }

    /* Return to the start directory (if necessary).  */
    if (cwdfd != -1)
    {
        int save_err = errno;
        __fchdir (cwdfd);
        close(cwdfd);
        __set_errno (save_err);
    }
    else if (cwd != NULL)
    {
        int save_err = errno;
        __chdir (cwd);
        free (cwd);
        __set_errno (save_err);
    }

    /* Free all memory.  */
out_fail:
    save_err = errno;
    mytdestroy (data.known_objects, free);
    free (data.dirbuf);
    __set_errno (save_err);

    return result;
}



/* Entry points.  */
int NFTW_NAME(const char *path,
              NFTW_FUNC_T func,
              dir_notification_func_t up,
              int descriptors,
              int flags)
{
    return ftw_startup (path, 1, func, up, descriptors, flags);
}

