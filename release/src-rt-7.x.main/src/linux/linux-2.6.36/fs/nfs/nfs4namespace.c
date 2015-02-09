/*
 * linux/fs/nfs/nfs4namespace.c
 *
 * Copyright (C) 2005 Trond Myklebust <Trond.Myklebust@netapp.com>
 * - Modified by David Howells <dhowells@redhat.com>
 *
 * NFSv4 namespace
 */

#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nfs_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sunrpc/clnt.h>
#include <linux/vfs.h>
#include <linux/inet.h>
#include "internal.h"
#include "nfs4_fs.h"
#include "dns_resolve.h"

#define NFSDBG_FACILITY		NFSDBG_VFS

/*
 * Convert the NFSv4 pathname components into a standard posix path.
 *
 * Note that the resulting string will be placed at the end of the buffer
 */
static inline char *nfs4_pathname_string(const struct nfs4_pathname *pathname,
					 char *buffer, ssize_t buflen)
{
	char *end = buffer + buflen;
	int n;

	*--end = '\0';
	buflen--;

	n = pathname->ncomponents;
	while (--n >= 0) {
		const struct nfs4_string *component = &pathname->components[n];
		buflen -= component->len + 1;
		if (buflen < 0)
			goto Elong;
		end -= component->len;
		memcpy(end, component->data, component->len);
		*--end = '/';
	}
	return end;
Elong:
	return ERR_PTR(-ENAMETOOLONG);
}

/*
 * Determine the mount path as a string
 */
static char *nfs4_path(const struct vfsmount *mnt_parent,
		       const struct dentry *dentry,
		       char *buffer, ssize_t buflen)
{
	const char *srvpath;

	srvpath = strchr(mnt_parent->mnt_devname, ':');
	if (srvpath)
		srvpath++;
	else
		srvpath = mnt_parent->mnt_devname;

	return nfs_path(srvpath, mnt_parent->mnt_root, dentry, buffer, buflen);
}

/*
 * Check that fs_locations::fs_root [RFC3530 6.3] is a prefix for what we
 * believe to be the server path to this dentry
 */
static int nfs4_validate_fspath(const struct vfsmount *mnt_parent,
				const struct dentry *dentry,
				const struct nfs4_fs_locations *locations,
				char *page, char *page2)
{
	const char *path, *fs_path;

	path = nfs4_path(mnt_parent, dentry, page, PAGE_SIZE);
	if (IS_ERR(path))
		return PTR_ERR(path);

	fs_path = nfs4_pathname_string(&locations->fs_path, page2, PAGE_SIZE);
	if (IS_ERR(fs_path))
		return PTR_ERR(fs_path);

	if (strncmp(path, fs_path, strlen(fs_path)) != 0) {
		dprintk("%s: path %s does not begin with fsroot %s\n",
			__func__, path, fs_path);
		return -ENOENT;
	}

	return 0;
}

static size_t nfs_parse_server_name(char *string, size_t len,
		struct sockaddr *sa, size_t salen)
{
	ssize_t ret;

	ret = rpc_pton(string, len, sa, salen);
	if (ret == 0) {
		ret = nfs_dns_resolve_name(string, len, sa, salen);
		if (ret < 0)
			ret = 0;
	}
	return ret;
}

static struct vfsmount *try_location(struct nfs_clone_mount *mountdata,
				     char *page, char *page2,
				     const struct nfs4_fs_location *location)
{
	const size_t addr_bufsize = sizeof(struct sockaddr_storage);
	struct vfsmount *mnt = ERR_PTR(-ENOENT);
	char *mnt_path;
	unsigned int maxbuflen;
	unsigned int s;

	mnt_path = nfs4_pathname_string(&location->rootpath, page2, PAGE_SIZE);
	if (IS_ERR(mnt_path))
		return ERR_CAST(mnt_path);
	mountdata->mnt_path = mnt_path;
	maxbuflen = mnt_path - 1 - page2;

	mountdata->addr = kmalloc(addr_bufsize, GFP_KERNEL);
	if (mountdata->addr == NULL)
		return ERR_PTR(-ENOMEM);

	for (s = 0; s < location->nservers; s++) {
		const struct nfs4_string *buf = &location->servers[s];

		if (buf->len <= 0 || buf->len >= maxbuflen)
			continue;

		if (memchr(buf->data, IPV6_SCOPE_DELIMITER, buf->len))
			continue;

		mountdata->addrlen = nfs_parse_server_name(buf->data, buf->len,
				mountdata->addr, addr_bufsize);
		if (mountdata->addrlen == 0)
			continue;

		rpc_set_port(mountdata->addr, NFS_PORT);

		memcpy(page2, buf->data, buf->len);
		page2[buf->len] = '\0';
		mountdata->hostname = page2;

		snprintf(page, PAGE_SIZE, "%s:%s",
				mountdata->hostname,
				mountdata->mnt_path);

		mnt = vfs_kern_mount(&nfs4_referral_fs_type, 0, page, mountdata);
		if (!IS_ERR(mnt))
			break;
	}
	kfree(mountdata->addr);
	return mnt;
}

/**
 * nfs_follow_referral - set up mountpoint when hitting a referral on moved error
 * @mnt_parent - mountpoint of parent directory
 * @dentry - parent directory
 * @locations - array of NFSv4 server location information
 *
 */
static struct vfsmount *nfs_follow_referral(const struct vfsmount *mnt_parent,
					    const struct dentry *dentry,
					    const struct nfs4_fs_locations *locations)
{
	struct vfsmount *mnt = ERR_PTR(-ENOENT);
	struct nfs_clone_mount mountdata = {
		.sb = mnt_parent->mnt_sb,
		.dentry = dentry,
		.authflavor = NFS_SB(mnt_parent->mnt_sb)->client->cl_auth->au_flavor,
	};
	char *page = NULL, *page2 = NULL;
	int loc, error;

	if (locations == NULL || locations->nlocations <= 0)
		goto out;

	dprintk("%s: referral at %s/%s\n", __func__,
		dentry->d_parent->d_name.name, dentry->d_name.name);

	page = (char *) __get_free_page(GFP_USER);
	if (!page)
		goto out;

	page2 = (char *) __get_free_page(GFP_USER);
	if (!page2)
		goto out;

	/* Ensure fs path is a prefix of current dentry path */
	error = nfs4_validate_fspath(mnt_parent, dentry, locations, page, page2);
	if (error < 0) {
		mnt = ERR_PTR(error);
		goto out;
	}

	for (loc = 0; loc < locations->nlocations; loc++) {
		const struct nfs4_fs_location *location = &locations->locations[loc];

		if (location == NULL || location->nservers <= 0 ||
		    location->rootpath.ncomponents == 0)
			continue;

		mnt = try_location(&mountdata, page, page2, location);
		if (!IS_ERR(mnt))
			break;
	}

out:
	free_page((unsigned long) page);
	free_page((unsigned long) page2);
	dprintk("%s: done\n", __func__);
	return mnt;
}

/*
 * nfs_do_refmount - handle crossing a referral on server
 * @mnt_parent - mountpoint of referral
 * @dentry - dentry of referral
 *
 */
struct vfsmount *nfs_do_refmount(const struct vfsmount *mnt_parent, struct dentry *dentry)
{
	struct vfsmount *mnt = ERR_PTR(-ENOMEM);
	struct dentry *parent;
	struct nfs4_fs_locations *fs_locations = NULL;
	struct page *page;
	int err;

	/* BUG_ON(IS_ROOT(dentry)); */
	dprintk("%s: enter\n", __func__);

	page = alloc_page(GFP_KERNEL);
	if (page == NULL)
		goto out;

	fs_locations = kmalloc(sizeof(struct nfs4_fs_locations), GFP_KERNEL);
	if (fs_locations == NULL)
		goto out_free;

	/* Get locations */
	mnt = ERR_PTR(-ENOENT);

	parent = dget_parent(dentry);
	dprintk("%s: getting locations for %s/%s\n",
		__func__, parent->d_name.name, dentry->d_name.name);

	err = nfs4_proc_fs_locations(parent->d_inode, &dentry->d_name, fs_locations, page);
	dput(parent);
	if (err != 0 ||
	    fs_locations->nlocations <= 0 ||
	    fs_locations->fs_path.ncomponents <= 0)
		goto out_free;

	mnt = nfs_follow_referral(mnt_parent, dentry, fs_locations);
out_free:
	__free_page(page);
	kfree(fs_locations);
out:
	dprintk("%s: done\n", __func__);
	return mnt;
}
