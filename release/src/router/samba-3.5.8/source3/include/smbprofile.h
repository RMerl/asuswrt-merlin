#ifndef _PROFILE_H_
#define _PROFILE_H_
/* 
   Unix SMB/CIFS implementation.
   store smbd profiling information in shared memory
   Copyright (C) Andrew Tridgell 1999
   Copyright (C) James Peach 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/* this file defines the profile structure in the profile shared
   memory area */

#define PROF_SHMEM_KEY ((key_t)0x07021999)
#define PROF_SHM_MAGIC 0x6349985
#define PROF_SHM_VERSION 11

/* time values in the following structure are in microseconds */

#define __profile_stats_value(which, domain) domain[which]

enum profile_stats_values
{
	PR_VALUE_SMBD_IDLE = 0,
#define smbd_idle_count __profile_stats_value(PR_VALUE_SMBD_IDLE, count)
#define smbd_idle_time __profile_stats_value(PR_VALUE_SMBD_IDLE, time)

/* system call counters */
	PR_VALUE_SYSCALL_OPENDIR,
#define syscall_opendir_count __profile_stats_value(PR_VALUE_SYSCALL_OPENDIR, count)
#define syscall_opendir_time __profile_stats_value(PR_VALUE_SYSCALL_OPENDIR, time)

	PR_VALUE_SYSCALL_READDIR,
#define syscall_readdir_count __profile_stats_value(PR_VALUE_SYSCALL_READDIR, count)
#define syscall_readdir_time __profile_stats_value(PR_VALUE_SYSCALL_READDIR, time)

	PR_VALUE_SYSCALL_SEEKDIR,
#define syscall_seekdir_count __profile_stats_value(PR_VALUE_SYSCALL_SEEKDIR, count)
#define syscall_seekdir_time __profile_stats_value(PR_VALUE_SYSCALL_SEEKDIR, time)

	PR_VALUE_SYSCALL_TELLDIR,
#define syscall_telldir_count __profile_stats_value(PR_VALUE_SYSCALL_TELLDIR, count)
#define syscall_telldir_time __profile_stats_value(PR_VALUE_SYSCALL_TELLDIR, time)

	PR_VALUE_SYSCALL_REWINDDIR,
#define syscall_rewinddir_count __profile_stats_value(PR_VALUE_SYSCALL_REWINDDIR, count)
#define syscall_rewinddir_time __profile_stats_value(PR_VALUE_SYSCALL_REWINDDIR, time)

	PR_VALUE_SYSCALL_MKDIR,
#define syscall_mkdir_count __profile_stats_value(PR_VALUE_SYSCALL_MKDIR, count)
#define syscall_mkdir_time __profile_stats_value(PR_VALUE_SYSCALL_MKDIR, time)

	PR_VALUE_SYSCALL_RMDIR,
#define syscall_rmdir_count __profile_stats_value(PR_VALUE_SYSCALL_RMDIR, count)
#define syscall_rmdir_time __profile_stats_value(PR_VALUE_SYSCALL_RMDIR, time)

	PR_VALUE_SYSCALL_CLOSEDIR,
#define syscall_closedir_count __profile_stats_value(PR_VALUE_SYSCALL_CLOSEDIR, count)
#define syscall_closedir_time __profile_stats_value(PR_VALUE_SYSCALL_CLOSEDIR, time)

	PR_VALUE_SYSCALL_OPEN,
#define syscall_open_count __profile_stats_value(PR_VALUE_SYSCALL_OPEN, count)
#define syscall_open_time __profile_stats_value(PR_VALUE_SYSCALL_OPEN, time)

	PR_VALUE_SYSCALL_CREATEFILE,
#define syscall_createfile_count __profile_stats_value(PR_VALUE_SYSCALL_CREATEFILE, count)
#define syscall_createfile_time __profile_stats_value(PR_VALUE_SYSCALL_CREATEFILE, time)

	PR_VALUE_SYSCALL_CLOSE,
#define syscall_close_count __profile_stats_value(PR_VALUE_SYSCALL_CLOSE, count)
#define syscall_close_time __profile_stats_value(PR_VALUE_SYSCALL_CLOSE, time)

	PR_VALUE_SYSCALL_READ,
#define syscall_read_count __profile_stats_value(PR_VALUE_SYSCALL_READ, count)
#define syscall_read_time __profile_stats_value(PR_VALUE_SYSCALL_READ, time)

	PR_VALUE_SYSCALL_PREAD,
#define syscall_pread_count __profile_stats_value(PR_VALUE_SYSCALL_PREAD, count)
#define syscall_pread_time __profile_stats_value(PR_VALUE_SYSCALL_PREAD, time)

	PR_VALUE_SYSCALL_WRITE,
#define syscall_write_count __profile_stats_value(PR_VALUE_SYSCALL_WRITE, count)
#define syscall_write_time __profile_stats_value(PR_VALUE_SYSCALL_WRITE, time)

	PR_VALUE_SYSCALL_PWRITE,
#define syscall_pwrite_count __profile_stats_value(PR_VALUE_SYSCALL_PWRITE, count)
#define syscall_pwrite_time __profile_stats_value(PR_VALUE_SYSCALL_PWRITE, time)

	PR_VALUE_SYSCALL_LSEEK,
#define syscall_lseek_count __profile_stats_value(PR_VALUE_SYSCALL_LSEEK, count)
#define syscall_lseek_time __profile_stats_value(PR_VALUE_SYSCALL_LSEEK, time)

	PR_VALUE_SYSCALL_SENDFILE,
#define syscall_sendfile_count __profile_stats_value(PR_VALUE_SYSCALL_SENDFILE, count)
#define syscall_sendfile_time __profile_stats_value(PR_VALUE_SYSCALL_SENDFILE, time)

	PR_VALUE_SYSCALL_RECVFILE,
#define syscall_recvfile_count __profile_stats_value(PR_VALUE_SYSCALL_RECVFILE, count)
#define syscall_recvfile_time __profile_stats_value(PR_VALUE_SYSCALL_RECVFILE, time)

	PR_VALUE_SYSCALL_RENAME,
#define syscall_rename_count __profile_stats_value(PR_VALUE_SYSCALL_RENAME, count)
#define syscall_rename_time __profile_stats_value(PR_VALUE_SYSCALL_RENAME, time)

	PR_VALUE_SYSCALL_RENAME_AT,
#define syscall_rename_at_count __profile_stats_value(PR_VALUE_SYSCALL_RENAME_AT, count)
#define syscall_rename_at_time __profile_stats_value(PR_VALUE_SYSCALL_RENAME_AT, time)

	PR_VALUE_SYSCALL_FSYNC,
#define syscall_fsync_count __profile_stats_value(PR_VALUE_SYSCALL_FSYNC, count)
#define syscall_fsync_time __profile_stats_value(PR_VALUE_SYSCALL_FSYNC, time)

	PR_VALUE_SYSCALL_STAT,
#define syscall_stat_count __profile_stats_value(PR_VALUE_SYSCALL_STAT, count)
#define syscall_stat_time __profile_stats_value(PR_VALUE_SYSCALL_STAT, time)

	PR_VALUE_SYSCALL_FSTAT,
#define syscall_fstat_count __profile_stats_value(PR_VALUE_SYSCALL_FSTAT, count)
#define syscall_fstat_time __profile_stats_value(PR_VALUE_SYSCALL_FSTAT, time)

	PR_VALUE_SYSCALL_LSTAT,
#define syscall_lstat_count __profile_stats_value(PR_VALUE_SYSCALL_LSTAT, count)
#define syscall_lstat_time __profile_stats_value(PR_VALUE_SYSCALL_LSTAT, time)

	PR_VALUE_SYSCALL_GET_ALLOC_SIZE,
#define syscall_get_alloc_size_count __profile_stats_value(PR_VALUE_SYSCALL_GET_ALLOC_SIZE, count)
#define syscall_get_alloc_size_time __profile_stats_value(PR_VALUE_SYSCALL_GET_ALLOC_SIZE, time)

	PR_VALUE_SYSCALL_UNLINK,
#define syscall_unlink_count __profile_stats_value(PR_VALUE_SYSCALL_UNLINK, count)
#define syscall_unlink_time __profile_stats_value(PR_VALUE_SYSCALL_UNLINK, time)

	PR_VALUE_SYSCALL_CHMOD,
#define syscall_chmod_count __profile_stats_value(PR_VALUE_SYSCALL_CHMOD, count)
#define syscall_chmod_time __profile_stats_value(PR_VALUE_SYSCALL_CHMOD, time)

	PR_VALUE_SYSCALL_FCHMOD,
#define syscall_fchmod_count __profile_stats_value(PR_VALUE_SYSCALL_FCHMOD, count)
#define syscall_fchmod_time __profile_stats_value(PR_VALUE_SYSCALL_FCHMOD, time)

	PR_VALUE_SYSCALL_CHOWN,
#define syscall_chown_count __profile_stats_value(PR_VALUE_SYSCALL_CHOWN, count)
#define syscall_chown_time __profile_stats_value(PR_VALUE_SYSCALL_CHOWN, time)

	PR_VALUE_SYSCALL_FCHOWN,
#define syscall_fchown_count __profile_stats_value(PR_VALUE_SYSCALL_FCHOWN, count)
#define syscall_fchown_time __profile_stats_value(PR_VALUE_SYSCALL_FCHOWN, time)

	PR_VALUE_SYSCALL_LCHOWN,
#define syscall_lchown_count __profile_stats_value(PR_VALUE_SYSCALL_LCHOWN, count)
#define syscall_lchown_time __profile_stats_value(PR_VALUE_SYSCALL_LCHOWN, time)

	PR_VALUE_SYSCALL_CHDIR,
#define syscall_chdir_count __profile_stats_value(PR_VALUE_SYSCALL_CHDIR, count)
#define syscall_chdir_time __profile_stats_value(PR_VALUE_SYSCALL_CHDIR, time)

	PR_VALUE_SYSCALL_GETWD,
#define syscall_getwd_count __profile_stats_value(PR_VALUE_SYSCALL_GETWD, count)
#define syscall_getwd_time __profile_stats_value(PR_VALUE_SYSCALL_GETWD, time)

	PR_VALUE_SYSCALL_NTIMES,
#define syscall_ntimes_count __profile_stats_value(PR_VALUE_SYSCALL_NTIMES, count)
#define syscall_ntimes_time __profile_stats_value(PR_VALUE_SYSCALL_NTIMES, time)

	PR_VALUE_SYSCALL_FTRUNCATE,
#define syscall_ftruncate_count __profile_stats_value(PR_VALUE_SYSCALL_FTRUNCATE, count)
#define syscall_ftruncate_time __profile_stats_value(PR_VALUE_SYSCALL_FTRUNCATE, time)

	PR_VALUE_SYSCALL_FCNTL_LOCK,
#define syscall_fcntl_lock_count __profile_stats_value(PR_VALUE_SYSCALL_FCNTL_LOCK, count)
#define syscall_fcntl_lock_time __profile_stats_value(PR_VALUE_SYSCALL_FCNTL_LOCK, time)

	PR_VALUE_SYSCALL_KERNEL_FLOCK,
#define syscall_kernel_flock_count __profile_stats_value(PR_VALUE_SYSCALL_KERNEL_FLOCK, count)
#define syscall_kernel_flock_time __profile_stats_value(PR_VALUE_SYSCALL_KERNEL_FLOCK, time)

	PR_VALUE_SYSCALL_LINUX_SETLEASE,
#define syscall_linux_setlease_count __profile_stats_value(PR_VALUE_SYSCALL_LINUX_SETLEASE, count)
#define syscall_linux_setlease_time __profile_stats_value(PR_VALUE_SYSCALL_LINUX_SETLEASE, time)

	PR_VALUE_SYSCALL_FCNTL_GETLOCK,
#define syscall_fcntl_getlock_count __profile_stats_value(PR_VALUE_SYSCALL_FCNTL_GETLOCK, count)
#define syscall_fcntl_getlock_time __profile_stats_value(PR_VALUE_SYSCALL_FCNTL_GETLOCK, time)

	PR_VALUE_SYSCALL_READLINK,
#define syscall_readlink_count __profile_stats_value(PR_VALUE_SYSCALL_READLINK, count)
#define syscall_readlink_time __profile_stats_value(PR_VALUE_SYSCALL_READLINK, time)

	PR_VALUE_SYSCALL_SYMLINK,
#define syscall_symlink_count __profile_stats_value(PR_VALUE_SYSCALL_SYMLINK, count)
#define syscall_symlink_time __profile_stats_value(PR_VALUE_SYSCALL_SYMLINK, time)

	PR_VALUE_SYSCALL_LINK,
#define syscall_link_count __profile_stats_value(PR_VALUE_SYSCALL_LINK, count)
#define syscall_link_time __profile_stats_value(PR_VALUE_SYSCALL_LINK, time)

	PR_VALUE_SYSCALL_MKNOD,
#define syscall_mknod_count __profile_stats_value(PR_VALUE_SYSCALL_MKNOD, count)
#define syscall_mknod_time __profile_stats_value(PR_VALUE_SYSCALL_MKNOD, time)

	PR_VALUE_SYSCALL_REALPATH,
#define syscall_realpath_count __profile_stats_value(PR_VALUE_SYSCALL_REALPATH, count)
#define syscall_realpath_time __profile_stats_value(PR_VALUE_SYSCALL_REALPATH, time)

	PR_VALUE_SYSCALL_GET_QUOTA,
#define syscall_get_quota_count __profile_stats_value(PR_VALUE_SYSCALL_GET_QUOTA, count)
#define syscall_get_quota_time __profile_stats_value(PR_VALUE_SYSCALL_GET_QUOTA, time)

	PR_VALUE_SYSCALL_SET_QUOTA,
#define syscall_set_quota_count __profile_stats_value(PR_VALUE_SYSCALL_SET_QUOTA, count)
#define syscall_set_quota_time __profile_stats_value(PR_VALUE_SYSCALL_SET_QUOTA, time)

	PR_VALUE_SYSCALL_GET_SD,
#define syscall_get_sd_count __profile_stats_value(PR_VALUE_SYSCALL_GET_SD, count)
#define syscall_get_sd_time __profile_stats_value(PR_VALUE_SYSCALL_GET_SD, time)

	PR_VALUE_SYSCALL_SET_SD,
#define syscall_set_sd_count __profile_stats_value(PR_VALUE_SYSCALL_SET_SD, count)
#define syscall_set_sd_time __profile_stats_value(PR_VALUE_SYSCALL_SET_SD, time)

	PR_VALUE_SYSCALL_BRL_LOCK,
#define syscall_brl_lock_count __profile_stats_value(PR_VALUE_SYSCALL_BRL_LOCK, count)
#define syscall_brl_lock_time __profile_stats_value(PR_VALUE_SYSCALL_BRL_LOCK, time)

	PR_VALUE_SYSCALL_BRL_UNLOCK,
#define syscall_brl_unlock_count __profile_stats_value(PR_VALUE_SYSCALL_BRL_UNLOCK, count)
#define syscall_brl_unlock_time __profile_stats_value(PR_VALUE_SYSCALL_BRL_UNLOCK, time)

	PR_VALUE_SYSCALL_BRL_CANCEL,
#define syscall_brl_cancel_count __profile_stats_value(PR_VALUE_SYSCALL_BRL_CANCEL, count)
#define syscall_brl_cancel_time __profile_stats_value(PR_VALUE_SYSCALL_BRL_CANCEL, time)

	PR_VALUE_SYSCALL_STRICT_LOCK,
#define syscall_strict_lock_count __profile_stats_value(PR_VALUE_SYSCALL_STRICT_LOCK, count)
#define syscall_strict_lock_time __profile_stats_value(PR_VALUE_SYSCALL_STRICT_LOCK, time)

	PR_VALUE_SYSCALL_STRICT_UNLOCK,
#define syscall_strict_unlock_count __profile_stats_value(PR_VALUE_SYSCALL_STRICT_UNLOCK, count)
#define syscall_strict_unlock_time __profile_stats_value(PR_VALUE_SYSCALL_STRICT_UNLOCK, time)

/* counters for individual SMB types */
	PR_VALUE_SMBMKDIR,
#define SMBmkdir_count __profile_stats_value(PR_VALUE_SMBMKDIR, count)
#define SMBmkdir_time __profile_stats_value(PR_VALUE_SMBMKDIR, time)

	PR_VALUE_SMBRMDIR,
#define SMBrmdir_count __profile_stats_value(PR_VALUE_SMBRMDIR, count)
#define SMBrmdir_time __profile_stats_value(PR_VALUE_SMBRMDIR, time)

	PR_VALUE_SMBOPEN,
#define SMBopen_count __profile_stats_value(PR_VALUE_SMBOPEN, count)
#define SMBopen_time __profile_stats_value(PR_VALUE_SMBOPEN, time)

	PR_VALUE_SMBCREATE,
#define SMBcreate_count __profile_stats_value(PR_VALUE_SMBCREATE, count)
#define SMBcreate_time __profile_stats_value(PR_VALUE_SMBCREATE, time)

	PR_VALUE_SMBCLOSE,
#define SMBclose_count __profile_stats_value(PR_VALUE_SMBCLOSE, count)
#define SMBclose_time __profile_stats_value(PR_VALUE_SMBCLOSE, time)

	PR_VALUE_SMBFLUSH,
#define SMBflush_count __profile_stats_value(PR_VALUE_SMBFLUSH, count)
#define SMBflush_time __profile_stats_value(PR_VALUE_SMBFLUSH, time)

	PR_VALUE_SMBUNLINK,
#define SMBunlink_count __profile_stats_value(PR_VALUE_SMBUNLINK, count)
#define SMBunlink_time __profile_stats_value(PR_VALUE_SMBUNLINK, time)

	PR_VALUE_SMBMV,
#define SMBmv_count __profile_stats_value(PR_VALUE_SMBMV, count)
#define SMBmv_time __profile_stats_value(PR_VALUE_SMBMV, time)

	PR_VALUE_SMBGETATR,
#define SMBgetatr_count __profile_stats_value(PR_VALUE_SMBGETATR, count)
#define SMBgetatr_time __profile_stats_value(PR_VALUE_SMBGETATR, time)

	PR_VALUE_SMBSETATR,
#define SMBsetatr_count __profile_stats_value(PR_VALUE_SMBSETATR, count)
#define SMBsetatr_time __profile_stats_value(PR_VALUE_SMBSETATR, time)

	PR_VALUE_SMBREAD,
#define SMBread_count __profile_stats_value(PR_VALUE_SMBREAD, count)
#define SMBread_time __profile_stats_value(PR_VALUE_SMBREAD, time)

	PR_VALUE_SMBWRITE,
#define SMBwrite_count __profile_stats_value(PR_VALUE_SMBWRITE, count)
#define SMBwrite_time __profile_stats_value(PR_VALUE_SMBWRITE, time)

	PR_VALUE_SMBLOCK,
#define SMBlock_count __profile_stats_value(PR_VALUE_SMBLOCK, count)
#define SMBlock_time __profile_stats_value(PR_VALUE_SMBLOCK, time)

	PR_VALUE_SMBUNLOCK,
#define SMBunlock_count __profile_stats_value(PR_VALUE_SMBUNLOCK, count)
#define SMBunlock_time __profile_stats_value(PR_VALUE_SMBUNLOCK, time)

	PR_VALUE_SMBCTEMP,
#define SMBctemp_count __profile_stats_value(PR_VALUE_SMBCTEMP, count)
#define SMBctemp_time __profile_stats_value(PR_VALUE_SMBCTEMP, time)

	/* SMBmknew stats are currently combined with SMBcreate */
	PR_VALUE_SMBMKNEW,
#define SMBmknew_count __profile_stats_value(PR_VALUE_SMBMKNEW, count)
#define SMBmknew_time __profile_stats_value(PR_VALUE_SMBMKNEW, time)

	PR_VALUE_SMBCHECKPATH,
#define SMBcheckpath_count __profile_stats_value(PR_VALUE_SMBCHECKPATH, count)
#define SMBcheckpath_time __profile_stats_value(PR_VALUE_SMBCHECKPATH, time)

	PR_VALUE_SMBEXIT,
#define SMBexit_count __profile_stats_value(PR_VALUE_SMBEXIT, count)
#define SMBexit_time __profile_stats_value(PR_VALUE_SMBEXIT, time)

	PR_VALUE_SMBLSEEK,
#define SMBlseek_count __profile_stats_value(PR_VALUE_SMBLSEEK, count)
#define SMBlseek_time __profile_stats_value(PR_VALUE_SMBLSEEK, time)

	PR_VALUE_SMBLOCKREAD,
#define SMBlockread_count __profile_stats_value(PR_VALUE_SMBLOCKREAD, count)
#define SMBlockread_time __profile_stats_value(PR_VALUE_SMBLOCKREAD, time)

	PR_VALUE_SMBWRITEUNLOCK,
#define SMBwriteunlock_count __profile_stats_value(PR_VALUE_SMBWRITEUNLOCK, count)
#define SMBwriteunlock_time __profile_stats_value(PR_VALUE_SMBWRITEUNLOCK, time)

	PR_VALUE_SMBREADBRAW,
#define SMBreadbraw_count __profile_stats_value(PR_VALUE_SMBREADBRAW, count)
#define SMBreadbraw_time __profile_stats_value(PR_VALUE_SMBREADBRAW, time)

	PR_VALUE_SMBREADBMPX,
#define SMBreadBmpx_count __profile_stats_value(PR_VALUE_SMBREADBMPX, count)
#define SMBreadBmpx_time __profile_stats_value(PR_VALUE_SMBREADBMPX, time)

	PR_VALUE_SMBREADBS,
#define SMBreadBs_count __profile_stats_value(PR_VALUE_SMBREADBS, count)
#define SMBreadBs_time __profile_stats_value(PR_VALUE_SMBREADBS, time)

	PR_VALUE_SMBWRITEBRAW,
#define SMBwritebraw_count __profile_stats_value(PR_VALUE_SMBWRITEBRAW, count)
#define SMBwritebraw_time __profile_stats_value(PR_VALUE_SMBWRITEBRAW, time)

	PR_VALUE_SMBWRITEBMPX,
#define SMBwriteBmpx_count __profile_stats_value(PR_VALUE_SMBWRITEBMPX, count)
#define SMBwriteBmpx_time __profile_stats_value(PR_VALUE_SMBWRITEBMPX, time)

	PR_VALUE_SMBWRITEBS,
#define SMBwriteBs_count __profile_stats_value(PR_VALUE_SMBWRITEBS, count)
#define SMBwriteBs_time __profile_stats_value(PR_VALUE_SMBWRITEBS, time)

	PR_VALUE_SMBWRITEC,
#define SMBwritec_count __profile_stats_value(PR_VALUE_SMBWRITEC, count)
#define SMBwritec_time __profile_stats_value(PR_VALUE_SMBWRITEC, time)

	PR_VALUE_SMBSETATTRE,
#define SMBsetattrE_count __profile_stats_value(PR_VALUE_SMBSETATTRE, count)
#define SMBsetattrE_time __profile_stats_value(PR_VALUE_SMBSETATTRE, time)

	PR_VALUE_SMBGETATTRE,
#define SMBgetattrE_count __profile_stats_value(PR_VALUE_SMBGETATTRE, count)
#define SMBgetattrE_time __profile_stats_value(PR_VALUE_SMBGETATTRE, time)

	PR_VALUE_SMBLOCKINGX,
#define SMBlockingX_count __profile_stats_value(PR_VALUE_SMBLOCKINGX, count)
#define SMBlockingX_time __profile_stats_value(PR_VALUE_SMBLOCKINGX, time)

	PR_VALUE_SMBTRANS,
#define SMBtrans_count __profile_stats_value(PR_VALUE_SMBTRANS, count)
#define SMBtrans_time __profile_stats_value(PR_VALUE_SMBTRANS, time)

	PR_VALUE_SMBTRANSS,
#define SMBtranss_count __profile_stats_value(PR_VALUE_SMBTRANSS, count)
#define SMBtranss_time __profile_stats_value(PR_VALUE_SMBTRANSS, time)

	PR_VALUE_SMBIOCTL,
#define SMBioctl_count __profile_stats_value(PR_VALUE_SMBIOCTL, count)
#define SMBioctl_time __profile_stats_value(PR_VALUE_SMBIOCTL, time)

	PR_VALUE_SMBIOCTLS,
#define SMBioctls_count __profile_stats_value(PR_VALUE_SMBIOCTLS, count)
#define SMBioctls_time __profile_stats_value(PR_VALUE_SMBIOCTLS, time)

	PR_VALUE_SMBCOPY,
#define SMBcopy_count __profile_stats_value(PR_VALUE_SMBCOPY, count)
#define SMBcopy_time __profile_stats_value(PR_VALUE_SMBCOPY, time)

	PR_VALUE_SMBMOVE,
#define SMBmove_count __profile_stats_value(PR_VALUE_SMBMOVE, count)
#define SMBmove_time __profile_stats_value(PR_VALUE_SMBMOVE, time)

	PR_VALUE_SMBECHO,
#define SMBecho_count __profile_stats_value(PR_VALUE_SMBECHO, count)
#define SMBecho_time __profile_stats_value(PR_VALUE_SMBECHO, time)

	PR_VALUE_SMBWRITECLOSE,
#define SMBwriteclose_count __profile_stats_value(PR_VALUE_SMBWRITECLOSE, count)
#define SMBwriteclose_time __profile_stats_value(PR_VALUE_SMBWRITECLOSE, time)

	PR_VALUE_SMBOPENX,
#define SMBopenX_count __profile_stats_value(PR_VALUE_SMBOPENX, count)
#define SMBopenX_time __profile_stats_value(PR_VALUE_SMBOPENX, time)

	PR_VALUE_SMBREADX,
#define SMBreadX_count __profile_stats_value(PR_VALUE_SMBREADX, count)
#define SMBreadX_time __profile_stats_value(PR_VALUE_SMBREADX, time)

	PR_VALUE_SMBWRITEX,
#define SMBwriteX_count __profile_stats_value(PR_VALUE_SMBWRITEX, count)
#define SMBwriteX_time __profile_stats_value(PR_VALUE_SMBWRITEX, time)

	PR_VALUE_SMBTRANS2,
#define SMBtrans2_count __profile_stats_value(PR_VALUE_SMBTRANS2, count)
#define SMBtrans2_time __profile_stats_value(PR_VALUE_SMBTRANS2, time)

	PR_VALUE_SMBTRANSS2,
#define SMBtranss2_count __profile_stats_value(PR_VALUE_SMBTRANSS2, count)
#define SMBtranss2_time __profile_stats_value(PR_VALUE_SMBTRANSS2, time)

	PR_VALUE_SMBFINDCLOSE,
#define SMBfindclose_count __profile_stats_value(PR_VALUE_SMBFINDCLOSE, count)
#define SMBfindclose_time __profile_stats_value(PR_VALUE_SMBFINDCLOSE, time)

	PR_VALUE_SMBFINDNCLOSE,
#define SMBfindnclose_count __profile_stats_value(PR_VALUE_SMBFINDNCLOSE, count)
#define SMBfindnclose_time __profile_stats_value(PR_VALUE_SMBFINDNCLOSE, time)

	PR_VALUE_SMBTCON,
#define SMBtcon_count __profile_stats_value(PR_VALUE_SMBTCON, count)
#define SMBtcon_time __profile_stats_value(PR_VALUE_SMBTCON, time)

	PR_VALUE_SMBTDIS,
#define SMBtdis_count __profile_stats_value(PR_VALUE_SMBTDIS, count)
#define SMBtdis_time __profile_stats_value(PR_VALUE_SMBTDIS, time)

	PR_VALUE_SMBNEGPROT,
#define SMBnegprot_count __profile_stats_value(PR_VALUE_SMBNEGPROT, count)
#define SMBnegprot_time __profile_stats_value(PR_VALUE_SMBNEGPROT, time)

	PR_VALUE_SMBSESSSETUPX,
#define SMBsesssetupX_count __profile_stats_value(PR_VALUE_SMBSESSSETUPX, count)
#define SMBsesssetupX_time __profile_stats_value(PR_VALUE_SMBSESSSETUPX, time)

	PR_VALUE_SMBULOGOFFX,
#define SMBulogoffX_count __profile_stats_value(PR_VALUE_SMBULOGOFFX, count)
#define SMBulogoffX_time __profile_stats_value(PR_VALUE_SMBULOGOFFX, time)

	PR_VALUE_SMBTCONX,
#define SMBtconX_count __profile_stats_value(PR_VALUE_SMBTCONX, count)
#define SMBtconX_time __profile_stats_value(PR_VALUE_SMBTCONX, time)

	PR_VALUE_SMBDSKATTR,
#define SMBdskattr_count __profile_stats_value(PR_VALUE_SMBDSKATTR, count)
#define SMBdskattr_time __profile_stats_value(PR_VALUE_SMBDSKATTR, time)

	PR_VALUE_SMBSEARCH,
#define SMBsearch_count __profile_stats_value(PR_VALUE_SMBSEARCH, count)
#define SMBsearch_time __profile_stats_value(PR_VALUE_SMBSEARCH, time)

	/* SBMffirst stats combined with SMBsearch */
	PR_VALUE_SMBFFIRST,
#define SMBffirst_count __profile_stats_value(PR_VALUE_SMBFFIRST, count)
#define SMBffirst_time __profile_stats_value(PR_VALUE_SMBFFIRST, time)

	/* SBMfunique stats combined with SMBsearch */
	PR_VALUE_SMBFUNIQUE,
#define SMBfunique_count __profile_stats_value(PR_VALUE_SMBFUNIQUE, count)
#define SMBfunique_time __profile_stats_value(PR_VALUE_SMBFUNIQUE, time)

	PR_VALUE_SMBFCLOSE,
#define SMBfclose_count __profile_stats_value(PR_VALUE_SMBFCLOSE, count)
#define SMBfclose_time __profile_stats_value(PR_VALUE_SMBFCLOSE, time)

	PR_VALUE_SMBNTTRANS,
#define SMBnttrans_count __profile_stats_value(PR_VALUE_SMBNTTRANS, count)
#define SMBnttrans_time __profile_stats_value(PR_VALUE_SMBNTTRANS, time)

	PR_VALUE_SMBNTTRANSS,
#define SMBnttranss_count __profile_stats_value(PR_VALUE_SMBNTTRANSS, count)
#define SMBnttranss_time __profile_stats_value(PR_VALUE_SMBNTTRANSS, time)

	PR_VALUE_SMBNTCREATEX,
#define SMBntcreateX_count __profile_stats_value(PR_VALUE_SMBNTCREATEX, count)
#define SMBntcreateX_time __profile_stats_value(PR_VALUE_SMBNTCREATEX, time)

	PR_VALUE_SMBNTCANCEL,
#define SMBntcancel_count __profile_stats_value(PR_VALUE_SMBNTCANCEL, count)
#define SMBntcancel_time __profile_stats_value(PR_VALUE_SMBNTCANCEL, time)

	PR_VALUE_SMBNTRENAME,
#define SMBntrename_count __profile_stats_value(PR_VALUE_SMBNTRENAME, count)
#define SMBntrename_time __profile_stats_value(PR_VALUE_SMBNTRENAME, time)

	PR_VALUE_SMBSPLOPEN,
#define SMBsplopen_count __profile_stats_value(PR_VALUE_SMBSPLOPEN, count)
#define SMBsplopen_time __profile_stats_value(PR_VALUE_SMBSPLOPEN, time)

	PR_VALUE_SMBSPLWR,
#define SMBsplwr_count __profile_stats_value(PR_VALUE_SMBSPLWR, count)
#define SMBsplwr_time __profile_stats_value(PR_VALUE_SMBSPLWR, time)

	PR_VALUE_SMBSPLCLOSE,
#define SMBsplclose_count __profile_stats_value(PR_VALUE_SMBSPLCLOSE, count)
#define SMBsplclose_time __profile_stats_value(PR_VALUE_SMBSPLCLOSE, time)

	PR_VALUE_SMBSPLRETQ,
#define SMBsplretq_count __profile_stats_value(PR_VALUE_SMBSPLRETQ, count)
#define SMBsplretq_time __profile_stats_value(PR_VALUE_SMBSPLRETQ, time)

	PR_VALUE_SMBSENDS,
#define SMBsends_count __profile_stats_value(PR_VALUE_SMBSENDS, count)
#define SMBsends_time __profile_stats_value(PR_VALUE_SMBSENDS, time)

	PR_VALUE_SMBSENDB,
#define SMBsendb_count __profile_stats_value(PR_VALUE_SMBSENDB, count)
#define SMBsendb_time __profile_stats_value(PR_VALUE_SMBSENDB, time)

	PR_VALUE_SMBFWDNAME,
#define SMBfwdname_count __profile_stats_value(PR_VALUE_SMBFWDNAME, count)
#define SMBfwdname_time __profile_stats_value(PR_VALUE_SMBFWDNAME, time)

	PR_VALUE_SMBCANCELF,
#define SMBcancelf_count __profile_stats_value(PR_VALUE_SMBCANCELF, count)
#define SMBcancelf_time __profile_stats_value(PR_VALUE_SMBCANCELF, time)

	PR_VALUE_SMBGETMAC,
#define SMBgetmac_count __profile_stats_value(PR_VALUE_SMBGETMAC, count)
#define SMBgetmac_time __profile_stats_value(PR_VALUE_SMBGETMAC, time)

	PR_VALUE_SMBSENDSTRT,
#define SMBsendstrt_count __profile_stats_value(PR_VALUE_SMBSENDSTRT, count)
#define SMBsendstrt_time __profile_stats_value(PR_VALUE_SMBSENDSTRT, time)

	PR_VALUE_SMBSENDEND,
#define SMBsendend_count __profile_stats_value(PR_VALUE_SMBSENDEND, count)
#define SMBsendend_time __profile_stats_value(PR_VALUE_SMBSENDEND, time)

	PR_VALUE_SMBSENDTXT,
#define SMBsendtxt_count __profile_stats_value(PR_VALUE_SMBSENDTXT, count)
#define SMBsendtxt_time __profile_stats_value(PR_VALUE_SMBSENDTXT, time)

	PR_VALUE_SMBINVALID,
#define SMBinvalid_count __profile_stats_value(PR_VALUE_SMBINVALID, count)
#define SMBinvalid_time __profile_stats_value(PR_VALUE_SMBINVALID, time)

/* Pathworks setdir command */
	PR_VALUE_PATHWORKS_SETDIR,
#define pathworks_setdir_count __profile_stats_value(PR_VALUE_PATHWORKS_SETDIR, count)
#define pathworks_setdir_time __profile_stats_value(PR_VALUE_PATHWORKS_SETDIR, time)

/* These are the TRANS2 sub commands */
	PR_VALUE_TRANS2_OPEN,
#define Trans2_open_count __profile_stats_value(PR_VALUE_TRANS2_OPEN, count)
#define Trans2_open_time __profile_stats_value(PR_VALUE_TRANS2_OPEN, time)

	PR_VALUE_TRANS2_FINDFIRST,
#define Trans2_findfirst_count __profile_stats_value(PR_VALUE_TRANS2_FINDFIRST, count)
#define Trans2_findfirst_time __profile_stats_value(PR_VALUE_TRANS2_FINDFIRST, time)

	PR_VALUE_TRANS2_FINDNEXT,
#define Trans2_findnext_count __profile_stats_value(PR_VALUE_TRANS2_FINDNEXT, count)
#define Trans2_findnext_time __profile_stats_value(PR_VALUE_TRANS2_FINDNEXT, time)

	PR_VALUE_TRANS2_QFSINFO,
#define Trans2_qfsinfo_count __profile_stats_value(PR_VALUE_TRANS2_QFSINFO, count)
#define Trans2_qfsinfo_time __profile_stats_value(PR_VALUE_TRANS2_QFSINFO, time)

	PR_VALUE_TRANS2_SETFSINFO,
#define Trans2_setfsinfo_count __profile_stats_value(PR_VALUE_TRANS2_SETFSINFO, count)
#define Trans2_setfsinfo_time __profile_stats_value(PR_VALUE_TRANS2_SETFSINFO, time)

	PR_VALUE_TRANS2_QPATHINFO,
#define Trans2_qpathinfo_count __profile_stats_value(PR_VALUE_TRANS2_QPATHINFO, count)
#define Trans2_qpathinfo_time __profile_stats_value(PR_VALUE_TRANS2_QPATHINFO, time)

	PR_VALUE_TRANS2_SETPATHINFO,
#define Trans2_setpathinfo_count __profile_stats_value(PR_VALUE_TRANS2_SETPATHINFO, count)
#define Trans2_setpathinfo_time __profile_stats_value(PR_VALUE_TRANS2_SETPATHINFO, time)

	PR_VALUE_TRANS2_QFILEINFO,
#define Trans2_qfileinfo_count __profile_stats_value(PR_VALUE_TRANS2_QFILEINFO, count)
#define Trans2_qfileinfo_time __profile_stats_value(PR_VALUE_TRANS2_QFILEINFO, time)

	PR_VALUE_TRANS2_SETFILEINFO,
#define Trans2_setfileinfo_count __profile_stats_value(PR_VALUE_TRANS2_SETFILEINFO, count)
#define Trans2_setfileinfo_time __profile_stats_value(PR_VALUE_TRANS2_SETFILEINFO, time)

	PR_VALUE_TRANS2_FSCTL,
#define Trans2_fsctl_count __profile_stats_value(PR_VALUE_TRANS2_FSCTL, count)
#define Trans2_fsctl_time __profile_stats_value(PR_VALUE_TRANS2_FSCTL, time)

	PR_VALUE_TRANS2_IOCTL,
#define Trans2_ioctl_count __profile_stats_value(PR_VALUE_TRANS2_IOCTL, count)
#define Trans2_ioctl_time __profile_stats_value(PR_VALUE_TRANS2_IOCTL, time)

	PR_VALUE_TRANS2_FINDNOTIFYFIRST,
#define Trans2_findnotifyfirst_count __profile_stats_value(PR_VALUE_TRANS2_FINDNOTIFYFIRST, count)
#define Trans2_findnotifyfirst_time __profile_stats_value(PR_VALUE_TRANS2_FINDNOTIFYFIRST, time)

	PR_VALUE_TRANS2_FINDNOTIFYNEXT,
#define Trans2_findnotifynext_count __profile_stats_value(PR_VALUE_TRANS2_FINDNOTIFYNEXT, count)
#define Trans2_findnotifynext_time __profile_stats_value(PR_VALUE_TRANS2_FINDNOTIFYNEXT, time)

	PR_VALUE_TRANS2_MKDIR,
#define Trans2_mkdir_count __profile_stats_value(PR_VALUE_TRANS2_MKDIR, count)
#define Trans2_mkdir_time __profile_stats_value(PR_VALUE_TRANS2_MKDIR, time)

	PR_VALUE_TRANS2_SESSION_SETUP,
#define Trans2_session_setup_count __profile_stats_value(PR_VALUE_TRANS2_SESSION_SETUP, count)
#define Trans2_session_setup_time __profile_stats_value(PR_VALUE_TRANS2_SESSION_SETUP, time)

	PR_VALUE_TRANS2_GET_DFS_REFERRAL,
#define Trans2_get_dfs_referral_count __profile_stats_value(PR_VALUE_TRANS2_GET_DFS_REFERRAL, count)
#define Trans2_get_dfs_referral_time __profile_stats_value(PR_VALUE_TRANS2_GET_DFS_REFERRAL, time)

	PR_VALUE_TRANS2_REPORT_DFS_INCONSISTANCY,
#define Trans2_report_dfs_inconsistancy_count __profile_stats_value(PR_VALUE_TRANS2_REPORT_DFS_INCONSISTANCY, count)
#define Trans2_report_dfs_inconsistancy_time __profile_stats_value(PR_VALUE_TRANS2_REPORT_DFS_INCONSISTANCY, time)

/* These are the NT transact sub commands. */
	PR_VALUE_NT_TRANSACT_CREATE,
#define NT_transact_create_count __profile_stats_value(PR_VALUE_NT_TRANSACT_CREATE, count)
#define NT_transact_create_time __profile_stats_value(PR_VALUE_NT_TRANSACT_CREATE, time)

	PR_VALUE_NT_TRANSACT_IOCTL,
#define NT_transact_ioctl_count __profile_stats_value(PR_VALUE_NT_TRANSACT_IOCTL, count)
#define NT_transact_ioctl_time __profile_stats_value(PR_VALUE_NT_TRANSACT_IOCTL, time)

	PR_VALUE_NT_TRANSACT_SET_SECURITY_DESC,
#define NT_transact_set_security_desc_count __profile_stats_value(PR_VALUE_NT_TRANSACT_SET_SECURITY_DESC, count)
#define NT_transact_set_security_desc_time __profile_stats_value(PR_VALUE_NT_TRANSACT_SET_SECURITY_DESC, time)

	PR_VALUE_NT_TRANSACT_NOTIFY_CHANGE,
#define NT_transact_notify_change_count __profile_stats_value(PR_VALUE_NT_TRANSACT_NOTIFY_CHANGE, count)
#define NT_transact_notify_change_time __profile_stats_value(PR_VALUE_NT_TRANSACT_NOTIFY_CHANGE, time)

	PR_VALUE_NT_TRANSACT_RENAME,
#define NT_transact_rename_count __profile_stats_value(PR_VALUE_NT_TRANSACT_RENAME, count)
#define NT_transact_rename_time __profile_stats_value(PR_VALUE_NT_TRANSACT_RENAME, time)

	PR_VALUE_NT_TRANSACT_QUERY_SECURITY_DESC,
#define NT_transact_query_security_desc_count __profile_stats_value(PR_VALUE_NT_TRANSACT_QUERY_SECURITY_DESC, count)
#define NT_transact_query_security_desc_time __profile_stats_value(PR_VALUE_NT_TRANSACT_QUERY_SECURITY_DESC, time)

	PR_VALUE_NT_TRANSACT_GET_USER_QUOTA,
#define NT_transact_get_user_quota_count __profile_stats_value(PR_VALUE_NT_TRANSACT_GET_USER_QUOTA, count)
#define NT_transact_get_user_quota_time __profile_stats_value(PR_VALUE_NT_TRANSACT_GET_USER_QUOTA, time)

	PR_VALUE_NT_TRANSACT_SET_USER_QUOTA,
#define NT_transact_set_user_quota_count __profile_stats_value(PR_VALUE_NT_TRANSACT_SET_USER_QUOTA, count)
#define NT_transact_set_user_quota_time __profile_stats_value(PR_VALUE_NT_TRANSACT_SET_USER_QUOTA, time)

/* These are ACL manipulation calls */
	PR_VALUE_GET_NT_ACL,
#define get_nt_acl_count __profile_stats_value(PR_VALUE_GET_NT_ACL, count)
#define get_nt_acl_time __profile_stats_value(PR_VALUE_GET_NT_ACL, time)

	PR_VALUE_FGET_NT_ACL,
#define fget_nt_acl_count __profile_stats_value(PR_VALUE_FGET_NT_ACL, count)
#define fget_nt_acl_time __profile_stats_value(PR_VALUE_FGET_NT_ACL, time)

	PR_VALUE_FSET_NT_ACL,
#define fset_nt_acl_count __profile_stats_value(PR_VALUE_FSET_NT_ACL, count)
#define fset_nt_acl_time __profile_stats_value(PR_VALUE_FSET_NT_ACL, time)

	PR_VALUE_CHMOD_ACL,
#define chmod_acl_count __profile_stats_value(PR_VALUE_CHMOD_ACL, count)
#define chmod_acl_time __profile_stats_value(PR_VALUE_CHMOD_ACL, time)

	PR_VALUE_FCHMOD_ACL,
#define fchmod_acl_count __profile_stats_value(PR_VALUE_FCHMOD_ACL, count)
#define fchmod_acl_time __profile_stats_value(PR_VALUE_FCHMOD_ACL, time)

/* These are nmbd stats */
	PR_VALUE_NAME_RELEASE,
#define name_release_count __profile_stats_value(PR_VALUE_NAME_RELEASE, count)
#define name_release_time __profile_stats_value(PR_VALUE_NAME_RELEASE, time)

	PR_VALUE_NAME_REFRESH,
#define name_refresh_count __profile_stats_value(PR_VALUE_NAME_REFRESH, count)
#define name_refresh_time __profile_stats_value(PR_VALUE_NAME_REFRESH, time)

	PR_VALUE_NAME_REGISTRATION,
#define name_registration_count __profile_stats_value(PR_VALUE_NAME_REGISTRATION, count)
#define name_registration_time __profile_stats_value(PR_VALUE_NAME_REGISTRATION, time)

	PR_VALUE_NODE_STATUS,
#define node_status_count __profile_stats_value(PR_VALUE_NODE_STATUS, count)
#define node_status_time __profile_stats_value(PR_VALUE_NODE_STATUS, time)

	PR_VALUE_NAME_QUERY,
#define name_query_count __profile_stats_value(PR_VALUE_NAME_QUERY, count)
#define name_query_time __profile_stats_value(PR_VALUE_NAME_QUERY, time)

	PR_VALUE_HOST_ANNOUNCE,
#define host_announce_count __profile_stats_value(PR_VALUE_HOST_ANNOUNCE, count)
#define host_announce_time __profile_stats_value(PR_VALUE_HOST_ANNOUNCE, time)

	PR_VALUE_WORKGROUP_ANNOUNCE,
#define workgroup_announce_count __profile_stats_value(PR_VALUE_WORKGROUP_ANNOUNCE, count)
#define workgroup_announce_time __profile_stats_value(PR_VALUE_WORKGROUP_ANNOUNCE, time)

	PR_VALUE_LOCAL_MASTER_ANNOUNCE,
#define local_master_announce_count __profile_stats_value(PR_VALUE_LOCAL_MASTER_ANNOUNCE, count)
#define local_master_announce_time __profile_stats_value(PR_VALUE_LOCAL_MASTER_ANNOUNCE, time)

	PR_VALUE_MASTER_BROWSER_ANNOUNCE,
#define master_browser_announce_count __profile_stats_value(PR_VALUE_MASTER_BROWSER_ANNOUNCE, count)
#define master_browser_announce_time __profile_stats_value(PR_VALUE_MASTER_BROWSER_ANNOUNCE, time)

	PR_VALUE_LM_HOST_ANNOUNCE,
#define lm_host_announce_count __profile_stats_value(PR_VALUE_LM_HOST_ANNOUNCE, count)
#define lm_host_announce_time __profile_stats_value(PR_VALUE_LM_HOST_ANNOUNCE, time)

	PR_VALUE_GET_BACKUP_LIST,
#define get_backup_list_count __profile_stats_value(PR_VALUE_GET_BACKUP_LIST, count)
#define get_backup_list_time __profile_stats_value(PR_VALUE_GET_BACKUP_LIST, time)

	PR_VALUE_RESET_BROWSER,
#define reset_browser_count __profile_stats_value(PR_VALUE_RESET_BROWSER, count)
#define reset_browser_time __profile_stats_value(PR_VALUE_RESET_BROWSER, time)

	PR_VALUE_ANNOUNCE_REQUEST,
#define announce_request_count __profile_stats_value(PR_VALUE_ANNOUNCE_REQUEST, count)
#define announce_request_time __profile_stats_value(PR_VALUE_ANNOUNCE_REQUEST, time)

	PR_VALUE_LM_ANNOUNCE_REQUEST,
#define lm_announce_request_count __profile_stats_value(PR_VALUE_LM_ANNOUNCE_REQUEST, count)
#define lm_announce_request_time __profile_stats_value(PR_VALUE_LM_ANNOUNCE_REQUEST, time)

	PR_VALUE_DOMAIN_LOGON,
#define domain_logon_count __profile_stats_value(PR_VALUE_DOMAIN_LOGON, count)
#define domain_logon_time __profile_stats_value(PR_VALUE_DOMAIN_LOGON, time)

	PR_VALUE_SYNC_BROWSE_LISTS,
#define sync_browse_lists_count __profile_stats_value(PR_VALUE_SYNC_BROWSE_LISTS, count)
#define sync_browse_lists_time __profile_stats_value(PR_VALUE_SYNC_BROWSE_LISTS, time)

	PR_VALUE_RUN_ELECTIONS,
#define run_elections_count __profile_stats_value(PR_VALUE_RUN_ELECTIONS, count)
#define run_elections_time __profile_stats_value(PR_VALUE_RUN_ELECTIONS, time)

	PR_VALUE_ELECTION,
#define election_count __profile_stats_value(PR_VALUE_ELECTION, count)
#define election_time __profile_stats_value(PR_VALUE_ELECTION, time)

	/* This mist remain the last value. */
	PR_VALUE_MAX
}; /* enum profile_stats_values */

const char * profile_value_name(enum profile_stats_values val);

struct profile_stats {
/* general counters */
	unsigned smb_count; /* how many SMB packets we have processed */
	unsigned uid_changes; /* how many times we change our effective uid */

/* system call and protocol operation counters and cumulative times */
	unsigned count[PR_VALUE_MAX];
	unsigned time[PR_VALUE_MAX];

/* cumulative byte counts */
	unsigned syscall_pread_bytes;
	unsigned syscall_pwrite_bytes;
	unsigned syscall_read_bytes;
	unsigned syscall_write_bytes;
	unsigned syscall_sendfile_bytes;
	unsigned syscall_recvfile_bytes;

/* stat cache counters */
	unsigned statcache_lookups;
	unsigned statcache_misses;
	unsigned statcache_hits;

/* write cache counters */
	unsigned writecache_read_hits;
	unsigned writecache_abutted_writes;
	unsigned writecache_total_writes;
	unsigned writecache_non_oplock_writes;
	unsigned writecache_direct_writes;
	unsigned writecache_init_writes;
	unsigned writecache_flushed_writes[NUM_FLUSH_REASONS];
	unsigned writecache_num_perfect_writes;
	unsigned writecache_num_write_caches;
	unsigned writecache_allocated_write_caches;
};

struct profile_header {
	int prof_shm_magic;
	int prof_shm_version;
	struct profile_stats stats;
};

extern struct profile_header *profile_h;
extern struct profile_stats *profile_p;
extern bool do_profile_flag;
extern bool do_profile_times;

#ifdef WITH_PROFILE

/* these are helper macros - do not call them directly in the code
 * use the DO_PROFILE_* START_PROFILE and END_PROFILE ones
 * below which test for the profile flage first
 */
#define INC_PROFILE_COUNT(x) profile_p->x++
#define DEC_PROFILE_COUNT(x) profile_p->x--
#define ADD_PROFILE_COUNT(x,y) profile_p->x += (y)

#if defined(HAVE_CLOCK_GETTIME)

extern clockid_t __profile_clock;

static inline uint64_t profile_timestamp(void)
{
	struct timespec ts;

	/* FIXME: On a single-CPU system, or a system where we have bound
	 * daemon threads to single CPUs (eg. using cpusets or processor
	 * affinity), it might be preferable to use CLOCK_PROCESS_CPUTIME_ID.
	 */

	clock_gettime(__profile_clock, &ts);
	return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000); /* usec */
}

#else

static inline uint64_t profile_timestamp(void)
{
	struct timeval tv;
	GetTimeOfDay(&tv);
	return (tv.tv_sec * 1000000) + tv.tv_usec;
}

#endif

/* end of helper macros */

#define DO_PROFILE_INC(x) \
	if (do_profile_flag) { \
		INC_PROFILE_COUNT(x); \
	}

#define DO_PROFILE_DEC(x) \
	if (do_profile_flag) { \
		DEC_PROFILE_COUNT(x); \
	}

#define DO_PROFILE_DEC_INC(x,y) \
	if (do_profile_flag) { \
		DEC_PROFILE_COUNT(x); \
		INC_PROFILE_COUNT(y); \
	}

#define DO_PROFILE_ADD(x,n) \
	if (do_profile_flag) { \
		ADD_PROFILE_COUNT(x,n); \
	}

#define START_PROFILE(x) \
	uint64_t __profstamp_##x = 0; \
	if (do_profile_flag) { \
		__profstamp_##x = do_profile_times ? profile_timestamp() : 0;\
		INC_PROFILE_COUNT(x##_count); \
  	}

#define START_PROFILE_BYTES(x,n) \
	uint64_t __profstamp_##x = 0; \
	if (do_profile_flag) { \
		__profstamp_##x = do_profile_times ? profile_timestamp() : 0;\
		INC_PROFILE_COUNT(x##_count); \
		ADD_PROFILE_COUNT(x##_bytes, n); \
  	}

#define END_PROFILE(x) \
	if (do_profile_times) { \
		ADD_PROFILE_COUNT(x##_time, \
		    profile_timestamp() - __profstamp_##x); \
	}


#else /* WITH_PROFILE */

#define DO_PROFILE_INC(x)
#define DO_PROFILE_DEC(x)
#define DO_PROFILE_DEC_INC(x,y)
#define DO_PROFILE_ADD(x,n)
#define START_PROFILE(x)
#define START_PROFILE_BYTES(x,n)
#define END_PROFILE(x)

#endif /* WITH_PROFILE */

#endif
