/*
 * linux/kernel/capability.c
 *
 * Copyright (C) 1997  Andrew Main <zefram@fysh.org>
 *
 * Integrated into 2.1.97+,  Andrew G. Morgan <morgan@kernel.org>
 * 30 May 2002:	Cleanup, Robert M. Love <rml@tech9.net>
 */

#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/pid_namespace.h>
#include <asm/uaccess.h>

/*
 * Leveraged for setting/resetting capabilities
 */

const kernel_cap_t __cap_empty_set = CAP_EMPTY_SET;
const kernel_cap_t __cap_full_set = CAP_FULL_SET;
const kernel_cap_t __cap_init_eff_set = CAP_INIT_EFF_SET;

EXPORT_SYMBOL(__cap_empty_set);
EXPORT_SYMBOL(__cap_full_set);
EXPORT_SYMBOL(__cap_init_eff_set);

int file_caps_enabled = 1;

static int __init file_caps_disable(char *str)
{
	file_caps_enabled = 0;
	return 1;
}
__setup("no_file_caps", file_caps_disable);

/*
 * More recent versions of libcap are available from:
 *
 *   http://www.kernel.org/pub/linux/libs/security/linux-privs/
 */

static void warn_legacy_capability_use(void)
{
	static int warned;
	if (!warned) {
		char name[sizeof(current->comm)];

		printk(KERN_INFO "warning: `%s' uses 32-bit capabilities"
		       " (legacy support in use)\n",
		       get_task_comm(name, current));
		warned = 1;
	}
}

/*
 * Version 2 capabilities worked fine, but the linux/capability.h file
 * that accompanied their introduction encouraged their use without
 * the necessary user-space source code changes. As such, we have
 * created a version 3 with equivalent functionality to version 2, but
 * with a header change to protect legacy source code from using
 * version 2 when it wanted to use version 1. If your system has code
 * that trips the following warning, it is using version 2 specific
 * capabilities and may be doing so insecurely.
 *
 * The remedy is to either upgrade your version of libcap (to 2.10+,
 * if the application is linked against it), or recompile your
 * application with modern kernel headers and this warning will go
 * away.
 */

static void warn_deprecated_v2(void)
{
	static int warned;

	if (!warned) {
		char name[sizeof(current->comm)];

		printk(KERN_INFO "warning: `%s' uses deprecated v2"
		       " capabilities in a way that may be insecure.\n",
		       get_task_comm(name, current));
		warned = 1;
	}
}

/*
 * Version check. Return the number of u32s in each capability flag
 * array, or a negative value on error.
 */
static int cap_validate_magic(cap_user_header_t header, unsigned *tocopy)
{
	__u32 version;

	if (get_user(version, &header->version))
		return -EFAULT;

	switch (version) {
	case _LINUX_CAPABILITY_VERSION_1:
		warn_legacy_capability_use();
		*tocopy = _LINUX_CAPABILITY_U32S_1;
		break;
	case _LINUX_CAPABILITY_VERSION_2:
		warn_deprecated_v2();
		/*
		 * fall through - v3 is otherwise equivalent to v2.
		 */
	case _LINUX_CAPABILITY_VERSION_3:
		*tocopy = _LINUX_CAPABILITY_U32S_3;
		break;
	default:
		if (put_user((u32)_KERNEL_CAPABILITY_VERSION, &header->version))
			return -EFAULT;
		return -EINVAL;
	}

	return 0;
}

/*
 * The only thing that can change the capabilities of the current
 * process is the current process. As such, we can't be in this code
 * at the same time as we are in the process of setting capabilities
 * in this process. The net result is that we can limit our use of
 * locks to when we are reading the caps of another process.
 */
static inline int cap_get_target_pid(pid_t pid, kernel_cap_t *pEp,
				     kernel_cap_t *pIp, kernel_cap_t *pPp)
{
	int ret;

	if (pid && (pid != task_pid_vnr(current))) {
		struct task_struct *target;

		rcu_read_lock();

		target = find_task_by_vpid(pid);
		if (!target)
			ret = -ESRCH;
		else
			ret = security_capget(target, pEp, pIp, pPp);

		rcu_read_unlock();
	} else
		ret = security_capget(current, pEp, pIp, pPp);

	return ret;
}

/**
 * sys_capget - get the capabilities of a given process.
 * @header: pointer to struct that contains capability version and
 *	target pid data
 * @dataptr: pointer to struct that contains the effective, permitted,
 *	and inheritable capabilities that are returned
 *
 * Returns 0 on success and < 0 on error.
 */
SYSCALL_DEFINE2(capget, cap_user_header_t, header, cap_user_data_t, dataptr)
{
	int ret = 0;
	pid_t pid;
	unsigned tocopy;
	kernel_cap_t pE, pI, pP;

	ret = cap_validate_magic(header, &tocopy);
	if ((dataptr == NULL) || (ret != 0))
		return ((dataptr == NULL) && (ret == -EINVAL)) ? 0 : ret;

	if (get_user(pid, &header->pid))
		return -EFAULT;

	if (pid < 0)
		return -EINVAL;

	ret = cap_get_target_pid(pid, &pE, &pI, &pP);
	if (!ret) {
		struct __user_cap_data_struct kdata[_KERNEL_CAPABILITY_U32S];
		unsigned i;

		for (i = 0; i < tocopy; i++) {
			kdata[i].effective = pE.cap[i];
			kdata[i].permitted = pP.cap[i];
			kdata[i].inheritable = pI.cap[i];
		}

		/*
		 * Note, in the case, tocopy < _KERNEL_CAPABILITY_U32S,
		 * we silently drop the upper capabilities here. This
		 * has the effect of making older libcap
		 * implementations implicitly drop upper capability
		 * bits when they perform a: capget/modify/capset
		 * sequence.
		 *
		 * This behavior is considered fail-safe
		 * behavior. Upgrading the application to a newer
		 * version of libcap will enable access to the newer
		 * capabilities.
		 *
		 * An alternative would be to return an error here
		 * (-ERANGE), but that causes legacy applications to
		 * unexpectidly fail; the capget/modify/capset aborts
		 * before modification is attempted and the application
		 * fails.
		 */
		if (copy_to_user(dataptr, kdata, tocopy
				 * sizeof(struct __user_cap_data_struct))) {
			return -EFAULT;
		}
	}

	return ret;
}

/**
 * sys_capset - set capabilities for a process or (*) a group of processes
 * @header: pointer to struct that contains capability version and
 *	target pid data
 * @data: pointer to struct that contains the effective, permitted,
 *	and inheritable capabilities
 *
 * Set capabilities for the current process only.  The ability to any other
 * process(es) has been deprecated and removed.
 *
 * The restrictions on setting capabilities are specified as:
 *
 * I: any raised capabilities must be a subset of the old permitted
 * P: any raised capabilities must be a subset of the old permitted
 * E: must be set to a subset of new permitted
 *
 * Returns 0 on success and < 0 on error.
 */
SYSCALL_DEFINE2(capset, cap_user_header_t, header, const cap_user_data_t, data)
{
	struct __user_cap_data_struct kdata[_KERNEL_CAPABILITY_U32S];
	unsigned i, tocopy, copybytes;
	kernel_cap_t inheritable, permitted, effective;
	struct cred *new;
	int ret;
	pid_t pid;

	ret = cap_validate_magic(header, &tocopy);
	if (ret != 0)
		return ret;

	if (get_user(pid, &header->pid))
		return -EFAULT;

	/* may only affect current now */
	if (pid != 0 && pid != task_pid_vnr(current))
		return -EPERM;

	copybytes = tocopy * sizeof(struct __user_cap_data_struct);
	if (copybytes > sizeof(kdata))
		return -EFAULT;

	if (copy_from_user(&kdata, data, copybytes))
		return -EFAULT;

	for (i = 0; i < tocopy; i++) {
		effective.cap[i] = kdata[i].effective;
		permitted.cap[i] = kdata[i].permitted;
		inheritable.cap[i] = kdata[i].inheritable;
	}
	while (i < _KERNEL_CAPABILITY_U32S) {
		effective.cap[i] = 0;
		permitted.cap[i] = 0;
		inheritable.cap[i] = 0;
		i++;
	}

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	ret = security_capset(new, current_cred(),
			      &effective, &inheritable, &permitted);
	if (ret < 0)
		goto error;

	audit_log_capset(pid, new, current_cred());

	return commit_creds(new);

error:
	abort_creds(new);
	return ret;
}

/**
 * capable - Determine if the current task has a superior capability in effect
 * @cap: The capability to be tested for
 *
 * Return true if the current task has the given superior capability currently
 * available for use, false if not.
 *
 * This sets PF_SUPERPRIV on the task if the capability is available on the
 * assumption that it's about to be used.
 */
int capable(int cap)
{
	if (unlikely(!cap_valid(cap))) {
		printk(KERN_CRIT "capable() called with invalid cap=%u\n", cap);
		BUG();
	}

	if (security_capable(cap) == 0) {
		current->flags |= PF_SUPERPRIV;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(capable);
