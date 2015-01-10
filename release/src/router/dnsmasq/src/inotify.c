/* dnsmasq is Copyright (c) 2000-2014 Simon Kelley
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>

/* the strategy is to set a inotify on the directories containing
   resolv files, for any files in the directory which are close-write 
   or moved into the directory.
   
   When either of those happen, we look to see if the file involved
   is actually a resolv-file, and if so, call poll-resolv with
   the "force" argument, to ensure it's read.

   This adds one new error condition: the directories containing
   all specified resolv-files must exist at start-up, even if the actual
   files don't. 
*/

static char *inotify_buffer;
#define INOTIFY_SZ (sizeof(struct inotify_event) + NAME_MAX + 1)

void inotify_dnsmasq_init()
{
  struct resolvc *res;

  inotify_buffer = safe_malloc(INOTIFY_SZ);


  daemon->inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  
  if (daemon->inotifyfd == -1)
    die(_("failed to create inotify: %s"), NULL, EC_MISC);
  
  for (res = daemon->resolv_files; res; res = res->next)
    {
      char *d = NULL, *path;
      
      if (!(path = realpath(res->name, NULL)))
	{
	  /* realpath will fail if the file doesn't exist, but
	     dnsmasq copes with missing files, so fall back 
	     and assume that symlinks are not in use in that case. */
	  if (errno == ENOENT)
	    path = res->name;
	  else
	    die(_("cannot cannonicalise resolv-file %s: %s"), res->name, EC_MISC); 
	}
      
      if ((d = strrchr(path, '/')))
	{
	  *d = 0; /* make path just directory */
	  res->wd = inotify_add_watch(daemon->inotifyfd, path, IN_CLOSE_WRITE | IN_MOVED_TO);
	  res->file = d+1; /* pointer to filename */
	  *d = '/';
	  
	  if (res->wd == -1 && errno == ENOENT)
	    die(_("directory %s for resolv-file is missing, cannot poll"), res->name, EC_MISC);
	  
	  if (res->wd == -1)
	    die(_("failed to create inotify for %s: %s"), res->name, EC_MISC);
	}
    }
}

int inotify_check(void)
{
  int hit = 0;
  
  while (1)
    {
      int rc;
      char *p;
      struct resolvc *res;
      struct inotify_event *in;

      while ((rc = read(daemon->inotifyfd, inotify_buffer, INOTIFY_SZ)) == -1 && errno == EINTR);
      
      if (rc <= 0)
	break;
      
      for (p = inotify_buffer; rc - (p - inotify_buffer) >= (int)sizeof(struct inotify_event); p += sizeof(struct inotify_event) + in->len) 
	{
	  in = (struct inotify_event*)p;
	  
	  for (res = daemon->resolv_files; res; res = res->next)
	    if (res->wd == in->wd && in->len != 0 && strcmp(res->file, in->name) == 0)
	      hit = 1;
	}
    }

  return hit;
}

#endif

  
  
