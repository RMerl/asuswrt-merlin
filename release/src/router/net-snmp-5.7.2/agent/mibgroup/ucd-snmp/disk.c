/*
 * disk.c
 */

#include <net-snmp/net-snmp-config.h>

/* workaround for bug in autoconf 2.60b and 2.61 */
#ifdef HAVE_GETMNTENT
#undef HAVE_GETMNTENT
#define HAVE_GETMNTENT 1 /* previously might be only "#define HAVE_GETMNTENT" */
#endif

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#if !(defined(bsdi2) || defined(netbsd1))
#include <sys/vmmeter.h>
#endif
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#if HAVE_ASM_PAGE_H
#include <asm/page.h>
#endif
#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif
#if HAVE_SYS_FS_H
#include <sys/fs.h>
#else
#if HAVE_UFS_FS_H
#include <ufs/fs.h>
#else
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if !defined(dragonfly)
#ifdef HAVE_SYS_VNODE_H
#include <sys/vnode.h>
#endif
#endif
#ifdef HAVE_UFS_UFS_QUOTA_H
#include <ufs/ufs/quota.h>
#endif
#ifdef HAVE_UFS_UFS_INODE_H
#include <ufs/ufs/inode.h>
#endif
#if HAVE_UFS_FFS_FS_H
#include <ufs/ffs/fs.h>
#endif
#endif
#endif
#if HAVE_MTAB_H
#include <mtab.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if defined(__FreeBSD__) && __FreeBSD_version >= 700055 	/* Or HAVE_SYS_UCRED_H */
#include <sys/ucred.h>
#endif
#if defined(HAVE_STATFS)
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#if !defined(HAVE_STATVFS)
#define statvfs statfs
#endif
#endif
#if HAVE_VM_VM_H
#include <vm/vm.h>
#endif
#if HAVE_VM_SWAP_PAGER_H
#include <vm/swap_pager.h>
#endif
#if HAVE_SYS_FIXPOINT_H
#include <sys/fixpoint.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_FSTAB_H
#include <fstab.h>
#endif
#if HAVE_MNTENT_H
#include <mntent.h>
#endif
#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "struct.h"
#include "disk.h"
#include "util_funcs/header_simple_table.h"
#if USING_UCD_SNMP_ERRORMIB_MODULE
#include "errormib.h"
#else
#define setPerrorstatus(x) snmp_log_perror(x)
#endif

/*
 *  * config file parsing routines
 *   */
static void       disk_free_config(void);
static void       disk_parse_config(const char *, char *);
static void       disk_parse_config_all(const char *, char *);
#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS
static void       find_and_add_allDisks(int minpercent);
static void       add_device(char *path, char *device,
	                     int minspace, int minpercent, int override);
static void       modify_disk_parameters(int index, int minspace,
	                                 int minpercent);
static int        disk_exists(char *path);
static char      *find_device(char *path);
#endif

struct diskpart {
    char            device[STRMAX];
    char            path[STRMAX];
    int             minimumspace;
    int             minpercent;
};

#define MAX_INT_32 0x7fffffff
#define MAX_UINT_32 0xffffffff

unsigned int    numdisks;
int             allDisksIncluded = 0;
unsigned int    maxdisks = 0;
struct diskpart *disks;

struct variable2 extensible_disk_variables[] = {
  {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {MIBINDEX}},
  {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {ERRORNAME}},
  {DISKDEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKDEVICE}},
  {DISKMINIMUM, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKMINIMUM}},
  {DISKMINPERCENT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKMINPERCENT}},
  {DISKTOTAL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKTOTAL}},
  {DISKAVAIL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKAVAIL}},
  {DISKUSED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKUSED}},
  {DISKPERCENT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKPERCENT}},
  {DISKPERCENTNODE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {DISKPERCENTNODE}},
  {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {ERRORFLAG}},
  {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
   var_extensible_disk, 1, {ERRORMSG}},
   {DISKTOTALLOW, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKTOTALLOW}},
   {DISKTOTALHIGH, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKTOTALHIGH}},
   {DISKAVAILLOW, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKAVAILLOW}},
   {DISKAVAILHIGH, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKAVAILHIGH}},
   {DISKUSEDLOW, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKUSEDLOW}},
   {DISKUSEDHIGH, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
    var_extensible_disk, 1, {DISKUSEDHIGH}},
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath 
 */
oid             disk_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_DISKMIBNUM, 1 };

void
init_disk(void)
{
  /*
   * register ourselves with the agent to handle our mib tree 
   */
  REGISTER_MIB("ucd-snmp/disk", extensible_disk_variables, variable2,
	       disk_variables_oid);

  snmpd_register_config_handler("disk", disk_parse_config,
				disk_free_config,
				"path [ minspace | minpercent% ]");
  snmpd_register_config_handler("includeAllDisks", disk_parse_config_all,
				disk_free_config,
				"minpercent%");
  allDisksIncluded = 0;
}

static void
disk_free_config(void)
{
  unsigned int             i;

  numdisks = 0;
  for (i = 0; i < maxdisks; i++) {    /* init/erase disk db */
    disks[i].device[0] = 0;
    disks[i].path[0] = 0;
    disks[i].minimumspace = -1;
    disks[i].minpercent = -1;
  }
  allDisksIncluded = 0;
}

static void 
disk_parse_config(const char *token, char *cptr)
{
#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS
  char            path[STRMAX];
  int             minpercent;
  int             minspace;

  if (numdisks == maxdisks) {
      if (maxdisks == 0) {
          maxdisks = 50;
          disks = (struct diskpart *)malloc(maxdisks * sizeof(struct diskpart));
          if (!disks) {
              config_perror("malloc failed for new disk allocation.");
	      netsnmp_config_error("\tignoring:  %s", cptr);
              return;
          }
          memset(disks, 0, maxdisks * sizeof(struct diskpart));
      } else {
          maxdisks *= 2;
          disks = (struct diskpart *)realloc(disks, maxdisks * sizeof(struct diskpart));
          if (!disks) {
              config_perror("malloc failed for new disk allocation.");
	      netsnmp_config_error("\tignoring:  %s", cptr);
              return;
          }
          memset(disks + maxdisks/2, 0, maxdisks/2 * sizeof(struct diskpart));
      }
  }

  /*
   * read disk path (eg, /1 or /usr) 
   */
  copy_nword(cptr, path, sizeof(path));
  cptr = skip_not_white(cptr);
  cptr = skip_white(cptr);
	
  /*
   * read optional minimum disk usage spec 
   */
  if(cptr != NULL) {
      if(strchr(cptr, '%') == NULL) {
          minspace = atoi(cptr);
          minpercent = -1;
      }
      else {
          minspace = -1;
          minpercent = atoi(cptr);
      }
  } else {
      minspace = NETSNMP_DEFDISKMINIMUMSPACE;
      minpercent = -1;
  }

  /*
   * check if the disk already exists, if so then modify its
   * parameters. if it does not exist then add it
   */
  add_device(path, find_device(path), minspace, minpercent, 1);
#endif /* HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS */
}

static void 
disk_parse_config_all(const char *token, char *cptr)
{
#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS
  int             minpercent = DISKMINPERCENT;
    
  if (numdisks == maxdisks) {
      if (maxdisks == 0) {
          maxdisks = 50;
          disks = (struct diskpart *)malloc(maxdisks * sizeof(struct diskpart));
          if (!disks) {
              config_perror("malloc failed for new disk allocation.");
	      netsnmp_config_error("\tignoring:  %s", cptr);
              return;
          }
          memset(disks, 0, maxdisks * sizeof(struct diskpart));
      } else {
          maxdisks *= 2;
          disks = (struct diskpart *)realloc(disks, maxdisks * sizeof(struct diskpart));
          if (!disks) {
              config_perror("malloc failed for new disk allocation.");
	      netsnmp_config_error("\tignoring:  %s", cptr);
              return;
          }
          memset(disks + maxdisks/2, 0, maxdisks/2 * sizeof(struct diskpart));
      }
  }
  /*
   * read the minimum disk usage percent
   */
  if(cptr != NULL) {
      if(strchr(cptr, '%') != NULL) {
          minpercent = atoi(cptr);
      }
  }
  /*
   * if we have already seen the "includeAllDisks" directive
   * then search for the disk in the "disks" array and modify
   * the values. if we havent seen the "includeAllDisks"
   * directive then include this disk
   */
  if(allDisksIncluded) {
      config_perror("includeAllDisks already specified.");
      netsnmp_config_error("\tignoring: includeAllDisks %s", cptr);
  }
  else {
      allDisksIncluded = 1;
      find_and_add_allDisks(minpercent);
  }
#endif /* HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS */
}


#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS
static void
add_device(char *path, char *device, int minspace, int minpercent, int override) 
{
  int index;

  if (!path || !strcmp(path, "none")) {
    DEBUGMSGTL(("ucd-snmp/disk", "Skipping null path device (%s)\n", device));
    return;
  }
  if (numdisks == maxdisks) {
      if (maxdisks == 0) {
          maxdisks = 50;
          disks = (struct diskpart *)malloc(maxdisks * sizeof(struct diskpart));
          if (!disks) {
	      netsnmp_config_error("\tignoring:  %s", device);
              return;
          }
          memset(disks, 0, maxdisks * sizeof(struct diskpart));
      } else {
          maxdisks *= 2;
          disks = (struct diskpart *)realloc(disks, maxdisks * sizeof(struct diskpart));
          if (!disks) {
              config_perror("malloc failed for new disk allocation.");
	      netsnmp_config_error("\tignoring:  %s", device);
              return;
          }
          memset(disks + maxdisks/2, 0, maxdisks/2 * sizeof(struct diskpart));
      }
  }

  index = disk_exists(path);
  if((index != -1) && (index < maxdisks) && (override==1)) {
    modify_disk_parameters(index, minspace, minpercent);
  }
  else if(index == -1){
    /* add if and only if the device was found */
    if(device[0] != 0) {
      /* The following buffers are cleared above, no need to add '\0' */
      strlcpy(disks[numdisks].path, path, sizeof(disks[numdisks].path));
      strlcpy(disks[numdisks].device, device, sizeof(disks[numdisks].device));
      disks[numdisks].minimumspace = minspace;
      disks[numdisks].minpercent   = minpercent;
      numdisks++;  
    }
    else {
      disks[numdisks].minimumspace = -1;
      disks[numdisks].minpercent = -1;
      disks[numdisks].path[0] = 0;
      disks[numdisks].device[0] = 0;
    }
  }
}

void
modify_disk_parameters(int index, int minspace, int minpercent)
{
  disks[index].minimumspace = minspace;
  disks[index].minpercent   = minpercent;
}

int disk_exists(char *path) 
{
  unsigned int index;
  for(index = 0; index < numdisks; index++) {
    DEBUGMSGTL(("ucd-snmp/disk", "Checking for %s. Found %s at %d\n", path, disks[index].path, index));
    if(strcmp(path, disks[index].path) == 0) {
      return index;
    }
  }
  return -1;
}

static void 
find_and_add_allDisks(int minpercent)
{
#if HAVE_GETMNTENT
#if HAVE_SYS_MNTTAB_H
  struct mnttab   mnttab;
#else
  struct mntent  *mntent;
#endif
  FILE           *mntfp;
#elif HAVE_FSTAB_H
  struct fstab   *fstab1;
#elif HAVE_STATFS
  struct statfs   statf;
#endif
#if defined(HAVE_GETMNTENT) && !defined(HAVE_SETMNTENT)
  int             i;
#endif

#if defined(HAVE_GETMNTENT) || defined(HAVE_FSTAB_H)
  int dummy = 0;
#endif

  /* 
   * find the device for the path and copy the device into the
   * string declared above and at the end of the routine return it
   * to the caller 
   */
#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS   
#if HAVE_GETMNTENT
#if HAVE_SETMNTENT
  mntfp = setmntent(ETC_MNTTAB, "r");
  if (!mntfp) {
      netsnmp_config_error("Can't open %s (setmntent)\n", ETC_MNTTAB);
      return;
  }
  while (mntfp && NULL != (mntent = getmntent(mntfp))) {
    add_device(mntent->mnt_dir, mntent->mnt_fsname, -1, minpercent, 0);
    dummy = 1;
  }
  if (mntfp)
    endmntent(mntfp);
  if(dummy != 0) {
    /*
     * dummy clause for else below
     */
  }
#else                           /* getmentent but not setmntent */
  mntfp = fopen(ETC_MNTTAB, "r");
  if (!mntfp) {
      netsnmp_config_error("Can't open %s (fopen)\n", ETC_MNTTAB);
      return;
  }
  while ((i = getmntent(mntfp, &mnttab)) == 0) {
    add_device(mnttab.mnt_mountp, mnttab.mnt_special, -1, minpercent, 0);
    dummy = 1;
  }
  fclose(mntfp);
  if(dummy != 0) {
    /*
     * dummy clause for else below
     */
  }
#endif /* HAVE_SETMNTENT */
#elif HAVE_FSTAB_H
  setfsent();			/* open /etc/fstab */
  while((fstab1 = getfsent()) != NULL) {
    add_device(fstab1->fs_file, fstab1->fs_spec, -1, minpercent, 0);
    dummy = 1;
  }
  endfsent();			/* close /etc/fstab */
#if defined(__FreeBSD__) && __FreeBSD_version >= 700055
  {
    struct statfs *mntbuf;
    size_t i, mntsize;
    mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
    for (i = 0; i < mntsize; i++) {
      if (strncmp(mntbuf[i].f_fstypename, "zfs", 3) == 0) {
	add_device(mntbuf[i].f_mntonname, mntbuf[i].f_mntfromname, -1, minpercent, 0);
      }
    }
  }
#endif
  if(dummy != 0) {
    /*
     * dummy clause for else below
     */
  }
#elif HAVE_STATFS
  /*
   * since there is no way to get all the mounted systems with just
   * statfs we default to the root partition "/"
   */
  if (statfs("/", &statf) == 0) {
    add_device("/", statf.f_mntfromname, -1, minpercent, 0);
  }
#endif
  else {
    if (numdisks == maxdisks) {
      return;
    }
    netsnmp_config_warn("Couldn't find device for disk %s",
			disks[numdisks].path);
    disks[numdisks].minimumspace = -1;
    disks[numdisks].minpercent = -1;
    disks[numdisks].path[0] = 0;
  }
#else
  config_perror("'disk' checks not supported on this architecture.");
#endif                   /* HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS */  
 
}

static char *
find_device(char *path)
{
#if HAVE_GETMNTENT
#if HAVE_SYS_MNTTAB_H
  struct mnttab   mnttab;
#else
  struct mntent  *mntent;
#endif
  FILE           *mntfp;
#elif HAVE_FSTAB_H
  struct fstab   *fstab;
  struct stat     stat1;
#elif HAVE_STATFS
  struct statfs   statf;
#endif
  static char     device[STRMAX];
#if defined(HAVE_GETMNTENT) && !defined(HAVE_SETMNTENT)
  int             i;
#endif

  device[0] = '\0';		/* null terminate the device */


  /* find the device for the path and copy the device into the
   * string declared above and at the end of the routine return it
   * to the caller 
   */
#if HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS   
#if HAVE_GETMNTENT
#if HAVE_SETMNTENT
  mntfp = setmntent(ETC_MNTTAB, "r");
  if (!mntfp) {
      netsnmp_config_error("Can't open %s (setmntent)\n", ETC_MNTTAB);
      return NULL;
  }
  while (mntfp && NULL != (mntent = getmntent(mntfp)))
    if (strcmp(path, mntent->mnt_dir) == 0) {
      strlcpy(device, mntent->mnt_fsname, sizeof(device));
      DEBUGMSGTL(("ucd-snmp/disk", "Disk:  %s\n",
		  mntent->mnt_fsname));
      break;
    } else {
      DEBUGMSGTL(("ucd-snmp/disk", "  %s != %s\n",
		  path, mntent->mnt_dir));
    }
  if (mntfp)
    endmntent(mntfp);
#else                           /* getmentent but not setmntent */
  mntfp = fopen(ETC_MNTTAB, "r");
  if (!mntfp) {
      netsnmp_config_error("Can't open %s (fopen)\n", ETC_MNTTAB);
      return NULL;
  }
  while ((i = getmntent(mntfp, &mnttab)) == 0)
    if (strcmp(path, mnttab.mnt_mountp) == 0)
      break;
    else {
      DEBUGMSGTL(("ucd-snmp/disk", "  %s != %s\n",
		  path, mnttab.mnt_mountp));
    }
  fclose(mntfp);
  if (i == 0)
    strlcpy(device, mnttab.mnt_special, sizeof(device));
#endif /* HAVE_SETMNTENT */
#elif HAVE_FSTAB_H
  stat(path, &stat1);
  setfsent();
  if ((fstab = getfsfile(path)))
    strlcpy(device, fstab->fs_spec, sizeof(device));
  endfsent();
  if (device[0] != '\0') {
     /*
      * dummy clause for else below
      */
   }

#elif HAVE_STATFS
  if (statfs(path, &statf) == 0) {
    strlcpy(device, statf.f_mntfromname, sizeof(device));
    DEBUGMSGTL(("ucd-snmp/disk", "Disk:  %s\n",
		statf.f_mntfromname));
  }
#endif
  else {
    netsnmp_config_warn("Couldn't find device for disk %s", path);
  }
#else
  config_perror("'disk' checks not supported on this architecture.");
#endif                   /* HAVE_FSTAB_H || HAVE_GETMNTENT || HAVE_STATFS */  
  return device;
}
#endif

/*
 * Part of UCD-SNMP-MIB::dskEntry, which is so hard to fill 
 * (i.e. platform dependent parts).
 */
struct dsk_entry {
    unsigned long long  dskTotal;
    unsigned long long  dskUsed;
    unsigned long long  dskAvail;
    unsigned long       dskPercent;
    unsigned long       dskPercentInode;
    unsigned long       dskErrorFlag;
};

/**
 * Fill in the provided dsk_entry structure.
 * Returns -1 on error, 0 on success.
 */

static int
fill_dsk_entry(int disknum, struct dsk_entry *entry)
{
#if defined(HAVE_STATVFS) || defined(HAVE_STATFS)
    float           multiplier;
#endif
#if defined(HAVE_FSTAB_H) && !defined(HAVE_SYS_STATVFS_H) && !defined(HAVE_STATFS)
    double          totalblks, free, used, avail, availblks;
#endif

#if defined(HAVE_STATVFS) || defined(HAVE_STATFS)
#ifdef STAT_STATFS_FS_DATA
    struct fs_data  fsd;
    struct {
        u_int           f_blocks, f_bfree, f_bavail, f_bsize;
    } vfs;
#else
    struct statvfs  vfs;
#endif
#else
#if HAVE_FSTAB_H
    int             file;
    union {
        struct fs       iu_fs;
        char            dummy[SBSIZE];
    } sb;
#define filesys sb.iu_fs
#endif
#endif

    entry->dskPercentInode = -1;

#if defined(HAVE_STATVFS) || defined(HAVE_STATFS)
#ifdef STAT_STATFS_FS_DATA
    if (statvfs(disks[disknum].path, &fsd) == -1)
#else
    if (statvfs(disks[disknum].path, &vfs) == -1)
#endif
    {
        snmp_log(LOG_ERR, "Couldn't open device %s\n",
                 disks[disknum].device);
        setPerrorstatus("statvfs dev/disk");
        return -1;
    }
#ifdef STAT_STATFS_FS_DATA
    vfs.f_blocks = fsd.fd_btot;
    vfs.f_bfree = fsd.fd_bfree;
    vfs.f_bavail = fsd.fd_bfreen;
    vfs.f_bsize = 1024;         /*  Ultrix f_bsize is a VM parameter apparently.  */
#endif
#if defined(HAVE_ODS)
    vfs.f_blocks = vfs.f_spare[0];
    vfs.f_bfree = vfs.f_spare[1];
    vfs.f_bavail = vfs.f_spare[2];
#endif

    multiplier = (float)vfs.f_bsize / (float)1024.0;
#ifdef HAVE_STRUCT_STATVFS_F_FRSIZE
    if (vfs.f_frsize > 255)
        multiplier = (float)vfs.f_frsize / (float)1024.0;
#endif

    entry->dskTotal = (unsigned long long)(vfs.f_blocks * multiplier);
    entry->dskAvail = (unsigned long long)(vfs.f_bavail * multiplier);
    entry->dskUsed = (unsigned long long)((vfs.f_blocks - vfs.f_bfree) * multiplier);

    entry->dskPercent = 
        vfs.f_blocks == 0 ? 0 :
        vfs.f_bavail <= 0 ? 100 :
        (int) ((double) (vfs.f_blocks - vfs.f_bfree) /
               (double) (vfs.f_blocks -
                         (vfs.f_bfree - vfs.f_bavail)) * 100.0 + 0.5);

#if defined(HAVE_STRUCT_STATVFS_F_FILES) || defined HAVE_STRUCT_STATFS_F_FAVAIL
    entry->dskPercentInode = vfs.f_favail <= 0 ? 100 :
        (int) ((double) (vfs.f_files - vfs.f_ffree) /
               (double) (vfs.f_files -
                         (vfs.f_ffree - vfs.f_favail)) * 100.0 + 0.5);
#else
#if defined(HAVE_STRUCT_STATFS_F_FILES) && defined(HAVE_STRUCT_STATFS_F_FFREE)
    entry->dskPercentInode = vfs.f_files == 0 ? 100.0 :
      (int) ((double) (vfs.f_files - vfs.f_ffree) /
              (double) (vfs.f_files) * 100.0 + 0.5);
#endif 
#endif /* defined(HAVE_STRUCT_STATVFS_F_FILES) */

#elif HAVE_FSTAB_H
    /*
     * read the disk information 
     */
    if ((file = open(disks[disknum].device, 0)) < 0) {
        snmp_log(LOG_ERR, "Couldn't open device %s\n",
                 disks[disknum].device);
        setPerrorstatus("open dev/disk");
        return -1;
    }
    lseek(file, (long) (SBLOCK * DEV_BSIZE), 0);
    if (read(file, (char *) &filesys, SBSIZE) != SBSIZE) {
        setPerrorstatus("open dev/disk");
        snmp_log(LOG_ERR, "Error reading device %s\n",
                 disks[disknum].device);
        close(file);
        return -1;
    }
    close(file);

    totalblks = filesys.fs_dsize;
    free = filesys.fs_cstotal.cs_nbfree * filesys.fs_frag +
        filesys.fs_cstotal.cs_nffree;
    used = totalblks - free;
    availblks = totalblks * (100 - filesys.fs_minfree) / 100;
    avail = availblks > used ? availblks - used : 0;
    entry->dskPercent =
        totalblks == 0 ? 0 :
        availblks == 0 ? 100 :
        (int) ((double) used / (double) totalblks * 100.0 + 0.5);
    multiplier = (float)filesys.fs_fsize / (float)1024.0;
    entry->dskTotal = (unsigned long long)(totalblks * multiplier);
    entry->dskAvail = (unsigned long long)(avail * multiplier);
    entry->dskUsed = (unsigned long long)(used * multiplier);
#else
    /* MinGW */
    entry->dskPercent = 0;
    entry->dskTotal = 0;
    entry->dskAvail = 0;
    entry->dskUsed = 0;
#endif

    entry->dskErrorFlag =
        (disks[disknum].minimumspace >= 0
            ? entry->dskAvail < (unsigned long long)disks[disknum].minimumspace
            : 100 - entry->dskPercent <= (unsigned int)disks[disknum].minpercent) ? 1 : 0;

    return 0;
}

/*
 * var_extensible_disk(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */
u_char         *
var_extensible_disk(struct variable *vp,
                    oid * name,
                    size_t * length,
                    int exact,
                    size_t * var_len, WriteMethod ** write_method)
{
    int             ret;
	unsigned int	disknum = 0;
    struct dsk_entry entry;
    static long     long_ret;
    static char     errmsg[300];

tryAgain:
    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, numdisks))
        return (NULL);
    disknum = name[*length - 1] - 1;
	if (disknum > maxdisks)
		return NULL;
    switch (vp->magic) {
    case MIBINDEX:
        long_ret = disknum + 1;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* DISKPATH */
        *var_len = strlen(disks[disknum].path);
        return ((u_char *) disks[disknum].path);
    case DISKDEVICE:
        *var_len = strlen(disks[disknum].device);
        return ((u_char *) disks[disknum].device);
    case DISKMINIMUM:
        long_ret = disks[disknum].minimumspace;
        return ((u_char *) (&long_ret));
    case DISKMINPERCENT:
        long_ret = disks[disknum].minpercent;
        return ((u_char *) (&long_ret));
    }

    ret = fill_dsk_entry(disknum, &entry);
    if (ret < 0) {
        if (!exact)
            goto tryAgain;
        return NULL;
    }

    switch (vp->magic) {
    case DISKTOTAL:
        if (entry.dskTotal > MAX_INT_32)
            long_ret = MAX_INT_32;
        else
            long_ret = (long)(entry.dskTotal);
        return ((u_char *) (&long_ret));
    case DISKTOTALLOW:
        long_ret = entry.dskTotal & MAX_UINT_32;
        return ((u_char *) (&long_ret));
    case DISKTOTALHIGH:
        long_ret = entry.dskTotal >> 32;
        return ((u_char *) (&long_ret));
        
    case DISKAVAIL:
        if (entry.dskAvail > MAX_INT_32)
            long_ret = MAX_INT_32;
        else
            long_ret = (long)(entry.dskAvail);
        return ((u_char *) (&long_ret));
    case DISKAVAILLOW:
        long_ret = entry.dskAvail & MAX_UINT_32;
        return ((u_char *) (&long_ret));
    case DISKAVAILHIGH:
        long_ret = entry.dskAvail >> 32;
        return ((u_char *) (&long_ret));

    case DISKUSED:
        if (entry.dskUsed > MAX_INT_32)
            long_ret = MAX_INT_32;
        else
            long_ret = (long)(entry.dskUsed);
        return ((u_char *) (&long_ret));
    case DISKUSEDLOW:
        long_ret = entry.dskUsed & MAX_UINT_32;
        return ((u_char *) (&long_ret));
    case DISKUSEDHIGH:
        long_ret = entry.dskUsed >> 32;
        return ((u_char *) (&long_ret));

    case DISKPERCENT:
        long_ret = entry.dskPercent;
        return ((u_char *) (&long_ret));

    case DISKPERCENTNODE:
        long_ret = entry.dskPercentInode;
        return ((u_char *) (&long_ret));

    case ERRORFLAG:
        long_ret = entry.dskErrorFlag;
        return ((u_char *) (&long_ret));

    case ERRORMSG:
        if (entry.dskErrorFlag) {
            if (disks[disknum].minimumspace >= 0)
                snprintf(errmsg, sizeof(errmsg),
                        "%s: less than %d free (= %d)",
                        disks[disknum].path, disks[disknum].minimumspace,
                        (int) entry.dskAvail);
            else
                snprintf(errmsg, sizeof(errmsg),
                        "%s: less than %d%% free (= %d%%)",
                        disks[disknum].path, disks[disknum].minpercent,
                        (int)entry.dskPercent);
            errmsg[ sizeof(errmsg)-1 ] = 0;
        } else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    }
    return NULL;
}
