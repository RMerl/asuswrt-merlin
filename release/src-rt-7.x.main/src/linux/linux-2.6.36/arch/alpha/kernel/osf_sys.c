/*
 *  linux/arch/alpha/kernel/osf_sys.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */

/*
 * This file handles some of the stranger OSF/1 system call interfaces.
 * Some of the system calls expect a non-C calling standard, others have
 * special parameter blocks..
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/utsname.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/shm.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/ipc.h>
#include <linux/namei.h>
#include <linux/uio.h>
#include <linux/vfs.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

#include <asm/fpu.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/sysinfo.h>
#include <asm/hwrpb.h>
#include <asm/processor.h>

/*
 * Brk needs to return an error.  Still support Linux's brk(0) query idiom,
 * which OSF programs just shouldn't be doing.  We're still not quite
 * identical to OSF as we don't return 0 on success, but doing otherwise
 * would require changes to libc.  Hopefully this is good enough.
 */
SYSCALL_DEFINE1(osf_brk, unsigned long, brk)
{
	unsigned long retval = sys_brk(brk);
	if (brk && brk != retval)
		retval = -ENOMEM;
	return retval;
}
 
/*
 * This is pure guess-work..
 */
SYSCALL_DEFINE4(osf_set_program_attributes, unsigned long, text_start,
		unsigned long, text_len, unsigned long, bss_start,
		unsigned long, bss_len)
{
	struct mm_struct *mm;

	mm = current->mm;
	mm->end_code = bss_start + bss_len;
	mm->start_brk = bss_start + bss_len;
	mm->brk = bss_start + bss_len;
	return 0;
}

/*
 * OSF/1 directory handling functions...
 *
 * The "getdents()" interface is much more sane: the "basep" stuff is
 * braindamage (it can't really handle filesystems where the directory
 * offset differences aren't the same as "d_reclen").
 */
#define NAME_OFFSET	offsetof (struct osf_dirent, d_name)

struct osf_dirent {
	unsigned int d_ino;
	unsigned short d_reclen;
	unsigned short d_namlen;
	char d_name[1];
};

struct osf_dirent_callback {
	struct osf_dirent __user *dirent;
	long __user *basep;
	unsigned int count;
	int error;
};

static int
osf_filldir(void *__buf, const char *name, int namlen, loff_t offset,
	    u64 ino, unsigned int d_type)
{
	struct osf_dirent __user *dirent;
	struct osf_dirent_callback *buf = (struct osf_dirent_callback *) __buf;
	unsigned int reclen = ALIGN(NAME_OFFSET + namlen + 1, sizeof(u32));
	unsigned int d_ino;

	buf->error = -EINVAL;	/* only used if we fail */
	if (reclen > buf->count)
		return -EINVAL;
	d_ino = ino;
	if (sizeof(d_ino) < sizeof(ino) && d_ino != ino) {
		buf->error = -EOVERFLOW;
		return -EOVERFLOW;
	}
	if (buf->basep) {
		if (put_user(offset, buf->basep))
			goto Efault;
		buf->basep = NULL;
	}
	dirent = buf->dirent;
	if (put_user(d_ino, &dirent->d_ino) ||
	    put_user(namlen, &dirent->d_namlen) ||
	    put_user(reclen, &dirent->d_reclen) ||
	    copy_to_user(dirent->d_name, name, namlen) ||
	    put_user(0, dirent->d_name + namlen))
		goto Efault;
	dirent = (void __user *)dirent + reclen;
	buf->dirent = dirent;
	buf->count -= reclen;
	return 0;
Efault:
	buf->error = -EFAULT;
	return -EFAULT;
}

SYSCALL_DEFINE4(osf_getdirentries, unsigned int, fd,
		struct osf_dirent __user *, dirent, unsigned int, count,
		long __user *, basep)
{
	int error;
	struct file *file;
	struct osf_dirent_callback buf;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.dirent = dirent;
	buf.basep = basep;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, osf_filldir, &buf);
	if (error >= 0)
		error = buf.error;
	if (count != buf.count)
		error = count - buf.count;

	fput(file);
 out:
	return error;
}

#undef NAME_OFFSET

SYSCALL_DEFINE6(osf_mmap, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags, unsigned long, fd,
		unsigned long, off)
{
	unsigned long ret = -EINVAL;

	if ((off + PAGE_ALIGN(len)) < off)
		goto out;
	if (off & ~PAGE_MASK)
		goto out;
	ret = sys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
 out:
	return ret;
}


/*
 * The OSF/1 statfs structure is much larger, but this should
 * match the beginning, at least.
 */
struct osf_statfs {
	short f_type;
	short f_flags;
	int f_fsize;
	int f_bsize;
	int f_blocks;
	int f_bfree;
	int f_bavail;
	int f_files;
	int f_ffree;
	__kernel_fsid_t f_fsid;
};

static int
linux_to_osf_statfs(struct kstatfs *linux_stat, struct osf_statfs __user *osf_stat,
		    unsigned long bufsiz)
{
	struct osf_statfs tmp_stat;

	tmp_stat.f_type = linux_stat->f_type;
	tmp_stat.f_flags = 0;	/* mount flags */
	tmp_stat.f_fsize = linux_stat->f_frsize;
	tmp_stat.f_bsize = linux_stat->f_bsize;
	tmp_stat.f_blocks = linux_stat->f_blocks;
	tmp_stat.f_bfree = linux_stat->f_bfree;
	tmp_stat.f_bavail = linux_stat->f_bavail;
	tmp_stat.f_files = linux_stat->f_files;
	tmp_stat.f_ffree = linux_stat->f_ffree;
	tmp_stat.f_fsid = linux_stat->f_fsid;
	if (bufsiz > sizeof(tmp_stat))
		bufsiz = sizeof(tmp_stat);
	return copy_to_user(osf_stat, &tmp_stat, bufsiz) ? -EFAULT : 0;
}

static int
do_osf_statfs(struct path *path, struct osf_statfs __user *buffer,
	      unsigned long bufsiz)
{
	struct kstatfs linux_stat;
	int error = vfs_statfs(path, &linux_stat);
	if (!error)
		error = linux_to_osf_statfs(&linux_stat, buffer, bufsiz);
	return error;	
}

SYSCALL_DEFINE3(osf_statfs, const char __user *, pathname,
		struct osf_statfs __user *, buffer, unsigned long, bufsiz)
{
	struct path path;
	int retval;

	retval = user_path(pathname, &path);
	if (!retval) {
		retval = do_osf_statfs(&path, buffer, bufsiz);
		path_put(&path);
	}
	return retval;
}

SYSCALL_DEFINE3(osf_fstatfs, unsigned long, fd,
		struct osf_statfs __user *, buffer, unsigned long, bufsiz)
{
	struct file *file;
	int retval;

	retval = -EBADF;
	file = fget(fd);
	if (file) {
		retval = do_osf_statfs(&file->f_path, buffer, bufsiz);
		fput(file);
	}
	return retval;
}

/*
 * Uhh.. OSF/1 mount parameters aren't exactly obvious..
 *
 * Although to be frank, neither are the native Linux/i386 ones..
 */
struct ufs_args {
	char __user *devname;
	int flags;
	uid_t exroot;
};

struct cdfs_args {
	char __user *devname;
	int flags;
	uid_t exroot;

	/* This has lots more here, which Linux handles with the option block
	   but I'm too lazy to do the translation into ASCII.  */
};

struct procfs_args {
	char __user *devname;
	int flags;
	uid_t exroot;
};

/*
 * We can't actually handle ufs yet, so we translate UFS mounts to
 * ext2fs mounts. I wouldn't mind a UFS filesystem, but the UFS
 * layout is so braindead it's a major headache doing it.
 *
 * Just how long ago was it written? OTOH our UFS driver may be still
 * unhappy with OSF UFS. [CHECKME]
 */
static int
osf_ufs_mount(char *dirname, struct ufs_args __user *args, int flags)
{
	int retval;
	struct cdfs_args tmp;
	char *devname;

	retval = -EFAULT;
	if (copy_from_user(&tmp, args, sizeof(tmp)))
		goto out;
	devname = getname(tmp.devname);
	retval = PTR_ERR(devname);
	if (IS_ERR(devname))
		goto out;
	retval = do_mount(devname, dirname, "ext2", flags, NULL);
	putname(devname);
 out:
	return retval;
}

static int
osf_cdfs_mount(char *dirname, struct cdfs_args __user *args, int flags)
{
	int retval;
	struct cdfs_args tmp;
	char *devname;

	retval = -EFAULT;
	if (copy_from_user(&tmp, args, sizeof(tmp)))
		goto out;
	devname = getname(tmp.devname);
	retval = PTR_ERR(devname);
	if (IS_ERR(devname))
		goto out;
	retval = do_mount(devname, dirname, "iso9660", flags, NULL);
	putname(devname);
 out:
	return retval;
}

static int
osf_procfs_mount(char *dirname, struct procfs_args __user *args, int flags)
{
	struct procfs_args tmp;

	if (copy_from_user(&tmp, args, sizeof(tmp)))
		return -EFAULT;

	return do_mount("", dirname, "proc", flags, NULL);
}

SYSCALL_DEFINE4(osf_mount, unsigned long, typenr, const char __user *, path,
		int, flag, void __user *, data)
{
	int retval;
	char *name;

	name = getname(path);
	retval = PTR_ERR(name);
	if (IS_ERR(name))
		goto out;
	switch (typenr) {
	case 1:
		retval = osf_ufs_mount(name, data, flag);
		break;
	case 6:
		retval = osf_cdfs_mount(name, data, flag);
		break;
	case 9:
		retval = osf_procfs_mount(name, data, flag);
		break;
	default:
		retval = -EINVAL;
		printk("osf_mount(%ld, %x)\n", typenr, flag);
	}
	putname(name);
 out:
	return retval;
}

SYSCALL_DEFINE1(osf_utsname, char __user *, name)
{
	int error;

	down_read(&uts_sem);
	error = -EFAULT;
	if (copy_to_user(name + 0, utsname()->sysname, 32))
		goto out;
	if (copy_to_user(name + 32, utsname()->nodename, 32))
		goto out;
	if (copy_to_user(name + 64, utsname()->release, 32))
		goto out;
	if (copy_to_user(name + 96, utsname()->version, 32))
		goto out;
	if (copy_to_user(name + 128, utsname()->machine, 32))
		goto out;

	error = 0;
 out:
	up_read(&uts_sem);	
	return error;
}

SYSCALL_DEFINE0(getpagesize)
{
	return PAGE_SIZE;
}

SYSCALL_DEFINE0(getdtablesize)
{
	return sysctl_nr_open;
}

/*
 * For compatibility with OSF/1 only.  Use utsname(2) instead.
 */
SYSCALL_DEFINE2(osf_getdomainname, char __user *, name, int, namelen)
{
	unsigned len;
	int i;

	if (!access_ok(VERIFY_WRITE, name, namelen))
		return -EFAULT;

	len = namelen;
	if (namelen > 32)
		len = 32;

	down_read(&uts_sem);
	for (i = 0; i < len; ++i) {
		__put_user(utsname()->domainname[i], name + i);
		if (utsname()->domainname[i] == '\0')
			break;
	}
	up_read(&uts_sem);

	return 0;
}

/*
 * The following stuff should move into a header file should it ever
 * be labeled "officially supported."  Right now, there is just enough
 * support to avoid applications (such as tar) printing error
 * messages.  The attributes are not really implemented.
 */

/*
 * Values for Property list entry flag
 */
#define PLE_PROPAGATE_ON_COPY		0x1	/* cp(1) will copy entry
						   by default */
#define PLE_FLAG_MASK			0x1	/* Valid flag values */
#define PLE_FLAG_ALL			-1	/* All flag value */

struct proplistname_args {
	unsigned int pl_mask;
	unsigned int pl_numnames;
	char **pl_names;
};

union pl_args {
	struct setargs {
		char __user *path;
		long follow;
		long nbytes;
		char __user *buf;
	} set;
	struct fsetargs {
		long fd;
		long nbytes;
		char __user *buf;
	} fset;
	struct getargs {
		char __user *path;
		long follow;
		struct proplistname_args __user *name_args;
		long nbytes;
		char __user *buf;
		int __user *min_buf_size;
	} get;
	struct fgetargs {
		long fd;
		struct proplistname_args __user *name_args;
		long nbytes;
		char __user *buf;
		int __user *min_buf_size;
	} fget;
	struct delargs {
		char __user *path;
		long follow;
		struct proplistname_args __user *name_args;
	} del;
	struct fdelargs {
		long fd;
		struct proplistname_args __user *name_args;
	} fdel;
};

enum pl_code {
	PL_SET = 1, PL_FSET = 2,
	PL_GET = 3, PL_FGET = 4,
	PL_DEL = 5, PL_FDEL = 6
};

SYSCALL_DEFINE2(osf_proplist_syscall, enum pl_code, code,
		union pl_args __user *, args)
{
	long error;
	int __user *min_buf_size_ptr;

	switch (code) {
	case PL_SET:
		if (get_user(error, &args->set.nbytes))
			error = -EFAULT;
		break;
	case PL_FSET:
		if (get_user(error, &args->fset.nbytes))
			error = -EFAULT;
		break;
	case PL_GET:
		error = get_user(min_buf_size_ptr, &args->get.min_buf_size);
		if (error)
			break;
		error = put_user(0, min_buf_size_ptr);
		break;
	case PL_FGET:
		error = get_user(min_buf_size_ptr, &args->fget.min_buf_size);
		if (error)
			break;
		error = put_user(0, min_buf_size_ptr);
		break;
	case PL_DEL:
	case PL_FDEL:
		error = 0;
		break;
	default:
		error = -EOPNOTSUPP;
		break;
	};
	return error;
}

SYSCALL_DEFINE2(osf_sigstack, struct sigstack __user *, uss,
		struct sigstack __user *, uoss)
{
	unsigned long usp = rdusp();
	unsigned long oss_sp = current->sas_ss_sp + current->sas_ss_size;
	unsigned long oss_os = on_sig_stack(usp);
	int error;

	if (uss) {
		void __user *ss_sp;

		error = -EFAULT;
		if (get_user(ss_sp, &uss->ss_sp))
			goto out;

		/* If the current stack was set with sigaltstack, don't
		   swap stacks while we are on it.  */
		error = -EPERM;
		if (current->sas_ss_sp && on_sig_stack(usp))
			goto out;

		/* Since we don't know the extent of the stack, and we don't
		   track onstack-ness, but rather calculate it, we must 
		   presume a size.  Ho hum this interface is lossy.  */
		current->sas_ss_sp = (unsigned long)ss_sp - SIGSTKSZ;
		current->sas_ss_size = SIGSTKSZ;
	}

	if (uoss) {
		error = -EFAULT;
		if (! access_ok(VERIFY_WRITE, uoss, sizeof(*uoss))
		    || __put_user(oss_sp, &uoss->ss_sp)
		    || __put_user(oss_os, &uoss->ss_onstack))
			goto out;
	}

	error = 0;
 out:
	return error;
}

SYSCALL_DEFINE3(osf_sysinfo, int, command, char __user *, buf, long, count)
{
	const char *sysinfo_table[] = {
		utsname()->sysname,
		utsname()->nodename,
		utsname()->release,
		utsname()->version,
		utsname()->machine,
		"alpha",	/* instruction set architecture */
		"dummy",	/* hardware serial number */
		"dummy",	/* hardware manufacturer */
		"dummy",	/* secure RPC domain */
	};
	unsigned long offset;
	const char *res;
	long len, err = -EINVAL;

	offset = command-1;
	if (offset >= ARRAY_SIZE(sysinfo_table)) {
		/* Digital UNIX has a few unpublished interfaces here */
		printk("sysinfo(%d)", command);
		goto out;
	}

	down_read(&uts_sem);
	res = sysinfo_table[offset];
	len = strlen(res)+1;
	if (len > count)
		len = count;
	if (copy_to_user(buf, res, len))
		err = -EFAULT;
	else
		err = 0;
	up_read(&uts_sem);
 out:
	return err;
}

SYSCALL_DEFINE5(osf_getsysinfo, unsigned long, op, void __user *, buffer,
		unsigned long, nbytes, int __user *, start, void __user *, arg)
{
	unsigned long w;
	struct percpu_struct *cpu;

	switch (op) {
	case GSI_IEEE_FP_CONTROL:
		/* Return current software fp control & status bits.  */
		/* Note that DU doesn't verify available space here.  */

 		w = current_thread_info()->ieee_state & IEEE_SW_MASK;
 		w = swcr_update_status(w, rdfpcr());
		if (put_user(w, (unsigned long __user *) buffer))
			return -EFAULT;
		return 0;

	case GSI_IEEE_STATE_AT_SIGNAL:
		/*
		 * Not sure anybody will ever use this weird stuff.  These
		 * ops can be used (under OSF/1) to set the fpcr that should
		 * be used when a signal handler starts executing.
		 */
		break;

 	case GSI_UACPROC:
		if (nbytes < sizeof(unsigned int))
			return -EINVAL;
 		w = (current_thread_info()->flags >> UAC_SHIFT) & UAC_BITMASK;
 		if (put_user(w, (unsigned int __user *)buffer))
 			return -EFAULT;
 		return 1;

	case GSI_PROC_TYPE:
		if (nbytes < sizeof(unsigned long))
			return -EINVAL;
		cpu = (struct percpu_struct*)
		  ((char*)hwrpb + hwrpb->processor_offset);
		w = cpu->type;
		if (put_user(w, (unsigned long  __user*)buffer))
			return -EFAULT;
		return 1;

	case GSI_GET_HWRPB:
		if (nbytes < sizeof(*hwrpb))
			return -EINVAL;
		if (copy_to_user(buffer, hwrpb, nbytes) != 0)
			return -EFAULT;
		return 1;

	default:
		break;
	}

	return -EOPNOTSUPP;
}

SYSCALL_DEFINE5(osf_setsysinfo, unsigned long, op, void __user *, buffer,
		unsigned long, nbytes, int __user *, start, void __user *, arg)
{
	switch (op) {
	case SSI_IEEE_FP_CONTROL: {
		unsigned long swcr, fpcr;
		unsigned int *state;

		/* 
		 * Alpha Architecture Handbook 4.7.7.3:
		 * To be fully IEEE compiant, we must track the current IEEE
		 * exception state in software, because spurious bits can be
		 * set in the trap shadow of a software-complete insn.
		 */

		if (get_user(swcr, (unsigned long __user *)buffer))
			return -EFAULT;
		state = &current_thread_info()->ieee_state;

		/* Update softare trap enable bits.  */
		*state = (*state & ~IEEE_SW_MASK) | (swcr & IEEE_SW_MASK);

		/* Update the real fpcr.  */
		fpcr = rdfpcr() & FPCR_DYN_MASK;
		fpcr |= ieee_swcr_to_fpcr(swcr);
		wrfpcr(fpcr);

		return 0;
	}

	case SSI_IEEE_RAISE_EXCEPTION: {
		unsigned long exc, swcr, fpcr, fex;
		unsigned int *state;

		if (get_user(exc, (unsigned long __user *)buffer))
			return -EFAULT;
		state = &current_thread_info()->ieee_state;
		exc &= IEEE_STATUS_MASK;

		/* Update softare trap enable bits.  */
 		swcr = (*state & IEEE_SW_MASK) | exc;
		*state |= exc;

		/* Update the real fpcr.  */
		fpcr = rdfpcr();
		fpcr |= ieee_swcr_to_fpcr(swcr);
		wrfpcr(fpcr);

 		/* If any exceptions set by this call, and are unmasked,
		   send a signal.  Old exceptions are not signaled.  */
		fex = (exc >> IEEE_STATUS_TO_EXCSUM_SHIFT) & swcr;
 		if (fex) {
			siginfo_t info;
			int si_code = 0;

			if (fex & IEEE_TRAP_ENABLE_DNO) si_code = FPE_FLTUND;
			if (fex & IEEE_TRAP_ENABLE_INE) si_code = FPE_FLTRES;
			if (fex & IEEE_TRAP_ENABLE_UNF) si_code = FPE_FLTUND;
			if (fex & IEEE_TRAP_ENABLE_OVF) si_code = FPE_FLTOVF;
			if (fex & IEEE_TRAP_ENABLE_DZE) si_code = FPE_FLTDIV;
			if (fex & IEEE_TRAP_ENABLE_INV) si_code = FPE_FLTINV;

			info.si_signo = SIGFPE;
			info.si_errno = 0;
			info.si_code = si_code;
			info.si_addr = NULL;
 			send_sig_info(SIGFPE, &info, current);
 		}
		return 0;
	}

	case SSI_IEEE_STATE_AT_SIGNAL:
	case SSI_IEEE_IGNORE_STATE_AT_SIGNAL:
		/*
		 * Not sure anybody will ever use this weird stuff.  These
		 * ops can be used (under OSF/1) to set the fpcr that should
		 * be used when a signal handler starts executing.
		 */
		break;

 	case SSI_NVPAIRS: {
		unsigned long v, w, i;
		unsigned int old, new;
		
 		for (i = 0; i < nbytes; ++i) {

 			if (get_user(v, 2*i + (unsigned int __user *)buffer))
 				return -EFAULT;
 			if (get_user(w, 2*i + 1 + (unsigned int __user *)buffer))
 				return -EFAULT;
 			switch (v) {
 			case SSIN_UACPROC:
			again:
				old = current_thread_info()->flags;
				new = old & ~(UAC_BITMASK << UAC_SHIFT);
				new = new | (w & UAC_BITMASK) << UAC_SHIFT;
				if (cmpxchg(&current_thread_info()->flags,
					    old, new) != old)
					goto again;
 				break;
 
 			default:
 				return -EOPNOTSUPP;
 			}
 		}
 		return 0;
	}
 
	default:
		break;
	}

	return -EOPNOTSUPP;
}

/* Translations due to the fact that OSF's time_t is an int.  Which
   affects all sorts of things, like timeval and itimerval.  */

extern struct timezone sys_tz;

struct timeval32
{
    int tv_sec, tv_usec;
};

struct itimerval32
{
    struct timeval32 it_interval;
    struct timeval32 it_value;
};

static inline long
get_tv32(struct timeval *o, struct timeval32 __user *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->tv_sec, &i->tv_sec) |
		 __get_user(o->tv_usec, &i->tv_usec)));
}

static inline long
put_tv32(struct timeval32 __user *o, struct timeval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->tv_sec, &o->tv_sec) |
		 __put_user(i->tv_usec, &o->tv_usec)));
}

static inline long
get_it32(struct itimerval *o, struct itimerval32 __user *i)
{
	return (!access_ok(VERIFY_READ, i, sizeof(*i)) ||
		(__get_user(o->it_interval.tv_sec, &i->it_interval.tv_sec) |
		 __get_user(o->it_interval.tv_usec, &i->it_interval.tv_usec) |
		 __get_user(o->it_value.tv_sec, &i->it_value.tv_sec) |
		 __get_user(o->it_value.tv_usec, &i->it_value.tv_usec)));
}

static inline long
put_it32(struct itimerval32 __user *o, struct itimerval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->it_interval.tv_sec, &o->it_interval.tv_sec) |
		 __put_user(i->it_interval.tv_usec, &o->it_interval.tv_usec) |
		 __put_user(i->it_value.tv_sec, &o->it_value.tv_sec) |
		 __put_user(i->it_value.tv_usec, &o->it_value.tv_usec)));
}

static inline void
jiffies_to_timeval32(unsigned long jiffies, struct timeval32 *value)
{
	value->tv_usec = (jiffies % HZ) * (1000000L / HZ);
	value->tv_sec = jiffies / HZ;
}

SYSCALL_DEFINE2(osf_gettimeofday, struct timeval32 __user *, tv,
		struct timezone __user *, tz)
{
	if (tv) {
		struct timeval ktv;
		do_gettimeofday(&ktv);
		if (put_tv32(tv, &ktv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
			return -EFAULT;
	}
	return 0;
}

SYSCALL_DEFINE2(osf_settimeofday, struct timeval32 __user *, tv,
		struct timezone __user *, tz)
{
	struct timespec kts;
	struct timezone ktz;

 	if (tv) {
		if (get_tv32((struct timeval *)&kts, tv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_from_user(&ktz, tz, sizeof(*tz)))
			return -EFAULT;
	}

	kts.tv_nsec *= 1000;

	return do_sys_settimeofday(tv ? &kts : NULL, tz ? &ktz : NULL);
}

SYSCALL_DEFINE2(osf_getitimer, int, which, struct itimerval32 __user *, it)
{
	struct itimerval kit;
	int error;

	error = do_getitimer(which, &kit);
	if (!error && put_it32(it, &kit))
		error = -EFAULT;

	return error;
}

SYSCALL_DEFINE3(osf_setitimer, int, which, struct itimerval32 __user *, in,
		struct itimerval32 __user *, out)
{
	struct itimerval kin, kout;
	int error;

	if (in) {
		if (get_it32(&kin, in))
			return -EFAULT;
	} else
		memset(&kin, 0, sizeof(kin));

	error = do_setitimer(which, &kin, out ? &kout : NULL);
	if (error || !out)
		return error;

	if (put_it32(out, &kout))
		return -EFAULT;

	return 0;

}

SYSCALL_DEFINE2(osf_utimes, const char __user *, filename,
		struct timeval32 __user *, tvs)
{
	struct timespec tv[2];

	if (tvs) {
		struct timeval ktvs[2];
		if (get_tv32(&ktvs[0], &tvs[0]) ||
		    get_tv32(&ktvs[1], &tvs[1]))
			return -EFAULT;

		if (ktvs[0].tv_usec < 0 || ktvs[0].tv_usec >= 1000000 ||
		    ktvs[1].tv_usec < 0 || ktvs[1].tv_usec >= 1000000)
			return -EINVAL;

		tv[0].tv_sec = ktvs[0].tv_sec;
		tv[0].tv_nsec = 1000 * ktvs[0].tv_usec;
		tv[1].tv_sec = ktvs[1].tv_sec;
		tv[1].tv_nsec = 1000 * ktvs[1].tv_usec;
	}

	return do_utimes(AT_FDCWD, filename, tvs ? tv : NULL, 0);
}

#define MAX_SELECT_SECONDS \
	((unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)

SYSCALL_DEFINE5(osf_select, int, n, fd_set __user *, inp, fd_set __user *, outp,
		fd_set __user *, exp, struct timeval32 __user *, tvp)
{
	struct timespec end_time, *to = NULL;
	if (tvp) {
		time_t sec, usec;

		to = &end_time;

		if (!access_ok(VERIFY_READ, tvp, sizeof(*tvp))
		    || __get_user(sec, &tvp->tv_sec)
		    || __get_user(usec, &tvp->tv_usec)) {
		    	return -EFAULT;
		}

		if (sec < 0 || usec < 0)
			return -EINVAL;

		if (poll_select_set_timeout(to, sec, usec * NSEC_PER_USEC))
			return -EINVAL;		

	}

	/* OSF does not copy back the remaining time.  */
	return core_sys_select(n, inp, outp, exp, to);
}

struct rusage32 {
	struct timeval32 ru_utime;	/* user time used */
	struct timeval32 ru_stime;	/* system time used */
	long	ru_maxrss;		/* maximum resident set size */
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data size */
	long	ru_isrss;		/* integral unshared stack size */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
};

SYSCALL_DEFINE2(osf_getrusage, int, who, struct rusage32 __user *, ru)
{
	struct rusage32 r;

	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;

	memset(&r, 0, sizeof(r));
	switch (who) {
	case RUSAGE_SELF:
		jiffies_to_timeval32(current->utime, &r.ru_utime);
		jiffies_to_timeval32(current->stime, &r.ru_stime);
		r.ru_minflt = current->min_flt;
		r.ru_majflt = current->maj_flt;
		break;
	case RUSAGE_CHILDREN:
		jiffies_to_timeval32(current->signal->cutime, &r.ru_utime);
		jiffies_to_timeval32(current->signal->cstime, &r.ru_stime);
		r.ru_minflt = current->signal->cmin_flt;
		r.ru_majflt = current->signal->cmaj_flt;
		break;
	}

	return copy_to_user(ru, &r, sizeof(r)) ? -EFAULT : 0;
}

SYSCALL_DEFINE4(osf_wait4, pid_t, pid, int __user *, ustatus, int, options,
		struct rusage32 __user *, ur)
{
	struct rusage r;
	long ret, err;
	mm_segment_t old_fs;

	if (!ur)
		return sys_wait4(pid, ustatus, options, NULL);

	old_fs = get_fs();
		
	set_fs (KERNEL_DS);
	ret = sys_wait4(pid, ustatus, options, (struct rusage __user *) &r);
	set_fs (old_fs);

	if (!access_ok(VERIFY_WRITE, ur, sizeof(*ur)))
		return -EFAULT;

	err = 0;
	err |= __put_user(r.ru_utime.tv_sec, &ur->ru_utime.tv_sec);
	err |= __put_user(r.ru_utime.tv_usec, &ur->ru_utime.tv_usec);
	err |= __put_user(r.ru_stime.tv_sec, &ur->ru_stime.tv_sec);
	err |= __put_user(r.ru_stime.tv_usec, &ur->ru_stime.tv_usec);
	err |= __put_user(r.ru_maxrss, &ur->ru_maxrss);
	err |= __put_user(r.ru_ixrss, &ur->ru_ixrss);
	err |= __put_user(r.ru_idrss, &ur->ru_idrss);
	err |= __put_user(r.ru_isrss, &ur->ru_isrss);
	err |= __put_user(r.ru_minflt, &ur->ru_minflt);
	err |= __put_user(r.ru_majflt, &ur->ru_majflt);
	err |= __put_user(r.ru_nswap, &ur->ru_nswap);
	err |= __put_user(r.ru_inblock, &ur->ru_inblock);
	err |= __put_user(r.ru_oublock, &ur->ru_oublock);
	err |= __put_user(r.ru_msgsnd, &ur->ru_msgsnd);
	err |= __put_user(r.ru_msgrcv, &ur->ru_msgrcv);
	err |= __put_user(r.ru_nsignals, &ur->ru_nsignals);
	err |= __put_user(r.ru_nvcsw, &ur->ru_nvcsw);
	err |= __put_user(r.ru_nivcsw, &ur->ru_nivcsw);

	return err ? err : ret;
}

/*
 * I don't know what the parameters are: the first one
 * seems to be a timeval pointer, and I suspect the second
 * one is the time remaining.. Ho humm.. No documentation.
 */
SYSCALL_DEFINE2(osf_usleep_thread, struct timeval32 __user *, sleep,
		struct timeval32 __user *, remain)
{
	struct timeval tmp;
	unsigned long ticks;

	if (get_tv32(&tmp, sleep))
		goto fault;

	ticks = timeval_to_jiffies(&tmp);

	ticks = schedule_timeout_interruptible(ticks);

	if (remain) {
		jiffies_to_timeval(ticks, &tmp);
		if (put_tv32(remain, &tmp))
			goto fault;
	}
	
	return 0;
 fault:
	return -EFAULT;
}


struct timex32 {
	unsigned int modes;	/* mode selector */
	long offset;		/* time offset (usec) */
	long freq;		/* frequency offset (scaled ppm) */
	long maxerror;		/* maximum error (usec) */
	long esterror;		/* estimated error (usec) */
	int status;		/* clock command/status */
	long constant;		/* pll time constant */
	long precision;		/* clock precision (usec) (read only) */
	long tolerance;		/* clock frequency tolerance (ppm)
				 * (read only)
				 */
	struct timeval32 time;	/* (read only) */
	long tick;		/* (modified) usecs between clock ticks */

	long ppsfreq;           /* pps frequency (scaled ppm) (ro) */
	long jitter;            /* pps jitter (us) (ro) */
	int shift;              /* interval duration (s) (shift) (ro) */
	long stabil;            /* pps stability (scaled ppm) (ro) */
	long jitcnt;            /* jitter limit exceeded (ro) */
	long calcnt;            /* calibration intervals (ro) */
	long errcnt;            /* calibration errors (ro) */
	long stbcnt;            /* stability limit exceeded (ro) */

	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
};

SYSCALL_DEFINE1(old_adjtimex, struct timex32 __user *, txc_p)
{
        struct timex txc;
	int ret;

	/* copy relevant bits of struct timex. */
	if (copy_from_user(&txc, txc_p, offsetof(struct timex32, time)) ||
	    copy_from_user(&txc.tick, &txc_p->tick, sizeof(struct timex32) - 
			   offsetof(struct timex32, time)))
	  return -EFAULT;

	ret = do_adjtimex(&txc);	
	if (ret < 0)
	  return ret;
	
	/* copy back to timex32 */
	if (copy_to_user(txc_p, &txc, offsetof(struct timex32, time)) ||
	    (copy_to_user(&txc_p->tick, &txc.tick, sizeof(struct timex32) - 
			  offsetof(struct timex32, tick))) ||
	    (put_tv32(&txc_p->time, &txc.time)))
	  return -EFAULT;

	return ret;
}

/* Get an address range which is currently unmapped.  Similar to the
   generic version except that we know how to honor ADDR_LIMIT_32BIT.  */

static unsigned long
arch_get_unmapped_area_1(unsigned long addr, unsigned long len,
		         unsigned long limit)
{
	struct vm_area_struct *vma = find_vma(current->mm, addr);

	while (1) {
		/* At this point:  (!vma || addr < vma->vm_end). */
		if (limit - len < addr)
			return -ENOMEM;
		if (!vma || addr + len <= vma->vm_start)
			return addr;
		addr = vma->vm_end;
		vma = vma->vm_next;
	}
}

unsigned long
arch_get_unmapped_area(struct file *filp, unsigned long addr,
		       unsigned long len, unsigned long pgoff,
		       unsigned long flags)
{
	unsigned long limit;

	/* "32 bit" actually means 31 bit, since pointers sign extend.  */
	if (current->personality & ADDR_LIMIT_32BIT)
		limit = 0x80000000;
	else
		limit = TASK_SIZE;

	if (len > limit)
		return -ENOMEM;

	if (flags & MAP_FIXED)
		return addr;

	/* First, see if the given suggestion fits.

	   The OSF/1 loader (/sbin/loader) relies on us returning an
	   address larger than the requested if one exists, which is
	   a terribly broken way to program.

	   That said, I can see the use in being able to suggest not
	   merely specific addresses, but regions of memory -- perhaps
	   this feature should be incorporated into all ports?  */

	if (addr) {
		addr = arch_get_unmapped_area_1 (PAGE_ALIGN(addr), len, limit);
		if (addr != (unsigned long) -ENOMEM)
			return addr;
	}

	/* Next, try allocating at TASK_UNMAPPED_BASE.  */
	addr = arch_get_unmapped_area_1 (PAGE_ALIGN(TASK_UNMAPPED_BASE),
					 len, limit);
	if (addr != (unsigned long) -ENOMEM)
		return addr;

	/* Finally, try allocating in low memory.  */
	addr = arch_get_unmapped_area_1 (PAGE_SIZE, len, limit);

	return addr;
}

#ifdef CONFIG_OSF4_COMPAT

/* Clear top 32 bits of iov_len in the user's buffer for
   compatibility with old versions of OSF/1 where iov_len
   was defined as int. */
static int
osf_fix_iov_len(const struct iovec __user *iov, unsigned long count)
{
	unsigned long i;

	for (i = 0 ; i < count ; i++) {
		int __user *iov_len_high = (int __user *)&iov[i].iov_len + 1;

		if (put_user(0, iov_len_high))
			return -EFAULT;
	}
	return 0;
}

SYSCALL_DEFINE3(osf_readv, unsigned long, fd,
		const struct iovec __user *, vector, unsigned long, count)
{
	if (unlikely(personality(current->personality) == PER_OSF4))
		if (osf_fix_iov_len(vector, count))
			return -EFAULT;
	return sys_readv(fd, vector, count);
}

SYSCALL_DEFINE3(osf_writev, unsigned long, fd,
		const struct iovec __user *, vector, unsigned long, count)
{
	if (unlikely(personality(current->personality) == PER_OSF4))
		if (osf_fix_iov_len(vector, count))
			return -EFAULT;
	return sys_writev(fd, vector, count);
}

#endif
