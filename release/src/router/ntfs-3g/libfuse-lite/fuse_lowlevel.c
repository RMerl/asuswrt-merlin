/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_lowlevel.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse_lowlevel_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define PARAM(inarg) (((const char *)(inarg)) + sizeof(*(inarg)))
#define OFFSET_MAX 0x7fffffffffffffffLL

struct fuse_ll;

struct fuse_req {
    struct fuse_ll *f;
    uint64_t unique;
    int ctr;
    pthread_mutex_t lock;
    struct fuse_ctx ctx;
    struct fuse_chan *ch;
    int interrupted;
    union {
        struct {
            uint64_t unique;
        } i;
        struct {
            fuse_interrupt_func_t func;
            void *data;
        } ni;
    } u;
    struct fuse_req *next;
    struct fuse_req *prev;
};

struct fuse_ll {
    int debug;
    int allow_root;
    struct fuse_lowlevel_ops op;
    int got_init;
    void *userdata;
    uid_t owner;
    struct fuse_conn_info conn;
    struct fuse_req list;
    struct fuse_req interrupts;
    pthread_mutex_t lock;
    int got_destroy;
};

static void convert_stat(const struct stat *stbuf, struct fuse_attr *attr)
{
    attr->ino       = stbuf->st_ino;
    attr->mode      = stbuf->st_mode;
    attr->nlink     = stbuf->st_nlink;
    attr->uid       = stbuf->st_uid;
    attr->gid       = stbuf->st_gid;
    attr->rdev      = stbuf->st_rdev;
    attr->size      = stbuf->st_size;
    attr->blocks    = stbuf->st_blocks;
    attr->atime     = stbuf->st_atime;
    attr->mtime     = stbuf->st_mtime;
    attr->ctime     = stbuf->st_ctime;
    attr->atimensec = ST_ATIM_NSEC(stbuf);
    attr->mtimensec = ST_MTIM_NSEC(stbuf);
    attr->ctimensec = ST_CTIM_NSEC(stbuf);
#ifdef POSIXACLS
    attr->filling = 0; /* JPA trying to be safe */
#endif
}

static void convert_attr(const struct fuse_setattr_in *attr, struct stat *stbuf)
{
    stbuf->st_mode         = attr->mode;
    stbuf->st_uid          = attr->uid;
    stbuf->st_gid          = attr->gid;
    stbuf->st_size         = attr->size;
    stbuf->st_atime        = attr->atime;
    stbuf->st_mtime        = attr->mtime;
    ST_ATIM_NSEC_SET(stbuf, attr->atimensec);
    ST_MTIM_NSEC_SET(stbuf, attr->mtimensec);
}

static  size_t iov_length(const struct iovec *iov, size_t count)
{
    size_t seg;
    size_t ret = 0;

    for (seg = 0; seg < count; seg++)
        ret += iov[seg].iov_len;
    return ret;
}

static void list_init_req(struct fuse_req *req)
{
    req->next = req;
    req->prev = req;
}

static void list_del_req(struct fuse_req *req)
{
    struct fuse_req *prev = req->prev;
    struct fuse_req *next = req->next;
    prev->next = next;
    next->prev = prev;
}

static void list_add_req(struct fuse_req *req, struct fuse_req *next)
{
    struct fuse_req *prev = next->prev;
    req->next = next;
    req->prev = prev;
    prev->next = req;
    next->prev = req;
}

static void destroy_req(fuse_req_t req)
{
    pthread_mutex_destroy(&req->lock);
    free(req);
}

static void free_req(fuse_req_t req)
{
    int ctr;
    struct fuse_ll *f = req->f;

    pthread_mutex_lock(&req->lock);
    req->u.ni.func = NULL;
    req->u.ni.data = NULL;
    pthread_mutex_unlock(&req->lock);

    pthread_mutex_lock(&f->lock);
    list_del_req(req);
    ctr = --req->ctr;
    pthread_mutex_unlock(&f->lock);
    if (!ctr)
        destroy_req(req);
}

static int send_reply_iov(fuse_req_t req, int error, struct iovec *iov,
                          int count)
{
    struct fuse_out_header out;
    int res;

    if (error <= -1000 || error > 0) {
        fprintf(stderr, "fuse: bad error value: %i\n",  error);
        error = -ERANGE;
    }

    out.unique = req->unique;
    out.error = error;
    iov[0].iov_base = &out;
    iov[0].iov_len = sizeof(struct fuse_out_header);
    out.len = iov_length(iov, count);

    if (req->f->debug)
        fprintf(stderr, "   unique: %llu, error: %i (%s), outsize: %i\n",
                (unsigned long long) out.unique, out.error,
                strerror(-out.error), out.len);
    res = fuse_chan_send(req->ch, iov, count);
    free_req(req);

    return res;
}

static int send_reply(fuse_req_t req, int error, const void *arg,
                      size_t argsize)
{
    struct iovec iov[2];
    int count = 1;
    if (argsize) {
        iov[1].iov_base = (void *) arg;
        iov[1].iov_len = argsize;
        count++;
    }
    return send_reply_iov(req, error, iov, count);
}

int fuse_reply_iov(fuse_req_t req, const struct iovec *iov, int count)
{
    int res;
    struct iovec *padded_iov;

    padded_iov = malloc((count + 1) * sizeof(struct iovec));
    if (padded_iov == NULL)
        return fuse_reply_err(req, -ENOMEM);

    memcpy(padded_iov + 1, iov, count * sizeof(struct iovec));
    count++;

    res = send_reply_iov(req, 0, padded_iov, count);
    free(padded_iov);

    return res;
}

size_t fuse_dirent_size(size_t namelen)
{
    return FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + namelen);
}

char *fuse_add_dirent(char *buf, const char *name, const struct stat *stbuf,
                      off_t off)
{
    unsigned namelen = strlen(name);
    unsigned entlen = FUSE_NAME_OFFSET + namelen;
    unsigned entsize = fuse_dirent_size(namelen);
    unsigned padlen = entsize - entlen;
    struct fuse_dirent *dirent = (struct fuse_dirent *) buf;

    dirent->ino = stbuf->st_ino;
    dirent->off = off;
    dirent->namelen = namelen;
    dirent->type = (stbuf->st_mode & 0170000) >> 12;
    strncpy(dirent->name, name, namelen);
    if (padlen)
        memset(buf + entlen, 0, padlen);

    return buf + entsize;
}

size_t fuse_add_direntry(fuse_req_t req, char *buf, size_t bufsize,
                         const char *name, const struct stat *stbuf, off_t off)
{
    size_t entsize;

    (void) req;
    entsize = fuse_dirent_size(strlen(name));
    if (entsize <= bufsize && buf)
        fuse_add_dirent(buf, name, stbuf, off);
    return entsize;
}

static void convert_statfs(const struct statvfs *stbuf,
                           struct fuse_kstatfs *kstatfs)
{
    kstatfs->bsize	= stbuf->f_bsize;
    kstatfs->frsize	= stbuf->f_frsize;
    kstatfs->blocks	= stbuf->f_blocks;
    kstatfs->bfree	= stbuf->f_bfree;
    kstatfs->bavail	= stbuf->f_bavail;
    kstatfs->files	= stbuf->f_files;
    kstatfs->ffree	= stbuf->f_ffree;
    kstatfs->namelen	= stbuf->f_namemax;
}

static int send_reply_ok(fuse_req_t req, const void *arg, size_t argsize)
{
    return send_reply(req, 0, arg, argsize);
}

int fuse_reply_err(fuse_req_t req, int err)
{
    return send_reply(req, -err, NULL, 0);
}

void fuse_reply_none(fuse_req_t req)
{
    fuse_chan_send(req->ch, NULL, 0);
    free_req(req);
}

static unsigned long calc_timeout_sec(double t)
{
    if (t > (double) ULONG_MAX)
        return ULONG_MAX;
    else if (t < 0.0)
        return 0;
    else
        return (unsigned long) t;
}

static unsigned int calc_timeout_nsec(double t)
{
    unsigned long secs = calc_timeout_sec(t);
    double f = t - (double)secs;
    if (f < 0.0)
        return 0;
    else if (f >= 0.999999999)
        return 999999999;
    else
        return (unsigned int) (f * 1.0e9);
}

static void fill_entry(struct fuse_entry_out *arg,
                       const struct fuse_entry_param *e)
{
    arg->nodeid = e->ino;
    arg->generation = e->generation;
    arg->entry_valid = calc_timeout_sec(e->entry_timeout);
    arg->entry_valid_nsec = calc_timeout_nsec(e->entry_timeout);
    arg->attr_valid = calc_timeout_sec(e->attr_timeout);
    arg->attr_valid_nsec = calc_timeout_nsec(e->attr_timeout);
    convert_stat(&e->attr, &arg->attr);
}

static void fill_open(struct fuse_open_out *arg,
                      const struct fuse_file_info *f)
{
    arg->fh = f->fh;
    if (f->direct_io)
        arg->open_flags |= FOPEN_DIRECT_IO;
    if (f->keep_cache)
        arg->open_flags |= FOPEN_KEEP_CACHE;
}

int fuse_reply_entry(fuse_req_t req, const struct fuse_entry_param *e)
{
    struct fuse_entry_out arg;

    /* before ABI 7.4 e->ino == 0 was invalid, only ENOENT meant
       negative entry */
    if (!e->ino && req->f->conn.proto_minor < 4)
        return fuse_reply_err(req, ENOENT);

    memset(&arg, 0, sizeof(arg));
    fill_entry(&arg, e);
#ifdef POSIXACLS
    return send_reply_ok(req, &arg, (req->f->conn.proto_minor >= 12 
			? sizeof(arg) : FUSE_COMPAT_ENTRY_OUT_SIZE));
#else
    return send_reply_ok(req, &arg, sizeof(arg));
#endif
}

int fuse_reply_create(fuse_req_t req, const struct fuse_entry_param *e,
                      const struct fuse_file_info *f)
{
    struct {
        struct fuse_entry_out e;
        struct fuse_open_out o;
    } arg;

    memset(&arg, 0, sizeof(arg));
    fill_entry(&arg.e, e);
#ifdef POSIXACLS
    if (req->f->conn.proto_minor < 12) {
	fill_open((struct fuse_open_out*)
		((char*)&arg + FUSE_COMPAT_ENTRY_OUT_SIZE), f);
	return send_reply_ok(req, &arg,
		FUSE_COMPAT_ENTRY_OUT_SIZE + sizeof(struct fuse_open_out));
    } else {
    	fill_open(&arg.o, f);
    	return send_reply_ok(req, &arg, sizeof(arg));
    }
#else
    fill_open(&arg.o, f);
    return send_reply_ok(req, &arg, sizeof(arg));
#endif
}

int fuse_reply_attr(fuse_req_t req, const struct stat *attr,
                    double attr_timeout)
{
    struct fuse_attr_out arg;

    memset(&arg, 0, sizeof(arg));
    arg.attr_valid = calc_timeout_sec(attr_timeout);
    arg.attr_valid_nsec = calc_timeout_nsec(attr_timeout);
    convert_stat(attr, &arg.attr);

#ifdef POSIXACLS
    return send_reply_ok(req, &arg, (req->f->conn.proto_minor >= 12
			? sizeof(arg) : FUSE_COMPAT_FUSE_ATTR_OUT_SIZE));
#else
    return send_reply_ok(req, &arg, sizeof(arg));
#endif
}

int fuse_reply_readlink(fuse_req_t req, const char *linkname)
{
    return send_reply_ok(req, linkname, strlen(linkname));
}

int fuse_reply_open(fuse_req_t req, const struct fuse_file_info *f)
{
    struct fuse_open_out arg;

    memset(&arg, 0, sizeof(arg));
    fill_open(&arg, f);
    return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_write(fuse_req_t req, size_t count)
{
    struct fuse_write_out arg;

    memset(&arg, 0, sizeof(arg));
    arg.size = count;

    return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_buf(fuse_req_t req, const char *buf, size_t size)
{
    return send_reply_ok(req, buf, size);
}

int fuse_reply_statfs(fuse_req_t req, const struct statvfs *stbuf)
{
    struct fuse_statfs_out arg;
    size_t size = req->f->conn.proto_minor < 4 ? FUSE_COMPAT_STATFS_SIZE : sizeof(arg);

    memset(&arg, 0, sizeof(arg));
    convert_statfs(stbuf, &arg.st);

    return send_reply_ok(req, &arg, size);
}

int fuse_reply_xattr(fuse_req_t req, size_t count)
{
    struct fuse_getxattr_out arg;

    memset(&arg, 0, sizeof(arg));
    arg.size = count;

    return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_lock(fuse_req_t req, struct flock *lock)
{
    struct fuse_lk_out arg;

    memset(&arg, 0, sizeof(arg));
    arg.lk.type = lock->l_type;
    if (lock->l_type != F_UNLCK) {
        arg.lk.start = lock->l_start;
        if (lock->l_len == 0)
            arg.lk.end = OFFSET_MAX;
        else
            arg.lk.end = lock->l_start + lock->l_len - 1;
    }
    arg.lk.pid = lock->l_pid;
    return send_reply_ok(req, &arg, sizeof(arg));
}

int fuse_reply_bmap(fuse_req_t req, uint64_t idx)
{
    struct fuse_bmap_out arg;

    memset(&arg, 0, sizeof(arg));
    arg.block = idx;

    return send_reply_ok(req, &arg, sizeof(arg));
}

static void do_lookup(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const char *name = (const char *) inarg;

    if (req->f->op.lookup)
        req->f->op.lookup(req, nodeid, name);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_forget(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_forget_in *arg = (const struct fuse_forget_in *) inarg;

    if (req->f->op.forget)
        req->f->op.forget(req, nodeid, arg->nlookup);
    else
        fuse_reply_none(req);
}

static void do_getattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    (void) inarg;

    if (req->f->op.getattr)
        req->f->op.getattr(req, nodeid, NULL);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_setattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_setattr_in *arg = (const struct fuse_setattr_in *) inarg;

    if (req->f->op.setattr) {
        struct fuse_file_info *fi = NULL;
        struct fuse_file_info fi_store;
        struct stat stbuf;
        memset(&stbuf, 0, sizeof(stbuf));
        convert_attr(arg, &stbuf);
        if (arg->valid & FATTR_FH) {
            memset(&fi_store, 0, sizeof(fi_store));
            fi = &fi_store;
            fi->fh = arg->fh;
            fi->fh_old = fi->fh;
        }
        req->f->op.setattr(req, nodeid, &stbuf, arg->valid & ~FATTR_FH, fi);
    } else
        fuse_reply_err(req, ENOSYS);
}

static void do_access(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_access_in *arg = (const struct fuse_access_in *) inarg;

    if (req->f->op.access)
        req->f->op.access(req, nodeid, arg->mask);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_readlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    (void) inarg;

    if (req->f->op.readlink)
        req->f->op.readlink(req, nodeid);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_mknod(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_mknod_in *arg = (const struct fuse_mknod_in *) inarg;
    const char *name = PARAM(arg);

#ifdef POSIXACLS
    if (req->f->conn.proto_minor >= 12)
	req->ctx.umask = arg->umask;
    else
#endif
	name = (const char *) inarg + FUSE_COMPAT_MKNOD_IN_SIZE;

    if (req->f->op.mknod)
		req->f->op.mknod(req, nodeid, name, arg->mode, arg->rdev);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_mkdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_mkdir_in *arg = (const struct fuse_mkdir_in *) inarg;

#ifdef POSIXACLS
    if (req->f->conn.proto_minor >= 12)
	req->ctx.umask = arg->umask;
#endif

    if (req->f->op.mkdir)
        req->f->op.mkdir(req, nodeid, PARAM(arg), arg->mode);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_unlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const char *name = (const char *) inarg;

    if (req->f->op.unlink)
        req->f->op.unlink(req, nodeid, name);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_rmdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const char *name = (const char *) inarg;

    if (req->f->op.rmdir)
        req->f->op.rmdir(req, nodeid, name);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_symlink(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const char *name = (const char *) inarg;
    const char *linkname = ((const char *) inarg) + strlen((const char *) inarg) + 1;

    if (req->f->op.symlink)
        req->f->op.symlink(req, linkname, nodeid, name);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_rename(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_rename_in *arg = (const struct fuse_rename_in *) inarg;
    const char *oldname = PARAM(arg);
    const char *newname = oldname + strlen(oldname) + 1;

    if (req->f->op.rename)
        req->f->op.rename(req, nodeid, oldname, arg->newdir, newname);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_link(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_link_in *arg = (const struct fuse_link_in *) inarg;

    if (req->f->op.link)
        req->f->op.link(req, arg->oldnodeid, nodeid, PARAM(arg));
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_create(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
	const struct fuse_create_in *arg = (const struct fuse_create_in *) inarg;

    if (req->f->op.create) {
        struct fuse_file_info fi;
	const char *name = PARAM(arg);

        memset(&fi, 0, sizeof(fi));
        fi.flags = arg->flags;

#ifdef POSIXACLS
	if (req->f->conn.proto_minor >= 12)
		req->ctx.umask = arg->umask;
	else
#endif
		name = (const char *) inarg + sizeof(struct fuse_open_in);

	req->f->op.create(req, nodeid, name, arg->mode, &fi);
    } else
        fuse_reply_err(req, ENOSYS);
}

static void do_open(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_open_in *arg = (const struct fuse_open_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.flags = arg->flags;

    if (req->f->op.open)
        req->f->op.open(req, nodeid, &fi);
    else
        fuse_reply_open(req, &fi);
}

static void do_read(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_read_in *arg = (const struct fuse_read_in *) inarg;

    if (req->f->op.read) {
        struct fuse_file_info fi;

        memset(&fi, 0, sizeof(fi));
        fi.fh = arg->fh;
        fi.fh_old = fi.fh;
        req->f->op.read(req, nodeid, arg->size, arg->offset, &fi);
    } else
        fuse_reply_err(req, ENOSYS);
}

static void do_write(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_write_in *arg = (const struct fuse_write_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;
    fi.writepage = arg->write_flags & 1;

    if (req->f->op.write) {
#ifdef POSIXACLS
	const char *buf;

	if (req->f->conn.proto_minor >= 12)
		buf = PARAM(arg);
	else
		buf = ((const char*)arg) + FUSE_COMPAT_WRITE_IN_SIZE;
        req->f->op.write(req, nodeid, buf, arg->size, arg->offset, &fi);
#else
        req->f->op.write(req, nodeid, PARAM(arg), arg->size, arg->offset, &fi);
#endif
    } else
        fuse_reply_err(req, ENOSYS);
}

static void do_flush(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_flush_in *arg = (const struct fuse_flush_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;
    fi.flush = 1;
    if (req->f->conn.proto_minor >= 7)
        fi.lock_owner = arg->lock_owner;

    if (req->f->op.flush)
        req->f->op.flush(req, nodeid, &fi);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_release(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_release_in *arg = (const struct fuse_release_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.flags = arg->flags;
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;
    if (req->f->conn.proto_minor >= 8) {
        fi.flush = (arg->release_flags & FUSE_RELEASE_FLUSH) ? 1 : 0;
        fi.lock_owner = arg->lock_owner;
    }

    if (req->f->op.release)
        req->f->op.release(req, nodeid, &fi);
    else
        fuse_reply_err(req, 0);
}

static void do_fsync(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_fsync_in *arg = (const struct fuse_fsync_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;

    if (req->f->op.fsync)
        req->f->op.fsync(req, nodeid, arg->fsync_flags & 1, &fi);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_opendir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_open_in *arg = (const struct fuse_open_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.flags = arg->flags;

    if (req->f->op.opendir)
        req->f->op.opendir(req, nodeid, &fi);
    else
        fuse_reply_open(req, &fi);
}

static void do_readdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_read_in *arg = (const struct fuse_read_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;

    if (req->f->op.readdir)
        req->f->op.readdir(req, nodeid, arg->size, arg->offset, &fi);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_releasedir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_release_in *arg = (const struct fuse_release_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.flags = arg->flags;
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;

    if (req->f->op.releasedir)
        req->f->op.releasedir(req, nodeid, &fi);
    else
        fuse_reply_err(req, 0);
}

static void do_fsyncdir(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_fsync_in *arg = (const struct fuse_fsync_in *) inarg;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.fh_old = fi.fh;

    if (req->f->op.fsyncdir)
        req->f->op.fsyncdir(req, nodeid, arg->fsync_flags & 1, &fi);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_statfs(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    (void) nodeid;
    (void) inarg;

    if (req->f->op.statfs)
        req->f->op.statfs(req, nodeid);
    else {
        struct statvfs buf = {
            .f_namemax = 255,
            .f_bsize = 512,
        };
        fuse_reply_statfs(req, &buf);
    }
}

static void do_setxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_setxattr_in *arg = (const struct fuse_setxattr_in *) inarg;
    const char *name = PARAM(arg);
    const char *value = name + strlen(name) + 1;

    if (req->f->op.setxattr)
        req->f->op.setxattr(req, nodeid, name, value, arg->size, arg->flags);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_getxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_getxattr_in *arg = (const struct fuse_getxattr_in *) inarg;

    if (req->f->op.getxattr)
        req->f->op.getxattr(req, nodeid, PARAM(arg), arg->size);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_listxattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_getxattr_in *arg = (const struct fuse_getxattr_in *) inarg;

    if (req->f->op.listxattr)
        req->f->op.listxattr(req, nodeid, arg->size);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_removexattr(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const char *name = (const char *) inarg;

    if (req->f->op.removexattr)
        req->f->op.removexattr(req, nodeid, name);
    else
        fuse_reply_err(req, ENOSYS);
}

static void convert_fuse_file_lock(const struct fuse_file_lock *fl,
                                   struct flock *flock)
{
    memset(flock, 0, sizeof(struct flock));
    flock->l_type = fl->type;
    flock->l_whence = SEEK_SET;
    flock->l_start = fl->start;
    if (fl->end == OFFSET_MAX)
        flock->l_len = 0;
    else
        flock->l_len = fl->end - fl->start + 1;
    flock->l_pid = fl->pid;
}

static void do_getlk(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_lk_in *arg = (const struct fuse_lk_in *) inarg;
    struct fuse_file_info fi;
    struct flock flock;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.lock_owner = arg->owner;

    convert_fuse_file_lock(&arg->lk, &flock);
    if (req->f->op.getlk)
        req->f->op.getlk(req, nodeid, &fi, &flock);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_setlk_common(fuse_req_t req, fuse_ino_t nodeid,
                            const void *inarg, int should_sleep)
{
    const struct fuse_lk_in *arg = (const struct fuse_lk_in *) inarg;
    struct fuse_file_info fi;
    struct flock flock;

    memset(&fi, 0, sizeof(fi));
    fi.fh = arg->fh;
    fi.lock_owner = arg->owner;

    convert_fuse_file_lock(&arg->lk, &flock);
    if (req->f->op.setlk)
        req->f->op.setlk(req, nodeid, &fi, &flock, should_sleep);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_setlk(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    do_setlk_common(req, nodeid, inarg, 0);
}

static void do_setlkw(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    do_setlk_common(req, nodeid, inarg, 1);
}

static int find_interrupted(struct fuse_ll *f, struct fuse_req *req)
{
    struct fuse_req *curr;

    for (curr = f->list.next; curr != &f->list; curr = curr->next) {
        if (curr->unique == req->u.i.unique) {
            curr->ctr++;
            pthread_mutex_unlock(&f->lock);

            /* Ugh, ugly locking */
            pthread_mutex_lock(&curr->lock);
            pthread_mutex_lock(&f->lock);
            curr->interrupted = 1;
            pthread_mutex_unlock(&f->lock);
            if (curr->u.ni.func)
                curr->u.ni.func(curr, curr->u.ni.data);
            pthread_mutex_unlock(&curr->lock);

            pthread_mutex_lock(&f->lock);
            curr->ctr--;
            if (!curr->ctr)
                destroy_req(curr);

            return 1;
        }
    }
    for (curr = f->interrupts.next; curr != &f->interrupts;
         curr = curr->next) {
        if (curr->u.i.unique == req->u.i.unique)
            return 1;
    }
    return 0;
}

static void do_interrupt(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_interrupt_in *arg = (const struct fuse_interrupt_in *) inarg;
    struct fuse_ll *f = req->f;

    (void) nodeid;
    if (f->debug)
        fprintf(stderr, "INTERRUPT: %llu\n", (unsigned long long) arg->unique);

    req->u.i.unique = arg->unique;

    pthread_mutex_lock(&f->lock);
    if (find_interrupted(f, req))
        destroy_req(req);
    else
        list_add_req(req, &f->interrupts);
    pthread_mutex_unlock(&f->lock);
}

static struct fuse_req *check_interrupt(struct fuse_ll *f, struct fuse_req *req)
{
    struct fuse_req *curr;

    for (curr = f->interrupts.next; curr != &f->interrupts; curr = curr->next) {
        if (curr->u.i.unique == req->unique) {
            req->interrupted = 1;
            list_del_req(curr);
            free(curr);
            return NULL;
        }
    }
    curr = f->interrupts.next;
    if (curr != &f->interrupts) {
        list_del_req(curr);
        list_init_req(curr);
        return curr;
    } else
        return NULL;
}

static void do_bmap(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_bmap_in *arg = (const struct fuse_bmap_in *) inarg;

    if (req->f->op.bmap)
        req->f->op.bmap(req, nodeid, arg->blocksize, arg->block);
    else
        fuse_reply_err(req, ENOSYS);
}

static void do_init(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    const struct fuse_init_in *arg = (const struct fuse_init_in *) inarg;
    struct fuse_init_out outarg;
    struct fuse_ll *f = req->f;
    size_t bufsize = fuse_chan_bufsize(req->ch);

    (void) nodeid;
    if (f->debug) {
        fprintf(stderr, "INIT: %u.%u\n", arg->major, arg->minor);
        if (arg->major > 7 || (arg->major == 7 && arg->minor >= 6)) {
            fprintf(stderr, "flags=0x%08x\n", arg->flags);
            fprintf(stderr, "max_readahead=0x%08x\n", arg->max_readahead);
        }
    }
    f->conn.proto_major = arg->major;
    f->conn.proto_minor = arg->minor;

    if (arg->major < 7) {
        fprintf(stderr, "fuse: unsupported protocol version: %u.%u\n",
                arg->major, arg->minor);
        fuse_reply_err(req, EPROTO);
        return;
    }

    if (arg->major > 7 || (arg->major == 7 && arg->minor >= 6)) {
        if (f->conn.async_read)
            f->conn.async_read = arg->flags & FUSE_ASYNC_READ;
        if (arg->max_readahead < f->conn.max_readahead)
            f->conn.max_readahead = arg->max_readahead;
#ifdef POSIXACLS
	if (arg->flags & FUSE_DONT_MASK)
	    f->conn.capable |= FUSE_CAP_DONT_MASK;
#endif
    } else {
        f->conn.async_read = 0;
        f->conn.max_readahead = 0;
    }

    if (bufsize < FUSE_MIN_READ_BUFFER) {
        fprintf(stderr, "fuse: warning: buffer size too small: %zu\n",
                bufsize);
        bufsize = FUSE_MIN_READ_BUFFER;
    }

    bufsize -= 4096;
    if (bufsize < f->conn.max_write)
        f->conn.max_write = bufsize;

    f->got_init = 1;
    if (f->op.init)
        f->op.init(f->userdata, &f->conn);

    memset(&outarg, 0, sizeof(outarg));
    outarg.major = FUSE_KERNEL_VERSION;
	/*
	 * if POSIXACLS is not set, protocol 7.8 provides a good
	 * compatibility with older kernel modules.
	 * if POSIXACLS is set, we try to use protocol 7.12 supposed
	 * to have the ability to process the umask conditionnally,
	 * but, when using an older kernel module, we fallback to 7.8
	 */
#ifdef POSIXACLS
    if (arg->major > 7 || (arg->major == 7 && arg->minor >= 12))
	    outarg.minor = FUSE_KERNEL_MINOR_VERSION;
    else
	    outarg.minor = FUSE_KERNEL_MINOR_FALLBACK;
#else
    outarg.minor = FUSE_KERNEL_MINOR_VERSION;
#endif
    if (f->conn.async_read)
        outarg.flags |= FUSE_ASYNC_READ;
    if (f->op.getlk && f->op.setlk)
        outarg.flags |= FUSE_POSIX_LOCKS;
#ifdef POSIXACLS
    if (f->conn.want & FUSE_CAP_DONT_MASK)
	outarg.flags |= FUSE_DONT_MASK;
#endif
    outarg.max_readahead = f->conn.max_readahead;
    outarg.max_write = f->conn.max_write;

    if (f->debug) {
        fprintf(stderr, "   INIT: %u.%u\n", outarg.major, outarg.minor);
        fprintf(stderr, "   flags=0x%08x\n", outarg.flags);
        fprintf(stderr, "   max_readahead=0x%08x\n", outarg.max_readahead);
        fprintf(stderr, "   max_write=0x%08x\n", outarg.max_write);
    }

    send_reply_ok(req, &outarg, arg->minor < 5 ? 8 : sizeof(outarg));
}

static void do_destroy(fuse_req_t req, fuse_ino_t nodeid, const void *inarg)
{
    struct fuse_ll *f = req->f;

    (void) nodeid;
    (void) inarg;

    f->got_destroy = 1;
    if (f->op.destroy)
        f->op.destroy(f->userdata);

    send_reply_ok(req, NULL, 0);
}

void *fuse_req_userdata(fuse_req_t req)
{
    return req->f->userdata;
}

const struct fuse_ctx *fuse_req_ctx(fuse_req_t req)
{
    return &req->ctx;
}

/*
 * The size of fuse_ctx got extended, so need to be careful about
 * incompatibility (i.e. a new binary cannot work with an old
 * library).
 */
const struct fuse_ctx *fuse_req_ctx_compat24(fuse_req_t req);
const struct fuse_ctx *fuse_req_ctx_compat24(fuse_req_t req)
{
	return fuse_req_ctx(req);
}
//FUSE_SYMVER(".symver fuse_req_ctx_compat24,fuse_req_ctx@FUSE_2.4");

void fuse_req_interrupt_func(fuse_req_t req, fuse_interrupt_func_t func,
                             void *data)
{
    pthread_mutex_lock(&req->lock);
    req->u.ni.func = func;
    req->u.ni.data = data;
    if (req->interrupted && func)
        func(req, data);
    pthread_mutex_unlock(&req->lock);
}

int fuse_req_interrupted(fuse_req_t req)
{
    int interrupted;

    pthread_mutex_lock(&req->f->lock);
    interrupted = req->interrupted;
    pthread_mutex_unlock(&req->f->lock);

    return interrupted;
}

static struct {
    void (*func)(fuse_req_t, fuse_ino_t, const void *);
    const char *name;
} fuse_ll_ops[] = {
    [FUSE_LOOKUP]      = { do_lookup,      "LOOKUP"      },
    [FUSE_FORGET]      = { do_forget,      "FORGET"      },
    [FUSE_GETATTR]     = { do_getattr,     "GETATTR"     },
    [FUSE_SETATTR]     = { do_setattr,     "SETATTR"     },
    [FUSE_READLINK]    = { do_readlink,    "READLINK"    },
    [FUSE_SYMLINK]     = { do_symlink,     "SYMLINK"     },
    [FUSE_MKNOD]       = { do_mknod,       "MKNOD"       },
    [FUSE_MKDIR]       = { do_mkdir,       "MKDIR"       },
    [FUSE_UNLINK]      = { do_unlink,      "UNLINK"      },
    [FUSE_RMDIR]       = { do_rmdir,       "RMDIR"       },
    [FUSE_RENAME]      = { do_rename,      "RENAME"      },
    [FUSE_LINK]        = { do_link,        "LINK"        },
    [FUSE_OPEN]        = { do_open,        "OPEN"        },
    [FUSE_READ]        = { do_read,        "READ"        },
    [FUSE_WRITE]       = { do_write,       "WRITE"       },
    [FUSE_STATFS]      = { do_statfs,      "STATFS"      },
    [FUSE_RELEASE]     = { do_release,     "RELEASE"     },
    [FUSE_FSYNC]       = { do_fsync,       "FSYNC"       },
    [FUSE_SETXATTR]    = { do_setxattr,    "SETXATTR"    },
    [FUSE_GETXATTR]    = { do_getxattr,    "GETXATTR"    },
    [FUSE_LISTXATTR]   = { do_listxattr,   "LISTXATTR"   },
    [FUSE_REMOVEXATTR] = { do_removexattr, "REMOVEXATTR" },
    [FUSE_FLUSH]       = { do_flush,       "FLUSH"       },
    [FUSE_INIT]        = { do_init,        "INIT"        },
    [FUSE_OPENDIR]     = { do_opendir,     "OPENDIR"     },
    [FUSE_READDIR]     = { do_readdir,     "READDIR"     },
    [FUSE_RELEASEDIR]  = { do_releasedir,  "RELEASEDIR"  },
    [FUSE_FSYNCDIR]    = { do_fsyncdir,    "FSYNCDIR"    },
    [FUSE_GETLK]       = { do_getlk,       "GETLK"       },
    [FUSE_SETLK]       = { do_setlk,       "SETLK"       },
    [FUSE_SETLKW]      = { do_setlkw,      "SETLKW"      },
    [FUSE_ACCESS]      = { do_access,      "ACCESS"      },
    [FUSE_CREATE]      = { do_create,      "CREATE"      },
    [FUSE_INTERRUPT]   = { do_interrupt,   "INTERRUPT"   },
    [FUSE_BMAP]        = { do_bmap,        "BMAP"        },
    [FUSE_DESTROY]     = { do_destroy,     "DESTROY"     },
};

#define FUSE_MAXOP (sizeof(fuse_ll_ops) / sizeof(fuse_ll_ops[0]))

static const char *opname(enum fuse_opcode opcode)
{
    if (opcode >= FUSE_MAXOP || !fuse_ll_ops[opcode].name)
        return "???";
    else
        return fuse_ll_ops[opcode].name;
}

static void fuse_ll_process(void *data, const char *buf, size_t len,
                     struct fuse_chan *ch)
{
    struct fuse_ll *f = (struct fuse_ll *) data;
    const struct fuse_in_header *in = (const struct fuse_in_header *) buf;
    const void *inarg = buf + sizeof(struct fuse_in_header);
    struct fuse_req *req;

    if (f->debug)
        fprintf(stderr, "unique: %llu, opcode: %s (%i), nodeid: %lu, insize: %zu\n",
                (unsigned long long) in->unique,
                opname((enum fuse_opcode) in->opcode), in->opcode,
                (unsigned long) in->nodeid, len);

    req = (struct fuse_req *) calloc(1, sizeof(struct fuse_req));
    if (req == NULL) {
        fprintf(stderr, "fuse: failed to allocate request\n");
        return;
    }

    req->f = f;
    req->unique = in->unique;
    req->ctx.uid = in->uid;
    req->ctx.gid = in->gid;
    req->ctx.pid = in->pid;
    req->ch = ch;
    req->ctr = 1;
    list_init_req(req);
    fuse_mutex_init(&req->lock);

    if (!f->got_init && in->opcode != FUSE_INIT)
        fuse_reply_err(req, EIO);
    else if (f->allow_root && in->uid != f->owner && in->uid != 0 &&
             in->opcode != FUSE_INIT && in->opcode != FUSE_READ &&
             in->opcode != FUSE_WRITE && in->opcode != FUSE_FSYNC &&
             in->opcode != FUSE_RELEASE && in->opcode != FUSE_READDIR &&
             in->opcode != FUSE_FSYNCDIR && in->opcode != FUSE_RELEASEDIR) {
        fuse_reply_err(req, EACCES);
    } else if (in->opcode >= FUSE_MAXOP || !fuse_ll_ops[in->opcode].func)
        fuse_reply_err(req, ENOSYS);
    else {
        if (in->opcode != FUSE_INTERRUPT) {
            struct fuse_req *intr;
            pthread_mutex_lock(&f->lock);
            intr = check_interrupt(f, req);
            list_add_req(req, &f->list);
            pthread_mutex_unlock(&f->lock);
            if (intr)
                fuse_reply_err(intr, EAGAIN);
        }
        fuse_ll_ops[in->opcode].func(req, in->nodeid, inarg);
    }
}

enum {
    KEY_HELP,
    KEY_VERSION,
};

static struct fuse_opt fuse_ll_opts[] = {
    { "debug", offsetof(struct fuse_ll, debug), 1 },
    { "-d", offsetof(struct fuse_ll, debug), 1 },
    { "allow_root", offsetof(struct fuse_ll, allow_root), 1 },
    { "max_write=%u", offsetof(struct fuse_ll, conn.max_write), 0 },
    { "max_readahead=%u", offsetof(struct fuse_ll, conn.max_readahead), 0 },
    { "async_read", offsetof(struct fuse_ll, conn.async_read), 1 },
    { "sync_read", offsetof(struct fuse_ll, conn.async_read), 0 },
    FUSE_OPT_KEY("max_read=", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_KEY("-V", KEY_VERSION),
    FUSE_OPT_KEY("--version", KEY_VERSION),
    FUSE_OPT_END
};

static void fuse_ll_version(void)
{
    fprintf(stderr, "using FUSE kernel interface version %i.%i\n",
            FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);
}

static void fuse_ll_help(void)
{
    fprintf(stderr,
"    -o max_write=N         set maximum size of write requests\n"
"    -o max_readahead=N     set maximum readahead\n"
"    -o async_read          perform reads asynchronously (default)\n"
"    -o sync_read           perform reads synchronously\n");
}

static int fuse_ll_opt_proc(void *data, const char *arg, int key,
                            struct fuse_args *outargs)
{
    (void) data; (void) outargs;

    switch (key) {
    case KEY_HELP:
        fuse_ll_help();
        break;

    case KEY_VERSION:
        fuse_ll_version();
        break;

    default:
        fprintf(stderr, "fuse: unknown option `%s'\n", arg);
    }

    return -1;
}

static void fuse_ll_destroy(void *data)
{
    struct fuse_ll *f = (struct fuse_ll *) data;

    if (f->got_init && !f->got_destroy) {
        if (f->op.destroy)
            f->op.destroy(f->userdata);
    }

    pthread_mutex_destroy(&f->lock);
    free(f);
}

struct fuse_session *fuse_lowlevel_new(struct fuse_args *args,
				       const struct fuse_lowlevel_ops *op,
                                       size_t op_size, void *userdata)
{
    struct fuse_ll *f;
    struct fuse_session *se;
    struct fuse_session_ops sop = {
        .process = fuse_ll_process,
        .destroy = fuse_ll_destroy,
    };

    if (sizeof(struct fuse_lowlevel_ops) < op_size) {
        fprintf(stderr, "fuse: warning: library too old, some operations may not work\n");
        op_size = sizeof(struct fuse_lowlevel_ops);
    }

    f = (struct fuse_ll *) calloc(1, sizeof(struct fuse_ll));
    if (f == NULL) {
        fprintf(stderr, "fuse: failed to allocate fuse object\n");
        goto out;
    }

    f->conn.async_read = 1;
    f->conn.max_write = UINT_MAX;
    f->conn.max_readahead = UINT_MAX;
    list_init_req(&f->list);
    list_init_req(&f->interrupts);
    fuse_mutex_init(&f->lock);

    if (fuse_opt_parse(args, f, fuse_ll_opts, fuse_ll_opt_proc) == -1)
        goto out_free;

    memcpy(&f->op, op, op_size);
    f->owner = getuid();
    f->userdata = userdata;

    se = fuse_session_new(&sop, f);
    if (!se)
        goto out_free;

    return se;

 out_free:
    free(f);
 out:
    return NULL;
}

