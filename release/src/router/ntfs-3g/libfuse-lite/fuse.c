/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_i.h"
#include "fuse_lowlevel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/time.h>

#define FUSE_DEFAULT_INTR_SIGNAL SIGUSR1

#define FUSE_UNKNOWN_INO 0xffffffff
#define OFFSET_MAX 0x7fffffffffffffffLL

struct fuse_config {
    unsigned int uid;
    unsigned int gid;
    unsigned int  umask;
    double entry_timeout;
    double negative_timeout;
    double attr_timeout;
    double ac_attr_timeout;
    int ac_attr_timeout_set;
    int debug;
    int hard_remove;
    int use_ino;
    int readdir_ino;
    int set_mode;
    int set_uid;
    int set_gid;
    int direct_io;
    int kernel_cache;
    int intr;
    int intr_signal;
    int help;
};

struct fuse_fs {
    struct fuse_operations op;
    void *user_data;
};

struct fuse {
    struct fuse_session *se;
    struct node **name_table;
    size_t name_table_size;
    struct node **id_table;
    size_t id_table_size;
    fuse_ino_t ctr;
    unsigned int generation;
    unsigned int hidectr;
    pthread_mutex_t lock;
    pthread_rwlock_t tree_lock;
    struct fuse_config conf;
    int intr_installed;
    struct fuse_fs *fs;
    int utime_omit_ok;
};

struct lock {
    int type;
    off_t start;
    off_t end;
    pid_t pid;
    uint64_t owner;
    struct lock *next;
};

struct node {
    struct node *name_next;
    struct node *id_next;
    fuse_ino_t nodeid;
    unsigned int generation;
    int refctr;
    struct node *parent;
    char *name;
    uint64_t nlookup;
    int open_count;
    int is_hidden;
    struct lock *locks;
};

struct fuse_dh {
    pthread_mutex_t lock;
    struct fuse *fuse;
    fuse_req_t req;
    char *contents;
    int allocated;
    unsigned len;
    unsigned size;
    unsigned needlen;
    int filled;
    uint64_t fh;
    int error;
    fuse_ino_t nodeid;
};

struct fuse_context_i {
    struct fuse_context ctx;
    fuse_req_t req;
};

static pthread_key_t fuse_context_key;
static pthread_mutex_t fuse_context_lock = PTHREAD_MUTEX_INITIALIZER;
static int fuse_context_ref;

static struct node *get_node_nocheck(struct fuse *f, fuse_ino_t nodeid)
{
    size_t hash = nodeid % f->id_table_size;
    struct node *node;

    for (node = f->id_table[hash]; node != NULL; node = node->id_next)
        if (node->nodeid == nodeid)
            return node;

    return NULL;
}

static struct node *get_node(struct fuse *f, fuse_ino_t nodeid)
{
    struct node *node = get_node_nocheck(f, nodeid);
    if (!node) {
        fprintf(stderr, "fuse internal error: node %llu not found\n",
                (unsigned long long) nodeid);
        abort();
    }
    return node;
}

static void free_node(struct node *node)
{
    free(node->name);
    free(node);
}

static void unhash_id(struct fuse *f, struct node *node)
{
    size_t hash = node->nodeid % f->id_table_size;
    struct node **nodep = &f->id_table[hash];

    for (; *nodep != NULL; nodep = &(*nodep)->id_next)
        if (*nodep == node) {
            *nodep = node->id_next;
            return;
        }
}

static void hash_id(struct fuse *f, struct node *node)
{
    size_t hash = node->nodeid % f->id_table_size;
    node->id_next = f->id_table[hash];
    f->id_table[hash] = node;
}

static unsigned int name_hash(struct fuse *f, fuse_ino_t parent,
                              const char *name)
{
    unsigned int hash = *name;

    if (hash)
        for (name += 1; *name != '\0'; name++)
            hash = (hash << 5) - hash + *name;

    return (hash + parent) % f->name_table_size;
}

static void unref_node(struct fuse *f, struct node *node);

static void unhash_name(struct fuse *f, struct node *node)
{
    if (node->name) {
        size_t hash = name_hash(f, node->parent->nodeid, node->name);
        struct node **nodep = &f->name_table[hash];

        for (; *nodep != NULL; nodep = &(*nodep)->name_next)
            if (*nodep == node) {
                *nodep = node->name_next;
                node->name_next = NULL;
                unref_node(f, node->parent);
                free(node->name);
                node->name = NULL;
                node->parent = NULL;
                return;
            }
        fprintf(stderr, "fuse internal error: unable to unhash node: %llu\n",
                (unsigned long long) node->nodeid);
        abort();
    }
}

static int hash_name(struct fuse *f, struct node *node, fuse_ino_t parentid,
                     const char *name)
{
    size_t hash = name_hash(f, parentid, name);
    struct node *parent = get_node(f, parentid);
    node->name = strdup(name);
    if (node->name == NULL)
        return -1;

    parent->refctr ++;
    node->parent = parent;
    node->name_next = f->name_table[hash];
    f->name_table[hash] = node;
    return 0;
}

static void delete_node(struct fuse *f, struct node *node)
{
    if (f->conf.debug)
        fprintf(stderr, "delete: %llu\n", (unsigned long long) node->nodeid);

    assert(!node->name);
    unhash_id(f, node);
    free_node(node);
}

static void unref_node(struct fuse *f, struct node *node)
{
    assert(node->refctr > 0);
    node->refctr --;
    if (!node->refctr)
        delete_node(f, node);
}

static fuse_ino_t next_id(struct fuse *f)
{
    do {
        f->ctr = (f->ctr + 1) & 0xffffffff;
        if (!f->ctr)
            f->generation ++;
    } while (f->ctr == 0 || f->ctr == FUSE_UNKNOWN_INO ||
             get_node_nocheck(f, f->ctr) != NULL);
    return f->ctr;
}

static struct node *lookup_node(struct fuse *f, fuse_ino_t parent,
                                const char *name)
{
    size_t hash = name_hash(f, parent, name);
    struct node *node;

    for (node = f->name_table[hash]; node != NULL; node = node->name_next)
        if (node->parent->nodeid == parent && strcmp(node->name, name) == 0)
            return node;

    return NULL;
}

static struct node *find_node(struct fuse *f, fuse_ino_t parent,
                              const char *name)
{
    struct node *node;

    pthread_mutex_lock(&f->lock);
    node = lookup_node(f, parent, name);
    if (node == NULL) {
        node = (struct node *) calloc(1, sizeof(struct node));
        if (node == NULL)
            goto out_err;

        node->refctr = 1;
        node->nodeid = next_id(f);
        node->open_count = 0;
        node->is_hidden = 0;
        node->generation = f->generation;
        if (hash_name(f, node, parent, name) == -1) {
            free(node);
            node = NULL;
            goto out_err;
        }
        hash_id(f, node);
    }
    node->nlookup ++;
 out_err:
    pthread_mutex_unlock(&f->lock);
    return node;
}

static char *add_name(char **buf, unsigned *bufsize, char *s, const char *name)
{
    size_t len = strlen(name);

    if (s - len <= *buf) {
	unsigned pathlen = *bufsize - (s - *buf);
	unsigned newbufsize = *bufsize;
	char *newbuf;

	while (newbufsize < pathlen + len + 1) {
	    if (newbufsize >= 0x80000000)
	    	newbufsize = 0xffffffff;
	    else
	    	newbufsize *= 2;
	}

	newbuf = realloc(*buf, newbufsize);
	if (newbuf == NULL)
		return NULL;

	*buf = newbuf;
	s = newbuf + newbufsize - pathlen;
	memmove(s, newbuf + *bufsize - pathlen, pathlen);
	*bufsize = newbufsize;
    }
    s -= len;
    strncpy(s, name, len);
    s--;
    *s = '/';

    return s;
}

static char *get_path_name(struct fuse *f, fuse_ino_t nodeid, const char *name)
{
    unsigned bufsize = 256;
    char *buf;
    char *s;
    struct node *node;

    buf = malloc(bufsize);
    if (buf == NULL)
            return NULL;

    s = buf + bufsize - 1;
    *s = '\0';

    if (name != NULL) {
        s = add_name(&buf, &bufsize, s, name);
        if (s == NULL)
            goto out_free;
    }

    pthread_mutex_lock(&f->lock);
    for (node = get_node(f, nodeid); node && node->nodeid != FUSE_ROOT_ID;
         node = node->parent) {
        if (node->name == NULL) {
            s = NULL;
            break;
        }

        s = add_name(&buf, &bufsize, s, node->name);
        if (s == NULL)
            break;
    }
    pthread_mutex_unlock(&f->lock);

    if (node == NULL || s == NULL)
        goto out_free;
    
    if (s[0])
            memmove(buf, s, bufsize - (s - buf));
    else
            strcpy(buf, "/");
    return buf;
    
out_free:
    free(buf);
    return NULL;
}

static char *get_path(struct fuse *f, fuse_ino_t nodeid)
{
    return get_path_name(f, nodeid, NULL);
}

static void forget_node(struct fuse *f, fuse_ino_t nodeid, uint64_t nlookup)
{
    struct node *node;
    if (nodeid == FUSE_ROOT_ID)
        return;
    pthread_mutex_lock(&f->lock);
    node = get_node(f, nodeid);
    assert(node->nlookup >= nlookup);
    node->nlookup -= nlookup;
    if (!node->nlookup) {
        unhash_name(f, node);
        unref_node(f, node);
    }
    pthread_mutex_unlock(&f->lock);
}

static void remove_node(struct fuse *f, fuse_ino_t dir, const char *name)
{
    struct node *node;

    pthread_mutex_lock(&f->lock);
    node = lookup_node(f, dir, name);
    if (node != NULL)
        unhash_name(f, node);
    pthread_mutex_unlock(&f->lock);
}

static int rename_node(struct fuse *f, fuse_ino_t olddir, const char *oldname,
                        fuse_ino_t newdir, const char *newname, int hide)
{
    struct node *node;
    struct node *newnode;
    int err = 0;

    pthread_mutex_lock(&f->lock);
    node  = lookup_node(f, olddir, oldname);
    newnode  = lookup_node(f, newdir, newname);
    if (node == NULL)
        goto out;

    if (newnode != NULL) {
        if (hide) {
            fprintf(stderr, "fuse: hidden file got created during hiding\n");
            err = -EBUSY;
            goto out;
        }
        unhash_name(f, newnode);
    }

    unhash_name(f, node);
    if (hash_name(f, node, newdir, newname) == -1) {
        err = -ENOMEM;
        goto out;
    }

    if (hide)
        node->is_hidden = 1;

 out:
    pthread_mutex_unlock(&f->lock);
    return err;
}

static void set_stat(struct fuse *f, fuse_ino_t nodeid, struct stat *stbuf)
{
    if (!f->conf.use_ino)
        stbuf->st_ino = nodeid;
    if (f->conf.set_mode)
        stbuf->st_mode = (stbuf->st_mode & S_IFMT) | (0777 & ~f->conf.umask);
    if (f->conf.set_uid)
        stbuf->st_uid = f->conf.uid;
    if (f->conf.set_gid)
        stbuf->st_gid = f->conf.gid;
}

static struct fuse *req_fuse(fuse_req_t req)
{
    return (struct fuse *) fuse_req_userdata(req);
}

static void fuse_intr_sighandler(int sig)
{
    (void) sig;
    /* Nothing to do */
}

struct fuse_intr_data {
    pthread_t id;
    pthread_cond_t cond;
    int finished;
};

static void fuse_interrupt(fuse_req_t req, void *d_)
{
    struct fuse_intr_data *d = d_;
    struct fuse *f = req_fuse(req);

    if (d->id == pthread_self())
        return;

    pthread_mutex_lock(&f->lock);
    while (!d->finished) {
        struct timeval now;
        struct timespec timeout;

        pthread_kill(d->id, f->conf.intr_signal);
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 1;
        timeout.tv_nsec = now.tv_usec * 1000;
        pthread_cond_timedwait(&d->cond, &f->lock, &timeout);
    }
    pthread_mutex_unlock(&f->lock);
}

static void fuse_do_finish_interrupt(struct fuse *f, fuse_req_t req,
                                     struct fuse_intr_data *d)
{
    pthread_mutex_lock(&f->lock);
    d->finished = 1;
    pthread_cond_broadcast(&d->cond);
    pthread_mutex_unlock(&f->lock);
    fuse_req_interrupt_func(req, NULL, NULL);
    pthread_cond_destroy(&d->cond);
}

static void fuse_do_prepare_interrupt(fuse_req_t req, struct fuse_intr_data *d)
{
    d->id = pthread_self();
    pthread_cond_init(&d->cond, NULL);
    d->finished = 0;
    fuse_req_interrupt_func(req, fuse_interrupt, d);
}

static void fuse_finish_interrupt(struct fuse *f, fuse_req_t req,
                                         struct fuse_intr_data *d)
{
    if (f->conf.intr)
        fuse_do_finish_interrupt(f, req, d);
}

static void fuse_prepare_interrupt(struct fuse *f, fuse_req_t req,
                                          struct fuse_intr_data *d)
{
    if (f->conf.intr)
        fuse_do_prepare_interrupt(req, d);
}

int fuse_fs_getattr(struct fuse_fs *fs, const char *path, struct stat *buf)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.getattr)
        return fs->op.getattr(path, buf);
    else
        return -ENOSYS;
}

int fuse_fs_fgetattr(struct fuse_fs *fs, const char *path, struct stat *buf,
                     struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.fgetattr)
        return fs->op.fgetattr(path, buf, fi);
    else if (fs->op.getattr)
        return fs->op.getattr(path, buf);
    else
        return -ENOSYS;
}

int fuse_fs_rename(struct fuse_fs *fs, const char *oldpath,
                   const char *newpath)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.rename)
        return fs->op.rename(oldpath, newpath);
    else
        return -ENOSYS;
}

int fuse_fs_unlink(struct fuse_fs *fs, const char *path)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.unlink)
        return fs->op.unlink(path);
    else
        return -ENOSYS;
}

int fuse_fs_rmdir(struct fuse_fs *fs, const char *path)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.rmdir)
        return fs->op.rmdir(path);
    else
        return -ENOSYS;
}

int fuse_fs_symlink(struct fuse_fs *fs, const char *linkname, const char *path)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.symlink)
        return fs->op.symlink(linkname, path);
    else
        return -ENOSYS;
}

int fuse_fs_link(struct fuse_fs *fs, const char *oldpath, const char *newpath)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.link)
        return fs->op.link(oldpath, newpath);
    else
        return -ENOSYS;
}

int fuse_fs_release(struct fuse_fs *fs,  const char *path,
                    struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.release)
        return fs->op.release(path, fi);
    else
        return 0;
}

int fuse_fs_opendir(struct fuse_fs *fs, const char *path,
                    struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.opendir)
        return fs->op.opendir(path, fi);
    else
        return 0;
}

int fuse_fs_open(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.open)
        return fs->op.open(path, fi);
    else
        return 0;
}

int fuse_fs_read(struct fuse_fs *fs, const char *path, char *buf, size_t size,
                 off_t off, struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.read)
        return fs->op.read(path, buf, size, off, fi);
    else
        return -ENOSYS;
}

int fuse_fs_write(struct fuse_fs *fs, const char *path, const char *buf,
                  size_t size, off_t off, struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.write)
        return fs->op.write(path, buf, size, off, fi);
    else
        return -ENOSYS;
}

int fuse_fs_fsync(struct fuse_fs *fs, const char *path, int datasync,
                  struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.fsync)
        return fs->op.fsync(path, datasync, fi);
    else
        return -ENOSYS;
}

int fuse_fs_fsyncdir(struct fuse_fs *fs, const char *path, int datasync,
                     struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.fsyncdir)
        return fs->op.fsyncdir(path, datasync, fi);
    else
        return -ENOSYS;
}

int fuse_fs_flush(struct fuse_fs *fs, const char *path,
                  struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.flush)
        return fs->op.flush(path, fi);
    else
        return -ENOSYS;
}

int fuse_fs_statfs(struct fuse_fs *fs, const char *path, struct statvfs *buf)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.statfs)
        return fs->op.statfs(path, buf);
    else {
        buf->f_namemax = 255;
        buf->f_bsize = 512;
        return 0;
    }
}

int fuse_fs_releasedir(struct fuse_fs *fs, const char *path,
                       struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.releasedir)
        return fs->op.releasedir(path, fi);
    else
        return 0;
}

int fuse_fs_readdir(struct fuse_fs *fs, const char *path, void *buf,
                    fuse_fill_dir_t filler, off_t off,
                    struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.readdir)
        return fs->op.readdir(path, buf, filler, off, fi);
    else
        return -ENOSYS;
}

int fuse_fs_create(struct fuse_fs *fs, const char *path, mode_t mode,
                   struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.create)
        return fs->op.create(path, mode, fi);
    else
        return -ENOSYS;
}

int fuse_fs_lock(struct fuse_fs *fs, const char *path,
                 struct fuse_file_info *fi, int cmd, struct flock *lock)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.lock)
        return fs->op.lock(path, fi, cmd, lock);
    else
        return -ENOSYS;
}

int fuse_fs_chown(struct fuse_fs *fs, const char *path, uid_t uid, gid_t gid)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.chown)
        return fs->op.chown(path, uid, gid);
    else
        return -ENOSYS;
}

int fuse_fs_truncate(struct fuse_fs *fs, const char *path, off_t size)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.truncate)
        return fs->op.truncate(path, size);
    else
        return -ENOSYS;
}

int fuse_fs_ftruncate(struct fuse_fs *fs, const char *path, off_t size,
                      struct fuse_file_info *fi)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.ftruncate)
        return fs->op.ftruncate(path, size, fi);
    else if (fs->op.truncate)
        return fs->op.truncate(path, size);
    else
        return -ENOSYS;
}

int fuse_fs_utimens(struct fuse_fs *fs, const char *path,
                    const struct timespec tv[2])
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.utimens)
        return fs->op.utimens(path, tv);
    else if(fs->op.utime) {
        struct utimbuf buf;
        buf.actime = tv[0].tv_sec;
        buf.modtime = tv[1].tv_sec;
        return fs->op.utime(path, &buf);
    } else
        return -ENOSYS;
}

int fuse_fs_access(struct fuse_fs *fs, const char *path, int mask)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.access)
        return fs->op.access(path, mask);
    else
        return -ENOSYS;
}

int fuse_fs_readlink(struct fuse_fs *fs, const char *path, char *buf,
                     size_t len)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.readlink)
        return fs->op.readlink(path, buf, len);
    else
        return -ENOSYS;
}

int fuse_fs_mknod(struct fuse_fs *fs, const char *path, mode_t mode,
                  dev_t rdev)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.mknod)
        return fs->op.mknod(path, mode, rdev);
    else
        return -ENOSYS;
}

int fuse_fs_mkdir(struct fuse_fs *fs, const char *path, mode_t mode)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.mkdir)
        return fs->op.mkdir(path, mode);
    else
        return -ENOSYS;
}

int fuse_fs_setxattr(struct fuse_fs *fs, const char *path, const char *name,
                     const char *value, size_t size, int flags)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.setxattr)
        return fs->op.setxattr(path, name, value, size, flags);
    else
        return -ENOSYS;
}

int fuse_fs_getxattr(struct fuse_fs *fs, const char *path, const char *name,
                     char *value, size_t size)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.getxattr)
        return fs->op.getxattr(path, name, value, size);
    else
        return -ENOSYS;
}

int fuse_fs_listxattr(struct fuse_fs *fs, const char *path, char *list,
                      size_t size)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.listxattr)
        return fs->op.listxattr(path, list, size);
    else
        return -ENOSYS;
}

int fuse_fs_bmap(struct fuse_fs *fs, const char *path, size_t blocksize,
                 uint64_t *idx)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.bmap)
        return fs->op.bmap(path, blocksize, idx);
    else
        return -ENOSYS;
}

int fuse_fs_removexattr(struct fuse_fs *fs, const char *path, const char *name)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.removexattr)
        return fs->op.removexattr(path, name);
    else
        return -ENOSYS;
}

static int is_open(struct fuse *f, fuse_ino_t dir, const char *name)
{
    struct node *node;
    int isopen = 0;
    pthread_mutex_lock(&f->lock);
    node = lookup_node(f, dir, name);
    if (node && node->open_count > 0)
        isopen = 1;
    pthread_mutex_unlock(&f->lock);
    return isopen;
}

static char *hidden_name(struct fuse *f, fuse_ino_t dir, const char *oldname,
                         char *newname, size_t bufsize)
{
    struct stat buf;
    struct node *node;
    struct node *newnode;
    char *newpath;
    int res;
    int failctr = 10;

    do {
        pthread_mutex_lock(&f->lock);
        node = lookup_node(f, dir, oldname);
        if (node == NULL) {
            pthread_mutex_unlock(&f->lock);
            return NULL;
        }
        do {
            f->hidectr ++;
            snprintf(newname, bufsize, ".fuse_hidden%08x%08x",
                     (unsigned int) node->nodeid, f->hidectr);
            newnode = lookup_node(f, dir, newname);
        } while(newnode);
        pthread_mutex_unlock(&f->lock);

        newpath = get_path_name(f, dir, newname);
        if (!newpath)
            break;

        res = fuse_fs_getattr(f->fs, newpath, &buf);
        if (res == -ENOENT)
            break;
        free(newpath);
        newpath = NULL;
    } while(res == 0 && --failctr);

    return newpath;
}

static int hide_node(struct fuse *f, const char *oldpath,
                     fuse_ino_t dir, const char *oldname)
{
    char newname[64];
    char *newpath;
    int err = -EBUSY;

    newpath = hidden_name(f, dir, oldname, newname, sizeof(newname));
    if (newpath) {
        err = fuse_fs_rename(f->fs, oldpath, newpath);
        if (!err)
            err = rename_node(f, dir, oldname, dir, newname, 1);
        free(newpath);
    }
    return err;
}

static int lookup_path(struct fuse *f, fuse_ino_t nodeid,
                       const char *name, const char *path,
                       struct fuse_entry_param *e, struct fuse_file_info *fi)
{
    int res;

    memset(e, 0, sizeof(struct fuse_entry_param));
    if (fi)
        res = fuse_fs_fgetattr(f->fs, path, &e->attr, fi);
    else
        res = fuse_fs_getattr(f->fs, path, &e->attr);
    if (res == 0) {
        struct node *node;

        node = find_node(f, nodeid, name);
        if (node == NULL)
            res = -ENOMEM;
        else {
            e->ino = node->nodeid;
            e->generation = node->generation;
            e->entry_timeout = f->conf.entry_timeout;
            e->attr_timeout = f->conf.attr_timeout;
            set_stat(f, e->ino, &e->attr);
            if (f->conf.debug)
                fprintf(stderr, "   NODEID: %lu\n", (unsigned long) e->ino);
        }
    }
    return res;
}

static struct fuse_context_i *fuse_get_context_internal(void)
{
    struct fuse_context_i *c;

    c = (struct fuse_context_i *) pthread_getspecific(fuse_context_key);
    if (c == NULL) {
        c = (struct fuse_context_i *) malloc(sizeof(struct fuse_context_i));
        if (c == NULL) {
            /* This is hard to deal with properly, so just abort.  If
               memory is so low that the context cannot be allocated,
               there's not much hope for the filesystem anyway */ 
            fprintf(stderr, "fuse: failed to allocate thread specific data\n");
            abort();
        }
        pthread_setspecific(fuse_context_key, c);
    }
    return c;
}

static void fuse_freecontext(void *data)
{
    free(data);
}

static int fuse_create_context_key(void)
{
    int err = 0;
    pthread_mutex_lock(&fuse_context_lock);
    if (!fuse_context_ref) {
        err = pthread_key_create(&fuse_context_key, fuse_freecontext);
        if (err) {
            fprintf(stderr, "fuse: failed to create thread specific key: %s\n",
                    strerror(err));
            pthread_mutex_unlock(&fuse_context_lock);
            return -1;
        }
    }
    fuse_context_ref++;
    pthread_mutex_unlock(&fuse_context_lock);
    return 0;
}

static void fuse_delete_context_key(void)
{
    pthread_mutex_lock(&fuse_context_lock);
    fuse_context_ref--;
    if (!fuse_context_ref) {
        free(pthread_getspecific(fuse_context_key));
        pthread_key_delete(fuse_context_key);
    }
    pthread_mutex_unlock(&fuse_context_lock);
}

static struct fuse *req_fuse_prepare(fuse_req_t req)
{
    struct fuse_context_i *c = fuse_get_context_internal();
    const struct fuse_ctx *ctx = fuse_req_ctx(req);
    c->req = req;
    c->ctx.fuse = req_fuse(req);
    c->ctx.uid = ctx->uid;
    c->ctx.gid = ctx->gid;
    c->ctx.pid = ctx->pid;
#ifdef POSIXACLS
    c->ctx.umask = ctx->umask;
#endif
    return c->ctx.fuse;
}

static void reply_err(fuse_req_t req, int err)
{
    /* fuse_reply_err() uses non-negated errno values */
    fuse_reply_err(req, -err);
}

static void reply_entry(fuse_req_t req, const struct fuse_entry_param *e,
                        int err)
{
    if (!err) {
        struct fuse *f = req_fuse(req);
        if (fuse_reply_entry(req, e) == -ENOENT)
            forget_node(f, e->ino, 1);
    } else
        reply_err(req, err);
}

void fuse_fs_init(struct fuse_fs *fs, struct fuse_conn_info *conn)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.init)
        fs->user_data = fs->op.init(conn);
}

static void fuse_lib_init(void *data, struct fuse_conn_info *conn)
{
    struct fuse *f = (struct fuse *) data;
    struct fuse_context_i *c = fuse_get_context_internal();

    memset(c, 0, sizeof(*c));
    c->ctx.fuse = f;
    fuse_fs_init(f->fs, conn);
}

void fuse_fs_destroy(struct fuse_fs *fs)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.destroy)
        fs->op.destroy(fs->user_data);
    free(fs);
}

static void fuse_lib_destroy(void *data)
{
    struct fuse *f = (struct fuse *) data;
    struct fuse_context_i *c = fuse_get_context_internal();

    memset(c, 0, sizeof(*c));
    c->ctx.fuse = f;
    fuse_fs_destroy(f->fs);
    f->fs = NULL;
}

static void fuse_lib_lookup(fuse_req_t req, fuse_ino_t parent,
                            const char *name)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_entry_param e;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "LOOKUP %s\n", path);
        fuse_prepare_interrupt(f, req, &d); 
        err = lookup_path(f, parent, name, path, &e, NULL);
        if (err == -ENOENT && f->conf.negative_timeout != 0.0) {
            e.ino = 0;
            e.entry_timeout = f->conf.negative_timeout;
            err = 0;
        }
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_entry(req, &e, err);
}

static void fuse_lib_forget(fuse_req_t req, fuse_ino_t ino,
                            unsigned long nlookup)
{
    struct fuse *f = req_fuse(req);
    if (f->conf.debug)
        fprintf(stderr, "FORGET %llu/%lu\n", (unsigned long long)ino, nlookup);
    forget_node(f, ino, nlookup);
    fuse_reply_none(req);
}

static void fuse_lib_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct stat buf;
    char *path;
    int err;

    (void) fi;
    memset(&buf, 0, sizeof(buf));

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_getattr(f->fs, path, &buf);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    if (!err) {
        set_stat(f, ino, &buf);
        fuse_reply_attr(req, &buf, f->conf.attr_timeout);
    } else
        reply_err(req, err);
}

int fuse_fs_chmod(struct fuse_fs *fs, const char *path, mode_t mode)
{
    fuse_get_context()->private_data = fs->user_data;
    if (fs->op.chmod)
        return fs->op.chmod(path, mode);
    else
        return -ENOSYS;
}

static void fuse_lib_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int valid, struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct stat buf;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = 0;
        if (!err && (valid & FUSE_SET_ATTR_MODE))
            err = fuse_fs_chmod(f->fs, path, attr->st_mode);
        if (!err && (valid & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID))) {
            uid_t uid = 
                (valid & FUSE_SET_ATTR_UID) ? attr->st_uid : (uid_t) -1;
            gid_t gid = 
                (valid & FUSE_SET_ATTR_GID) ? attr->st_gid : (gid_t) -1;
            err = fuse_fs_chown(f->fs, path, uid, gid);
        }
        if (!err && (valid & FUSE_SET_ATTR_SIZE)) {
            if (fi)
                err = fuse_fs_ftruncate(f->fs, path, attr->st_size, fi);
            else
                err = fuse_fs_truncate(f->fs, path, attr->st_size);
        }
#ifdef HAVE_UTIMENSAT
        if (!err && f->utime_omit_ok &&
            (valid & (FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME))) {
            struct timespec tv[2];

            tv[0].tv_sec = 0;
            tv[1].tv_sec = 0;
            tv[0].tv_nsec = UTIME_OMIT;
            tv[1].tv_nsec = UTIME_OMIT;

            if (valid & FUSE_SET_ATTR_ATIME_NOW)
                tv[0].tv_nsec = UTIME_NOW;
            else if (valid & FUSE_SET_ATTR_ATIME)
                tv[0] = attr->st_atim;

            if (valid & FUSE_SET_ATTR_MTIME_NOW)
                tv[1].tv_nsec = UTIME_NOW;
            else if (valid & FUSE_SET_ATTR_MTIME)
                tv[1] = attr->st_mtim;

            err = fuse_fs_utimens(f->fs, path, tv);
        } else
#endif
        if (!err && (valid & (FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME)) ==
            (FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME)) {
            struct timespec tv[2];
            tv[0].tv_sec = attr->st_atime;
            tv[0].tv_nsec = ST_ATIM_NSEC(attr);
            tv[1].tv_sec = attr->st_mtime;
            tv[1].tv_nsec = ST_MTIM_NSEC(attr);
            err = fuse_fs_utimens(f->fs, path, tv);
        }
        if (!err)
            err = fuse_fs_getattr(f->fs,  path, &buf);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    if (!err) {
        set_stat(f, ino, &buf);
        fuse_reply_attr(req, &buf, f->conf.attr_timeout);
    } else
        reply_err(req, err);
}

static void fuse_lib_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "ACCESS %s 0%o\n", path, mask);
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_access(f->fs, path, mask);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static void fuse_lib_readlink(fuse_req_t req, fuse_ino_t ino)
{
    struct fuse *f = req_fuse_prepare(req);
    char linkname[PATH_MAX + 1];
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_readlink(f->fs, path, linkname, sizeof(linkname));
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    if (!err) {
        linkname[PATH_MAX] = '\0';
        fuse_reply_readlink(req, linkname);
    } else
        reply_err(req, err);
}

static void fuse_lib_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
                           mode_t mode, dev_t rdev)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_entry_param e;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "MKNOD %s\n", path);
        fuse_prepare_interrupt(f, req, &d);
        err = -ENOSYS;
        if (S_ISREG(mode)) {
            struct fuse_file_info fi;

            memset(&fi, 0, sizeof(fi));
            fi.flags = O_CREAT | O_EXCL | O_WRONLY;
            err = fuse_fs_create(f->fs, path, mode, &fi);
            if (!err) {
                err = lookup_path(f, parent, name, path, &e, &fi);
                fuse_fs_release(f->fs, path, &fi);
            }
        }
        if (err == -ENOSYS) {
            err = fuse_fs_mknod(f->fs, path, mode, rdev);
            if (!err)
                err = lookup_path(f, parent, name, path, &e, NULL);
        }
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_entry(req, &e, err);
}

static void fuse_lib_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
                           mode_t mode)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_entry_param e;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "MKDIR %s\n", path);
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_mkdir(f->fs, path, mode);
        if (!err)
            err = lookup_path(f, parent, name, path, &e, NULL);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_entry(req, &e, err);
}

static void fuse_lib_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_wrlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "UNLINK %s\n", path);
        fuse_prepare_interrupt(f, req, &d);
        if (!f->conf.hard_remove && is_open(f, parent, name))
            err = hide_node(f, path, parent, name);
        else {
            err = fuse_fs_unlink(f->fs, path);
            if (!err)
                remove_node(f, parent, name);
        }
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static void fuse_lib_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_wrlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "RMDIR %s\n", path);
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_rmdir(f->fs, path);
        fuse_finish_interrupt(f, req, &d);
        if (!err)
            remove_node(f, parent, name);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static void fuse_lib_symlink(fuse_req_t req, const char *linkname,
                             fuse_ino_t parent, const char *name)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_entry_param e;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "SYMLINK %s\n", path);
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_symlink(f->fs, linkname, path);
        if (!err)
            err = lookup_path(f, parent, name, path, &e, NULL);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_entry(req, &e, err);
}

static void fuse_lib_rename(fuse_req_t req, fuse_ino_t olddir,
                            const char *oldname, fuse_ino_t newdir,
                            const char *newname)
{
    struct fuse *f = req_fuse_prepare(req);
    char *oldpath;
    char *newpath;
    int err;

    err = -ENOENT;
    pthread_rwlock_wrlock(&f->tree_lock);
    oldpath = get_path_name(f, olddir, oldname);
    if (oldpath != NULL) {
        newpath = get_path_name(f, newdir, newname);
        if (newpath != NULL) {
            struct fuse_intr_data d;
            if (f->conf.debug)
                fprintf(stderr, "RENAME %s -> %s\n", oldpath, newpath);
            err = 0;
            fuse_prepare_interrupt(f, req, &d);
            if (!f->conf.hard_remove && is_open(f, newdir, newname))
                err = hide_node(f, newpath, newdir, newname);
            if (!err) {
                err = fuse_fs_rename(f->fs, oldpath, newpath);
                if (!err)
                    err = rename_node(f, olddir, oldname, newdir, newname, 0);
            }
            fuse_finish_interrupt(f, req, &d);
            free(newpath);
        }
        free(oldpath);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static void fuse_lib_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
                          const char *newname)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_entry_param e;
    char *oldpath;
    char *newpath;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    oldpath = get_path(f, ino);
    if (oldpath != NULL) {
        newpath =  get_path_name(f, newparent, newname);
        if (newpath != NULL) {
            struct fuse_intr_data d;
            if (f->conf.debug)
                fprintf(stderr, "LINK %s\n", newpath);
            fuse_prepare_interrupt(f, req, &d);
            err = fuse_fs_link(f->fs, oldpath, newpath);
            if (!err)
                err = lookup_path(f, newparent, newname, newpath, &e, NULL);
            fuse_finish_interrupt(f, req, &d);
            free(newpath);
        }
        free(oldpath);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_entry(req, &e, err);
}

static void fuse_do_release(struct fuse *f, fuse_ino_t ino, const char *path,
                            struct fuse_file_info *fi)
{
    struct node *node;
    int unlink_hidden = 0;

    fuse_fs_release(f->fs, path ? path : "-", fi);

    pthread_mutex_lock(&f->lock);
    node = get_node(f, ino);
    assert(node->open_count > 0);
    --node->open_count;
    if (node->is_hidden && !node->open_count) {
        unlink_hidden = 1;
        node->is_hidden = 0;
    }
    pthread_mutex_unlock(&f->lock);

    if(unlink_hidden && path)
        fuse_fs_unlink(f->fs, path);
}

static void fuse_lib_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    struct fuse_entry_param e;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path_name(f, parent, name);
    if (path) {
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_create(f->fs, path, mode, fi);
        if (!err) {
            err = lookup_path(f, parent, name, path, &e, fi);
            if (err)
                fuse_fs_release(f->fs, path, fi);
            else if (!S_ISREG(e.attr.st_mode)) {
                err = -EIO;
                fuse_fs_release(f->fs, path, fi);
                forget_node(f, e.ino, 1);
            } else {
                if (f->conf.direct_io)
                    fi->direct_io = 1;
                if (f->conf.kernel_cache)
                    fi->keep_cache = 1;

            }
        }
        fuse_finish_interrupt(f, req, &d);
    }
    if (!err) {
        pthread_mutex_lock(&f->lock);
        get_node(f, e.ino)->open_count++;
        pthread_mutex_unlock(&f->lock);
        if (fuse_reply_create(req, &e, fi) == -ENOENT) {
            /* The open syscall was interrupted, so it must be cancelled */
            fuse_prepare_interrupt(f, req, &d);
            fuse_do_release(f, e.ino, path, fi);
            fuse_finish_interrupt(f, req, &d);
            forget_node(f, e.ino, 1);
        } else if (f->conf.debug) {
            fprintf(stderr, "  CREATE[%llu] flags: 0x%x %s\n",
                    (unsigned long long) fi->fh, fi->flags, path);
        }
    } else
        reply_err(req, err);

    if (path)
        free(path);

    pthread_rwlock_unlock(&f->tree_lock);
}

static void fuse_lib_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    char *path = NULL;
    int err = 0;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path) {
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_open(f->fs, path, fi);
        if (!err) {
            if (f->conf.direct_io)
                fi->direct_io = 1;
            if (f->conf.kernel_cache)
                fi->keep_cache = 1;
        }
        fuse_finish_interrupt(f, req, &d);
    }
    if (!err) {
        pthread_mutex_lock(&f->lock);
        get_node(f, ino)->open_count++;
        pthread_mutex_unlock(&f->lock);
        if (fuse_reply_open(req, fi) == -ENOENT) {
            /* The open syscall was interrupted, so it must be cancelled */
            fuse_prepare_interrupt(f, req, &d);
            fuse_do_release(f, ino, path, fi);
            fuse_finish_interrupt(f, req, &d);
        } else if (f->conf.debug) {
            fprintf(stderr, "OPEN[%llu] flags: 0x%x %s\n",
                    (unsigned long long) fi->fh, fi->flags, path);
        }
    } else
        reply_err(req, err);

    if (path)
        free(path);
    pthread_rwlock_unlock(&f->tree_lock);
}

static void fuse_lib_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    char *buf;
    int res;

    buf = (char *) malloc(size);
    if (buf == NULL) {
        reply_err(req, -ENOMEM);
        return;
    }

    res = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "READ[%llu] %lu bytes from %llu\n",
                    (unsigned long long) fi->fh, (unsigned long) size,
                    (unsigned long long) off);

        fuse_prepare_interrupt(f, req, &d);
        res = fuse_fs_read(f->fs, path, buf, size, off, fi);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);

    if (res >= 0) {
        if (f->conf.debug)
            fprintf(stderr, "   READ[%llu] %u bytes\n",
                    (unsigned long long)fi->fh, res);
        if ((size_t) res > size)
            fprintf(stderr, "fuse: read too many bytes");
        fuse_reply_buf(req, buf, res);
    } else
        reply_err(req, res);

    free(buf);
}

static void fuse_lib_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                       size_t size, off_t off, struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int res;

    res = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "WRITE%s[%llu] %lu bytes to %llu\n",
                    fi->writepage ? "PAGE" : "", (unsigned long long) fi->fh,
                    (unsigned long) size, (unsigned long long) off);

        fuse_prepare_interrupt(f, req, &d);
        res = fuse_fs_write(f->fs, path, buf, size, off, fi);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);

    if (res >= 0) {
        if (f->conf.debug)
            fprintf(stderr, "   WRITE%s[%llu] %u bytes\n",
                    fi->writepage ? "PAGE" : "", (unsigned long long) fi->fh,
                    res);
        if ((size_t) res > size)
            fprintf(stderr, "fuse: wrote too many bytes");
        fuse_reply_write(req, res);
    } else
        reply_err(req, res);
}

static void fuse_lib_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
                       struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        if (f->conf.debug)
            fprintf(stderr, "FSYNC[%llu]\n", (unsigned long long) fi->fh);
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_fsync(f->fs, path, datasync, fi);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static struct fuse_dh *get_dirhandle(const struct fuse_file_info *llfi,
                                     struct fuse_file_info *fi)
{
    struct fuse_dh *dh = (struct fuse_dh *) (uintptr_t) llfi->fh;
    memset(fi, 0, sizeof(struct fuse_file_info));
    fi->fh = dh->fh;
    fi->fh_old = dh->fh;
    return dh;
}

static void fuse_lib_opendir(fuse_req_t req, fuse_ino_t ino,
                       struct fuse_file_info *llfi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    struct fuse_dh *dh;
    struct fuse_file_info fi;
    char *path;
    int err;

    dh = (struct fuse_dh *) malloc(sizeof(struct fuse_dh));
    if (dh == NULL) {
        reply_err(req, -ENOMEM);
        return;
    }
    memset(dh, 0, sizeof(struct fuse_dh));
    dh->fuse = f;
    dh->contents = NULL;
    dh->len = 0;
    dh->filled = 0;
    dh->nodeid = ino;
    fuse_mutex_init(&dh->lock);

    llfi->fh = (uintptr_t) dh;

    memset(&fi, 0, sizeof(fi));
    fi.flags = llfi->flags;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_opendir(f->fs, path, &fi);
        fuse_finish_interrupt(f, req, &d);
        dh->fh = fi.fh;
    }
    if (!err) {
        if (fuse_reply_open(req, llfi) == -ENOENT) {
            /* The opendir syscall was interrupted, so it must be cancelled */
            fuse_prepare_interrupt(f, req, &d);
            fuse_fs_releasedir(f->fs, path, &fi);
            fuse_finish_interrupt(f, req, &d);
            pthread_mutex_destroy(&dh->lock);
            free(dh);
        }
    } else {
        reply_err(req, err);
        pthread_mutex_destroy(&dh->lock);
        free(dh);
    }
    free(path);
    pthread_rwlock_unlock(&f->tree_lock);
}

static int extend_contents(struct fuse_dh *dh, unsigned minsize)
{
    if (minsize > dh->size) {
        char *newptr;
        unsigned newsize = dh->size;
        if (!newsize)
            newsize = 1024;
        while (newsize < minsize) {
	    if (newsize >= 0x80000000)
	       	newsize = 0xffffffff;
	    else
	       	newsize *= 2;
        }

        newptr = (char *) realloc(dh->contents, newsize);
        if (!newptr) {
            dh->error = -ENOMEM;
            return -1;
        }
        dh->contents = newptr;
        dh->size = newsize;
    }
    return 0;
}

static int fill_dir(void *dh_, const char *name, const struct stat *statp,
                    off_t off)
{
    struct fuse_dh *dh = (struct fuse_dh *) dh_;
    struct stat stbuf;
    size_t newlen;

    if (statp)
        stbuf = *statp;
    else {
        memset(&stbuf, 0, sizeof(stbuf));
        stbuf.st_ino = FUSE_UNKNOWN_INO;
    }

    if (!dh->fuse->conf.use_ino) {
        stbuf.st_ino = FUSE_UNKNOWN_INO;
        if (dh->fuse->conf.readdir_ino) {
            struct node *node;
            pthread_mutex_lock(&dh->fuse->lock);
            node = lookup_node(dh->fuse, dh->nodeid, name);
            if (node)
                stbuf.st_ino  = (ino_t) node->nodeid;
            pthread_mutex_unlock(&dh->fuse->lock);
        }
    }

    if (off) {
        if (extend_contents(dh, dh->needlen) == -1)
            return 1;

        dh->filled = 0;
        newlen = dh->len + fuse_add_direntry(dh->req, dh->contents + dh->len,
                                             dh->needlen - dh->len, name,
                                             &stbuf, off);
        if (newlen > dh->needlen)
            return 1;
    } else {
        newlen = dh->len + fuse_add_direntry(dh->req, NULL, 0, name, NULL, 0);
        if (extend_contents(dh, newlen) == -1)
            return 1;

        fuse_add_direntry(dh->req, dh->contents + dh->len, dh->size - dh->len,
                          name, &stbuf, newlen);
    }
    dh->len = newlen;
    return 0;
}

static int readdir_fill(struct fuse *f, fuse_req_t req, fuse_ino_t ino,
                        size_t size, off_t off, struct fuse_dh *dh,
                        struct fuse_file_info *fi)
{
    int err = -ENOENT;
    char *path;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;

        dh->len = 0;
        dh->error = 0;
        dh->needlen = size;
        dh->filled = 1;
        dh->req = req;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_readdir(f->fs, path, dh, fill_dir, off, fi);
        fuse_finish_interrupt(f, req, &d);
        dh->req = NULL;
        if (!err)
            err = dh->error;
        if (err)
            dh->filled = 0;
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    return err;
}

static void fuse_lib_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *llfi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_file_info fi;
    struct fuse_dh *dh = get_dirhandle(llfi, &fi);

    pthread_mutex_lock(&dh->lock);
    /* According to SUS, directory contents need to be refreshed on
       rewinddir() */
    if (!off)
        dh->filled = 0;

    if (!dh->filled) {
        int err = readdir_fill(f, req, ino, size, off, dh, &fi);
        if (err) {
            reply_err(req, err);
            goto out;
        }
    }
    if (dh->filled) {
        if (off < dh->len) {
            if (off + size > dh->len)
                size = dh->len - off;
        } else
            size = 0;
    } else {
        size = dh->len;
        off = 0;
    }
    fuse_reply_buf(req, dh->contents + off, size);
 out:
    pthread_mutex_unlock(&dh->lock);
}

static void fuse_lib_releasedir(fuse_req_t req, fuse_ino_t ino,
                            struct fuse_file_info *llfi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    struct fuse_file_info fi;
    struct fuse_dh *dh = get_dirhandle(llfi, &fi);
    char *path;

    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    fuse_prepare_interrupt(f, req, &d);
    fuse_fs_releasedir(f->fs, path ? path : "-", &fi);
    fuse_finish_interrupt(f, req, &d);
    if (path)
        free(path);
    pthread_rwlock_unlock(&f->tree_lock);
    pthread_mutex_lock(&dh->lock);
    pthread_mutex_unlock(&dh->lock);
    pthread_mutex_destroy(&dh->lock);
    free(dh->contents);
    free(dh);
    reply_err(req, 0);
}

static void fuse_lib_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
                          struct fuse_file_info *llfi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_file_info fi;
    char *path;
    int err;

    get_dirhandle(llfi, &fi);

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_fsyncdir(f->fs, path, datasync, &fi);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static void fuse_lib_statfs(fuse_req_t req, fuse_ino_t ino)
{
    struct fuse *f = req_fuse_prepare(req);
    struct statvfs buf;
    char *path;
    int err;

    memset(&buf, 0, sizeof(buf));
    pthread_rwlock_rdlock(&f->tree_lock);
    if (!ino) {
        err = -ENOMEM;
        path = strdup("/");
    } else {
        err = -ENOENT;
        path = get_path(f, ino);
    }
    if (path) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_statfs(f->fs, path, &buf);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);

    if (!err)
        fuse_reply_statfs(req, &buf);
    else
        reply_err(req, err);
}

static void fuse_lib_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                              const char *value, size_t size, int flags)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_setxattr(f->fs, path, name, value, size, flags);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static int common_getxattr(struct fuse *f, fuse_req_t req, fuse_ino_t ino,
                           const char *name, char *value, size_t size)
{
    int err;
    char *path;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_getxattr(f->fs, path, name, value, size);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    return err;
}

static void fuse_lib_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                              size_t size)
{
    struct fuse *f = req_fuse_prepare(req);
    int res;

    if (size) {
        char *value = (char *) malloc(size);
        if (value == NULL) {
            reply_err(req, -ENOMEM);
            return;
        }
        res = common_getxattr(f, req, ino, name, value, size);
        if (res > 0)
            fuse_reply_buf(req, value, res);
        else
            reply_err(req, res);
        free(value);
    } else {
        res = common_getxattr(f, req, ino, name, NULL, 0);
        if (res >= 0)
            fuse_reply_xattr(req, res);
        else
            reply_err(req, res);
    }
}

static int common_listxattr(struct fuse *f, fuse_req_t req, fuse_ino_t ino,
                            char *list, size_t size)
{
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_listxattr(f->fs, path, list, size);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    return err;
}

static void fuse_lib_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    struct fuse *f = req_fuse_prepare(req);
    int res;

    if (size) {
        char *list = (char *) malloc(size);
        if (list == NULL) {
            reply_err(req, -ENOMEM);
            return;
        }
        res = common_listxattr(f, req, ino, list, size);
        if (res > 0)
            fuse_reply_buf(req, list, res);
        else
            reply_err(req, res);
        free(list);
    } else {
        res = common_listxattr(f, req, ino, NULL, 0);
        if (res >= 0)
            fuse_reply_xattr(req, res);
        else
            reply_err(req, res);
    }
}

static void fuse_lib_removexattr(fuse_req_t req, fuse_ino_t ino,
                                 const char *name)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_removexattr(f->fs, path, name);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static struct lock *locks_conflict(struct node *node, const struct lock *lock)
{
    struct lock *l;

    for (l = node->locks; l; l = l->next)
        if (l->owner != lock->owner &&
            lock->start <= l->end && l->start <= lock->end &&
            (l->type == F_WRLCK || lock->type == F_WRLCK))
            break;

    return l;
}

static void delete_lock(struct lock **lockp)
{
    struct lock *l = *lockp;
    *lockp = l->next;
    free(l);
}

static void insert_lock(struct lock **pos, struct lock *lock)
{
    lock->next = *pos;
    *pos = lock;
}

static int locks_insert(struct node *node, struct lock *lock)
{
    struct lock **lp;
    struct lock *newl1 = NULL;
    struct lock *newl2 = NULL;

    if (lock->type != F_UNLCK || lock->start != 0 || lock->end != OFFSET_MAX) {
        newl1 = malloc(sizeof(struct lock));
        newl2 = malloc(sizeof(struct lock));

        if (!newl1 || !newl2) {
            free(newl1);
            free(newl2);
            return -ENOLCK;
        }
    }

    for (lp = &node->locks; *lp;) {
        struct lock *l = *lp;
        if (l->owner != lock->owner)
            goto skip;

        if (lock->type == l->type) {
            if (l->end < lock->start - 1)
                goto skip;
            if (lock->end < l->start - 1)
                break;
            if (l->start <= lock->start && lock->end <= l->end)
                goto out;
            if (l->start < lock->start)
                lock->start = l->start;
            if (lock->end < l->end)
                lock->end = l->end;
            goto delete;
        } else {
            if (l->end < lock->start)
                goto skip;
            if (lock->end < l->start)
                break;
            if (lock->start <= l->start && l->end <= lock->end)
                goto delete;
            if (l->end <= lock->end) {
                l->end = lock->start - 1;
                goto skip;
            }
            if (lock->start <= l->start) {
                l->start = lock->end + 1;
                break;
            }
            *newl2 = *l;
            newl2->start = lock->end + 1;
            l->end = lock->start - 1;
            insert_lock(&l->next, newl2);
            newl2 = NULL;
        }
    skip:
        lp = &l->next;
        continue;

    delete:
        delete_lock(lp);
    }
    if (lock->type != F_UNLCK) {
        *newl1 = *lock;
        insert_lock(lp, newl1);
        newl1 = NULL;
    }
out:
    free(newl1);
    free(newl2);
    return 0;
}

static void flock_to_lock(struct flock *flock, struct lock *lock)
{
    memset(lock, 0, sizeof(struct lock));
    lock->type = flock->l_type;
    lock->start = flock->l_start;
    lock->end = flock->l_len ? flock->l_start + flock->l_len - 1 : OFFSET_MAX;
    lock->pid = flock->l_pid;
}

static void lock_to_flock(struct lock *lock, struct flock *flock)
{
    flock->l_type = lock->type;
    flock->l_start = lock->start;
    flock->l_len = (lock->end == OFFSET_MAX) ? 0 : lock->end - lock->start + 1;
    flock->l_pid = lock->pid;
}

static int fuse_flush_common(struct fuse *f, fuse_req_t req, fuse_ino_t ino,
                             const char *path, struct fuse_file_info *fi)
{
    struct fuse_intr_data d;
    struct flock lock;
    struct lock l;
    int err;
    int errlock;

    fuse_prepare_interrupt(f, req, &d);
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    err = fuse_fs_flush(f->fs, path, fi);
    errlock = fuse_fs_lock(f->fs, path, fi, F_SETLK, &lock);
    fuse_finish_interrupt(f, req, &d);

    if (errlock != -ENOSYS) {
        flock_to_lock(&lock, &l);
        l.owner = fi->lock_owner;
        pthread_mutex_lock(&f->lock);
        locks_insert(get_node(f, ino), &l);
        pthread_mutex_unlock(&f->lock);

        /* if op.lock() is defined FLUSH is needed regardless of op.flush() */
        if (err == -ENOSYS)
            err = 0;
    }
    return err;
}

static void fuse_lib_release(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    char *path;
    int err = 0;

    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (f->conf.debug)
        fprintf(stderr, "RELEASE%s[%llu] flags: 0x%x\n",
                fi->flush ? "+FLUSH" : "",
                (unsigned long long) fi->fh, fi->flags);

    if (fi->flush) {
        err = fuse_flush_common(f, req, ino, path, fi);
        if (err == -ENOSYS)
            err = 0;
    }

    fuse_prepare_interrupt(f, req, &d);
    fuse_do_release(f, ino, path, fi);
    fuse_finish_interrupt(f, req, &d);
    free(path);
    pthread_rwlock_unlock(&f->tree_lock);

    reply_err(req, err);
}

static void fuse_lib_flush(fuse_req_t req, fuse_ino_t ino,
                       struct fuse_file_info *fi)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path && f->conf.debug)
        fprintf(stderr, "FLUSH[%llu]\n", (unsigned long long) fi->fh);
    err = fuse_flush_common(f, req, ino, path, fi);
    free(path);
    pthread_rwlock_unlock(&f->tree_lock);
    reply_err(req, err);
}

static int fuse_lock_common(fuse_req_t req, fuse_ino_t ino,
                            struct fuse_file_info *fi, struct flock *lock,
                            int cmd)
{
    struct fuse *f = req_fuse_prepare(req);
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        struct fuse_intr_data d;
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_lock(f->fs, path, fi, cmd, lock);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    return err;
}

static void fuse_lib_getlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock)
{
    int err;
    struct lock l;
    struct lock *conflict;
    struct fuse *f = req_fuse(req);

    flock_to_lock(lock, &l);
    l.owner = fi->lock_owner;
    pthread_mutex_lock(&f->lock);
    conflict = locks_conflict(get_node(f, ino), &l);
    if (conflict)
        lock_to_flock(conflict, lock);
    pthread_mutex_unlock(&f->lock);
    if (!conflict)
        err = fuse_lock_common(req, ino, fi, lock, F_GETLK);
    else
        err = 0;

    if (!err)
        fuse_reply_lock(req, lock);
    else
        reply_err(req, err);
}

static void fuse_lib_setlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock,
                           int should_sleep)
{
    int err = fuse_lock_common(req, ino, fi, lock, should_sleep ? F_SETLKW : F_SETLK);
    if (!err) {
        struct fuse *f = req_fuse(req);
        struct lock l;
        flock_to_lock(lock, &l);
        l.owner = fi->lock_owner;
        pthread_mutex_lock(&f->lock);
        locks_insert(get_node(f, ino), &l);
        pthread_mutex_unlock(&f->lock);
    }
    reply_err(req, err);
}

static void fuse_lib_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize,
                          uint64_t idx)
{
    struct fuse *f = req_fuse_prepare(req);
    struct fuse_intr_data d;
    char *path;
    int err;

    err = -ENOENT;
    pthread_rwlock_rdlock(&f->tree_lock);
    path = get_path(f, ino);
    if (path != NULL) {
        fuse_prepare_interrupt(f, req, &d);
        err = fuse_fs_bmap(f->fs, path, blocksize, &idx);
        fuse_finish_interrupt(f, req, &d);
        free(path);
    }
    pthread_rwlock_unlock(&f->tree_lock);
    if (!err)
        fuse_reply_bmap(req, idx);
    else
        reply_err(req, err);
}

static struct fuse_lowlevel_ops fuse_path_ops = {
    .init = fuse_lib_init,
    .destroy = fuse_lib_destroy,
    .lookup = fuse_lib_lookup,
    .forget = fuse_lib_forget,
    .getattr = fuse_lib_getattr,
    .setattr = fuse_lib_setattr,
    .access = fuse_lib_access,
    .readlink = fuse_lib_readlink,
    .mknod = fuse_lib_mknod,
    .mkdir = fuse_lib_mkdir,
    .unlink = fuse_lib_unlink,
    .rmdir = fuse_lib_rmdir,
    .symlink = fuse_lib_symlink,
    .rename = fuse_lib_rename,
    .link = fuse_lib_link,
    .create = fuse_lib_create,
    .open = fuse_lib_open,
    .read = fuse_lib_read,
    .write = fuse_lib_write,
    .flush = fuse_lib_flush,
    .release = fuse_lib_release,
    .fsync = fuse_lib_fsync,
    .opendir = fuse_lib_opendir,
    .readdir = fuse_lib_readdir,
    .releasedir = fuse_lib_releasedir,
    .fsyncdir = fuse_lib_fsyncdir,
    .statfs = fuse_lib_statfs,
    .setxattr = fuse_lib_setxattr,
    .getxattr = fuse_lib_getxattr,
    .listxattr = fuse_lib_listxattr,
    .removexattr = fuse_lib_removexattr,
    .getlk = fuse_lib_getlk,
    .setlk = fuse_lib_setlk,
    .bmap = fuse_lib_bmap,
};

struct fuse_session *fuse_get_session(struct fuse *f)
{
    return f->se;
}

int fuse_loop(struct fuse *f)
{
    if (f)
        return fuse_session_loop(f->se);
    else
        return -1;
}

void fuse_exit(struct fuse *f)
{
    fuse_session_exit(f->se);
}

struct fuse_context *fuse_get_context(void)
{
    return &fuse_get_context_internal()->ctx;
}

int fuse_interrupted(void)
{
    return fuse_req_interrupted(fuse_get_context_internal()->req);
}

enum {
    KEY_HELP,
};

#define FUSE_LIB_OPT(t, p, v) { t, offsetof(struct fuse_config, p), v }

static const struct fuse_opt fuse_lib_opts[] = {
    FUSE_OPT_KEY("-h",                    KEY_HELP),
    FUSE_OPT_KEY("--help",                KEY_HELP),
    FUSE_OPT_KEY("debug",                 FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d",                    FUSE_OPT_KEY_KEEP),
    FUSE_LIB_OPT("debug",                 debug, 1),
    FUSE_LIB_OPT("-d",                    debug, 1),
    FUSE_LIB_OPT("hard_remove",           hard_remove, 1),
    FUSE_LIB_OPT("use_ino",               use_ino, 1),
    FUSE_LIB_OPT("readdir_ino",           readdir_ino, 1),
    FUSE_LIB_OPT("direct_io",             direct_io, 1),
    FUSE_LIB_OPT("kernel_cache",          kernel_cache, 1),
    FUSE_LIB_OPT("umask=",                set_mode, 1),
    FUSE_LIB_OPT("umask=%o",              umask, 0),
    FUSE_LIB_OPT("uid=",                  set_uid, 1),
    FUSE_LIB_OPT("uid=%d",                uid, 0),
    FUSE_LIB_OPT("gid=",                  set_gid, 1),
    FUSE_LIB_OPT("gid=%d",                gid, 0),
    FUSE_LIB_OPT("entry_timeout=%lf",     entry_timeout, 0),
    FUSE_LIB_OPT("attr_timeout=%lf",      attr_timeout, 0),
    FUSE_LIB_OPT("ac_attr_timeout=%lf",   ac_attr_timeout, 0),
    FUSE_LIB_OPT("ac_attr_timeout=",      ac_attr_timeout_set, 1),
    FUSE_LIB_OPT("negative_timeout=%lf",  negative_timeout, 0),
    FUSE_LIB_OPT("intr",                  intr, 1),
    FUSE_LIB_OPT("intr_signal=%d",        intr_signal, 0),
    FUSE_OPT_END
};

static void fuse_lib_help(void)
{
    fprintf(stderr,
"    -o hard_remove         immediate removal (don't hide files)\n"
"    -o use_ino             let filesystem set inode numbers\n"
"    -o readdir_ino         try to fill in d_ino in readdir\n"
"    -o direct_io           use direct I/O\n"
"    -o kernel_cache        cache files in kernel\n"
"    -o umask=M             set file permissions (octal)\n"
"    -o uid=N               set file owner\n"
"    -o gid=N               set file group\n"
"    -o entry_timeout=T     cache timeout for names (1.0s)\n"
"    -o negative_timeout=T  cache timeout for deleted names (0.0s)\n"
"    -o attr_timeout=T      cache timeout for attributes (1.0s)\n"
"    -o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)\n"
"    -o intr                allow requests to be interrupted\n"
"    -o intr_signal=NUM     signal to send on interrupt (%i)\n"
"\n", FUSE_DEFAULT_INTR_SIGNAL);
}

static int fuse_lib_opt_proc(void *data, const char *arg, int key,
                             struct fuse_args *outargs)
{
    (void) arg; (void) outargs;

    if (key == KEY_HELP) {
        struct fuse_config *conf = (struct fuse_config *) data;
        fuse_lib_help();
        conf->help = 1;
    }

    return 1;
}

static int fuse_init_intr_signal(int signum, int *installed)
{
    struct sigaction old_sa;

    if (sigaction(signum, NULL, &old_sa) == -1) {
        perror("fuse: cannot get old signal handler");
        return -1;
    }

    if (old_sa.sa_handler == SIG_DFL) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = fuse_intr_sighandler;
        sigemptyset(&sa.sa_mask);

        if (sigaction(signum, &sa, NULL) == -1) {
            perror("fuse: cannot set interrupt signal handler");
            return -1;
        }
        *installed = 1;
    }
    return 0;
}

static void fuse_restore_intr_signal(int signum)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = SIG_DFL;
    sigaction(signum, &sa, NULL);
}

struct fuse_fs *fuse_fs_new(const struct fuse_operations *op, size_t op_size,
                            void *user_data)
{
    struct fuse_fs *fs;

    if (sizeof(struct fuse_operations) < op_size) {
        fprintf(stderr, "fuse: warning: library too old, some operations may not not work\n");
        op_size = sizeof(struct fuse_operations);
    }

    fs = (struct fuse_fs *) calloc(1, sizeof(struct fuse_fs));
    if (!fs) {
        fprintf(stderr, "fuse: failed to allocate fuse_fs object\n");
        return NULL;
    }

    fs->user_data = user_data;
    if (op)
        memcpy(&fs->op, op, op_size);
    return fs;
}

struct fuse *fuse_new(struct fuse_chan *ch, struct fuse_args *args,
		      const struct fuse_operations *op, size_t op_size,
		      void *user_data)
{
    struct fuse *f;
    struct node *root;
    struct fuse_fs *fs;
    struct fuse_lowlevel_ops llop = fuse_path_ops;

    if (fuse_create_context_key() == -1)
        goto out;

    f = (struct fuse *) calloc(1, sizeof(struct fuse));
    if (f == NULL) {
        fprintf(stderr, "fuse: failed to allocate fuse object\n");
        goto out_delete_context_key;
    }

    fs = fuse_fs_new(op, op_size, user_data);
    if (!fs)
        goto out_free;

    f->fs = fs;
    f->utime_omit_ok = fs->op.flag_utime_omit_ok;

    /* Oh f**k, this is ugly! */
    if (!fs->op.lock) {
        llop.getlk = NULL;
        llop.setlk = NULL;
    }

    f->conf.entry_timeout = 1.0;
    f->conf.attr_timeout = 1.0;
    f->conf.negative_timeout = 0.0;
    f->conf.intr_signal = FUSE_DEFAULT_INTR_SIGNAL;

    if (fuse_opt_parse(args, &f->conf, fuse_lib_opts, fuse_lib_opt_proc) == -1)
        goto out_free_fs;

    if (!f->conf.ac_attr_timeout_set)
        f->conf.ac_attr_timeout = f->conf.attr_timeout;

#ifdef __FreeBSD__
    /*
     * In FreeBSD, we always use these settings as inode numbers are needed to
     * make getcwd(3) work.
     */
    f->conf.readdir_ino = 1;
#endif

    f->se = fuse_lowlevel_new(args, &llop, sizeof(llop), f);
    if (f->se == NULL) {
        goto out_free_fs;
    }

    fuse_session_add_chan(f->se, ch);

    if (f->conf.debug)
        fprintf(stderr, "utime_omit_ok: %i\n", f->utime_omit_ok);
    f->ctr = 0;
    f->generation = 0;
    /* FIXME: Dynamic hash table */
    f->name_table_size = 14057;
    f->name_table = (struct node **)
        calloc(1, sizeof(struct node *) * f->name_table_size);
    if (f->name_table == NULL) {
        fprintf(stderr, "fuse: memory allocation failed\n");
        goto out_free_session;
    }

    f->id_table_size = 14057;
    f->id_table = (struct node **)
        calloc(1, sizeof(struct node *) * f->id_table_size);
    if (f->id_table == NULL) {
        fprintf(stderr, "fuse: memory allocation failed\n");
        goto out_free_name_table;
    }

    fuse_mutex_init(&f->lock);
    pthread_rwlock_init(&f->tree_lock, NULL);

    root = (struct node *) calloc(1, sizeof(struct node));
    if (root == NULL) {
        fprintf(stderr, "fuse: memory allocation failed\n");
        goto out_free_id_table;
    }

    root->name = strdup("/");
    if (root->name == NULL) {
        fprintf(stderr, "fuse: memory allocation failed\n");
        goto out_free_root;
    }

    if (f->conf.intr &&
        fuse_init_intr_signal(f->conf.intr_signal, &f->intr_installed) == -1)
        goto out_free_root_name;

    root->parent = NULL;
    root->nodeid = FUSE_ROOT_ID;
    root->generation = 0;
    root->refctr = 1;
    root->nlookup = 1;
    hash_id(f, root);

    return f;

 out_free_root_name:
    free(root->name);
 out_free_root:
    free(root);
 out_free_id_table:
    free(f->id_table);
 out_free_name_table:
    free(f->name_table);
 out_free_session:
    fuse_session_destroy(f->se);
 out_free_fs:
    /* Horrible compatibility hack to stop the destructor from being
       called on the filesystem without init being called first */
    fs->op.destroy = NULL;
    fuse_fs_destroy(f->fs);
 out_free:
    free(f);
 out_delete_context_key:
    fuse_delete_context_key();
 out:
    return NULL;
}

void fuse_destroy(struct fuse *f)
{
    size_t i;

    if (f->conf.intr && f->intr_installed)
        fuse_restore_intr_signal(f->conf.intr_signal);

    if (f->fs) {
        struct fuse_context_i *c = fuse_get_context_internal();

        memset(c, 0, sizeof(*c));
        c->ctx.fuse = f;

        for (i = 0; i < f->id_table_size; i++) {
            struct node *node;

            for (node = f->id_table[i]; node != NULL; node = node->id_next) {
                if (node->is_hidden) {
                    char *path = get_path(f, node->nodeid);
                    if (path) {
                        fuse_fs_unlink(f->fs, path);
                        free(path);
                    }
                }
            }
        }
    }
    for (i = 0; i < f->id_table_size; i++) {
        struct node *node;
        struct node *next;

        for (node = f->id_table[i]; node != NULL; node = next) {
            next = node->id_next;
            free_node(node);
        }
    }
    free(f->id_table);
    free(f->name_table);
    pthread_mutex_destroy(&f->lock);
    pthread_rwlock_destroy(&f->tree_lock);
    fuse_session_destroy(f->se);
    free(f);
    fuse_delete_context_key();
}

