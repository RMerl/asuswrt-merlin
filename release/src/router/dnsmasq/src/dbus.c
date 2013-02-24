/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

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

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

const char* introspection_xml_template =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node name=\"" DNSMASQ_PATH "\">\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"%s\">\n"
"    <method name=\"ClearCache\">\n"
"    </method>\n"
"    <method name=\"GetVersion\">\n"
"      <arg name=\"version\" direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"    <method name=\"SetServers\">\n"
"      <arg name=\"servers\" direction=\"in\" type=\"av\"/>\n"
"    </method>\n"
"    <method name=\"SetDomainServers\">\n"
"      <arg name=\"servers\" direction=\"in\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"SetServersEx\">\n"
"      <arg name=\"servers\" direction=\"in\" type=\"aas\"/>\n"
"    </method>\n"
"    <signal name=\"DhcpLeaseAdded\">\n"
"      <arg name=\"ipaddr\" type=\"s\"/>\n"
"      <arg name=\"hwaddr\" type=\"s\"/>\n"
"      <arg name=\"hostname\" type=\"s\"/>\n"
"    </signal>\n"
"    <signal name=\"DhcpLeaseDeleted\">\n"
"      <arg name=\"ipaddr\" type=\"s\"/>\n"
"      <arg name=\"hwaddr\" type=\"s\"/>\n"
"      <arg name=\"hostname\" type=\"s\"/>\n"
"    </signal>\n"
"    <signal name=\"DhcpLeaseUpdated\">\n"
"      <arg name=\"ipaddr\" type=\"s\"/>\n"
"      <arg name=\"hwaddr\" type=\"s\"/>\n"
"      <arg name=\"hostname\" type=\"s\"/>\n"
"    </signal>\n"
"  </interface>\n"
"</node>\n";

static char *introspection_xml = NULL;

struct watch {
  DBusWatch *watch;      
  struct watch *next;
};


static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
  struct watch *w;

  for (w = daemon->watches; w; w = w->next)
    if (w->watch == watch)
      return TRUE;

  if (!(w = whine_malloc(sizeof(struct watch))))
    return FALSE;

  w->watch = watch;
  w->next = daemon->watches;
  daemon->watches = w;

  w = data; /* no warning */
  return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data)
{
  struct watch **up, *w;  
  
  for (up = &(daemon->watches), w = daemon->watches; w; w = w->next)
    if (w->watch == watch)
      {
        *up = w->next;
        free(w);
      }
    else
      up = &(w->next);

  w = data; /* no warning */
}

static void add_update_server(union mysockaddr *addr,
                              union mysockaddr *source_addr,
			      const char *interface,
                              const char *domain)
{
  struct server *serv;

  /* See if there is a suitable candidate, and unmark */
  for (serv = daemon->servers; serv; serv = serv->next)
    if ((serv->flags & SERV_FROM_DBUS) &&
       (serv->flags & SERV_MARK))
      {
	if (domain)
	  {
	    if (!(serv->flags & SERV_HAS_DOMAIN) || !hostname_isequal(domain, serv->domain))
	      continue;
	  }
	else
	  {
	    if (serv->flags & SERV_HAS_DOMAIN)
	      continue;
	  }
	
	serv->flags &= ~SERV_MARK;

        break;
      }
  
  if (!serv && (serv = whine_malloc(sizeof (struct server))))
    {
      /* Not found, create a new one. */
      memset(serv, 0, sizeof(struct server));
      
      if (domain && !(serv->domain = whine_malloc(strlen(domain)+1)))
	{
	  free(serv);
          serv = NULL;
        }
      else
        {
	  serv->next = daemon->servers;
	  daemon->servers = serv;
	  serv->flags = SERV_FROM_DBUS;
	  if (domain)
	    {
	      strcpy(serv->domain, domain);
	      serv->flags |= SERV_HAS_DOMAIN;
	    }
	}
    }
  
  if (serv)
    {
      if (interface)
	strcpy(serv->interface, interface);
      else
	serv->interface[0] = 0;
            
      if (source_addr->in.sin_family == AF_INET &&
          addr->in.sin_addr.s_addr == 0 &&
          serv->domain)
        serv->flags |= SERV_NO_ADDR;
      else
        {
          serv->flags &= ~SERV_NO_ADDR;
          serv->addr = *addr;
          serv->source_addr = *source_addr;
        }
    }
}

static void mark_dbus(void)
{
  struct server *serv;

  /* mark everything from DBUS */
  for (serv = daemon->servers; serv; serv = serv->next)
    if (serv->flags & SERV_FROM_DBUS)
      serv->flags |= SERV_MARK;
}

static void cleanup_dbus()
{
  struct server *serv, *tmp, **up;

  /* unlink and free anything still marked. */
  for (serv = daemon->servers, up = &daemon->servers; serv; serv = tmp) 
    {
      tmp = serv->next;
      if (serv->flags & SERV_MARK)
       {
         server_gone(serv);
         *up = serv->next;
         if (serv->domain)
	   free(serv->domain);
	 free(serv);
       }
      else 
       up = &serv->next;
    }
}

static void dbus_read_servers(DBusMessage *message)
{
  DBusMessageIter iter;
  union  mysockaddr addr, source_addr;
  char *domain;
  
  dbus_message_iter_init(message, &iter);

  mark_dbus();

  while (1)
    {
      int skip = 0;

      if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32)
	{
	  u32 a;
	  
	  dbus_message_iter_get_basic(&iter, &a);
	  dbus_message_iter_next (&iter);
	  
#ifdef HAVE_SOCKADDR_SA_LEN
	  source_addr.in.sin_len = addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
	  addr.in.sin_addr.s_addr = ntohl(a);
	  source_addr.in.sin_family = addr.in.sin_family = AF_INET;
	  addr.in.sin_port = htons(NAMESERVER_PORT);
	  source_addr.in.sin_addr.s_addr = INADDR_ANY;
	  source_addr.in.sin_port = htons(daemon->query_port);
	}
      else if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_BYTE)
	{
	  unsigned char p[sizeof(struct in6_addr)];
	  unsigned int i;

	  skip = 1;

	  for(i = 0; i < sizeof(struct in6_addr); i++)
	    {
	      dbus_message_iter_get_basic(&iter, &p[i]);
	      dbus_message_iter_next (&iter);
	      if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_BYTE)
		break;
	    }

#ifndef HAVE_IPV6
	  my_syslog(LOG_WARNING, _("attempt to set an IPv6 server address via DBus - no IPv6 support"));
#else
	  if (i == sizeof(struct in6_addr)-1)
	    {
	      memcpy(&addr.in6.sin6_addr, p, sizeof(struct in6_addr));
#ifdef HAVE_SOCKADDR_SA_LEN
              source_addr.in6.sin6_len = addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
              source_addr.in6.sin6_family = addr.in6.sin6_family = AF_INET6;
              addr.in6.sin6_port = htons(NAMESERVER_PORT);
              source_addr.in6.sin6_flowinfo = addr.in6.sin6_flowinfo = 0;
	      source_addr.in6.sin6_scope_id = addr.in6.sin6_scope_id = 0;
              source_addr.in6.sin6_addr = in6addr_any;
              source_addr.in6.sin6_port = htons(daemon->query_port);
	      skip = 0;
	    }
#endif
	}
      else
	/* At the end */
	break;
      
      /* process each domain */
      do {
	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING)
	  {
	    dbus_message_iter_get_basic(&iter, &domain);
	    dbus_message_iter_next (&iter);
	  }
	else
	  domain = NULL;
	
	if (!skip)
	  add_update_server(&addr, &source_addr, NULL, domain);
     
      } while (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING); 
    }
   
  /* unlink and free anything still marked. */
  cleanup_dbus();
}

static DBusMessage* dbus_read_servers_ex(DBusMessage *message, int strings)
{
  DBusMessageIter iter, array_iter, string_iter;
  DBusMessage *error = NULL;
  const char *addr_err;
  char *dup = NULL;
  
  my_syslog(LOG_INFO, _("setting upstream servers from DBus"));
  
  if (!dbus_message_iter_init(message, &iter))
    {
      return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
                                    "Failed to initialize dbus message iter");
    }

  /* check that the message contains an array of arrays */
  if ((dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) ||
      (dbus_message_iter_get_element_type(&iter) != (strings ? DBUS_TYPE_STRING : DBUS_TYPE_ARRAY)))
    {
      return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
                                    strings ? "Expected array of string" : "Expected array of string arrays");
     }
 
  mark_dbus();

  /* array_iter points to each "as" element in the outer array */
  dbus_message_iter_recurse(&iter, &array_iter);
  while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID)
    {
      const char *str = NULL;
      union  mysockaddr addr, source_addr;
      char interface[IF_NAMESIZE];
      char *str_addr, *str_domain = NULL;

      if (strings)
	{
	  dbus_message_iter_get_basic(&array_iter, &str);
	  if (!str || !strlen (str))
	    {
	      error = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					     "Empty string");
	      break;
	    }
	  
	  /* dup the string because it gets modified during parsing */
	  if (!(dup = str_domain = whine_malloc(strlen(str)+1)))
	    break;

	  strcpy(str_domain, str);

	  /* point to address part of old string for error message */
	  if ((str_addr = strrchr(str, '/')))
	    str = str_addr+1;
	  
	  if ((str_addr = strrchr(str_domain, '/')))
	    {
	      if (*str_domain != '/' || str_addr == str_domain)
		{
		  error = dbus_message_new_error_printf(message,
							DBUS_ERROR_INVALID_ARGS,
							"No domain terminator '%s'",
							str);
		  break;
		}
	      *str_addr++ = 0;
	      str_domain++;
	    }
	  else
	    {
	      str_addr = str_domain;
	      str_domain = NULL;
	    }

	  
	}
      else
	{
	  /* check the types of the struct and its elements */
	  if ((dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_ARRAY) ||
	      (dbus_message_iter_get_element_type(&array_iter) != DBUS_TYPE_STRING))
	    {
	      error = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					     "Expected inner array of strings");
	      break;
	    }
	  
	  /* string_iter points to each "s" element in the inner array */
	  dbus_message_iter_recurse(&array_iter, &string_iter);
	  if (dbus_message_iter_get_arg_type(&string_iter) != DBUS_TYPE_STRING)
	    {
	      /* no IP address given */
	      error = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					     "Expected IP address");
	      break;
	    }
	  
	  dbus_message_iter_get_basic(&string_iter, &str);
	  if (!str || !strlen (str))
	    {
	      error = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					     "Empty IP address");
	      break;
	    }
	  
	  /* dup the string because it gets modified during parsing */
	  if (!(dup = str_addr = whine_malloc(strlen(str)+1)))
	    break;
	   
	  strcpy(str_addr, str);
	}

      memset(&addr, 0, sizeof(addr));
      memset(&source_addr, 0, sizeof(source_addr));
      memset(&interface, 0, sizeof(interface));

      /* parse the IP address */
      addr_err = parse_server(str_addr, &addr, &source_addr, (char *) &interface, NULL);

      if (addr_err)
        {
          error = dbus_message_new_error_printf(message, DBUS_ERROR_INVALID_ARGS,
                                                "Invalid IP address '%s': %s",
                                                str, addr_err);
          break;
        }

      if (strings)
	{
	  char *p;
	  
	  do {
	    if (str_domain)
	      {
		if ((p = strchr(str_domain, '/')))
		  *p++ = 0;
	      }
	    else 
	      p = NULL;
	    
	    add_update_server(&addr, &source_addr, interface, str_domain);
	  } while ((str_domain = p));
	}
      else
	{
	  /* jump past the address to the domain list (if any) */
	  dbus_message_iter_next (&string_iter);
	  
	  /* parse domains and add each server/domain pair to the list */
	  do {
	    str = NULL;
	    if (dbus_message_iter_get_arg_type(&string_iter) == DBUS_TYPE_STRING)
	      dbus_message_iter_get_basic(&string_iter, &str);
	    dbus_message_iter_next (&string_iter);
	    
	    add_update_server(&addr, &source_addr, interface, str);
	  } while (dbus_message_iter_get_arg_type(&string_iter) == DBUS_TYPE_STRING);
	}
	 
      /* jump to next element in outer array */
      dbus_message_iter_next(&array_iter);
    }

  cleanup_dbus();

  if (dup)
    free(dup);

  return error;
}

DBusHandlerResult message_handler(DBusConnection *connection, 
				  DBusMessage *message, 
				  void *user_data)
{
  char *method = (char *)dbus_message_get_member(message);
  DBusMessage *reply = NULL;
    
  if (dbus_message_is_method_call(message, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
    {
      /* string length: "%s" provides space for termination zero */
      if (!introspection_xml && 
	  (introspection_xml = whine_malloc(strlen(introspection_xml_template) + strlen(daemon->dbus_name))))
	sprintf(introspection_xml, introspection_xml_template, daemon->dbus_name);
    
      if (introspection_xml)
	{
	  reply = dbus_message_new_method_return(message);
	  dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspection_xml, DBUS_TYPE_INVALID);
	}
    }
  else if (strcmp(method, "GetVersion") == 0)
    {
      char *v = VERSION;
      reply = dbus_message_new_method_return(message);
      
      dbus_message_append_args(reply, DBUS_TYPE_STRING, &v, DBUS_TYPE_INVALID);
    }
  else if (strcmp(method, "SetServers") == 0)
    {
      my_syslog(LOG_INFO, _("setting upstream servers from DBus"));
      dbus_read_servers(message);
      check_servers();
    }
  else if (strcmp(method, "SetServersEx") == 0)
    {
      reply = dbus_read_servers_ex(message, 0);
      check_servers();
    }
  else if (strcmp(method, "SetDomainServers") == 0)
    {
      reply = dbus_read_servers_ex(message, 1);
      check_servers();
    }
  else if (strcmp(method, "ClearCache") == 0)
    clear_cache_and_reload(dnsmasq_time());
  else
    return (DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
  
  method = user_data; /* no warning */

  /* If no reply or no error, return nothing */
  if (!reply)
    reply = dbus_message_new_method_return(message);

  if (reply)
    {
      dbus_connection_send (connection, reply, NULL);
      dbus_message_unref (reply);
    }

  return (DBUS_HANDLER_RESULT_HANDLED);
}
 

/* returns NULL or error message, may fail silently if dbus daemon not yet up. */
char *dbus_init(void)
{
  DBusConnection *connection = NULL;
  DBusObjectPathVTable dnsmasq_vtable = {NULL, &message_handler, NULL, NULL, NULL, NULL };
  DBusError dbus_error;
  DBusMessage *message;

  dbus_error_init (&dbus_error);
  if (!(connection = dbus_bus_get (DBUS_BUS_SYSTEM, &dbus_error)))
    return NULL;
    
  dbus_connection_set_exit_on_disconnect(connection, FALSE);
  dbus_connection_set_watch_functions(connection, add_watch, remove_watch, 
				      NULL, NULL, NULL);
  dbus_error_init (&dbus_error);
  dbus_bus_request_name (connection, daemon->dbus_name, 0, &dbus_error);
  if (dbus_error_is_set (&dbus_error))
    return (char *)dbus_error.message;
  
  if (!dbus_connection_register_object_path(connection,  DNSMASQ_PATH, 
					    &dnsmasq_vtable, NULL))
    return _("could not register a DBus message handler");
  
  daemon->dbus = connection; 
  
  if ((message = dbus_message_new_signal(DNSMASQ_PATH, daemon->dbus_name, "Up")))
    {
      dbus_connection_send(connection, message, NULL);
      dbus_message_unref(message);
    }

  return NULL;
}
 

void set_dbus_listeners(int *maxfdp,
			fd_set *rset, fd_set *wset, fd_set *eset)
{
  struct watch *w;
  
  for (w = daemon->watches; w; w = w->next)
    if (dbus_watch_get_enabled(w->watch))
      {
	unsigned int flags = dbus_watch_get_flags(w->watch);
	int fd = dbus_watch_get_unix_fd(w->watch);
	
	bump_maxfd(fd, maxfdp);
	
	if (flags & DBUS_WATCH_READABLE)
	  FD_SET(fd, rset);
	
	if (flags & DBUS_WATCH_WRITABLE)
	  FD_SET(fd, wset);
	
	FD_SET(fd, eset);
      }
}

void check_dbus_listeners(fd_set *rset, fd_set *wset, fd_set *eset)
{
  DBusConnection *connection = (DBusConnection *)daemon->dbus;
  struct watch *w;

  for (w = daemon->watches; w; w = w->next)
    if (dbus_watch_get_enabled(w->watch))
      {
	unsigned int flags = 0;
	int fd = dbus_watch_get_unix_fd(w->watch);
	
	if (FD_ISSET(fd, rset))
	  flags |= DBUS_WATCH_READABLE;
	
	if (FD_ISSET(fd, wset))
	  flags |= DBUS_WATCH_WRITABLE;
	
	if (FD_ISSET(fd, eset))
	  flags |= DBUS_WATCH_ERROR;

	if (flags != 0)
	  dbus_watch_handle(w->watch, flags);
      }

  if (connection)
    {
      dbus_connection_ref (connection);
      while (dbus_connection_dispatch (connection) == DBUS_DISPATCH_DATA_REMAINS);
      dbus_connection_unref (connection);
    }
}

#ifdef HAVE_DHCP
void emit_dbus_signal(int action, struct dhcp_lease *lease, char *hostname)
{
  DBusConnection *connection = (DBusConnection *)daemon->dbus;
  DBusMessage* message = NULL;
  DBusMessageIter args;
  char *action_str, *mac = daemon->namebuff;
  unsigned char *p;
  int i;

  if (!connection)
    return;
  
  if (!hostname)
    hostname = "";
  
#ifdef HAVE_DHCP6
   if (lease->flags & (LEASE_TA | LEASE_NA))
     {
       print_mac(mac, lease->clid, lease->clid_len);
       inet_ntop(AF_INET6, lease->hwaddr, daemon->addrbuff, ADDRSTRLEN);
     }
   else
#endif
     {
       p = extended_hwaddr(lease->hwaddr_type, lease->hwaddr_len,
			   lease->hwaddr, lease->clid_len, lease->clid, &i);
       print_mac(mac, p, i);
       inet_ntop(AF_INET, &lease->addr, daemon->addrbuff, ADDRSTRLEN);
     }

  if (action == ACTION_DEL)
    action_str = "DhcpLeaseDeleted";
  else if (action == ACTION_ADD)
    action_str = "DhcpLeaseAdded";
  else if (action == ACTION_OLD)
    action_str = "DhcpLeaseUpdated";
  else
    return;

  if (!(message = dbus_message_new_signal(DNSMASQ_PATH, daemon->dbus_name, action_str)))
    return;
  
  dbus_message_iter_init_append(message, &args);
  
  if (dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &daemon->addrbuff) &&
      dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &mac) &&
      dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &hostname))
    dbus_connection_send(connection, message, NULL);
  
  dbus_message_unref(message);
}
#endif

#endif
