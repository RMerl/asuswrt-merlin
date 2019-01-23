/* dnsmasq is Copyright (c) 2000-2017 Simon Kelley

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

/* Wrapper for poll(). Allocates and extends array of struct pollfds,
   keeps them in fd order so that we can set and test conditions on
   fd using a simple but efficient binary chop. */

/* poll_reset()
   poll_listen(fd, event)
   .
   .
   poll_listen(fd, event);

   hits = do_poll(timeout);

   if (poll_check(fd, event)
    .
    .

   if (poll_check(fd, event)
    .
    .

    event is OR of POLLIN, POLLOUT, POLLERR, etc
*/

static struct pollfd *pollfds = NULL;
static nfds_t nfds, arrsize = 0;

/* Binary search. Returns either the pollfd with fd, or
   if the fd doesn't match, or return equals nfds, the entry
   to the left of which a new record should be inserted. */
static nfds_t fd_search(int fd)
{
  nfds_t left, right, mid;
  
  if ((right = nfds) == 0)
    return 0;
  
  left = 0;
  
  while (1)
    {
      if (right == left + 1)
	return (pollfds[left].fd >= fd) ? left : right;
      
      mid = (left + right)/2;
      
      if (pollfds[mid].fd > fd)
	right = mid;
      else 
	left = mid;
    }
}

void poll_reset(void)
{
  nfds = 0;
}

int do_poll(int timeout)
{
  return poll(pollfds, nfds, timeout);
}

int poll_check(int fd, short event)
{
  nfds_t i = fd_search(fd);
  
  if (i < nfds && pollfds[i].fd == fd)
    return pollfds[i].revents & event;

  return 0;
}

void poll_listen(int fd, short event)
{
   nfds_t i = fd_search(fd);
  
   if (i < nfds && pollfds[i].fd == fd)
     pollfds[i].events |= event;
   else
     {
       if (arrsize != nfds)
	 memmove(&pollfds[i+1], &pollfds[i], (nfds - i) * sizeof(struct pollfd));
       else
	 {
	   /* Array too small, extend. */
	   struct pollfd *new;

	   arrsize = (arrsize == 0) ? 64 : arrsize * 2;

	   if (!(new = whine_malloc(arrsize * sizeof(struct pollfd))))
	     return;

	   if (pollfds)
	     {
	       memcpy(new, pollfds, i * sizeof(struct pollfd));
	       memcpy(&new[i+1], &pollfds[i], (nfds - i) * sizeof(struct pollfd));
	       free(pollfds);
	     }
	   
	   pollfds = new;
	 }
       
       pollfds[i].fd = fd;
       pollfds[i].events = event;
       nfds++;
     }
}
