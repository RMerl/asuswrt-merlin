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

#ifdef HAVE_DHCP6

static int dhcp6_maybe_relay(struct in6_addr *link_address, struct dhcp_netid **relay_tagsp, struct dhcp_context *context, 
			     int interface, char *iface_name, struct in6_addr *fallback, void *inbuff, size_t sz, int is_unicast, time_t now);
static int dhcp6_no_relay(int msg_type,  struct in6_addr *link_address, struct dhcp_netid *tags, struct dhcp_context *context, 
			  int interface, char *iface_name, struct in6_addr *fallback, void *inbuff, size_t sz, int is_unicast, time_t now);
static void log6_opts(int nest, unsigned int xid, void *start_opts, void *end_opts);
static void log6_packet(char *type, unsigned char *clid, int clid_len, struct in6_addr *addr, int xid, char *iface, char *string);

static void *opt6_find (void *opts, void *end, unsigned int search, unsigned int minsize);
static void *opt6_next(void *opts, void *end);
static unsigned int opt6_uint(unsigned char *opt, int offset, int size);

#define opt6_len(opt) ((int)(opt6_uint(opt, -2, 2)))
#define opt6_type(opt) (opt6_uint(opt, -4, 2))
#define opt6_ptr(opt, i) ((void *)&(((unsigned char *)(opt))[4+(i)]))


unsigned short dhcp6_reply(struct dhcp_context *context, int interface, char *iface_name,
			   struct in6_addr *fallback, size_t sz, int is_unicast, time_t now)
{
  struct dhcp_netid *relay_tags = NULL;
  struct dhcp_vendor *vendor;
  int msg_type;
  
  if (sz <= 4)
    return 0;
  
  msg_type = *((unsigned char *)daemon->dhcp_packet.iov_base);
  
  /* Mark these so we only match each at most once, to avoid tangled linked lists */
  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
    vendor->netid.next = &vendor->netid;
  
  save_counter(0);
  
  if (dhcp6_maybe_relay(NULL, &relay_tags, context, interface, iface_name, fallback, daemon->dhcp_packet.iov_base, sz, is_unicast, now))
    return msg_type == DHCP6RELAYFORW ? DHCPV6_SERVER_PORT : DHCPV6_CLIENT_PORT;

  return 0;
}

/* This cost me blood to write, it will probably cost you blood to understand - srk. */
static int dhcp6_maybe_relay(struct in6_addr *link_address, struct dhcp_netid **relay_tagsp, struct dhcp_context *context,
			     int interface, char *iface_name, struct in6_addr *fallback, void *inbuff, size_t sz, int is_unicast, time_t now)
{
  void *end = inbuff + sz;
  void *opts = inbuff + 34;
  int msg_type = *((unsigned char *)inbuff);
  unsigned char *outmsgtypep;
  void *opt;
  struct dhcp_vendor *vendor;

  /* if not an encaplsulated relayed message, just do the stuff */
  if (msg_type != DHCP6RELAYFORW)
    {
      /* if link_address != NULL if points to the link address field of the 
	 innermost nested RELAYFORW message, which is where we find the
	 address of the network on which we can allocate an address.
	 Recalculate the available contexts using that information. */
      
      if (link_address)
	{
	  struct dhcp_context *c;
	  context = NULL;
	  
	  for (c = daemon->dhcp6; c; c = c->next)
	    if (!IN6_IS_ADDR_LOOPBACK(link_address) &&
		!IN6_IS_ADDR_LINKLOCAL(link_address) &&
		!IN6_IS_ADDR_MULTICAST(link_address) &&
		is_same_net6(link_address, &c->start6, c->prefix) &&
		is_same_net6(link_address, &c->end6, c->prefix))
	      {
		c->current = context;
		context = c;
	      }

	  if (!context)
	    {
	      inet_ntop(AF_INET6, link_address, daemon->addrbuff, ADDRSTRLEN); 
	      my_syslog(MS_DHCP | LOG_WARNING, 
			_("no address range available for DHCPv6 request from relay at %s"),
			daemon->addrbuff);
	      return 0;
	    }
	}

      if (!context)
	{
	  my_syslog(MS_DHCP | LOG_WARNING, 
		    _("no address range available for DHCPv6 request via %s"), iface_name);
	  return 0;
	}

      return dhcp6_no_relay(msg_type, link_address, *relay_tagsp, context, interface, iface_name, fallback, inbuff, sz, is_unicast, now);
    }

  /* must have at least msg_type+hopcount+link_address+peer_address+minimal size option
     which is               1   +    1   +    16      +     16     + 2 + 2 = 38 */
  if (sz < 38)
    return 0;
  
  /* copy header stuff into reply message and set type to reply */
  outmsgtypep = put_opt6(inbuff, 34);
  *outmsgtypep = DHCP6RELAYREPL;

  /* look for relay options and set tags if found. */
  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
    {
      int mopt;
      
      if (vendor->match_type == MATCH_SUBSCRIBER)
	mopt = OPTION6_SUBSCRIBER_ID;
      else if (vendor->match_type == MATCH_REMOTE)
	mopt = OPTION6_REMOTE_ID; 
      else
	continue;

      if ((opt = opt6_find(opts, end, mopt, 1)) &&
	  vendor->len == opt6_len(opt) &&
	  memcmp(vendor->data, opt6_ptr(opt, 0), vendor->len) == 0 &&
	  vendor->netid.next != &vendor->netid)
	{
	  vendor->netid.next = *relay_tagsp;
	  *relay_tagsp = &vendor->netid;
	  break;
	}
    }
  
  for (opt = opts; opt; opt = opt6_next(opt, end))
    {
      int o = new_opt6(opt6_type(opt));
      if (opt6_type(opt) == OPTION6_RELAY_MSG)
	{
	  struct in6_addr link_address;
	  /* the packet data is unaligned, copy to aligned storage */
	  memcpy(&link_address, inbuff + 2, IN6ADDRSZ); 
	  /* Not, zero is_unicast since that is now known to refer to the 
	     relayed packet, not the original sent by the client */
	  if (!dhcp6_maybe_relay(&link_address, relay_tagsp, context, interface, iface_name, fallback, opt6_ptr(opt, 0), opt6_len(opt), 0, now))
	    return 0;
	}
      else
	put_opt6(opt6_ptr(opt, 0), opt6_len(opt));
      end_opt6(o);	    
    }
  
  return 1;
}

static int dhcp6_no_relay(int msg_type, struct in6_addr *link_address, struct dhcp_netid *tags, struct dhcp_context *context, 
			  int interface, char *iface_name, struct in6_addr *fallback, void *inbuff, size_t sz, int is_unicast, time_t now)
{
  void *packet_options = inbuff + 4;
  void *end = inbuff + sz;
  void *opt, *oro;
  int i, o, o1;
  unsigned char *clid = NULL;
  int clid_len = 0, start_opts;
  struct dhcp_netid *tagif, *context_tags = NULL;
  char *client_hostname= NULL, *hostname = NULL;
  char *domain = NULL, *send_domain = NULL;
  struct dhcp_config *config = NULL;
  struct dhcp_netid known_id, iface_id, v6_id;
  int done_dns = 0, hostname_auth = 0, do_encap = 0;
  unsigned char *outmsgtypep;
  struct dhcp_opt *opt_cfg;
  struct dhcp_vendor *vendor;
  struct dhcp_context *context_tmp;
  unsigned int xid, ignore = 0;
  unsigned int fqdn_flags = 0x01; /* default to send if we recieve no FQDN option */

  /* set tag with name == interface */
  iface_id.net = iface_name;
  iface_id.next = tags;
  tags = &iface_id; 

  /* set tag "dhcpv6" */
  v6_id.net = "dhcpv6";
  v6_id.next = tags;
  tags = &v6_id;

  /* copy over transaction-id, and save pointer to message type */
  outmsgtypep = put_opt6(inbuff, 4);
  start_opts = save_counter(-1);
  xid = outmsgtypep[3] | outmsgtypep[2] << 8 | outmsgtypep[1] << 16;
   
  /* We're going to be linking tags from all context we use. 
     mark them as unused so we don't link one twice and break the list */
  for (context_tmp = context; context_tmp; context_tmp = context_tmp->current)
    {
      context->netid.next = &context->netid;

      if (option_bool(OPT_LOG_OPTS))
	{
	   inet_ntop(AF_INET6, &context_tmp->start6, daemon->dhcp_buff, ADDRSTRLEN); 
	   inet_ntop(AF_INET6, &context_tmp->end6, daemon->dhcp_buff2, ADDRSTRLEN); 
	   if (context_tmp->flags & (CONTEXT_STATIC))
	     my_syslog(MS_DHCP | LOG_INFO, _("%u available DHCPv6 subnet: %s/%d"),
		       xid, daemon->dhcp_buff, context_tmp->prefix);
	   else
	     my_syslog(MS_DHCP | LOG_INFO, _("%u available DHCP range: %s -- %s"), 
		       xid, daemon->dhcp_buff, daemon->dhcp_buff2);
	}
    }

  if ((opt = opt6_find(packet_options, end, OPTION6_CLIENT_ID, 1)))
    {
      clid = opt6_ptr(opt, 0);
      clid_len = opt6_len(opt);
      o = new_opt6(OPTION6_CLIENT_ID);
      put_opt6(clid, clid_len);
      end_opt6(o);
    }
  else if (msg_type != DHCP6IREQ)
    return 0;

  /* server-id must match except for SOLICIT and CONFIRM messages */
  if (msg_type != DHCP6SOLICIT && msg_type != DHCP6CONFIRM && msg_type != DHCP6IREQ &&
      (!(opt = opt6_find(packet_options, end, OPTION6_SERVER_ID, 1)) ||
       opt6_len(opt) != daemon->duid_len ||
       memcmp(opt6_ptr(opt, 0), daemon->duid, daemon->duid_len) != 0))
    return 0;
  
  o = new_opt6(OPTION6_SERVER_ID);
  put_opt6(daemon->duid, daemon->duid_len);
  end_opt6(o);

  if (is_unicast &&
      (msg_type == DHCP6REQUEST || msg_type == DHCP6RENEW || msg_type == DHCP6RELEASE || msg_type == DHCP6DECLINE))
    
    {  
      o1 = new_opt6(OPTION6_STATUS_CODE);
      put_opt6_short(DHCP6USEMULTI);
      put_opt6_string("Use multicast");
      end_opt6(o1);
      return 1;
    }

  /* match vendor and user class options */
  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
    {
      int mopt;
      
      if (vendor->match_type == MATCH_VENDOR)
	mopt = OPTION6_VENDOR_CLASS;
      else if (vendor->match_type == MATCH_USER)
	mopt = OPTION6_USER_CLASS; 
      else
	continue;

      if ((opt = opt6_find(packet_options, end, mopt, 2)))
	{
	  void *enc_opt, *enc_end = opt6_ptr(opt, opt6_len(opt));
	  int offset = 0;
	  
	  if (mopt == OPTION6_VENDOR_CLASS)
	    {
	      if (opt6_len(opt) < 4)
		continue;
	      
	      if (vendor->enterprise != opt6_uint(opt, 0, 4))
		continue;
	    
	      offset = 4;
	    }
 
	  for (enc_opt = opt6_ptr(opt, offset); enc_opt; enc_opt = opt6_next(enc_opt, enc_end))
	    for (i = 0; i <= (opt6_len(enc_opt) - vendor->len); i++)
	      if (memcmp(vendor->data, opt6_ptr(enc_opt, i), vendor->len) == 0)
		{
		  vendor->netid.next = tags;
		  tags = &vendor->netid;
		  break;
		}
	}
    }

  if (option_bool(OPT_LOG_OPTS) && (opt = opt6_find(packet_options, end, OPTION6_VENDOR_CLASS, 4)))
    my_syslog(MS_DHCP | LOG_INFO, _("%u vendor class: %u"), xid, opt6_uint(opt, 0, 4));
  
  /* dhcp-match. If we have hex-and-wildcards, look for a left-anchored match.
     Otherwise assume the option is an array, and look for a matching element. 
     If no data given, existance of the option is enough. This code handles 
     V-I opts too. */
  for (opt_cfg = daemon->dhcp_match6; opt_cfg; opt_cfg = opt_cfg->next)
    {
      int match = 0;
      
      if (opt_cfg->flags & DHOPT_RFC3925)
	{
	  for (opt = opt6_find(packet_options, end, OPTION6_VENDOR_OPTS, 4);
	       opt;
	       opt = opt6_find(opt6_next(opt, end), end, OPTION6_VENDOR_OPTS, 4))
	    {
	      void *vopt;
	      void *vend = opt6_ptr(opt, opt6_len(opt));
	      
	      for (vopt = opt6_find(opt6_ptr(opt, 4), vend, opt_cfg->opt, 0);
		   vopt;
		   vopt = opt6_find(opt6_next(vopt, vend), vend, opt_cfg->opt, 0))
		if ((match = match_bytes(opt_cfg, opt6_ptr(vopt, 0), opt6_len(vopt))))
		  break;
	    }
	  if (match)
	    break;
	}
      else
	{
	  if (!(opt = opt6_find(packet_options, end, opt_cfg->opt, 1)))
	    continue;
	  
	  match = match_bytes(opt_cfg, opt6_ptr(opt, 0), opt6_len(opt));
	} 
  
      if (match)
	{
	  opt_cfg->netid->next = tags;
	  tags = opt_cfg->netid;
	}
    }
  
  if ((opt = opt6_find(packet_options, end, OPTION6_FQDN, 1)))
    {
      /* RFC4704 refers */
       int len = opt6_len(opt) - 1;
       
       fqdn_flags = opt6_uint(opt, 0, 1);
       
       /* Always force update, since the client has no way to do it itself. */
       if (!option_bool(OPT_FQDN_UPDATE) && !(fqdn_flags & 0x01))
	 fqdn_flags |= 0x03;
 
       fqdn_flags &= ~0x04;

       if (len != 0 && len < 255)
	 {
	   unsigned char *pp, *op = opt6_ptr(opt, 1);
	   char *pq = daemon->dhcp_buff;
	   
	   pp = op;
	   while (*op != 0 && ((op + (*op)) - pp) < len)
	     {
	       memcpy(pq, op+1, *op);
	       pq += *op;
	       op += (*op)+1;
	       *(pq++) = '.';
	     }
	   
	   if (pq != daemon->dhcp_buff)
	     pq--;
	   *pq = 0;
	   
	   if (legal_hostname(daemon->dhcp_buff))
	     {
	       client_hostname = daemon->dhcp_buff;
	       if (option_bool(OPT_LOG_OPTS))
		 my_syslog(MS_DHCP | LOG_INFO, _("%u client provides name: %s"), xid, client_hostname); 
	     }
	 }
    }	 
  
  if (clid)
    {
      config = find_config6(daemon->dhcp_conf, context, clid, clid_len, NULL);
      
      if (have_config(config, CONFIG_NAME))
	{
	  hostname = config->hostname;
	  domain = config->domain;
	  hostname_auth = 1;
	}
      else if (client_hostname)
	{
	  domain = strip_hostname(client_hostname);
	  
	  if (strlen(client_hostname) != 0)
	    {
	      hostname = client_hostname;
	      if (!config)
		{
		  /* Search again now we have a hostname. 
		     Only accept configs without CLID here, (it won't match)
		     to avoid impersonation by name. */
		  struct dhcp_config *new = find_config6(daemon->dhcp_conf, context, NULL, 0, hostname);
		  if (new && !have_config(new, CONFIG_CLID) && !new->hwaddr)
		    config = new;
		}
	    }
	}
    }

  if (config)
    {
      struct dhcp_netid_list *list;
      
      for (list = config->netid; list; list = list->next)
        {
          list->list->next = tags;
          tags = list->list;
        }

      /* set "known" tag for known hosts */
      known_id.net = "known";
      known_id.next = tags;
      tags = &known_id;

      if (have_config(config, CONFIG_DISABLE))
	ignore = 1;
    }
   
  tagif = run_tag_if(tags);
  
  /* if all the netids in the ignore list are present, ignore this client */
  if (daemon->dhcp_ignore)
    {
      struct dhcp_netid_list *id_list;
     
      for (id_list = daemon->dhcp_ignore; id_list; id_list = id_list->next)
	if (match_netid(id_list->list, tagif, 0))
	  ignore = 1;
    }
  
  /* if all the netids in the ignore_name list are present, ignore client-supplied name */
  if (!hostname_auth)
    {
       struct dhcp_netid_list *id_list;
       
       for (id_list = daemon->dhcp_ignore_names; id_list; id_list = id_list->next)
	 if ((!id_list->list) || match_netid(id_list->list, tagif, 0))
	   break;
       if (id_list)
	 hostname = NULL;
    }
  

  switch (msg_type)
    {
    default:
      return 0;

    case DHCP6SOLICIT:
    case DHCP6REQUEST:
      {
	void *rapid_commit = opt6_find(packet_options, end, OPTION6_RAPID_COMMIT, 0);
	int make_lease = (msg_type == DHCP6REQUEST || rapid_commit); 
	int serial = 0, used_config = 0;

	if (rapid_commit)
	  {
	    o = new_opt6(OPTION6_RAPID_COMMIT);
	    end_opt6(o);
	  }

	/* set reply message type */
	*outmsgtypep = make_lease ? DHCP6REPLY : DHCP6ADVERTISE;
	
	log6_packet(msg_type == DHCP6SOLICIT ? "DHCPSOLICIT" : "DHCPREQUEST",
		    clid, clid_len, NULL, xid, iface_name, ignore ? "ignored" : NULL);
	
	if (ignore)
	  return 0;
	
	for (opt = packet_options; opt; opt = opt6_next(opt, end))
	  {   
	    int iaid, ia_type = opt6_type(opt);
	    void *ia_option, *ia_end;
	    unsigned int min_time = 0xffffffff;
	    int t1cntr = 0;
	    int address_assigned = 0;
	    
	    if (ia_type != OPTION6_IA_NA && ia_type != OPTION6_IA_TA)
	      continue;
	    
	    if (ia_type == OPTION6_IA_NA && opt6_len(opt) < 12)
	      continue;
	    
	    if (ia_type == OPTION6_IA_TA && opt6_len(opt) < 4)
	      continue;
	    
	    iaid = opt6_uint(opt, 0, 4);
	    ia_end = opt6_ptr(opt, opt6_len(opt));
	    ia_option =  opt6_find(opt6_ptr(opt, ia_type == OPTION6_IA_NA ? 12 : 4), ia_end, OPTION6_IAADDR, 24);
	    
	    /* reset "USED" flags on leases */
	    lease6_filter(ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, context);
	    
	    o = new_opt6(ia_type);
	    put_opt6_long(iaid);
	    if (ia_type == OPTION6_IA_NA)
	      {
		/* save pointer */
		t1cntr = save_counter(-1);
		/* so we can fill these in later */
		put_opt6_long(0);
		put_opt6_long(0); 
	      }
	    
	    while (1)
	      {
		struct in6_addr alloced_addr, *addrp = NULL;
		u32 requested_time = 0;
		struct dhcp_lease *lease = NULL;
		
		if (ia_option)
		  {
		    struct in6_addr *req_addr = opt6_ptr(ia_option, 0);
		    requested_time = opt6_uint(ia_option, 16, 4);
		    
		    if (!address6_available(context, req_addr, tags) &&
			(!have_config(config, CONFIG_ADDR6) || memcmp(&config->addr6, req_addr, IN6ADDRSZ) != 0))
		      {
			if (msg_type == DHCP6REQUEST)
			  {
			    /* host has a lease, but it's not on the correct link */
			    o1 = new_opt6(OPTION6_STATUS_CODE);
			    put_opt6_short(DHCP6NOTONLINK);
			    put_opt6_string("Not on link");
			    end_opt6(o1);
			  }
		      }
		    else if ((lease = lease6_find(NULL, 0, ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, 
						  iaid, req_addr)) &&
			     (clid_len != lease->clid_len ||
			      memcmp(clid, lease->clid, clid_len) != 0))
		      {
			/* Address leased to another DUID */
			o1 = new_opt6(OPTION6_STATUS_CODE);
			put_opt6_short(DHCP6UNSPEC);
			put_opt6_string("Address in use");
			end_opt6(o1);
		      } 
		    else
		      addrp = req_addr;
		  }
		else
		  {
		    /* must have an address to CONFIRM */
		    if (msg_type == DHCP6REQUEST && ia_type == OPTION6_IA_NA)
		      return 0;
		    
		    /* Don't used configured addresses for temporary leases. */
		    if (have_config(config, CONFIG_ADDR6) && !used_config && ia_type == OPTION6_IA_NA)
		      {
			struct dhcp_lease *ltmp = lease6_find_by_addr(&config->addr6, 128, 0);
			 
			used_config = 1;
			inet_ntop(AF_INET6, &config->addr6, daemon->addrbuff, ADDRSTRLEN);
			
			if (ltmp && ltmp->clid && 
			    (ltmp->clid_len != clid_len || memcmp(ltmp->clid, clid, clid_len) != 0))
			  my_syslog(MS_DHCP | LOG_WARNING, _("not using configured address %s because it is leased to %s"),
				    daemon->addrbuff, print_mac(daemon->namebuff, ltmp->clid, ltmp->clid_len));
			else if (have_config(config, CONFIG_DECLINED) &&
				 difftime(now, config->decline_time) < (float)DECLINE_BACKOFF)
			  my_syslog(MS_DHCP | LOG_WARNING, _("not using configured address %s because it was previously declined"), 
				    daemon->addrbuff);
			else
			  {
			    addrp = &config->addr6;
			    /* may have existing lease for this address */
			    lease = lease6_find(clid, clid_len, 
						ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, addrp); 
			  }
		      }
		    
		    /* existing lease */
		    if (!addrp &&
			(lease = lease6_find(clid, clid_len, 
					     ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, NULL)) &&
			!config_find_by_address6(daemon->dhcp_conf, (struct in6_addr *)&lease->hwaddr, 128, 0))
		      addrp = (struct in6_addr *)&lease->hwaddr;
		    
		    if (!addrp && address6_allocate(context, clid, clid_len, serial++, tags, &alloced_addr))
		      addrp = &alloced_addr;		    
		  }
		
		if (addrp)
		  {
		    unsigned int lease_time;
		    struct dhcp_context *this_context;
		    struct dhcp_config *valid_config = config;

		    /* don't use a config to set lease time if it specifies an address which isn't this. */
		    if (have_config(config, CONFIG_ADDR6) && memcmp(&config->addr6, addrp, IN6ADDRSZ) != 0)
		      valid_config = NULL;

		    address_assigned = 1;
		    
		    /* shouldn't ever fail */
		    if ((this_context = narrow_context6(context, addrp, tagif)))
		      {
			/* get tags from context if we've not used it before */
			if (this_context->netid.next == &this_context->netid && this_context->netid.net)
			  {
			    this_context->netid.next = context_tags;
			    context_tags = &this_context->netid;
			    if (!hostname_auth)
			      {
				struct dhcp_netid_list *id_list;
				
				for (id_list = daemon->dhcp_ignore_names; id_list; id_list = id_list->next)
				  if ((!id_list->list) || match_netid(id_list->list, &this_context->netid, 0))
				    break;
				if (id_list)
				  hostname = NULL;
			      }
			  }
			
			lease_time = have_config(valid_config, CONFIG_TIME) ? valid_config->lease_time : this_context->lease_time;
			
			if (ia_option)
			  {
			    if (requested_time < 120u )
			      requested_time = 120u; /* sanity */
			    if (lease_time == 0xffffffff || (requested_time != 0xffffffff && requested_time < lease_time))
			      lease_time = requested_time;
			  }
			  
			if (lease_time < min_time)
			  min_time = lease_time;
			
			/* May fail to create lease */
			if (!lease && make_lease)
			  lease = lease6_allocate(addrp, ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA);
			
			if (lease)
			  {
			    lease_set_expires(lease, lease_time, now);
			    lease_set_hwaddr(lease, NULL, clid, 0, iaid, clid_len, now, 0);
			    lease_set_interface(lease, interface, now);
			    if (hostname && ia_type == OPTION6_IA_NA)
			      {
				char *addr_domain = get_domain6(addrp);
				if (!send_domain)
				  send_domain = addr_domain;
				lease_set_hostname(lease, hostname, hostname_auth, addr_domain, domain);
			      }

#ifdef HAVE_SCRIPT
			    if (daemon->lease_change_command)
			      {
				void *class_opt;
				lease->flags |= LEASE_CHANGED;
				free(lease->extradata);
				lease->extradata = NULL;
				lease->extradata_size = lease->extradata_len = 0;
				lease->hwaddr_len = 0; /* surrogate for no of vendor classes */

				if ((class_opt = opt6_find(packet_options, end, OPTION6_VENDOR_CLASS, 4)))
				  {
				    void *enc_opt, *enc_end = opt6_ptr(class_opt, opt6_len(class_opt));
				    lease->hwaddr_len++;
				    /* send enterprise number first  */
				    sprintf(daemon->dhcp_buff2, "%u", opt6_uint(class_opt, 0, 4));
				    lease_add_extradata(lease, (unsigned char *)daemon->dhcp_buff2, strlen(daemon->dhcp_buff2), 0);
				    
				    if (opt6_len(class_opt) >= 6) 
				      for (enc_opt = opt6_ptr(class_opt, 4); enc_opt; enc_opt = opt6_next(enc_opt, enc_end))
					{
					  lease->hwaddr_len++;
					  lease_add_extradata(lease, opt6_ptr(enc_opt, 0), opt6_len(enc_opt), 0);
					}
				  }
				
				lease_add_extradata(lease, (unsigned char *)client_hostname, 
						    client_hostname ? strlen(client_hostname) : 0, 0);				
				
				/* space-concat tag set */
				if (!tagif && !context_tags)
				  lease_add_extradata(lease, NULL, 0, 0);
				else
				  {
				    struct dhcp_netid *n, *l, *tmp = tags;
				    
				    /* link temporarily */
				    for (n = context_tags; n && n->next; n = n->next);
				    if ((l = n))
				      {
					l->next = tags;
					tmp = context_tags;
				      }
				    
				    for (n = run_tag_if(tmp); n; n = n->next)
				      {
					struct dhcp_netid *n1;
					/* kill dupes */
					for (n1 = n->next; n1; n1 = n1->next)
					  if (strcmp(n->net, n1->net) == 0)
					    break;
					if (!n1)
					  lease_add_extradata(lease, (unsigned char *)n->net, strlen(n->net), n->next ? ' ' : 0); 
				      }

				    /* unlink again */
				    if (l)
				      l->next = NULL;
				  }

				if (link_address)
				  inet_ntop(AF_INET6, link_address, daemon->addrbuff, ADDRSTRLEN);
				
				lease_add_extradata(lease, (unsigned char *)daemon->addrbuff, link_address ? strlen(daemon->addrbuff) : 0, 0);
				  
				if ((class_opt = opt6_find(packet_options, end, OPTION6_USER_CLASS, 2)))
				  {
				    void *enc_opt, *enc_end = opt6_ptr(class_opt, opt6_len(class_opt));
				    for (enc_opt = opt6_ptr(class_opt, 0); enc_opt; enc_opt = opt6_next(enc_opt, enc_end))
				      lease_add_extradata(lease, opt6_ptr(enc_opt, 0), opt6_len(enc_opt), 0);
				  }
			      }
#endif	
			    
			  }
			else if (!send_domain)
			  send_domain = get_domain6(addrp);
			  
			
			if (lease || !make_lease)
			  {
			    o1 =  new_opt6(OPTION6_IAADDR);
			    put_opt6(addrp, sizeof(*addrp));
			    /* preferred lifetime */
			    put_opt6_long(this_context && (this_context->flags & CONTEXT_DEPRECATE) ? 0 : lease_time);
			    put_opt6_long(lease_time); /* valid lifetime */
			    end_opt6(o1);

			    log6_packet( make_lease ? "DHCPREPLY" : "DHCPADVERTISE", 
					 clid, clid_len, addrp, xid, iface_name, hostname);
			  }
			
		      }
		  }
		
		
		if (!ia_option || 
		    !(ia_option = opt6_find(opt6_next(ia_option, ia_end), ia_end, OPTION6_IAADDR, 24)))
		  {
		    if (address_assigned) 
		      {
			o1 = new_opt6(OPTION6_STATUS_CODE);
			put_opt6_short(DHCP6SUCCESS);
			put_opt6_string("Oh hai from dnsmasq");
			end_opt6(o1);
			
			if (ia_type == OPTION6_IA_NA)
			  {
			    /* go back an fill in fields in IA_NA option */
			    unsigned int t1 = min_time == 0xffffffff ? 0xffffffff : min_time/2;
			    unsigned int t2 = min_time == 0xffffffff ? 0xffffffff : (min_time/8) * 7;
			    int sav = save_counter(t1cntr);
			    put_opt6_long(t1);
			    put_opt6_long(t2);
			    save_counter(sav);
			  }
		      }
		    else
		      { 
			/* no address, return error */
			o1 = new_opt6(OPTION6_STATUS_CODE);
			put_opt6_short(DHCP6NOADDRS);
			put_opt6_string("No addresses available");
			end_opt6(o1);
		      }
		    
		    end_opt6(o);
		    	
		    if (address_assigned) 
		      {
			/* If --dhcp-authoritative is set, we can tell client not to wait for
			   other possible servers */
			o = new_opt6(OPTION6_PREFERENCE);
			put_opt6_char(option_bool(OPT_AUTHORITATIVE) ? 255 : 0);
			end_opt6(o);
		      }
		    
		    break;
		  }
	      } 
	  }
  
	break;
      }   
   
    case DHCP6RENEW:
      {
	/* set reply message type */
	*outmsgtypep = DHCP6REPLY;
	
	log6_packet("DHCPRENEW", clid, clid_len, NULL, xid, iface_name, NULL);

	for (opt = packet_options; opt; opt = opt6_next(opt, end))
	  {
	    int ia_type = opt6_type(opt);
	    void *ia_option, *ia_end;
	    unsigned int min_time = 0xffffffff;
	    int t1cntr = 0, iacntr;
	    unsigned int iaid;
	    
	    if (ia_type != OPTION6_IA_NA && ia_type != OPTION6_IA_TA)
	      continue;
	    
	    if (ia_type == OPTION6_IA_NA && opt6_len(opt) < 12)
	      continue;
   
	    if (ia_type == OPTION6_IA_TA && opt6_len(opt) < 4)
	      continue;
	    
	    iaid = opt6_uint(opt, 0, 4);
	    	      	      
	    o = new_opt6(ia_type);
	    put_opt6_long(iaid);
	    if (ia_type == OPTION6_IA_NA)
	      {
		/* save pointer */
		t1cntr = save_counter(-1);
		/* so we can fill these in later */
		put_opt6_long(0);
		put_opt6_long(0); 
	      }
	    
	    iacntr = save_counter(-1); 
	    
	    /* reset "USED" flags on leases */
	    lease6_filter(ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, context);
	    
	    ia_option = opt6_ptr(opt, ia_type == OPTION6_IA_NA ? 12 : 4);
	    ia_end = opt6_ptr(opt, opt6_len(opt));

	    for (ia_option = opt6_find(ia_option, ia_end, OPTION6_IAADDR, 24);
		 ia_option;
		 ia_option = opt6_find(opt6_next(ia_option, ia_end), ia_end, OPTION6_IAADDR, 24))
	      {
		struct dhcp_lease *lease = NULL;
		struct in6_addr *req_addr = opt6_ptr(ia_option, 0);
		u32 requested_time = opt6_uint(ia_option, 16, 4);
		unsigned int lease_time;
		struct dhcp_context *this_context;
		struct dhcp_config *valid_config = config;
		
		/* don't use a config to set lease time if it specifies an address which isn't this. */
		if (have_config(config, CONFIG_ADDR6) && memcmp(&config->addr6, req_addr, IN6ADDRSZ) != 0)
		  valid_config = NULL;

		if (!(lease = lease6_find(clid, clid_len,
					  ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, 
					  iaid, req_addr)))
		  {
		    /* If the server cannot find a client entry for the IA the server
		       returns the IA containing no addresses with a Status Code option set
		       to NoBinding in the Reply message. */
		    save_counter(iacntr);
		    t1cntr = 0;
		    
		    log6_packet("DHCPREPLY", clid, clid_len, req_addr, xid, iface_name, "lease not found");
		    
		    o1 = new_opt6(OPTION6_STATUS_CODE);
		    put_opt6_short(DHCP6NOBINDING);
		    put_opt6_string("No binding found");
		    end_opt6(o1);
		    break;
		  }
		
		
		if (!address6_available(context, req_addr, tagif) || 
		    !(this_context = narrow_context6(context, req_addr, tagif)))
		  {
		    lease_time = 0;
		    this_context = NULL;
		  }
		else
		  {
		    /* get tags from context if we've not used it before */
		    if (this_context->netid.next == &this_context->netid && this_context->netid.net)
		      {
			this_context->netid.next = context_tags;
			context_tags = &this_context->netid;
			if (!hostname_auth)
			  {
			    struct dhcp_netid_list *id_list;
			    
			    for (id_list = daemon->dhcp_ignore_names; id_list; id_list = id_list->next)
			      if ((!id_list->list) || match_netid(id_list->list, &this_context->netid, 0))
				break;
			    if (id_list)
			      hostname = NULL;
			  }
		      }
		    
		    lease_time = have_config(valid_config, CONFIG_TIME) ? valid_config->lease_time : this_context->lease_time;
		    
		    if (requested_time < 120u )
		      requested_time = 120u; /* sanity */
		    if (lease_time == 0xffffffff || (requested_time != 0xffffffff && requested_time < lease_time))
		      lease_time = requested_time;
		    
		    lease_set_expires(lease, lease_time, now);
		    if (ia_type == OPTION6_IA_NA && hostname)
		      {
			char *addr_domain = get_domain6(req_addr);
			if (!send_domain)
			  send_domain = addr_domain;
			lease_set_hostname(lease, hostname, hostname_auth, addr_domain, domain);
		      }
		    
		    if (lease_time < min_time)
		      min_time = lease_time;
		  }
		
		log6_packet("DHCPREPLY", clid, clid_len, req_addr, xid, iface_name, hostname);	
		
		o1 =  new_opt6(OPTION6_IAADDR);
		put_opt6(req_addr, sizeof(*req_addr));
		/* preferred lifetime */
		put_opt6_long(this_context && (this_context->flags & CONTEXT_DEPRECATE) ? 0 : lease_time);
		put_opt6_long(lease_time); /* valid lifetime */
		end_opt6(o1);
	      }
	    
	    if (t1cntr != 0)
	      {
		/* go back an fill in fields in IA_NA option */
		int sav = save_counter(t1cntr);
		unsigned int t1, t2, fuzz = rand16();
		
		while (fuzz > (min_time/16))
		  fuzz = fuzz/2; 
		t1 = min_time == 0xffffffff ? 0xffffffff : min_time/2 - fuzz;
		t2 = min_time == 0xffffffff ? 0xffffffff : ((min_time/8)*7) - fuzz;
		
		put_opt6_long(t1);
		put_opt6_long(t2);
		save_counter(sav);
	      }
	    
	    end_opt6(o);
	  }
	break;
	
      }
      
    case DHCP6CONFIRM:
      {
	/* set reply message type */
	*outmsgtypep = DHCP6REPLY;
	
	log6_packet("DHCPCONFIRM", clid, clid_len, NULL, xid, iface_name, NULL);
	
	for (opt = packet_options; opt; opt = opt6_next(opt, end))
	  {
	    int ia_type = opt6_type(opt);
	    void *ia_option, *ia_end;
	    
	    if (ia_type != OPTION6_IA_NA && ia_type != OPTION6_IA_TA)
	      continue;
	    
	    if (ia_type == OPTION6_IA_NA && opt6_len(opt) < 12)
	      continue;
	     
	    if (ia_type == OPTION6_IA_TA && opt6_len(opt) < 4)
	      continue;
	    
	    ia_option = opt6_ptr(opt, ia_type == OPTION6_IA_NA ? 12 : 4);
	    ia_end = opt6_ptr(opt, opt6_len(opt));
	    
	    for (ia_option = opt6_find(ia_option, ia_end, OPTION6_IAADDR, 24);
		 ia_option;
		 ia_option = opt6_find(opt6_next(ia_option, ia_end), ia_end, OPTION6_IAADDR, 24))
	      {
		struct in6_addr *req_addr = opt6_ptr(ia_option, 0);
		
		if (!address6_available(context, req_addr, run_tag_if(tags)))
		  {
		    o1 = new_opt6(OPTION6_STATUS_CODE);
		    put_opt6_short(DHCP6NOTONLINK);
		    put_opt6_string("Confirm failed");
		    end_opt6(o1);
		    return 1;
		  }

		log6_packet("DHCPREPLY", clid, clid_len, req_addr, xid, iface_name, hostname);
	      }
	  }	 

	o1 = new_opt6(OPTION6_STATUS_CODE);
	put_opt6_short(DHCP6SUCCESS );
	put_opt6_string("All addresses still on link");
	end_opt6(o1);
	return 1;
    }
      
    case DHCP6IREQ:
      {
	/* We can't discriminate contexts based on address, as we don't know it.
	   If there is only one possible context, we can use its tags */
	if (context && !context->current)
	  {
	    context->netid.next = NULL;
	    context_tags =  &context->netid;
	  }
	log6_packet("DHCPINFORMATION-REQUEST", clid, clid_len, NULL, xid, iface_name, ignore ? "ignored" : hostname);
	if (ignore)
	  return 0;
	*outmsgtypep = DHCP6REPLY;
	break;
      }
      
      
    case DHCP6RELEASE:
      {
	/* set reply message type */
	*outmsgtypep = DHCP6REPLY;

	log6_packet("DHCPRELEASE", clid, clid_len, NULL, xid, iface_name, NULL);

	for (opt = packet_options; opt; opt = opt6_next(opt, end))
	  {
	    int iaid, ia_type = opt6_type(opt);
	    void *ia_option, *ia_end;
	    int made_ia = 0;
	    	    
	    if (ia_type != OPTION6_IA_NA && ia_type != OPTION6_IA_TA)
	      continue;

	    if (ia_type == OPTION6_IA_NA && opt6_len(opt) < 12)
	      continue;
	    
	    if (ia_type == OPTION6_IA_TA && opt6_len(opt) < 4)
	      continue;
	    
	    iaid = opt6_uint(opt, 0, 4);
	    ia_end = opt6_ptr(opt, opt6_len(opt));
	    ia_option = opt6_ptr(opt, ia_type == OPTION6_IA_NA ? 12 : 4);
	    
	    /* reset "USED" flags on leases */
	    lease6_filter(ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, context);
		
	    for (ia_option = opt6_find(ia_option, ia_end, OPTION6_IAADDR, 24);
		 ia_option;
		 ia_option = opt6_find(opt6_next(ia_option, ia_end), ia_end, OPTION6_IAADDR, 24))
	      {
		struct dhcp_lease *lease;
		
		if ((lease = lease6_find(clid, clid_len, ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA,
					 iaid, opt6_ptr(ia_option, 0))))
		  lease_prune(lease, now);
		else
		  {
		    if (!made_ia)
		      {
			o = new_opt6(ia_type);
			put_opt6_long(iaid);
			if (ia_type == OPTION6_IA_NA)
			  {
			    put_opt6_long(0);
			    put_opt6_long(0); 
			  }
			made_ia = 1;
		      }
		    
		    o1 = new_opt6(OPTION6_IAADDR);
		    put_opt6(opt6_ptr(ia_option, 0), IN6ADDRSZ);
		    put_opt6_long(0);
		    put_opt6_long(0);
		    end_opt6(o1);
		  }
	      }
	    
	    if (made_ia)
	      {
		o1 = new_opt6(OPTION6_STATUS_CODE);
		put_opt6_short(DHCP6NOBINDING);
		put_opt6_string("No binding found");
		end_opt6(o1);
		
		end_opt6(o);
	      }
	  }
	
	o1 = new_opt6(OPTION6_STATUS_CODE);
	put_opt6_short(DHCP6SUCCESS);
	put_opt6_string("Release received");
	end_opt6(o1);
	
	return 1;
      }

    case DHCP6DECLINE:
      {
	/* set reply message type */
	*outmsgtypep = DHCP6REPLY;
	
	log6_packet("DHCPDECLINE", clid, clid_len, NULL, xid, iface_name, NULL);

	for (opt = packet_options; opt; opt = opt6_next(opt, end))
	  {
	    int iaid, ia_type = opt6_type(opt);
	    void *ia_option, *ia_end;
	    int made_ia = 0;
	    	    
	    if (ia_type != OPTION6_IA_NA && ia_type != OPTION6_IA_TA)
	      continue;

	    if (ia_type == OPTION6_IA_NA && opt6_len(opt) < 12)
	      continue;
	    
	    if (ia_type == OPTION6_IA_TA && opt6_len(opt) < 4)
	      continue;
	    
	    iaid = opt6_uint(opt, 0, 4);
	    ia_end = opt6_ptr(opt, opt6_len(opt));
	    ia_option = opt6_ptr(opt, ia_type == OPTION6_IA_NA ? 12 : 4);
	    
	    /* reset "USED" flags on leases */
	    lease6_filter(ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA, iaid, context);
		
	    for (ia_option = opt6_find(ia_option, ia_end, OPTION6_IAADDR, 24);
		 ia_option;
		 ia_option = opt6_find(opt6_next(ia_option, ia_end), ia_end, OPTION6_IAADDR, 24))
	      {
		struct dhcp_lease *lease;
		struct in6_addr *addrp = opt6_ptr(ia_option, 0);

		if (have_config(config, CONFIG_ADDR6) && 
		    memcmp(&config->addr6, addrp, IN6ADDRSZ) == 0)
		  {
		    prettyprint_time(daemon->dhcp_buff3, DECLINE_BACKOFF);
		    inet_ntop(AF_INET6, addrp, daemon->addrbuff, ADDRSTRLEN);
		    my_syslog(MS_DHCP | LOG_WARNING, _("disabling DHCP static address %s for %s"), 
			      daemon->addrbuff, daemon->dhcp_buff3);
		    config->flags |= CONFIG_DECLINED;
		    config->decline_time = now;
		  }
		else
		  /* make sure this host gets a different address next time. */
		  for (; context; context = context->current)
		    context->addr_epoch++;
		
		if ((lease = lease6_find(clid, clid_len, ia_type == OPTION6_IA_NA ? LEASE_NA : LEASE_TA,
					 iaid, opt6_ptr(ia_option, 0))))
		  lease_prune(lease, now);
		else
		  {
		    if (!made_ia)
		      {
			o = new_opt6(ia_type);
			put_opt6_long(iaid);
			if (ia_type == OPTION6_IA_NA)
			  {
			    put_opt6_long(0);
			    put_opt6_long(0); 
			  }
			made_ia = 1;
		      }
		    
		    o1 = new_opt6(OPTION6_IAADDR);
		    put_opt6(opt6_ptr(ia_option, 0), IN6ADDRSZ);
		    put_opt6_long(0);
		    put_opt6_long(0);
		    end_opt6(o1);
		  }
	      }
	    
	    if (made_ia)
	      {
		o1 = new_opt6(OPTION6_STATUS_CODE);
		put_opt6_short(DHCP6NOBINDING);
		put_opt6_string("No binding found");
		end_opt6(o1);
		
		end_opt6(o);
	      }
	    
	  }
	return 1;
      }

    }
  
  
  /* filter options based on tags, those we want get DHOPT_TAGOK bit set */
  tagif = option_filter(tags, context_tags, daemon->dhcp_opts6);
  
  oro = opt6_find(packet_options, end, OPTION6_ORO, 0);
  
  for (opt_cfg = daemon->dhcp_opts6; opt_cfg; opt_cfg = opt_cfg->next)
    {
      /* netids match and not encapsulated? */
      if (!(opt_cfg->flags & DHOPT_TAGOK))
	continue;
      
      if (!(opt_cfg->flags & DHOPT_FORCE) && oro)
	{
	  for (i = 0; i <  opt6_len(oro) - 1; i += 2)
	    if (opt6_uint(oro, i, 2) == (unsigned)opt_cfg->opt)
	      break;
	  
	  /* option not requested */
	  if (i >=  opt6_len(oro) - 1)
	    continue;
	}
      
      if (opt_cfg->opt == OPTION6_DNS_SERVER)
	{
	  done_dns = 1;
	  if (opt_cfg->len == 0)
	    continue;
	}
      
      o = new_opt6(opt_cfg->opt);
      if (opt_cfg->flags & DHOPT_ADDR6)
	{
	  int j;
	  struct in6_addr *a = (struct in6_addr *)opt_cfg->val;
          for (j = 0; j < opt_cfg->len; j+=IN6ADDRSZ, a++)
            {
              /* zero means "self" (but not in vendorclass options.) */
              if (IN6_IS_ADDR_UNSPECIFIED(a))
                {
		  if (IN6_IS_ADDR_UNSPECIFIED(&context->local6))
		    put_opt6(fallback, IN6ADDRSZ);
		  else
		    put_opt6(&context->local6, IN6ADDRSZ);
		}
              else
                put_opt6(a, IN6ADDRSZ);
            }
	}
      else if (opt_cfg->val)
	put_opt6(opt_cfg->val, opt_cfg->len);
      end_opt6(o);
    }
  
  if (!done_dns && 
      (!IN6_IS_ADDR_UNSPECIFIED(&context->local6) ||
       !IN6_IS_ADDR_UNSPECIFIED(fallback)))
    {
      o = new_opt6(OPTION6_DNS_SERVER);
      if (IN6_IS_ADDR_UNSPECIFIED(&context->local6))
	put_opt6(fallback, IN6ADDRSZ);
      else
	put_opt6(&context->local6, IN6ADDRSZ);
      end_opt6(o); 
    }
   
    /* handle vendor-identifying vendor-encapsulated options,
       dhcp-option = vi-encap:13,17,....... */
  for (opt_cfg = daemon->dhcp_opts6; opt_cfg; opt_cfg = opt_cfg->next)
    opt_cfg->flags &= ~DHOPT_ENCAP_DONE;
    
  if (oro)
    for (i = 0; i <  opt6_len(oro) - 1; i += 2)
      if (opt6_uint(oro, i, 2) == OPTION6_VENDOR_OPTS)
	do_encap = 1;
  
  for (opt_cfg = daemon->dhcp_opts6; opt_cfg; opt_cfg = opt_cfg->next)
    { 
      if (opt_cfg->flags & DHOPT_RFC3925)
	{
	  int found = 0;
	  struct dhcp_opt *oc;
	  
	  if (opt_cfg->flags & DHOPT_ENCAP_DONE)
	    continue;
	  
	  for (oc = daemon->dhcp_opts6; oc; oc = oc->next)
	    {
	      oc->flags &= ~DHOPT_ENCAP_MATCH;
	      
	      if (!(oc->flags & DHOPT_RFC3925) || opt_cfg->u.encap != oc->u.encap)
		continue;
	      
	      oc->flags |= DHOPT_ENCAP_DONE;
	      if (match_netid(oc->netid, tagif, 1))
		{
		  /* option requested/forced? */
		  if (!oro || do_encap || (oc->flags & DHOPT_FORCE))
		    {
		      oc->flags |= DHOPT_ENCAP_MATCH;
		      found = 1;
		    }
		} 
	    }
	  
	  if (found)
	    { 
	      o = new_opt6(OPTION6_VENDOR_OPTS);	      
	      put_opt6_long(opt_cfg->u.encap);	
	     
	      for (oc = daemon->dhcp_opts6; oc; oc = oc->next)
		if (oc->flags & DHOPT_ENCAP_MATCH)
		  {
		    o1 = new_opt6(oc->opt);
		    put_opt6(oc->val, oc->len);
		    end_opt6(o1);
		  }
	      end_opt6(o);
	    }
	}
    }      


  if (hostname)
    {
      unsigned char *p;
      size_t len = strlen(hostname);
      
      if (send_domain)
	len += strlen(send_domain) + 1;

      o = new_opt6(OPTION6_FQDN);
      if ((p = expand(len + 3)))
	{
	  *(p++) = fqdn_flags;
	  p = do_rfc1035_name(p, hostname);
	  if (send_domain)
	    p = do_rfc1035_name(p, send_domain);
	  *p = 0;
	}
      end_opt6(o);
    }


  /* logging */
  if (option_bool(OPT_LOG_OPTS) && oro)
    {
      char *q = daemon->namebuff;
      for (i = 0; i <  opt6_len(oro) - 1; i += 2)
	{
	  char *s = option_string(AF_INET6, opt6_uint(oro, i, 2), NULL, 0, NULL, 0);
	  q += snprintf(q, MAXDNAME - (q - daemon->namebuff),
			"%d%s%s%s", 
			opt6_uint(oro, i, 2),
			strlen(s) != 0 ? ":" : "",
			s, 
			(i > opt6_len(oro) - 3) ? "" : ", ");
	  if ( i >  opt6_len(oro) - 3 || (q - daemon->namebuff) > 40)
	    {
	      q = daemon->namebuff;
	      my_syslog(MS_DHCP | LOG_INFO, _("%u requested options: %s"), xid, daemon->namebuff);
	    }
	}
    }
 
  log_tags(tagif, xid);

  if (option_bool(OPT_LOG_OPTS))
    log6_opts(0, xid, daemon->outpacket.iov_base + start_opts, daemon->outpacket.iov_base + save_counter(-1));
  
  return 1;
}

static void log6_opts(int nest, unsigned int xid, void *start_opts, void *end_opts)
{
  void *opt;
  char *desc = nest ? "nest" : "sent";
  
  if (start_opts == end_opts)
    return;
  
  for (opt = start_opts; opt; opt = opt6_next(opt, end_opts))
    {
      int type = opt6_type(opt);
      void *ia_options = NULL;
      char *optname;
      
      if (type == OPTION6_IA_NA)
	{
	  sprintf(daemon->namebuff, "IAID=%u T1=%u T2=%u",
		  opt6_uint(opt, 0, 4), opt6_uint(opt, 4, 4), opt6_uint(opt, 8, 4));
	  optname = "ia-na";
	  ia_options = opt6_ptr(opt, 12);
	}
      else if (type == OPTION6_IA_TA)
	{
	  sprintf(daemon->namebuff, "IAID=%u", opt6_uint(opt, 0, 4));
	  optname = "ia-ta";
	  ia_options = opt6_ptr(opt, 4);
	}
      else if (type == OPTION6_IAADDR)
	{
	  inet_ntop(AF_INET6, opt6_ptr(opt, 0), daemon->addrbuff, ADDRSTRLEN);
	  sprintf(daemon->namebuff, "%s PL=%u VL=%u", 
		  daemon->addrbuff, opt6_uint(opt, 16, 4), opt6_uint(opt, 20, 4));
	  optname = "iaaddr";
	  ia_options = opt6_ptr(opt, 24);
	}
      else if (type == OPTION6_STATUS_CODE)
	{
	  int len = sprintf(daemon->namebuff, "%u ", opt6_uint(opt, 0, 2));
	  memcpy(daemon->namebuff + len, opt6_ptr(opt, 2), opt6_len(opt)-2);
	  daemon->namebuff[len + opt6_len(opt) - 2] = 0;
	  optname = "status";
	}
      else
	{
	  /* account for flag byte on FQDN */
	  int offset = type == OPTION6_FQDN ? 1 : 0;
	  optname = option_string(AF_INET6, type, opt6_ptr(opt, offset), opt6_len(opt) - offset, daemon->namebuff, MAXDNAME);
	}
      
      my_syslog(MS_DHCP | LOG_INFO, "%u %s size:%3d option:%3d %s  %s", 
		xid, desc, opt6_len(opt), type, optname, daemon->namebuff);
      
      if (ia_options)
	log6_opts(1, xid, ia_options, opt6_ptr(opt, opt6_len(opt)));
    }
}		 
 
static void log6_packet(char *type, unsigned char *clid, int clid_len, struct in6_addr *addr, int xid, char *iface, char *string)
{
  /* avoid buffer overflow */
  if (clid_len > 100)
    clid_len = 100;
  
  print_mac(daemon->namebuff, clid, clid_len);

  if (addr)
    {
      inet_ntop(AF_INET6, addr, daemon->dhcp_buff2, 255);
      strcat(daemon->dhcp_buff2, " ");
    }
  else
    daemon->dhcp_buff2[0] = 0;

  if(option_bool(OPT_LOG_OPTS))
    my_syslog(MS_DHCP | LOG_INFO, "%u %s(%s) %s%s %s",
	      xid, 
	      type,
	      iface, 
	      daemon->dhcp_buff2,
	      daemon->namebuff,
	      string ? string : "");
  else
    my_syslog(MS_DHCP | LOG_INFO, "%s(%s) %s%s %s",
	      type,
	      iface, 
	      daemon->dhcp_buff2,
	      daemon->namebuff,
	      string ? string : "");
}

static void *opt6_find (void *opts, void *end, unsigned int search, unsigned int minsize)
{
  u16 opt, opt_len;
  void *start;
  
  if (!opts)
    return NULL;
    
  while (1)
    {
      if (end - opts < 4) 
	return NULL;
      
      start = opts;
      GETSHORT(opt, opts);
      GETSHORT(opt_len, opts);
      
      if (opt_len > (end - opts))
	return NULL;
      
      if (opt == search && (opt_len >= minsize))
	return start;
      
      opts += opt_len;
    }
}

static void *opt6_next(void *opts, void *end)
{
  u16 opt_len;
  
  if (end - opts < 4) 
    return NULL;
  
  opts += 2;
  GETSHORT(opt_len, opts);
  
  if (opt_len >= (end - opts))
    return NULL;
  
  return opts + opt_len;
}

static unsigned int opt6_uint(unsigned char *opt, int offset, int size)
{
  /* this worries about unaligned data and byte order */
  unsigned int ret = 0;
  int i;
  unsigned char *p = opt6_ptr(opt, offset);
  
  for (i = 0; i < size; i++)
    ret = (ret << 8) | *p++;
  
  return ret;
} 

#endif
