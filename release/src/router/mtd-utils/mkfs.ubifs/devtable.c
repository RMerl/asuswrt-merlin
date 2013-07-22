/*
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Author: Artem Bityutskiy
 *
 * Part of the device table parsing code was taken from the mkfs.jffs2 utility.
 * The original author of that code is Erik Andersen, hence:
 *	Copyright (C) 2001, 2002 Erik Andersen <andersen@codepoet.org>
 */

/*
 * This file implemented device table support. Device table entries take the
 * form of:
 * <path>    <type> <mode> <uid> <gid> <major> <minor> <start>	<inc> <count>
 * /dev/mem  c       640   0     0     1       1       0        0     -
 *
 * Type can be one of:
 * f  A regular file
 * d  Directory
 * c  Character special device file
 * b  Block special device file
 * p  Fifo (named pipe)
 *
 * Don't bother with symlinks (permissions are irrelevant), hard links (special
 * cases of regular files), or sockets (why bother).
 *
 * Regular files must exist in the target root directory. If a char, block,
 * fifo, or directory does not exist, it will be created.
 *
 * Please, refer the device_table.txt file which can be found at MTD utilities
 * for more information about what the device table is.
 */

#include "mkfs.ubifs.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_itr.h"

/*
 * The hash table which contains paths to files/directories/device nodes
 * referred to in the device table. For example, if the device table refers
 * "/dev/loop0", the @path_htbl will contain "/dev" element.
 */
static struct hashtable *path_htbl;

/* Hash function used for hash tables */
static unsigned int r5_hash(void *s)
{
	unsigned int a = 0;
	const signed char *str = s;

	while (*str) {
		a += *str << 4;
		a += *str >> 4;
		a *= 11;
		str++;
	}

	return a;
}

/*
 * Check whether 2 keys of a hash table are equivalent. The keys are path/file
 * names, so we simply use 'strcmp()'.
 */
static int is_equivalent(void *k1, void *k2)
{
	return !strcmp(k1, k2);
}

/**
 * separate_last - separate out the last path component
 * @buf: the path to split
 * @len: length of the @buf string
 * @path: the beginning of path is returned here
 * @name: the last path component is returned here
 *
 * This helper function separates out the the last component of the full path
 * string. For example, "/dev/loop" would be split on "/dev" and "loop". This
 * function allocates memory for @path and @name and return the result there.
 * Returns zero in case of success and a negative error code in case of
 * failure.
 */
static int separate_last(const char *buf, int len, char **path, char **name)
{
	int path_len = len, name_len;
	const char *p = buf + len, *n;

	while (*--p != '/')
		path_len -= 1;

	/* Drop the final '/' unless this is the root directory */
	name_len = len - path_len;
	n = buf + path_len;
	if (path_len > 1)
		path_len -= 1;

	*path = malloc(path_len + 1);
	if (!*path)
		return err_msg("cannot allocate %d bytes of memory",
			       path_len + 1);
	memcpy(*path, buf, path_len);
	(*path)[path_len] = '\0';

	*name = malloc(name_len + 1);
	if (!*name) {
		free(*path);
		return err_msg("cannot allocate %d bytes of memory",
			       name_len + 1);
	}
	memcpy(*name, n, name_len + 1);

	return 0;
}

static int interpret_table_entry(const char *line)
{
	char buf[1024], type, *path = NULL, *name = NULL;
	int len;
	struct path_htbl_element *ph_elt = NULL;
	struct name_htbl_element *nh_elt = NULL;
	unsigned int mode = 0755, uid = 0, gid = 0, major = 0, minor = 0;
	unsigned int start = 0, increment = 0, count = 0;

	if (sscanf(line, "%1023s %c %o %u %u %u %u %u %u %u",
		   buf, &type, &mode, &uid, &gid, &major, &minor,
		   &start, &increment, &count) < 0)
		return sys_err_msg("sscanf failed");

	dbg_msg(3, "name %s, type %c, mode %o, uid %u, gid %u, major %u, "
		"minor %u, start %u, inc %u, cnt %u",
		buf, type, mode, uid, gid, major, minor, start,
		increment, count);

	len = strnlen(buf, 1024);
	if (len == 1024)
		return err_msg("too long path");

	if (!strcmp(buf, "/"))
		return err_msg("device table entries require absolute paths");
	if (buf[1] == '\0')
		return err_msg("root directory cannot be created");
	if (strstr(buf, "//"))
		return err_msg("'//' cannot be used in the path");
	if (buf[len - 1] == '/')
		return err_msg("do not put '/' at the end");

	if (strstr(buf, "/./") || strstr(buf, "/../") ||
	    !strcmp(buf + len - 2, "/.") || !strcmp(buf + len - 3, "/.."))
		return err_msg("'.' and '..' cannot be used in the path");

	switch (type) {
		case 'd':
			mode |= S_IFDIR;
			break;
		case 'f':
			mode |= S_IFREG;
			break;
		case 'p':
			mode |= S_IFIFO;
			break;
		case 'c':
			mode |= S_IFCHR;
			break;
		case 'b':
			mode |= S_IFBLK;
			break;
		default:
			return err_msg("unsupported file type '%c'", type);
	}

	if (separate_last(buf, len, &path, &name))
		return -1;

	/*
	 * Check if this path already exist in the path hash table and add it
	 * if it is not.
	 */
	ph_elt = hashtable_search(path_htbl, path);
	if (!ph_elt) {
		dbg_msg(3, "inserting '%s' into path hash table", path);
		ph_elt = malloc(sizeof(struct path_htbl_element));
		if (!ph_elt) {
			err_msg("cannot allocate %zd bytes of memory",
				sizeof(struct path_htbl_element));
			goto out_free;
		}

		if (!hashtable_insert(path_htbl, path, ph_elt)) {
			err_msg("cannot insert into path hash table");
			goto out_free;
		}

		ph_elt->path = path;
		path = NULL;
		ph_elt->name_htbl = create_hashtable(128, &r5_hash,
						     &is_equivalent);
		if (!ph_elt->name_htbl) {
			err_msg("cannot create name hash table");
			goto out_free;
		}
	}

	if (increment != 0 && count == 0)
		return err_msg("count cannot be zero if increment is non-zero");

	/*
	 * Add the file/directory/device node (last component of the path) to
	 * the name hashtable. The name hashtable resides in the corresponding
	 * path hashtable element.
	 */

	if (count == 0) {
		/* This entry does not require any iterating */
		nh_elt = malloc(sizeof(struct name_htbl_element));
		if (!nh_elt) {
			err_msg("cannot allocate %zd bytes of memory",
				sizeof(struct name_htbl_element));
			goto out_free;
		}

		nh_elt->mode = mode;
		nh_elt->uid = uid;
		nh_elt->gid = gid;
		nh_elt->dev = makedev(major, minor);

		dbg_msg(3, "inserting '%s' into name hash table (major %d, minor %d)",
			name, major(nh_elt->dev), minor(nh_elt->dev));

		if (hashtable_search(ph_elt->name_htbl, name))
			return err_msg("'%s' is referred twice", buf);

		nh_elt->name = name;
		if (!hashtable_insert(ph_elt->name_htbl, name, nh_elt)) {
			err_msg("cannot insert into name hash table");
			goto out_free;
		}
	} else {
		int i, num = start + count, len = strlen(name) + 20;
		char *nm;

		for (i = start; i < num; i++) {
			nh_elt = malloc(sizeof(struct name_htbl_element));
			if (!nh_elt) {
				err_msg("cannot allocate %zd bytes of memory",
					sizeof(struct name_htbl_element));
				goto out_free;
			}

			nh_elt->mode = mode;
			nh_elt->uid = uid;
			nh_elt->gid = gid;
			nh_elt->dev = makedev(major, minor + (i - start) * increment);

			nm = malloc(len);
			if (!nm) {
				err_msg("cannot allocate %d bytes of memory", len);
				goto out_free;
			}

			sprintf(nm, "%s%d", name, i);
			nh_elt->name = nm;

			dbg_msg(3, "inserting '%s' into name hash table (major %d, minor %d)",
			        nm, major(nh_elt->dev), minor(nh_elt->dev));

			if (hashtable_search(ph_elt->name_htbl, nm)) {
				err_msg("'%s' is referred twice", buf);
				free (nm);
				goto out_free;
			}

			if (!hashtable_insert(ph_elt->name_htbl, nm, nh_elt)) {
				err_msg("cannot insert into name hash table");
				free (nm);
				goto out_free;
			}
		}
		free(name);
		name = NULL;
	}

	return 0;

out_free:
	free(ph_elt);
	free(nh_elt);
	free(path);
	free(name);
	return -1;
}

/**
 * parse_devtable - parse the device table.
 * @tbl_file: device table file name
 *
 * This function parses the device table and prepare the hash table which will
 * later be used by mkfs.ubifs to create the specified files/device nodes.
 * Returns zero in case of success and a negative error code in case of
 * failure.
 */
int parse_devtable(const char *tbl_file)
{
	FILE *f;
	char *line = NULL;
	struct stat st;
	size_t len;

	dbg_msg(1, "parsing device table file '%s'", tbl_file);

	path_htbl = create_hashtable(128, &r5_hash, &is_equivalent);
	if (!path_htbl)
		return err_msg("cannot create path hash table");

	f = fopen(tbl_file, "r");
	if (!f)
		return sys_err_msg("cannot open '%s'", tbl_file);

	if (fstat(fileno(f), &st) < 0) {
		sys_err_msg("cannot stat '%s'", tbl_file);
		goto out_close;
	}

	if (st.st_size < 10) {
		sys_err_msg("'%s' is too short", tbl_file);
		goto out_close;
	}

	/*
	 * The general plan now is to read in one line at a time, check for
	 * leading comment delimiters ('#'), then try and parse the line as a
	 * device table
	 */
	while (getline(&line, &len, f) != -1) {
		/* First trim off any white-space */
		len = strlen(line);

		/* Trim trailing white-space */
		while (len > 0 && isspace(line[len - 1]))
			line[--len] = '\0';
		/* Trim leading white-space */
		memmove(line, &line[strspn(line, " \n\r\t\v")], len);

		/* How long are we after trimming? */
		len = strlen(line);

		/* If this is not a comment line, try to interpret it */
		if (len && *line != '#') {
			if (interpret_table_entry(line)) {
				err_msg("cannot parse '%s'", line);
				goto out_close;
			}
		}

		free(line);
		line = NULL;
	}

	dbg_msg(1, "finished parsing");
	fclose(f);
	return 0;

out_close:
	fclose(f);
	free_devtable_info();
	return -1;
}

/**
 * devtbl_find_path - find a path in the path hash table.
 * @path: UBIFS path to find.
 *
 * This looks up the path hash table. Returns the path hash table element
 * reference if @path was found and %NULL if not.
 */
struct path_htbl_element *devtbl_find_path(const char *path)
{
	if (!path_htbl)
		return NULL;

	return hashtable_search(path_htbl, (void *)path);
}

/**
 * devtbl_find_name - find a name in the name hash table.
 * @ph_etl: path hash table element to find at
 * @name: name to find
 *
 * This looks up the name hash table. Returns the name hash table element
 * reference if @name found and %NULL if not.
 */
struct name_htbl_element *devtbl_find_name(struct path_htbl_element *ph_elt,
					   const char *name)
{
	if (!path_htbl)
		return NULL;

	return hashtable_search(ph_elt->name_htbl, (void *)name);
}

/**
 * override_attributes - override inode attributes.
 * @st: struct stat object to containing the attributes to override
 * @ph_elt: path hash table element object
 * @nh_elt: name hash table element object containing the new values
 *
 * The device table file may override attributes like UID of files. For
 * example, the device table may contain a "/dev" entry, and the UBIFS FS on
 * the host may contain "/dev" directory. In this case the attributes of the
 * "/dev" directory inode has to be as the device table specifies.
 *
 * Note, the hash element is removed by this function as well.
 */
int override_attributes(struct stat *st, struct path_htbl_element *ph_elt,
			struct name_htbl_element *nh_elt)
{
	if (!path_htbl)
		return 0;

	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode) ||
	    S_ISFIFO(st->st_mode))
		return err_msg("%s/%s both exists at UBIFS root at host, "
			       "and is referred from the device table",
			       strcmp(ph_elt->path, "/") ? ph_elt->path : "",
			       nh_elt->name);

	if ((st->st_mode & S_IFMT) != (nh_elt->mode & S_IFMT))
		return err_msg("%s/%s is referred from the device table also exists in "
			       "the UBIFS root directory at host, but the file type is "
			       "different", strcmp(ph_elt->path, "/") ? ph_elt->path : "",
			       nh_elt->name);

	dbg_msg(3, "set UID %d, GID %d, mode %o for %s/%s as device table says",
		nh_elt->uid, nh_elt->gid, nh_elt->mode, ph_elt->path, nh_elt->name);

	st->st_uid = nh_elt->uid;
	st->st_gid = nh_elt->gid;
	st->st_mode = nh_elt->mode;

	hashtable_remove(ph_elt->name_htbl, (void *)nh_elt->name);
	return 0;
}

/**
 * first_name_htbl_element - return first element of the name hash table.
 * @ph_elt: the path hash table the name hash table belongs to
 * @itr: double pointer to a 'struct hashtable_itr' object where the
 *       information about further iterations is stored
 *
 * This function implements name hash table iteration together with
 * 'next_name_htbl_element()'. Returns the first name hash table element or
 * %NULL if the hash table is empty.
 */
struct name_htbl_element *
first_name_htbl_element(struct path_htbl_element *ph_elt,
			struct hashtable_itr **itr)
{
	if (!path_htbl || !ph_elt || hashtable_count(ph_elt->name_htbl) == 0)
		return NULL;

	*itr = hashtable_iterator(ph_elt->name_htbl);
	return hashtable_iterator_value(*itr);
}

/**
 * first_name_htbl_element - return next element of the name hash table.
 * @ph_elt: the path hash table the name hash table belongs to
 * @itr: double pointer to a 'struct hashtable_itr' object where the
 *       information about further iterations is stored
 *
 * This function implements name hash table iteration together with
 * 'first_name_htbl_element()'. Returns the next name hash table element or
 * %NULL if there are no more elements.
 */
struct name_htbl_element *
next_name_htbl_element(struct path_htbl_element *ph_elt,
		       struct hashtable_itr **itr)
{
	if (!path_htbl || !ph_elt || !hashtable_iterator_advance(*itr))
		return NULL;

	return hashtable_iterator_value(*itr);
}

/**
 * free_devtable_info - free device table information.
 *
 * This function frees the path hash table and the name hash tables.
 */
void free_devtable_info(void)
{
	struct hashtable_itr *ph_itr;
	struct path_htbl_element *ph_elt;

	if (!path_htbl)
		return;

	if (hashtable_count(path_htbl) > 0) {
		ph_itr = hashtable_iterator(path_htbl);
		do {
			ph_elt = hashtable_iterator_value(ph_itr);
			/*
			 * Note, since we use the same string for the key and
			 * @name in the name hash table elements, we do not
			 * have to iterate name hash table because @name memory
			 * will be freed when freeing the key.
			 */
			hashtable_destroy(ph_elt->name_htbl, 1);
		} while (hashtable_iterator_advance(ph_itr));
	}
	hashtable_destroy(path_htbl, 1);
}
