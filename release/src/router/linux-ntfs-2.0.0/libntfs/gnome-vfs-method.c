/*
 * gnome-vfs-method.c - Gnome-VFS init/shutdown implementation of interface to
 *			libntfs. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Jan Kratochvil <project-captive@jankratochvil.net>
 * Copyright (c) 2003-2006 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#undef FALSE
#undef TRUE
#include "types.h"		/* for 'FALSE'/'TRUE' libntfs definition */
#define FALSE FALSE
#define TRUE TRUE

#include "gnome-vfs-method.h"	/* self */
#include <libgnomevfs/gnome-vfs-method.h>
#include <glib/gmessages.h>
#include "gnome-vfs-module.h"
#include <glib/ghash.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <libgnomevfs/gnome-vfs-utils.h>

#include "volume.h"
#include "dir.h"

static GnomeVFSMethod GnomeVFSMethod_static;
G_LOCK_DEFINE_STATIC(GnomeVFSMethod_static);

/* map: (gchar *)method_name -> (struct method_name_info *) */
static GHashTable *method_name_hash;
G_LOCK_DEFINE_STATIC(method_name_hash);

struct method_name_info {
	gchar *args;
};

static void method_name_hash_key_destroy_func(gchar *key)
{
	g_return_if_fail(key != NULL);

	g_free(key);
}

static void method_name_hash_value_destroy_func(struct method_name_info *value)
{
	g_return_if_fail(value != NULL);

	g_free(value->args);
	g_free(value);
}

static void method_name_hash_init(void)
{
	G_LOCK(method_name_hash);
	if (!method_name_hash) {
		method_name_hash = g_hash_table_new_full(
				g_str_hash,	/* hash_func */
				g_str_equal,	/* key_equal_func */
				(GDestroyNotify) method_name_hash_key_destroy_func,	/* key_destroy_func */
				(GDestroyNotify) method_name_hash_value_destroy_func);	/* value_destroy_func */
	}
	G_UNLOCK(method_name_hash);
}

/*
 * map: (gchar *)uri_parent_string "method_name:uri_parent" -> (ntfs_volume *)
 */
static GHashTable *uri_parent_string_hash;
G_LOCK_DEFINE_STATIC(uri_parent_string_hash);

static void uri_parent_string_hash_key_destroy_func(gchar *key)
{
	g_return_if_fail(key != NULL);

	g_free(key);
}

static void uri_parent_string_hash_value_destroy_func(ntfs_volume *value)
{
	g_return_if_fail(value != NULL);

	ntfs_umount(	/* errors ignored */
			value,	/* vol */
			TRUE);	/* force; possibly loose modifications */
}

static void uri_parent_string_hash_init(void)
{
	G_LOCK(uri_parent_string_hash);
	if (!uri_parent_string_hash) {
		uri_parent_string_hash = g_hash_table_new_full(
				g_str_hash,	/* hash_func */
				g_str_equal,	/* key_equal_func */
				(GDestroyNotify) uri_parent_string_hash_key_destroy_func,	/* key_destroy_func */
				(GDestroyNotify) uri_parent_string_hash_value_destroy_func);	/* value_destroy_func */
	}
	G_UNLOCK(uri_parent_string_hash);
}

static GnomeVFSResult libntfs_gnomevfs_uri_parent_init(
		ntfs_volume **volume_return, GnomeVFSURI *uri)
{
	gchar *uri_parent_string;
	gchar *uri_parent_string_parent;
	ntfs_volume *volume;

	g_return_val_if_fail(uri != NULL, GNOME_VFS_ERROR_INVALID_URI);
	g_return_val_if_fail(volume_return != NULL,
			     GNOME_VFS_ERROR_BAD_PARAMETERS);

	uri_parent_string_hash_init();

	if (!uri->parent)
		return GNOME_VFS_ERROR_INVALID_URI;
	if (!uri->text)		/* not needed here but we don't permit non-specific fs-image reference */
		return GNOME_VFS_ERROR_INVALID_URI;
	uri_parent_string_parent = gnome_vfs_uri_to_string(uri->parent,
			GNOME_VFS_URI_HIDE_NONE);
	g_assert(uri_parent_string_parent != NULL);

	uri_parent_string = g_strdup_printf("%s:%s", uri->method_string,
			uri_parent_string_parent);
	g_assert(uri_parent_string != NULL);

	G_LOCK(uri_parent_string_hash);
	volume = g_hash_table_lookup(uri_parent_string_hash, uri_parent_string);
	G_UNLOCK(uri_parent_string_hash);
	if (!volume) {
		struct method_name_info *method_name_info;

		G_LOCK(method_name_hash);
		method_name_info = g_hash_table_lookup(method_name_hash,
				uri->method_string);
		G_UNLOCK(method_name_hash);
		if (!method_name_info) {
			/* should not happend */
			g_return_val_if_reached(GNOME_VFS_ERROR_INVALID_URI);
		}

		/* TODO: Generic GnomeVFS filter. */
		if (strcmp(uri->parent->method_string, "file")) {
			g_free(uri_parent_string);
			return GNOME_VFS_ERROR_INVALID_URI;
		}

		if (!(volume = ntfs_mount(uri->parent->text,
				NTFS_MNT_RDONLY))) {
			g_free(uri_parent_string);
			return GNOME_VFS_ERROR_WRONG_FORMAT;
		}

		G_LOCK(uri_parent_string_hash);
		g_hash_table_insert(uri_parent_string_hash,
				g_strdup(uri_parent_string), volume);
		G_UNLOCK(uri_parent_string_hash);
	}
	g_free(uri_parent_string);

	*volume_return = volume;
	return GNOME_VFS_OK;
}

static GnomeVFSResult inode_open_by_pathname(ntfs_inode **inode_return,
		ntfs_volume *volume, const gchar *pathname)
{
	MFT_REF mref;
	ntfs_inode *inode;
	gchar *pathname_parse, *pathname_next;
	int errint;

	g_return_val_if_fail(inode_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(volume != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(pathname != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

	pathname = g_path_skip_root(pathname);
	pathname_parse = g_alloca(strlen(pathname) + 1);
	strcpy(pathname_parse, pathname);
	mref = FILE_root;
	for (;;) {
		ntfschar *pathname_parse_ucs2;
		gchar *pathname_parse_unescaped;
		int i;

		G_LOCK(libntfs);
		inode = ntfs_inode_open(volume, mref);
		G_UNLOCK(libntfs);
		if (!inode)
			return GNOME_VFS_ERROR_NOT_FOUND;
		if (!*pathname_parse) {
			*inode_return = inode;
			return GNOME_VFS_OK;
		}
		for (pathname_next = pathname_parse; *pathname_next &&
		     *pathname_next != G_DIR_SEPARATOR; pathname_next++) ;
		if (*pathname_next) {
			/* terminate current path element */
			*pathname_next++ = 0;
		}
		while (*pathname_next == G_DIR_SEPARATOR)
			pathname_next++;
		/* FIXME: Is 'pathname' utf8? */
		pathname_parse_unescaped = gnome_vfs_unescape_string(
				pathname_parse, NULL);	/* illegal_characters */
		libntfs_newn(pathname_parse_ucs2,
				strlen(pathname_parse_unescaped) + 1);
		for (i = 0; pathname_parse_unescaped[i]; i++)
			pathname_parse_ucs2[i] = cpu_to_le16(
					pathname_parse_unescaped[i]);
		pathname_parse_ucs2[i] = 0;
		g_free(pathname_parse_unescaped);
		G_LOCK(libntfs);
		mref = ntfs_inode_lookup_by_name(inode, pathname_parse_ucs2, i);
		G_UNLOCK(libntfs);
		g_free(pathname_parse_ucs2);
		if ((MFT_REF)-1 == mref)
			return GNOME_VFS_ERROR_NOT_FOUND;
		G_LOCK(libntfs);
		errint = ntfs_inode_close(inode);
		G_UNLOCK(libntfs);
		if (errint)
			g_return_val_if_reached(GNOME_VFS_ERROR_INTERNAL);
		pathname_parse = pathname_next;
	}
	/* NOTREACHED */
}

struct libntfs_directory {
	ntfs_inode *inode;
	GList *file_info_list;	/* of (GnomeVFSFileInfo *); last item has ->data == NULL */
};

static GnomeVFSResult libntfs_gnomevfs_open_directory(GnomeVFSMethod *method,
		GnomeVFSMethodHandle **method_handle, GnomeVFSURI *uri,
		GnomeVFSFileInfoOptions options __attribute__((unused)),
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	ntfs_volume *volume;
	ntfs_inode *inode;
	struct libntfs_directory *libntfs_directory;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(method_handle != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_uri_parent_init(&volume, uri)))
		return errvfsresult;

	if (GNOME_VFS_OK != (errvfsresult = inode_open_by_pathname(&inode,
			volume, uri->text)))
		return errvfsresult;

	libntfs_new(libntfs_directory);
	libntfs_directory->inode = inode;
	libntfs_directory->file_info_list = NULL;

	*method_handle = (GnomeVFSMethodHandle *)libntfs_directory;
	return errvfsresult;
}

static GnomeVFSResult libntfs_gnomevfs_close_directory(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle,
		GnomeVFSContext *context __attribute__((unused)))
{
	struct libntfs_directory *libntfs_directory;
	int errint;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_directory = (struct libntfs_directory *)method_handle;
	g_return_val_if_fail(libntfs_directory != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	G_LOCK(libntfs);
	errint = ntfs_inode_close(libntfs_directory->inode);
	G_UNLOCK(libntfs);
	if (errint)
		g_return_val_if_reached(GNOME_VFS_ERROR_INTERNAL);

	if (libntfs_directory->file_info_list) {
		GList *last_l;

		/*
		 * Prevent gnome_vfs_file_info_list_free() and its
		 * gnome_vfs_file_info_unref() on the last 'file_info_list'
		 * items as it is EOF with NULL '->data'.
		 */
		last_l = g_list_last(libntfs_directory->file_info_list);
		g_assert(last_l->data == NULL);
		libntfs_directory->file_info_list = g_list_delete_link(
				libntfs_directory->file_info_list, last_l);
		gnome_vfs_file_info_list_free(
				libntfs_directory->file_info_list);
	}

	g_free(libntfs_directory);

	return GNOME_VFS_OK;
}

static gchar *libntfs_ntfscharo_utf8(const ntfschar *name, const int name_len)
{
	GString *gstring;
	int i;

	gstring = g_string_sized_new(name_len);
	for (i = 0; i < name_len; i++)
		gstring = g_string_append_unichar(gstring,
				le16_to_cpu(name[i]));
	return g_string_free(gstring,	/* returns utf8-formatted string */
			FALSE);	/* free_segment */
}

/*
 * Do not lock 'libntfs' here as we are already locked inside ntfs_readdir().
 */
static int libntfs_gnomevfs_read_directory_filldir(
		struct libntfs_directory *libntfs_directory /* dirent */,
		const ntfschar *name, const int name_len,
		const int name_type __attribute__((unused)),
		const s64 pos, const MFT_REF mref, const unsigned dt_type)
{
	GnomeVFSFileInfo *file_info;

	g_return_val_if_fail(libntfs_directory != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);
	g_return_val_if_fail(name_len >= 0, -1);
	g_return_val_if_fail(pos >= 0, -1);

	/* system directory */
	if (MREF(mref) != FILE_root && MREF(mref) < FILE_first_user)
		return 0;	/* continue traversal */

	file_info = gnome_vfs_file_info_new();
	file_info->name = libntfs_ntfscharo_utf8(name, name_len);
	file_info->valid_fields = 0;

	switch (dt_type) {
	case NTFS_DT_FIFO:
		file_info->type = GNOME_VFS_FILE_TYPE_FIFO;
		break;
	case NTFS_DT_CHR:
		file_info->type = GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE;
		break;
	case NTFS_DT_DIR:
		file_info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
		break;
	case NTFS_DT_BLK:
		file_info->type = GNOME_VFS_FILE_TYPE_BLOCK_DEVICE;
		break;
	case NTFS_DT_REG:
		file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
		break;
	case NTFS_DT_LNK:
		file_info->type = GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK;
		break;
	case NTFS_DT_SOCK:
		file_info->type = GNOME_VFS_FILE_TYPE_SOCKET;
		break;
		/* FIXME: What is 'NTFS_DT_WHT'? */
	default:
		file_info->type = GNOME_VFS_FILE_TYPE_UNKNOWN;
	}
	if (file_info->type != GNOME_VFS_FILE_TYPE_UNKNOWN)
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;

	/* Detect 'file_info->size': */
	if (file_info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
		ntfs_inode *inode;

		inode = ntfs_inode_open(libntfs_directory->inode->vol, mref);
		/* FIXME: Check failed 'inode' open. */
		if (inode) {
			ntfs_attr *attr;
			int errint;

			attr = ntfs_attr_open(inode,	/* ni */
					AT_DATA,	/* type */
					AT_UNNAMED,	/* name */
					0);		/* name_len */
			/* FIXME: Check failed 'attr' open. */
			if (attr) {
				/* FIXME: Is 'data_size' the right field? */
				file_info->size = attr->data_size;
				file_info->valid_fields |=
						GNOME_VFS_FILE_INFO_FIELDS_SIZE;
				ntfs_attr_close(attr);
			}
			errint = ntfs_inode_close(inode);
			/* FIXME: Check 'errint'. */
		}
	}

	libntfs_directory->file_info_list = g_list_prepend(
			libntfs_directory->file_info_list, file_info);

	return 0;	/* continue traversal */
}

static GnomeVFSResult libntfs_gnomevfs_read_directory(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle,
		GnomeVFSFileInfo *file_info,
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	struct libntfs_directory *libntfs_directory;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_directory = (struct libntfs_directory *)method_handle;
	g_return_val_if_fail(libntfs_directory != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(file_info != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (!libntfs_directory->file_info_list) {
		int errint;
		s64 pos;

		pos = 0; /* read from the start; incl. "." and ".." entries */
		G_LOCK(libntfs);
		errint = ntfs_readdir(libntfs_directory->inode,	/* dir_ni */
				&pos,	/* pos */
				libntfs_directory,	/* dirent */
				(ntfs_filldir_t)libntfs_gnomevfs_read_directory_filldir);	/* filldir */
		G_UNLOCK(libntfs);
		if (errint)
			return GNOME_VFS_ERROR_INTERNAL;

		libntfs_directory->file_info_list = g_list_prepend(
				libntfs_directory->file_info_list, NULL); /* EOF */
		libntfs_directory->file_info_list = g_list_reverse(
				libntfs_directory->file_info_list);
	}

	if (!libntfs_directory->file_info_list->data) {
		g_assert(libntfs_directory->file_info_list->next == NULL);
		/*
		 * Do not clear the list to leave us stuck at EOF - GnomeVFS
		 * behaves that way.
		 */
		errvfsresult = GNOME_VFS_ERROR_EOF;
	} else {
		/* Cut first list item. */
		gnome_vfs_file_info_copy(file_info, /* dest */
				libntfs_directory->file_info_list->data); /* src */
		gnome_vfs_file_info_unref(
				libntfs_directory->file_info_list->data);
		libntfs_directory->file_info_list = g_list_delete_link(
				libntfs_directory->file_info_list,
				libntfs_directory->file_info_list);
		errvfsresult = GNOME_VFS_OK;
	}
	return errvfsresult;
}

struct libntfs_file {
	ntfs_inode *inode;
	ntfs_attr *attr;
	s64 pos;
};

static GnomeVFSResult libntfs_open_attr(struct libntfs_file *libntfs_file)
{
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(libntfs_file->inode != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (!libntfs_file->attr) {
		G_LOCK(libntfs);
		libntfs_file->attr = ntfs_attr_open(
				libntfs_file->inode,	/* ni */
				AT_DATA,	/* type */
				AT_UNNAMED,	/* name */
				0);	/* name_len */
		G_UNLOCK(libntfs);
		if (!libntfs_file->attr)
			return GNOME_VFS_ERROR_BAD_FILE;
		libntfs_file->pos = 0;
	}

	return GNOME_VFS_OK;
}

static GnomeVFSResult libntfs_gnomevfs_open(GnomeVFSMethod *method,
		GnomeVFSMethodHandle **method_handle_return, GnomeVFSURI *uri,
		GnomeVFSOpenMode mode,
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	ntfs_volume *volume;
	ntfs_inode *inode;
	struct libntfs_file *libntfs_file;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(method_handle_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_uri_parent_init(&volume, uri)))
		return errvfsresult;

	if (mode & GNOME_VFS_OPEN_WRITE)
		return GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM;

	if (GNOME_VFS_OK != (errvfsresult =
			inode_open_by_pathname(&inode, volume, uri->text)))
		return errvfsresult;

	libntfs_new(libntfs_file);
	libntfs_file->inode = inode;
	libntfs_file->attr = NULL;

	*method_handle_return = (GnomeVFSMethodHandle *)libntfs_file;
	return errvfsresult;
}

static GnomeVFSResult libntfs_gnomevfs_create(GnomeVFSMethod *method,
		GnomeVFSMethodHandle **method_handle_return, GnomeVFSURI *uri,
		GnomeVFSOpenMode mode __attribute__((unused)),
		gboolean exclusive __attribute__((unused)),
		guint perm __attribute__((unused)),
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	ntfs_volume *volume;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(method_handle_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_uri_parent_init(&volume, uri)))
		return errvfsresult;

	return GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM;
}

static GnomeVFSResult libntfs_gnomevfs_close(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle,
		GnomeVFSContext *context __attribute__((unused)))
{
	struct libntfs_file *libntfs_file;
	int errint;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_file = (struct libntfs_file *) method_handle;
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (libntfs_file->attr) {
		G_LOCK(libntfs);
		ntfs_attr_close(libntfs_file->attr);
		G_UNLOCK(libntfs);
	}
	G_LOCK(libntfs);
	errint = ntfs_inode_close(libntfs_file->inode);
	G_UNLOCK(libntfs);
	if (errint)
		g_return_val_if_reached(GNOME_VFS_ERROR_INTERNAL);

	g_free(libntfs_file);

	return GNOME_VFS_OK;
}

static GnomeVFSResult libntfs_gnomevfs_read(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle, gpointer buffer,
		GnomeVFSFileSize num_bytes, GnomeVFSFileSize *bytes_read_return,
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	struct libntfs_file *libntfs_file;
	s64 count_s64, got;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_file = (struct libntfs_file *)method_handle;
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(buffer != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(bytes_read_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult = libntfs_open_attr(libntfs_file)))
		return errvfsresult;

	count_s64 = num_bytes;
	g_assert((GnomeVFSFileSize)count_s64 == num_bytes);
	G_LOCK(libntfs);
	got = ntfs_attr_pread(libntfs_file->attr, libntfs_file->pos, count_s64,
			buffer);
	G_UNLOCK(libntfs);
	if (got == -1)
		return GNOME_VFS_ERROR_IO;

	libntfs_file->pos += got;
	*bytes_read_return = got;
	g_assert((s64)*bytes_read_return == got);

	return GNOME_VFS_OK;
}

static GnomeVFSResult libntfs_gnomevfs_seek(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle,
		GnomeVFSSeekPosition whence, GnomeVFSFileOffset offset,
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	struct libntfs_file *libntfs_file;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_file = (struct libntfs_file *)method_handle;
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult = libntfs_open_attr(libntfs_file)))
		return errvfsresult;

	switch (whence) {
	case GNOME_VFS_SEEK_START:
		libntfs_file->pos = offset;
		break;
	case GNOME_VFS_SEEK_CURRENT:
		libntfs_file->pos += offset;
		break;
	case GNOME_VFS_SEEK_END:
		/* FIXME: NOT IMPLEMENTED YET */
		g_return_val_if_reached(GNOME_VFS_ERROR_BAD_PARAMETERS);
	default:
		g_assert_not_reached();
	}

	return GNOME_VFS_OK;
}

static GnomeVFSResult libntfs_gnomevfs_tell(GnomeVFSMethod *method,
		GnomeVFSMethodHandle *method_handle,
		GnomeVFSFileSize *offset_return)
{
	GnomeVFSResult errvfsresult;
	struct libntfs_file *libntfs_file;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_file = (struct libntfs_file *)method_handle;
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(offset_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	if (GNOME_VFS_OK != (errvfsresult = libntfs_open_attr(libntfs_file)))
		return errvfsresult;

	*offset_return = libntfs_file->pos;
	g_assert((s64)*offset_return == libntfs_file->pos);

	return errvfsresult;
}

static gboolean libntfs_gnomevfs_is_local(GnomeVFSMethod *method,
		const GnomeVFSURI *uri)
{
	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(uri != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

	return gnome_vfs_uri_is_local(uri->parent);
}

static GnomeVFSResult libntfs_gnomevfs_get_file_info_from_handle(
		GnomeVFSMethod *method, GnomeVFSMethodHandle *method_handle,
		GnomeVFSFileInfo *file_info,
		GnomeVFSFileInfoOptions options __attribute__((unused)),
		GnomeVFSContext *context __attribute__((unused)))
{
	GnomeVFSResult errvfsresult;
	struct libntfs_file *libntfs_file;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	libntfs_file = (struct libntfs_file *)method_handle;
	g_return_val_if_fail(libntfs_file != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(file_info != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	/* handle 'options & GNOME_VFS_FILE_INFO_GET_MIME_TYPE'? */

	file_info->valid_fields = 0;
	/* FIXME: It is complicated to read filename of open 'ntfs_inode'. */
	file_info->name = NULL;

	if (GNOME_VFS_OK != (errvfsresult = libntfs_open_attr(libntfs_file))) {
		/* Assume we are directory: */
		file_info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
		/*
		 * Do not: file_info->valid_fields |=
		 * GNOME_VFS_FILE_INFO_FIELDS_TYPE;
		 * as gnome-vfs-xfer.c/copy_items() does not check
		 * 'GNOME_VFS_FILE_INFO_FIELDS_TYPE' and we are just bluffing
		 * we know it.
		 */
		return GNOME_VFS_OK;
	}

	/* FIXME: Is 'data_size' the right field? */
	file_info->size = libntfs_file->attr->data_size;
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_SIZE;

	/*
	 * FIXME: We do not really know the type of 'libntfs_file' but
	 * gnome-vfs-xfer.c/copy_items() requires 'GNOME_VFS_FILE_TYPE_REGULAR'
	 * to copy it.
	 */
	file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
	/*
	 * Do not: file_info->valid_fields|=GNOME_VFS_FILE_INFO_FIELDS_TYPE;
	 * as gnome-vfs-xfer.c/copy_items() does not check
	 * 'GNOME_VFS_FILE_INFO_FIELDS_TYPE' and we are just bluffing we know
	 * it.
	 */

	return errvfsresult;
}

static GnomeVFSResult libntfs_gnomevfs_get_file_info(GnomeVFSMethod *method,
		GnomeVFSURI *uri, GnomeVFSFileInfo *file_info,
		GnomeVFSFileInfoOptions options, GnomeVFSContext *context)
{
	GnomeVFSResult errvfsresult;
	GnomeVFSMethodHandle *method_handle;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(file_info != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	/* handle 'options & GNOME_VFS_FILE_INFO_GET_MIME_TYPE'? */

	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_open(method, &method_handle, uri,
			GNOME_VFS_OPEN_READ, context)))
		return errvfsresult;
	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_get_file_info_from_handle(method,
			method_handle, file_info, options, context)))
		return errvfsresult;
	if (GNOME_VFS_OK != (errvfsresult =
			libntfs_gnomevfs_close(method, method_handle, context)))
		return errvfsresult;

	return GNOME_VFS_OK;
}

static GnomeVFSResult libntfs_gnomevfs_check_same_fs(GnomeVFSMethod *method,
		GnomeVFSURI *a, GnomeVFSURI *b, gboolean *same_fs_return,
		GnomeVFSContext *context __attribute__((unused)))
{
	ntfs_volume *volume_a;
	ntfs_volume *volume_b;
	GnomeVFSResult errvfsresult;

	g_return_val_if_fail(method == &GnomeVFSMethod_static,
			GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail(same_fs_return != NULL,
			GNOME_VFS_ERROR_BAD_PARAMETERS);

	errvfsresult = libntfs_gnomevfs_uri_parent_init(&volume_a, a);
	g_return_val_if_fail(errvfsresult == GNOME_VFS_OK, errvfsresult);

	errvfsresult = libntfs_gnomevfs_uri_parent_init(&volume_b, b);
	g_return_val_if_fail(errvfsresult == GNOME_VFS_OK, errvfsresult);

	*same_fs_return = (volume_a == volume_b);

	return GNOME_VFS_OK;
}

/**
 * libntfs_gnomevfs_init:
 *
 * Returns: Initialized structure of #GnomeVFSMethod with static methods of
 * libntfs-gnomevfs.
 */
GnomeVFSMethod *libntfs_gnomevfs_method_init(const gchar *method_name,
		const gchar *args)
{
	struct method_name_info *method_name_info;

	g_return_val_if_fail(method_name != NULL, NULL);
	/* 'args' may be NULL if not supplied. */

	method_name_hash_init();

	G_LOCK(method_name_hash);
	method_name_info = g_hash_table_lookup(method_name_hash, method_name);
	if (method_name_info && strcmp(method_name_info->args, args))
		method_name_info = NULL;
	G_UNLOCK(method_name_hash);
	if (!method_name_info) {
		libntfs_new(method_name_info);
		method_name_info->args = g_strdup(args);
		G_LOCK(method_name_hash);
		g_hash_table_replace(method_name_hash, g_strdup(method_name),
				method_name_info);
		G_UNLOCK(method_name_hash);
	}

	G_LOCK(GnomeVFSMethod_static);
	LIBNTFS_MEMZERO(&GnomeVFSMethod_static);
	GnomeVFSMethod_static.method_table_size = sizeof(GnomeVFSMethod_static);
	GnomeVFSMethod_static.open = libntfs_gnomevfs_open;	/* mandatory */
	GnomeVFSMethod_static.create = libntfs_gnomevfs_create;	/* mandatory */
	GnomeVFSMethod_static.close = libntfs_gnomevfs_close;
	GnomeVFSMethod_static.read = libntfs_gnomevfs_read;
	GnomeVFSMethod_static.seek = libntfs_gnomevfs_seek;
	GnomeVFSMethod_static.tell = libntfs_gnomevfs_tell;
	GnomeVFSMethod_static.open_directory = libntfs_gnomevfs_open_directory;
	GnomeVFSMethod_static.close_directory =
			libntfs_gnomevfs_close_directory;
	GnomeVFSMethod_static.read_directory = libntfs_gnomevfs_read_directory;
	GnomeVFSMethod_static.get_file_info =
			libntfs_gnomevfs_get_file_info;	/* mandatory */
	GnomeVFSMethod_static.get_file_info_from_handle =
			libntfs_gnomevfs_get_file_info_from_handle;
	GnomeVFSMethod_static.is_local =
			libntfs_gnomevfs_is_local;	/* mandatory */
	GnomeVFSMethod_static.check_same_fs = libntfs_gnomevfs_check_same_fs;
	/* TODO: GnomeVFSMethodFindDirectoryFunc find_directory; */
	/* TODO: GnomeVFSMethodFileControlFunc file_control; */
	/* R/W:  GnomeVFSMethodCreateSymbolicLinkFunc create_symbolic_link; */
	/* R/W:  GnomeVFSMethodMonitorAddFunc monitor_add; */
	/* R/W:  GnomeVFSMethodMonitorCancelFunc monitor_cancel; */
	/* R/W:  GnomeVFSMethod_static.write; */
	/* R/W:  GnomeVFSMethod_static.truncate_handle; */
	/* R/W:  GnomeVFSMethod_static.make_directory; */
	/* R/W:  GnomeVFSMethod_static.remove_directory; */
	/* R/W:  GnomeVFSMethod_static.move; */
	/* R/W:  GnomeVFSMethod_static.unlink; */
	/* R/W:  GnomeVFSMethod_static.set_file_info; */
	/* R/W:  GnomeVFSMethod_static.truncate; */
	G_UNLOCK(GnomeVFSMethod_static);

	return &GnomeVFSMethod_static;
}

/**
 * libntfs_gnomevfs_method_shutdown:
 *
 * Shutdowns libntfs-gnomevfs successfuly flushing all caches.
 *
 * Sad note about gnome-vfs-2.1.5 is that it never calls this function. :-)
 */
void libntfs_gnomevfs_method_shutdown(void)
{
	uri_parent_string_hash_init();
	G_LOCK(uri_parent_string_hash);
	g_hash_table_destroy(uri_parent_string_hash);
	uri_parent_string_hash = NULL;
	G_UNLOCK(uri_parent_string_hash);

	method_name_hash_init();
	G_LOCK(method_name_hash);
	g_hash_table_destroy(method_name_hash);
	method_name_hash = NULL;
	G_UNLOCK(method_name_hash);
}

