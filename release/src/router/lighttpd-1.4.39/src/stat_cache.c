#include "log.h"
#include "stat_cache.h"
#include "fdevent.h"
#include "etag.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

#ifdef HAVE_ATTR_ATTRIBUTES_H
# include <attr/attributes.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
# include <sys/extattr.h>
#endif

#ifdef HAVE_LIBSMBCLIENT
#include <libsmbclient.h>
#endif

#ifdef HAVE_FAM_H
# include <fam.h>
#endif

#include "sys-mmap.h"

/* NetBSD 1.3.x needs it */
#ifndef MAP_FAILED
# define MAP_FAILED -1
#endif

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#ifndef HAVE_LSTAT
# define lstat stat
#endif

#define DBE 0

#if 0
/* enables debug code for testing if all nodes in the stat-cache as accessable */
#define DEBUG_STAT_CACHE
#endif

/*
 * stat-cache
 *
 * we cache the stat() calls in our own storage
 * the directories are cached in FAM
 *
 * if we get a change-event from FAM, we increment the version in the FAM->dir mapping
 *
 * if the stat()-cache is queried we check if the version id for the directory is the
 * same and return immediatly.
 *
 *
 * What we need:
 *
 * - for each stat-cache entry we need a fast indirect lookup on the directory name
 * - for each FAMRequest we have to find the version in the directory cache (index as userdata)
 *
 * stat <<-> directory <-> FAMRequest
 *
 * if file is deleted, directory is dirty, file is rechecked ...
 * if directory is deleted, directory mapping is removed
 *
 * */

#ifdef HAVE_FAM_H
typedef struct {
	FAMRequest *req;

	buffer *name;

	int version;
} fam_dir_entry;
#endif

/* the directory name is too long to always compare on it
 * - we need a hash
 * - the hash-key is used as sorting criteria for a tree
 * - a splay-tree is used as we can use the caching effect of it
 */

/* we want to cleanup the stat-cache every few seconds, let's say 10
 *
 * - remove entries which are outdated since 30s
 * - remove entries which are fresh but havn't been used since 60s
 * - if we don't have a stat-cache entry for a directory, release it from the monitor
 */

#ifdef DEBUG_STAT_CACHE
typedef struct {
	int *ptr;

	size_t used;
	size_t size;
} fake_keys;

static fake_keys ctrl;
#endif

stat_cache *stat_cache_init(void) {
	stat_cache *sc = NULL;

	sc = calloc(1, sizeof(*sc));

	sc->dir_name = buffer_init();
	sc->hash_key = buffer_init();

#ifdef HAVE_FAM_H
	sc->fam_fcce_ndx = -1;
#endif

#ifdef DEBUG_STAT_CACHE
	ctrl.size = 0;
#endif

	return sc;
}

static stat_cache_entry * stat_cache_entry_init(void) {
	stat_cache_entry *sce = NULL;

	sce = calloc(1, sizeof(*sce));

	sce->name = buffer_init();
	sce->etag = buffer_init();
	sce->content_type = buffer_init();

	return sce;
}

static void stat_cache_entry_free(void *data) {
	stat_cache_entry *sce = data;
	if (!sce) return;

	buffer_free(sce->etag);
	buffer_free(sce->name);
	buffer_free(sce->content_type);

	free(sce);
}

#ifdef HAVE_FAM_H
static fam_dir_entry * fam_dir_entry_init(void) {
	fam_dir_entry *fam_dir = NULL;

	fam_dir = calloc(1, sizeof(*fam_dir));

	fam_dir->name = buffer_init();

	return fam_dir;
}

static void fam_dir_entry_free(FAMConnection *fc, void *data) {
	fam_dir_entry *fam_dir = data;

	if (!fam_dir) return;

	FAMCancelMonitor(fc, fam_dir->req);

	buffer_free(fam_dir->name);
	free(fam_dir->req);

	free(fam_dir);
}
#endif

void stat_cache_free(stat_cache *sc) {
	while (sc->files) {
		int osize;
		splay_tree *node = sc->files;

		osize = sc->files->size;

		stat_cache_entry_free(node->data);
		sc->files = splaytree_delete(sc->files, node->key);

		force_assert(osize - 1 == splaytree_size(sc->files));
	}

	buffer_free(sc->dir_name);
	buffer_free(sc->hash_key);

#ifdef HAVE_FAM_H
	while (sc->dirs) {
		int osize;
		splay_tree *node = sc->dirs;

		osize = sc->dirs->size;

		fam_dir_entry_free(&sc->fam, node->data);
		sc->dirs = splaytree_delete(sc->dirs, node->key);

		if (osize == 1) {
			force_assert(NULL == sc->dirs);
		} else {
			force_assert(osize == (sc->dirs->size + 1));
		}
	}

	if (-1 != sc->fam_fcce_ndx) {
		/* fd events already gone */
		sc->fam_fcce_ndx = -1;

		FAMClose(&sc->fam);
	}
#endif
	free(sc);
}

#if defined(HAVE_XATTR)
static int stat_cache_attr_get(buffer *buf, char *name) {
	int attrlen;
	int ret;

	buffer_string_prepare_copy(buf, 1023);
	attrlen = buf->size - 1;
	if(0 == (ret = attr_get(name, "Content-Type", buf->ptr, &attrlen, 0))) {
		buffer_commit(buf, attrlen);
	}
	return ret;
}
#elif defined(HAVE_EXTATTR)
static int stat_cache_attr_get(buffer *buf, char *name) {
	ssize_t attrlen;

	buffer_string_prepare_copy(buf, 1023);

	if (-1 != (attrlen = extattr_get_file(name, EXTATTR_NAMESPACE_USER, "Content-Type", buf->ptr, buf->size - 1))) {
		buf->used = attrlen + 1;
		buf->ptr[attrlen] = '\0';
		return 0;
	}
	return -1;
}
#endif

/* the famous DJB hash function for strings */
static uint32_t hashme(buffer *str) {
	uint32_t hash = 5381;
	const char *s;
	for (s = str->ptr; *s; s++) {
		hash = ((hash << 5) + hash) + *s;
	}

	hash &= ~(((uint32_t)1) << 31); /* strip the highest bit */

	return hash;
}

#ifdef HAVE_FAM_H
handler_t stat_cache_handle_fdevent(server *srv, void *_fce, int revent) {
	size_t i;
	stat_cache *sc = srv->stat_cache;
	size_t events;

	UNUSED(_fce);
	/* */

	if (revent & FDEVENT_IN) {
		events = FAMPending(&sc->fam);

		for (i = 0; i < events; i++) {
			FAMEvent fe;
			fam_dir_entry *fam_dir;
			splay_tree *node;
			int ndx, j;

			FAMNextEvent(&sc->fam, &fe);

			/* handle event */

			switch(fe.code) {
			case FAMChanged:
			case FAMDeleted:
			case FAMMoved:
				/* if the filename is a directory remove the entry */

				fam_dir = fe.userdata;
				fam_dir->version++;

				/* file/dir is still here */
				if (fe.code == FAMChanged) break;

				/* we have 2 versions, follow and no-follow-symlink */

				for (j = 0; j < 2; j++) {
					buffer_copy_string(sc->hash_key, fe.filename);
					buffer_append_int(sc->hash_key, j);

					ndx = hashme(sc->hash_key);

					sc->dirs = splaytree_splay(sc->dirs, ndx);
					node = sc->dirs;

					if (node && (node->key == ndx)) {
						int osize = splaytree_size(sc->dirs);

						fam_dir_entry_free(&sc->fam, node->data);
						sc->dirs = splaytree_delete(sc->dirs, ndx);

						force_assert(osize - 1 == splaytree_size(sc->dirs));
					}
				}
				break;
			default:
				break;
			}
		}
	}

	if (revent & FDEVENT_HUP) {
		/* fam closed the connection */
		fdevent_event_del(srv->ev, &(sc->fam_fcce_ndx), FAMCONNECTION_GETFD(&sc->fam));
		fdevent_unregister(srv->ev, FAMCONNECTION_GETFD(&sc->fam));

		FAMClose(&sc->fam);
	}

	return HANDLER_GO_ON;
}

static int buffer_copy_dirname(buffer *dst, buffer *file) {
	size_t i;

	if (buffer_string_is_empty(file)) return -1;

	for (i = buffer_string_length(file); i > 0; i--) {
		if (file->ptr[i] == '/') {
			buffer_copy_string_len(dst, file->ptr, i);
			return 0;
		}
	}

	return -1;
}
#endif

#ifdef HAVE_LSTAT
static int stat_cache_lstat(server *srv, buffer *dname, struct stat *lst) {
	if (lstat(dname->ptr, lst) == 0) {
		return S_ISLNK(lst->st_mode) ? 0 : 1;
	}
	else {
		log_error_write(srv, __FILE__, __LINE__, "sbs",
				"lstat failed for:",
				dname, strerror(errno));
	};
	return -1;
}
#endif

/***
 *
 *
 *
 * returns:
 *  - HANDLER_FINISHED on cache-miss (don't forget to reopen the file)
 *  - HANDLER_ERROR on stat() failed -> see errno for problem
 */

handler_t stat_cache_get_entry(server *srv, connection *con, buffer *name, stat_cache_entry **ret_sce) {
#ifdef HAVE_FAM_H
	fam_dir_entry *fam_dir = NULL;
	int dir_ndx = -1;
#endif
	stat_cache_entry *sce = NULL;
	stat_cache *sc;
	struct stat st;
	size_t k;
	int fd;
	struct stat lst;
#ifdef DEBUG_STAT_CACHE
	size_t i;
#endif

	int file_ndx;

	*ret_sce = NULL;

	/*
	 * check if the directory for this file has changed
	 */

	sc = srv->stat_cache;

	buffer_copy_buffer(sc->hash_key, name);
	buffer_append_int(sc->hash_key, con->conf.follow_symlink);

	file_ndx = hashme(sc->hash_key);
	sc->files = splaytree_splay(sc->files, file_ndx);

#ifdef DEBUG_STAT_CACHE
	for (i = 0; i < ctrl.used; i++) {
		if (ctrl.ptr[i] == file_ndx) break;
	}
#endif

	if (sc->files && (sc->files->key == file_ndx)) {
#ifdef DEBUG_STAT_CACHE
		/* it was in the cache */
		force_assert(i < ctrl.used);
#endif

		/* we have seen this file already and
		 * don't stat() it again in the same second */

		sce = sc->files->data;

		/* check if the name is the same, we might have a collision */

		if (buffer_is_equal(name, sce->name)) {
			if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_SIMPLE) {
				if (sce->stat_ts == srv->cur_ts) {
					*ret_sce = sce;
					return HANDLER_GO_ON;
				}
			}
		} else {
			/* collision, forget about the entry */
			sce = NULL;
		}
	} else {
#ifdef DEBUG_STAT_CACHE
		if (i != ctrl.used) {
			log_error_write(srv, __FILE__, __LINE__, "xSB",
				file_ndx, "was already inserted but not found in cache, ", name);
		}
		force_assert(i == ctrl.used);
#endif
	}

#ifdef HAVE_FAM_H
	/* dir-check */
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM) {
		if (0 != buffer_copy_dirname(sc->dir_name, name)) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
				"no '/' found in filename:", name);
			return HANDLER_ERROR;
		}

		buffer_copy_buffer(sc->hash_key, sc->dir_name);
		buffer_append_int(sc->hash_key, con->conf.follow_symlink);

		dir_ndx = hashme(sc->hash_key);

		sc->dirs = splaytree_splay(sc->dirs, dir_ndx);

		if ((NULL != sc->dirs) && (sc->dirs->key == dir_ndx)) {
			fam_dir = sc->dirs->data;

			/* check whether we got a collision */
			if (buffer_is_equal(sc->dir_name, fam_dir->name)) {
				/* test whether a found file cache entry is still ok */
				if ((NULL != sce) && (fam_dir->version == sce->dir_version)) {
					/* the stat()-cache entry is still ok */

					*ret_sce = sce;
					return HANDLER_GO_ON;
				}
			} else {
				/* hash collision, forget about the entry */
				fam_dir = NULL;
			}
		}
	}
#endif

	/*
	 * *lol*
	 * - open() + fstat() on a named-pipe results in a (intended) hang.
	 * - stat() if regular file + open() to see if we can read from it is better
	 *
	 * */
	int r;

	//- Sungmin add	
#ifdef HAVE_LIBSMBCLIENT
	if(con->mode == DIRECT) 
		r =  stat(name->ptr, &st);
	else if(con->mode == SMB_NTLM||con->mode == SMB_BASIC)
		r = smbc_wrapper_stat(con, name->ptr, &st);
#else
	r =  stat(name->ptr, &st);
#endif

	if (r == -1) {
		return HANDLER_ERROR;
	}


	if (S_ISREG(st.st_mode)) {
		/* fix broken stat/open for symlinks to reg files with appended slash on freebsd,osx */
		if (name->ptr[buffer_string_length(name) - 1] == '/') {
			errno = ENOTDIR;
			return HANDLER_ERROR;
		}

		/* try to open the file to check if we can read it */
		//- Sungmin add
#ifdef HAVE_LIBSMBCLIENT		
		if(con->mode == DIRECT) {
			fd = open(name->ptr, O_RDONLY);
			if(fd == -1) {
				return HANDLER_ERROR;
			}
			close(fd);
		}
		else if(con->mode == SMB_NTLM) {
			smb_file_t *fp;
			fp = smbc_cli_ntcreate(con->smb_info->cli, name->ptr, 
				FILE_READ_DATA | FILE_WRITE_DATA, FILE_OPEN, 0);
			if(fp == NULL) {
				return HANDLER_ERROR;
			}
			smbc_cli_close(con->smb_info->cli, fp);
		}
		else {
			//Cdbg(DBE, "call smbc_open..%s", con->smb_info->url.path->ptr);			
			//Cdbg(DBE, "call smbc_open..%s", name->ptr);			

			//fd = smbc_open(con->smb_info->url.path->ptr, O_RDONLY, 0);
			fd = smbc_open(name->ptr, O_RDONLY, 0);
			
			if(fd == -1) {
				return HANDLER_ERROR;
			}
			
			smbc_close(fd);
		}
#else
		if (-1 == (fd = open(name->ptr, O_RDONLY))) {
			return HANDLER_ERROR;
		}
		close(fd);
#endif		
	}

	if (NULL == sce) {

		sce = stat_cache_entry_init();
		buffer_copy_buffer(sce->name, name);

		/* already splayed file_ndx */
		if ((NULL != sc->files) && (sc->files->key == file_ndx)) {
			/* hash collision: replace old entry */
			stat_cache_entry_free(sc->files->data);
			sc->files->data = sce;
		} else {
			int osize = splaytree_size(sc->files);

			sc->files = splaytree_insert(sc->files, file_ndx, sce);
			force_assert(osize + 1 == splaytree_size(sc->files));

#ifdef DEBUG_STAT_CACHE
			if (ctrl.size == 0) {
				ctrl.size = 16;
				ctrl.used = 0;
				ctrl.ptr = malloc(ctrl.size * sizeof(*ctrl.ptr));
			} else if (ctrl.size == ctrl.used) {
				ctrl.size += 16;
				ctrl.ptr = realloc(ctrl.ptr, ctrl.size * sizeof(*ctrl.ptr));
			}

			ctrl.ptr[ctrl.used++] = file_ndx;
#endif
		}
		force_assert(sc->files);
		force_assert(sc->files->data == sce);
	}

	sce->st = st;
	sce->stat_ts = srv->cur_ts;

	/* catch the obvious symlinks
	 *
	 * this is not a secure check as we still have a race-condition between
	 * the stat() and the open. We can only solve this by
	 * 1. open() the file
	 * 2. fstat() the fd
	 *
	 * and keeping the file open for the rest of the time. But this can
	 * only be done at network level.
	 *
	 * per default it is not a symlink
	 * */
#ifdef HAVE_LSTAT
	sce->is_symlink = 0;

	/* we want to only check for symlinks if we should block symlinks.
	 */
	if (!con->conf.follow_symlink) {
		if (stat_cache_lstat(srv, name, &lst)  == 0) {
#ifdef DEBUG_STAT_CACHE
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"found symlink", name);
#endif
				sce->is_symlink = 1;
		}

		/*
		 * we assume "/" can not be symlink, so
		 * skip the symlink stuff if our path is /
		 **/
		else if (buffer_string_length(name) > 1) {
			buffer *dname;
			char *s_cur;

			dname = buffer_init();
			buffer_copy_buffer(dname, name);

			while ((s_cur = strrchr(dname->ptr, '/'))) {
				buffer_string_set_length(dname, s_cur - dname->ptr);
				if (dname->ptr == s_cur) {
#ifdef DEBUG_STAT_CACHE
					log_error_write(srv, __FILE__, __LINE__, "s", "reached /");
#endif
					break;
				}
#ifdef DEBUG_STAT_CACHE
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"checking if", dname, "is a symlink");
#endif
				if (stat_cache_lstat(srv, dname, &lst)  == 0) {
					sce->is_symlink = 1;
#ifdef DEBUG_STAT_CACHE
					log_error_write(srv, __FILE__, __LINE__, "sb",
							"found symlink", dname);
#endif
					break;
				};
			};
			buffer_free(dname);
		};
	};
#endif

	if (S_ISREG(st.st_mode)) {
		/* determine mimetype */
		buffer_reset(sce->content_type);
#if defined(HAVE_XATTR) || defined(HAVE_EXTATTR)
		if (con->conf.use_xattr) {
			stat_cache_attr_get(sce->content_type, name->ptr);
		}
#endif
		/* xattr did not set a content-type. ask the config */
		if (buffer_string_is_empty(sce->content_type)) {
			size_t namelen = buffer_string_length(name);

			for (k = 0; k < con->conf.mimetypes->used; k++) {
				data_string *ds = (data_string *)con->conf.mimetypes->data[k];
				buffer *type = ds->key;
				size_t typelen = buffer_string_length(type);

				if (buffer_is_empty(type)) continue;

				/* check if the right side is the same */
				if (typelen > namelen) continue;

				if (0 == strncasecmp(name->ptr + namelen - typelen, type->ptr, typelen)) {
					buffer_copy_buffer(sce->content_type, ds->value);
					break;
				}
			}
		}
		etag_create(sce->etag, &(sce->st), con->etag_flags);
	} else if (S_ISDIR(st.st_mode)) {
		etag_create(sce->etag, &(sce->st), con->etag_flags);
	}

#ifdef HAVE_FAM_H
	if (srv->srvconf.stat_cache_engine == STAT_CACHE_ENGINE_FAM) {
		/* is this directory already registered ? */
		if (NULL == fam_dir) {
			fam_dir = fam_dir_entry_init();

			buffer_copy_buffer(fam_dir->name, sc->dir_name);

			fam_dir->version = 1;

			fam_dir->req = calloc(1, sizeof(FAMRequest));

			if (0 != FAMMonitorDirectory(&sc->fam, fam_dir->name->ptr,
						     fam_dir->req, fam_dir)) {

				log_error_write(srv, __FILE__, __LINE__, "sbsbs",
						"monitoring dir failed:",
						fam_dir->name, 
						"file:", name,
						FamErrlist[FAMErrno]);

				fam_dir_entry_free(&sc->fam, fam_dir);
				fam_dir = NULL;
			} else {
				int osize = splaytree_size(sc->dirs);

				/* already splayed dir_ndx */
				if ((NULL != sc->dirs) && (sc->dirs->key == dir_ndx)) {
					/* hash collision: replace old entry */
					fam_dir_entry_free(&sc->fam, sc->dirs->data);
					sc->dirs->data = fam_dir;
				} else {
					sc->dirs = splaytree_insert(sc->dirs, dir_ndx, fam_dir);
					force_assert(osize == (splaytree_size(sc->dirs) - 1));
				}

				force_assert(sc->dirs);
				force_assert(sc->dirs->data == fam_dir);
			}
		}

		/* bind the fam_fc to the stat() cache entry */

		if (fam_dir) {
			sce->dir_version = fam_dir->version;
		}
	}
#endif

	*ret_sce = sce;

	return HANDLER_GO_ON;
}

/**
 * remove stat() from cache which havn't been stat()ed for
 * more than 10 seconds
 *
 *
 * walk though the stat-cache, collect the ids which are too old
 * and remove them in a second loop
 */

static int stat_cache_tag_old_entries(server *srv, splay_tree *t, int *keys, size_t *ndx) {
	stat_cache_entry *sce;

	if (!t) return 0;

	stat_cache_tag_old_entries(srv, t->left, keys, ndx);
	stat_cache_tag_old_entries(srv, t->right, keys, ndx);

	sce = t->data;

	if (srv->cur_ts - sce->stat_ts > 2) {
		keys[(*ndx)++] = t->key;
	}

	return 0;
}

int stat_cache_trigger_cleanup(server *srv) {
	stat_cache *sc;
	size_t max_ndx = 0, i;
	int *keys;

	sc = srv->stat_cache;

	if (!sc->files) return 0;

	keys = calloc(1, sizeof(int) * sc->files->size);

	stat_cache_tag_old_entries(srv, sc->files, keys, &max_ndx);

	for (i = 0; i < max_ndx; i++) {
		int ndx = keys[i];
		splay_tree *node;

		sc->files = splaytree_splay(sc->files, ndx);

		node = sc->files;

		if (node && (node->key == ndx)) {
#ifdef DEBUG_STAT_CACHE
			size_t j;
			int osize = splaytree_size(sc->files);
			stat_cache_entry *sce = node->data;
#endif
			stat_cache_entry_free(node->data);
			sc->files = splaytree_delete(sc->files, ndx);

#ifdef DEBUG_STAT_CACHE
			for (j = 0; j < ctrl.used; j++) {
				if (ctrl.ptr[j] == ndx) {
					ctrl.ptr[j] = ctrl.ptr[--ctrl.used];
					break;
				}
			}

			force_assert(osize - 1 == splaytree_size(sc->files));
#endif
		}
	}

	free(keys);

	return 0;
}
