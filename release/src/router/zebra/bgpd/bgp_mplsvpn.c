/* MPLS-VPN
   Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "command.h"
#include "prefix.h"
#include "log.h"
#include "memory.h"
#include "stream.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_mplsvpn.h"

int route_vty_out (struct vty *, struct prefix *, struct bgp_info *, int, safi_t);
int route_vty_out_tag (struct vty *, struct prefix *, struct bgp_info *, int, safi_t);
void route_vty_out_tmp (struct vty *, struct prefix *, struct attr *, safi_t);

u_int16_t
decode_rd_type (u_char *pnt)
{
  u_int16_t v;
  
  v = ((u_int16_t) *pnt++ << 8);
  v |= (u_int16_t) *pnt;
  return v;
}

u_int32_t
decode_label (u_char *pnt)
{
  u_int32_t l;

  l = ((u_int32_t) *pnt++ << 12);
  l |= (u_int32_t) *pnt++ << 4;
  l |= (u_int32_t) ((*pnt & 0xf0) >> 4);
  return l;
}

void
decode_rd_as (u_char *pnt, struct rd_as *rd_as)
{
  rd_as->as = (u_int16_t) *pnt++ << 8;
  rd_as->as |= (u_int16_t) *pnt++;
  
  rd_as->val = ((u_int32_t) *pnt++ << 24);
  rd_as->val |= ((u_int32_t) *pnt++ << 16);
  rd_as->val |= ((u_int32_t) *pnt++ << 8);
  rd_as->val |= (u_int32_t) *pnt;
}

void
decode_rd_ip (u_char *pnt, struct rd_ip *rd_ip)
{
  memcpy (&rd_ip->ip, pnt, 4);
  pnt += 4;
  
  rd_ip->val = ((u_int16_t) *pnt++ << 8);
  rd_ip->val |= (u_int16_t) *pnt;
}

int bgp_update (struct peer *, struct prefix *, struct attr *, 
		afi_t, safi_t, int, int, struct prefix_rd *, u_char *);

int bgp_withdraw (struct peer *, struct prefix *, struct attr *, 
		  int, int, int, int, struct prefix_rd *, u_char *);
int
bgp_nlri_parse_vpnv4 (struct peer *peer, struct attr *attr, 
		      struct bgp_nlri *packet)
{
  u_char *pnt;
  u_char *lim;
  struct prefix p;
  int psize;
  int prefixlen;
  u_int32_t label;
  u_int16_t type;
  struct rd_as rd_as;
  struct rd_ip rd_ip;
  struct prefix_rd prd;
  u_char *tagpnt;

  /* Check peer status. */
  if (peer->status != Established)
    return 0;
  
  /* Make prefix_rd */
  prd.family = AF_UNSPEC;
  prd.prefixlen = 64;

  pnt = packet->nlri;
  lim = pnt + packet->length;

  for (; pnt < lim; pnt += psize)
    {
      /* Clear prefix structure. */
      memset (&p, 0, sizeof (struct prefix));

      /* Fetch prefix length. */
      prefixlen = *pnt++;
      p.family = AF_INET;
      psize = PSIZE (prefixlen);

      if (prefixlen < 88)
	{
	  zlog_err ("prefix length is less than 88: %d", prefixlen);
	  return -1;
	}

      label = decode_label (pnt);

      /* Copyr label to prefix. */
      tagpnt = pnt;;

      /* Copy routing distinguisher to rd. */
      memcpy (&prd.val, pnt + 3, 8);

      /* Decode RD type. */
      type = decode_rd_type (pnt + 3);

      /* Decode RD value. */
      if (type == RD_TYPE_AS)
	decode_rd_as (pnt + 5, &rd_as);
      else if (type == RD_TYPE_IP)
	decode_rd_ip (pnt + 5, &rd_ip);
      else
	{
	  zlog_err ("Invalid RD type %d", type);
	  return -1;
	}

      p.prefixlen = prefixlen - 88;
      memcpy (&p.u.prefix, pnt + 11, psize - 11);

#if 0
      if (type == RD_TYPE_AS)
	zlog_info ("prefix %ld:%ld:%ld:%s/%d", label, rd_as.as, rd_as.val,
		   inet_ntoa (p.u.prefix4), p.prefixlen);
      else if (type == RD_TYPE_IP)
	zlog_info ("prefix %ld:%s:%ld:%s/%d", label, inet_ntoa (rd_ip.ip),
		   rd_ip.val, inet_ntoa (p.u.prefix4), p.prefixlen);
#endif /* 0 */

      if (pnt + psize > lim)
	return -1;

      if (attr)
	bgp_update (peer, &p, attr, AFI_IP, SAFI_MPLS_VPN,
		    ZEBRA_ROUTE_BGP, BGP_ROUTE_NORMAL, &prd, tagpnt);
      else
	bgp_withdraw (peer, &p, attr, AFI_IP, SAFI_MPLS_VPN,
		      ZEBRA_ROUTE_BGP, BGP_ROUTE_NORMAL, &prd, tagpnt);
    }

  /* Packet length consistency check. */
  if (pnt != lim)
    return -1;

  return 0;
}

int
str2prefix_rd (u_char *str, struct prefix_rd *prd)
{
  int ret;
  u_char *p;
  u_char *p2;
  struct stream *s;
  u_char *half;
  struct in_addr addr;

  s = stream_new (8);

  prd->family = AF_UNSPEC;
  prd->prefixlen = 64;

  p = strchr (str, ':');
  if (! p)
    return 0;

  if (! all_digit (p + 1))
    return 0;

  half = XMALLOC (MTYPE_TMP, (p - str) + 1);
  memcpy (half, str, (p - str));
  half[p - str] = '\0';

  p2 = strchr (str, '.');

  if (! p2)
    {
      if (! all_digit (half))
	{
	  XFREE (MTYPE_TMP, half);
	  return 0;
	}
      stream_putw (s, RD_TYPE_AS);
      stream_putw (s, atoi (half));
      stream_putl (s, atol (p + 1));
    }
  else
    {
      ret = inet_aton (half, &addr);
      if (! ret)
	{
	  XFREE (MTYPE_TMP, half);
	  return 0;
	}
      stream_putw (s, RD_TYPE_IP);
      stream_put_in_addr (s, &addr);
      stream_putw (s, atol (p + 1));
    }
  memcpy (prd->val, s->data, 8);

  return 1;
}

int
str2tag (u_char *str, u_char *tag)
{
  u_int32_t l;

  l = atol (str);

  tag[0] = (u_char)(l >> 12);
  tag[1] = (u_char)(l >> 4);
  tag[2] = (u_char)(l << 4);

  return 1;
}

char *
prefix_rd2str (struct prefix_rd *prd, char *buf, size_t size)
{
  u_char *pnt;
  u_int16_t type;
  struct rd_as rd_as;
  struct rd_ip rd_ip;

  if (size < RD_ADDRSTRLEN)
    return NULL;

  pnt = prd->val;

  type = decode_rd_type (pnt);

  if (type == RD_TYPE_AS)
    {
      decode_rd_as (pnt + 2, &rd_as);
      snprintf (buf, size, "%d:%d", rd_as.as, rd_as.val);
      return buf;
    }
  else if (type == RD_TYPE_IP)
    {
      decode_rd_ip (pnt + 2, &rd_ip);
      snprintf (buf, size, "%s:%d", inet_ntoa (rd_ip.ip), rd_ip.val);
      return buf;
    }

  return NULL;
}

/* For testing purpose, static route of MPLS-VPN. */
DEFUN (vpnv4_network,
       vpnv4_network_cmd,
       "network A.B.C.D/M rd ASN:nn_or_IP-address:nn tag WORD",
       "Specify a network to announce via BGP\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Specify Route Distinguisher\n"
       "VPN Route Distinguisher\n"
       "BGP tag\n"
       "tag value\n")
{
  return bgp_static_set_vpnv4 (vty, argv[0], argv[1], argv[2]);
}

/* For testing purpose, static route of MPLS-VPN. */
DEFUN (no_vpnv4_network,
       no_vpnv4_network_cmd,
       "no network A.B.C.D/M rd ASN:nn_or_IP-address:nn tag WORD",
       NO_STR
       "Specify a network to announce via BGP\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Specify Route Distinguisher\n"
       "VPN Route Distinguisher\n"
       "BGP tag\n"
       "tag value\n")
{
  return bgp_static_unset_vpnv4 (vty, argv[0], argv[1], argv[2]);
}

int
show_adj_route_vpn (struct vty *vty, struct peer *peer, struct prefix_rd *prd)
{
  struct bgp *bgp;
  struct bgp_table *table;
  struct bgp_node *rn;
  struct bgp_node *rm;
  struct attr *attr;
  int rd_header;
  int header = 1;
  char v4_header[] = "   Network          Next Hop            Metric LocPrf Weight Path%s";

  bgp = bgp_get_default ();
  if (bgp == NULL)
    {
      vty_out (vty, "No BGP process is configured%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  for (rn = bgp_table_top (bgp->rib[AFI_IP][SAFI_MPLS_VPN]); rn;
       rn = bgp_route_next (rn))
    {
      if (prd && memcmp (rn->p.u.val, prd->val, 8) != 0)
        continue;

      if ((table = rn->info) != NULL)
        {
          rd_header = 1;

          for (rm = bgp_table_top (table); rm; rm = bgp_route_next (rm))
            if ((attr = rm->info) != NULL)
              {
                if (header)
                  {
                    vty_out (vty, "BGP table version is 0, local router ID is %s%s",
                             inet_ntoa (bgp->router_id), VTY_NEWLINE);
                    vty_out (vty, "Status codes: s suppressed, d damped, h history, * valid, > best, i - internal%s",
                             VTY_NEWLINE);
                    vty_out (vty, "Origin codes: i - IGP, e - EGP, ? - incomplete%s%s",
                             VTY_NEWLINE, VTY_NEWLINE);
                    vty_out (vty, v4_header, VTY_NEWLINE);
                    header = 0;
                  }

                if (rd_header)
                  {
                    u_int16_t type;
                    struct rd_as rd_as;
                    struct rd_ip rd_ip;
                    u_char *pnt;

                    pnt = rn->p.u.val;

                    /* Decode RD type. */
                    type = decode_rd_type (pnt);
                    /* Decode RD value. */
                    if (type == RD_TYPE_AS)
                      decode_rd_as (pnt + 2, &rd_as);
                    else if (type == RD_TYPE_IP)
                      decode_rd_ip (pnt + 2, &rd_ip);

                    vty_out (vty, "Route Distinguisher: ");

                    if (type == RD_TYPE_AS)
                      vty_out (vty, "%d:%d", rd_as.as, rd_as.val);
                    else if (type == RD_TYPE_IP)
                      vty_out (vty, "%s:%d", inet_ntoa (rd_ip.ip), rd_ip.val);

                    vty_out (vty, "%s", VTY_NEWLINE);
                    rd_header = 0;
                  }
                route_vty_out_tmp (vty, &rm->p, attr, SAFI_MPLS_VPN);
              }
        }
    }
  return CMD_SUCCESS;
}

enum bgp_show_type
{
  bgp_show_type_normal,
  bgp_show_type_regexp,
  bgp_show_type_prefix_list,
  bgp_show_type_filter_list,
  bgp_show_type_neighbor,
  bgp_show_type_cidr_only,
  bgp_show_type_prefix_longer,
  bgp_show_type_community_all,
  bgp_show_type_community,
  bgp_show_type_community_exact,
  bgp_show_type_community_list,
  bgp_show_type_community_list_exact
};

int
bgp_show_mpls_vpn (struct vty *vty, struct prefix_rd *prd, enum bgp_show_type type,
		   void *output_arg, int tags)
{
  struct bgp *bgp;
  struct bgp_table *table;
  struct bgp_node *rn;
  struct bgp_node *rm;
  struct bgp_info *ri;
  int rd_header;
  int header = 1;
  char v4_header[] = "   Network          Next Hop            Metric LocPrf Weight Path%s";
  char v4_header_tag[] = "   Network          Next Hop      In tag/Out tag%s";

  bgp = bgp_get_default ();
  if (bgp == NULL)
    {
      vty_out (vty, "No BGP process is configured%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  for (rn = bgp_table_top (bgp->rib[AFI_IP][SAFI_MPLS_VPN]); rn; rn = bgp_route_next (rn))
    {
      if (prd && memcmp (rn->p.u.val, prd->val, 8) != 0)
	continue;

      if ((table = rn->info) != NULL)
	{
	  rd_header = 1;

	  for (rm = bgp_table_top (table); rm; rm = bgp_route_next (rm))
	    for (ri = rm->info; ri; ri = ri->next)
	      {
		if (type == bgp_show_type_neighbor)
		  {
		    union sockunion *su = output_arg;

		    if (ri->peer->su_remote == NULL || ! sockunion_same(ri->peer->su_remote, su))
		      continue;
		  }
		if (header)
		  {
		    if (tags)
		      vty_out (vty, v4_header_tag, VTY_NEWLINE);
		    else
		      {
			vty_out (vty, "BGP table version is 0, local router ID is %s%s",
				 inet_ntoa (bgp->router_id), VTY_NEWLINE);
			vty_out (vty, "Status codes: s suppressed, d damped, h history, * valid, > best, i - internal%s",
				 VTY_NEWLINE);
			vty_out (vty, "Origin codes: i - IGP, e - EGP, ? - incomplete%s%s",
				 VTY_NEWLINE, VTY_NEWLINE);
			vty_out (vty, v4_header, VTY_NEWLINE);
		      }
		    header = 0;
		  }

		if (rd_header)
		  {
		    u_int16_t type;
		    struct rd_as rd_as;
		    struct rd_ip rd_ip;
		    u_char *pnt;

		    pnt = rn->p.u.val;

		    /* Decode RD type. */
		    type = decode_rd_type (pnt);
		    /* Decode RD value. */
		    if (type == RD_TYPE_AS)
		      decode_rd_as (pnt + 2, &rd_as);
		    else if (type == RD_TYPE_IP)
		      decode_rd_ip (pnt + 2, &rd_ip);

		    vty_out (vty, "Route Distinguisher: ");

		    if (type == RD_TYPE_AS)
		      vty_out (vty, "%d:%d", rd_as.as, rd_as.val);
		    else if (type == RD_TYPE_IP)
		      vty_out (vty, "%s:%d", inet_ntoa (rd_ip.ip), rd_ip.val);
		  
		    vty_out (vty, "%s", VTY_NEWLINE);		  
		    rd_header = 0;
		  }
	        if (tags)
		  route_vty_out_tag (vty, &rm->p, ri, 0, SAFI_MPLS_VPN);
	        else
		  route_vty_out (vty, &rm->p, ri, 0, SAFI_MPLS_VPN);
	      }
        }
    }
  return CMD_SUCCESS;
}

DEFUN (show_ip_bgp_vpnv4_all,
       show_ip_bgp_vpnv4_all_cmd,
       "show ip bgp vpnv4 all",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n")
{
  return bgp_show_mpls_vpn (vty, NULL, bgp_show_type_normal, NULL, 0);
}

DEFUN (show_ip_bgp_vpnv4_rd,
       show_ip_bgp_vpnv4_rd_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n")
{
  int ret;
  struct prefix_rd prd;

  ret = str2prefix_rd (argv[0], &prd);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Route Distinguisher%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return bgp_show_mpls_vpn (vty, &prd, bgp_show_type_normal, NULL, 0);
}

DEFUN (show_ip_bgp_vpnv4_all_tags,
       show_ip_bgp_vpnv4_all_tags_cmd,
       "show ip bgp vpnv4 all tags",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Display BGP tags for prefixes\n")
{
  return bgp_show_mpls_vpn (vty, NULL, bgp_show_type_normal, NULL,  1);
}

DEFUN (show_ip_bgp_vpnv4_rd_tags,
       show_ip_bgp_vpnv4_rd_tags_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn tags",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n"
       "Display BGP tags for prefixes\n")
{
  int ret;
  struct prefix_rd prd;

  ret = str2prefix_rd (argv[0], &prd);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Route Distinguisher%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return bgp_show_mpls_vpn (vty, &prd, bgp_show_type_normal, NULL, 1);
}

DEFUN (show_ip_bgp_vpnv4_all_neighbor_routes,
       show_ip_bgp_vpnv4_all_neighbor_routes_cmd,
       "show ip bgp vpnv4 all neighbors A.B.C.D routes",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Display routes learned from neighbor\n")
{
  union sockunion *su;
  struct peer *peer;
  
  su = sockunion_str2su (argv[0]);
  if (su == NULL)
    {
      vty_out (vty, "Malformed address: %s%s", argv[0], VTY_NEWLINE);
               return CMD_WARNING;
    }

  peer = peer_lookup (NULL, su);
  if (! peer || ! peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      vty_out (vty, "%% No such neighbor or address family%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_show_mpls_vpn (vty, NULL, bgp_show_type_neighbor, su, 0);
}

DEFUN (show_ip_bgp_vpnv4_rd_neighbor_routes,
       show_ip_bgp_vpnv4_rd_neighbor_routes_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn neighbors A.B.C.D routes",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Display routes learned from neighbor\n")
{
  int ret;
  union sockunion *su;
  struct peer *peer;
  struct prefix_rd prd;

  ret = str2prefix_rd (argv[0], &prd);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Route Distinguisher%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  su = sockunion_str2su (argv[1]);
  if (su == NULL)
    {
      vty_out (vty, "Malformed address: %s%s", argv[0], VTY_NEWLINE);
               return CMD_WARNING;
    }

  peer = peer_lookup (NULL, su);
  if (! peer || ! peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      vty_out (vty, "%% No such neighbor or address family%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_show_mpls_vpn (vty, &prd, bgp_show_type_neighbor, su, 0);
}

DEFUN (show_ip_bgp_vpnv4_all_neighbor_advertised_routes,
       show_ip_bgp_vpnv4_all_neighbor_advertised_routes_cmd,
       "show ip bgp vpnv4 all neighbors A.B.C.D advertised-routes",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Display the routes advertised to a BGP neighbor\n")
{
  int ret;
  struct peer *peer;
  union sockunion su;

  ret = str2sockunion (argv[0], &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed address: %s%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }
  peer = peer_lookup (NULL, &su);
  if (! peer || ! peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      vty_out (vty, "%% No such neighbor or address family%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return show_adj_route_vpn (vty, peer, NULL);
}

DEFUN (show_ip_bgp_vpnv4_rd_neighbor_advertised_routes,
       show_ip_bgp_vpnv4_rd_neighbor_advertised_routes_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn neighbors A.B.C.D advertised-routes",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Display the routes advertised to a BGP neighbor\n")
{
  int ret;
  struct peer *peer;
  struct prefix_rd prd;
  union sockunion su;

  ret = str2sockunion (argv[1], &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed address: %s%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }
  peer = peer_lookup (NULL, &su);
  if (! peer || ! peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      vty_out (vty, "%% No such neighbor or address family%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = str2prefix_rd (argv[0], &prd);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Route Distinguisher%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return show_adj_route_vpn (vty, peer, &prd);
}

void
bgp_mplsvpn_init ()
{
  install_element (BGP_VPNV4_NODE, &vpnv4_network_cmd);
  install_element (BGP_VPNV4_NODE, &no_vpnv4_network_cmd);


  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_tags_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_tags_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_neighbor_routes_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_neighbor_routes_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_neighbor_advertised_routes_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_neighbor_advertised_routes_cmd);

  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_tags_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_tags_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_neighbor_routes_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_neighbor_routes_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_neighbor_advertised_routes_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_neighbor_advertised_routes_cmd);
}
