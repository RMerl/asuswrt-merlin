/* Updated: Karl MacMillan <kmacmillan@tresys.com>
 *
 *	Added conditional policy language extensions
 *
 *  Updated: Hewlett-Packard <paul.moore@hp.com>
 *
 *	Added support for the policy capability bitmap
 *
 * Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2003 - 2004 Tresys Technology, LLC
 * Copyright (C) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 */

#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/security.h>
#include <linux/major.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <linux/audit.h>
#include <linux/uaccess.h>

/* selinuxfs pseudo filesystem for exporting the security policy API.
   Based on the proc code and the fs/nfsd/nfsctl.c code. */

#include "flask.h"
#include "avc.h"
#include "avc_ss.h"
#include "security.h"
#include "objsec.h"
#include "conditional.h"

/* Policy capability filenames */
static char *policycap_names[] = {
	"network_peer_controls",
	"open_perms"
};

unsigned int selinux_checkreqprot = CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE;

static int __init checkreqprot_setup(char *str)
{
	unsigned long checkreqprot;
	if (!strict_strtoul(str, 0, &checkreqprot))
		selinux_checkreqprot = checkreqprot ? 1 : 0;
	return 1;
}
__setup("checkreqprot=", checkreqprot_setup);

static DEFINE_MUTEX(sel_mutex);

/* global data for booleans */
static struct dentry *bool_dir;
static int bool_num;
static char **bool_pending_names;
static int *bool_pending_values;

/* global data for classes */
static struct dentry *class_dir;
static unsigned long last_class_ino;

/* global data for policy capabilities */
static struct dentry *policycap_dir;

extern void selnl_notify_setenforce(int val);

/* Check whether a task is allowed to use a security operation. */
static int task_has_security(struct task_struct *tsk,
			     u32 perms)
{
	const struct task_security_struct *tsec;
	u32 sid = 0;

	rcu_read_lock();
	tsec = __task_cred(tsk)->security;
	if (tsec)
		sid = tsec->sid;
	rcu_read_unlock();
	if (!tsec)
		return -EACCES;

	return avc_has_perm(sid, SECINITSID_SECURITY,
			    SECCLASS_SECURITY, perms, NULL);
}

enum sel_inos {
	SEL_ROOT_INO = 2,
	SEL_LOAD,	/* load policy */
	SEL_ENFORCE,	/* get or set enforcing status */
	SEL_CONTEXT,	/* validate context */
	SEL_ACCESS,	/* compute access decision */
	SEL_CREATE,	/* compute create labeling decision */
	SEL_RELABEL,	/* compute relabeling decision */
	SEL_USER,	/* compute reachable user contexts */
	SEL_POLICYVERS,	/* return policy version for this kernel */
	SEL_COMMIT_BOOLS, /* commit new boolean values */
	SEL_MLS,	/* return if MLS policy is enabled */
	SEL_DISABLE,	/* disable SELinux until next reboot */
	SEL_MEMBER,	/* compute polyinstantiation membership decision */
	SEL_CHECKREQPROT, /* check requested protection, not kernel-applied one */
	SEL_COMPAT_NET,	/* whether to use old compat network packet controls */
	SEL_REJECT_UNKNOWN, /* export unknown reject handling to userspace */
	SEL_DENY_UNKNOWN, /* export unknown deny handling to userspace */
	SEL_INO_NEXT,	/* The next inode number to use */
};

static unsigned long sel_last_ino = SEL_INO_NEXT - 1;

#define SEL_INITCON_INO_OFFSET		0x01000000
#define SEL_BOOL_INO_OFFSET		0x02000000
#define SEL_CLASS_INO_OFFSET		0x04000000
#define SEL_POLICYCAP_INO_OFFSET	0x08000000
#define SEL_INO_MASK			0x00ffffff

#define TMPBUFLEN	12
static ssize_t sel_read_enforce(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%d", selinux_enforcing);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
static ssize_t sel_write_enforce(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)

{
	char *page;
	ssize_t length;
	int new_value;

	if (count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value != selinux_enforcing) {
		length = task_has_security(current, SECURITY__SETENFORCE);
		if (length)
			goto out;
		audit_log(current->audit_context, GFP_KERNEL, AUDIT_MAC_STATUS,
			"enforcing=%d old_enforcing=%d auid=%u ses=%u",
			new_value, selinux_enforcing,
			audit_get_loginuid(current),
			audit_get_sessionid(current));
		selinux_enforcing = new_value;
		if (selinux_enforcing)
			avc_ss_reset(0);
		selnl_notify_setenforce(selinux_enforcing);
	}
	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
#else
#define sel_write_enforce NULL
#endif

static const struct file_operations sel_enforce_ops = {
	.read		= sel_read_enforce,
	.write		= sel_write_enforce,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_handle_unknown(struct file *filp, char __user *buf,
					size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;
	ino_t ino = filp->f_path.dentry->d_inode->i_ino;
	int handle_unknown = (ino == SEL_REJECT_UNKNOWN) ?
		security_get_reject_unknown() : !security_get_allow_unknown();

	length = scnprintf(tmpbuf, TMPBUFLEN, "%d", handle_unknown);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static const struct file_operations sel_handle_unknown_ops = {
	.read		= sel_read_handle_unknown,
	.llseek		= generic_file_llseek,
};

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
static ssize_t sel_write_disable(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)

{
	char *page;
	ssize_t length;
	int new_value;
	extern int selinux_disable(void);

	if (count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value) {
		length = selinux_disable();
		if (length < 0)
			goto out;
		audit_log(current->audit_context, GFP_KERNEL, AUDIT_MAC_STATUS,
			"selinux=0 auid=%u ses=%u",
			audit_get_loginuid(current),
			audit_get_sessionid(current));
	}

	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
#else
#define sel_write_disable NULL
#endif

static const struct file_operations sel_disable_ops = {
	.write		= sel_write_disable,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_policyvers(struct file *filp, char __user *buf,
				   size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", POLICYDB_VERSION_MAX);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static const struct file_operations sel_policyvers_ops = {
	.read		= sel_read_policyvers,
	.llseek		= generic_file_llseek,
};

/* declaration for sel_write_load */
static int sel_make_bools(void);
static int sel_make_classes(void);
static int sel_make_policycap(void);

/* declaration for sel_make_class_dirs */
static int sel_make_dir(struct inode *dir, struct dentry *dentry,
			unsigned long *ino);

static ssize_t sel_read_mls(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%d",
			   security_mls_enabled());
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static const struct file_operations sel_mls_ops = {
	.read		= sel_read_mls,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_write_load(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)

{
	int ret;
	ssize_t length;
	void *data = NULL;

	mutex_lock(&sel_mutex);

	length = task_has_security(current, SECURITY__LOAD_POLICY);
	if (length)
		goto out;

	if (*ppos != 0) {
		/* No partial writes. */
		length = -EINVAL;
		goto out;
	}

	if ((count > 64 * 1024 * 1024)
	    || (data = vmalloc(count)) == NULL) {
		length = -ENOMEM;
		goto out;
	}

	length = -EFAULT;
	if (copy_from_user(data, buf, count) != 0)
		goto out;

	length = security_load_policy(data, count);
	if (length)
		goto out;

	ret = sel_make_bools();
	if (ret) {
		length = ret;
		goto out1;
	}

	ret = sel_make_classes();
	if (ret) {
		length = ret;
		goto out1;
	}

	ret = sel_make_policycap();
	if (ret)
		length = ret;
	else
		length = count;

out1:
	audit_log(current->audit_context, GFP_KERNEL, AUDIT_MAC_POLICY_LOAD,
		"policy loaded auid=%u ses=%u",
		audit_get_loginuid(current),
		audit_get_sessionid(current));
out:
	mutex_unlock(&sel_mutex);
	vfree(data);
	return length;
}

static const struct file_operations sel_load_ops = {
	.write		= sel_write_load,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_write_context(struct file *file, char *buf, size_t size)
{
	char *canon;
	u32 sid, len;
	ssize_t length;

	length = task_has_security(current, SECURITY__CHECK_CONTEXT);
	if (length)
		return length;

	length = security_context_to_sid(buf, size, &sid);
	if (length < 0)
		return length;

	length = security_sid_to_context(sid, &canon, &len);
	if (length < 0)
		return length;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		printk(KERN_ERR "SELinux: %s:  context size (%u) exceeds "
			"payload max\n", __func__, len);
		length = -ERANGE;
		goto out;
	}

	memcpy(buf, canon, len);
	length = len;
out:
	kfree(canon);
	return length;
}

static ssize_t sel_read_checkreqprot(struct file *filp, char __user *buf,
				     size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", selinux_checkreqprot);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static ssize_t sel_write_checkreqprot(struct file *file, const char __user *buf,
				      size_t count, loff_t *ppos)
{
	char *page;
	ssize_t length;
	unsigned int new_value;

	length = task_has_security(current, SECURITY__SETCHECKREQPROT);
	if (length)
		return length;

	if (count >= PAGE_SIZE)
		return -ENOMEM;
	if (*ppos != 0) {
		/* No partial writes. */
		return -EINVAL;
	}
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;
	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1)
		goto out;

	selinux_checkreqprot = new_value ? 1 : 0;
	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
static const struct file_operations sel_checkreqprot_ops = {
	.read		= sel_read_checkreqprot,
	.write		= sel_write_checkreqprot,
	.llseek		= generic_file_llseek,
};

/*
 * Remaining nodes use transaction based IO methods like nfsd/nfsctl.c
 */
static ssize_t sel_write_access(struct file *file, char *buf, size_t size);
static ssize_t sel_write_create(struct file *file, char *buf, size_t size);
static ssize_t sel_write_relabel(struct file *file, char *buf, size_t size);
static ssize_t sel_write_user(struct file *file, char *buf, size_t size);
static ssize_t sel_write_member(struct file *file, char *buf, size_t size);

static ssize_t (*write_op[])(struct file *, char *, size_t) = {
	[SEL_ACCESS] = sel_write_access,
	[SEL_CREATE] = sel_write_create,
	[SEL_RELABEL] = sel_write_relabel,
	[SEL_USER] = sel_write_user,
	[SEL_MEMBER] = sel_write_member,
	[SEL_CONTEXT] = sel_write_context,
};

static ssize_t selinux_transaction_write(struct file *file, const char __user *buf, size_t size, loff_t *pos)
{
	ino_t ino = file->f_path.dentry->d_inode->i_ino;
	char *data;
	ssize_t rv;

	if (ino >= ARRAY_SIZE(write_op) || !write_op[ino])
		return -EINVAL;

	data = simple_transaction_get(file, buf, size);
	if (IS_ERR(data))
		return PTR_ERR(data);

	rv = write_op[ino](file, data, size);
	if (rv > 0) {
		simple_transaction_set(file, rv);
		rv = size;
	}
	return rv;
}

static const struct file_operations transaction_ops = {
	.write		= selinux_transaction_write,
	.read		= simple_transaction_read,
	.release	= simple_transaction_release,
	.llseek		= generic_file_llseek,
};

/*
 * payload - write methods
 * If the method has a response, the response should be put in buf,
 * and the length returned.  Otherwise return 0 or and -error.
 */

static ssize_t sel_write_access(struct file *file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid;
	u16 tclass;
	struct av_decision avd;
	ssize_t length;

	length = task_has_security(current, SECURITY__COMPUTE_AV);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kzalloc(size + 1, GFP_KERNEL);
	if (!scon)
		return length;

	tcon = kzalloc(size + 1, GFP_KERNEL);
	if (!tcon)
		goto out;

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon) + 1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon) + 1, &tsid);
	if (length < 0)
		goto out2;

	security_compute_av_user(ssid, tsid, tclass, &avd);

	length = scnprintf(buf, SIMPLE_TRANSACTION_LIMIT,
			  "%x %x %x %x %u %x",
			  avd.allowed, 0xffffffff,
			  avd.auditallow, avd.auditdeny,
			  avd.seqno, avd.flags);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_create(struct file *file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_CREATE);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kzalloc(size + 1, GFP_KERNEL);
	if (!scon)
		return length;

	tcon = kzalloc(size + 1, GFP_KERNEL);
	if (!tcon)
		goto out;

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon) + 1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon) + 1, &tsid);
	if (length < 0)
		goto out2;

	length = security_transition_sid_user(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		printk(KERN_ERR "SELinux: %s:  context size (%u) exceeds "
			"payload max\n", __func__, len);
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_relabel(struct file *file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_RELABEL);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kzalloc(size + 1, GFP_KERNEL);
	if (!scon)
		return length;

	tcon = kzalloc(size + 1, GFP_KERNEL);
	if (!tcon)
		goto out;

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon) + 1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon) + 1, &tsid);
	if (length < 0)
		goto out2;

	length = security_change_sid(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static ssize_t sel_write_user(struct file *file, char *buf, size_t size)
{
	char *con, *user, *ptr;
	u32 sid, *sids;
	ssize_t length;
	char *newcon;
	int i, rc;
	u32 len, nsids;

	length = task_has_security(current, SECURITY__COMPUTE_USER);
	if (length)
		return length;

	length = -ENOMEM;
	con = kzalloc(size + 1, GFP_KERNEL);
	if (!con)
		return length;

	user = kzalloc(size + 1, GFP_KERNEL);
	if (!user)
		goto out;

	length = -EINVAL;
	if (sscanf(buf, "%s %s", con, user) != 2)
		goto out2;

	length = security_context_to_sid(con, strlen(con) + 1, &sid);
	if (length < 0)
		goto out2;

	length = security_get_user_sids(sid, user, &sids, &nsids);
	if (length < 0)
		goto out2;

	length = sprintf(buf, "%u", nsids) + 1;
	ptr = buf + length;
	for (i = 0; i < nsids; i++) {
		rc = security_sid_to_context(sids[i], &newcon, &len);
		if (rc) {
			length = rc;
			goto out3;
		}
		if ((length + len) >= SIMPLE_TRANSACTION_LIMIT) {
			kfree(newcon);
			length = -ERANGE;
			goto out3;
		}
		memcpy(ptr, newcon, len);
		kfree(newcon);
		ptr += len;
		length += len;
	}
out3:
	kfree(sids);
out2:
	kfree(user);
out:
	kfree(con);
	return length;
}

static ssize_t sel_write_member(struct file *file, char *buf, size_t size)
{
	char *scon, *tcon;
	u32 ssid, tsid, newsid;
	u16 tclass;
	ssize_t length;
	char *newcon;
	u32 len;

	length = task_has_security(current, SECURITY__COMPUTE_MEMBER);
	if (length)
		return length;

	length = -ENOMEM;
	scon = kzalloc(size + 1, GFP_KERNEL);
	if (!scon)
		return length;

	tcon = kzalloc(size + 1, GFP_KERNEL);
	if (!tcon)
		goto out;

	length = -EINVAL;
	if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
		goto out2;

	length = security_context_to_sid(scon, strlen(scon) + 1, &ssid);
	if (length < 0)
		goto out2;
	length = security_context_to_sid(tcon, strlen(tcon) + 1, &tsid);
	if (length < 0)
		goto out2;

	length = security_member_sid(ssid, tsid, tclass, &newsid);
	if (length < 0)
		goto out2;

	length = security_sid_to_context(newsid, &newcon, &len);
	if (length < 0)
		goto out2;

	if (len > SIMPLE_TRANSACTION_LIMIT) {
		printk(KERN_ERR "SELinux: %s:  context size (%u) exceeds "
			"payload max\n", __func__, len);
		length = -ERANGE;
		goto out3;
	}

	memcpy(buf, newcon, len);
	length = len;
out3:
	kfree(newcon);
out2:
	kfree(tcon);
out:
	kfree(scon);
	return length;
}

static struct inode *sel_make_inode(struct super_block *sb, int mode)
{
	struct inode *ret = new_inode(sb);

	if (ret) {
		ret->i_mode = mode;
		ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
	}
	return ret;
}

static ssize_t sel_read_bool(struct file *filep, char __user *buf,
			     size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length;
	ssize_t ret;
	int cur_enforcing;
	struct inode *inode = filep->f_path.dentry->d_inode;
	unsigned index = inode->i_ino & SEL_INO_MASK;
	const char *name = filep->f_path.dentry->d_name.name;

	mutex_lock(&sel_mutex);

	if (index >= bool_num || strcmp(name, bool_pending_names[index])) {
		ret = -EINVAL;
		goto out;
	}

	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}

	cur_enforcing = security_get_bool_value(index);
	if (cur_enforcing < 0) {
		ret = cur_enforcing;
		goto out;
	}
	length = scnprintf(page, PAGE_SIZE, "%d %d", cur_enforcing,
			  bool_pending_values[index]);
	ret = simple_read_from_buffer(buf, count, ppos, page, length);
out:
	mutex_unlock(&sel_mutex);
	if (page)
		free_page((unsigned long)page);
	return ret;
}

static ssize_t sel_write_bool(struct file *filep, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length;
	int new_value;
	struct inode *inode = filep->f_path.dentry->d_inode;
	unsigned index = inode->i_ino & SEL_INO_MASK;
	const char *name = filep->f_path.dentry->d_name.name;

	mutex_lock(&sel_mutex);

	length = task_has_security(current, SECURITY__SETBOOL);
	if (length)
		goto out;

	if (index >= bool_num || strcmp(name, bool_pending_names[index])) {
		length = -EINVAL;
		goto out;
	}

	if (count >= PAGE_SIZE) {
		length = -ENOMEM;
		goto out;
	}

	if (*ppos != 0) {
		/* No partial writes. */
		length = -EINVAL;
		goto out;
	}
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		length = -ENOMEM;
		goto out;
	}

	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value)
		new_value = 1;

	bool_pending_values[index] = new_value;
	length = count;

out:
	mutex_unlock(&sel_mutex);
	if (page)
		free_page((unsigned long) page);
	return length;
}

static const struct file_operations sel_bool_ops = {
	.read		= sel_read_bool,
	.write		= sel_write_bool,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_commit_bools_write(struct file *filep,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t length;
	int new_value;

	mutex_lock(&sel_mutex);

	length = task_has_security(current, SECURITY__SETBOOL);
	if (length)
		goto out;

	if (count >= PAGE_SIZE) {
		length = -ENOMEM;
		goto out;
	}
	if (*ppos != 0) {
		/* No partial writes. */
		goto out;
	}
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		length = -ENOMEM;
		goto out;
	}

	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value && bool_pending_values)
		security_set_bools(bool_num, bool_pending_values);

	length = count;

out:
	mutex_unlock(&sel_mutex);
	if (page)
		free_page((unsigned long) page);
	return length;
}

static const struct file_operations sel_commit_bools_ops = {
	.write		= sel_commit_bools_write,
	.llseek		= generic_file_llseek,
};

static void sel_remove_entries(struct dentry *de)
{
	struct list_head *node;

	spin_lock(&dcache_lock);
	node = de->d_subdirs.next;
	while (node != &de->d_subdirs) {
		struct dentry *d = list_entry(node, struct dentry, d_u.d_child);
		list_del_init(node);

		if (d->d_inode) {
			d = dget_locked(d);
			spin_unlock(&dcache_lock);
			d_delete(d);
			simple_unlink(de->d_inode, d);
			dput(d);
			spin_lock(&dcache_lock);
		}
		node = de->d_subdirs.next;
	}

	spin_unlock(&dcache_lock);
}

#define BOOL_DIR_NAME "booleans"

static int sel_make_bools(void)
{
	int i, ret = 0;
	ssize_t len;
	struct dentry *dentry = NULL;
	struct dentry *dir = bool_dir;
	struct inode *inode = NULL;
	struct inode_security_struct *isec;
	char **names = NULL, *page;
	int num;
	int *values = NULL;
	u32 sid;

	/* remove any existing files */
	for (i = 0; i < bool_num; i++)
		kfree(bool_pending_names[i]);
	kfree(bool_pending_names);
	kfree(bool_pending_values);
	bool_pending_names = NULL;
	bool_pending_values = NULL;

	sel_remove_entries(dir);

	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	ret = security_get_bools(&num, &names, &values);
	if (ret != 0)
		goto out;

	for (i = 0; i < num; i++) {
		dentry = d_alloc_name(dir, names[i]);
		if (!dentry) {
			ret = -ENOMEM;
			goto err;
		}
		inode = sel_make_inode(dir->d_sb, S_IFREG | S_IRUGO | S_IWUSR);
		if (!inode) {
			ret = -ENOMEM;
			goto err;
		}

		len = snprintf(page, PAGE_SIZE, "/%s/%s", BOOL_DIR_NAME, names[i]);
		if (len < 0) {
			ret = -EINVAL;
			goto err;
		} else if (len >= PAGE_SIZE) {
			ret = -ENAMETOOLONG;
			goto err;
		}
		isec = (struct inode_security_struct *)inode->i_security;
		ret = security_genfs_sid("selinuxfs", page, SECCLASS_FILE, &sid);
		if (ret)
			goto err;
		isec->sid = sid;
		isec->initialized = 1;
		inode->i_fop = &sel_bool_ops;
		inode->i_ino = i|SEL_BOOL_INO_OFFSET;
		d_add(dentry, inode);
	}
	bool_num = num;
	bool_pending_names = names;
	bool_pending_values = values;
out:
	free_page((unsigned long)page);
	return ret;
err:
	if (names) {
		for (i = 0; i < num; i++)
			kfree(names[i]);
		kfree(names);
	}
	kfree(values);
	sel_remove_entries(dir);
	ret = -ENOMEM;
	goto out;
}

#define NULL_FILE_NAME "null"

struct dentry *selinux_null;

static ssize_t sel_read_avc_cache_threshold(struct file *filp, char __user *buf,
					    size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", avc_cache_threshold);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static ssize_t sel_write_avc_cache_threshold(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)

{
	char *page;
	ssize_t ret;
	int new_value;

	if (count >= PAGE_SIZE) {
		ret = -ENOMEM;
		goto out;
	}

	if (*ppos != 0) {
		/* No partial writes. */
		ret = -EINVAL;
		goto out;
	}

	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(page, buf, count)) {
		ret = -EFAULT;
		goto out_free;
	}

	if (sscanf(page, "%u", &new_value) != 1) {
		ret = -EINVAL;
		goto out;
	}

	if (new_value != avc_cache_threshold) {
		ret = task_has_security(current, SECURITY__SETSECPARAM);
		if (ret)
			goto out_free;
		avc_cache_threshold = new_value;
	}
	ret = count;
out_free:
	free_page((unsigned long)page);
out:
	return ret;
}

static ssize_t sel_read_avc_hash_stats(struct file *filp, char __user *buf,
				       size_t count, loff_t *ppos)
{
	char *page;
	ssize_t ret = 0;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out;
	}
	ret = avc_get_hash_stats(page);
	if (ret >= 0)
		ret = simple_read_from_buffer(buf, count, ppos, page, ret);
	free_page((unsigned long)page);
out:
	return ret;
}

static const struct file_operations sel_avc_cache_threshold_ops = {
	.read		= sel_read_avc_cache_threshold,
	.write		= sel_write_avc_cache_threshold,
	.llseek		= generic_file_llseek,
};

static const struct file_operations sel_avc_hash_stats_ops = {
	.read		= sel_read_avc_hash_stats,
	.llseek		= generic_file_llseek,
};

#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
static struct avc_cache_stats *sel_avc_get_stat_idx(loff_t *idx)
{
	int cpu;

	for (cpu = *idx; cpu < nr_cpu_ids; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*idx = cpu + 1;
		return &per_cpu(avc_cache_stats, cpu);
	}
	return NULL;
}

static void *sel_avc_stats_seq_start(struct seq_file *seq, loff_t *pos)
{
	loff_t n = *pos - 1;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	return sel_avc_get_stat_idx(&n);
}

static void *sel_avc_stats_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return sel_avc_get_stat_idx(pos);
}

static int sel_avc_stats_seq_show(struct seq_file *seq, void *v)
{
	struct avc_cache_stats *st = v;

	if (v == SEQ_START_TOKEN)
		seq_printf(seq, "lookups hits misses allocations reclaims "
			   "frees\n");
	else
		seq_printf(seq, "%u %u %u %u %u %u\n", st->lookups,
			   st->hits, st->misses, st->allocations,
			   st->reclaims, st->frees);
	return 0;
}

static void sel_avc_stats_seq_stop(struct seq_file *seq, void *v)
{ }

static const struct seq_operations sel_avc_cache_stats_seq_ops = {
	.start		= sel_avc_stats_seq_start,
	.next		= sel_avc_stats_seq_next,
	.show		= sel_avc_stats_seq_show,
	.stop		= sel_avc_stats_seq_stop,
};

static int sel_open_avc_cache_stats(struct inode *inode, struct file *file)
{
	return seq_open(file, &sel_avc_cache_stats_seq_ops);
}

static const struct file_operations sel_avc_cache_stats_ops = {
	.open		= sel_open_avc_cache_stats,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif

static int sel_make_avc_files(struct dentry *dir)
{
	int i, ret = 0;
	static struct tree_descr files[] = {
		{ "cache_threshold",
		  &sel_avc_cache_threshold_ops, S_IRUGO|S_IWUSR },
		{ "hash_stats", &sel_avc_hash_stats_ops, S_IRUGO },
#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
		{ "cache_stats", &sel_avc_cache_stats_ops, S_IRUGO },
#endif
	};

	for (i = 0; i < ARRAY_SIZE(files); i++) {
		struct inode *inode;
		struct dentry *dentry;

		dentry = d_alloc_name(dir, files[i].name);
		if (!dentry) {
			ret = -ENOMEM;
			goto out;
		}

		inode = sel_make_inode(dir->d_sb, S_IFREG|files[i].mode);
		if (!inode) {
			ret = -ENOMEM;
			goto out;
		}
		inode->i_fop = files[i].ops;
		inode->i_ino = ++sel_last_ino;
		d_add(dentry, inode);
	}
out:
	return ret;
}

static ssize_t sel_read_initcon(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct inode *inode;
	char *con;
	u32 sid, len;
	ssize_t ret;

	inode = file->f_path.dentry->d_inode;
	sid = inode->i_ino&SEL_INO_MASK;
	ret = security_sid_to_context(sid, &con, &len);
	if (ret < 0)
		return ret;

	ret = simple_read_from_buffer(buf, count, ppos, con, len);
	kfree(con);
	return ret;
}

static const struct file_operations sel_initcon_ops = {
	.read		= sel_read_initcon,
	.llseek		= generic_file_llseek,
};

static int sel_make_initcon_files(struct dentry *dir)
{
	int i, ret = 0;

	for (i = 1; i <= SECINITSID_NUM; i++) {
		struct inode *inode;
		struct dentry *dentry;
		dentry = d_alloc_name(dir, security_get_initial_sid_context(i));
		if (!dentry) {
			ret = -ENOMEM;
			goto out;
		}

		inode = sel_make_inode(dir->d_sb, S_IFREG|S_IRUGO);
		if (!inode) {
			ret = -ENOMEM;
			goto out;
		}
		inode->i_fop = &sel_initcon_ops;
		inode->i_ino = i|SEL_INITCON_INO_OFFSET;
		d_add(dentry, inode);
	}
out:
	return ret;
}

static inline unsigned int sel_div(unsigned long a, unsigned long b)
{
	return a / b - (a % b < 0);
}

static inline unsigned long sel_class_to_ino(u16 class)
{
	return (class * (SEL_VEC_MAX + 1)) | SEL_CLASS_INO_OFFSET;
}

static inline u16 sel_ino_to_class(unsigned long ino)
{
	return sel_div(ino & SEL_INO_MASK, SEL_VEC_MAX + 1);
}

static inline unsigned long sel_perm_to_ino(u16 class, u32 perm)
{
	return (class * (SEL_VEC_MAX + 1) + perm) | SEL_CLASS_INO_OFFSET;
}

static inline u32 sel_ino_to_perm(unsigned long ino)
{
	return (ino & SEL_INO_MASK) % (SEL_VEC_MAX + 1);
}

static ssize_t sel_read_class(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	ssize_t rc, len;
	char *page;
	unsigned long ino = file->f_path.dentry->d_inode->i_ino;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (!page) {
		rc = -ENOMEM;
		goto out;
	}

	len = snprintf(page, PAGE_SIZE, "%d", sel_ino_to_class(ino));
	rc = simple_read_from_buffer(buf, count, ppos, page, len);
	free_page((unsigned long)page);
out:
	return rc;
}

static const struct file_operations sel_class_ops = {
	.read		= sel_read_class,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_perm(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	ssize_t rc, len;
	char *page;
	unsigned long ino = file->f_path.dentry->d_inode->i_ino;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (!page) {
		rc = -ENOMEM;
		goto out;
	}

	len = snprintf(page, PAGE_SIZE, "%d", sel_ino_to_perm(ino));
	rc = simple_read_from_buffer(buf, count, ppos, page, len);
	free_page((unsigned long)page);
out:
	return rc;
}

static const struct file_operations sel_perm_ops = {
	.read		= sel_read_perm,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_policycap(struct file *file, char __user *buf,
				  size_t count, loff_t *ppos)
{
	int value;
	char tmpbuf[TMPBUFLEN];
	ssize_t length;
	unsigned long i_ino = file->f_path.dentry->d_inode->i_ino;

	value = security_policycap_supported(i_ino & SEL_INO_MASK);
	length = scnprintf(tmpbuf, TMPBUFLEN, "%d", value);

	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static const struct file_operations sel_policycap_ops = {
	.read		= sel_read_policycap,
	.llseek		= generic_file_llseek,
};

static int sel_make_perm_files(char *objclass, int classvalue,
				struct dentry *dir)
{
	int i, rc = 0, nperms;
	char **perms;

	rc = security_get_permissions(objclass, &perms, &nperms);
	if (rc)
		goto out;

	for (i = 0; i < nperms; i++) {
		struct inode *inode;
		struct dentry *dentry;

		dentry = d_alloc_name(dir, perms[i]);
		if (!dentry) {
			rc = -ENOMEM;
			goto out1;
		}

		inode = sel_make_inode(dir->d_sb, S_IFREG|S_IRUGO);
		if (!inode) {
			rc = -ENOMEM;
			goto out1;
		}
		inode->i_fop = &sel_perm_ops;
		/* i+1 since perm values are 1-indexed */
		inode->i_ino = sel_perm_to_ino(classvalue, i + 1);
		d_add(dentry, inode);
	}

out1:
	for (i = 0; i < nperms; i++)
		kfree(perms[i]);
	kfree(perms);
out:
	return rc;
}

static int sel_make_class_dir_entries(char *classname, int index,
					struct dentry *dir)
{
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;
	int rc;

	dentry = d_alloc_name(dir, "index");
	if (!dentry) {
		rc = -ENOMEM;
		goto out;
	}

	inode = sel_make_inode(dir->d_sb, S_IFREG|S_IRUGO);
	if (!inode) {
		rc = -ENOMEM;
		goto out;
	}

	inode->i_fop = &sel_class_ops;
	inode->i_ino = sel_class_to_ino(index);
	d_add(dentry, inode);

	dentry = d_alloc_name(dir, "perms");
	if (!dentry) {
		rc = -ENOMEM;
		goto out;
	}

	rc = sel_make_dir(dir->d_inode, dentry, &last_class_ino);
	if (rc)
		goto out;

	rc = sel_make_perm_files(classname, index, dentry);

out:
	return rc;
}

static void sel_remove_classes(void)
{
	struct list_head *class_node;

	list_for_each(class_node, &class_dir->d_subdirs) {
		struct dentry *class_subdir = list_entry(class_node,
					struct dentry, d_u.d_child);
		struct list_head *class_subdir_node;

		list_for_each(class_subdir_node, &class_subdir->d_subdirs) {
			struct dentry *d = list_entry(class_subdir_node,
						struct dentry, d_u.d_child);

			if (d->d_inode)
				if (d->d_inode->i_mode & S_IFDIR)
					sel_remove_entries(d);
		}

		sel_remove_entries(class_subdir);
	}

	sel_remove_entries(class_dir);
}

static int sel_make_classes(void)
{
	int rc = 0, nclasses, i;
	char **classes;

	/* delete any existing entries */
	sel_remove_classes();

	rc = security_get_classes(&classes, &nclasses);
	if (rc < 0)
		goto out;

	/* +2 since classes are 1-indexed */
	last_class_ino = sel_class_to_ino(nclasses + 2);

	for (i = 0; i < nclasses; i++) {
		struct dentry *class_name_dir;

		class_name_dir = d_alloc_name(class_dir, classes[i]);
		if (!class_name_dir) {
			rc = -ENOMEM;
			goto out1;
		}

		rc = sel_make_dir(class_dir->d_inode, class_name_dir,
				&last_class_ino);
		if (rc)
			goto out1;

		/* i+1 since class values are 1-indexed */
		rc = sel_make_class_dir_entries(classes[i], i + 1,
				class_name_dir);
		if (rc)
			goto out1;
	}

out1:
	for (i = 0; i < nclasses; i++)
		kfree(classes[i]);
	kfree(classes);
out:
	return rc;
}

static int sel_make_policycap(void)
{
	unsigned int iter;
	struct dentry *dentry = NULL;
	struct inode *inode = NULL;

	sel_remove_entries(policycap_dir);

	for (iter = 0; iter <= POLICYDB_CAPABILITY_MAX; iter++) {
		if (iter < ARRAY_SIZE(policycap_names))
			dentry = d_alloc_name(policycap_dir,
					      policycap_names[iter]);
		else
			dentry = d_alloc_name(policycap_dir, "unknown");

		if (dentry == NULL)
			return -ENOMEM;

		inode = sel_make_inode(policycap_dir->d_sb, S_IFREG | S_IRUGO);
		if (inode == NULL)
			return -ENOMEM;

		inode->i_fop = &sel_policycap_ops;
		inode->i_ino = iter | SEL_POLICYCAP_INO_OFFSET;
		d_add(dentry, inode);
	}

	return 0;
}

static int sel_make_dir(struct inode *dir, struct dentry *dentry,
			unsigned long *ino)
{
	int ret = 0;
	struct inode *inode;

	inode = sel_make_inode(dir->i_sb, S_IFDIR | S_IRUGO | S_IXUGO);
	if (!inode) {
		ret = -ENOMEM;
		goto out;
	}
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	inode->i_ino = ++(*ino);
	/* directory inodes start off with i_nlink == 2 (for "." entry) */
	inc_nlink(inode);
	d_add(dentry, inode);
	/* bump link count on parent directory, too */
	inc_nlink(dir);
out:
	return ret;
}

static int sel_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret;
	struct dentry *dentry;
	struct inode *inode, *root_inode;
	struct inode_security_struct *isec;

	static struct tree_descr selinux_files[] = {
		[SEL_LOAD] = {"load", &sel_load_ops, S_IRUSR|S_IWUSR},
		[SEL_ENFORCE] = {"enforce", &sel_enforce_ops, S_IRUGO|S_IWUSR},
		[SEL_CONTEXT] = {"context", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_ACCESS] = {"access", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CREATE] = {"create", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_RELABEL] = {"relabel", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_USER] = {"user", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_POLICYVERS] = {"policyvers", &sel_policyvers_ops, S_IRUGO},
		[SEL_COMMIT_BOOLS] = {"commit_pending_bools", &sel_commit_bools_ops, S_IWUSR},
		[SEL_MLS] = {"mls", &sel_mls_ops, S_IRUGO},
		[SEL_DISABLE] = {"disable", &sel_disable_ops, S_IWUSR},
		[SEL_MEMBER] = {"member", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CHECKREQPROT] = {"checkreqprot", &sel_checkreqprot_ops, S_IRUGO|S_IWUSR},
		[SEL_REJECT_UNKNOWN] = {"reject_unknown", &sel_handle_unknown_ops, S_IRUGO},
		[SEL_DENY_UNKNOWN] = {"deny_unknown", &sel_handle_unknown_ops, S_IRUGO},
		/* last one */ {""}
	};
	ret = simple_fill_super(sb, SELINUX_MAGIC, selinux_files);
	if (ret)
		goto err;

	root_inode = sb->s_root->d_inode;

	dentry = d_alloc_name(sb->s_root, BOOL_DIR_NAME);
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sel_make_dir(root_inode, dentry, &sel_last_ino);
	if (ret)
		goto err;

	bool_dir = dentry;

	dentry = d_alloc_name(sb->s_root, NULL_FILE_NAME);
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	inode = sel_make_inode(sb, S_IFCHR | S_IRUGO | S_IWUGO);
	if (!inode) {
		ret = -ENOMEM;
		goto err;
	}
	inode->i_ino = ++sel_last_ino;
	isec = (struct inode_security_struct *)inode->i_security;
	isec->sid = SECINITSID_DEVNULL;
	isec->sclass = SECCLASS_CHR_FILE;
	isec->initialized = 1;

	init_special_inode(inode, S_IFCHR | S_IRUGO | S_IWUGO, MKDEV(MEM_MAJOR, 3));
	d_add(dentry, inode);
	selinux_null = dentry;

	dentry = d_alloc_name(sb->s_root, "avc");
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sel_make_dir(root_inode, dentry, &sel_last_ino);
	if (ret)
		goto err;

	ret = sel_make_avc_files(dentry);
	if (ret)
		goto err;

	dentry = d_alloc_name(sb->s_root, "initial_contexts");
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sel_make_dir(root_inode, dentry, &sel_last_ino);
	if (ret)
		goto err;

	ret = sel_make_initcon_files(dentry);
	if (ret)
		goto err;

	dentry = d_alloc_name(sb->s_root, "class");
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sel_make_dir(root_inode, dentry, &sel_last_ino);
	if (ret)
		goto err;

	class_dir = dentry;

	dentry = d_alloc_name(sb->s_root, "policy_capabilities");
	if (!dentry) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sel_make_dir(root_inode, dentry, &sel_last_ino);
	if (ret)
		goto err;

	policycap_dir = dentry;

out:
	return ret;
err:
	printk(KERN_ERR "SELinux: %s:  failed while creating inodes\n",
		__func__);
	goto out;
}

static int sel_get_sb(struct file_system_type *fs_type,
		      int flags, const char *dev_name, void *data,
		      struct vfsmount *mnt)
{
	return get_sb_single(fs_type, flags, data, sel_fill_super, mnt);
}

static struct file_system_type sel_fs_type = {
	.name		= "selinuxfs",
	.get_sb		= sel_get_sb,
	.kill_sb	= kill_litter_super,
};

struct vfsmount *selinuxfs_mount;

static int __init init_sel_fs(void)
{
	int err;

	if (!selinux_enabled)
		return 0;
	err = register_filesystem(&sel_fs_type);
	if (!err) {
		selinuxfs_mount = kern_mount(&sel_fs_type);
		if (IS_ERR(selinuxfs_mount)) {
			printk(KERN_ERR "selinuxfs:  could not mount!\n");
			err = PTR_ERR(selinuxfs_mount);
			selinuxfs_mount = NULL;
		}
	}
	return err;
}

__initcall(init_sel_fs);

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
void exit_sel_fs(void)
{
	unregister_filesystem(&sel_fs_type);
}
#endif
