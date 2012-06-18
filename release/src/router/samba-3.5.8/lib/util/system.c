/* 
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1998-2002
   
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

#include "includes.h"
#include "system/network.h"
#include "system/filesys.h"

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

/**************************************************************************
A wrapper for gethostbyname() that tries avoids looking up hostnames 
in the root domain, which can cause dial-on-demand links to come up for no
apparent reason.
****************************************************************************/

_PUBLIC_ struct hostent *sys_gethostbyname(const char *name)
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

_PUBLIC_ struct in_addr sys_inet_makeaddr(int net, int host)
{
	struct in_addr in;
	struct in_addr in2;
	in = inet_makeaddr(net, host);
	in2.s_addr = in.s_addr;
	return in2;
}

/**************************************************************************
 Wrapper for fork. Ensures we clear our pid cache.
****************************************************************************/

static pid_t mypid = (pid_t)-1;

_PUBLIC_ pid_t sys_fork(void)
{
	pid_t forkret = fork();

	if (forkret == (pid_t)0) {
		/* Child - reset mypid so sys_getpid does a system call. */
		mypid = (pid_t) -1;
	}

	return forkret;
}

/**************************************************************************
 Wrapper for getpid. Ensures we only do a system call *once*.
****************************************************************************/

_PUBLIC_ pid_t sys_getpid(void)
{
	if (mypid == (pid_t)-1)
		mypid = getpid();

	return mypid;
}
