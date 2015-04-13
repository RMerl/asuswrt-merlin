/* dnsmasq is Copyright (c) 2000-2015 Simon Kelley

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

#ifdef HAVE_SCRIPT

/* This file has code to fork a helper process which recieves data via a pipe 
   shared with the main process and which is responsible for calling a script when
   DHCP leases change.

   The helper process is forked before the main process drops root, so it retains root 
   privs to pass on to the script. For this reason it tries to be paranoid about 
   data received from the main process, in case that has been compromised. We don't
   want the helper to give an attacker root. In particular, the script to be run is
   not settable via the pipe, once the fork has taken place it is not alterable by the 
   main process.
*/

static void my_setenv(const char *name, const char *value, int *error);
static unsigned char *grab_extradata(unsigned char *buf, unsigned char *end,  char *env, int *err);

#ifdef HAVE_LUASCRIPT
#define LUA_COMPAT_ALL
#include <lua.h>  
#include <lualib.h>  
#include <lauxlib.h>  

#ifndef lua_open
#define lua_open()     luaL_newstate()
#endif

lua_State *lua;

static unsigned char *grab_extradata_lua(unsigned char *buf, unsigned char *end, char *field);
#endif


struct script_data
{
  int flags;
  int action, hwaddr_len, hwaddr_type;
  int clid_len, hostname_len, ed_len;
  struct in_addr addr, giaddr;
  unsigned int remaining_time;
#ifdef HAVE_BROKEN_RTC
  unsigned int length;
#else
  time_t expires;
#endif
#ifdef HAVE_TFTP
  off_t file_len;
#endif
#ifdef HAVE_IPV6
  struct in6_addr addr6;
#endif
#ifdef HAVE_DHCP6
  int iaid, vendorclass_count;
#endif
  unsigned char hwaddr[DHCP_CHADDR_MAX];
  char interface[IF_NAMESIZE];
};

static struct script_data *buf = NULL;
static size_t bytes_in_buf = 0, buf_size = 0;

int create_helper(int event_fd, int err_fd, uid_t uid, gid_t gid, long max_fd)
{
  pid_t pid;
  int i, pipefd[2];
  struct sigaction sigact;

  /* create the pipe through which the main program sends us commands,
     then fork our process. */
  if (pipe(pipefd) == -1 || !fix_fd(pipefd[1]) || (pid = fork()) == -1)
    {
      send_event(err_fd, EVENT_PIPE_ERR, errno, NULL);
      _exit(0);
    }

  if (pid != 0)
    {
      close(pipefd[0]); /* close reader side */
      return pipefd[1];
    }

  /* ignore SIGTERM, so that we can clean up when the main process gets hit
     and SIGALRM so that we can use sleep() */
  sigact.sa_handler = SIG_IGN;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGALRM, &sigact, NULL);

  if (!option_bool(OPT_DEBUG) && uid != 0)
    {
      gid_t dummy;
      if (setgroups(0, &dummy) == -1 || 
	  setgid(gid) == -1 || 
	  setuid(uid) == -1)
	{
	  if (option_bool(OPT_NO_FORK))
	    /* send error to daemon process if no-fork */
	    send_event(event_fd, EVENT_USER_ERR, errno, daemon->scriptuser);
	  else
	    {
	      /* kill daemon */
	      send_event(event_fd, EVENT_DIE, 0, NULL);
	      /* return error */
	      send_event(err_fd, EVENT_USER_ERR, errno, daemon->scriptuser);
	    }
	  _exit(0);
	}
    }

  /* close all the sockets etc, we don't need them here. 
     Don't close err_fd, in case the lua-init fails.
     Note that we have to do this before lua init
     so we don't close any lua fds. */
  for (max_fd--; max_fd >= 0; max_fd--)
    if (max_fd != STDOUT_FILENO && max_fd != STDERR_FILENO && 
	max_fd != STDIN_FILENO && max_fd != pipefd[0] && 
	max_fd != event_fd && max_fd != err_fd)
      close(max_fd);
  
#ifdef HAVE_LUASCRIPT
  if (daemon->luascript)
    {
      const char *lua_err = NULL;
      lua = lua_open();
      luaL_openlibs(lua);
      
      /* get Lua to load our script file */
      if (luaL_dofile(lua, daemon->luascript) != 0)
	lua_err = lua_tostring(lua, -1);
      else
	{
	  lua_getglobal(lua, "lease");
	  if (lua_type(lua, -1) != LUA_TFUNCTION) 
	    lua_err = _("lease() function missing in Lua script");
	}
      
      if (lua_err)
	{
	  if (option_bool(OPT_NO_FORK) || option_bool(OPT_DEBUG))
	    /* send error to daemon process if no-fork */
	    send_event(event_fd, EVENT_LUA_ERR, 0, (char *)lua_err);
	  else
	    {
	      /* kill daemon */
	      send_event(event_fd, EVENT_DIE, 0, NULL);
	      /* return error */
	      send_event(err_fd, EVENT_LUA_ERR, 0, (char *)lua_err);
	    }
	  _exit(0);
	}
      
      lua_pop(lua, 1);  /* remove nil from stack */
      lua_getglobal(lua, "init");
      if (lua_type(lua, -1) == LUA_TFUNCTION)
	lua_call(lua, 0, 0);
      else
	lua_pop(lua, 1);  /* remove nil from stack */	
    }
#endif

  /* All init done, close our copy of the error pipe, so that main process can return */
  if (err_fd != -1)
    close(err_fd);
    
  /* loop here */
  while(1)
    {
      struct script_data data;
      char *p, *action_str, *hostname = NULL, *domain = NULL;
      unsigned char *buf = (unsigned char *)daemon->namebuff;
      unsigned char *end, *extradata, *alloc_buff = NULL;
      int is6, err = 0;

      free(alloc_buff);
      
      /* we read zero bytes when pipe closed: this is our signal to exit */ 
      if (!read_write(pipefd[0], (unsigned char *)&data, sizeof(data), 1))
	{
#ifdef HAVE_LUASCRIPT
	  if (daemon->luascript)
	    {
	      lua_getglobal(lua, "shutdown");
	      if (lua_type(lua, -1) == LUA_TFUNCTION)
		lua_call(lua, 0, 0);
	    }
#endif
	  _exit(0);
	}
 
      is6 = !!(data.flags & (LEASE_TA | LEASE_NA));
      
      if (data.action == ACTION_DEL)
	action_str = "del";
      else if (data.action == ACTION_ADD)
	action_str = "add";
      else if (data.action == ACTION_OLD || data.action == ACTION_OLD_HOSTNAME)
	action_str = "old";
      else if (data.action == ACTION_TFTP)
	{
	  action_str = "tftp";
	  is6 = (data.flags != AF_INET);
	}
      else
	continue;

      	
      /* stringify MAC into dhcp_buff */
      p = daemon->dhcp_buff;
      if (data.hwaddr_type != ARPHRD_ETHER || data.hwaddr_len == 0) 
	p += sprintf(p, "%.2x-", data.hwaddr_type);
      for (i = 0; (i < data.hwaddr_len) && (i < DHCP_CHADDR_MAX); i++)
	{
	  p += sprintf(p, "%.2x", data.hwaddr[i]);
	  if (i != data.hwaddr_len - 1)
	    p += sprintf(p, ":");
	}
      
      /* supplied data may just exceed normal buffer (unlikely) */
      if ((data.hostname_len + data.ed_len + data.clid_len) > MAXDNAME && 
	  !(alloc_buff = buf = malloc(data.hostname_len + data.ed_len + data.clid_len)))
	continue;
      
      if (!read_write(pipefd[0], buf, 
		      data.hostname_len + data.ed_len + data.clid_len, 1))
	continue;

      /* CLID into packet */
      for (p = daemon->packet, i = 0; i < data.clid_len; i++)
	{
	  p += sprintf(p, "%.2x", buf[i]);
	  if (i != data.clid_len - 1) 
	      p += sprintf(p, ":");
	}

#ifdef HAVE_DHCP6
      if (is6)
	{
	  /* or IAID and server DUID for IPv6 */
	  sprintf(daemon->dhcp_buff3, "%s%u", data.flags & LEASE_TA ? "T" : "", data.iaid);	
	  for (p = daemon->dhcp_packet.iov_base, i = 0; i < daemon->duid_len; i++)
	    {
	      p += sprintf(p, "%.2x", daemon->duid[i]);
	      if (i != daemon->duid_len - 1) 
		p += sprintf(p, ":");
	    }

	}
#endif

      buf += data.clid_len;

      if (data.hostname_len != 0)
	{
	  char *dot;
	  hostname = (char *)buf;
	  hostname[data.hostname_len - 1] = 0;
	  if (data.action != ACTION_TFTP)
	    {
	      if (!legal_hostname(hostname))
		hostname = NULL;
	      else if ((dot = strchr(hostname, '.')))
		{
		  domain = dot+1;
		  *dot = 0;
		} 
	    }
	}
    
      extradata = buf + data.hostname_len;
    
      if (!is6)
	inet_ntop(AF_INET, &data.addr, daemon->addrbuff, ADDRSTRLEN);
#ifdef HAVE_DHCP6
      else
	inet_ntop(AF_INET6, &data.addr6, daemon->addrbuff, ADDRSTRLEN);
#endif

#ifdef HAVE_TFTP
      /* file length */
      if (data.action == ACTION_TFTP)
	sprintf(is6 ? daemon->packet : daemon->dhcp_buff, "%lu", (unsigned long)data.file_len);
#endif

#ifdef HAVE_LUASCRIPT
      if (daemon->luascript)
	{
	  if (data.action == ACTION_TFTP)
	    {
	      lua_getglobal(lua, "tftp"); 
	      if (lua_type(lua, -1) != LUA_TFUNCTION)
		lua_pop(lua, 1); /* tftp function optional */
	      else
		{
		  lua_pushstring(lua, action_str); /* arg1 - action */
		  lua_newtable(lua);               /* arg2 - data table */
		  lua_pushstring(lua, daemon->addrbuff);
		  lua_setfield(lua, -2, "destination_address");
		  lua_pushstring(lua, hostname);
		  lua_setfield(lua, -2, "file_name"); 
		  lua_pushstring(lua, is6 ? daemon->packet : daemon->dhcp_buff);
		  lua_setfield(lua, -2, "file_size");
		  lua_call(lua, 2, 0);	/* pass 2 values, expect 0 */
		}
	    }
	  else
	    {
	      lua_getglobal(lua, "lease");     /* function to call */
	      lua_pushstring(lua, action_str); /* arg1 - action */
	      lua_newtable(lua);               /* arg2 - data table */
	      
	      if (is6)
		{
		  lua_pushstring(lua, daemon->packet);
		  lua_setfield(lua, -2, "client_duid");
		  lua_pushstring(lua, daemon->dhcp_packet.iov_base);
		  lua_setfield(lua, -2, "server_duid");
		  lua_pushstring(lua, daemon->dhcp_buff3);
		  lua_setfield(lua, -2, "iaid");
		}
	      
	      if (!is6 && data.clid_len != 0)
		{
		  lua_pushstring(lua, daemon->packet);
		  lua_setfield(lua, -2, "client_id");
		}
	      
	      if (strlen(data.interface) != 0)
		{
		  lua_pushstring(lua, data.interface);
		  lua_setfield(lua, -2, "interface");
		}
	      
#ifdef HAVE_BROKEN_RTC	
	      lua_pushnumber(lua, data.length);
	      lua_setfield(lua, -2, "lease_length");
#else
	      lua_pushnumber(lua, data.expires);
	      lua_setfield(lua, -2, "lease_expires");
#endif
	      
	      if (hostname)
		{
		  lua_pushstring(lua, hostname);
		  lua_setfield(lua, -2, "hostname");
		}
	      
	      if (domain)
		{
		  lua_pushstring(lua, domain);
		  lua_setfield(lua, -2, "domain");
		}
	      
	      end = extradata + data.ed_len;
	      buf = extradata;
	      
	      if (!is6)
		buf = grab_extradata_lua(buf, end, "vendor_class");
#ifdef HAVE_DHCP6
	      else  if (data.vendorclass_count != 0)
		{
		  sprintf(daemon->dhcp_buff2, "vendor_class_id");
		  buf = grab_extradata_lua(buf, end, daemon->dhcp_buff2);
		  for (i = 0; i < data.vendorclass_count - 1; i++)
		    {
		      sprintf(daemon->dhcp_buff2, "vendor_class%i", i);
		      buf = grab_extradata_lua(buf, end, daemon->dhcp_buff2);
		    }
		}
#endif
	      
	      buf = grab_extradata_lua(buf, end, "supplied_hostname");
	      
	      if (!is6)
		{
		  buf = grab_extradata_lua(buf, end, "cpewan_oui");
		  buf = grab_extradata_lua(buf, end, "cpewan_serial");   
		  buf = grab_extradata_lua(buf, end, "cpewan_class");
		  buf = grab_extradata_lua(buf, end, "circuit_id");
		  buf = grab_extradata_lua(buf, end, "subscriber_id");
		  buf = grab_extradata_lua(buf, end, "remote_id");
		}
	      
	      buf = grab_extradata_lua(buf, end, "tags");
	      
	      if (is6)
		buf = grab_extradata_lua(buf, end, "relay_address");
	      else if (data.giaddr.s_addr != 0)
		{
		  lua_pushstring(lua, inet_ntoa(data.giaddr));
		  lua_setfield(lua, -2, "relay_address");
		}
	      
	      for (i = 0; buf; i++)
		{
		  sprintf(daemon->dhcp_buff2, "user_class%i", i);
		  buf = grab_extradata_lua(buf, end, daemon->dhcp_buff2);
		}
	      
	      if (data.action != ACTION_DEL && data.remaining_time != 0)
		{
		  lua_pushnumber(lua, data.remaining_time);
		  lua_setfield(lua, -2, "time_remaining");
		}
	      
	      if (data.action == ACTION_OLD_HOSTNAME && hostname)
		{
		  lua_pushstring(lua, hostname);
		  lua_setfield(lua, -2, "old_hostname");
		}
	      
	      if (!is6 || data.hwaddr_len != 0)
		{
		  lua_pushstring(lua, daemon->dhcp_buff);
		  lua_setfield(lua, -2, "mac_address");
		}
	      
	      lua_pushstring(lua, daemon->addrbuff);
	      lua_setfield(lua, -2, "ip_address");
	    
	      lua_call(lua, 2, 0);	/* pass 2 values, expect 0 */
	    }
	}
#endif

      /* no script, just lua */
      if (!daemon->lease_change_command)
	continue;

      /* possible fork errors are all temporary resource problems */
      while ((pid = fork()) == -1 && (errno == EAGAIN || errno == ENOMEM))
	sleep(2);

      if (pid == -1)
	continue;
      
      /* wait for child to complete */
      if (pid != 0)
	{
	  /* reap our children's children, if necessary */
	  while (1)
	    {
	      int status;
	      pid_t rc = wait(&status);
	      
	      if (rc == pid)
		{
		  /* On error send event back to main process for logging */
		  if (WIFSIGNALED(status))
		    send_event(event_fd, EVENT_KILLED, WTERMSIG(status), NULL);
		  else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		    send_event(event_fd, EVENT_EXITED, WEXITSTATUS(status), NULL);
		  break;
		}
	      
	      if (rc == -1 && errno != EINTR)
		break;
	    }
	  
	  continue;
	}
      
      if (data.action != ACTION_TFTP)
	{
#ifdef HAVE_DHCP6
	  my_setenv("DNSMASQ_IAID", is6 ? daemon->dhcp_buff3 : NULL, &err);
	  my_setenv("DNSMASQ_SERVER_DUID", is6 ? daemon->dhcp_packet.iov_base : NULL, &err); 
	  my_setenv("DNSMASQ_MAC", is6 && data.hwaddr_len != 0 ? daemon->dhcp_buff : NULL, &err);
#endif
	  
	  my_setenv("DNSMASQ_CLIENT_ID", !is6 && data.clid_len != 0 ? daemon->packet : NULL, &err);
	  my_setenv("DNSMASQ_INTERFACE", strlen(data.interface) != 0 ? data.interface : NULL, &err);
	  
#ifdef HAVE_BROKEN_RTC
	  sprintf(daemon->dhcp_buff2, "%u", data.length);
	  my_setenv("DNSMASQ_LEASE_LENGTH", daemon->dhcp_buff2, &err);
#else
	  sprintf(daemon->dhcp_buff2, "%lu", (unsigned long)data.expires);
	  my_setenv("DNSMASQ_LEASE_EXPIRES", daemon->dhcp_buff2, &err); 
#endif
	  
	  my_setenv("DNSMASQ_DOMAIN", domain, &err);
	  
	  end = extradata + data.ed_len;
	  buf = extradata;
	  
	  if (!is6)
	    buf = grab_extradata(buf, end, "DNSMASQ_VENDOR_CLASS", &err);
#ifdef HAVE_DHCP6
	  else
	    {
	      if (data.vendorclass_count != 0)
		{
		  buf = grab_extradata(buf, end, "DNSMASQ_VENDOR_CLASS_ID", &err);
		  for (i = 0; i < data.vendorclass_count - 1; i++)
		    {
		      sprintf(daemon->dhcp_buff2, "DNSMASQ_VENDOR_CLASS%i", i);
		      buf = grab_extradata(buf, end, daemon->dhcp_buff2, &err);
		    }
		}
	    }
#endif
	  
	  buf = grab_extradata(buf, end, "DNSMASQ_SUPPLIED_HOSTNAME", &err);
	  
	  if (!is6)
	    {
	      buf = grab_extradata(buf, end, "DNSMASQ_CPEWAN_OUI", &err);
	      buf = grab_extradata(buf, end, "DNSMASQ_CPEWAN_SERIAL", &err);   
	      buf = grab_extradata(buf, end, "DNSMASQ_CPEWAN_CLASS", &err);
	      buf = grab_extradata(buf, end, "DNSMASQ_CIRCUIT_ID", &err);
	      buf = grab_extradata(buf, end, "DNSMASQ_SUBSCRIBER_ID", &err);
	      buf = grab_extradata(buf, end, "DNSMASQ_REMOTE_ID", &err);
	    }
	  
	  buf = grab_extradata(buf, end, "DNSMASQ_TAGS", &err);

	  if (is6)
	    buf = grab_extradata(buf, end, "DNSMASQ_RELAY_ADDRESS", &err);
	  else 
	    my_setenv("DNSMASQ_RELAY_ADDRESS", data.giaddr.s_addr != 0 ? inet_ntoa(data.giaddr) : NULL, &err); 
	  
	  for (i = 0; buf; i++)
	    {
	      sprintf(daemon->dhcp_buff2, "DNSMASQ_USER_CLASS%i", i);
	      buf = grab_extradata(buf, end, daemon->dhcp_buff2, &err);
	    }
	  
	  sprintf(daemon->dhcp_buff2, "%u", data.remaining_time);
	  my_setenv("DNSMASQ_TIME_REMAINING", data.action != ACTION_DEL && data.remaining_time != 0 ? daemon->dhcp_buff2 : NULL, &err);
	  
	  my_setenv("DNSMASQ_OLD_HOSTNAME", data.action == ACTION_OLD_HOSTNAME ? hostname : NULL, &err);
	  if (data.action == ACTION_OLD_HOSTNAME)
	    hostname = NULL;
	}

      my_setenv("DNSMASQ_LOG_DHCP", option_bool(OPT_LOG_OPTS) ? "1" : NULL, &err);
      
      /* we need to have the event_fd around if exec fails */
      if ((i = fcntl(event_fd, F_GETFD)) != -1)
	fcntl(event_fd, F_SETFD, i | FD_CLOEXEC);
      close(pipefd[0]);

      p =  strrchr(daemon->lease_change_command, '/');
      if (err == 0)
	{
	  execl(daemon->lease_change_command, 
		p ? p+1 : daemon->lease_change_command,
		action_str, is6 ? daemon->packet : daemon->dhcp_buff, 
		daemon->addrbuff, hostname, (char*)NULL);
	  err = errno;
	}
      /* failed, send event so the main process logs the problem */
      send_event(event_fd, EVENT_EXEC_ERR, err, NULL);
      _exit(0); 
    }
}

static void my_setenv(const char *name, const char *value, int *error)
{
  if (*error == 0)
    {
      if (!value)
	unsetenv(name);
      else if (setenv(name, value, 1) != 0)
	*error = errno;
    }
}
 
static unsigned char *grab_extradata(unsigned char *buf, unsigned char *end,  char *env, int *err)
{
  unsigned char *next = NULL;
  char *val = NULL;

  if (buf && (buf != end))
    {
      for (next = buf; ; next++)
	if (next == end)
	  {
	    next = NULL;
	    break;
	  }
	else if (*next == 0)
	  break;

      if (next && (next != buf))
	{
	  char *p;
	  /* No "=" in value */
	  if ((p = strchr((char *)buf, '=')))
	    *p = 0;
	  val = (char *)buf;
	}
    }
  
  my_setenv(env, val, err);
   
  return next ? next + 1 : NULL;
}

#ifdef HAVE_LUASCRIPT
static unsigned char *grab_extradata_lua(unsigned char *buf, unsigned char *end, char *field)
{
  unsigned char *next;

  if (!buf || (buf == end))
    return NULL;

  for (next = buf; *next != 0; next++)
    if (next == end)
      return NULL;
  
  if (next != buf)
    {
      lua_pushstring(lua,  (char *)buf);
      lua_setfield(lua, -2, field);
    }

  return next + 1;
}
#endif

static void buff_alloc(size_t size)
{
  if (size > buf_size)
    {
      struct script_data *new;
      
      /* start with reasonable size, will almost never need extending. */
      if (size < sizeof(struct script_data) + 200)
	size = sizeof(struct script_data) + 200;

      if (!(new = whine_malloc(size)))
	return;
      if (buf)
	free(buf);
      buf = new;
      buf_size = size;
    }
}

/* pack up lease data into a buffer */    
void queue_script(int action, struct dhcp_lease *lease, char *hostname, time_t now)
{
  unsigned char *p;
  unsigned int hostname_len = 0, clid_len = 0, ed_len = 0;
  int fd = daemon->dhcpfd;
#ifdef HAVE_DHCP6 
  if (!daemon->dhcp)
    fd = daemon->dhcp6fd;
#endif

  /* no script */
  if (daemon->helperfd == -1)
    return;

  if (lease->extradata)
    ed_len = lease->extradata_len;
  if (lease->clid)
    clid_len = lease->clid_len;
  if (hostname)
    hostname_len = strlen(hostname) + 1;

  buff_alloc(sizeof(struct script_data) +  clid_len + ed_len + hostname_len);

  buf->action = action;
  buf->flags = lease->flags;
#ifdef HAVE_DHCP6 
  buf->vendorclass_count = lease->vendorclass_count;
  buf->addr6 = lease->addr6;
  buf->iaid = lease->iaid;
#endif
  buf->hwaddr_len = lease->hwaddr_len;
  buf->hwaddr_type = lease->hwaddr_type;
  buf->clid_len = clid_len;
  buf->ed_len = ed_len;
  buf->hostname_len = hostname_len;
  buf->addr = lease->addr;
  buf->giaddr = lease->giaddr;
  memcpy(buf->hwaddr, lease->hwaddr, DHCP_CHADDR_MAX);
  if (!indextoname(fd, lease->last_interface, buf->interface))
    buf->interface[0] = 0;
  
#ifdef HAVE_BROKEN_RTC 
  buf->length = lease->length;
#else
  buf->expires = lease->expires;
#endif

  if (lease->expires != 0)
    buf->remaining_time = (unsigned int)difftime(lease->expires, now);
  else
    buf->remaining_time = 0;

  p = (unsigned char *)(buf+1);
  if (clid_len != 0)
    {
      memcpy(p, lease->clid, clid_len);
      p += clid_len;
    }
  if (hostname_len != 0)
    {
      memcpy(p, hostname, hostname_len);
      p += hostname_len;
    }
  if (ed_len != 0)
    {
      memcpy(p, lease->extradata, ed_len);
      p += ed_len;
    }
  bytes_in_buf = p - (unsigned char *)buf;
}

#ifdef HAVE_TFTP
/* This nastily re-uses DHCP-fields for TFTP stuff */
void queue_tftp(off_t file_len, char *filename, union mysockaddr *peer)
{
  unsigned int filename_len;

  /* no script */
  if (daemon->helperfd == -1)
    return;
  
  filename_len = strlen(filename) + 1;
  buff_alloc(sizeof(struct script_data) +  filename_len);
  memset(buf, 0, sizeof(struct script_data));

  buf->action = ACTION_TFTP;
  buf->hostname_len = filename_len;
  buf->file_len = file_len;

  if ((buf->flags = peer->sa.sa_family) == AF_INET)
    buf->addr = peer->in.sin_addr;
#ifdef HAVE_IPV6
  else
    buf->addr6 = peer->in6.sin6_addr;
#endif

  memcpy((unsigned char *)(buf+1), filename, filename_len);
  
  bytes_in_buf = sizeof(struct script_data) +  filename_len;
}
#endif

int helper_buf_empty(void)
{
  return bytes_in_buf == 0;
}

void helper_write(void)
{
  ssize_t rc;

  if (bytes_in_buf == 0)
    return;
  
  if ((rc = write(daemon->helperfd, buf, bytes_in_buf)) != -1)
    {
      if (bytes_in_buf != (size_t)rc)
	memmove(buf, buf + rc, bytes_in_buf - rc); 
      bytes_in_buf -= rc;
    }
  else
    {
      if (errno == EAGAIN || errno == EINTR)
	return;
      bytes_in_buf = 0;
    }
}

#endif



