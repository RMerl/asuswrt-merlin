/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   support for quotas
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


/* 
 * This is one of the most system dependent parts of Samba, and its
 * done a litle differently. Each system has its own way of doing 
 * things :-(
 */

#include "includes.h"

extern int DEBUGLEVEL;

#if defined(VXFS_QUOTA)

/*
 * In addition to their native filesystems, some systems have Veritas VxFS.
 * Declare here, define at end: reduces likely "include" interaction problems.
 *      David Lee <T.D.Lee@durham.ac.uk>
 */
static BOOL disk_quotas_vxfs(const pstring name, char *path,
			     SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, 
			     SMB_BIG_UINT *dsize);

#endif /* VXFS_QUOTA */   
 
#ifdef LINUX

#include <sys/types.h>
#include <asm/types.h>
#include <sys/quota.h>

#include <mntent.h>
#include <linux/unistd.h>

_syscall4(int, quotactl, int, cmd, const char *, special, int, id, caddr_t, addr);

/****************************************************************************
try to get the disk space from disk quotas (LINUX version)
****************************************************************************/

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  int r;
  struct dqblk D;
  SMB_STRUCT_STAT S;
  FILE *fp;
  struct mntent *mnt;
  SMB_DEV_T devno;
  int found;
  uid_t euser_id;

  euser_id = geteuid();
  
  /* find the block device file */
  
  if ( sys_stat(path, &S) == -1 ) {
    return(False) ;
  }

  devno = S.st_dev ;
  
  fp = setmntent(MOUNTED,"r");
  found = False ;
  
  while ((mnt = getmntent(fp))) {
    if ( sys_stat(mnt->mnt_dir,&S) == -1 )
      continue ;
    if (S.st_dev == devno) {
      found = True ;
      break ;
    }
  }
  endmntent(fp) ;
  
  if (!found) {
      return(False);
    }

  save_re_uid();
  set_effective_uid(0);  
  r=quotactl(QCMD(Q_GETQUOTA,USRQUOTA), mnt->mnt_fsname, euser_id, (caddr_t)&D);
  restore_re_uid();

  /* Use softlimit to determine disk space, except when it has been exceeded */
  *bsize = 1024;
  if (r)
    {
      if (errno == EDQUOT) 
       {
         *dfree =0;
         *dsize =D.dqb_curblocks;
         return (True);
    }
      else return(False);
  }
  /* Use softlimit to determine disk space, except when it has been exceeded */
  if (
      (D.dqb_bsoftlimit && D.dqb_curblocks>=D.dqb_bsoftlimit) ||
      (D.dqb_bhardlimit && D.dqb_curblocks>=D.dqb_bhardlimit) ||
      (D.dqb_isoftlimit && D.dqb_curinodes>=D.dqb_isoftlimit) ||
      (D.dqb_ihardlimit && D.dqb_curinodes>=D.dqb_ihardlimit)
     )
    {
      *dfree = 0;
      *dsize = D.dqb_curblocks;
    }
  else if (D.dqb_bsoftlimit==0 && D.dqb_bhardlimit==0)
    {
      return(False);
    }
  else {
    if (D.dqb_bsoftlimit == 0)
      D.dqb_bsoftlimit = D.dqb_bhardlimit;
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
    *dsize = D.dqb_bsoftlimit;
  }
  return (True);
}

#elif defined(CRAY)

#include <sys/quota.h>
#include <mntent.h>

/****************************************************************************
try to get the disk space from disk quotas (CRAY VERSION)
****************************************************************************/

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  struct mntent *mnt;
  FILE *fd;
  SMB_STRUCT_STAT sbuf;
  SMB_DEV_T devno ;
  static SMB_DEV_T devno_cached = 0 ;
  static pstring name;
  struct q_request request ;
  struct qf_header header ;
  static int quota_default = 0 ;
  int found ;
  
  if ( sys_stat(path,&sbuf) == -1 )
    return(False) ;
  
  devno = sbuf.st_dev ;
  
  if ( devno != devno_cached ) {
    
    devno_cached = devno ;
    
    if ((fd = setmntent(KMTAB)) == NULL)
      return(False) ;
    
    found = False ;
    
    while ((mnt = getmntent(fd)) != NULL) {
      
      if ( sys_stat(mnt->mnt_dir,&sbuf) == -1 )
	continue ;
      
      if (sbuf.st_dev == devno) {
	
	found = True ;
	break ;
	
      }
      
    }
    
    pstrcpy(name,mnt->mnt_dir) ;
    endmntent(fd) ;
    
    if ( ! found )
      return(False) ;
  }
  
  request.qf_magic = QF_MAGIC ;
  request.qf_entry.id = geteuid() ;
  
  if (quotactl(name, Q_GETQUOTA, &request) == -1)
    return(False) ;
  
  if ( ! request.user )
    return(False) ;
  
  if ( request.qf_entry.user_q.f_quota == QFV_DEFAULT ) {
    
    if ( ! quota_default ) {
      
      if ( quotactl(name, Q_GETHEADER, &header) == -1 )
	return(False) ;
      else
	quota_default = header.user_h.def_fq ;
    }
    
    *dfree = quota_default ;
    
  }else if ( request.qf_entry.user_q.f_quota == QFV_PREVENT ) {
    
    *dfree = 0 ;
    
  }else{
    
    *dfree = request.qf_entry.user_q.f_quota ;
    
  }
  
  *dsize = request.qf_entry.user_q.f_use ;
  
  if ( *dfree < *dsize )
    *dfree = 0 ;
  else
    *dfree -= *dsize ;
  
  *bsize = 4096 ;  /* Cray blocksize */
  
  return(True) ;
  
}


#elif defined(SUNOS5) || defined(SUNOS4)

#include <fcntl.h>
#include <sys/param.h>
#if defined(SUNOS5)
#include <sys/fs/ufs_quota.h>
#include <sys/mnttab.h>
#else /* defined(SUNOS4) */
#include <ufs/quota.h>
#include <mntent.h>
#endif

/****************************************************************************
try to get the disk space from disk quotas (SunOS & Solaris2 version)
Quota code by Peter Urbanec (amiga@cse.unsw.edu.au).
****************************************************************************/

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  uid_t euser_id;
  int ret;
  struct dqblk D;
#if defined(SUNOS5)
  struct quotctl command;
  int file;
  static struct mnttab mnt;
  static pstring name;
#else /* SunOS4 */
  struct mntent *mnt;
  static pstring name;
#endif
  FILE *fd;
  SMB_STRUCT_STAT sbuf;
  SMB_DEV_T devno ;
  static SMB_DEV_T devno_cached = 0 ;
  int found ;

  euser_id = geteuid();
  
  if ( sys_stat(path,&sbuf) == -1 )
    return(False) ;
  
  devno = sbuf.st_dev ;
  DEBUG(5,("disk_quotas: looking for path \"%s\" devno=%o\n", path,devno));
  if ( devno != devno_cached ) {
    devno_cached = devno ;
#if defined(SUNOS5)
    if ((fd = sys_fopen(MNTTAB, "r")) == NULL)
      return(False) ;
    
    found = False ;
    while (getmntent(fd, &mnt) == 0) {
      if ( sys_stat(mnt.mnt_mountp,&sbuf) == -1 )
	continue ;
      DEBUG(5,("disk_quotas: testing \"%s\" devno=%o\n", 
	       mnt.mnt_mountp,sbuf.st_dev));
      if (sbuf.st_dev == devno) {
	found = True ;
	break ;
      }
    }
    
    pstrcpy(name,mnt.mnt_mountp) ;
    pstrcat(name,"/quotas") ;
    fclose(fd) ;
#else /* SunOS4 */
    if ((fd = setmntent(MOUNTED, "r")) == NULL)
      return(False) ;
    
    found = False ;
    while ((mnt = getmntent(fd)) != NULL) {
      if ( sys_stat(mnt->mnt_dir,&sbuf) == -1 )
	continue ;
      DEBUG(5,("disk_quotas: testing \"%s\" devno=%o\n", 
	       mnt->mnt_dir,sbuf.st_dev));
      if (sbuf.st_dev == devno) {
	found = True ;
	break ;
      }
    }
    
    pstrcpy(name,mnt->mnt_fsname) ;
    endmntent(fd) ;
#endif
    
    if ( ! found )
      return(False) ;
  }


  save_re_uid();
  set_effective_uid(0);

#if defined(SUNOS5)
  DEBUG(5,("disk_quotas: looking for quotas file \"%s\"\n", name));
  if((file=sys_open(name, O_RDONLY,0))<0) {
	  restore_re_uid();
	  return(False);
  }
  command.op = Q_GETQUOTA;
  command.uid = euser_id;
  command.addr = (caddr_t) &D;
  ret = ioctl(file, Q_QUOTACTL, &command);
  close(file);
#else
  DEBUG(5,("disk_quotas: trying quotactl on device \"%s\"\n", name));
  ret = quotactl(Q_GETQUOTA, name, euser_id, &D);
#endif

  restore_re_uid();

  if (ret < 0) {
    DEBUG(5,("disk_quotas ioctl (Solaris) failed. Error = %s\n", strerror(errno) ));
#if defined(SUNOS5) && defined(VXFS_QUOTA)
    /* If normal quotactl() fails, try vxfs private calls */
    set_effective_uid(euser_id);
    DEBUG(5,("disk_quotas: VXFS_QUOTA: mount type \"%s\"\n", mnt.mnt_fstype));
    if ( 0 == strcmp ( mnt.mnt_fstype, "vxfs" )) {
      DEBUG(5,("disk_quotas: Trying VxFS quotactl call..\n"));
      ret = disk_quotas_vxfs(name, path, bsize, dfree, dsize);
      
      return(ret);
    }
#endif

    return(False);
  }

  /* If softlimit is zero, set it equal to hardlimit.
   */
  
  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  /* Use softlimit to determine disk space. A user exceeding the quota is told
   * that there's no space left. Writes might actually work for a bit if the
   * hardlimit is set higher than softlimit. Effectively the disk becomes
   * made of rubber latex and begins to expand to accommodate the user :-)
   */

  if (D.dqb_bsoftlimit==0)
    return(False);
  *bsize = DEV_BSIZE;
  *dsize = D.dqb_bsoftlimit;

  if (D.dqb_curblocks > D.dqb_bsoftlimit) {
     *dfree = 0;
     *dsize = D.dqb_curblocks;
  } else
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
      
  DEBUG(5,("disk_quotas for path \"%s\" returning  bsize %.0f, dfree %.0f, dsize %.0f\n",
         path,(double)*bsize,(double)*dfree,(double)*dsize));

      return(True);
}


#elif defined(OSF1)
#include <ufs/quota.h>

/****************************************************************************
try to get the disk space from disk quotas - OSF1 version
****************************************************************************/

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  int r, save_errno;
  struct dqblk D;
  SMB_STRUCT_STAT S;
  uid_t euser_id;

  /*
   * This code presumes that OSF1 will only
   * give out quota info when the real uid 
   * matches the effective uid. JRA.
   */
  euser_id = geteuid();
  save_re_uid();
  if (set_re_uid() != 0) return False;

  r= quotactl(path,QCMD(Q_GETQUOTA, USRQUOTA),euser_id,(char *) &D);
  if (r) {
     save_errno = errno;
  }

  restore_re_uid();

  *bsize = DEV_BSIZE;

  if (r)
  {
      if (save_errno == EDQUOT)   // disk quota exceeded
      {
         *dfree = 0;
         *dsize = D.dqb_curblocks;
         return (True);
      }
      else
         return (False);  
  }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  /* Use softlimit to determine disk space, except when it has been exceeded */

  if (D.dqb_bsoftlimit==0)
    return(False);

  if ((D.dqb_curblocks>D.dqb_bsoftlimit)) {
    *dfree = 0;
    *dsize = D.dqb_curblocks;
  } else {
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
    *dsize = D.dqb_bsoftlimit;
  }
  return (True);
}

#elif defined (IRIX6)
/****************************************************************************
try to get the disk space from disk quotas (IRIX 6.2 version)
****************************************************************************/

#include <sys/quota.h>
#include <mntent.h>

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  uid_t euser_id;
  int r;
  struct dqblk D;
  struct fs_disk_quota        F;
  SMB_STRUCT_STAT S;
  FILE *fp;
  struct mntent *mnt;
  SMB_DEV_T devno;
  int found;
  
  /* find the block device file */
  
  if ( sys_stat(path, &S) == -1 ) {
    return(False) ;
  }

  devno = S.st_dev ;
  
  fp = setmntent(MOUNTED,"r");
  found = False ;
  
  while ((mnt = getmntent(fp))) {
    if ( sys_stat(mnt->mnt_dir,&S) == -1 )
      continue ;
    if (S.st_dev == devno) {
      found = True ;
      break ;
    }
  }
  endmntent(fp) ;
  
  if (!found) {
    return(False);
  }

  euser_id=geteuid();
  save_re_uid();
  set_effective_uid(0);  

  /* Use softlimit to determine disk space, except when it has been exceeded */

  *bsize = 512;

  if ( 0 == strcmp ( mnt->mnt_type, "efs" ))
  {
    r=quotactl (Q_GETQUOTA, mnt->mnt_fsname, euser_id, (caddr_t) &D);

    restore_re_uid();

    if (r==-1)
      return(False);
        
    /* Use softlimit to determine disk space, except when it has been exceeded */
    if (
        (D.dqb_bsoftlimit && D.dqb_curblocks>=D.dqb_bsoftlimit) ||
        (D.dqb_bhardlimit && D.dqb_curblocks>=D.dqb_bhardlimit) ||
        (D.dqb_fsoftlimit && D.dqb_curfiles>=D.dqb_fsoftlimit) ||
        (D.dqb_fhardlimit && D.dqb_curfiles>=D.dqb_fhardlimit)
       )
    {
      *dfree = 0;
      *dsize = D.dqb_curblocks;
    }
    else if (D.dqb_bsoftlimit==0 && D.dqb_bhardlimit==0)
    {
      return(False);
    }
    else 
    {
      *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
      *dsize = D.dqb_bsoftlimit;
    }

  }
  else if ( 0 == strcmp ( mnt->mnt_type, "xfs" ))
  {
    r=quotactl (Q_XGETQUOTA, mnt->mnt_fsname, euser_id, (caddr_t) &F);

    restore_re_uid();

    if (r==-1)
      return(False);
        
    /* Use softlimit to determine disk space, except when it has been exceeded */
    if (
        (F.d_blk_softlimit && F.d_bcount>=F.d_blk_softlimit) ||
        (F.d_blk_hardlimit && F.d_bcount>=F.d_blk_hardlimit) ||
        (F.d_ino_softlimit && F.d_icount>=F.d_ino_softlimit) ||
        (F.d_ino_hardlimit && F.d_icount>=F.d_ino_hardlimit)
       )
    {
      *dfree = 0;
      *dsize = F.d_bcount;
    }
    else if (F.d_blk_softlimit==0 && F.d_blk_hardlimit==0)
    {
      return(False);
    }
    else 
    {
      *dfree = (F.d_blk_softlimit - F.d_bcount);
      *dsize = F.d_blk_softlimit;
    }

  }
  else
  {
	  restore_re_uid();
	  return(False);
  }

  return (True);

}

#else

#if    defined(__FreeBSD__) || defined(__OpenBSD__)
#include <ufs/ufs/quota.h>
#include <machine/param.h>
#elif         AIX
/* AIX quota patch from Ole Holm Nielsen <ohnielse@fysik.dtu.dk> */
#include <jfs/quota.h>
/* AIX 4.X: Rename members of the dqblk structure (ohnielse@fysik.dtu.dk) */
#define dqb_curfiles dqb_curinodes
#define dqb_fhardlimit dqb_ihardlimit
#define dqb_fsoftlimit dqb_isoftlimit
#else /* !__FreeBSD__ && !AIX && !__OpenBSD__ */
#include <sys/quota.h>
#include <devnm.h>
#endif

/****************************************************************************
try to get the disk space from disk quotas - default version
****************************************************************************/

BOOL disk_quotas(char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  int r;
  struct dqblk D;
  uid_t euser_id;
#if !defined(__FreeBSD__) && !defined(AIX) && !defined(__OpenBSD__)
  char dev_disk[256];
  SMB_STRUCT_STAT S;
  /* find the block device file */
  if ((sys_stat(path, &S)<0) ||
      (devnm(S_IFBLK, S.st_dev, dev_disk, 256, 0)<0)) return (False);
#endif

  euser_id = geteuid();

#ifdef HPUX
  /* for HPUX, real uid must be same as euid to execute quotactl for euid */
  save_re_uid();
  if (set_re_uid() != 0) return False;
  
  r=quotactl(Q_GETQUOTA, dev_disk, euser_id, &D);

  restore_re_uid();
#else 
#if defined(__FreeBSD__) || defined(__OpenBSD__)
  {
    /* FreeBSD patches from Marty Moll <martym@arbor.edu> */
    gid_t egrp_id;
 
    save_re_uid();
    set_effective_uid(0);

    egrp_id = getegid();
    r= quotactl(path,QCMD(Q_GETQUOTA,USRQUOTA),euser_id,(char *) &D);

    /* As FreeBSD has group quotas, if getting the user
       quota fails, try getting the group instead. */
    if (r) {
	    r= quotactl(path,QCMD(Q_GETQUOTA,GRPQUOTA),egrp_id,(char *) &D);
    }

    restore_re_uid();
  }
#elif defined(AIX)
  /* AIX has both USER and GROUP quotas: 
     Get the USER quota (ohnielse@fysik.dtu.dk) */
  r= quotactl(path,QCMD(Q_GETQUOTA,USRQUOTA),euser_id,(char *) &D);
#else /* !__FreeBSD__ && !AIX && !__OpenBSD__ */
  r=quotactl(Q_GETQUOTA, dev_disk, euser_id, &D);
#endif /* !__FreeBSD__ && !AIX && !__OpenBSD__ */
#endif /* HPUX */

  /* Use softlimit to determine disk space, except when it has been exceeded */
#if defined(__FreeBSD__) || defined(__OpenBSD__)
  *bsize = DEV_BSIZE;
#else /* !__FreeBSD__ && !__OpenBSD__ */
  *bsize = 1024;
#endif /*!__FreeBSD__ && !__OpenBSD__ */

  if (r)
    {
      if (errno == EDQUOT) 
	{
 	  *dfree =0;
 	  *dsize =D.dqb_curblocks;
 	  return (True);
	}
      else return(False);
    }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  if (D.dqb_bsoftlimit==0)
    return(False);
  /* Use softlimit to determine disk space, except when it has been exceeded */
  if ((D.dqb_curblocks>D.dqb_bsoftlimit)
#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
||((D.dqb_curfiles>D.dqb_fsoftlimit) && (D.dqb_fsoftlimit != 0))
#endif
    ) {
      *dfree = 0;
      *dsize = D.dqb_curblocks;
    }
  else {
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
    *dsize = D.dqb_bsoftlimit;
  }
  return (True);
}

#endif

#if defined(VXFS_QUOTA)

/****************************************************************************
Try to get the disk space from Veritas disk quotas.
    David Lee <T.D.Lee@durham.ac.uk> August 1999.

Warning:
    It is understood that Veritas do not publicly support this ioctl interface.
    Rather their preference would be for the user (us) to call the native
    OS and then for the OS itself to call through to the VxFS filesystem.
    Presumably HPUX 10 does this, which is why this patch is not required for
    HPUX 10.

****************************************************************************/
/*************************************************************************** 
   VERITAS Disclaimer:
   The following code uses private VxFS interfaces that are subject
   to change without notice. It is distributed in the hope that it will
   be useful, but WITHOUT ANY WARRANTY whatsoever. 
   VERITAS disclaims all liability resulting from use of this code.
   VERITAS is under no obligation to maintain these private
   interfaces in future releases of VERITAS products. 
 ***************************************************************************/

#if defined(SUNOS5)
#include <sys/fs/vx_solaris.h>
#endif
#include <sys/fs/vx_machdep.h>
#include <sys/fs/vx_layout.h>
#include <sys/fs/vx_quota.h>
#include <sys/fs/vx_aioctl.h>
#include <sys/fs/vx_ioctl.h>

static BOOL disk_quotas_vxfs(const pstring name, char *path, SMB_BIG_UINT *bsize, SMB_BIG_UINT *dfree, SMB_BIG_UINT *dsize)
{
  uid_t user_id, euser_id;
  int ret;
  struct vx_dqblk D;
  struct vx_quotctl quotabuf;
  struct vx_genioctl genbuf;
  pstring qfname;
  int file;

  /*
   * "name" may or may not include a trailing "/quotas".
   * Arranging consistency of calling here in "quotas.c" may not be easy and
   * it might be easier to examine and adjust it here.
   * Fortunately, VxFS seems not to mind at present.
   */
  pstrcpy(qfname, name) ;
  /* pstrcat(qfname, "/quotas") ; */	/* possibly examine and adjust "name" */

  euser_id = geteuid();
  set_effective_uid(0);

  DEBUG(5,("disk_quotas: looking for VxFS quotas file \"%s\"\n", qfname));
  if((file=sys_open(qfname, O_RDONLY,0))<0) {
    set_effective_uid(euser_id);
    return(False);
  }
  genbuf.ioc_cmd = VX_QUOTACTL;
  genbuf.ioc_up = (void *) &quotabuf;

  quotabuf.cmd = VX_GETQUOTA;
  quotabuf.uid = euser_id;
  quotabuf.addr = (caddr_t) &D;
  ret = ioctl(file, VX_ADMIN_IOCTL, &genbuf);
  close(file);

  set_effective_uid(euser_id);

  if (ret < 0) {
    DEBUG(5,("disk_quotas ioctl (VxFS) failed. Error = %s\n", strerror(errno) ));
    return(False);
  }

  /* If softlimit is zero, set it equal to hardlimit.
   */

  if (D.dqb_bsoftlimit==0)
    D.dqb_bsoftlimit = D.dqb_bhardlimit;

  /* Use softlimit to determine disk space. A user exceeding the quota is told
   * that there's no space left. Writes might actually work for a bit if the
   * hardlimit is set higher than softlimit. Effectively the disk becomes
   * made of rubber latex and begins to expand to accommodate the user :-)
   */
  DEBUG(5,("disk_quotas for path \"%s\" block c/s/h %ld/%ld/%ld; file c/s/h %ld/%ld/%ld\n",
         path, D.dqb_curblocks, D.dqb_bsoftlimit, D.dqb_bhardlimit,
         D.dqb_curfiles, D.dqb_fsoftlimit, D.dqb_fhardlimit));

  if (D.dqb_bsoftlimit==0)
    return(False);
  *bsize = DEV_BSIZE;
  *dsize = D.dqb_bsoftlimit;

  if (D.dqb_curblocks > D.dqb_bsoftlimit) {
     *dfree = 0;
     *dsize = D.dqb_curblocks;
  } else
    *dfree = D.dqb_bsoftlimit - D.dqb_curblocks;
      
  DEBUG(5,("disk_quotas for path \"%s\" returning  bsize %.0f, dfree %.0f, dsize %.0f\n",
         path,(double)*bsize,(double)*dfree,(double)*dsize));

  return(True);
}

#endif /* VXFS_QUOTA */
