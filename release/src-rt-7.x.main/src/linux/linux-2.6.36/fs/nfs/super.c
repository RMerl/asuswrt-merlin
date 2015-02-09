/*
 *  linux/fs/nfs/super.c
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  nfs superblock handling functions
 *
 *  Modularised by Alan Cox <alan@lxorguk.ukuu.org.uk>, while hacking some
 *  experimental NFS changes. Modularisation taken straight from SYS5 fs.
 *
 *  Change to nfs_read_super() to permit NFS mounts to multi-homed hosts.
 *  J.S.Peatfield@damtp.cam.ac.uk
 *
 *  Split from inode.c by David Howells <dhowells@redhat.com>
 *
 * - superblocks are indexed on server only - all inodes, dentries, etc. associated with a
 *   particular server are held in the same superblock
 * - NFS superblocks can have several effective roots to the dentry tree
 * - directory type roots are spliced into the tree when a path from one root reaches the root
 *   of another (see nfs_lookup())
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/metrics.h>
#include <linux/sunrpc/xprtsock.h>
#include <linux/sunrpc/xprtrdma.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/nfs4_mount.h>
#include <linux/lockd/bind.h>
#include <linux/smp_lock.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/mnt_namespace.h>
#include <linux/namei.h>
#include <linux/nfs_idmap.h>
#include <linux/vfs.h>
#include <linux/inet.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <net/ipv6.h>
#include <linux/netdevice.h>
#include <linux/nfs_xdr.h>
#include <linux/magic.h>
#include <linux/parser.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "nfs4_fs.h"
#include "callback.h"
#include "delegation.h"
#include "iostat.h"
#include "internal.h"
#include "fscache.h"

#define NFSDBG_FACILITY		NFSDBG_VFS

enum {
	/* Mount options that take no arguments */
	Opt_soft, Opt_hard,
	Opt_posix, Opt_noposix,
	Opt_cto, Opt_nocto,
	Opt_ac, Opt_noac,
	Opt_lock, Opt_nolock,
	Opt_v2, Opt_v3, Opt_v4,
	Opt_udp, Opt_tcp, Opt_rdma,
	Opt_acl, Opt_noacl,
	Opt_rdirplus, Opt_nordirplus,
	Opt_sharecache, Opt_nosharecache,
	Opt_resvport, Opt_noresvport,
	Opt_fscache, Opt_nofscache,

	/* Mount options that take integer arguments */
	Opt_port,
	Opt_rsize, Opt_wsize, Opt_bsize,
	Opt_timeo, Opt_retrans,
	Opt_acregmin, Opt_acregmax,
	Opt_acdirmin, Opt_acdirmax,
	Opt_actimeo,
	Opt_namelen,
	Opt_mountport,
	Opt_mountvers,
	Opt_nfsvers,
	Opt_minorversion,

	/* Mount options that take string arguments */
	Opt_sec, Opt_proto, Opt_mountproto, Opt_mounthost,
	Opt_addr, Opt_mountaddr, Opt_clientaddr,
	Opt_lookupcache,
	Opt_fscache_uniq,

	/* Special mount options */
	Opt_userspace, Opt_deprecated, Opt_sloppy,

	Opt_err
};

static const match_table_t nfs_mount_option_tokens = {
	{ Opt_userspace, "bg" },
	{ Opt_userspace, "fg" },
	{ Opt_userspace, "retry=%s" },

	{ Opt_sloppy, "sloppy" },

	{ Opt_soft, "soft" },
	{ Opt_hard, "hard" },
	{ Opt_deprecated, "intr" },
	{ Opt_deprecated, "nointr" },
	{ Opt_posix, "posix" },
	{ Opt_noposix, "noposix" },
	{ Opt_cto, "cto" },
	{ Opt_nocto, "nocto" },
	{ Opt_ac, "ac" },
	{ Opt_noac, "noac" },
	{ Opt_lock, "lock" },
	{ Opt_nolock, "nolock" },
	{ Opt_v2, "v2" },
	{ Opt_v3, "v3" },
	{ Opt_v4, "v4" },
	{ Opt_udp, "udp" },
	{ Opt_tcp, "tcp" },
	{ Opt_rdma, "rdma" },
	{ Opt_acl, "acl" },
	{ Opt_noacl, "noacl" },
	{ Opt_rdirplus, "rdirplus" },
	{ Opt_nordirplus, "nordirplus" },
	{ Opt_sharecache, "sharecache" },
	{ Opt_nosharecache, "nosharecache" },
	{ Opt_resvport, "resvport" },
	{ Opt_noresvport, "noresvport" },
	{ Opt_fscache, "fsc" },
	{ Opt_nofscache, "nofsc" },

	{ Opt_port, "port=%s" },
	{ Opt_rsize, "rsize=%s" },
	{ Opt_wsize, "wsize=%s" },
	{ Opt_bsize, "bsize=%s" },
	{ Opt_timeo, "timeo=%s" },
	{ Opt_retrans, "retrans=%s" },
	{ Opt_acregmin, "acregmin=%s" },
	{ Opt_acregmax, "acregmax=%s" },
	{ Opt_acdirmin, "acdirmin=%s" },
	{ Opt_acdirmax, "acdirmax=%s" },
	{ Opt_actimeo, "actimeo=%s" },
	{ Opt_namelen, "namlen=%s" },
	{ Opt_mountport, "mountport=%s" },
	{ Opt_mountvers, "mountvers=%s" },
	{ Opt_nfsvers, "nfsvers=%s" },
	{ Opt_nfsvers, "vers=%s" },
	{ Opt_minorversion, "minorversion=%s" },

	{ Opt_sec, "sec=%s" },
	{ Opt_proto, "proto=%s" },
	{ Opt_mountproto, "mountproto=%s" },
	{ Opt_addr, "addr=%s" },
	{ Opt_clientaddr, "clientaddr=%s" },
	{ Opt_mounthost, "mounthost=%s" },
	{ Opt_mountaddr, "mountaddr=%s" },

	{ Opt_lookupcache, "lookupcache=%s" },
	{ Opt_fscache_uniq, "fsc=%s" },

	{ Opt_err, NULL }
};

enum {
	Opt_xprt_udp, Opt_xprt_udp6, Opt_xprt_tcp, Opt_xprt_tcp6, Opt_xprt_rdma,

	Opt_xprt_err
};

static const match_table_t nfs_xprt_protocol_tokens = {
	{ Opt_xprt_udp, "udp" },
	{ Opt_xprt_udp6, "udp6" },
	{ Opt_xprt_tcp, "tcp" },
	{ Opt_xprt_tcp6, "tcp6" },
	{ Opt_xprt_rdma, "rdma" },

	{ Opt_xprt_err, NULL }
};

enum {
	Opt_sec_none, Opt_sec_sys,
	Opt_sec_krb5, Opt_sec_krb5i, Opt_sec_krb5p,
	Opt_sec_lkey, Opt_sec_lkeyi, Opt_sec_lkeyp,
	Opt_sec_spkm, Opt_sec_spkmi, Opt_sec_spkmp,

	Opt_sec_err
};

static const match_table_t nfs_secflavor_tokens = {
	{ Opt_sec_none, "none" },
	{ Opt_sec_none, "null" },
	{ Opt_sec_sys, "sys" },

	{ Opt_sec_krb5, "krb5" },
	{ Opt_sec_krb5i, "krb5i" },
	{ Opt_sec_krb5p, "krb5p" },

	{ Opt_sec_lkey, "lkey" },
	{ Opt_sec_lkeyi, "lkeyi" },
	{ Opt_sec_lkeyp, "lkeyp" },

	{ Opt_sec_spkm, "spkm3" },
	{ Opt_sec_spkmi, "spkm3i" },
	{ Opt_sec_spkmp, "spkm3p" },

	{ Opt_sec_err, NULL }
};

enum {
	Opt_lookupcache_all, Opt_lookupcache_positive,
	Opt_lookupcache_none,

	Opt_lookupcache_err
};

static match_table_t nfs_lookupcache_tokens = {
	{ Opt_lookupcache_all, "all" },
	{ Opt_lookupcache_positive, "pos" },
	{ Opt_lookupcache_positive, "positive" },
	{ Opt_lookupcache_none, "none" },

	{ Opt_lookupcache_err, NULL }
};


static void nfs_umount_begin(struct super_block *);
static int  nfs_statfs(struct dentry *, struct kstatfs *);
static int  nfs_show_options(struct seq_file *, struct vfsmount *);
static int  nfs_show_stats(struct seq_file *, struct vfsmount *);
static int nfs_get_sb(struct file_system_type *, int, const char *, void *, struct vfsmount *);
static int nfs_xdev_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static void nfs_put_super(struct super_block *);
static void nfs_kill_super(struct super_block *);
static int nfs_remount(struct super_block *sb, int *flags, char *raw_data);

static struct file_system_type nfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs",
	.get_sb		= nfs_get_sb,
	.kill_sb	= nfs_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

struct file_system_type nfs_xdev_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs",
	.get_sb		= nfs_xdev_get_sb,
	.kill_sb	= nfs_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

static const struct super_operations nfs_sops = {
	.alloc_inode	= nfs_alloc_inode,
	.destroy_inode	= nfs_destroy_inode,
	.write_inode	= nfs_write_inode,
	.put_super	= nfs_put_super,
	.statfs		= nfs_statfs,
	.evict_inode	= nfs_evict_inode,
	.umount_begin	= nfs_umount_begin,
	.show_options	= nfs_show_options,
	.show_stats	= nfs_show_stats,
	.remount_fs	= nfs_remount,
};

#ifdef CONFIG_NFS_V4
static int nfs4_validate_text_mount_data(void *options,
	struct nfs_parsed_mount_data *args, const char *dev_name);
static int nfs4_try_mount(int flags, const char *dev_name,
	struct nfs_parsed_mount_data *data, struct vfsmount *mnt);
static int nfs4_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static int nfs4_remote_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static int nfs4_xdev_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static int nfs4_referral_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static int nfs4_remote_referral_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt);
static void nfs4_kill_super(struct super_block *sb);

static struct file_system_type nfs4_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs4",
	.get_sb		= nfs4_get_sb,
	.kill_sb	= nfs4_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

static struct file_system_type nfs4_remote_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs4",
	.get_sb		= nfs4_remote_get_sb,
	.kill_sb	= nfs4_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

struct file_system_type nfs4_xdev_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs4",
	.get_sb		= nfs4_xdev_get_sb,
	.kill_sb	= nfs4_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

static struct file_system_type nfs4_remote_referral_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs4",
	.get_sb		= nfs4_remote_referral_get_sb,
	.kill_sb	= nfs4_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

struct file_system_type nfs4_referral_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfs4",
	.get_sb		= nfs4_referral_get_sb,
	.kill_sb	= nfs4_kill_super,
	.fs_flags	= FS_RENAME_DOES_D_MOVE|FS_REVAL_DOT|FS_BINARY_MOUNTDATA,
};

static const struct super_operations nfs4_sops = {
	.alloc_inode	= nfs_alloc_inode,
	.destroy_inode	= nfs_destroy_inode,
	.write_inode	= nfs_write_inode,
	.put_super	= nfs_put_super,
	.statfs		= nfs_statfs,
	.evict_inode	= nfs4_evict_inode,
	.umount_begin	= nfs_umount_begin,
	.show_options	= nfs_show_options,
	.show_stats	= nfs_show_stats,
	.remount_fs	= nfs_remount,
};
#endif

static struct shrinker acl_shrinker = {
	.shrink		= nfs_access_cache_shrinker,
	.seeks		= DEFAULT_SEEKS,
};

/*
 * Register the NFS filesystems
 */
int __init register_nfs_fs(void)
{
	int ret;

        ret = register_filesystem(&nfs_fs_type);
	if (ret < 0)
		goto error_0;

	ret = nfs_register_sysctl();
	if (ret < 0)
		goto error_1;
#ifdef CONFIG_NFS_V4
	ret = register_filesystem(&nfs4_fs_type);
	if (ret < 0)
		goto error_2;
#endif
	register_shrinker(&acl_shrinker);
	return 0;

#ifdef CONFIG_NFS_V4
error_2:
	nfs_unregister_sysctl();
#endif
error_1:
	unregister_filesystem(&nfs_fs_type);
error_0:
	return ret;
}

/*
 * Unregister the NFS filesystems
 */
void __exit unregister_nfs_fs(void)
{
	unregister_shrinker(&acl_shrinker);
#ifdef CONFIG_NFS_V4
	unregister_filesystem(&nfs4_fs_type);
#endif
	nfs_unregister_sysctl();
	unregister_filesystem(&nfs_fs_type);
}

void nfs_sb_active(struct super_block *sb)
{
	struct nfs_server *server = NFS_SB(sb);

	if (atomic_inc_return(&server->active) == 1)
		atomic_inc(&sb->s_active);
}

void nfs_sb_deactive(struct super_block *sb)
{
	struct nfs_server *server = NFS_SB(sb);

	if (atomic_dec_and_test(&server->active))
		deactivate_super(sb);
}

/*
 * Deliver file system statistics to userspace
 */
static int nfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct nfs_server *server = NFS_SB(dentry->d_sb);
	unsigned char blockbits;
	unsigned long blockres;
	struct nfs_fh *fh = NFS_FH(dentry->d_inode);
	struct nfs_fsstat res;
	int error = -ENOMEM;

	res.fattr = nfs_alloc_fattr();
	if (res.fattr == NULL)
		goto out_err;

	error = server->nfs_client->rpc_ops->statfs(server, fh, &res);
	if (unlikely(error == -ESTALE)) {
		struct dentry *pd_dentry;

		pd_dentry = dget_parent(dentry);
		if (pd_dentry != NULL) {
			nfs_zap_caches(pd_dentry->d_inode);
			dput(pd_dentry);
		}
	}
	nfs_free_fattr(res.fattr);
	if (error < 0)
		goto out_err;

	buf->f_type = NFS_SUPER_MAGIC;

	/*
	 * Current versions of glibc do not correctly handle the
	 * case where f_frsize != f_bsize.  Eventually we want to
	 * report the value of wtmult in this field.
	 */
	buf->f_frsize = dentry->d_sb->s_blocksize;

	/*
	 * On most *nix systems, f_blocks, f_bfree, and f_bavail
	 * are reported in units of f_frsize.  Linux hasn't had
	 * an f_frsize field in its statfs struct until recently,
	 * thus historically Linux's sys_statfs reports these
	 * fields in units of f_bsize.
	 */
	buf->f_bsize = dentry->d_sb->s_blocksize;
	blockbits = dentry->d_sb->s_blocksize_bits;
	blockres = (1 << blockbits) - 1;
	buf->f_blocks = (res.tbytes + blockres) >> blockbits;
	buf->f_bfree = (res.fbytes + blockres) >> blockbits;
	buf->f_bavail = (res.abytes + blockres) >> blockbits;

	buf->f_files = res.tfiles;
	buf->f_ffree = res.afiles;

	buf->f_namelen = server->namelen;

	return 0;

 out_err:
	dprintk("%s: statfs error = %d\n", __func__, -error);
	return error;
}

/*
 * Map the security flavour number to a name
 */
static const char *nfs_pseudoflavour_to_name(rpc_authflavor_t flavour)
{
	static const struct {
		rpc_authflavor_t flavour;
		const char *str;
	} sec_flavours[] = {
		{ RPC_AUTH_NULL, "null" },
		{ RPC_AUTH_UNIX, "sys" },
		{ RPC_AUTH_GSS_KRB5, "krb5" },
		{ RPC_AUTH_GSS_KRB5I, "krb5i" },
		{ RPC_AUTH_GSS_KRB5P, "krb5p" },
		{ RPC_AUTH_GSS_LKEY, "lkey" },
		{ RPC_AUTH_GSS_LKEYI, "lkeyi" },
		{ RPC_AUTH_GSS_LKEYP, "lkeyp" },
		{ RPC_AUTH_GSS_SPKM, "spkm" },
		{ RPC_AUTH_GSS_SPKMI, "spkmi" },
		{ RPC_AUTH_GSS_SPKMP, "spkmp" },
		{ UINT_MAX, "unknown" }
	};
	int i;

	for (i = 0; sec_flavours[i].flavour != UINT_MAX; i++) {
		if (sec_flavours[i].flavour == flavour)
			break;
	}
	return sec_flavours[i].str;
}

static void nfs_show_mountd_netid(struct seq_file *m, struct nfs_server *nfss,
				  int showdefaults)
{
	struct sockaddr *sap = (struct sockaddr *) &nfss->mountd_address;

	seq_printf(m, ",mountproto=");
	switch (sap->sa_family) {
	case AF_INET:
		switch (nfss->mountd_protocol) {
		case IPPROTO_UDP:
			seq_printf(m, RPCBIND_NETID_UDP);
			break;
		case IPPROTO_TCP:
			seq_printf(m, RPCBIND_NETID_TCP);
			break;
		default:
			if (showdefaults)
				seq_printf(m, "auto");
		}
		break;
	case AF_INET6:
		switch (nfss->mountd_protocol) {
		case IPPROTO_UDP:
			seq_printf(m, RPCBIND_NETID_UDP6);
			break;
		case IPPROTO_TCP:
			seq_printf(m, RPCBIND_NETID_TCP6);
			break;
		default:
			if (showdefaults)
				seq_printf(m, "auto");
		}
		break;
	default:
		if (showdefaults)
			seq_printf(m, "auto");
	}
}

static void nfs_show_mountd_options(struct seq_file *m, struct nfs_server *nfss,
				    int showdefaults)
{
	struct sockaddr *sap = (struct sockaddr *)&nfss->mountd_address;

	if (nfss->flags & NFS_MOUNT_LEGACY_INTERFACE)
		return;

	switch (sap->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)sap;
		seq_printf(m, ",mountaddr=%pI4", &sin->sin_addr.s_addr);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sap;
		seq_printf(m, ",mountaddr=%pI6c", &sin6->sin6_addr);
		break;
	}
	default:
		if (showdefaults)
			seq_printf(m, ",mountaddr=unspecified");
	}

	if (nfss->mountd_version || showdefaults)
		seq_printf(m, ",mountvers=%u", nfss->mountd_version);
	if (nfss->mountd_port || showdefaults)
		seq_printf(m, ",mountport=%u", nfss->mountd_port);

	nfs_show_mountd_netid(m, nfss, showdefaults);
}

#ifdef CONFIG_NFS_V4
static void nfs_show_nfsv4_options(struct seq_file *m, struct nfs_server *nfss,
				    int showdefaults)
{
	struct nfs_client *clp = nfss->nfs_client;

	seq_printf(m, ",clientaddr=%s", clp->cl_ipaddr);
	seq_printf(m, ",minorversion=%u", clp->cl_minorversion);
}
#else
static void nfs_show_nfsv4_options(struct seq_file *m, struct nfs_server *nfss,
				    int showdefaults)
{
}
#endif

/*
 * Describe the mount options in force on this server representation
 */
static void nfs_show_mount_options(struct seq_file *m, struct nfs_server *nfss,
				   int showdefaults)
{
	static const struct proc_nfs_info {
		int flag;
		const char *str;
		const char *nostr;
	} nfs_info[] = {
		{ NFS_MOUNT_SOFT, ",soft", ",hard" },
		{ NFS_MOUNT_POSIX, ",posix", "" },
		{ NFS_MOUNT_NOCTO, ",nocto", "" },
		{ NFS_MOUNT_NOAC, ",noac", "" },
		{ NFS_MOUNT_NONLM, ",nolock", "" },
		{ NFS_MOUNT_NOACL, ",noacl", "" },
		{ NFS_MOUNT_NORDIRPLUS, ",nordirplus", "" },
		{ NFS_MOUNT_UNSHARED, ",nosharecache", "" },
		{ NFS_MOUNT_NORESVPORT, ",noresvport", "" },
		{ 0, NULL, NULL }
	};
	const struct proc_nfs_info *nfs_infop;
	struct nfs_client *clp = nfss->nfs_client;
	u32 version = clp->rpc_ops->version;

	seq_printf(m, ",vers=%u", version);
	seq_printf(m, ",rsize=%u", nfss->rsize);
	seq_printf(m, ",wsize=%u", nfss->wsize);
	if (nfss->bsize != 0)
		seq_printf(m, ",bsize=%u", nfss->bsize);
	seq_printf(m, ",namlen=%u", nfss->namelen);
	if (nfss->acregmin != NFS_DEF_ACREGMIN*HZ || showdefaults)
		seq_printf(m, ",acregmin=%u", nfss->acregmin/HZ);
	if (nfss->acregmax != NFS_DEF_ACREGMAX*HZ || showdefaults)
		seq_printf(m, ",acregmax=%u", nfss->acregmax/HZ);
	if (nfss->acdirmin != NFS_DEF_ACDIRMIN*HZ || showdefaults)
		seq_printf(m, ",acdirmin=%u", nfss->acdirmin/HZ);
	if (nfss->acdirmax != NFS_DEF_ACDIRMAX*HZ || showdefaults)
		seq_printf(m, ",acdirmax=%u", nfss->acdirmax/HZ);
	for (nfs_infop = nfs_info; nfs_infop->flag; nfs_infop++) {
		if (nfss->flags & nfs_infop->flag)
			seq_puts(m, nfs_infop->str);
		else
			seq_puts(m, nfs_infop->nostr);
	}
	seq_printf(m, ",proto=%s",
		   rpc_peeraddr2str(nfss->client, RPC_DISPLAY_NETID));
	if (version == 4) {
		if (nfss->port != NFS_PORT)
			seq_printf(m, ",port=%u", nfss->port);
	} else
		if (nfss->port)
			seq_printf(m, ",port=%u", nfss->port);

	seq_printf(m, ",timeo=%lu", 10U * nfss->client->cl_timeout->to_initval / HZ);
	seq_printf(m, ",retrans=%u", nfss->client->cl_timeout->to_retries);
	seq_printf(m, ",sec=%s", nfs_pseudoflavour_to_name(nfss->client->cl_auth->au_flavor));

	if (version != 4)
		nfs_show_mountd_options(m, nfss, showdefaults);
	else
		nfs_show_nfsv4_options(m, nfss, showdefaults);

	if (nfss->options & NFS_OPTION_FSCACHE)
		seq_printf(m, ",fsc");

	if (nfss->flags & NFS_MOUNT_LOOKUP_CACHE_NONEG) {
		if (nfss->flags & NFS_MOUNT_LOOKUP_CACHE_NONE)
			seq_printf(m, ",lookupcache=none");
		else
			seq_printf(m, ",lookupcache=pos");
	}
}

/*
 * Describe the mount options on this VFS mountpoint
 */
static int nfs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	struct nfs_server *nfss = NFS_SB(mnt->mnt_sb);

	nfs_show_mount_options(m, nfss, 0);

	seq_printf(m, ",addr=%s",
			rpc_peeraddr2str(nfss->nfs_client->cl_rpcclient,
							RPC_DISPLAY_ADDR));

	return 0;
}

/*
 * Present statistical information for this VFS mountpoint
 */
static int nfs_show_stats(struct seq_file *m, struct vfsmount *mnt)
{
	int i, cpu;
	struct nfs_server *nfss = NFS_SB(mnt->mnt_sb);
	struct rpc_auth *auth = nfss->client->cl_auth;
	struct nfs_iostats totals = { };

	seq_printf(m, "statvers=%s", NFS_IOSTAT_VERS);

	/*
	 * Display all mount option settings
	 */
	seq_printf(m, "\n\topts:\t");
	seq_puts(m, mnt->mnt_sb->s_flags & MS_RDONLY ? "ro" : "rw");
	seq_puts(m, mnt->mnt_sb->s_flags & MS_SYNCHRONOUS ? ",sync" : "");
	seq_puts(m, mnt->mnt_sb->s_flags & MS_NOATIME ? ",noatime" : "");
	seq_puts(m, mnt->mnt_sb->s_flags & MS_NODIRATIME ? ",nodiratime" : "");
	nfs_show_mount_options(m, nfss, 1);

	seq_printf(m, "\n\tage:\t%lu", (jiffies - nfss->mount_time) / HZ);

	seq_printf(m, "\n\tcaps:\t");
	seq_printf(m, "caps=0x%x", nfss->caps);
	seq_printf(m, ",wtmult=%u", nfss->wtmult);
	seq_printf(m, ",dtsize=%u", nfss->dtsize);
	seq_printf(m, ",bsize=%u", nfss->bsize);
	seq_printf(m, ",namlen=%u", nfss->namelen);

#ifdef CONFIG_NFS_V4
	if (nfss->nfs_client->rpc_ops->version == 4) {
		seq_printf(m, "\n\tnfsv4:\t");
		seq_printf(m, "bm0=0x%x", nfss->attr_bitmask[0]);
		seq_printf(m, ",bm1=0x%x", nfss->attr_bitmask[1]);
		seq_printf(m, ",acl=0x%x", nfss->acl_bitmask);
	}
#endif

	/*
	 * Display security flavor in effect for this mount
	 */
	seq_printf(m, "\n\tsec:\tflavor=%u", auth->au_ops->au_flavor);
	if (auth->au_flavor)
		seq_printf(m, ",pseudoflavor=%u", auth->au_flavor);

	/*
	 * Display superblock I/O counters
	 */
	for_each_possible_cpu(cpu) {
		struct nfs_iostats *stats;

		preempt_disable();
		stats = per_cpu_ptr(nfss->io_stats, cpu);

		for (i = 0; i < __NFSIOS_COUNTSMAX; i++)
			totals.events[i] += stats->events[i];
		for (i = 0; i < __NFSIOS_BYTESMAX; i++)
			totals.bytes[i] += stats->bytes[i];
#ifdef CONFIG_NFS_FSCACHE
		for (i = 0; i < __NFSIOS_FSCACHEMAX; i++)
			totals.fscache[i] += stats->fscache[i];
#endif

		preempt_enable();
	}

	seq_printf(m, "\n\tevents:\t");
	for (i = 0; i < __NFSIOS_COUNTSMAX; i++)
		seq_printf(m, "%lu ", totals.events[i]);
	seq_printf(m, "\n\tbytes:\t");
	for (i = 0; i < __NFSIOS_BYTESMAX; i++)
		seq_printf(m, "%Lu ", totals.bytes[i]);
#ifdef CONFIG_NFS_FSCACHE
	if (nfss->options & NFS_OPTION_FSCACHE) {
		seq_printf(m, "\n\tfsc:\t");
		for (i = 0; i < __NFSIOS_FSCACHEMAX; i++)
			seq_printf(m, "%Lu ", totals.bytes[i]);
	}
#endif
	seq_printf(m, "\n");

	rpc_print_iostats(m, nfss->client);

	return 0;
}

/*
 * Begin unmount by attempting to remove all automounted mountpoints we added
 * in response to xdev traversals and referrals
 */
static void nfs_umount_begin(struct super_block *sb)
{
	struct nfs_server *server;
	struct rpc_clnt *rpc;

	server = NFS_SB(sb);
	/* -EIO all pending I/O */
	rpc = server->client_acl;
	if (!IS_ERR(rpc))
		rpc_killall_tasks(rpc);
	rpc = server->client;
	if (!IS_ERR(rpc))
		rpc_killall_tasks(rpc);
}

static struct nfs_parsed_mount_data *nfs_alloc_parsed_mount_data(unsigned int version)
{
	struct nfs_parsed_mount_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data) {
		data->acregmin		= NFS_DEF_ACREGMIN;
		data->acregmax		= NFS_DEF_ACREGMAX;
		data->acdirmin		= NFS_DEF_ACDIRMIN;
		data->acdirmax		= NFS_DEF_ACDIRMAX;
		data->mount_server.port	= NFS_UNSPEC_PORT;
		data->nfs_server.port	= NFS_UNSPEC_PORT;
		data->nfs_server.protocol = XPRT_TRANSPORT_TCP;
		data->auth_flavors[0]	= RPC_AUTH_UNIX;
		data->auth_flavor_len	= 1;
		data->version		= version;
		data->minorversion	= 0;
	}
	return data;
}

/*
 * Sanity-check a server address provided by the mount command.
 *
 * Address family must be initialized, and address must not be
 * the ANY address for that family.
 */
static int nfs_verify_server_address(struct sockaddr *addr)
{
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sa = (struct sockaddr_in *)addr;
		return sa->sin_addr.s_addr != htonl(INADDR_ANY);
	}
	case AF_INET6: {
		struct in6_addr *sa = &((struct sockaddr_in6 *)addr)->sin6_addr;
		return !ipv6_addr_any(sa);
	}
	}

	dfprintk(MOUNT, "NFS: Invalid IP address specified\n");
	return 0;
}

/*
 * Select between a default port value and a user-specified port value.
 * If a zero value is set, then autobind will be used.
 */
static void nfs_set_port(struct sockaddr *sap, int *port,
				 const unsigned short default_port)
{
	if (*port == NFS_UNSPEC_PORT)
		*port = default_port;

	rpc_set_port(sap, *port);
}

/*
 * Sanity check the NFS transport protocol.
 *
 */
static void nfs_validate_transport_protocol(struct nfs_parsed_mount_data *mnt)
{
	switch (mnt->nfs_server.protocol) {
	case XPRT_TRANSPORT_UDP:
	case XPRT_TRANSPORT_TCP:
	case XPRT_TRANSPORT_RDMA:
		break;
	default:
		mnt->nfs_server.protocol = XPRT_TRANSPORT_TCP;
	}
}

/*
 * For text based NFSv2/v3 mounts, the mount protocol transport default
 * settings should depend upon the specified NFS transport.
 */
static void nfs_set_mount_transport_protocol(struct nfs_parsed_mount_data *mnt)
{
	nfs_validate_transport_protocol(mnt);

	if (mnt->mount_server.protocol == XPRT_TRANSPORT_UDP ||
	    mnt->mount_server.protocol == XPRT_TRANSPORT_TCP)
			return;
	switch (mnt->nfs_server.protocol) {
	case XPRT_TRANSPORT_UDP:
		mnt->mount_server.protocol = XPRT_TRANSPORT_UDP;
		break;
	case XPRT_TRANSPORT_TCP:
	case XPRT_TRANSPORT_RDMA:
		mnt->mount_server.protocol = XPRT_TRANSPORT_TCP;
	}
}

/*
 * Parse the value of the 'sec=' option.
 */
static int nfs_parse_security_flavors(char *value,
				      struct nfs_parsed_mount_data *mnt)
{
	substring_t args[MAX_OPT_ARGS];

	dfprintk(MOUNT, "NFS: parsing sec=%s option\n", value);

	switch (match_token(value, nfs_secflavor_tokens, args)) {
	case Opt_sec_none:
		mnt->auth_flavors[0] = RPC_AUTH_NULL;
		break;
	case Opt_sec_sys:
		mnt->auth_flavors[0] = RPC_AUTH_UNIX;
		break;
	case Opt_sec_krb5:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_KRB5;
		break;
	case Opt_sec_krb5i:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_KRB5I;
		break;
	case Opt_sec_krb5p:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_KRB5P;
		break;
	case Opt_sec_lkey:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_LKEY;
		break;
	case Opt_sec_lkeyi:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_LKEYI;
		break;
	case Opt_sec_lkeyp:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_LKEYP;
		break;
	case Opt_sec_spkm:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_SPKM;
		break;
	case Opt_sec_spkmi:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_SPKMI;
		break;
	case Opt_sec_spkmp:
		mnt->auth_flavors[0] = RPC_AUTH_GSS_SPKMP;
		break;
	default:
		return 0;
	}

	mnt->auth_flavor_len = 1;
	return 1;
}

/*
 * Error-check and convert a string of mount options from user space into
 * a data structure.  The whole mount string is processed; bad options are
 * skipped as they are encountered.  If there were no errors, return 1;
 * otherwise return 0 (zero).
 */
static int nfs_parse_mount_options(char *raw,
				   struct nfs_parsed_mount_data *mnt)
{
	char *p, *string, *secdata;
	int rc, sloppy = 0, invalid_option = 0;
	unsigned short protofamily = AF_UNSPEC;
	unsigned short mountfamily = AF_UNSPEC;

	if (!raw) {
		dfprintk(MOUNT, "NFS: mount options string was NULL.\n");
		return 1;
	}
	dfprintk(MOUNT, "NFS: nfs mount opts='%s'\n", raw);

	secdata = alloc_secdata();
	if (!secdata)
		goto out_nomem;

	rc = security_sb_copy_data(raw, secdata);
	if (rc)
		goto out_security_failure;

	rc = security_sb_parse_opts_str(secdata, &mnt->lsm_opts);
	if (rc)
		goto out_security_failure;

	free_secdata(secdata);

	while ((p = strsep(&raw, ",")) != NULL) {
		substring_t args[MAX_OPT_ARGS];
		unsigned long option;
		int token;

		if (!*p)
			continue;

		dfprintk(MOUNT, "NFS:   parsing nfs mount option '%s'\n", p);

		token = match_token(p, nfs_mount_option_tokens, args);
		switch (token) {

		/*
		 * boolean options:  foo/nofoo
		 */
		case Opt_soft:
			mnt->flags |= NFS_MOUNT_SOFT;
			break;
		case Opt_hard:
			mnt->flags &= ~NFS_MOUNT_SOFT;
			break;
		case Opt_posix:
			mnt->flags |= NFS_MOUNT_POSIX;
			break;
		case Opt_noposix:
			mnt->flags &= ~NFS_MOUNT_POSIX;
			break;
		case Opt_cto:
			mnt->flags &= ~NFS_MOUNT_NOCTO;
			break;
		case Opt_nocto:
			mnt->flags |= NFS_MOUNT_NOCTO;
			break;
		case Opt_ac:
			mnt->flags &= ~NFS_MOUNT_NOAC;
			break;
		case Opt_noac:
			mnt->flags |= NFS_MOUNT_NOAC;
			break;
		case Opt_lock:
			mnt->flags &= ~NFS_MOUNT_NONLM;
			break;
		case Opt_nolock:
			mnt->flags |= NFS_MOUNT_NONLM;
			break;
		case Opt_v2:
			mnt->flags &= ~NFS_MOUNT_VER3;
			mnt->version = 2;
			break;
		case Opt_v3:
			mnt->flags |= NFS_MOUNT_VER3;
			mnt->version = 3;
			break;
#ifdef CONFIG_NFS_V4
		case Opt_v4:
			mnt->flags &= ~NFS_MOUNT_VER3;
			mnt->version = 4;
			break;
#endif
		case Opt_udp:
			mnt->flags &= ~NFS_MOUNT_TCP;
			mnt->nfs_server.protocol = XPRT_TRANSPORT_UDP;
			break;
		case Opt_tcp:
			mnt->flags |= NFS_MOUNT_TCP;
			mnt->nfs_server.protocol = XPRT_TRANSPORT_TCP;
			break;
		case Opt_rdma:
			mnt->flags |= NFS_MOUNT_TCP; /* for side protocols */
			mnt->nfs_server.protocol = XPRT_TRANSPORT_RDMA;
			xprt_load_transport(p);
			break;
		case Opt_acl:
			mnt->flags &= ~NFS_MOUNT_NOACL;
			break;
		case Opt_noacl:
			mnt->flags |= NFS_MOUNT_NOACL;
			break;
		case Opt_rdirplus:
			mnt->flags &= ~NFS_MOUNT_NORDIRPLUS;
			break;
		case Opt_nordirplus:
			mnt->flags |= NFS_MOUNT_NORDIRPLUS;
			break;
		case Opt_sharecache:
			mnt->flags &= ~NFS_MOUNT_UNSHARED;
			break;
		case Opt_nosharecache:
			mnt->flags |= NFS_MOUNT_UNSHARED;
			break;
		case Opt_resvport:
			mnt->flags &= ~NFS_MOUNT_NORESVPORT;
			break;
		case Opt_noresvport:
			mnt->flags |= NFS_MOUNT_NORESVPORT;
			break;
		case Opt_fscache:
			mnt->options |= NFS_OPTION_FSCACHE;
			kfree(mnt->fscache_uniq);
			mnt->fscache_uniq = NULL;
			break;
		case Opt_nofscache:
			mnt->options &= ~NFS_OPTION_FSCACHE;
			kfree(mnt->fscache_uniq);
			mnt->fscache_uniq = NULL;
			break;

		/*
		 * options that take numeric values
		 */
		case Opt_port:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0 || option > USHRT_MAX)
				goto out_invalid_value;
			mnt->nfs_server.port = option;
			break;
		case Opt_rsize:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->rsize = option;
			break;
		case Opt_wsize:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->wsize = option;
			break;
		case Opt_bsize:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->bsize = option;
			break;
		case Opt_timeo:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0 || option == 0)
				goto out_invalid_value;
			mnt->timeo = option;
			break;
		case Opt_retrans:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0 || option == 0)
				goto out_invalid_value;
			mnt->retrans = option;
			break;
		case Opt_acregmin:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->acregmin = option;
			break;
		case Opt_acregmax:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->acregmax = option;
			break;
		case Opt_acdirmin:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->acdirmin = option;
			break;
		case Opt_acdirmax:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->acdirmax = option;
			break;
		case Opt_actimeo:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->acregmin = mnt->acregmax =
			mnt->acdirmin = mnt->acdirmax = option;
			break;
		case Opt_namelen:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			mnt->namlen = option;
			break;
		case Opt_mountport:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0 || option > USHRT_MAX)
				goto out_invalid_value;
			mnt->mount_server.port = option;
			break;
		case Opt_mountvers:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0 ||
			    option < NFS_MNT_VERSION ||
			    option > NFS_MNT3_VERSION)
				goto out_invalid_value;
			mnt->mount_server.version = option;
			break;
		case Opt_nfsvers:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			switch (option) {
			case NFS2_VERSION:
				mnt->flags &= ~NFS_MOUNT_VER3;
				mnt->version = 2;
				break;
			case NFS3_VERSION:
				mnt->flags |= NFS_MOUNT_VER3;
				mnt->version = 3;
				break;
#ifdef CONFIG_NFS_V4
			case NFS4_VERSION:
				mnt->flags &= ~NFS_MOUNT_VER3;
				mnt->version = 4;
				break;
#endif
			default:
				goto out_invalid_value;
			}
			break;
		case Opt_minorversion:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = strict_strtoul(string, 10, &option);
			kfree(string);
			if (rc != 0)
				goto out_invalid_value;
			if (option > NFS4_MAX_MINOR_VERSION)
				goto out_invalid_value;
			mnt->minorversion = option;
			break;

		/*
		 * options that take text values
		 */
		case Opt_sec:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			rc = nfs_parse_security_flavors(string, mnt);
			kfree(string);
			if (!rc) {
				dfprintk(MOUNT, "NFS:   unrecognized "
						"security flavor\n");
				return 0;
			}
			break;
		case Opt_proto:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			token = match_token(string,
					    nfs_xprt_protocol_tokens, args);

			protofamily = AF_INET;
			switch (token) {
			case Opt_xprt_udp6:
				protofamily = AF_INET6;
			case Opt_xprt_udp:
				mnt->flags &= ~NFS_MOUNT_TCP;
				mnt->nfs_server.protocol = XPRT_TRANSPORT_UDP;
				kfree(string);
				break;
			case Opt_xprt_tcp6:
				protofamily = AF_INET6;
			case Opt_xprt_tcp:
				mnt->flags |= NFS_MOUNT_TCP;
				mnt->nfs_server.protocol = XPRT_TRANSPORT_TCP;
				kfree(string);
				break;
			case Opt_xprt_rdma:
				/* vector side protocols to TCP */
				mnt->flags |= NFS_MOUNT_TCP;
				mnt->nfs_server.protocol = XPRT_TRANSPORT_RDMA;
				xprt_load_transport(string);
				kfree(string);
				break;
			default:
				dfprintk(MOUNT, "NFS:   unrecognized "
						"transport protocol\n");
				kfree(string);
				return 0;
			}
			break;
		case Opt_mountproto:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			token = match_token(string,
					    nfs_xprt_protocol_tokens, args);
			kfree(string);

			mountfamily = AF_INET;
			switch (token) {
			case Opt_xprt_udp6:
				mountfamily = AF_INET6;
			case Opt_xprt_udp:
				mnt->mount_server.protocol = XPRT_TRANSPORT_UDP;
				break;
			case Opt_xprt_tcp6:
				mountfamily = AF_INET6;
			case Opt_xprt_tcp:
				mnt->mount_server.protocol = XPRT_TRANSPORT_TCP;
				break;
			case Opt_xprt_rdma: /* not used for side protocols */
			default:
				dfprintk(MOUNT, "NFS:   unrecognized "
						"transport protocol\n");
				return 0;
			}
			break;
		case Opt_addr:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			mnt->nfs_server.addrlen =
				rpc_pton(string, strlen(string),
					(struct sockaddr *)
					&mnt->nfs_server.address,
					sizeof(mnt->nfs_server.address));
			kfree(string);
			if (mnt->nfs_server.addrlen == 0)
				goto out_invalid_address;
			break;
		case Opt_clientaddr:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			kfree(mnt->client_address);
			mnt->client_address = string;
			break;
		case Opt_mounthost:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			kfree(mnt->mount_server.hostname);
			mnt->mount_server.hostname = string;
			break;
		case Opt_mountaddr:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			mnt->mount_server.addrlen =
				rpc_pton(string, strlen(string),
					(struct sockaddr *)
					&mnt->mount_server.address,
					sizeof(mnt->mount_server.address));
			kfree(string);
			if (mnt->mount_server.addrlen == 0)
				goto out_invalid_address;
			break;
		case Opt_lookupcache:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			token = match_token(string,
					nfs_lookupcache_tokens, args);
			kfree(string);
			switch (token) {
				case Opt_lookupcache_all:
					mnt->flags &= ~(NFS_MOUNT_LOOKUP_CACHE_NONEG|NFS_MOUNT_LOOKUP_CACHE_NONE);
					break;
				case Opt_lookupcache_positive:
					mnt->flags &= ~NFS_MOUNT_LOOKUP_CACHE_NONE;
					mnt->flags |= NFS_MOUNT_LOOKUP_CACHE_NONEG;
					break;
				case Opt_lookupcache_none:
					mnt->flags |= NFS_MOUNT_LOOKUP_CACHE_NONEG|NFS_MOUNT_LOOKUP_CACHE_NONE;
					break;
				default:
					dfprintk(MOUNT, "NFS:   invalid "
							"lookupcache argument\n");
					return 0;
			};
			break;
		case Opt_fscache_uniq:
			string = match_strdup(args);
			if (string == NULL)
				goto out_nomem;
			kfree(mnt->fscache_uniq);
			mnt->fscache_uniq = string;
			mnt->options |= NFS_OPTION_FSCACHE;
			break;

		/*
		 * Special options
		 */
		case Opt_sloppy:
			sloppy = 1;
			dfprintk(MOUNT, "NFS:   relaxing parsing rules\n");
			break;
		case Opt_userspace:
		case Opt_deprecated:
			dfprintk(MOUNT, "NFS:   ignoring mount option "
					"'%s'\n", p);
			break;

		default:
			invalid_option = 1;
			dfprintk(MOUNT, "NFS:   unrecognized mount option "
					"'%s'\n", p);
		}
	}

	if (!sloppy && invalid_option)
		return 0;

	/*
	 * verify that any proto=/mountproto= options match the address
	 * familiies in the addr=/mountaddr= options.
	 */
	if (protofamily != AF_UNSPEC &&
	    protofamily != mnt->nfs_server.address.ss_family)
		goto out_proto_mismatch;

	if (mountfamily != AF_UNSPEC) {
		if (mnt->mount_server.addrlen) {
			if (mountfamily != mnt->mount_server.address.ss_family)
				goto out_mountproto_mismatch;
		} else {
			if (mountfamily != mnt->nfs_server.address.ss_family)
				goto out_mountproto_mismatch;
		}
	}

	return 1;

out_mountproto_mismatch:
	printk(KERN_INFO "NFS: mount server address does not match mountproto= "
			 "option\n");
	return 0;
out_proto_mismatch:
	printk(KERN_INFO "NFS: server address does not match proto= option\n");
	return 0;
out_invalid_address:
	printk(KERN_INFO "NFS: bad IP address specified: %s\n", p);
	return 0;
out_invalid_value:
	printk(KERN_INFO "NFS: bad mount option value specified: %s\n", p);
	return 0;
out_nomem:
	printk(KERN_INFO "NFS: not enough memory to parse option\n");
	return 0;
out_security_failure:
	free_secdata(secdata);
	printk(KERN_INFO "NFS: security options invalid: %d\n", rc);
	return 0;
}

/*
 * Match the requested auth flavors with the list returned by
 * the server.  Returns zero and sets the mount's authentication
 * flavor on success; returns -EACCES if server does not support
 * the requested flavor.
 */
static int nfs_walk_authlist(struct nfs_parsed_mount_data *args,
			     struct nfs_mount_request *request)
{
	unsigned int i, j, server_authlist_len = *(request->auth_flav_len);

	/*
	 * Certain releases of Linux's mountd return an empty
	 * flavor list.  To prevent behavioral regression with
	 * these servers (ie. rejecting mounts that used to
	 * succeed), revert to pre-2.6.32 behavior (no checking)
	 * if the returned flavor list is empty.
	 */
	if (server_authlist_len == 0)
		return 0;

	/*
	 * We avoid sophisticated negotiating here, as there are
	 * plenty of cases where we can get it wrong, providing
	 * either too little or too much security.
	 *
	 * RFC 2623, section 2.7 suggests we SHOULD prefer the
	 * flavor listed first.  However, some servers list
	 * AUTH_NULL first.  Our caller plants AUTH_SYS, the
	 * preferred default, in args->auth_flavors[0] if user
	 * didn't specify sec= mount option.
	 */
	for (i = 0; i < args->auth_flavor_len; i++)
		for (j = 0; j < server_authlist_len; j++)
			if (args->auth_flavors[i] == request->auth_flavs[j]) {
				dfprintk(MOUNT, "NFS: using auth flavor %d\n",
					request->auth_flavs[j]);
				args->auth_flavors[0] = request->auth_flavs[j];
				return 0;
			}

	dfprintk(MOUNT, "NFS: server does not support requested auth flavor\n");
	nfs_umount(request);
	return -EACCES;
}

/*
 * Use the remote server's MOUNT service to request the NFS file handle
 * corresponding to the provided path.
 */
static int nfs_try_mount(struct nfs_parsed_mount_data *args,
			 struct nfs_fh *root_fh)
{
	rpc_authflavor_t server_authlist[NFS_MAX_SECFLAVORS];
	unsigned int server_authlist_len = ARRAY_SIZE(server_authlist);
	struct nfs_mount_request request = {
		.sap		= (struct sockaddr *)
						&args->mount_server.address,
		.dirpath	= args->nfs_server.export_path,
		.protocol	= args->mount_server.protocol,
		.fh		= root_fh,
		.noresvport	= args->flags & NFS_MOUNT_NORESVPORT,
		.auth_flav_len	= &server_authlist_len,
		.auth_flavs	= server_authlist,
	};
	int status;

	if (args->mount_server.version == 0) {
		switch (args->version) {
			default:
				args->mount_server.version = NFS_MNT3_VERSION;
				break;
			case 2:
				args->mount_server.version = NFS_MNT_VERSION;
		}
	}
	request.version = args->mount_server.version;

	if (args->mount_server.hostname)
		request.hostname = args->mount_server.hostname;
	else
		request.hostname = args->nfs_server.hostname;

	/*
	 * Construct the mount server's address.
	 */
	if (args->mount_server.address.ss_family == AF_UNSPEC) {
		memcpy(request.sap, &args->nfs_server.address,
		       args->nfs_server.addrlen);
		args->mount_server.addrlen = args->nfs_server.addrlen;
	}
	request.salen = args->mount_server.addrlen;
	nfs_set_port(request.sap, &args->mount_server.port, 0);

	/*
	 * Now ask the mount server to map our export path
	 * to a file handle.
	 */
	status = nfs_mount(&request);
	if (status != 0) {
		dfprintk(MOUNT, "NFS: unable to mount server %s, error %d\n",
				request.hostname, status);
		return status;
	}

	/*
	 * MNTv1 (NFSv2) does not support auth flavor negotiation.
	 */
	if (args->mount_server.version != NFS_MNT3_VERSION)
		return 0;
	return nfs_walk_authlist(args, &request);
}

static int nfs_parse_simple_hostname(const char *dev_name,
				     char **hostname, size_t maxnamlen,
				     char **export_path, size_t maxpathlen)
{
	size_t len;
	char *colon, *comma;

	colon = strchr(dev_name, ':');
	if (colon == NULL)
		goto out_bad_devname;

	len = colon - dev_name;
	if (len > maxnamlen)
		goto out_hostname;

	/* N.B. caller will free nfs_server.hostname in all cases */
	*hostname = kstrndup(dev_name, len, GFP_KERNEL);
	if (!*hostname)
		goto out_nomem;

	/* kill possible hostname list: not supported */
	comma = strchr(*hostname, ',');
	if (comma != NULL) {
		if (comma == *hostname)
			goto out_bad_devname;
		*comma = '\0';
	}

	colon++;
	len = strlen(colon);
	if (len > maxpathlen)
		goto out_path;
	*export_path = kstrndup(colon, len, GFP_KERNEL);
	if (!*export_path)
		goto out_nomem;

	dfprintk(MOUNT, "NFS: MNTPATH: '%s'\n", *export_path);
	return 0;

out_bad_devname:
	dfprintk(MOUNT, "NFS: device name not in host:path format\n");
	return -EINVAL;

out_nomem:
	dfprintk(MOUNT, "NFS: not enough memory to parse device name\n");
	return -ENOMEM;

out_hostname:
	dfprintk(MOUNT, "NFS: server hostname too long\n");
	return -ENAMETOOLONG;

out_path:
	dfprintk(MOUNT, "NFS: export pathname too long\n");
	return -ENAMETOOLONG;
}

/*
 * Hostname has square brackets around it because it contains one or
 * more colons.  We look for the first closing square bracket, and a
 * colon must follow it.
 */
static int nfs_parse_protected_hostname(const char *dev_name,
					char **hostname, size_t maxnamlen,
					char **export_path, size_t maxpathlen)
{
	size_t len;
	char *start, *end;

	start = (char *)(dev_name + 1);

	end = strchr(start, ']');
	if (end == NULL)
		goto out_bad_devname;
	if (*(end + 1) != ':')
		goto out_bad_devname;

	len = end - start;
	if (len > maxnamlen)
		goto out_hostname;

	/* N.B. caller will free nfs_server.hostname in all cases */
	*hostname = kstrndup(start, len, GFP_KERNEL);
	if (*hostname == NULL)
		goto out_nomem;

	end += 2;
	len = strlen(end);
	if (len > maxpathlen)
		goto out_path;
	*export_path = kstrndup(end, len, GFP_KERNEL);
	if (!*export_path)
		goto out_nomem;

	return 0;

out_bad_devname:
	dfprintk(MOUNT, "NFS: device name not in host:path format\n");
	return -EINVAL;

out_nomem:
	dfprintk(MOUNT, "NFS: not enough memory to parse device name\n");
	return -ENOMEM;

out_hostname:
	dfprintk(MOUNT, "NFS: server hostname too long\n");
	return -ENAMETOOLONG;

out_path:
	dfprintk(MOUNT, "NFS: export pathname too long\n");
	return -ENAMETOOLONG;
}

/*
 * Split "dev_name" into "hostname:export_path".
 *
 * The leftmost colon demarks the split between the server's hostname
 * and the export path.  If the hostname starts with a left square
 * bracket, then it may contain colons.
 *
 * Note: caller frees hostname and export path, even on error.
 */
static int nfs_parse_devname(const char *dev_name,
			     char **hostname, size_t maxnamlen,
			     char **export_path, size_t maxpathlen)
{
	if (*dev_name == '[')
		return nfs_parse_protected_hostname(dev_name,
						    hostname, maxnamlen,
						    export_path, maxpathlen);

	return nfs_parse_simple_hostname(dev_name,
					 hostname, maxnamlen,
					 export_path, maxpathlen);
}

/*
 * Validate the NFS2/NFS3 mount data
 * - fills in the mount root filehandle
 *
 * For option strings, user space handles the following behaviors:
 *
 * + DNS: mapping server host name to IP address ("addr=" option)
 *
 * + failure mode: how to behave if a mount request can't be handled
 *   immediately ("fg/bg" option)
 *
 * + retry: how often to retry a mount request ("retry=" option)
 *
 * + breaking back: trying proto=udp after proto=tcp, v2 after v3,
 *   mountproto=tcp after mountproto=udp, and so on
 */
static int nfs_validate_mount_data(void *options,
				   struct nfs_parsed_mount_data *args,
				   struct nfs_fh *mntfh,
				   const char *dev_name)
{
	struct nfs_mount_data *data = (struct nfs_mount_data *)options;
	struct sockaddr *sap = (struct sockaddr *)&args->nfs_server.address;

	if (data == NULL)
		goto out_no_data;

	switch (data->version) {
	case 1:
		data->namlen = 0;
	case 2:
		data->bsize = 0;
	case 3:
		if (data->flags & NFS_MOUNT_VER3)
			goto out_no_v3;
		data->root.size = NFS2_FHSIZE;
		memcpy(data->root.data, data->old_root.data, NFS2_FHSIZE);
	case 4:
		if (data->flags & NFS_MOUNT_SECFLAVOUR)
			goto out_no_sec;
	case 5:
		memset(data->context, 0, sizeof(data->context));
	case 6:
		if (data->flags & NFS_MOUNT_VER3) {
			if (data->root.size > NFS3_FHSIZE || data->root.size == 0)
				goto out_invalid_fh;
			mntfh->size = data->root.size;
			args->version = 3;
		} else {
			mntfh->size = NFS2_FHSIZE;
			args->version = 2;
		}


		memcpy(mntfh->data, data->root.data, mntfh->size);
		if (mntfh->size < sizeof(mntfh->data))
			memset(mntfh->data + mntfh->size, 0,
			       sizeof(mntfh->data) - mntfh->size);

		/*
		 * Translate to nfs_parsed_mount_data, which nfs_fill_super
		 * can deal with.
		 */
		args->flags		= data->flags & NFS_MOUNT_FLAGMASK;
		args->flags		|= NFS_MOUNT_LEGACY_INTERFACE;
		args->rsize		= data->rsize;
		args->wsize		= data->wsize;
		args->timeo		= data->timeo;
		args->retrans		= data->retrans;
		args->acregmin		= data->acregmin;
		args->acregmax		= data->acregmax;
		args->acdirmin		= data->acdirmin;
		args->acdirmax		= data->acdirmax;

		memcpy(sap, &data->addr, sizeof(data->addr));
		args->nfs_server.addrlen = sizeof(data->addr);
		if (!nfs_verify_server_address(sap))
			goto out_no_address;

		if (!(data->flags & NFS_MOUNT_TCP))
			args->nfs_server.protocol = XPRT_TRANSPORT_UDP;
		/* N.B. caller will free nfs_server.hostname in all cases */
		args->nfs_server.hostname = kstrdup(data->hostname, GFP_KERNEL);
		args->namlen		= data->namlen;
		args->bsize		= data->bsize;

		if (data->flags & NFS_MOUNT_SECFLAVOUR)
			args->auth_flavors[0] = data->pseudoflavor;
		if (!args->nfs_server.hostname)
			goto out_nomem;

		/*
		 * The legacy version 6 binary mount data from userspace has a
		 * field used only to transport selinux information into the
		 * the kernel.  To continue to support that functionality we
		 * have a touch of selinux knowledge here in the NFS code. The
		 * userspace code converted context=blah to just blah so we are
		 * converting back to the full string selinux understands.
		 */
		if (data->context[0]){
#ifdef CONFIG_SECURITY_SELINUX
			int rc;
			char *opts_str = kmalloc(sizeof(data->context) + 8, GFP_KERNEL);
			if (!opts_str)
				return -ENOMEM;
			strcpy(opts_str, "context=");
			data->context[NFS_MAX_CONTEXT_LEN] = '\0';
			strcat(opts_str, &data->context[0]);
			rc = security_sb_parse_opts_str(opts_str, &args->lsm_opts);
			kfree(opts_str);
			if (rc)
				return rc;
#else
			return -EINVAL;
#endif
		}

		break;
	default: {
		int status;

		if (nfs_parse_mount_options((char *)options, args) == 0)
			return -EINVAL;

		if (!nfs_verify_server_address(sap))
			goto out_no_address;

		if (args->version == 4)
#ifdef CONFIG_NFS_V4
			return nfs4_validate_text_mount_data(options,
							     args, dev_name);
#else
			goto out_v4_not_compiled;
#endif

		nfs_set_port(sap, &args->nfs_server.port, 0);

		nfs_set_mount_transport_protocol(args);

		status = nfs_parse_devname(dev_name,
					   &args->nfs_server.hostname,
					   PAGE_SIZE,
					   &args->nfs_server.export_path,
					   NFS_MAXPATHLEN);
		if (!status)
			status = nfs_try_mount(args, mntfh);

		kfree(args->nfs_server.export_path);
		args->nfs_server.export_path = NULL;

		if (status)
			return status;

		break;
		}
	}

#ifndef CONFIG_NFS_V3
	if (args->version == 3)
		goto out_v3_not_compiled;
#endif /* !CONFIG_NFS_V3 */

	return 0;

out_no_data:
	dfprintk(MOUNT, "NFS: mount program didn't pass any mount data\n");
	return -EINVAL;

out_no_v3:
	dfprintk(MOUNT, "NFS: nfs_mount_data version %d does not support v3\n",
		 data->version);
	return -EINVAL;

out_no_sec:
	dfprintk(MOUNT, "NFS: nfs_mount_data version supports only AUTH_SYS\n");
	return -EINVAL;

#ifndef CONFIG_NFS_V3
out_v3_not_compiled:
	dfprintk(MOUNT, "NFS: NFSv3 is not compiled into kernel\n");
	return -EPROTONOSUPPORT;
#endif /* !CONFIG_NFS_V3 */

#ifndef CONFIG_NFS_V4
out_v4_not_compiled:
	dfprintk(MOUNT, "NFS: NFSv4 is not compiled into kernel\n");
	return -EPROTONOSUPPORT;
#endif /* !CONFIG_NFS_V4 */

out_nomem:
	dfprintk(MOUNT, "NFS: not enough memory to handle mount options\n");
	return -ENOMEM;

out_no_address:
	dfprintk(MOUNT, "NFS: mount program didn't pass remote address\n");
	return -EINVAL;

out_invalid_fh:
	dfprintk(MOUNT, "NFS: invalid root filehandle\n");
	return -EINVAL;
}

static int
nfs_compare_remount_data(struct nfs_server *nfss,
			 struct nfs_parsed_mount_data *data)
{
	if (data->flags != nfss->flags ||
	    data->rsize != nfss->rsize ||
	    data->wsize != nfss->wsize ||
	    data->retrans != nfss->client->cl_timeout->to_retries ||
	    data->auth_flavors[0] != nfss->client->cl_auth->au_flavor ||
	    data->acregmin != nfss->acregmin / HZ ||
	    data->acregmax != nfss->acregmax / HZ ||
	    data->acdirmin != nfss->acdirmin / HZ ||
	    data->acdirmax != nfss->acdirmax / HZ ||
	    data->timeo != (10U * nfss->client->cl_timeout->to_initval / HZ) ||
	    data->nfs_server.port != nfss->port ||
	    data->nfs_server.addrlen != nfss->nfs_client->cl_addrlen ||
	    !rpc_cmp_addr((struct sockaddr *)&data->nfs_server.address,
			  (struct sockaddr *)&nfss->nfs_client->cl_addr))
		return -EINVAL;

	return 0;
}

static int
nfs_remount(struct super_block *sb, int *flags, char *raw_data)
{
	int error;
	struct nfs_server *nfss = sb->s_fs_info;
	struct nfs_parsed_mount_data *data;
	struct nfs_mount_data *options = (struct nfs_mount_data *)raw_data;
	struct nfs4_mount_data *options4 = (struct nfs4_mount_data *)raw_data;
	u32 nfsvers = nfss->nfs_client->rpc_ops->version;

	/*
	 * Userspace mount programs that send binary options generally send
	 * them populated with default values. We have no way to know which
	 * ones were explicitly specified. Fall back to legacy behavior and
	 * just return success.
	 */
	if ((nfsvers == 4 && (!options4 || options4->version == 1)) ||
	    (nfsvers <= 3 && (!options || (options->version >= 1 &&
					   options->version <= 6))))
		return 0;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	/* fill out struct with values from existing mount */
	data->flags = nfss->flags;
	data->rsize = nfss->rsize;
	data->wsize = nfss->wsize;
	data->retrans = nfss->client->cl_timeout->to_retries;
	data->auth_flavors[0] = nfss->client->cl_auth->au_flavor;
	data->acregmin = nfss->acregmin / HZ;
	data->acregmax = nfss->acregmax / HZ;
	data->acdirmin = nfss->acdirmin / HZ;
	data->acdirmax = nfss->acdirmax / HZ;
	data->timeo = 10U * nfss->client->cl_timeout->to_initval / HZ;
	data->nfs_server.port = nfss->port;
	data->nfs_server.addrlen = nfss->nfs_client->cl_addrlen;
	memcpy(&data->nfs_server.address, &nfss->nfs_client->cl_addr,
		data->nfs_server.addrlen);

	/* overwrite those values with any that were specified */
	error = nfs_parse_mount_options((char *)options, data);
	if (error < 0)
		goto out;

	/* compare new mount options with old ones */
	error = nfs_compare_remount_data(nfss, data);
out:
	kfree(data);
	return error;
}

/*
 * Initialise the common bits of the superblock
 */
static inline void nfs_initialise_sb(struct super_block *sb)
{
	struct nfs_server *server = NFS_SB(sb);

	sb->s_magic = NFS_SUPER_MAGIC;

	/* We probably want something more informative here */
	snprintf(sb->s_id, sizeof(sb->s_id),
		 "%x:%x", MAJOR(sb->s_dev), MINOR(sb->s_dev));

	if (sb->s_blocksize == 0)
		sb->s_blocksize = nfs_block_bits(server->wsize,
						 &sb->s_blocksize_bits);

	if (server->flags & NFS_MOUNT_NOAC)
		sb->s_flags |= MS_SYNCHRONOUS;

	sb->s_bdi = &server->backing_dev_info;

	nfs_super_set_maxbytes(sb, server->maxfilesize);
}

/*
 * Finish setting up an NFS2/3 superblock
 */
static void nfs_fill_super(struct super_block *sb,
			   struct nfs_parsed_mount_data *data)
{
	struct nfs_server *server = NFS_SB(sb);

	sb->s_blocksize_bits = 0;
	sb->s_blocksize = 0;
	if (data->bsize)
		sb->s_blocksize = nfs_block_size(data->bsize, &sb->s_blocksize_bits);

	if (server->nfs_client->rpc_ops->version == 3) {
		/* The VFS shouldn't apply the umask to mode bits. We will do
		 * so ourselves when necessary.
		 */
		sb->s_flags |= MS_POSIXACL;
		sb->s_time_gran = 1;
	}

	sb->s_op = &nfs_sops;
 	nfs_initialise_sb(sb);
}

/*
 * Finish setting up a cloned NFS2/3 superblock
 */
static void nfs_clone_super(struct super_block *sb,
			    const struct super_block *old_sb)
{
	struct nfs_server *server = NFS_SB(sb);

	sb->s_blocksize_bits = old_sb->s_blocksize_bits;
	sb->s_blocksize = old_sb->s_blocksize;
	sb->s_maxbytes = old_sb->s_maxbytes;

	if (server->nfs_client->rpc_ops->version == 3) {
		/* The VFS shouldn't apply the umask to mode bits. We will do
		 * so ourselves when necessary.
		 */
		sb->s_flags |= MS_POSIXACL;
		sb->s_time_gran = 1;
	}

	sb->s_op = old_sb->s_op;
 	nfs_initialise_sb(sb);
}

static int nfs_compare_mount_options(const struct super_block *s, const struct nfs_server *b, int flags)
{
	const struct nfs_server *a = s->s_fs_info;
	const struct rpc_clnt *clnt_a = a->client;
	const struct rpc_clnt *clnt_b = b->client;

	if ((s->s_flags & NFS_MS_MASK) != (flags & NFS_MS_MASK))
		goto Ebusy;
	if (a->nfs_client != b->nfs_client)
		goto Ebusy;
	if (a->flags != b->flags)
		goto Ebusy;
	if (a->wsize != b->wsize)
		goto Ebusy;
	if (a->rsize != b->rsize)
		goto Ebusy;
	if (a->acregmin != b->acregmin)
		goto Ebusy;
	if (a->acregmax != b->acregmax)
		goto Ebusy;
	if (a->acdirmin != b->acdirmin)
		goto Ebusy;
	if (a->acdirmax != b->acdirmax)
		goto Ebusy;
	if (clnt_a->cl_auth->au_flavor != clnt_b->cl_auth->au_flavor)
		goto Ebusy;
	return 1;
Ebusy:
	return 0;
}

struct nfs_sb_mountdata {
	struct nfs_server *server;
	int mntflags;
};

static int nfs_set_super(struct super_block *s, void *data)
{
	struct nfs_sb_mountdata *sb_mntdata = data;
	struct nfs_server *server = sb_mntdata->server;
	int ret;

	s->s_flags = sb_mntdata->mntflags;
	s->s_fs_info = server;
	ret = set_anon_super(s, server);
	if (ret == 0)
		server->s_dev = s->s_dev;
	return ret;
}

static int nfs_compare_super_address(struct nfs_server *server1,
				     struct nfs_server *server2)
{
	struct sockaddr *sap1, *sap2;

	sap1 = (struct sockaddr *)&server1->nfs_client->cl_addr;
	sap2 = (struct sockaddr *)&server2->nfs_client->cl_addr;

	if (sap1->sa_family != sap2->sa_family)
		return 0;

	switch (sap1->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin1 = (struct sockaddr_in *)sap1;
		struct sockaddr_in *sin2 = (struct sockaddr_in *)sap2;
		if (sin1->sin_addr.s_addr != sin2->sin_addr.s_addr)
			return 0;
		if (sin1->sin_port != sin2->sin_port)
			return 0;
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin1 = (struct sockaddr_in6 *)sap1;
		struct sockaddr_in6 *sin2 = (struct sockaddr_in6 *)sap2;
		if (!ipv6_addr_equal(&sin1->sin6_addr, &sin2->sin6_addr))
			return 0;
		if (sin1->sin6_port != sin2->sin6_port)
			return 0;
		break;
	}
	default:
		return 0;
	}

	return 1;
}

static int nfs_compare_super(struct super_block *sb, void *data)
{
	struct nfs_sb_mountdata *sb_mntdata = data;
	struct nfs_server *server = sb_mntdata->server, *old = NFS_SB(sb);
	int mntflags = sb_mntdata->mntflags;

	if (!nfs_compare_super_address(old, server))
		return 0;
	/* Note: NFS_MOUNT_UNSHARED == NFS4_MOUNT_UNSHARED */
	if (old->flags & NFS_MOUNT_UNSHARED)
		return 0;
	if (memcmp(&old->fsid, &server->fsid, sizeof(old->fsid)) != 0)
		return 0;
	return nfs_compare_mount_options(sb, server, mntflags);
}

static int nfs_bdi_register(struct nfs_server *server)
{
	return bdi_register_dev(&server->backing_dev_info, server->s_dev);
}

static int nfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt)
{
	struct nfs_server *server = NULL;
	struct super_block *s;
	struct nfs_parsed_mount_data *data;
	struct nfs_fh *mntfh;
	struct dentry *mntroot;
	int (*compare_super)(struct super_block *, void *) = nfs_compare_super;
	struct nfs_sb_mountdata sb_mntdata = {
		.mntflags = flags,
	};
	int error = -ENOMEM;

	data = nfs_alloc_parsed_mount_data(3);
	mntfh = nfs_alloc_fhandle();
	if (data == NULL || mntfh == NULL)
		goto out_free_fh;

	security_init_mnt_opts(&data->lsm_opts);

	/* Validate the mount data */
	error = nfs_validate_mount_data(raw_data, data, mntfh, dev_name);
	if (error < 0)
		goto out;

#ifdef CONFIG_NFS_V4
	if (data->version == 4) {
		error = nfs4_try_mount(flags, dev_name, data, mnt);
		kfree(data->client_address);
		kfree(data->nfs_server.export_path);
		goto out;
	}
#endif	/* CONFIG_NFS_V4 */

	/* Get a volume representation */
	server = nfs_create_server(data, mntfh);
	if (IS_ERR(server)) {
		error = PTR_ERR(server);
		goto out;
	}
	sb_mntdata.server = server;

	if (server->flags & NFS_MOUNT_UNSHARED)
		compare_super = NULL;

	/* Get a superblock - note that we may end up sharing one that already exists */
	s = sget(fs_type, compare_super, nfs_set_super, &sb_mntdata);
	if (IS_ERR(s)) {
		error = PTR_ERR(s);
		goto out_err_nosb;
	}

	if (s->s_fs_info != server) {
		nfs_free_server(server);
		server = NULL;
	} else {
		error = nfs_bdi_register(server);
		if (error)
			goto error_splat_bdi;
	}

	if (!s->s_root) {
		/* initial superblock/root creation */
		nfs_fill_super(s, data);
		nfs_fscache_get_super_cookie(
			s, data ? data->fscache_uniq : NULL, NULL);
	}

	mntroot = nfs_get_root(s, mntfh);
	if (IS_ERR(mntroot)) {
		error = PTR_ERR(mntroot);
		goto error_splat_super;
	}

	error = security_sb_set_mnt_opts(s, &data->lsm_opts);
	if (error)
		goto error_splat_root;

	s->s_flags |= MS_ACTIVE;
	mnt->mnt_sb = s;
	mnt->mnt_root = mntroot;
	error = 0;

out:
	kfree(data->nfs_server.hostname);
	kfree(data->mount_server.hostname);
	kfree(data->fscache_uniq);
	security_free_mnt_opts(&data->lsm_opts);
out_free_fh:
	nfs_free_fhandle(mntfh);
	kfree(data);
	return error;

out_err_nosb:
	nfs_free_server(server);
	goto out;

error_splat_root:
	dput(mntroot);
error_splat_super:
	if (server && !s->s_root)
		bdi_unregister(&server->backing_dev_info);
error_splat_bdi:
	deactivate_locked_super(s);
	goto out;
}

/*
 * Ensure that we unregister the bdi before kill_anon_super
 * releases the device name
 */
static void nfs_put_super(struct super_block *s)
{
	struct nfs_server *server = NFS_SB(s);

	bdi_unregister(&server->backing_dev_info);
}

/*
 * Destroy an NFS2/3 superblock
 */
static void nfs_kill_super(struct super_block *s)
{
	struct nfs_server *server = NFS_SB(s);

	kill_anon_super(s);
	nfs_fscache_release_super_cookie(s);
	nfs_free_server(server);
}

/*
 * Clone an NFS2/3 server record on xdev traversal (FSID-change)
 */
static int nfs_xdev_get_sb(struct file_system_type *fs_type, int flags,
			   const char *dev_name, void *raw_data,
			   struct vfsmount *mnt)
{
	struct nfs_clone_mount *data = raw_data;
	struct super_block *s;
	struct nfs_server *server;
	struct dentry *mntroot;
	int (*compare_super)(struct super_block *, void *) = nfs_compare_super;
	struct nfs_sb_mountdata sb_mntdata = {
		.mntflags = flags,
	};
	int error;

	dprintk("--> nfs_xdev_get_sb()\n");

	/* create a new volume representation */
	server = nfs_clone_server(NFS_SB(data->sb), data->fh, data->fattr);
	if (IS_ERR(server)) {
		error = PTR_ERR(server);
		goto out_err_noserver;
	}
	sb_mntdata.server = server;

	if (server->flags & NFS_MOUNT_UNSHARED)
		compare_super = NULL;

	/* Get a superblock - note that we may end up sharing one that already exists */
	s = sget(&nfs_fs_type, compare_super, nfs_set_super, &sb_mntdata);
	if (IS_ERR(s)) {
		error = PTR_ERR(s);
		goto out_err_nosb;
	}

	if (s->s_fs_info != server) {
		nfs_free_server(server);
		server = NULL;
	} else {
		error = nfs_bdi_register(server);
		if (error)
			goto error_splat_bdi;
	}

	if (!s->s_root) {
		/* initial superblock/root creation */
		nfs_clone_super(s, data->sb);
		nfs_fscache_get_super_cookie(s, NULL, data);
	}

	mntroot = nfs_get_root(s, data->fh);
	if (IS_ERR(mntroot)) {
		error = PTR_ERR(mntroot);
		goto error_splat_super;
	}
	if (mntroot->d_inode->i_op != NFS_SB(s)->nfs_client->rpc_ops->dir_inode_ops) {
		dput(mntroot);
		error = -ESTALE;
		goto error_splat_super;
	}

	s->s_flags |= MS_ACTIVE;
	mnt->mnt_sb = s;
	mnt->mnt_root = mntroot;

	/* clone any lsm security options from the parent to the new sb */
	security_sb_clone_mnt_opts(data->sb, s);

	dprintk("<-- nfs_xdev_get_sb() = 0\n");
	return 0;

out_err_nosb:
	nfs_free_server(server);
out_err_noserver:
	dprintk("<-- nfs_xdev_get_sb() = %d [error]\n", error);
	return error;

error_splat_super:
	if (server && !s->s_root)
		bdi_unregister(&server->backing_dev_info);
error_splat_bdi:
	deactivate_locked_super(s);
	dprintk("<-- nfs_xdev_get_sb() = %d [splat]\n", error);
	return error;
}

#ifdef CONFIG_NFS_V4

/*
 * Finish setting up a cloned NFS4 superblock
 */
static void nfs4_clone_super(struct super_block *sb,
			    const struct super_block *old_sb)
{
	sb->s_blocksize_bits = old_sb->s_blocksize_bits;
	sb->s_blocksize = old_sb->s_blocksize;
	sb->s_maxbytes = old_sb->s_maxbytes;
	sb->s_time_gran = 1;
	sb->s_op = old_sb->s_op;
 	nfs_initialise_sb(sb);
}

/*
 * Set up an NFS4 superblock
 */
static void nfs4_fill_super(struct super_block *sb)
{
	sb->s_time_gran = 1;
	sb->s_op = &nfs4_sops;
	nfs_initialise_sb(sb);
}

static void nfs4_validate_mount_flags(struct nfs_parsed_mount_data *args)
{
	args->flags &= ~(NFS_MOUNT_NONLM|NFS_MOUNT_NOACL|NFS_MOUNT_VER3);
}

static int nfs4_validate_text_mount_data(void *options,
					 struct nfs_parsed_mount_data *args,
					 const char *dev_name)
{
	struct sockaddr *sap = (struct sockaddr *)&args->nfs_server.address;

	nfs_set_port(sap, &args->nfs_server.port, NFS_PORT);

	nfs_validate_transport_protocol(args);

	nfs4_validate_mount_flags(args);

	if (args->version != 4) {
		dfprintk(MOUNT,
			 "NFS4: Illegal mount version\n");
		return -EINVAL;
	}

	if (args->auth_flavor_len > 1) {
		dfprintk(MOUNT,
			 "NFS4: Too many RPC auth flavours specified\n");
		return -EINVAL;
	}

	if (args->client_address == NULL) {
		dfprintk(MOUNT,
			 "NFS4: mount program didn't pass callback address\n");
		return -EINVAL;
	}

	return nfs_parse_devname(dev_name,
				   &args->nfs_server.hostname,
				   NFS4_MAXNAMLEN,
				   &args->nfs_server.export_path,
				   NFS4_MAXPATHLEN);
}

/*
 * Validate NFSv4 mount options
 */
static int nfs4_validate_mount_data(void *options,
				    struct nfs_parsed_mount_data *args,
				    const char *dev_name)
{
	struct sockaddr *sap = (struct sockaddr *)&args->nfs_server.address;
	struct nfs4_mount_data *data = (struct nfs4_mount_data *)options;
	char *c;

	if (data == NULL)
		goto out_no_data;

	switch (data->version) {
	case 1:
		if (data->host_addrlen > sizeof(args->nfs_server.address))
			goto out_no_address;
		if (data->host_addrlen == 0)
			goto out_no_address;
		args->nfs_server.addrlen = data->host_addrlen;
		if (copy_from_user(sap, data->host_addr, data->host_addrlen))
			return -EFAULT;
		if (!nfs_verify_server_address(sap))
			goto out_no_address;

		if (data->auth_flavourlen) {
			if (data->auth_flavourlen > 1)
				goto out_inval_auth;
			if (copy_from_user(&args->auth_flavors[0],
					   data->auth_flavours,
					   sizeof(args->auth_flavors[0])))
				return -EFAULT;
		}

		c = strndup_user(data->hostname.data, NFS4_MAXNAMLEN);
		if (IS_ERR(c))
			return PTR_ERR(c);
		args->nfs_server.hostname = c;

		c = strndup_user(data->mnt_path.data, NFS4_MAXPATHLEN);
		if (IS_ERR(c))
			return PTR_ERR(c);
		args->nfs_server.export_path = c;
		dfprintk(MOUNT, "NFS: MNTPATH: '%s'\n", c);

		c = strndup_user(data->client_addr.data, 16);
		if (IS_ERR(c))
			return PTR_ERR(c);
		args->client_address = c;

		/*
		 * Translate to nfs_parsed_mount_data, which nfs4_fill_super
		 * can deal with.
		 */

		args->flags	= data->flags & NFS4_MOUNT_FLAGMASK;
		args->rsize	= data->rsize;
		args->wsize	= data->wsize;
		args->timeo	= data->timeo;
		args->retrans	= data->retrans;
		args->acregmin	= data->acregmin;
		args->acregmax	= data->acregmax;
		args->acdirmin	= data->acdirmin;
		args->acdirmax	= data->acdirmax;
		args->nfs_server.protocol = data->proto;
		nfs_validate_transport_protocol(args);

		break;
	default:
		if (nfs_parse_mount_options((char *)options, args) == 0)
			return -EINVAL;

		if (!nfs_verify_server_address(sap))
			return -EINVAL;

		return nfs4_validate_text_mount_data(options, args, dev_name);
	}

	return 0;

out_no_data:
	dfprintk(MOUNT, "NFS4: mount program didn't pass any mount data\n");
	return -EINVAL;

out_inval_auth:
	dfprintk(MOUNT, "NFS4: Invalid number of RPC auth flavours %d\n",
		 data->auth_flavourlen);
	return -EINVAL;

out_no_address:
	dfprintk(MOUNT, "NFS4: mount program didn't pass remote address\n");
	return -EINVAL;
}

/*
 * Get the superblock for the NFS4 root partition
 */
static int nfs4_remote_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt)
{
	struct nfs_parsed_mount_data *data = raw_data;
	struct super_block *s;
	struct nfs_server *server;
	struct nfs_fh *mntfh;
	struct dentry *mntroot;
	int (*compare_super)(struct super_block *, void *) = nfs_compare_super;
	struct nfs_sb_mountdata sb_mntdata = {
		.mntflags = flags,
	};
	int error = -ENOMEM;

	mntfh = nfs_alloc_fhandle();
	if (data == NULL || mntfh == NULL)
		goto out_free_fh;

	security_init_mnt_opts(&data->lsm_opts);

	/* Get a volume representation */
	server = nfs4_create_server(data, mntfh);
	if (IS_ERR(server)) {
		error = PTR_ERR(server);
		goto out;
	}
	sb_mntdata.server = server;

	if (server->flags & NFS4_MOUNT_UNSHARED)
		compare_super = NULL;

	/* Get a superblock - note that we may end up sharing one that already exists */
	s = sget(&nfs4_fs_type, compare_super, nfs_set_super, &sb_mntdata);
	if (IS_ERR(s)) {
		error = PTR_ERR(s);
		goto out_free;
	}

	if (s->s_fs_info != server) {
		nfs_free_server(server);
		server = NULL;
	} else {
		error = nfs_bdi_register(server);
		if (error)
			goto error_splat_bdi;
	}

	if (!s->s_root) {
		/* initial superblock/root creation */
		nfs4_fill_super(s);
		nfs_fscache_get_super_cookie(
			s, data ? data->fscache_uniq : NULL, NULL);
	}

	mntroot = nfs4_get_root(s, mntfh);
	if (IS_ERR(mntroot)) {
		error = PTR_ERR(mntroot);
		goto error_splat_super;
	}

	error = security_sb_set_mnt_opts(s, &data->lsm_opts);
	if (error)
		goto error_splat_root;

	s->s_flags |= MS_ACTIVE;
	mnt->mnt_sb = s;
	mnt->mnt_root = mntroot;
	error = 0;

out:
	security_free_mnt_opts(&data->lsm_opts);
out_free_fh:
	nfs_free_fhandle(mntfh);
	return error;

out_free:
	nfs_free_server(server);
	goto out;

error_splat_root:
	dput(mntroot);
error_splat_super:
	if (server && !s->s_root)
		bdi_unregister(&server->backing_dev_info);
error_splat_bdi:
	deactivate_locked_super(s);
	goto out;
}

static struct vfsmount *nfs_do_root_mount(struct file_system_type *fs_type,
		int flags, void *data, const char *hostname)
{
	struct vfsmount *root_mnt;
	char *root_devname;
	size_t len;

	len = strlen(hostname) + 3;
	root_devname = kmalloc(len, GFP_KERNEL);
	if (root_devname == NULL)
		return ERR_PTR(-ENOMEM);
	snprintf(root_devname, len, "%s:/", hostname);
	root_mnt = vfs_kern_mount(fs_type, flags, root_devname, data);
	kfree(root_devname);
	return root_mnt;
}

static void nfs_fix_devname(const struct path *path, struct vfsmount *mnt)
{
	char *page = (char *) __get_free_page(GFP_KERNEL);
	char *devname, *tmp;

	if (page == NULL)
		return;
	devname = nfs_path(path->mnt->mnt_devname,
			path->mnt->mnt_root, path->dentry,
			page, PAGE_SIZE);
	if (IS_ERR(devname))
		goto out_freepage;
	tmp = kstrdup(devname, GFP_KERNEL);
	if (tmp == NULL)
		goto out_freepage;
	kfree(mnt->mnt_devname);
	mnt->mnt_devname = tmp;
out_freepage:
	free_page((unsigned long)page);
}

struct nfs_referral_count {
	struct list_head list;
	const struct task_struct *task;
	unsigned int referral_count;
};

static LIST_HEAD(nfs_referral_count_list);
static DEFINE_SPINLOCK(nfs_referral_count_list_lock);

static struct nfs_referral_count *nfs_find_referral_count(void)
{
	struct nfs_referral_count *p;

	list_for_each_entry(p, &nfs_referral_count_list, list) {
		if (p->task == current)
			return p;
	}
	return NULL;
}

#define NFS_MAX_NESTED_REFERRALS 2

static int nfs_referral_loop_protect(void)
{
	struct nfs_referral_count *p, *new;
	int ret = -ENOMEM;

	new = kmalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		goto out;
	new->task = current;
	new->referral_count = 1;

	ret = 0;
	spin_lock(&nfs_referral_count_list_lock);
	p = nfs_find_referral_count();
	if (p != NULL) {
		if (p->referral_count >= NFS_MAX_NESTED_REFERRALS)
			ret = -ELOOP;
		else
			p->referral_count++;
	} else {
		list_add(&new->list, &nfs_referral_count_list);
		new = NULL;
	}
	spin_unlock(&nfs_referral_count_list_lock);
	kfree(new);
out:
	return ret;
}

static void nfs_referral_loop_unprotect(void)
{
	struct nfs_referral_count *p;

	spin_lock(&nfs_referral_count_list_lock);
	p = nfs_find_referral_count();
	p->referral_count--;
	if (p->referral_count == 0)
		list_del(&p->list);
	else
		p = NULL;
	spin_unlock(&nfs_referral_count_list_lock);
	kfree(p);
}

static int nfs_follow_remote_path(struct vfsmount *root_mnt,
		const char *export_path, struct vfsmount *mnt_target)
{
	struct nameidata *nd = NULL;
	struct mnt_namespace *ns_private;
	struct super_block *s;
	int ret;

	nd = kmalloc(sizeof(*nd), GFP_KERNEL);
	if (nd == NULL)
		return -ENOMEM;

	ns_private = create_mnt_ns(root_mnt);
	ret = PTR_ERR(ns_private);
	if (IS_ERR(ns_private))
		goto out_mntput;

	ret = nfs_referral_loop_protect();
	if (ret != 0)
		goto out_put_mnt_ns;

	ret = vfs_path_lookup(root_mnt->mnt_root, root_mnt,
			export_path, LOOKUP_FOLLOW, nd);

	nfs_referral_loop_unprotect();
	put_mnt_ns(ns_private);

	if (ret != 0)
		goto out_err;

	s = nd->path.mnt->mnt_sb;
	atomic_inc(&s->s_active);
	mnt_target->mnt_sb = s;
	mnt_target->mnt_root = dget(nd->path.dentry);

	/* Correct the device pathname */
	nfs_fix_devname(&nd->path, mnt_target);

	path_put(&nd->path);
	kfree(nd);
	down_write(&s->s_umount);
	return 0;
out_put_mnt_ns:
	put_mnt_ns(ns_private);
out_mntput:
	mntput(root_mnt);
out_err:
	kfree(nd);
	return ret;
}

static int nfs4_try_mount(int flags, const char *dev_name,
			 struct nfs_parsed_mount_data *data,
			 struct vfsmount *mnt)
{
	char *export_path;
	struct vfsmount *root_mnt;
	int error;

	dfprintk(MOUNT, "--> nfs4_try_mount()\n");

	export_path = data->nfs_server.export_path;
	data->nfs_server.export_path = "/";
	root_mnt = nfs_do_root_mount(&nfs4_remote_fs_type, flags, data,
			data->nfs_server.hostname);
	data->nfs_server.export_path = export_path;

	error = PTR_ERR(root_mnt);
	if (IS_ERR(root_mnt))
		goto out;

	error = nfs_follow_remote_path(root_mnt, export_path, mnt);

out:
	dfprintk(MOUNT, "<-- nfs4_try_mount() = %d%s\n", error,
			error != 0 ? " [error]" : "");
	return error;
}

/*
 * Get the superblock for an NFS4 mountpoint
 */
static int nfs4_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *raw_data, struct vfsmount *mnt)
{
	struct nfs_parsed_mount_data *data;
	int error = -ENOMEM;

	data = nfs_alloc_parsed_mount_data(4);
	if (data == NULL)
		goto out_free_data;

	/* Validate the mount data */
	error = nfs4_validate_mount_data(raw_data, data, dev_name);
	if (error < 0)
		goto out;

	error = nfs4_try_mount(flags, dev_name, data, mnt);

out:
	kfree(data->client_address);
	kfree(data->nfs_server.export_path);
	kfree(data->nfs_server.hostname);
	kfree(data->fscache_uniq);
out_free_data:
	kfree(data);
	dprintk("<-- nfs4_get_sb() = %d%s\n", error,
			error != 0 ? " [error]" : "");
	return error;
}

static void nfs4_kill_super(struct super_block *sb)
{
	struct nfs_server *server = NFS_SB(sb);

	dprintk("--> %s\n", __func__);
	nfs_super_return_all_delegations(sb);
	kill_anon_super(sb);
	nfs_fscache_release_super_cookie(sb);
	nfs_free_server(server);
	dprintk("<-- %s\n", __func__);
}

/*
 * Clone an NFS4 server record on xdev traversal (FSID-change)
 */
static int nfs4_xdev_get_sb(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data,
			    struct vfsmount *mnt)
{
	struct nfs_clone_mount *data = raw_data;
	struct super_block *s;
	struct nfs_server *server;
	struct dentry *mntroot;
	int (*compare_super)(struct super_block *, void *) = nfs_compare_super;
	struct nfs_sb_mountdata sb_mntdata = {
		.mntflags = flags,
	};
	int error;

	dprintk("--> nfs4_xdev_get_sb()\n");

	/* create a new volume representation */
	server = nfs_clone_server(NFS_SB(data->sb), data->fh, data->fattr);
	if (IS_ERR(server)) {
		error = PTR_ERR(server);
		goto out_err_noserver;
	}
	sb_mntdata.server = server;

	if (server->flags & NFS4_MOUNT_UNSHARED)
		compare_super = NULL;

	/* Get a superblock - note that we may end up sharing one that already exists */
	s = sget(&nfs4_fs_type, compare_super, nfs_set_super, &sb_mntdata);
	if (IS_ERR(s)) {
		error = PTR_ERR(s);
		goto out_err_nosb;
	}

	if (s->s_fs_info != server) {
		nfs_free_server(server);
		server = NULL;
	} else {
		error = nfs_bdi_register(server);
		if (error)
			goto error_splat_bdi;
	}

	if (!s->s_root) {
		/* initial superblock/root creation */
		nfs4_clone_super(s, data->sb);
		nfs_fscache_get_super_cookie(s, NULL, data);
	}

	mntroot = nfs4_get_root(s, data->fh);
	if (IS_ERR(mntroot)) {
		error = PTR_ERR(mntroot);
		goto error_splat_super;
	}
	if (mntroot->d_inode->i_op != NFS_SB(s)->nfs_client->rpc_ops->dir_inode_ops) {
		dput(mntroot);
		error = -ESTALE;
		goto error_splat_super;
	}

	s->s_flags |= MS_ACTIVE;
	mnt->mnt_sb = s;
	mnt->mnt_root = mntroot;

	security_sb_clone_mnt_opts(data->sb, s);

	dprintk("<-- nfs4_xdev_get_sb() = 0\n");
	return 0;

out_err_nosb:
	nfs_free_server(server);
out_err_noserver:
	dprintk("<-- nfs4_xdev_get_sb() = %d [error]\n", error);
	return error;

error_splat_super:
	if (server && !s->s_root)
		bdi_unregister(&server->backing_dev_info);
error_splat_bdi:
	deactivate_locked_super(s);
	dprintk("<-- nfs4_xdev_get_sb() = %d [splat]\n", error);
	return error;
}

static int nfs4_remote_referral_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *raw_data,
		struct vfsmount *mnt)
{
	struct nfs_clone_mount *data = raw_data;
	struct super_block *s;
	struct nfs_server *server;
	struct dentry *mntroot;
	struct nfs_fh *mntfh;
	int (*compare_super)(struct super_block *, void *) = nfs_compare_super;
	struct nfs_sb_mountdata sb_mntdata = {
		.mntflags = flags,
	};
	int error = -ENOMEM;

	dprintk("--> nfs4_referral_get_sb()\n");

	mntfh = nfs_alloc_fhandle();
	if (mntfh == NULL)
		goto out_err_nofh;

	/* create a new volume representation */
	server = nfs4_create_referral_server(data, mntfh);
	if (IS_ERR(server)) {
		error = PTR_ERR(server);
		goto out_err_noserver;
	}
	sb_mntdata.server = server;

	if (server->flags & NFS4_MOUNT_UNSHARED)
		compare_super = NULL;

	/* Get a superblock - note that we may end up sharing one that already exists */
	s = sget(&nfs4_fs_type, compare_super, nfs_set_super, &sb_mntdata);
	if (IS_ERR(s)) {
		error = PTR_ERR(s);
		goto out_err_nosb;
	}

	if (s->s_fs_info != server) {
		nfs_free_server(server);
		server = NULL;
	} else {
		error = nfs_bdi_register(server);
		if (error)
			goto error_splat_bdi;
	}

	if (!s->s_root) {
		/* initial superblock/root creation */
		nfs4_fill_super(s);
		nfs_fscache_get_super_cookie(s, NULL, data);
	}

	mntroot = nfs4_get_root(s, mntfh);
	if (IS_ERR(mntroot)) {
		error = PTR_ERR(mntroot);
		goto error_splat_super;
	}
	if (mntroot->d_inode->i_op != NFS_SB(s)->nfs_client->rpc_ops->dir_inode_ops) {
		dput(mntroot);
		error = -ESTALE;
		goto error_splat_super;
	}

	s->s_flags |= MS_ACTIVE;
	mnt->mnt_sb = s;
	mnt->mnt_root = mntroot;

	security_sb_clone_mnt_opts(data->sb, s);

	nfs_free_fhandle(mntfh);
	dprintk("<-- nfs4_referral_get_sb() = 0\n");
	return 0;

out_err_nosb:
	nfs_free_server(server);
out_err_noserver:
	nfs_free_fhandle(mntfh);
out_err_nofh:
	dprintk("<-- nfs4_referral_get_sb() = %d [error]\n", error);
	return error;

error_splat_super:
	if (server && !s->s_root)
		bdi_unregister(&server->backing_dev_info);
error_splat_bdi:
	deactivate_locked_super(s);
	nfs_free_fhandle(mntfh);
	dprintk("<-- nfs4_referral_get_sb() = %d [splat]\n", error);
	return error;
}

/*
 * Create an NFS4 server record on referral traversal
 */
static int nfs4_referral_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *raw_data,
		struct vfsmount *mnt)
{
	struct nfs_clone_mount *data = raw_data;
	char *export_path;
	struct vfsmount *root_mnt;
	int error;

	dprintk("--> nfs4_referral_get_sb()\n");

	export_path = data->mnt_path;
	data->mnt_path = "/";

	root_mnt = nfs_do_root_mount(&nfs4_remote_referral_fs_type,
			flags, data, data->hostname);
	data->mnt_path = export_path;

	error = PTR_ERR(root_mnt);
	if (IS_ERR(root_mnt))
		goto out;

	error = nfs_follow_remote_path(root_mnt, export_path, mnt);
out:
	dprintk("<-- nfs4_referral_get_sb() = %d%s\n", error,
			error != 0 ? " [error]" : "");
	return error;
}

#endif /* CONFIG_NFS_V4 */
