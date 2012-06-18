/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba system utilities
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

#include "includes.h"

extern int DEBUGLEVEL;

/*
   The idea is that this file will eventually have wrappers around all
   important system calls in samba. The aims are:

   - to enable easier porting by putting OS dependent stuff in here

   - to allow for hooks into other "pseudo-filesystems"

   - to allow easier integration of things like the japanese extensions

   - to support the philosophy of Samba to expose the features of
     the OS within the SMB model. In general whatever file/printer/variable
     expansions/etc make sense to the OS should be acceptable to Samba.
*/


/*******************************************************************
this replaces the normal select() system call
return if some data has arrived on one of the file descriptors
return -1 means error
********************************************************************/
#ifndef HAVE_SELECT
static int pollfd(int fd)
{
  int     r=0;

#ifdef HAS_RDCHK
  r = rdchk(fd);
#elif defined(TCRDCHK)
  (void)ioctl(fd, TCRDCHK, &r);
#else
  (void)ioctl(fd, FIONREAD, &r);
#endif

  return(r);
}

int sys_select(int maxfd, fd_set *fds,struct timeval *tval)
{
  fd_set fds2;
  int counter=0;
  int found=0;

  FD_ZERO(&fds2);

  while (1) 
  {
    int i;
    for (i=0;i<maxfd;i++) {
      if (FD_ISSET(i,fds) && pollfd(i)>0) {
        found++;
        FD_SET(i,&fds2);
      }
    }

    if (found) {
      memcpy((void *)fds,(void *)&fds2,sizeof(fds2));
      return(found);
    }
      
    if (tval && tval->tv_sec < counter) return(0);
      sleep(1);
      counter++;
  }
}

#else /* !NO_SELECT */
int sys_select(int maxfd, fd_set *fds,struct timeval *tval)
{
#ifdef USE_POLL
  struct pollfd pfd[256];
  int i;
  int maxpoll;
  int timeout;
  int pollrtn;

  maxpoll = 0;
  for( i = 0; i < maxfd; i++) {
    if(FD_ISSET(i,fds)) {
      struct pollfd *pfdp = &pfd[maxpoll++];
      pfdp->fd = i;
      pfdp->events = POLLIN;
      pfdp->revents = 0;
    }
  }

  timeout = (tval != NULL) ? (tval->tv_sec * 1000) + (tval->tv_usec/1000) :
                -1;
  errno = 0;
  do {
    pollrtn = poll( &pfd[0], maxpoll, timeout);
  } while (pollrtn<0 && errno == EINTR);

  FD_ZERO(fds);

  for( i = 0; i < maxpoll; i++)
    if( pfd[i].revents & POLLIN )
      FD_SET(pfd[i].fd,fds);

  return pollrtn;
#else /* USE_POLL */

  struct timeval t2;
  int selrtn;

  do {
    if (tval) memcpy((void *)&t2,(void *)tval,sizeof(t2));
    errno = 0;
    selrtn = select(maxfd,SELECT_CAST fds,NULL,NULL,tval?&t2:NULL);
  } while (selrtn<0 && errno == EINTR);

  return(selrtn);
}
#endif /* USE_POLL */
#endif /* NO_SELECT */

/*******************************************************************
 A wrapper for usleep in case we don't have one.
********************************************************************/

int sys_usleep(long usecs)
{
#ifndef HAVE_USLEEP
  struct timeval tval;
#endif

  /*
   * We need this braindamage as the glibc usleep
   * is not SPEC1170 complient... grumble... JRA.
   */

  if(usecs < 0 || usecs > 1000000) {
    errno = EINVAL;
    return -1;
  }

#if HAVE_USLEEP
  usleep(usecs);
  return 0;
#else /* HAVE_USLEEP */
  /*
   * Fake it with select...
   */
  tval.tv_sec = 0;
  tval.tv_usec = usecs/1000;
  select(0,NULL,NULL,NULL,&tval);
  return 0;
#endif /* HAVE_USLEEP */
}

/*******************************************************************
A stat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_stat(const char *fname,SMB_STRUCT_STAT *sbuf)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_STAT64)
	ret = stat64(fname, sbuf);
#else
	ret = stat(fname, sbuf);
#endif
	/* we always want directories to appear zero size */
	if (ret == 0 && S_ISDIR(sbuf->st_mode)) sbuf->st_size = 0;
	return ret;
}

/*******************************************************************
 An fstat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_fstat(int fd,SMB_STRUCT_STAT *sbuf)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_FSTAT64)
	ret = fstat64(fd, sbuf);
#else
	ret = fstat(fd, sbuf);
#endif
	/* we always want directories to appear zero size */
	if (ret == 0 && S_ISDIR(sbuf->st_mode)) sbuf->st_size = 0;
	return ret;
}

/*******************************************************************
 An lstat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_lstat(const char *fname,SMB_STRUCT_STAT *sbuf)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_LSTAT64)
	ret = lstat64(fname, sbuf);
#else
	ret = lstat(fname, sbuf);
#endif
	/* we always want directories to appear zero size */
	if (ret == 0 && S_ISDIR(sbuf->st_mode)) sbuf->st_size = 0;
	return ret;
}

/*******************************************************************
 An ftruncate() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_ftruncate(int fd, SMB_OFF_T offset)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_FTRUNCATE64)
  return ftruncate64(fd, offset);
#else
  return ftruncate(fd, offset);
#endif
}

/*******************************************************************
 An lseek() wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_OFF_T sys_lseek(int fd, SMB_OFF_T offset, int whence)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_LSEEK64)
  return lseek64(fd, offset, whence);
#else
  return lseek(fd, offset, whence);
#endif
}

/*******************************************************************
 An fseek() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_fseek(FILE *fp, SMB_OFF_T offset, int whence)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FSEEK64)
  return fseek64(fp, offset, whence);
#elif defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FSEEKO64)
  return fseeko64(fp, offset, whence);
#else
  return fseek(fp, offset, whence);
#endif
}

/*******************************************************************
 An ftell() wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_OFF_T sys_ftell(FILE *fp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FTELL64)
  return (SMB_OFF_T)ftell64(fp);
#elif defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FTELLO64)
  return (SMB_OFF_T)ftello64(fp);
#else
  return (SMB_OFF_T)ftell(fp);
#endif
}

/*******************************************************************
 A creat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_creat(const char *path, mode_t mode)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_CREAT64)
  return creat64(path, mode);
#else
  /*
   * If creat64 isn't defined then ensure we call a potential open64.
   * JRA.
   */
  return sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
#endif
}

/*******************************************************************
 An open() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_open(const char *path, int oflag, mode_t mode)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OPEN64)
  return open64(path, oflag, mode);
#else
  return open(path, oflag, mode);
#endif
}

/*******************************************************************
 An fopen() wrapper that will deal with 64 bit filesizes.
********************************************************************/

FILE *sys_fopen(const char *path, const char *type)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_FOPEN64)
  return fopen64(path, type);
#else
  return fopen(path, type);
#endif
}

#if defined(HAVE_MMAP)

/*******************************************************************
 An mmap() wrapper that will deal with 64 bit filesizes.
********************************************************************/

void *sys_mmap(void *addr, size_t len, int prot, int flags, int fd, SMB_OFF_T offset)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_MMAP64)
  return mmap64(addr, len, prot, flags, fd, offset);
#else
  return mmap(addr, len, prot, flags, fd, offset);
#endif
}

#endif /* HAVE_MMAP */

/*******************************************************************
 A readdir wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_STRUCT_DIRENT *sys_readdir(DIR *dirp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_READDIR64)
  return readdir64(dirp);
#else
  return readdir(dirp);
#endif
}

/*******************************************************************
The wait() calls vary between systems
********************************************************************/

int sys_waitpid(pid_t pid,int *status,int options)
{
#ifdef HAVE_WAITPID
  return waitpid(pid,status,options);
#else /* HAVE_WAITPID */
  return wait4(pid, status, options, NULL);
#endif /* HAVE_WAITPID */
}

/*******************************************************************
system wrapper for getwd
********************************************************************/
char *sys_getwd(char *s)
{
    char *wd;
#ifdef HAVE_GETCWD
    wd = (char *)getcwd(s, sizeof (pstring));
#else
    wd = (char *)getwd(s);
#endif
    return wd;
}

/*******************************************************************
chown isn't used much but OS/2 doesn't have it
********************************************************************/

int sys_chown(const char *fname,uid_t uid,gid_t gid)
{
#ifndef HAVE_CHOWN
	static int done;
	if (!done) {
		DEBUG(1,("WARNING: no chown!\n"));
		done=1;
	}
#else
	return(chown(fname,uid,gid));
#endif
}

/*******************************************************************
os/2 also doesn't have chroot
********************************************************************/
int sys_chroot(const char *dname)
{
#ifndef HAVE_CHROOT
	static int done;
	if (!done) {
		DEBUG(1,("WARNING: no chroot!\n"));
		done=1;
	}
#else
	return(chroot(dname));
#endif
}

/**************************************************************************
A wrapper for gethostbyname() that tries avoids looking up hostnames 
in the root domain, which can cause dial-on-demand links to come up for no
apparent reason.
****************************************************************************/
struct hostent *sys_gethostbyname(const char *name)
{
#ifdef REDUCE_ROOT_DNS_LOOKUPS
  char query[256], hostname[256];
  char *domain;

  /* Does this name have any dots in it? If so, make no change */

  if (strchr(name, '.'))
    return(gethostbyname(name));

  /* Get my hostname, which should have domain name 
     attached. If not, just do the gethostname on the
     original string. 
  */

  gethostname(hostname, sizeof(hostname) - 1);
  hostname[sizeof(hostname) - 1] = 0;
  if ((domain = strchr(hostname, '.')) == NULL)
    return(gethostbyname(name));

  /* Attach domain name to query and do modified query.
     If names too large, just do gethostname on the
     original string.
  */

  if((strlen(name) + strlen(domain)) >= sizeof(query))
    return(gethostbyname(name));

  slprintf(query, sizeof(query)-1, "%s%s", name, domain);
  return(gethostbyname(query));
#else /* REDUCE_ROOT_DNS_LOOKUPS */
  return(gethostbyname(name));
#endif /* REDUCE_ROOT_DNS_LOOKUPS */
}


/**************************************************************************
 Try and abstract process capabilities (for systems that have them).
****************************************************************************/

BOOL set_process_capability( uint32 cap_flag, BOOL enable )
{
#if defined(HAVE_IRIX_SPECIFIC_CAPABILITIES)
  if(cap_flag == KERNEL_OPLOCK_CAPABILITY)
  {
    cap_t cap = cap_get_proc();

    if (cap == NULL) {
      DEBUG(0,("set_process_capability: cap_get_proc failed. Error was %s\n",
            strerror(errno)));
      return False;
    }

    if(enable)
      cap->cap_effective |= CAP_NETWORK_MGT;
    else
      cap->cap_effective &= ~CAP_NETWORK_MGT;

    if (cap_set_proc(cap) == -1) {
      DEBUG(0,("set_process_capability: cap_set_proc failed. Error was %s\n",
            strerror(errno)));
      cap_free(cap);
      return False;
    }

    cap_free(cap);

    DEBUG(10,("set_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
  }
#endif
  return True;
}

/**************************************************************************
 Try and abstract inherited process capabilities (for systems that have them).
****************************************************************************/

BOOL set_inherited_process_capability( uint32 cap_flag, BOOL enable )
{
#if defined(HAVE_IRIX_SPECIFIC_CAPABILITIES)
  if(cap_flag == KERNEL_OPLOCK_CAPABILITY)
  {
    cap_t cap = cap_get_proc();

    if (cap == NULL) {
      DEBUG(0,("set_inherited_process_capability: cap_get_proc failed. Error was %s\n",
            strerror(errno)));
      return False;
    }

    if(enable)
      cap->cap_inheritable |= CAP_NETWORK_MGT;
    else
      cap->cap_inheritable &= ~CAP_NETWORK_MGT;

    if (cap_set_proc(cap) == -1) {
      DEBUG(0,("set_inherited_process_capability: cap_set_proc failed. Error was %s\n", 
            strerror(errno)));
      cap_free(cap);
      return False;
    }

    cap_free(cap);

    DEBUG(10,("set_inherited_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
  }
#endif
  return True;
}

/**************************************************************************
 Wrapper for random().
****************************************************************************/

long sys_random(void)
{
#if defined(HAVE_RANDOM)
  return (long)random();
#elif defined(HAVE_RAND)
  return (long)rand();
#else
  DEBUG(0,("Error - no random function available !\n"));
  exit(1);
#endif
}

/**************************************************************************
 Wrapper for srandom().
****************************************************************************/

void sys_srandom(unsigned int seed)
{
#if defined(HAVE_SRANDOM)
  srandom(seed);
#elif defined(HAVE_SRAND)
  srand(seed);
#else
  DEBUG(0,("Error - no srandom function available !\n"));
  exit(1);
#endif
}

/**************************************************************************
 Returns equivalent to NGROUPS_MAX - using sysconf if needed.
****************************************************************************/

int groups_max(void)
{
#if defined(SYSCONF_SC_NGROUPS_MAX)
  int ret = sysconf(_SC_NGROUPS_MAX);
  return (ret == -1) ? NGROUPS_MAX : ret;
#else
  return NGROUPS_MAX;
#endif
}

/**************************************************************************
 Wrapper for getgroups. Deals with broken (int) case.
****************************************************************************/

int sys_getgroups(int setlen, gid_t *gidset)
{
#if !defined(HAVE_BROKEN_GETGROUPS)
  return getgroups(setlen, gidset);
#else

  GID_T gid;
  GID_T *group_list;
  int i, ngroups;

  if(setlen == 0) {
    return getgroups(setlen, &gid);
  }

  /*
   * Broken case. We need to allocate a
   * GID_T array of size setlen.
   */

  if(setlen < 0) {
    errno = EINVAL; 
    return -1;
  } 

  if (setlen == 0)
    setlen = groups_max();

  if((group_list = (GID_T *)malloc(setlen * sizeof(GID_T))) == NULL) {
    DEBUG(0,("sys_getgroups: Malloc fail.\n"));
    return -1;
  }

  if((ngroups = getgroups(setlen, group_list)) < 0) {
    int saved_errno = errno;
    free((char *)group_list);
    errno = saved_errno;
    return -1;
  }

  for(i = 0; i < ngroups; i++)
    gidset[i] = (gid_t)group_list[i];

  free((char *)group_list);
  return ngroups;
#endif /* HAVE_BROKEN_GETGROUPS */
}

#ifdef HAVE_SETGROUPS

/**************************************************************************
 Wrapper for setgroups. Deals with broken (int) case. Automatically used
 if we have broken getgroups.
****************************************************************************/

int sys_setgroups(int setlen, gid_t *gidset)
{
#if !defined(HAVE_BROKEN_GETGROUPS)
  return setgroups(setlen, gidset);
#else

  GID_T *group_list;
  int i ; 

  if (setlen == 0)
    return 0 ;

  if (setlen < 0 || setlen > groups_max()) {
    errno = EINVAL; 
    return -1;   
  }

  /*
   * Broken case. We need to allocate a
   * GID_T array of size setlen.
   */

  if((group_list = (GID_T *)malloc(setlen * sizeof(GID_T))) == NULL) {
    DEBUG(0,("sys_setgroups: Malloc fail.\n"));
    return -1;    
  }
 
  for(i = 0; i < setlen; i++) 
    group_list[i] = (GID_T) gidset[i]; 

  if(setgroups(setlen, group_list) != 0) {
    int saved_errno = errno;
    free((char *)group_list);
    errno = saved_errno;
    return -1;
  }
 
  free((char *)group_list);
  return 0 ;
#endif /* HAVE_BROKEN_GETGROUPS */
}

#endif /* HAVE_SETGROUPS */

/*
 * We only wrap pw_name and pw_passwd for now as these
 * are the only potentially modified fields.
 */

/**************************************************************************
 Helper function for getpwnam/getpwuid wrappers.
****************************************************************************/

static struct passwd *setup_pwret(struct passwd *pass)
{
	static pstring pw_name;
	static pstring pw_passwd;
	static struct passwd pw_ret;

	if (pass == NULL)
	{
		return NULL;
	}

	memcpy((char *)&pw_ret, pass, sizeof(struct passwd));

	if (pass->pw_name)
	{
		pw_ret.pw_name = pw_name;
		pstrcpy(pw_ret.pw_name, pass->pw_name);
	}

	if (pass->pw_passwd)
	{
		pw_ret.pw_passwd = pw_passwd;
		pstrcpy(pw_ret.pw_passwd, pass->pw_passwd);
	}

	return &pw_ret;
}

/**************************************************************************
 Wrapper for getpwnam(). Always returns a static that can be modified.
****************************************************************************/

struct passwd *sys_getpwnam(const char *name)
{
	return setup_pwret(getpwnam(name));
}

/**************************************************************************
 Wrapper for getpwuid(). Always returns a static that can be modified.
****************************************************************************/

struct passwd *sys_getpwuid(uid_t uid)
{
	return setup_pwret(getpwuid(uid));
}

/**************************************************************************
 Extract a command into an arg list. Uses a static pstring for storage.
 Caller frees returned arg list (which contains pointers into the static pstring).
****************************************************************************/

static char **extract_args(const char *command)
{
	static pstring trunc_cmd;
	char *ptr;
	int argcl;
	char **argl = NULL;
	int i;

	pstrcpy(trunc_cmd, command);

	if(!(ptr = strtok(trunc_cmd, " \t"))) {
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Count the args.
	 */

	for( argcl = 1; ptr; ptr = strtok(NULL, " \t"))
		argcl++;

	if((argl = (char **)malloc((argcl + 1) * sizeof(char *))) == NULL)
		return NULL;

	/*
	 * Now do the extraction.
	 */

	pstrcpy(trunc_cmd, command);

	ptr = strtok(trunc_cmd, " \t");
	i = 0;
	argl[i++] = ptr;

	while((ptr = strtok(NULL, " \t")) != NULL)
		argl[i++] = ptr;

	argl[i++] = NULL;
	return argl;
}

/**************************************************************************
 Wrapper for popen. Safer as it doesn't search a path.
 Modified from the glibc sources.
****************************************************************************/

typedef struct _popen_list
{
	FILE *fp;
	pid_t child_pid;
	struct _popen_list *next;
} popen_list;

static popen_list *popen_chain;

FILE *sys_popen(const char *command, const char *mode, BOOL paranoid)
{
	int parent_end, child_end;
	int pipe_fds[2];
    popen_list *entry = NULL;
	char **argl = NULL;

	if (pipe(pipe_fds) < 0)
		return NULL;

	if (mode[0] == 'r' && mode[1] == '\0') {
		parent_end = pipe_fds[0];
		child_end = pipe_fds[1];
    } else if (mode[0] == 'w' && mode[1] == '\0') {
		parent_end = pipe_fds[1];
		child_end = pipe_fds[0];
    } else {
		errno = EINVAL;
		goto err_exit;
    }

	if (!*command) {
		errno = EINVAL;
		goto err_exit;
	}

	if((entry = (popen_list *)malloc(sizeof(popen_list))) == NULL)
		goto err_exit;

	/*
	 * Extract the command and args into a NULL terminated array.
	 */

	if(!(argl = extract_args(command)))
		goto err_exit;

	if(paranoid) {
		/*
		 * Do some basic paranioa checks. Do a stat on the parent
		 * directory and ensure it's not world writable. Do a stat
		 * on the file itself and ensure it's owned by root and not
		 * world writable. Note this does *not* prevent symlink races,
		 * but is a generic "don't let the admin screw themselves"
		 * check.
		 */

		SMB_STRUCT_STAT st;
		pstring dir_name;
		char *ptr = strrchr(argl[0], '/');
	
		if(sys_stat(argl[0], &st) != 0)
			goto err_exit;

		if((st.st_uid != (uid_t)0) || (st.st_mode & S_IWOTH)) {
			errno = EACCES;
			goto err_exit;
		}
		
		if(!ptr) {
			/*
			 * No '/' in name - use current directory.
			 */
			pstrcpy(dir_name, ".");
		} else {

			/*
			 * Copy into a pstring and do the checks
			 * again (in case we were length tuncated).
			 */

			pstrcpy(dir_name, argl[0]);
			ptr = strrchr(dir_name, '/');
			if(!ptr) {
				errno = EINVAL;
				goto err_exit;
			}
			if(strcmp(dir_name, "/") != 0)
				*ptr = '\0';
			if(!dir_name[0])
				pstrcpy(dir_name, ".");
		}

		if(sys_stat(argl[0], &st) != 0)
			goto err_exit;

		if(!S_ISDIR(st.st_mode) || (st.st_mode & S_IWOTH)) {
			errno = EACCES;
			goto err_exit;
		}
	}

	entry->child_pid = fork();

	if (entry->child_pid == -1) {

		/*
		 * Error !
		 */

		goto err_exit;
	}

	if (entry->child_pid == 0) {

		/*
		 * Child !
		 */

		int child_std_end = (mode[0] == 'r') ? STDOUT_FILENO : STDIN_FILENO;
		popen_list *p;

		close(parent_end);
		if (child_end != child_std_end) {
			dup2 (child_end, child_std_end);
			close (child_end);
		}

		/*
		 * POSIX.2:  "popen() shall ensure that any streams from previous
		 * popen() calls that remain open in the parent process are closed
		 * in the new child process."
		 */

		for (p = popen_chain; p; p = p->next)
			close(fileno(p->fp));

		execv(argl[0], argl);
		_exit (127);
	}

	/*
	 * Parent.
	 */

	close (child_end);
	free((char *)argl);

	/*
	 * Create the FILE * representing this fd.
	 */
    entry->fp = fdopen(parent_end, mode);

	/* Link into popen_chain. */
	entry->next = popen_chain;
	popen_chain = entry;

	return entry->fp;

err_exit:

	if(entry)
		free((char *)entry);
	if(argl)
		free((char *)argl);
	close(pipe_fds[0]);
	close(pipe_fds[1]);
	return NULL;
}

/**************************************************************************
 Wrapper for pclose. Modified from the glibc sources.
****************************************************************************/

int sys_pclose( FILE *fp)
{
	int wstatus;
	popen_list **ptr = &popen_chain;
	popen_list *entry = NULL;
	pid_t wait_pid;
	int status = -1;

	/* Unlink from popen_chain. */
	for ( ; *ptr != NULL; ptr = &(*ptr)->next) {
		if ((*ptr)->fp == fp) {
			entry = *ptr;
			*ptr = (*ptr)->next;
			status = 0;
			break;
		}
	}

	if (status < 0 || close(fileno(entry->fp)) < 0)
		return -1;

	/*
	 * As Samba is catching and eating child process
	 * exits we don't really care about the child exit
	 * code, a -1 with errno = ECHILD will do fine for us.
	 */

	do {
		wait_pid = sys_waitpid (entry->child_pid, &wstatus, 0);
	} while (wait_pid == -1 && errno == EINTR);

	free((char *)entry);

	if (wait_pid == -1)
		return -1;
	return wstatus;
}
