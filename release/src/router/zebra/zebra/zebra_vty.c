/* Zebra VTY functions
 * Copyright (C) 2002 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "command.h"
#include "table.h"
#include "rib.h"

/* Return route type string for VTY output.  */
const char *
route_type_str (u_char type)
{
  switch (type)
    {
    case ZEBRA_ROUTE_SYSTEM:
      return "system";
    case ZEBRA_ROUTE_KERNEL:
      return "kernel";
    case ZEBRA_ROUTE_CONNECT:
      return "connected";
    case ZEBRA_ROUTE_STATIC:
      return "static";
    case ZEBRA_ROUTE_RIP:
      return "rip";
    case ZEBRA_ROUTE_RIPNG:
      return "rip";
    case ZEBRA_ROUTE_OSPF:
      return "ospf";
    case ZEBRA_ROUTE_OSPF6:
      return "ospf";
    case ZEBRA_ROUTE_BGP:
      return "bgp";
    default:
      return "unknown";
    }
};

/* Return route type string for VTY output.  */
const char
route_type_char (u_char type)
{
  switch (type)
    {
    case ZEBRA_ROUTE_SYSTEM:
      return 'S';
    case ZEBRA_ROUTE_KERNEL:
      return 'K';
    case ZEBRA_ROUTE_CONNECT:
      return 'C';
    case ZEBRA_ROUTE_STATIC:
      return 'S';
    case ZEBRA_ROUTE_RIP:
      return 'R';
    case ZEBRA_ROUTE_RIPNG:
      return 'R';
    case ZEBRA_ROUTE_OSPF:
      return 'O';
    case ZEBRA_ROUTE_OSPF6:
      return 'O';
    case ZEBRA_ROUTE_BGP:
      return 'B';
    default:
      return '?';
    }
};

/* General fucntion for static route. */
int
zebra_static_ipv4 (struct vty *vty, int add_cmd,
		   char *dest_str, char *mask_str, char *gate_str,
		   char *distance_str)
{
  int ret;
  u_char distance;
  struct prefix p;
  struct in_addr gate;
  struct in_addr mask;
  char *ifname;
  
  ret = str2prefix (dest_str, &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Cisco like mask notation. */
  if (mask_str)
    {
      ret = inet_aton (mask_str, &mask);
      if (ret == 0)
	{
	  vty_out (vty, "%% Malformed address%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
      p.prefixlen = ip_masklen (mask);
    }

  /* Apply mask for given prefix. */
  apply_mask (&p);

  /* Administrative distance. */
  if (distance_str)
    distance = atoi (distance_str);
  else
    distance = ZEBRA_STATIC_DISTANCE_DEFAULT;

  /* Null0 static route.  */
  if (strncasecmp (gate_str, "Null0", strlen (gate_str)) == 0)
    {
      if (add_cmd)
	static_add_ipv4 (&p, NULL, NULL, distance, 0);
      else
	static_delete_ipv4 (&p, NULL, NULL, distance, 0);
      return CMD_SUCCESS;
    }

  /* When gateway is A.B.C.D format, gate is treated as nexthop
     address other case gate is treated as interface name. */
  ret = inet_aton (gate_str, &gate);
  if (ret)
    ifname = NULL;
  else
    ifname = gate_str;

  if (add_cmd)
    static_add_ipv4 (&p, ifname ? NULL : &gate, ifname, distance, 0);
  else
    static_delete_ipv4 (&p, ifname ? NULL : &gate, ifname, distance, 0);

  return CMD_SUCCESS;
}

/* Static route configuration.  */
DEFUN (ip_route, 
       ip_route_cmd,
       "ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0)",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n")
{
  return zebra_static_ipv4 (vty, 1, argv[0], NULL, argv[1], NULL);
}

/* Mask as A.B.C.D format.  */
DEFUN (ip_route_mask,
       ip_route_mask_cmd,
       "ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0)",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n")
{
  return zebra_static_ipv4 (vty, 1, argv[0], argv[1], argv[2], NULL);
}

/* Distance option value.  */
DEFUN (ip_route_distance,
       ip_route_distance_cmd,
       "ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0) <1-255>",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  return zebra_static_ipv4 (vty, 1, argv[0], NULL, argv[1], argv[2]);
}

DEFUN (ip_route_mask_distance,
       ip_route_mask_distance_cmd,
       "ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0) <1-255>",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  return zebra_static_ipv4 (vty, 1, argv[0], argv[1], argv[2], argv[3]);
}

DEFUN (no_ip_route, 
       no_ip_route_cmd,
       "no ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0)",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n")
{
  return zebra_static_ipv4 (vty, 0, argv[0], NULL, argv[1], NULL);
}

DEFUN (no_ip_route_mask,
       no_ip_route_mask_cmd,
       "no ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0)",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n")
{
  return zebra_static_ipv4 (vty, 0, argv[0], argv[1], argv[2], NULL);
}

DEFUN (no_ip_route_distance,
       no_ip_route_distance_cmd,
       "no ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0) <1-255>",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  return zebra_static_ipv4 (vty, 0, argv[0], NULL, argv[1], argv[2]);
}

DEFUN (no_ip_route_mask_distance,
       no_ip_route_mask_distance_cmd,
       "no ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0) <1-255>",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  return zebra_static_ipv4 (vty, 0, argv[0], argv[1], argv[2], argv[3]);
}

/* New RIB.  Detailed information for IPv4 route. */
void
vty_show_ip_route_detail (struct vty *vty, struct route_node *rn)
{
  struct rib *rib;
  struct nexthop *nexthop;

  for (rib = rn->info; rib; rib = rib->next)
    {
      vty_out (vty, "Routing entry for %s/%d%s", 
	       inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen,
	       VTY_NEWLINE);
      vty_out (vty, "  Known via \"%s\"", route_type_str (rib->type));
      vty_out (vty, ", distance %d, metric %d", rib->distance, rib->metric);
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	vty_out (vty, ", best");
      if (rib->refcnt)
	vty_out (vty, ", refcnt %ld", rib->refcnt);
      vty_out (vty, "%s", VTY_NEWLINE);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7
      if (rib->type == ZEBRA_ROUTE_RIP
	  || rib->type == ZEBRA_ROUTE_OSPF
	  || rib->type == ZEBRA_ROUTE_BGP)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

	  vty_out (vty, "  Last update ");

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  "%02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, "%dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, "%02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	  vty_out (vty, " ago%s", VTY_NEWLINE);
	}

      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	{
	  vty_out (vty, "  %c",
		   CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB) ? '*' : ' ');

	  switch (nexthop->type)
	    {
	    case NEXTHOP_TYPE_IPV4:
	    case NEXTHOP_TYPE_IPV4_IFINDEX:
	      vty_out (vty, " %s", inet_ntoa (nexthop->gate.ipv4));
	      if (nexthop->ifindex)
		vty_out (vty, ", via %s", ifindex2ifname (nexthop->ifindex));
	      break;
	    case NEXTHOP_TYPE_IFINDEX:
	      vty_out (vty, " directly connected, %s",
		       ifindex2ifname (nexthop->ifindex));
	      break;
	    case NEXTHOP_TYPE_IFNAME:
	      vty_out (vty, " directly connected, %s", nexthop->ifname);
	      break;
	    case NEXTHOP_TYPE_BLACKHOLE:
	      vty_out (vty, " directly connected, via Null0");
	      break;
	    default:
	      break;
	    }
	  if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	    vty_out (vty, " inactive");

	  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	    {
	      vty_out (vty, " (recursive");
		
	      switch (nexthop->rtype)
		{
		case NEXTHOP_TYPE_IPV4:
		case NEXTHOP_TYPE_IPV4_IFINDEX:
		  vty_out (vty, " via %s)", inet_ntoa (nexthop->rgate.ipv4));
		  break;
		case NEXTHOP_TYPE_IFINDEX:
		case NEXTHOP_TYPE_IFNAME:
		  vty_out (vty, " is directly connected, %s)",
			   ifindex2ifname (nexthop->rifindex));
		  break;
		default:
		  break;
		}
	    }
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

void
vty_show_ip_route (struct vty *vty, struct route_node *rn, struct rib *rib)
{
  struct nexthop *nexthop;
  int len = 0;
  char buf[BUFSIZ];

  /* Nexthop information. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (nexthop == rib->nexthop)
	{
	  /* Prefix information. */
	  len = vty_out (vty, "%c%c%c %s/%d",
			 route_type_char (rib->type),
			 CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED)
			 ? '>' : ' ',
			 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
			 ? '*' : ' ',
			 inet_ntop (AF_INET, &rn->p.u.prefix, buf, BUFSIZ),
			 rn->p.prefixlen);
		
	  /* Distance and metric display. */
	  if (rib->type != ZEBRA_ROUTE_CONNECT 
	      && rib->type != ZEBRA_ROUTE_KERNEL)
	    len += vty_out (vty, " [%d/%d]", rib->distance,
			    rib->metric);
	}
      else
	vty_out (vty, "  %c%*c",
		 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
		 ? '*' : ' ',
		 len - 3, ' ');

      switch (nexthop->type)
	{
	case NEXTHOP_TYPE_IPV4:
	case NEXTHOP_TYPE_IPV4_IFINDEX:
	  vty_out (vty, " via %s", inet_ntoa (nexthop->gate.ipv4));
	  if (nexthop->ifindex)
	    vty_out (vty, ", %s", ifindex2ifname (nexthop->ifindex));
	  break;
	case NEXTHOP_TYPE_IFINDEX:
	  vty_out (vty, " is directly connected, %s",
		   ifindex2ifname (nexthop->ifindex));
	  break;
	case NEXTHOP_TYPE_IFNAME:
	  vty_out (vty, " is directly connected, %s", nexthop->ifname);
	  break;
	case NEXTHOP_TYPE_BLACKHOLE:
	  vty_out (vty, " is directly connected, Null0");
	default:
	  break;
	}
      if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	vty_out (vty, " inactive");

      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	{
	  vty_out (vty, " (recursive");
		
	  switch (nexthop->rtype)
	    {
	    case NEXTHOP_TYPE_IPV4:
	    case NEXTHOP_TYPE_IPV4_IFINDEX:
	      vty_out (vty, " via %s)", inet_ntoa (nexthop->rgate.ipv4));
	      break;
	    case NEXTHOP_TYPE_IFINDEX:
	    case NEXTHOP_TYPE_IFNAME:
	      vty_out (vty, " is directly connected, %s)",
		       ifindex2ifname (nexthop->rifindex));
	      break;
	    default:
	      break;
	    }
	}

      if (rib->type == ZEBRA_ROUTE_RIP
	  || rib->type == ZEBRA_ROUTE_OSPF
	  || rib->type == ZEBRA_ROUTE_BGP)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  ", %02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, ", %dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, ", %02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

#define SHOW_ROUTE_V4_HEADER "Codes: K - kernel route, C - connected, S - static, R - RIP, O - OSPF,%s       B - BGP, > - selected route, * - FIB route%s%s"

DEFUN (show_ip_route,
       show_ip_route_cmd,
       "show ip route",
       SHOW_STR
       IP_STR
       "IP routing table\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show all IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	if (first)
	  {
	    vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE, VTY_NEWLINE,
		     VTY_NEWLINE);
	    first = 0;
	  }
	vty_show_ip_route (vty, rn, rib);
      }
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_prefix_longer,
       show_ip_route_prefix_longer_cmd,
       "show ip route A.B.C.D/M longer-prefixes",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Show route matching the specified Network/Mask pair only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct prefix p;
  int ret;
  int first = 1;

  ret = str2prefix (argv[0], &p);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (prefix_match (&p, &rn->p))
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE,
		       VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_supernets,
       show_ip_route_supernets_cmd,
       "show ip route supernets-only",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Show supernet entries only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  u_int32_t addr; 
  int first = 1;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	addr = ntohl (rn->p.u.prefix4.s_addr);

	if ((IN_CLASSC (addr) && rn->p.prefixlen < 24)
	   || (IN_CLASSB (addr) && rn->p.prefixlen < 16)
	   || (IN_CLASSA (addr) && rn->p.prefixlen < 8)) 
	  {
	    if (first)
	      {
		vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE,
			 VTY_NEWLINE, VTY_NEWLINE);
		first = 0;
	      }
	    vty_show_ip_route (vty, rn, rib);
	  }
      }
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_protocol,
       show_ip_route_protocol_cmd,
       "show ip route (bgp|connected|kernel|ospf|rip|static)",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Border Gateway Protocol (BGP)\n"
       "Connected\n"
       "Kernel\n"
       "Open Shortest Path First (OSPF)\n"
       "Routing Information Protocol (RIP)\n"
       "Static routes\n")
{
  int type;
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  if (strncmp (argv[0], "b", 1) == 0)
    type = ZEBRA_ROUTE_BGP;
  else if (strncmp (argv[0], "c", 1) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "k", 1) ==0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "o", 1) == 0)
    type = ZEBRA_ROUTE_OSPF;
  else if (strncmp (argv[0], "r", 1) == 0)
    type = ZEBRA_ROUTE_RIP;
  else if (strncmp (argv[0], "s", 1) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else 
    {
      vty_out (vty, "Unknown route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (rib->type == type)
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V4_HEADER,
		       VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_addr,
       show_ip_route_addr_cmd,
       "show ip route A.B.C.D",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Network in the IP routing table to display\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed IPv4 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

DEFUN (show_ip_route_prefix,
       show_ip_route_prefix_cmd,
       "show ip route A.B.C.D/M",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed IPv4 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn || rn->p.prefixlen != p.prefixlen)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

void
zebra_show_ip_route (struct vty *vty, struct vrf *vrf)
{
  vty_out (vty, "IP routing table name is %s(%d)%s",
	   vrf->name ? vrf->name : "", vrf->id, VTY_NEWLINE);

  vty_out (vty, "Route Source    Networks%s", VTY_NEWLINE);
  vty_out (vty, "connected       %d%s", 0, VTY_NEWLINE);
  vty_out (vty, "static          %d%s", 0, VTY_NEWLINE);
  vty_out (vty, "rip             %d%s", 0, VTY_NEWLINE);

  vty_out (vty, "bgp             %d%s", 0, VTY_NEWLINE);
  vty_out (vty, " External: %d Internal: %d Local: %d%s",
	   0, 0, 0, VTY_NEWLINE);

  vty_out (vty, "ospf            %d%s", 0, VTY_NEWLINE);
  vty_out (vty,
	   "  Intra-area: %d Inter-area: %d External-1: %d External-2: %d%s",
	   0, 0, 0, 0, VTY_NEWLINE);
  vty_out (vty, "  NSSA External-1: %d NSSA External-2: %d%s",
	   0, 0, VTY_NEWLINE);

  vty_out (vty, "internal        %d%s", 0, VTY_NEWLINE);
  vty_out (vty, "Total           %d%s", 0, VTY_NEWLINE);
}

/* Show route summary.  */
DEFUN (show_ip_route_summary,
       show_ip_route_summary_cmd,
       "show ip route summary",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Summary of all routes\n")
{
  struct vrf *vrf;

  /* Default table id is zero.  */
  vrf = vrf_lookup (0);
  if (! vrf)
    {
      vty_out (vty, "%% No Default-IP-Routing-Table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  zebra_show_ip_route (vty, vrf);

  return CMD_SUCCESS;
}

/* Write IPv4 static route configuration. */
int
static_config_ipv4 (struct vty *vty)
{
  struct route_node *rn;
  struct static_ipv4 *si;  
  struct route_table *stable;
  int write;

  write = 0;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP, SAFI_UNICAST, 0);
  if (! stable)
    return -1;

  for (rn = route_top (stable); rn; rn = route_next (rn))
    for (si = rn->info; si; si = si->next)
      {
	vty_out (vty, "ip route %s/%d", inet_ntoa (rn->p.u.prefix4),
		 rn->p.prefixlen);

	switch (si->type)
	  {
	  case STATIC_IPV4_GATEWAY:
	    vty_out (vty, " %s", inet_ntoa (si->gate.ipv4));
	    break;
	  case STATIC_IPV4_IFNAME:
	    vty_out (vty, " %s", si->gate.ifname);
	    break;
	  case STATIC_IPV4_BLACKHOLE:
	    vty_out (vty, " Null0");
	    break;
	  }

	if (si->distance != ZEBRA_STATIC_DISTANCE_DEFAULT)
	  vty_out (vty, " %d", si->distance);
	vty_out (vty, "%s", VTY_NEWLINE);

	write = 1;
      }
  return write;
}

#ifdef HAVE_IPV6
/* General fucntion for IPv6 static route. */
int
static_ipv6_func (struct vty *vty, int add_cmd, char *dest_str,
		  char *gate_str, char *ifname, char *distance_str)
{
  int ret;
  u_char distance;
  struct prefix p;
  struct in6_addr *gate = NULL;
  struct in6_addr gate_addr;
  u_char type = 0;
  int table = 0;
  
  ret = str2prefix (dest_str, &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Apply mask for given prefix. */
  apply_mask (&p);

  /* Administrative distance. */
  if (distance_str)
    distance = atoi (distance_str);
  else
    distance = ZEBRA_STATIC_DISTANCE_DEFAULT;

  /* Null0 static route.  */
  if (strncasecmp (gate_str, "Null0", strlen (gate_str)) == 0)
    {
      type = STATIC_IPV6_BLACKHOLE;
      if (add_cmd)
	static_add_ipv6 (&p, type, NULL, NULL, distance, 0);
      else
	static_delete_ipv6 (&p, type, NULL, NULL, distance, 0);
      return CMD_SUCCESS;
    }

  /* When gateway is valid IPv6 addrees, then gate is treated as
     nexthop address other case gate is treated as interface name. */
  ret = inet_pton (AF_INET6, gate_str, &gate_addr);

  if (ifname)
    {
      /* When ifname is specified.  It must be come with gateway
         address. */
      if (ret != 1)
	{
	  vty_out (vty, "%% Malformed address%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
      type = STATIC_IPV6_GATEWAY_IFNAME;
      gate = &gate_addr;
    }
  else
    {
      if (ret == 1)
	{
	  type = STATIC_IPV6_GATEWAY;
	  gate = &gate_addr;
	}
      else
	{
	  type = STATIC_IPV6_IFNAME;
	  ifname = gate_str;
	}
    }

  if (add_cmd)
    static_add_ipv6 (&p, type, gate, ifname, distance, table);
  else
    static_delete_ipv6 (&p, type, gate, ifname, distance, table);

  return CMD_SUCCESS;
}

DEFUN (ipv6_route,
       ipv6_route_cmd,
       "ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE|null0)",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n")
{
  return static_ipv6_func (vty, 1, argv[0], argv[1], NULL, NULL);
}

DEFUN (ipv6_route_ifname,
       ipv6_route_ifname_cmd,
       "ipv6 route X:X::X:X/M X:X::X:X INTERFACE",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n")
{
  return static_ipv6_func (vty, 1, argv[0], argv[1], argv[2], NULL);
}

DEFUN (ipv6_route_pref,
       ipv6_route_pref_cmd,
       "ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE|null0) <1-255>",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  return static_ipv6_func (vty, 1, argv[0], argv[1], NULL, argv[2]);
}

DEFUN (ipv6_route_ifname_pref,
       ipv6_route_ifname_pref_cmd,
       "ipv6 route X:X::X:X/M X:X::X:X INTERFACE <1-255>",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  return static_ipv6_func (vty, 1, argv[0], argv[1], argv[2], argv[3]);
}

DEFUN (no_ipv6_route,
       no_ipv6_route_cmd,
       "no ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE|null0)",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n")
{
  return static_ipv6_func (vty, 0, argv[0], argv[1], NULL, NULL);
}

DEFUN (no_ipv6_route_ifname,
       no_ipv6_route_ifname_cmd,
       "no ipv6 route X:X::X:X/M X:X::X:X INTERFACE",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n")
{
  return static_ipv6_func (vty, 0, argv[0], argv[1], argv[2], NULL);
}

DEFUN (no_ipv6_route_pref,
       no_ipv6_route_pref_cmd,
       "no ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE|null0) <1-255>",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  return static_ipv6_func (vty, 0, argv[0], argv[1], NULL, argv[2]);
}

DEFUN (no_ipv6_route_ifname_pref,
       no_ipv6_route_ifname_pref_cmd,
       "no ipv6 route X:X::X:X/M X:X::X:X INTERFACE <1-255>",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  return static_ipv6_func (vty, 0, argv[0], argv[1], argv[2], argv[3]);
}

/* New RIB.  Detailed information for IPv6 route. */
void
vty_show_ipv6_route_detail (struct vty *vty, struct route_node *rn)
{
  struct rib *rib;
  struct nexthop *nexthop;
  char buf[BUFSIZ];

  for (rib = rn->info; rib; rib = rib->next)
    {
      vty_out (vty, "Routing entry for %s/%d%s", 
	       inet_ntop (AF_INET6, &rn->p.u.prefix6, buf, BUFSIZ),
	       rn->p.prefixlen,
	       VTY_NEWLINE);
      vty_out (vty, "  Known via \"%s\"", route_type_str (rib->type));
      vty_out (vty, ", distance %d, metric %d", rib->distance, rib->metric);
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	vty_out (vty, ", best");
      if (rib->refcnt)
	vty_out (vty, ", refcnt %ld", rib->refcnt);
      vty_out (vty, "%s", VTY_NEWLINE);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7
      if (rib->type == ZEBRA_ROUTE_RIPNG
	  || rib->type == ZEBRA_ROUTE_OSPF6
	  || rib->type == ZEBRA_ROUTE_BGP)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

	  vty_out (vty, "  Last update ");

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  "%02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, "%dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, "%02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	  vty_out (vty, " ago%s", VTY_NEWLINE);
	}

      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	{
	  vty_out (vty, "  %c",
		   CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB) ? '*' : ' ');

	  switch (nexthop->type)
	    {
	    case NEXTHOP_TYPE_IPV6:
	    case NEXTHOP_TYPE_IPV6_IFINDEX:
	    case NEXTHOP_TYPE_IPV6_IFNAME:
	      vty_out (vty, " %s",
		       inet_ntop (AF_INET6, &nexthop->gate.ipv6, buf, BUFSIZ));
	      if (nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME)
		vty_out (vty, ", %s", nexthop->ifname);
	      else if (nexthop->ifindex)
		vty_out (vty, ", via %s", ifindex2ifname (nexthop->ifindex));
	      break;
	    case NEXTHOP_TYPE_IFINDEX:
	      vty_out (vty, " directly connected, %s",
		       ifindex2ifname (nexthop->ifindex));
	      break;
	    case NEXTHOP_TYPE_IFNAME:
	      vty_out (vty, " directly connected, %s",
		       nexthop->ifname);
	      break;
	    case NEXTHOP_TYPE_BLACKHOLE:
	      vty_out (vty, " directly connected, via Null0");
	      break;
	    default:
	      break;
	    }
	  if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	    vty_out (vty, " inactive");

	  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	    {
	      vty_out (vty, " (recursive");
		
	      switch (nexthop->rtype)
		{
		case NEXTHOP_TYPE_IPV6:
		case NEXTHOP_TYPE_IPV6_IFINDEX:
		case NEXTHOP_TYPE_IPV6_IFNAME:
		  vty_out (vty, " via %s)",
			   inet_ntop (AF_INET6, &nexthop->rgate.ipv6,
				      buf, BUFSIZ));
		  if (nexthop->rifindex)
		    vty_out (vty, ", %s", ifindex2ifname (nexthop->rifindex));
		  break;
		case NEXTHOP_TYPE_IFINDEX:
		case NEXTHOP_TYPE_IFNAME:
		  vty_out (vty, " is directly connected, %s)",
			   ifindex2ifname (nexthop->rifindex));
		  break;
		default:
		  break;
		}
	    }
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

void
vty_show_ipv6_route (struct vty *vty, struct route_node *rn,
		     struct rib *rib)
{
  struct nexthop *nexthop;
  int len = 0;
  char buf[BUFSIZ];

  /* Nexthop information. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (nexthop == rib->nexthop)
	{
	  /* Prefix information. */
	  len = vty_out (vty, "%c%c%c %s/%d",
			 route_type_char (rib->type),
			 CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED)
			 ? '>' : ' ',
			 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
			 ? '*' : ' ',
			 inet_ntop (AF_INET6, &rn->p.u.prefix6, buf, BUFSIZ),
			 rn->p.prefixlen);

	  /* Distance and metric display. */
	  if (rib->type != ZEBRA_ROUTE_CONNECT 
	      && rib->type != ZEBRA_ROUTE_KERNEL)
	    len += vty_out (vty, " [%d/%d]", rib->distance,
			    rib->metric);
	}
      else
	vty_out (vty, "  %c%*c",
		 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
		 ? '*' : ' ',
		 len - 3, ' ');

      switch (nexthop->type)
	{
	case NEXTHOP_TYPE_IPV6:
	case NEXTHOP_TYPE_IPV6_IFINDEX:
	case NEXTHOP_TYPE_IPV6_IFNAME:
	  vty_out (vty, " via %s",
		   inet_ntop (AF_INET6, &nexthop->gate.ipv6, buf, BUFSIZ));
	  if (nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME)
	    vty_out (vty, ", %s", nexthop->ifname);
	  else if (nexthop->ifindex)
	    vty_out (vty, ", %s", ifindex2ifname (nexthop->ifindex));
	  break;
	case NEXTHOP_TYPE_IFINDEX:
	  vty_out (vty, " is directly connected, %s",
		   ifindex2ifname (nexthop->ifindex));
	  break;
	case NEXTHOP_TYPE_IFNAME:
	  vty_out (vty, " is directly connected, %s",
		   nexthop->ifname);
	  break;
	case NEXTHOP_TYPE_BLACKHOLE:
	  vty_out (vty, " is directly connected, Null0");
	default:
	  break;
	}
      if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	vty_out (vty, " inactive");

      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
	{
	  vty_out (vty, " (recursive");
		
	  switch (nexthop->rtype)
	    {
	    case NEXTHOP_TYPE_IPV6:
	    case NEXTHOP_TYPE_IPV6_IFINDEX:
	    case NEXTHOP_TYPE_IPV6_IFNAME:
	      vty_out (vty, " via %s)",
		       inet_ntop (AF_INET6, &nexthop->rgate.ipv6,
				  buf, BUFSIZ));
	      if (nexthop->rifindex)
		vty_out (vty, ", %s", ifindex2ifname (nexthop->rifindex));
	      break;
	    case NEXTHOP_TYPE_IFINDEX:
	    case NEXTHOP_TYPE_IFNAME:
	      vty_out (vty, " is directly connected, %s)",
		       ifindex2ifname (nexthop->rifindex));
	      break;
	    default:
	      break;
	    }
	}

      if (rib->type == ZEBRA_ROUTE_RIPNG
	  || rib->type == ZEBRA_ROUTE_OSPF6
	  || rib->type == ZEBRA_ROUTE_BGP)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  ", %02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, ", %dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, ", %02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

#define SHOW_ROUTE_V6_HEADER "Codes: K - kernel route, C - connected, S - static, R - RIPng, O - OSPFv3,%s       B - BGP, * - FIB route.%s%s"

DEFUN (show_ipv6_route,
       show_ipv6_route_cmd,
       "show ipv6 route",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show all IPv6 route. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	if (first)
	  {
	    vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	    first = 0;
	  }
	vty_show_ipv6_route (vty, rn, rib);
      }
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_prefix_longer,
       show_ipv6_route_prefix_longer_cmd,
       "show ipv6 route X:X::X:X/M longer-prefixes",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 prefix\n"
       "Show route matching the specified Network/Mask pair only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct prefix p;
  int ret;
  int first = 1;

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  ret = str2prefix (argv[0], &p);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Show matched type IPv6 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (prefix_match (&p, &rn->p))
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ipv6_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_protocol,
       show_ipv6_route_protocol_cmd,
       "show ipv6 route (bgp|connected|kernel|ospf6|ripng|static)",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Border Gateway Protocol (BGP)\n"
       "Connected\n"
       "Kernel\n"
       "Open Shortest Path First (OSPFv3)\n"
       "Routing Information Protocol (RIPng)\n"
       "Static routes\n")
{
  int type;
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  if (strncmp (argv[0], "b", 1) == 0)
    type = ZEBRA_ROUTE_BGP;
  else if (strncmp (argv[0], "c", 1) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "k", 1) ==0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "o", 1) == 0)
    type = ZEBRA_ROUTE_OSPF6;
  else if (strncmp (argv[0], "r", 1) == 0)
    type = ZEBRA_ROUTE_RIPNG;
  else if (strncmp (argv[0], "s", 1) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else 
    {
      vty_out (vty, "Unknown route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv6 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (rib->type == type)
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ipv6_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_addr,
       show_ipv6_route_addr_cmd,
       "show ipv6 route X:X::X:X",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 Address\n")
{
  int ret;
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv6 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "Malformed IPv6 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ipv6_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_prefix,
       show_ipv6_route_prefix_cmd,
       "show ipv6 route X:X::X:X/M",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 prefix\n")
{
  int ret;
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv6 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "Malformed IPv6 prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn || rn->p.prefixlen != p.prefixlen)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ipv6_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}


/* Write IPv6 static route configuration. */
int
static_config_ipv6 (struct vty *vty)
{
  struct route_node *rn;
  struct static_ipv6 *si;  
  int write;
  char buf[BUFSIZ];
  struct route_table *stable;

  write = 0;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! stable)
    return -1;

  for (rn = route_top (stable); rn; rn = route_next (rn))
    for (si = rn->info; si; si = si->next)
      {
	vty_out (vty, "ipv6 route %s/%d",
		 inet_ntop (AF_INET6, &rn->p.u.prefix6, buf, BUFSIZ),
		 rn->p.prefixlen);

	switch (si->type)
	  {
	  case STATIC_IPV6_GATEWAY:
	    vty_out (vty, " %s", inet_ntop (AF_INET6, &si->ipv6, buf, BUFSIZ));
	    break;
	  case STATIC_IPV6_IFNAME:
	    vty_out (vty, " %s", si->ifname);
	    break;
	  case STATIC_IPV6_GATEWAY_IFNAME:
	    vty_out (vty, " %s %s",
		     inet_ntop (AF_INET6, &si->ipv6, buf, BUFSIZ), si->ifname);
	    break;
	  case STATIC_IPV6_BLACKHOLE:
	    vty_out (vty, " Null0");
	    break;
	  }

	if (si->distance != ZEBRA_STATIC_DISTANCE_DEFAULT)
	  vty_out (vty, " %d", si->distance);
	vty_out (vty, "%s", VTY_NEWLINE);

	write = 1;
      }
  return write;
}
#endif /* HAVE_IPV6 */

/* Static ip route configuration write function. */
int
zebra_ip_config (struct vty *vty)
{
  int write = 0;

  write += static_config_ipv4 (vty);
#ifdef HAVE_IPV6
  write += static_config_ipv6 (vty);
#endif /* HAVE_IPV6 */

  return write;
}

/* IP node for static routes. */
struct cmd_node ip_node = { IP_NODE,  "",  1 };

/* Route VTY.  */
void
zebra_vty_route_init ()
{
  install_node (&ip_node, zebra_ip_config);

  install_element (CONFIG_NODE, &ip_route_cmd);
  install_element (CONFIG_NODE, &ip_route_mask_cmd);
  install_element (CONFIG_NODE, &no_ip_route_cmd);
  install_element (CONFIG_NODE, &no_ip_route_mask_cmd);
  install_element (CONFIG_NODE, &ip_route_distance_cmd);
  install_element (CONFIG_NODE, &ip_route_mask_distance_cmd);
  install_element (CONFIG_NODE, &no_ip_route_distance_cmd);
  install_element (CONFIG_NODE, &no_ip_route_mask_distance_cmd);

  install_element (VIEW_NODE, &show_ip_route_cmd);
  install_element (VIEW_NODE, &show_ip_route_addr_cmd);
  install_element (VIEW_NODE, &show_ip_route_prefix_cmd);
  install_element (VIEW_NODE, &show_ip_route_prefix_longer_cmd);
  install_element (VIEW_NODE, &show_ip_route_protocol_cmd);
  install_element (VIEW_NODE, &show_ip_route_supernets_cmd);
  install_element (ENABLE_NODE, &show_ip_route_cmd);
  install_element (ENABLE_NODE, &show_ip_route_addr_cmd);
  install_element (ENABLE_NODE, &show_ip_route_prefix_cmd);
  install_element (ENABLE_NODE, &show_ip_route_prefix_longer_cmd);
  install_element (ENABLE_NODE, &show_ip_route_protocol_cmd);
  install_element (ENABLE_NODE, &show_ip_route_supernets_cmd);

#if 0
  install_element (VIEW_NODE, &show_ip_route_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_route_summary_cmd);
#endif /* 0 */

#ifdef HAVE_IPV6
  install_element (CONFIG_NODE, &ipv6_route_cmd);
  install_element (CONFIG_NODE, &ipv6_route_ifname_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_ifname_cmd);
  install_element (CONFIG_NODE, &ipv6_route_pref_cmd);
  install_element (CONFIG_NODE, &ipv6_route_ifname_pref_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_pref_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_ifname_pref_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_protocol_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_addr_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_prefix_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_prefix_longer_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_protocol_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_addr_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_prefix_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_prefix_longer_cmd);
#endif /* HAVE_IPV6 */
}

void
zebra_vty_init ()
{
  zebra_vty_route_init ();
}
