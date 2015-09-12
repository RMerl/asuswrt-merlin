/* BGP VTY interface.
   Copyright (C) 1996, 97, 98, 99, 2000 Kunihiro Ishiguro

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
#include "plist.h"
#include "buffer.h"
#include "linklist.h"
#include "stream.h"
#include "thread.h"
#include "log.h"
#include "memory.h"
#include "hash.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_advertise.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_ecommunity.h"
#include "bgpd/bgp_damp.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_nexthop.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_zebra.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_vty.h"
#include "bgpd/bgp_mpath.h"

extern struct in_addr router_id_zebra;

/* Utility function to get address family from current node.  */
afi_t
bgp_node_afi (struct vty *vty)
{
  if (vty->node == BGP_IPV6_NODE || vty->node == BGP_IPV6M_NODE)
    return AFI_IP6;
  return AFI_IP;
}

/* Utility function to get subsequent address family from current
   node.  */
safi_t
bgp_node_safi (struct vty *vty)
{
  if (vty->node == BGP_VPNV4_NODE)
    return SAFI_MPLS_VPN;
  if (vty->node == BGP_IPV4M_NODE || vty->node == BGP_IPV6M_NODE)
    return SAFI_MULTICAST;
  return SAFI_UNICAST;
}

static int
peer_address_self_check (union sockunion *su)
{
  struct interface *ifp = NULL;

  if (su->sa.sa_family == AF_INET)
    ifp = if_lookup_by_ipv4_exact (&su->sin.sin_addr);
#ifdef HAVE_IPV6
  else if (su->sa.sa_family == AF_INET6)
    ifp = if_lookup_by_ipv6_exact (&su->sin6.sin6_addr);
#endif /* HAVE IPV6 */

  if (ifp)
    return 1;

  return 0;
}

/* Utility function for looking up peer from VTY.  */
static struct peer *
peer_lookup_vty (struct vty *vty, const char *ip_str)
{
  int ret;
  struct bgp *bgp;
  union sockunion su;
  struct peer *peer;

  bgp = vty->index;

  ret = str2sockunion (ip_str, &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed address: %s%s", ip_str, VTY_NEWLINE);
      return NULL;
    }

  peer = peer_lookup (bgp, &su);
  if (! peer)
    {
      vty_out (vty, "%% Specify remote-as or peer-group commands first%s", VTY_NEWLINE);
      return NULL;
    }
  return peer;
}

/* Utility function for looking up peer or peer group.  */
static struct peer *
peer_and_group_lookup_vty (struct vty *vty, const char *peer_str)
{
  int ret;
  struct bgp *bgp;
  union sockunion su;
  struct peer *peer;
  struct peer_group *group;

  bgp = vty->index;

  ret = str2sockunion (peer_str, &su);
  if (ret == 0)
    {
      peer = peer_lookup (bgp, &su);
      if (peer)
	return peer;
    }
  else
    {
      group = peer_group_lookup (bgp, peer_str);
      if (group)
	return group->conf;
    }

  vty_out (vty, "%% Specify remote-as or peer-group commands first%s",
	   VTY_NEWLINE);

  return NULL;
}

static int
bgp_vty_return (struct vty *vty, int ret)
{
  const char *str = NULL;

  switch (ret)
    {
    case BGP_ERR_INVALID_VALUE:
      str = "Invalid value";
      break;
    case BGP_ERR_INVALID_FLAG:
      str = "Invalid flag";
      break;
    case BGP_ERR_PEER_INACTIVE:
      str = "Activate the neighbor for the address family first";
      break;
    case BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER:
      str = "Invalid command for a peer-group member";
      break;
    case BGP_ERR_PEER_GROUP_SHUTDOWN:
      str = "Peer-group has been shutdown. Activate the peer-group first";
      break;
    case BGP_ERR_PEER_GROUP_HAS_THE_FLAG:
      str = "This peer is a peer-group member.  Please change peer-group configuration";
      break;
    case BGP_ERR_PEER_FLAG_CONFLICT:
      str = "Can't set override-capability and strict-capability-match at the same time";
      break;
    case BGP_ERR_PEER_GROUP_MEMBER_EXISTS:
      str = "No activate for peergroup can be given only if peer-group has no members";
      break;
    case BGP_ERR_PEER_BELONGS_TO_GROUP:
      str = "No activate for an individual peer-group member is invalid";
      break;
    case BGP_ERR_PEER_GROUP_AF_UNCONFIGURED:
      str = "Activate the peer-group for the address family first";
      break;
    case BGP_ERR_PEER_GROUP_NO_REMOTE_AS:
      str = "Specify remote-as or peer-group remote AS first";
      break;
    case BGP_ERR_PEER_GROUP_CANT_CHANGE:
      str = "Cannot change the peer-group. Deconfigure first";
      break;
    case BGP_ERR_PEER_GROUP_MISMATCH:
      str = "Cannot have different peer-group for the neighbor";
      break;
    case BGP_ERR_PEER_FILTER_CONFLICT:
      str = "Prefix/distribute list can not co-exist";
      break;
    case BGP_ERR_NOT_INTERNAL_PEER:
      str = "Invalid command. Not an internal neighbor";
      break;
    case BGP_ERR_REMOVE_PRIVATE_AS:
      str = "Private AS cannot be removed for IBGP peers";
      break;
    case BGP_ERR_LOCAL_AS_ALLOWED_ONLY_FOR_EBGP:
      str = "Local-AS allowed only for EBGP peers";
      break;
    case BGP_ERR_CANNOT_HAVE_LOCAL_AS_SAME_AS:
      str = "Cannot have local-as same as BGP AS number";
      break;
    case BGP_ERR_TCPSIG_FAILED:
      str = "Error while applying TCP-Sig to session(s)";
      break;
    case BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK:
      str = "ebgp-multihop and ttl-security cannot be configured together";
      break;
    case BGP_ERR_NO_IBGP_WITH_TTLHACK:
      str = "ttl-security only allowed for EBGP peers";
      break;
    }
  if (str)
    {
      vty_out (vty, "%% %s%s", str, VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

/* BGP global configuration.  */

DEFUN (bgp_multiple_instance_func,
       bgp_multiple_instance_cmd,
       "bgp multiple-instance",
       BGP_STR
       "Enable bgp multiple instance\n")
{
  bgp_option_set (BGP_OPT_MULTIPLE_INSTANCE);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_multiple_instance,
       no_bgp_multiple_instance_cmd,
       "no bgp multiple-instance",
       NO_STR
       BGP_STR
       "BGP multiple instance\n")
{
  int ret;

  ret = bgp_option_unset (BGP_OPT_MULTIPLE_INSTANCE);
  if (ret < 0)
    {
      vty_out (vty, "%% There are more than two BGP instances%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

DEFUN (bgp_config_type,
       bgp_config_type_cmd,
       "bgp config-type (cisco|zebra)",
       BGP_STR
       "Configuration type\n"
       "cisco\n"
       "zebra\n")
{
  if (strncmp (argv[0], "c", 1) == 0)
    bgp_option_set (BGP_OPT_CONFIG_CISCO);
  else
    bgp_option_unset (BGP_OPT_CONFIG_CISCO);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_config_type,
       no_bgp_config_type_cmd,
       "no bgp config-type",
       NO_STR
       BGP_STR
       "Display configuration type\n")
{
  bgp_option_unset (BGP_OPT_CONFIG_CISCO);
  return CMD_SUCCESS;
}

DEFUN (no_synchronization,
       no_synchronization_cmd,
       "no synchronization",
       NO_STR
       "Perform IGP synchronization\n")
{
  return CMD_SUCCESS;
}

DEFUN (no_auto_summary,
       no_auto_summary_cmd,
       "no auto-summary",
       NO_STR
       "Enable automatic network number summarization\n")
{
  return CMD_SUCCESS;
}

DEFUN_DEPRECATED (neighbor_version,
		  neighbor_version_cmd,
		  NEIGHBOR_CMD "version (4|4-)",
		  NEIGHBOR_STR
		  NEIGHBOR_ADDR_STR
		  "Set the BGP version to match a neighbor\n"
		  "Neighbor's BGP version\n")
{
  return CMD_SUCCESS;
}

/* "router bgp" commands. */
DEFUN (router_bgp, 
       router_bgp_cmd, 
       "router bgp " CMD_AS_RANGE,
       ROUTER_STR
       BGP_STR
       AS_STR)
{
  int ret;
  as_t as;
  struct bgp *bgp;
  const char *name = NULL;

  VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);

  if (argc == 2)
    name = argv[1];

  ret = bgp_get (&bgp, &as, name);
  switch (ret)
    {
    case BGP_ERR_MULTIPLE_INSTANCE_NOT_SET:
      vty_out (vty, "Please specify 'bgp multiple-instance' first%s", 
	       VTY_NEWLINE);
      return CMD_WARNING;
    case BGP_ERR_AS_MISMATCH:
      vty_out (vty, "BGP is already running; AS is %u%s", as, VTY_NEWLINE);
      return CMD_WARNING;
    case BGP_ERR_INSTANCE_MISMATCH:
      vty_out (vty, "BGP view name and AS number mismatch%s", VTY_NEWLINE);
      vty_out (vty, "BGP instance is already running; AS is %u%s",
	       as, VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty->node = BGP_NODE;
  vty->index = bgp;

  return CMD_SUCCESS;
}

ALIAS (router_bgp,
       router_bgp_view_cmd,
       "router bgp " CMD_AS_RANGE " view WORD",
       ROUTER_STR
       BGP_STR
       AS_STR
       "BGP view\n"
       "view name\n")

/* "no router bgp" commands. */
DEFUN (no_router_bgp,
       no_router_bgp_cmd,
       "no router bgp " CMD_AS_RANGE,
       NO_STR
       ROUTER_STR
       BGP_STR
       AS_STR)
{
  as_t as;
  struct bgp *bgp;
  const char *name = NULL;

  VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);

  if (argc == 2)
    name = argv[1];

  /* Lookup bgp structure. */
  bgp = bgp_lookup (as, name);
  if (! bgp)
    {
      vty_out (vty, "%% Can't find BGP instance%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_delete (bgp);

  return CMD_SUCCESS;
}

ALIAS (no_router_bgp,
       no_router_bgp_view_cmd,
       "no router bgp " CMD_AS_RANGE " view WORD",
       NO_STR
       ROUTER_STR
       BGP_STR
       AS_STR
       "BGP view\n"
       "view name\n")

/* BGP router-id.  */

DEFUN (bgp_router_id,
       bgp_router_id_cmd,
       "bgp router-id A.B.C.D",
       BGP_STR
       "Override configured router identifier\n"
       "Manually configured router identifier\n")
{
  int ret;
  struct in_addr id;
  struct bgp *bgp;

  bgp = vty->index;

  ret = inet_aton (argv[0], &id);
  if (! ret)
    {
      vty_out (vty, "%% Malformed bgp router identifier%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp->router_id_static = id;
  bgp_router_id_set (bgp, &id);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_router_id,
       no_bgp_router_id_cmd,
       "no bgp router-id",
       NO_STR
       BGP_STR
       "Override configured router identifier\n")
{
  int ret;
  struct in_addr id;
  struct bgp *bgp;

  bgp = vty->index;

  if (argc == 1)
    {
      ret = inet_aton (argv[0], &id);
      if (! ret)
	{
	  vty_out (vty, "%% Malformed BGP router identifier%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}

      if (! IPV4_ADDR_SAME (&bgp->router_id_static, &id))
	{
	  vty_out (vty, "%% BGP router-id doesn't match%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }

  bgp->router_id_static.s_addr = 0;
  bgp_router_id_set (bgp, &router_id_zebra);

  return CMD_SUCCESS;
}

ALIAS (no_bgp_router_id,
       no_bgp_router_id_val_cmd,
       "no bgp router-id A.B.C.D",
       NO_STR
       BGP_STR
       "Override configured router identifier\n"
       "Manually configured router identifier\n")

/* BGP Cluster ID.  */

DEFUN (bgp_cluster_id,
       bgp_cluster_id_cmd,
       "bgp cluster-id A.B.C.D",
       BGP_STR
       "Configure Route-Reflector Cluster-id\n"
       "Route-Reflector Cluster-id in IP address format\n")
{
  int ret;
  struct bgp *bgp;
  struct in_addr cluster;

  bgp = vty->index;

  ret = inet_aton (argv[0], &cluster);
  if (! ret)
    {
      vty_out (vty, "%% Malformed bgp cluster identifier%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_cluster_id_set (bgp, &cluster);

  return CMD_SUCCESS;
}

ALIAS (bgp_cluster_id,
       bgp_cluster_id32_cmd,
       "bgp cluster-id <1-4294967295>",
       BGP_STR
       "Configure Route-Reflector Cluster-id\n"
       "Route-Reflector Cluster-id as 32 bit quantity\n")

DEFUN (no_bgp_cluster_id,
       no_bgp_cluster_id_cmd,
       "no bgp cluster-id",
       NO_STR
       BGP_STR
       "Configure Route-Reflector Cluster-id\n")
{
  int ret;
  struct bgp *bgp;
  struct in_addr cluster;

  bgp = vty->index;

  if (argc == 1)
    {
      ret = inet_aton (argv[0], &cluster);
      if (! ret)
	{
	  vty_out (vty, "%% Malformed bgp cluster identifier%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }

  bgp_cluster_id_unset (bgp);

  return CMD_SUCCESS;
}

ALIAS (no_bgp_cluster_id,
       no_bgp_cluster_id_arg_cmd,
       "no bgp cluster-id A.B.C.D",
       NO_STR
       BGP_STR
       "Configure Route-Reflector Cluster-id\n"
       "Route-Reflector Cluster-id in IP address format\n")

DEFUN (bgp_confederation_identifier,
       bgp_confederation_identifier_cmd,
       "bgp confederation identifier " CMD_AS_RANGE,
       "BGP specific commands\n"
       "AS confederation parameters\n"
       "AS number\n"
       "Set routing domain confederation AS\n")
{
  struct bgp *bgp;
  as_t as;

  bgp = vty->index;

  VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);

  bgp_confederation_id_set (bgp, as);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_confederation_identifier,
       no_bgp_confederation_identifier_cmd,
       "no bgp confederation identifier",
       NO_STR
       "BGP specific commands\n"
       "AS confederation parameters\n"
       "AS number\n")
{
  struct bgp *bgp;
  as_t as __attribute__((unused)); /* Dummy for VTY_GET_INTEGER_RANGE */

  bgp = vty->index;

  if (argc == 1)
    VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);

  bgp_confederation_id_unset (bgp);

  return CMD_SUCCESS;
}

ALIAS (no_bgp_confederation_identifier,
       no_bgp_confederation_identifier_arg_cmd,
       "no bgp confederation identifier " CMD_AS_RANGE,
       NO_STR
       "BGP specific commands\n"
       "AS confederation parameters\n"
       "AS number\n"
       "Set routing domain confederation AS\n")

DEFUN (bgp_confederation_peers,
       bgp_confederation_peers_cmd,
       "bgp confederation peers ." CMD_AS_RANGE,
       "BGP specific commands\n"
       "AS confederation parameters\n"
       "Peer ASs in BGP confederation\n"
       AS_STR)
{
  struct bgp *bgp;
  as_t as;
  int i;

  bgp = vty->index;

  for (i = 0; i < argc; i++)
    {
      VTY_GET_INTEGER_RANGE ("AS", as, argv[i], 1, BGP_AS4_MAX);

      if (bgp->as == as)
	{
	  vty_out (vty, "%% Local member-AS not allowed in confed peer list%s",
		   VTY_NEWLINE);
	  continue;
	}

      bgp_confederation_peers_add (bgp, as);
    }
  return CMD_SUCCESS;
}

DEFUN (no_bgp_confederation_peers,
       no_bgp_confederation_peers_cmd,
       "no bgp confederation peers ." CMD_AS_RANGE,
       NO_STR
       "BGP specific commands\n"
       "AS confederation parameters\n"
       "Peer ASs in BGP confederation\n"
       AS_STR)
{
  struct bgp *bgp;
  as_t as;
  int i;

  bgp = vty->index;

  for (i = 0; i < argc; i++)
    {
      VTY_GET_INTEGER_RANGE ("AS", as, argv[i], 1, BGP_AS4_MAX);

      bgp_confederation_peers_remove (bgp, as);
    }
  return CMD_SUCCESS;
}

/* Maximum-paths configuration */
DEFUN (bgp_maxpaths,
       bgp_maxpaths_cmd,
       "maximum-paths <1-255>",
       "Forward packets over multiple paths\n"
       "Number of paths\n")
{
  struct bgp *bgp;
  u_int16_t maxpaths;
  int ret;

  bgp = vty->index;

  VTY_GET_INTEGER_RANGE ("maximum-paths", maxpaths, argv[0], 1, 255);

  ret = bgp_maximum_paths_set (bgp, bgp_node_afi (vty), bgp_node_safi(vty),
			       BGP_PEER_EBGP, maxpaths);
  if (ret < 0)
    {
      vty_out (vty,
	       "%% Failed to set maximum-paths %u for afi %u, safi %u%s",
	       maxpaths, bgp_node_afi (vty), bgp_node_safi(vty), VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (bgp_maxpaths_ibgp,
       bgp_maxpaths_ibgp_cmd,
       "maximum-paths ibgp <1-255>",
       "Forward packets over multiple paths\n"
       "iBGP-multipath\n"
       "Number of paths\n")
{
  struct bgp *bgp;
  u_int16_t maxpaths;
  int ret;

  bgp = vty->index;

  VTY_GET_INTEGER_RANGE ("maximum-paths", maxpaths, argv[0], 1, 255);

  ret = bgp_maximum_paths_set (bgp, bgp_node_afi (vty), bgp_node_safi(vty),
			       BGP_PEER_IBGP, maxpaths);
  if (ret < 0)
    {
      vty_out (vty,
	       "%% Failed to set maximum-paths ibgp %u for afi %u, safi %u%s",
	       maxpaths, bgp_node_afi (vty), bgp_node_safi(vty), VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (no_bgp_maxpaths,
       no_bgp_maxpaths_cmd,
       "no maximum-paths",
       NO_STR
       "Forward packets over multiple paths\n"
       "Number of paths\n")
{
  struct bgp *bgp;
  int ret;

  bgp = vty->index;

  ret = bgp_maximum_paths_unset (bgp, bgp_node_afi (vty), bgp_node_safi(vty),
				 BGP_PEER_EBGP);
  if (ret < 0)
    {
      vty_out (vty,
	       "%% Failed to unset maximum-paths for afi %u, safi %u%s",
	       bgp_node_afi (vty), bgp_node_safi(vty), VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

ALIAS (no_bgp_maxpaths,
       no_bgp_maxpaths_arg_cmd,
       "no maximum-paths <1-255>",
       NO_STR
       "Forward packets over multiple paths\n"
       "Number of paths\n")

DEFUN (no_bgp_maxpaths_ibgp,
       no_bgp_maxpaths_ibgp_cmd,
       "no maximum-paths ibgp",
       NO_STR
       "Forward packets over multiple paths\n"
       "iBGP-multipath\n"
       "Number of paths\n")
{
  struct bgp *bgp;
  int ret;

  bgp = vty->index;

  ret = bgp_maximum_paths_unset (bgp, bgp_node_afi (vty), bgp_node_safi(vty),
				 BGP_PEER_IBGP);
  if (ret < 0)
    {
      vty_out (vty,
	       "%% Failed to unset maximum-paths ibgp for afi %u, safi %u%s",
	       bgp_node_afi (vty), bgp_node_safi(vty), VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

ALIAS (no_bgp_maxpaths_ibgp,
       no_bgp_maxpaths_ibgp_arg_cmd,
       "no maximum-paths ibgp <1-255>",
       NO_STR
       "Forward packets over multiple paths\n"
       "iBGP-multipath\n"
       "Number of paths\n")

int
bgp_config_write_maxpaths (struct vty *vty, struct bgp *bgp, afi_t afi,
			   safi_t safi, int *write)
{
  if (bgp->maxpaths[afi][safi].maxpaths_ebgp != BGP_DEFAULT_MAXPATHS)
    {
      bgp_config_write_family_header (vty, afi, safi, write);
      vty_out (vty, " maximum-paths %d%s",
	       bgp->maxpaths[afi][safi].maxpaths_ebgp, VTY_NEWLINE);
    }

  if (bgp->maxpaths[afi][safi].maxpaths_ibgp != BGP_DEFAULT_MAXPATHS)
    {
      bgp_config_write_family_header (vty, afi, safi, write);
      vty_out (vty, " maximum-paths ibgp %d%s",
	       bgp->maxpaths[afi][safi].maxpaths_ibgp, VTY_NEWLINE);
    }

  return 0;
}

/* BGP timers.  */

DEFUN (bgp_timers,
       bgp_timers_cmd,
       "timers bgp <0-65535> <0-65535>",
       "Adjust routing timers\n"
       "BGP timers\n"
       "Keepalive interval\n"
       "Holdtime\n")
{
  struct bgp *bgp;
  unsigned long keepalive = 0;
  unsigned long holdtime = 0;

  bgp = vty->index;

  VTY_GET_INTEGER ("keepalive", keepalive, argv[0]);
  VTY_GET_INTEGER ("holdtime", holdtime, argv[1]);

  /* Holdtime value check. */
  if (holdtime < 3 && holdtime != 0)
    {
      vty_out (vty, "%% hold time value must be either 0 or greater than 3%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_timers_set (bgp, keepalive, holdtime);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_timers,
       no_bgp_timers_cmd,
       "no timers bgp",
       NO_STR
       "Adjust routing timers\n"
       "BGP timers\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_timers_unset (bgp);

  return CMD_SUCCESS;
}

ALIAS (no_bgp_timers,
       no_bgp_timers_arg_cmd,
       "no timers bgp <0-65535> <0-65535>",
       NO_STR
       "Adjust routing timers\n"
       "BGP timers\n"
       "Keepalive interval\n"
       "Holdtime\n")

DEFUN (bgp_client_to_client_reflection,
       bgp_client_to_client_reflection_cmd,
       "bgp client-to-client reflection",
       "BGP specific commands\n"
       "Configure client to client route reflection\n"
       "reflection of routes allowed\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_NO_CLIENT_TO_CLIENT);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_client_to_client_reflection,
       no_bgp_client_to_client_reflection_cmd,
       "no bgp client-to-client reflection",
       NO_STR
       "BGP specific commands\n"
       "Configure client to client route reflection\n"
       "reflection of routes allowed\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_NO_CLIENT_TO_CLIENT);
  return CMD_SUCCESS;
}

/* "bgp always-compare-med" configuration. */
DEFUN (bgp_always_compare_med,
       bgp_always_compare_med_cmd,
       "bgp always-compare-med",
       "BGP specific commands\n"
       "Allow comparing MED from different neighbors\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_ALWAYS_COMPARE_MED);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_always_compare_med,
       no_bgp_always_compare_med_cmd,
       "no bgp always-compare-med",
       NO_STR
       "BGP specific commands\n"
       "Allow comparing MED from different neighbors\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_ALWAYS_COMPARE_MED);
  return CMD_SUCCESS;
}

/* "bgp deterministic-med" configuration. */
DEFUN (bgp_deterministic_med,
       bgp_deterministic_med_cmd,
       "bgp deterministic-med",
       "BGP specific commands\n"
       "Pick the best-MED path among paths advertised from the neighboring AS\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_DETERMINISTIC_MED);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_deterministic_med,
       no_bgp_deterministic_med_cmd,
       "no bgp deterministic-med",
       NO_STR
       "BGP specific commands\n"
       "Pick the best-MED path among paths advertised from the neighboring AS\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_DETERMINISTIC_MED);
  return CMD_SUCCESS;
}

/* "bgp graceful-restart" configuration. */
DEFUN (bgp_graceful_restart,
       bgp_graceful_restart_cmd,
       "bgp graceful-restart",
       "BGP specific commands\n"
       "Graceful restart capability parameters\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_GRACEFUL_RESTART);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_graceful_restart,
       no_bgp_graceful_restart_cmd,
       "no bgp graceful-restart",
       NO_STR
       "BGP specific commands\n"
       "Graceful restart capability parameters\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_GRACEFUL_RESTART);
  return CMD_SUCCESS;
}

DEFUN (bgp_graceful_restart_stalepath_time,
       bgp_graceful_restart_stalepath_time_cmd,
       "bgp graceful-restart stalepath-time <1-3600>",
       "BGP specific commands\n"
       "Graceful restart capability parameters\n"
       "Set the max time to hold onto restarting peer's stale paths\n"
       "Delay value (seconds)\n")
{
  struct bgp *bgp;
  u_int32_t stalepath;

  bgp = vty->index;
  if (! bgp)
    return CMD_WARNING;

  VTY_GET_INTEGER_RANGE ("stalepath-time", stalepath, argv[0], 1, 3600);
  bgp->stalepath_time = stalepath;
  return CMD_SUCCESS;
}

DEFUN (no_bgp_graceful_restart_stalepath_time,
       no_bgp_graceful_restart_stalepath_time_cmd,
       "no bgp graceful-restart stalepath-time",
       NO_STR
       "BGP specific commands\n"
       "Graceful restart capability parameters\n"
       "Set the max time to hold onto restarting peer's stale paths\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  if (! bgp)
    return CMD_WARNING;

  bgp->stalepath_time = BGP_DEFAULT_STALEPATH_TIME;
  return CMD_SUCCESS;
}

ALIAS (no_bgp_graceful_restart_stalepath_time,
       no_bgp_graceful_restart_stalepath_time_val_cmd,
       "no bgp graceful-restart stalepath-time <1-3600>",
       NO_STR
       "BGP specific commands\n"
       "Graceful restart capability parameters\n"
       "Set the max time to hold onto restarting peer's stale paths\n"
       "Delay value (seconds)\n")

/* "bgp fast-external-failover" configuration. */
DEFUN (bgp_fast_external_failover,
       bgp_fast_external_failover_cmd,
       "bgp fast-external-failover",
       BGP_STR
       "Immediately reset session if a link to a directly connected external peer goes down\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_NO_FAST_EXT_FAILOVER);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_fast_external_failover,
       no_bgp_fast_external_failover_cmd,
       "no bgp fast-external-failover",
       NO_STR
       BGP_STR
       "Immediately reset session if a link to a directly connected external peer goes down\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_NO_FAST_EXT_FAILOVER);
  return CMD_SUCCESS;
}

/* "bgp enforce-first-as" configuration. */
DEFUN (bgp_enforce_first_as,
       bgp_enforce_first_as_cmd,
       "bgp enforce-first-as",
       BGP_STR
       "Enforce the first AS for EBGP routes\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_ENFORCE_FIRST_AS);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_enforce_first_as,
       no_bgp_enforce_first_as_cmd,
       "no bgp enforce-first-as",
       NO_STR
       BGP_STR
       "Enforce the first AS for EBGP routes\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_ENFORCE_FIRST_AS);
  return CMD_SUCCESS;
}

/* "bgp bestpath compare-routerid" configuration.  */
DEFUN (bgp_bestpath_compare_router_id,
       bgp_bestpath_compare_router_id_cmd,
       "bgp bestpath compare-routerid",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "Compare router-id for identical EBGP paths\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_COMPARE_ROUTER_ID);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_bestpath_compare_router_id,
       no_bgp_bestpath_compare_router_id_cmd,
       "no bgp bestpath compare-routerid",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "Compare router-id for identical EBGP paths\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_COMPARE_ROUTER_ID);
  return CMD_SUCCESS;
}

/* "bgp bestpath as-path ignore" configuration.  */
DEFUN (bgp_bestpath_aspath_ignore,
       bgp_bestpath_aspath_ignore_cmd,
       "bgp bestpath as-path ignore",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Ignore as-path length in selecting a route\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_ASPATH_IGNORE);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_bestpath_aspath_ignore,
       no_bgp_bestpath_aspath_ignore_cmd,
       "no bgp bestpath as-path ignore",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Ignore as-path length in selecting a route\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_ASPATH_IGNORE);
  return CMD_SUCCESS;
}

/* "bgp bestpath as-path confed" configuration.  */
DEFUN (bgp_bestpath_aspath_confed,
       bgp_bestpath_aspath_confed_cmd,
       "bgp bestpath as-path confed",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Compare path lengths including confederation sets & sequences in selecting a route\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_ASPATH_CONFED);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_bestpath_aspath_confed,
       no_bgp_bestpath_aspath_confed_cmd,
       "no bgp bestpath as-path confed",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Compare path lengths including confederation sets & sequences in selecting a route\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_ASPATH_CONFED);
  return CMD_SUCCESS;
}

/* "bgp bestpath as-path multipath-relax" configuration.  */
DEFUN (bgp_bestpath_aspath_multipath_relax,
       bgp_bestpath_aspath_multipath_relax_cmd,
       "bgp bestpath as-path multipath-relax",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Allow load sharing across routes that have different AS paths (but same length)\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_ASPATH_MULTIPATH_RELAX);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_bestpath_aspath_multipath_relax,
       no_bgp_bestpath_aspath_multipath_relax_cmd,
       "no bgp bestpath as-path multipath-relax",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "AS-path attribute\n"
       "Allow load sharing across routes that have different AS paths (but same length)\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_ASPATH_MULTIPATH_RELAX);
  return CMD_SUCCESS;
}

/* "bgp log-neighbor-changes" configuration.  */
DEFUN (bgp_log_neighbor_changes,
       bgp_log_neighbor_changes_cmd,
       "bgp log-neighbor-changes",
       "BGP specific commands\n"
       "Log neighbor up/down and reset reason\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_LOG_NEIGHBOR_CHANGES);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_log_neighbor_changes,
       no_bgp_log_neighbor_changes_cmd,
       "no bgp log-neighbor-changes",
       NO_STR
       "BGP specific commands\n"
       "Log neighbor up/down and reset reason\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_LOG_NEIGHBOR_CHANGES);
  return CMD_SUCCESS;
}

/* "bgp bestpath med" configuration. */
DEFUN (bgp_bestpath_med,
       bgp_bestpath_med_cmd,
       "bgp bestpath med (confed|missing-as-worst)",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Compare MED among confederation paths\n"
       "Treat missing MED as the least preferred one\n")
{
  struct bgp *bgp;
  
  bgp = vty->index;

  if (strncmp (argv[0], "confed", 1) == 0)
    bgp_flag_set (bgp, BGP_FLAG_MED_CONFED);
  else
    bgp_flag_set (bgp, BGP_FLAG_MED_MISSING_AS_WORST);

  return CMD_SUCCESS;
}

DEFUN (bgp_bestpath_med2,
       bgp_bestpath_med2_cmd,
       "bgp bestpath med confed missing-as-worst",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Compare MED among confederation paths\n"
       "Treat missing MED as the least preferred one\n")
{
  struct bgp *bgp;
  
  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_MED_CONFED);
  bgp_flag_set (bgp, BGP_FLAG_MED_MISSING_AS_WORST);
  return CMD_SUCCESS;
}

ALIAS (bgp_bestpath_med2,
       bgp_bestpath_med3_cmd,
       "bgp bestpath med missing-as-worst confed",
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Treat missing MED as the least preferred one\n"
       "Compare MED among confederation paths\n")

DEFUN (no_bgp_bestpath_med,
       no_bgp_bestpath_med_cmd,
       "no bgp bestpath med (confed|missing-as-worst)",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Compare MED among confederation paths\n"
       "Treat missing MED as the least preferred one\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  
  if (strncmp (argv[0], "confed", 1) == 0)
    bgp_flag_unset (bgp, BGP_FLAG_MED_CONFED);
  else
    bgp_flag_unset (bgp, BGP_FLAG_MED_MISSING_AS_WORST);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_bestpath_med2,
       no_bgp_bestpath_med2_cmd,
       "no bgp bestpath med confed missing-as-worst",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Compare MED among confederation paths\n"
       "Treat missing MED as the least preferred one\n")
{
  struct bgp *bgp;
  
  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_MED_CONFED);
  bgp_flag_unset (bgp, BGP_FLAG_MED_MISSING_AS_WORST);
  return CMD_SUCCESS;
}

ALIAS (no_bgp_bestpath_med2,
       no_bgp_bestpath_med3_cmd,
       "no bgp bestpath med missing-as-worst confed",
       NO_STR
       "BGP specific commands\n"
       "Change the default bestpath selection\n"
       "MED attribute\n"
       "Treat missing MED as the least preferred one\n"
       "Compare MED among confederation paths\n")

/* "no bgp default ipv4-unicast". */
DEFUN (no_bgp_default_ipv4_unicast,
       no_bgp_default_ipv4_unicast_cmd,
       "no bgp default ipv4-unicast",
       NO_STR
       "BGP specific commands\n"
       "Configure BGP defaults\n"
       "Activate ipv4-unicast for a peer by default\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_NO_DEFAULT_IPV4);
  return CMD_SUCCESS;
}

DEFUN (bgp_default_ipv4_unicast,
       bgp_default_ipv4_unicast_cmd,
       "bgp default ipv4-unicast",
       "BGP specific commands\n"
       "Configure BGP defaults\n"
       "Activate ipv4-unicast for a peer by default\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_NO_DEFAULT_IPV4);
  return CMD_SUCCESS;
}

/* "bgp import-check" configuration.  */
DEFUN (bgp_network_import_check,
       bgp_network_import_check_cmd,
       "bgp network import-check",
       "BGP specific commands\n"
       "BGP network command\n"
       "Check BGP network route exists in IGP\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_set (bgp, BGP_FLAG_IMPORT_CHECK);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_network_import_check,
       no_bgp_network_import_check_cmd,
       "no bgp network import-check",
       NO_STR
       "BGP specific commands\n"
       "BGP network command\n"
       "Check BGP network route exists in IGP\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_flag_unset (bgp, BGP_FLAG_IMPORT_CHECK);
  return CMD_SUCCESS;
}

DEFUN (bgp_default_local_preference,
       bgp_default_local_preference_cmd,
       "bgp default local-preference <0-4294967295>",
       "BGP specific commands\n"
       "Configure BGP defaults\n"
       "local preference (higher=more preferred)\n"
       "Configure default local preference value\n")
{
  struct bgp *bgp;
  u_int32_t local_pref;

  bgp = vty->index;

  VTY_GET_INTEGER ("local preference", local_pref, argv[0]);

  bgp_default_local_preference_set (bgp, local_pref);

  return CMD_SUCCESS;
}

DEFUN (no_bgp_default_local_preference,
       no_bgp_default_local_preference_cmd,
       "no bgp default local-preference",
       NO_STR
       "BGP specific commands\n"
       "Configure BGP defaults\n"
       "local preference (higher=more preferred)\n")
{
  struct bgp *bgp;

  bgp = vty->index;
  bgp_default_local_preference_unset (bgp);
  return CMD_SUCCESS;
}

ALIAS (no_bgp_default_local_preference,
       no_bgp_default_local_preference_val_cmd,
       "no bgp default local-preference <0-4294967295>",
       NO_STR
       "BGP specific commands\n"
       "Configure BGP defaults\n"
       "local preference (higher=more preferred)\n"
       "Configure default local preference value\n")

static int
peer_remote_as_vty (struct vty *vty, const char *peer_str, 
                    const char *as_str, afi_t afi, safi_t safi)
{
  int ret;
  struct bgp *bgp;
  as_t as;
  union sockunion su;

  bgp = vty->index;

  /* Get AS number.  */
  VTY_GET_INTEGER_RANGE ("AS", as, as_str, 1, BGP_AS4_MAX);

  /* If peer is peer group, call proper function.  */
  ret = str2sockunion (peer_str, &su);
  if (ret < 0)
    {
      ret = peer_group_remote_as (bgp, peer_str, &as);
      if (ret < 0)
	{
	  vty_out (vty, "%% Create the peer-group first%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
      return CMD_SUCCESS;
    }

  if (peer_address_self_check (&su))
    {
      vty_out (vty, "%% Can not configure the local system as neighbor%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = peer_remote_as (bgp, &su, &as, afi, safi);

  /* This peer belongs to peer group.  */
  switch (ret)
    {
    case BGP_ERR_PEER_GROUP_MEMBER:
      vty_out (vty, "%% Peer-group AS %u. Cannot configure remote-as for member%s", as, VTY_NEWLINE);
      return CMD_WARNING;
    case BGP_ERR_PEER_GROUP_PEER_TYPE_DIFFERENT:
      vty_out (vty, "%% The AS# can not be changed from %u to %s, peer-group members must be all internal or all external%s", as, as_str, VTY_NEWLINE);
      return CMD_WARNING;
    }
  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_remote_as,
       neighbor_remote_as_cmd,
       NEIGHBOR_CMD2 "remote-as " CMD_AS_RANGE,
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a BGP neighbor\n"
       AS_STR)
{
  return peer_remote_as_vty (vty, argv[0], argv[1], AFI_IP, SAFI_UNICAST);
}

DEFUN (neighbor_peer_group,
       neighbor_peer_group_cmd,
       "neighbor WORD peer-group",
       NEIGHBOR_STR
       "Neighbor tag\n"
       "Configure peer-group\n")
{
  struct bgp *bgp;
  struct peer_group *group;

  bgp = vty->index;

  group = peer_group_get (bgp, argv[0]);
  if (! group)
    return CMD_WARNING;

  return CMD_SUCCESS;
}

DEFUN (no_neighbor,
       no_neighbor_cmd,
       NO_NEIGHBOR_CMD2,
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2)
{
  int ret;
  union sockunion su;
  struct peer_group *group;
  struct peer *peer;

  ret = str2sockunion (argv[0], &su);
  if (ret < 0)
    {
      group = peer_group_lookup (vty->index, argv[0]);
      if (group)
	peer_group_delete (group);
      else
	{
	  vty_out (vty, "%% Create the peer-group first%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  else
    {
      peer = peer_lookup (vty->index, &su);
      if (peer)
        peer_delete (peer);
    }

  return CMD_SUCCESS;
}

ALIAS (no_neighbor,
       no_neighbor_remote_as_cmd,
       NO_NEIGHBOR_CMD "remote-as " CMD_AS_RANGE,
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Specify a BGP neighbor\n"
       AS_STR)

DEFUN (no_neighbor_peer_group,
       no_neighbor_peer_group_cmd,
       "no neighbor WORD peer-group",
       NO_STR
       NEIGHBOR_STR
       "Neighbor tag\n"
       "Configure peer-group\n")
{
  struct peer_group *group;

  group = peer_group_lookup (vty->index, argv[0]);
  if (group)
    peer_group_delete (group);
  else
    {
      vty_out (vty, "%% Create the peer-group first%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

DEFUN (no_neighbor_peer_group_remote_as,
       no_neighbor_peer_group_remote_as_cmd,
       "no neighbor WORD remote-as " CMD_AS_RANGE,
       NO_STR
       NEIGHBOR_STR
       "Neighbor tag\n"
       "Specify a BGP neighbor\n"
       AS_STR)
{
  struct peer_group *group;

  group = peer_group_lookup (vty->index, argv[0]);
  if (group)
    peer_group_remote_as_delete (group);
  else
    {
      vty_out (vty, "%% Create the peer-group first%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

DEFUN (neighbor_local_as,
       neighbor_local_as_cmd,
       NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE,
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_local_as_set (peer, atoi (argv[1]), 0, 0);
  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_local_as_no_prepend,
       neighbor_local_as_no_prepend_cmd,
       NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE " no-prepend",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n"
       "Do not prepend local-as to updates from ebgp peers\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_local_as_set (peer, atoi (argv[1]), 1, 0);
  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_local_as_no_prepend_replace_as,
       neighbor_local_as_no_prepend_replace_as_cmd,
       NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE " no-prepend replace-as",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n"
       "Do not prepend local-as to updates from ebgp peers\n"
       "Do not prepend local-as to updates from ibgp peers\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_local_as_set (peer, atoi (argv[1]), 1, 1);
  return bgp_vty_return (vty, ret);
}


DEFUN (no_neighbor_local_as,
       no_neighbor_local_as_cmd,
       NO_NEIGHBOR_CMD2 "local-as",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_local_as_unset (peer);
  return bgp_vty_return (vty, ret);
}

ALIAS (no_neighbor_local_as,
       no_neighbor_local_as_val_cmd,
       NO_NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE,
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n")

ALIAS (no_neighbor_local_as,
       no_neighbor_local_as_val2_cmd,
       NO_NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE " no-prepend",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n"
       "Do not prepend local-as to updates from ebgp peers\n")

ALIAS (no_neighbor_local_as,
       no_neighbor_local_as_val3_cmd,
       NO_NEIGHBOR_CMD2 "local-as " CMD_AS_RANGE " no-prepend replace-as",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify a local-as number\n"
       "AS number used as local AS\n"
       "Do not prepend local-as to updates from ebgp peers\n"
       "Do not prepend local-as to updates from ibgp peers\n")

DEFUN (neighbor_password,
       neighbor_password_cmd,
       NEIGHBOR_CMD2 "password LINE",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Set a password\n"
       "The password\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_password_set (peer, argv[1]);
  return bgp_vty_return (vty, ret);
}

DEFUN (no_neighbor_password,
       no_neighbor_password_cmd,
       NO_NEIGHBOR_CMD2 "password",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Set a password\n")
{
  struct peer *peer;
  int ret;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_password_unset (peer);
  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_activate,
       neighbor_activate_cmd,
       NEIGHBOR_CMD2 "activate",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Enable the Address Family for this Neighbor\n")
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  peer_activate (peer, bgp_node_afi (vty), bgp_node_safi (vty));

  return CMD_SUCCESS;
}

DEFUN (no_neighbor_activate,
       no_neighbor_activate_cmd,
       NO_NEIGHBOR_CMD2 "activate",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Enable the Address Family for this Neighbor\n")
{
  int ret;
  struct peer *peer;

  /* Lookup peer. */
  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_deactivate (peer, bgp_node_afi (vty), bgp_node_safi (vty));

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_set_peer_group,
       neighbor_set_peer_group_cmd,
       NEIGHBOR_CMD "peer-group WORD",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Member of the peer-group\n"
       "peer-group name\n")
{
  int ret;
  as_t as;
  union sockunion su;
  struct bgp *bgp;
  struct peer_group *group;

  bgp = vty->index;

  ret = str2sockunion (argv[0], &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed address: %s%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  group = peer_group_lookup (bgp, argv[1]);
  if (! group)
    {
      vty_out (vty, "%% Configure the peer-group first%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (peer_address_self_check (&su))
    {
      vty_out (vty, "%% Can not configure the local system as neighbor%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = peer_group_bind (bgp, &su, group, bgp_node_afi (vty), 
			 bgp_node_safi (vty), &as);

  if (ret == BGP_ERR_PEER_GROUP_PEER_TYPE_DIFFERENT)
    {
      vty_out (vty, "%% Peer with AS %u cannot be in this peer-group, members must be all internal or all external%s", as, VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_vty_return (vty, ret);
}

DEFUN (no_neighbor_set_peer_group,
       no_neighbor_set_peer_group_cmd,
       NO_NEIGHBOR_CMD "peer-group WORD",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Member of the peer-group\n"
       "peer-group name\n")
{
  int ret;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;

  bgp = vty->index;

  peer = peer_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  group = peer_group_lookup (bgp, argv[1]);
  if (! group)
    {
      vty_out (vty, "%% Configure the peer-group first%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = peer_group_unbind (bgp, peer, group, bgp_node_afi (vty),
			   bgp_node_safi (vty));

  return bgp_vty_return (vty, ret);
}

static int
peer_flag_modify_vty (struct vty *vty, const char *ip_str, 
                      u_int16_t flag, int set)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  if (set)
    ret = peer_flag_set (peer, flag);
  else
    ret = peer_flag_unset (peer, flag);

  return bgp_vty_return (vty, ret);
}

static int
peer_flag_set_vty (struct vty *vty, const char *ip_str, u_int16_t flag)
{
  return peer_flag_modify_vty (vty, ip_str, flag, 1);
}

static int
peer_flag_unset_vty (struct vty *vty, const char *ip_str, u_int16_t flag)
{
  return peer_flag_modify_vty (vty, ip_str, flag, 0);
}

/* neighbor passive. */
DEFUN (neighbor_passive,
       neighbor_passive_cmd,
       NEIGHBOR_CMD2 "passive",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Don't send open messages to this neighbor\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_PASSIVE);
}

DEFUN (no_neighbor_passive,
       no_neighbor_passive_cmd,
       NO_NEIGHBOR_CMD2 "passive",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Don't send open messages to this neighbor\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_PASSIVE);
}

/* neighbor shutdown. */
DEFUN (neighbor_shutdown,
       neighbor_shutdown_cmd,
       NEIGHBOR_CMD2 "shutdown",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Administratively shut down this neighbor\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_SHUTDOWN);
}

DEFUN (no_neighbor_shutdown,
       no_neighbor_shutdown_cmd,
       NO_NEIGHBOR_CMD2 "shutdown",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Administratively shut down this neighbor\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_SHUTDOWN);
}

/* Deprecated neighbor capability route-refresh. */
DEFUN_DEPRECATED (neighbor_capability_route_refresh,
		  neighbor_capability_route_refresh_cmd,
		  NEIGHBOR_CMD2 "capability route-refresh",
		  NEIGHBOR_STR
		  NEIGHBOR_ADDR_STR2
		  "Advertise capability to the peer\n"
		  "Advertise route-refresh capability to this neighbor\n")
{
  return CMD_SUCCESS;
}

DEFUN_DEPRECATED (no_neighbor_capability_route_refresh,
		  no_neighbor_capability_route_refresh_cmd,
		  NO_NEIGHBOR_CMD2 "capability route-refresh",
		  NO_STR
		  NEIGHBOR_STR
		  NEIGHBOR_ADDR_STR2
		  "Advertise capability to the peer\n"
		  "Advertise route-refresh capability to this neighbor\n")
{
  return CMD_SUCCESS;
}

/* neighbor capability dynamic. */
DEFUN (neighbor_capability_dynamic,
       neighbor_capability_dynamic_cmd,
       NEIGHBOR_CMD2 "capability dynamic",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Advertise capability to the peer\n"
       "Advertise dynamic capability to this neighbor\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_DYNAMIC_CAPABILITY);
}

DEFUN (no_neighbor_capability_dynamic,
       no_neighbor_capability_dynamic_cmd,
       NO_NEIGHBOR_CMD2 "capability dynamic",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Advertise capability to the peer\n"
       "Advertise dynamic capability to this neighbor\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_DYNAMIC_CAPABILITY);
}

/* neighbor dont-capability-negotiate */
DEFUN (neighbor_dont_capability_negotiate,
       neighbor_dont_capability_negotiate_cmd,
       NEIGHBOR_CMD2 "dont-capability-negotiate",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Do not perform capability negotiation\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_DONT_CAPABILITY);
}

DEFUN (no_neighbor_dont_capability_negotiate,
       no_neighbor_dont_capability_negotiate_cmd,
       NO_NEIGHBOR_CMD2 "dont-capability-negotiate",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Do not perform capability negotiation\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_DONT_CAPABILITY);
}

static int
peer_af_flag_modify_vty (struct vty *vty, const char *peer_str, afi_t afi,
			 safi_t safi, u_int32_t flag, int set)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, peer_str);
  if (! peer)
    return CMD_WARNING;

  if (set)
    ret = peer_af_flag_set (peer, afi, safi, flag);
  else
    ret = peer_af_flag_unset (peer, afi, safi, flag);

  return bgp_vty_return (vty, ret);
}

static int
peer_af_flag_set_vty (struct vty *vty, const char *peer_str, afi_t afi,
		      safi_t safi, u_int32_t flag)
{
  return peer_af_flag_modify_vty (vty, peer_str, afi, safi, flag, 1);
}

static int
peer_af_flag_unset_vty (struct vty *vty, const char *peer_str, afi_t afi,
			safi_t safi, u_int32_t flag)
{
  return peer_af_flag_modify_vty (vty, peer_str, afi, safi, flag, 0);
}

/* neighbor capability orf prefix-list. */
DEFUN (neighbor_capability_orf_prefix,
       neighbor_capability_orf_prefix_cmd,
       NEIGHBOR_CMD2 "capability orf prefix-list (both|send|receive)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Advertise capability to the peer\n"
       "Advertise ORF capability to the peer\n"
       "Advertise prefixlist ORF capability to this neighbor\n"
       "Capability to SEND and RECEIVE the ORF to/from this neighbor\n"
       "Capability to RECEIVE the ORF from this neighbor\n"
       "Capability to SEND the ORF to this neighbor\n")
{
  u_int16_t flag = 0;

  if (strncmp (argv[1], "s", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_SM;
  else if (strncmp (argv[1], "r", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_RM;
  else if (strncmp (argv[1], "b", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_SM|PEER_FLAG_ORF_PREFIX_RM;
  else
    return CMD_WARNING;

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flag);
}

DEFUN (no_neighbor_capability_orf_prefix,
       no_neighbor_capability_orf_prefix_cmd,
       NO_NEIGHBOR_CMD2 "capability orf prefix-list (both|send|receive)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Advertise capability to the peer\n"
       "Advertise ORF capability to the peer\n"
       "Advertise prefixlist ORF capability to this neighbor\n"
       "Capability to SEND and RECEIVE the ORF to/from this neighbor\n"
       "Capability to RECEIVE the ORF from this neighbor\n"
       "Capability to SEND the ORF to this neighbor\n")
{
  u_int16_t flag = 0;

  if (strncmp (argv[1], "s", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_SM;
  else if (strncmp (argv[1], "r", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_RM;
  else if (strncmp (argv[1], "b", 1) == 0)
    flag = PEER_FLAG_ORF_PREFIX_SM|PEER_FLAG_ORF_PREFIX_RM;
  else
    return CMD_WARNING;

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty), flag);
}

/* neighbor next-hop-self. */
DEFUN (neighbor_nexthop_self,
       neighbor_nexthop_self_cmd,
       NEIGHBOR_CMD2 "next-hop-self {all}",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Disable the next hop calculation for this neighbor\n"
       "Apply also to ibgp-learned routes when acting as a route reflector\n")
{
  u_int32_t flags = PEER_FLAG_NEXTHOP_SELF, unset = 0;
  int rc;

  /* Check if "all" is specified */
  if (argv[1] != NULL)
    flags |= PEER_FLAG_NEXTHOP_SELF_ALL;
  else
    unset |= PEER_FLAG_NEXTHOP_SELF_ALL;

  rc = peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			     bgp_node_safi (vty), flags);
  if ( rc == CMD_SUCCESS && unset )
    rc = peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty), unset);
  return rc;
}

DEFUN (no_neighbor_nexthop_self,
       no_neighbor_nexthop_self_cmd,
       NO_NEIGHBOR_CMD2 "next-hop-self {all}",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Disable the next hop calculation for this neighbor\n"
       "Apply also to ibgp-learned routes when acting as a route reflector\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_NEXTHOP_SELF|PEER_FLAG_NEXTHOP_SELF_ALL);
}

/* neighbor remove-private-AS. */
DEFUN (neighbor_remove_private_as,
       neighbor_remove_private_as_cmd,
       NEIGHBOR_CMD2 "remove-private-AS",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Remove private AS number from outbound updates\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       PEER_FLAG_REMOVE_PRIVATE_AS);
}

DEFUN (no_neighbor_remove_private_as,
       no_neighbor_remove_private_as_cmd,
       NO_NEIGHBOR_CMD2 "remove-private-AS",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Remove private AS number from outbound updates\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_REMOVE_PRIVATE_AS);
}

/* neighbor send-community. */
DEFUN (neighbor_send_community,
       neighbor_send_community_cmd,
       NEIGHBOR_CMD2 "send-community",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Send Community attribute to this neighbor\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       PEER_FLAG_SEND_COMMUNITY);
}

DEFUN (no_neighbor_send_community,
       no_neighbor_send_community_cmd,
       NO_NEIGHBOR_CMD2 "send-community",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Send Community attribute to this neighbor\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_SEND_COMMUNITY);
}

/* neighbor send-community extended. */
DEFUN (neighbor_send_community_type,
       neighbor_send_community_type_cmd,
       NEIGHBOR_CMD2 "send-community (both|extended|standard)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Send Community attribute to this neighbor\n"
       "Send Standard and Extended Community attributes\n"
       "Send Extended Community attributes\n"
       "Send Standard Community attributes\n")
{
  if (strncmp (argv[1], "s", 1) == 0)
    return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_SEND_COMMUNITY);
  if (strncmp (argv[1], "e", 1) == 0)
    return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_SEND_EXT_COMMUNITY);

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       (PEER_FLAG_SEND_COMMUNITY|
				PEER_FLAG_SEND_EXT_COMMUNITY));
}

DEFUN (no_neighbor_send_community_type,
       no_neighbor_send_community_type_cmd,
       NO_NEIGHBOR_CMD2 "send-community (both|extended|standard)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Send Community attribute to this neighbor\n"
       "Send Standard and Extended Community attributes\n"
       "Send Extended Community attributes\n"
       "Send Standard Community attributes\n")
{
  if (strncmp (argv[1], "s", 1) == 0)
    return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				   bgp_node_safi (vty),
				   PEER_FLAG_SEND_COMMUNITY);
  if (strncmp (argv[1], "e", 1) == 0)
    return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				   bgp_node_safi (vty),
				   PEER_FLAG_SEND_EXT_COMMUNITY);

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 (PEER_FLAG_SEND_COMMUNITY |
				  PEER_FLAG_SEND_EXT_COMMUNITY));
}

/* neighbor soft-reconfig. */
DEFUN (neighbor_soft_reconfiguration,
       neighbor_soft_reconfiguration_cmd,
       NEIGHBOR_CMD2 "soft-reconfiguration inbound",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Per neighbor soft reconfiguration\n"
       "Allow inbound soft reconfiguration for this neighbor\n")
{
  return peer_af_flag_set_vty (vty, argv[0],
			       bgp_node_afi (vty), bgp_node_safi (vty),
			       PEER_FLAG_SOFT_RECONFIG);
}

DEFUN (no_neighbor_soft_reconfiguration,
       no_neighbor_soft_reconfiguration_cmd,
       NO_NEIGHBOR_CMD2 "soft-reconfiguration inbound",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Per neighbor soft reconfiguration\n"
       "Allow inbound soft reconfiguration for this neighbor\n")
{
  return peer_af_flag_unset_vty (vty, argv[0],
				 bgp_node_afi (vty), bgp_node_safi (vty),
				 PEER_FLAG_SOFT_RECONFIG);
}

DEFUN (neighbor_route_reflector_client,
       neighbor_route_reflector_client_cmd,
       NEIGHBOR_CMD2 "route-reflector-client",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure a neighbor as Route Reflector client\n")
{
  struct peer *peer;


  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       PEER_FLAG_REFLECTOR_CLIENT);
}

DEFUN (no_neighbor_route_reflector_client,
       no_neighbor_route_reflector_client_cmd,
       NO_NEIGHBOR_CMD2 "route-reflector-client",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure a neighbor as Route Reflector client\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 PEER_FLAG_REFLECTOR_CLIENT);
}

static int
peer_rsclient_set_vty (struct vty *vty, const char *peer_str, 
                       int afi, int safi)
{
  int ret;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct listnode *node, *nnode;
  struct bgp_filter *pfilter;
  struct bgp_filter *gfilter;
  int locked_and_added = 0;

  bgp = vty->index;

  peer = peer_and_group_lookup_vty (vty, peer_str);
  if ( ! peer )
    return CMD_WARNING;

  /* If it is already a RS-Client, don't do anything. */
  if ( CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT) )
    return CMD_SUCCESS;

  if ( ! peer_rsclient_active (peer) )
    {
      peer = peer_lock (peer); /* rsclient peer list reference */
      listnode_add_sort (bgp->rsclient, peer);
      locked_and_added = 1;
    }

  ret = peer_af_flag_set (peer, afi, safi, PEER_FLAG_RSERVER_CLIENT);
  if (ret < 0)
    {
      if (locked_and_added)
        {
          listnode_delete (bgp->rsclient, peer);
          peer_unlock (peer); /* rsclient peer list reference */
        }

      return bgp_vty_return (vty, ret);
    }

  peer->rib[afi][safi] = bgp_table_init (afi, safi);
  peer->rib[afi][safi]->type = BGP_TABLE_RSCLIENT;
  /* RIB peer reference.  Released when table is free'd in bgp_table_free. */
  peer->rib[afi][safi]->owner = peer_lock (peer);

  /* Check for existing 'network' and 'redistribute' routes. */
  bgp_check_local_routes_rsclient (peer, afi, safi);

  /* Check for routes for peers configured with 'soft-reconfiguration'. */
  bgp_soft_reconfig_rsclient (peer, afi, safi);

  if (CHECK_FLAG(peer->sflags, PEER_STATUS_GROUP))
    {
      group = peer->group;
      gfilter = &peer->filter[afi][safi];

      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
        {
          pfilter = &peer->filter[afi][safi];

          /* Members of a non-RS-Client group should not be RS-Clients, as that 
             is checked when the become part of the peer-group */
          ret = peer_af_flag_set (peer, afi, safi, PEER_FLAG_RSERVER_CLIENT);
          if (ret < 0)
            return bgp_vty_return (vty, ret);

          /* Make peer's RIB point to group's RIB. */
          peer->rib[afi][safi] = group->conf->rib[afi][safi];

          /* Import policy. */
          if (pfilter->map[RMAP_IMPORT].name)
            free (pfilter->map[RMAP_IMPORT].name);
          if (gfilter->map[RMAP_IMPORT].name)
            {
              pfilter->map[RMAP_IMPORT].name = strdup (gfilter->map[RMAP_IMPORT].name);
              pfilter->map[RMAP_IMPORT].map = gfilter->map[RMAP_IMPORT].map;
            }
          else
            {
              pfilter->map[RMAP_IMPORT].name = NULL;
              pfilter->map[RMAP_IMPORT].map =NULL;
            }

          /* Export policy. */
          if (gfilter->map[RMAP_EXPORT].name && ! pfilter->map[RMAP_EXPORT].name)
            {
              pfilter->map[RMAP_EXPORT].name = strdup (gfilter->map[RMAP_EXPORT].name);
              pfilter->map[RMAP_EXPORT].map = gfilter->map[RMAP_EXPORT].map;
            }
        }
    }
  return CMD_SUCCESS;
}

static int
peer_rsclient_unset_vty (struct vty *vty, const char *peer_str, 
                         int afi, int safi)
{
  int ret;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct listnode *node, *nnode;

  bgp = vty->index;

  peer = peer_and_group_lookup_vty (vty, peer_str);
  if ( ! peer )
    return CMD_WARNING;

  /* If it is not a RS-Client, don't do anything. */
  if ( ! CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT) )
    return CMD_SUCCESS;

  if (CHECK_FLAG(peer->sflags, PEER_STATUS_GROUP))
    {
      group = peer->group;

      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
        {
          ret = peer_af_flag_unset (peer, afi, safi, PEER_FLAG_RSERVER_CLIENT);
          if (ret < 0)
            return bgp_vty_return (vty, ret);

          peer->rib[afi][safi] = NULL;
        }

        peer = group->conf;
    }

  ret = peer_af_flag_unset (peer, afi, safi, PEER_FLAG_RSERVER_CLIENT);
  if (ret < 0)
    return bgp_vty_return (vty, ret);

  if ( ! peer_rsclient_active (peer) )
    {
      bgp_clear_route (peer, afi, safi, BGP_CLEAR_ROUTE_MY_RSCLIENT);
      listnode_delete (bgp->rsclient, peer);
      peer_unlock (peer); /* peer bgp rsclient reference */
    }

  bgp_table_finish (&peer->rib[bgp_node_afi(vty)][bgp_node_safi(vty)]);

  return CMD_SUCCESS;
}

/* neighbor route-server-client. */
DEFUN (neighbor_route_server_client,
       neighbor_route_server_client_cmd,
       NEIGHBOR_CMD2 "route-server-client",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure a neighbor as Route Server client\n")
{
  return peer_rsclient_set_vty (vty, argv[0], bgp_node_afi(vty),
                  bgp_node_safi(vty));
}

DEFUN (no_neighbor_route_server_client,
       no_neighbor_route_server_client_cmd,
       NO_NEIGHBOR_CMD2 "route-server-client",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure a neighbor as Route Server client\n")
{
  return peer_rsclient_unset_vty (vty, argv[0], bgp_node_afi(vty),
                  bgp_node_safi(vty));
}

DEFUN (neighbor_nexthop_local_unchanged,
       neighbor_nexthop_local_unchanged_cmd,
       NEIGHBOR_CMD2 "nexthop-local unchanged",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure treatment of outgoing link-local nexthop attribute\n"
       "Leave link-local nexthop unchanged for this peer\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
                                bgp_node_safi (vty),
                                PEER_FLAG_NEXTHOP_LOCAL_UNCHANGED );
}

DEFUN (no_neighbor_nexthop_local_unchanged,
       no_neighbor_nexthop_local_unchanged_cmd,
       NO_NEIGHBOR_CMD2 "nexthop-local unchanged",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Configure treatment of outgoing link-local-nexthop attribute\n"
       "Leave link-local nexthop unchanged for this peer\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
                                PEER_FLAG_NEXTHOP_LOCAL_UNCHANGED );
}

DEFUN (neighbor_attr_unchanged,
       neighbor_attr_unchanged_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       (PEER_FLAG_AS_PATH_UNCHANGED |
				PEER_FLAG_NEXTHOP_UNCHANGED |
				PEER_FLAG_MED_UNCHANGED));
}

DEFUN (neighbor_attr_unchanged1,
       neighbor_attr_unchanged1_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged (as-path|next-hop|med)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = 0;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);
}

DEFUN (neighbor_attr_unchanged2,
       neighbor_attr_unchanged2_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged as-path (next-hop|med)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = PEER_FLAG_AS_PATH_UNCHANGED;

  if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);

}

DEFUN (neighbor_attr_unchanged3,
       neighbor_attr_unchanged3_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged next-hop (as-path|med)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "As-path attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = PEER_FLAG_NEXTHOP_UNCHANGED;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);
}

DEFUN (neighbor_attr_unchanged4,
       neighbor_attr_unchanged4_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged med (as-path|next-hop)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "As-path attribute\n"
       "Nexthop attribute\n")
{
  u_int16_t flags = PEER_FLAG_MED_UNCHANGED;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);

  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);
}

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged5_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged as-path next-hop med",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged6_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged as-path med next-hop",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Med attribute\n"
       "Nexthop attribute\n")

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged7_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged next-hop med as-path",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "Med attribute\n"
       "As-path attribute\n")

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged8_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged next-hop as-path med",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "As-path attribute\n"
       "Med attribute\n")

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged9_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged med next-hop as-path",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "Nexthop attribute\n"
       "As-path attribute\n")

ALIAS (neighbor_attr_unchanged,
       neighbor_attr_unchanged10_cmd,
       NEIGHBOR_CMD2 "attribute-unchanged med as-path next-hop",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "As-path attribute\n"
       "Nexthop attribute\n")

DEFUN (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged",
       NO_STR	 
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n")
{
  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty),
				 (PEER_FLAG_AS_PATH_UNCHANGED |
				  PEER_FLAG_NEXTHOP_UNCHANGED |
				  PEER_FLAG_MED_UNCHANGED));
}

DEFUN (no_neighbor_attr_unchanged1,
       no_neighbor_attr_unchanged1_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged (as-path|next-hop|med)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = 0;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty), flags);
}

DEFUN (no_neighbor_attr_unchanged2,
       no_neighbor_attr_unchanged2_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged as-path (next-hop|med)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = PEER_FLAG_AS_PATH_UNCHANGED;

  if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);
}

DEFUN (no_neighbor_attr_unchanged3,
       no_neighbor_attr_unchanged3_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged next-hop (as-path|med)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "As-path attribute\n"
       "Med attribute\n")
{
  u_int16_t flags = PEER_FLAG_NEXTHOP_UNCHANGED;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "med", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_MED_UNCHANGED);

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty), flags);
}

DEFUN (no_neighbor_attr_unchanged4,
       no_neighbor_attr_unchanged4_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged med (as-path|next-hop)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "As-path attribute\n"
       "Nexthop attribute\n")
{
  u_int16_t flags = PEER_FLAG_MED_UNCHANGED;

  if (strncmp (argv[1], "as-path", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_AS_PATH_UNCHANGED);
  else if (strncmp (argv[1], "next-hop", 1) == 0)
    SET_FLAG (flags, PEER_FLAG_NEXTHOP_UNCHANGED);

  return peer_af_flag_unset_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty), flags);
}

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged5_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged as-path next-hop med",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Nexthop attribute\n"
       "Med attribute\n")

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged6_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged as-path med next-hop",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "As-path attribute\n"
       "Med attribute\n"
       "Nexthop attribute\n")

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged7_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged next-hop med as-path",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "Med attribute\n"
       "As-path attribute\n")

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged8_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged next-hop as-path med",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Nexthop attribute\n"
       "As-path attribute\n"
       "Med attribute\n")

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged9_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged med next-hop as-path",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "Nexthop attribute\n"
       "As-path attribute\n")

ALIAS (no_neighbor_attr_unchanged,
       no_neighbor_attr_unchanged10_cmd,
       NO_NEIGHBOR_CMD2 "attribute-unchanged med as-path next-hop",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP attribute is propagated unchanged to this neighbor\n"
       "Med attribute\n"
       "As-path attribute\n"
       "Nexthop attribute\n")

/* For old version Zebra compatibility.  */
DEFUN_DEPRECATED (neighbor_transparent_as,
		  neighbor_transparent_as_cmd,
		  NEIGHBOR_CMD "transparent-as",
		  NEIGHBOR_STR
		  NEIGHBOR_ADDR_STR
		  "Do not append my AS number even peer is EBGP peer\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       PEER_FLAG_AS_PATH_UNCHANGED);
}

DEFUN_DEPRECATED (neighbor_transparent_nexthop,
		  neighbor_transparent_nexthop_cmd,
		  NEIGHBOR_CMD "transparent-nexthop",
		  NEIGHBOR_STR
		  NEIGHBOR_ADDR_STR
		  "Do not change nexthop even peer is EBGP peer\n")
{
  return peer_af_flag_set_vty (vty, argv[0], bgp_node_afi (vty),
			       bgp_node_safi (vty),
			       PEER_FLAG_NEXTHOP_UNCHANGED);
}

/* EBGP multihop configuration. */
static int
peer_ebgp_multihop_set_vty (struct vty *vty, const char *ip_str, 
                            const char *ttl_str)
{
  struct peer *peer;
  unsigned int ttl;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  if (! ttl_str)
    ttl = TTL_MAX;
  else
    VTY_GET_INTEGER_RANGE ("TTL", ttl, ttl_str, 1, 255);

  return bgp_vty_return (vty,  peer_ebgp_multihop_set (peer, ttl));
}

static int
peer_ebgp_multihop_unset_vty (struct vty *vty, const char *ip_str) 
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  return bgp_vty_return (vty, peer_ebgp_multihop_unset (peer));
}

/* neighbor ebgp-multihop. */
DEFUN (neighbor_ebgp_multihop,
       neighbor_ebgp_multihop_cmd,
       NEIGHBOR_CMD2 "ebgp-multihop",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Allow EBGP neighbors not on directly connected networks\n")
{
  return peer_ebgp_multihop_set_vty (vty, argv[0], NULL);
}

DEFUN (neighbor_ebgp_multihop_ttl,
       neighbor_ebgp_multihop_ttl_cmd,
       NEIGHBOR_CMD2 "ebgp-multihop <1-255>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Allow EBGP neighbors not on directly connected networks\n"
       "maximum hop count\n")
{
  return peer_ebgp_multihop_set_vty (vty, argv[0], argv[1]);
}

DEFUN (no_neighbor_ebgp_multihop,
       no_neighbor_ebgp_multihop_cmd,
       NO_NEIGHBOR_CMD2 "ebgp-multihop",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Allow EBGP neighbors not on directly connected networks\n")
{
  return peer_ebgp_multihop_unset_vty (vty, argv[0]);
}

ALIAS (no_neighbor_ebgp_multihop,
       no_neighbor_ebgp_multihop_ttl_cmd,
       NO_NEIGHBOR_CMD2 "ebgp-multihop <1-255>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Allow EBGP neighbors not on directly connected networks\n"
       "maximum hop count\n")

/* disable-connected-check */
DEFUN (neighbor_disable_connected_check,
       neighbor_disable_connected_check_cmd,
       NEIGHBOR_CMD2 "disable-connected-check",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "one-hop away EBGP peer using loopback address\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_DISABLE_CONNECTED_CHECK);
}

DEFUN (no_neighbor_disable_connected_check,
       no_neighbor_disable_connected_check_cmd,
       NO_NEIGHBOR_CMD2 "disable-connected-check",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "one-hop away EBGP peer using loopback address\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_DISABLE_CONNECTED_CHECK);
}

/* Enforce multihop.  */
ALIAS (neighbor_disable_connected_check,
       neighbor_enforce_multihop_cmd,
       NEIGHBOR_CMD2 "enforce-multihop",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Enforce EBGP neighbors perform multihop\n")

/* Enforce multihop.  */
ALIAS (no_neighbor_disable_connected_check,
       no_neighbor_enforce_multihop_cmd,
       NO_NEIGHBOR_CMD2 "enforce-multihop",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Enforce EBGP neighbors perform multihop\n")

DEFUN (neighbor_description,
       neighbor_description_cmd,
       NEIGHBOR_CMD2 "description .LINE",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Neighbor specific description\n"
       "Up to 80 characters describing this neighbor\n")
{
  struct peer *peer;
  char *str;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  if (argc == 1)
    return CMD_SUCCESS;

  str = argv_concat(argv, argc, 1);

  peer_description_set (peer, str);

  XFREE (MTYPE_TMP, str);

  return CMD_SUCCESS;
}

DEFUN (no_neighbor_description,
       no_neighbor_description_cmd,
       NO_NEIGHBOR_CMD2 "description",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Neighbor specific description\n")
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  peer_description_unset (peer);

  return CMD_SUCCESS;
}

ALIAS (no_neighbor_description,
       no_neighbor_description_val_cmd,
       NO_NEIGHBOR_CMD2 "description .LINE",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Neighbor specific description\n"
       "Up to 80 characters describing this neighbor\n")

/* Neighbor update-source. */
static int
peer_update_source_vty (struct vty *vty, const char *peer_str, 
                        const char *source_str)
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, peer_str);
  if (! peer)
    return CMD_WARNING;

  if (source_str)
    {
      union sockunion su;
      int ret = str2sockunion (source_str, &su);

      if (ret == 0)
	peer_update_source_addr_set (peer, &su);
      else
	peer_update_source_if_set (peer, source_str);
    }
  else
    peer_update_source_unset (peer);

  return CMD_SUCCESS;
}

#define BGP_UPDATE_SOURCE_STR "(A.B.C.D|X:X::X:X|WORD)"
#define BGP_UPDATE_SOURCE_HELP_STR \
  "IPv4 address\n" \
  "IPv6 address\n" \
  "Interface name (requires zebra to be running)\n"

DEFUN (neighbor_update_source,
       neighbor_update_source_cmd,
       NEIGHBOR_CMD2 "update-source " BGP_UPDATE_SOURCE_STR,
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Source of routing updates\n"
       BGP_UPDATE_SOURCE_HELP_STR)
{
  return peer_update_source_vty (vty, argv[0], argv[1]);
}

DEFUN (no_neighbor_update_source,
       no_neighbor_update_source_cmd,
       NO_NEIGHBOR_CMD2 "update-source",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Source of routing updates\n")
{
  return peer_update_source_vty (vty, argv[0], NULL);
}

static int
peer_default_originate_set_vty (struct vty *vty, const char *peer_str, 
                                afi_t afi, safi_t safi, 
                                const char *rmap, int set)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, peer_str);
  if (! peer)
    return CMD_WARNING;

  if (set)
    ret = peer_default_originate_set (peer, afi, safi, rmap);
  else
    ret = peer_default_originate_unset (peer, afi, safi);

  return bgp_vty_return (vty, ret);
}

/* neighbor default-originate. */
DEFUN (neighbor_default_originate,
       neighbor_default_originate_cmd,
       NEIGHBOR_CMD2 "default-originate",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Originate default route to this neighbor\n")
{
  return peer_default_originate_set_vty (vty, argv[0], bgp_node_afi (vty),
					 bgp_node_safi (vty), NULL, 1);
}

DEFUN (neighbor_default_originate_rmap,
       neighbor_default_originate_rmap_cmd,
       NEIGHBOR_CMD2 "default-originate route-map WORD",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Originate default route to this neighbor\n"
       "Route-map to specify criteria to originate default\n"
       "route-map name\n")
{
  return peer_default_originate_set_vty (vty, argv[0], bgp_node_afi (vty),
					 bgp_node_safi (vty), argv[1], 1);
}

DEFUN (no_neighbor_default_originate,
       no_neighbor_default_originate_cmd,
       NO_NEIGHBOR_CMD2 "default-originate",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Originate default route to this neighbor\n")
{
  return peer_default_originate_set_vty (vty, argv[0], bgp_node_afi (vty),
					 bgp_node_safi (vty), NULL, 0);
}

ALIAS (no_neighbor_default_originate,
       no_neighbor_default_originate_rmap_cmd,
       NO_NEIGHBOR_CMD2 "default-originate route-map WORD",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Originate default route to this neighbor\n"
       "Route-map to specify criteria to originate default\n"
       "route-map name\n")

/* Set neighbor's BGP port.  */
static int
peer_port_vty (struct vty *vty, const char *ip_str, int afi, 
               const char *port_str)
{
  struct peer *peer;
  u_int16_t port;
  struct servent *sp;

  peer = peer_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  if (! port_str)
    { 
      sp = getservbyname ("bgp", "tcp");
      port = (sp == NULL) ? BGP_PORT_DEFAULT : ntohs (sp->s_port);
    }
  else
    {
      VTY_GET_INTEGER("port", port, port_str);
    }

  peer_port_set (peer, port);

  return CMD_SUCCESS;
}

/* Set specified peer's BGP port.  */
DEFUN (neighbor_port,
       neighbor_port_cmd,
       NEIGHBOR_CMD "port <0-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Neighbor's BGP port\n"
       "TCP port number\n")
{
  return peer_port_vty (vty, argv[0], AFI_IP, argv[1]);
}

DEFUN (no_neighbor_port,
       no_neighbor_port_cmd,
       NO_NEIGHBOR_CMD "port",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Neighbor's BGP port\n")
{
  return peer_port_vty (vty, argv[0], AFI_IP, NULL);
}

ALIAS (no_neighbor_port,
       no_neighbor_port_val_cmd,
       NO_NEIGHBOR_CMD "port <0-65535>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Neighbor's BGP port\n"
       "TCP port number\n")

/* neighbor weight. */
static int
peer_weight_set_vty (struct vty *vty, const char *ip_str, 
                     const char *weight_str)
{
  struct peer *peer;
  unsigned long weight;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  VTY_GET_INTEGER_RANGE("weight", weight, weight_str, 0, 65535);

  return bgp_vty_return (vty, peer_weight_set (peer, weight));
}

static int
peer_weight_unset_vty (struct vty *vty, const char *ip_str)
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  return bgp_vty_return (vty, peer_weight_unset (peer));
}

DEFUN (neighbor_weight,
       neighbor_weight_cmd,
       NEIGHBOR_CMD2 "weight <0-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Set default weight for routes from this neighbor\n"
       "default weight\n")
{
  return peer_weight_set_vty (vty, argv[0], argv[1]);
}

DEFUN (no_neighbor_weight,
       no_neighbor_weight_cmd,
       NO_NEIGHBOR_CMD2 "weight",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Set default weight for routes from this neighbor\n")
{
  return peer_weight_unset_vty (vty, argv[0]);
}

ALIAS (no_neighbor_weight,
       no_neighbor_weight_val_cmd,
       NO_NEIGHBOR_CMD2 "weight <0-65535>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Set default weight for routes from this neighbor\n"
       "default weight\n")

/* Override capability negotiation. */
DEFUN (neighbor_override_capability,
       neighbor_override_capability_cmd,
       NEIGHBOR_CMD2 "override-capability",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Override capability negotiation result\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_OVERRIDE_CAPABILITY);
}

DEFUN (no_neighbor_override_capability,
       no_neighbor_override_capability_cmd,
       NO_NEIGHBOR_CMD2 "override-capability",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Override capability negotiation result\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_OVERRIDE_CAPABILITY);
}

DEFUN (neighbor_strict_capability,
       neighbor_strict_capability_cmd,
       NEIGHBOR_CMD "strict-capability-match",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Strict capability negotiation match\n")
{
  return peer_flag_set_vty (vty, argv[0], PEER_FLAG_STRICT_CAP_MATCH);
}

DEFUN (no_neighbor_strict_capability,
       no_neighbor_strict_capability_cmd,
       NO_NEIGHBOR_CMD "strict-capability-match",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Strict capability negotiation match\n")
{
  return peer_flag_unset_vty (vty, argv[0], PEER_FLAG_STRICT_CAP_MATCH);
}

static int
peer_timers_set_vty (struct vty *vty, const char *ip_str, 
                     const char *keep_str, const char *hold_str)
{
  int ret;
  struct peer *peer;
  u_int32_t keepalive;
  u_int32_t holdtime;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  VTY_GET_INTEGER_RANGE ("Keepalive", keepalive, keep_str, 0, 65535);
  VTY_GET_INTEGER_RANGE ("Holdtime", holdtime, hold_str, 0, 65535);

  ret = peer_timers_set (peer, keepalive, holdtime);

  return bgp_vty_return (vty, ret);
}

static int
peer_timers_unset_vty (struct vty *vty, const char *ip_str)
{
  int ret;
  struct peer *peer;

  peer = peer_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  ret = peer_timers_unset (peer);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_timers,
       neighbor_timers_cmd,
       NEIGHBOR_CMD2 "timers <0-65535> <0-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP per neighbor timers\n"
       "Keepalive interval\n"
       "Holdtime\n")
{
  return peer_timers_set_vty (vty, argv[0], argv[1], argv[2]);
}

DEFUN (no_neighbor_timers,
       no_neighbor_timers_cmd,
       NO_NEIGHBOR_CMD2 "timers",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "BGP per neighbor timers\n")
{
  return peer_timers_unset_vty (vty, argv[0]);
}

static int
peer_timers_connect_set_vty (struct vty *vty, const char *ip_str, 
                             const char *time_str)
{
  struct peer *peer;
  u_int32_t connect;

  peer = peer_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  VTY_GET_INTEGER_RANGE ("Connect time", connect, time_str, 0, 65535);

  return bgp_vty_return (vty, peer_timers_connect_set (peer, connect));
}

static int
peer_timers_connect_unset_vty (struct vty *vty, const char *ip_str)
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  return bgp_vty_return (vty, peer_timers_connect_unset (peer));
}

DEFUN (neighbor_timers_connect,
       neighbor_timers_connect_cmd,
       NEIGHBOR_CMD "timers connect <0-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "BGP per neighbor timers\n"
       "BGP connect timer\n"
       "Connect timer\n")
{
  return peer_timers_connect_set_vty (vty, argv[0], argv[1]);
}

DEFUN (no_neighbor_timers_connect,
       no_neighbor_timers_connect_cmd,
       NO_NEIGHBOR_CMD "timers connect",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "BGP per neighbor timers\n"
       "BGP connect timer\n")
{
  return peer_timers_connect_unset_vty (vty, argv[0]);
}

ALIAS (no_neighbor_timers_connect,
       no_neighbor_timers_connect_val_cmd,
       NO_NEIGHBOR_CMD "timers connect <0-65535>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "BGP per neighbor timers\n"
       "BGP connect timer\n"
       "Connect timer\n")

static int
peer_advertise_interval_vty (struct vty *vty, const char *ip_str, 
                             const char *time_str, int set)  
{
  int ret;
  struct peer *peer;
  u_int32_t routeadv = 0;

  peer = peer_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  if (time_str)
    VTY_GET_INTEGER_RANGE ("advertise interval", routeadv, time_str, 0, 600);

  if (set)
    ret = peer_advertise_interval_set (peer, routeadv);
  else
    ret = peer_advertise_interval_unset (peer);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_advertise_interval,
       neighbor_advertise_interval_cmd,
       NEIGHBOR_CMD "advertisement-interval <0-600>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Minimum interval between sending BGP routing updates\n"
       "time in seconds\n")
{
  return peer_advertise_interval_vty (vty, argv[0], argv[1], 1);
}

DEFUN (no_neighbor_advertise_interval,
       no_neighbor_advertise_interval_cmd,
       NO_NEIGHBOR_CMD "advertisement-interval",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Minimum interval between sending BGP routing updates\n")
{
  return peer_advertise_interval_vty (vty, argv[0], NULL, 0);
}

ALIAS (no_neighbor_advertise_interval,
       no_neighbor_advertise_interval_val_cmd,
       NO_NEIGHBOR_CMD "advertisement-interval <0-600>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Minimum interval between sending BGP routing updates\n"
       "time in seconds\n")

/* neighbor interface */
static int
peer_interface_vty (struct vty *vty, const char *ip_str, const char *str)
{
  int ret;
  struct peer *peer;

  peer = peer_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  if (str)
    ret = peer_interface_set (peer, str);
  else
    ret = peer_interface_unset (peer);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_interface,
       neighbor_interface_cmd,
       NEIGHBOR_CMD "interface WORD",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Interface\n"
       "Interface name\n")
{
  return peer_interface_vty (vty, argv[0], argv[1]);
}

DEFUN (no_neighbor_interface,
       no_neighbor_interface_cmd,
       NO_NEIGHBOR_CMD "interface WORD",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR
       "Interface\n"
       "Interface name\n")
{
  return peer_interface_vty (vty, argv[0], NULL);
}

/* Set distribute list to the peer. */
static int
peer_distribute_set_vty (struct vty *vty, const char *ip_str, 
                         afi_t afi, safi_t safi,
			 const char *name_str, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_distribute_set (peer, afi, safi, direct, name_str);

  return bgp_vty_return (vty, ret);
}

static int
peer_distribute_unset_vty (struct vty *vty, const char *ip_str, afi_t afi,
			   safi_t safi, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_distribute_unset (peer, afi, safi, direct);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_distribute_list,
       neighbor_distribute_list_cmd,
       NEIGHBOR_CMD2 "distribute-list (<1-199>|<1300-2699>|WORD) (in|out)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Filter updates to/from this neighbor\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n"
       "Filter incoming updates\n"
       "Filter outgoing updates\n")
{
  return peer_distribute_set_vty (vty, argv[0], bgp_node_afi (vty),
				  bgp_node_safi (vty), argv[1], argv[2]);
}

DEFUN (no_neighbor_distribute_list,
       no_neighbor_distribute_list_cmd,
       NO_NEIGHBOR_CMD2 "distribute-list (<1-199>|<1300-2699>|WORD) (in|out)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Filter updates to/from this neighbor\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n"
       "Filter incoming updates\n"
       "Filter outgoing updates\n")
{
  return peer_distribute_unset_vty (vty, argv[0], bgp_node_afi (vty),
				    bgp_node_safi (vty), argv[2]);
}

/* Set prefix list to the peer. */
static int
peer_prefix_list_set_vty (struct vty *vty, const char *ip_str, afi_t afi,
			  safi_t safi, const char *name_str, 
                          const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_prefix_list_set (peer, afi, safi, direct, name_str);

  return bgp_vty_return (vty, ret);
}

static int
peer_prefix_list_unset_vty (struct vty *vty, const char *ip_str, afi_t afi,
			    safi_t safi, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;
  
  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_prefix_list_unset (peer, afi, safi, direct);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_prefix_list,
       neighbor_prefix_list_cmd,
       NEIGHBOR_CMD2 "prefix-list WORD (in|out)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Filter updates to/from this neighbor\n"
       "Name of a prefix list\n"
       "Filter incoming updates\n"
       "Filter outgoing updates\n")
{
  return peer_prefix_list_set_vty (vty, argv[0], bgp_node_afi (vty),
				   bgp_node_safi (vty), argv[1], argv[2]);
}

DEFUN (no_neighbor_prefix_list,
       no_neighbor_prefix_list_cmd,
       NO_NEIGHBOR_CMD2 "prefix-list WORD (in|out)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Filter updates to/from this neighbor\n"
       "Name of a prefix list\n"
       "Filter incoming updates\n"
       "Filter outgoing updates\n")
{
  return peer_prefix_list_unset_vty (vty, argv[0], bgp_node_afi (vty),
				     bgp_node_safi (vty), argv[2]);
}

static int
peer_aslist_set_vty (struct vty *vty, const char *ip_str, 
                     afi_t afi, safi_t safi,
		     const char *name_str, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_aslist_set (peer, afi, safi, direct, name_str);

  return bgp_vty_return (vty, ret);
}

static int
peer_aslist_unset_vty (struct vty *vty, const char *ip_str, 
                       afi_t afi, safi_t safi,
		       const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = FILTER_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = FILTER_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = FILTER_OUT;

  ret = peer_aslist_unset (peer, afi, safi, direct);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_filter_list,
       neighbor_filter_list_cmd,
       NEIGHBOR_CMD2 "filter-list WORD (in|out)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Establish BGP filters\n"
       "AS path access-list name\n"
       "Filter incoming routes\n"
       "Filter outgoing routes\n")
{
  return peer_aslist_set_vty (vty, argv[0], bgp_node_afi (vty),
			      bgp_node_safi (vty), argv[1], argv[2]);
}

DEFUN (no_neighbor_filter_list,
       no_neighbor_filter_list_cmd,
       NO_NEIGHBOR_CMD2 "filter-list WORD (in|out)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Establish BGP filters\n"
       "AS path access-list name\n"
       "Filter incoming routes\n"
       "Filter outgoing routes\n")
{
  return peer_aslist_unset_vty (vty, argv[0], bgp_node_afi (vty),
				bgp_node_safi (vty), argv[2]);
}

/* Set route-map to the peer. */
static int
peer_route_map_set_vty (struct vty *vty, const char *ip_str, 
                        afi_t afi, safi_t safi,
			const char *name_str, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = RMAP_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "in", 2) == 0)
    direct = RMAP_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RMAP_OUT;
  else if (strncmp (direct_str, "im", 2) == 0)
    direct = RMAP_IMPORT;
  else if (strncmp (direct_str, "e", 1) == 0)
    direct = RMAP_EXPORT;

  ret = peer_route_map_set (peer, afi, safi, direct, name_str);

  return bgp_vty_return (vty, ret);
}

static int
peer_route_map_unset_vty (struct vty *vty, const char *ip_str, afi_t afi,
			  safi_t safi, const char *direct_str)
{
  int ret;
  struct peer *peer;
  int direct = RMAP_IN;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  /* Check filter direction. */
  if (strncmp (direct_str, "in", 2) == 0)
    direct = RMAP_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RMAP_OUT;
  else if (strncmp (direct_str, "im", 2) == 0)
    direct = RMAP_IMPORT;
  else if (strncmp (direct_str, "e", 1) == 0)
    direct = RMAP_EXPORT;

  ret = peer_route_map_unset (peer, afi, safi, direct);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_route_map,
       neighbor_route_map_cmd,
       NEIGHBOR_CMD2 "route-map WORD (in|out|import|export)",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Apply route map to neighbor\n"
       "Name of route map\n"
       "Apply map to incoming routes\n"
       "Apply map to outbound routes\n"
       "Apply map to routes going into a Route-Server client's table\n"
       "Apply map to routes coming from a Route-Server client")
{
  return peer_route_map_set_vty (vty, argv[0], bgp_node_afi (vty),
				 bgp_node_safi (vty), argv[1], argv[2]);
}

DEFUN (no_neighbor_route_map,
       no_neighbor_route_map_cmd,
       NO_NEIGHBOR_CMD2 "route-map WORD (in|out|import|export)",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Apply route map to neighbor\n"
       "Name of route map\n"
       "Apply map to incoming routes\n"
       "Apply map to outbound routes\n"
       "Apply map to routes going into a Route-Server client's table\n"
       "Apply map to routes coming from a Route-Server client")
{
  return peer_route_map_unset_vty (vty, argv[0], bgp_node_afi (vty),
				   bgp_node_safi (vty), argv[2]);
}

/* Set unsuppress-map to the peer. */
static int
peer_unsuppress_map_set_vty (struct vty *vty, const char *ip_str, afi_t afi,
			     safi_t safi, const char *name_str)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  ret = peer_unsuppress_map_set (peer, afi, safi, name_str);

  return bgp_vty_return (vty, ret);
}

/* Unset route-map from the peer. */
static int
peer_unsuppress_map_unset_vty (struct vty *vty, const char *ip_str, afi_t afi,
			       safi_t safi)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  ret = peer_unsuppress_map_unset (peer, afi, safi);

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_unsuppress_map,
       neighbor_unsuppress_map_cmd,
       NEIGHBOR_CMD2 "unsuppress-map WORD",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Route-map to selectively unsuppress suppressed routes\n"
       "Name of route map\n")
{
  return peer_unsuppress_map_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1]);
}

DEFUN (no_neighbor_unsuppress_map,
       no_neighbor_unsuppress_map_cmd,
       NO_NEIGHBOR_CMD2 "unsuppress-map WORD",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Route-map to selectively unsuppress suppressed routes\n"
       "Name of route map\n")
{
  return peer_unsuppress_map_unset_vty (vty, argv[0], bgp_node_afi (vty),
					bgp_node_safi (vty));
}

static int
peer_maximum_prefix_set_vty (struct vty *vty, const char *ip_str, afi_t afi,
			     safi_t safi, const char *num_str,  
			     const char *threshold_str, int warning,
			     const char *restart_str)
{
  int ret;
  struct peer *peer;
  u_int32_t max;
  u_char threshold;
  u_int16_t restart;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  VTY_GET_INTEGER ("maximum number", max, num_str);
  if (threshold_str)
    threshold = atoi (threshold_str);
  else
    threshold = MAXIMUM_PREFIX_THRESHOLD_DEFAULT;

  if (restart_str)
    restart = atoi (restart_str);
  else
    restart = 0;

  ret = peer_maximum_prefix_set (peer, afi, safi, max, threshold, warning, restart);

  return bgp_vty_return (vty, ret);
}

static int
peer_maximum_prefix_unset_vty (struct vty *vty, const char *ip_str, afi_t afi,
			       safi_t safi)
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, ip_str);
  if (! peer)
    return CMD_WARNING;

  ret = peer_maximum_prefix_unset (peer, afi, safi);

  return bgp_vty_return (vty, ret);
}

/* Maximum number of prefix configuration.  prefix count is different
   for each peer configuration.  So this configuration can be set for
   each peer configuration. */
DEFUN (neighbor_maximum_prefix,
       neighbor_maximum_prefix_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], NULL, 0,
				      NULL);
}

DEFUN (neighbor_maximum_prefix_threshold,
       neighbor_maximum_prefix_threshold_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> <1-100>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], argv[2], 0,
				      NULL);
}

DEFUN (neighbor_maximum_prefix_warning,
       neighbor_maximum_prefix_warning_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> warning-only",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Only give warning message when limit is exceeded\n")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], NULL, 1,
				      NULL);
}

DEFUN (neighbor_maximum_prefix_threshold_warning,
       neighbor_maximum_prefix_threshold_warning_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> <1-100> warning-only",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n"
       "Only give warning message when limit is exceeded\n")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], argv[2], 1, NULL);
}

DEFUN (neighbor_maximum_prefix_restart,
       neighbor_maximum_prefix_restart_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> restart <1-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Restart bgp connection after limit is exceeded\n"
       "Restart interval in minutes")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], NULL, 0, argv[2]);
}

DEFUN (neighbor_maximum_prefix_threshold_restart,
       neighbor_maximum_prefix_threshold_restart_cmd,
       NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> <1-100> restart <1-65535>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n"
       "Restart bgp connection after limit is exceeded\n"
       "Restart interval in minutes")
{
  return peer_maximum_prefix_set_vty (vty, argv[0], bgp_node_afi (vty),
				      bgp_node_safi (vty), argv[1], argv[2], 0, argv[3]);
}

DEFUN (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n")
{
  return peer_maximum_prefix_unset_vty (vty, argv[0], bgp_node_afi (vty),
					bgp_node_safi (vty));
}
 
ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_val_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n")

ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_threshold_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> warning-only",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n")

ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_warning_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> warning-only",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Only give warning message when limit is exceeded\n")

ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_threshold_warning_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> <1-100> warning-only",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n"
       "Only give warning message when limit is exceeded\n")

ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_restart_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> restart <1-65535>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Restart bgp connection after limit is exceeded\n"
       "Restart interval in minutes")

ALIAS (no_neighbor_maximum_prefix,
       no_neighbor_maximum_prefix_threshold_restart_cmd,
       NO_NEIGHBOR_CMD2 "maximum-prefix <1-4294967295> <1-100> restart <1-65535>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Maximum number of prefix accept from this peer\n"
       "maximum no. of prefix limit\n"
       "Threshold value (%) at which to generate a warning msg\n"
       "Restart bgp connection after limit is exceeded\n"
       "Restart interval in minutes")

/* "neighbor allowas-in" */
DEFUN (neighbor_allowas_in,
       neighbor_allowas_in_cmd,
       NEIGHBOR_CMD2 "allowas-in",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Accept as-path with my AS present in it\n")
{
  int ret;
  struct peer *peer;
  unsigned int allow_num;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  if (argc == 1)
    allow_num = 3;
  else
    VTY_GET_INTEGER_RANGE ("AS number", allow_num, argv[1], 1, 10);

  ret = peer_allowas_in_set (peer, bgp_node_afi (vty), bgp_node_safi (vty),
			     allow_num);

  return bgp_vty_return (vty, ret);
}

ALIAS (neighbor_allowas_in,
       neighbor_allowas_in_arg_cmd,
       NEIGHBOR_CMD2 "allowas-in <1-10>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Accept as-path with my AS present in it\n"
       "Number of occurances of AS number\n")

DEFUN (no_neighbor_allowas_in,
       no_neighbor_allowas_in_cmd,
       NO_NEIGHBOR_CMD2 "allowas-in",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "allow local ASN appears in aspath attribute\n")
{
  int ret;
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  ret = peer_allowas_in_unset (peer, bgp_node_afi (vty), bgp_node_safi (vty));

  return bgp_vty_return (vty, ret);
}

DEFUN (neighbor_ttl_security,
       neighbor_ttl_security_cmd,
       NEIGHBOR_CMD2 "ttl-security hops <1-254>",
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify the maximum number of hops to the BGP peer\n")
{
  struct peer *peer;
  int gtsm_hops;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;
    
  VTY_GET_INTEGER_RANGE ("", gtsm_hops, argv[1], 1, 254);

  return bgp_vty_return (vty, peer_ttl_security_hops_set (peer, gtsm_hops));
}

DEFUN (no_neighbor_ttl_security,
       no_neighbor_ttl_security_cmd,
       NO_NEIGHBOR_CMD2 "ttl-security hops <1-254>",
       NO_STR
       NEIGHBOR_STR
       NEIGHBOR_ADDR_STR2
       "Specify the maximum number of hops to the BGP peer\n")
{
  struct peer *peer;

  peer = peer_and_group_lookup_vty (vty, argv[0]);
  if (! peer)
    return CMD_WARNING;

  return bgp_vty_return (vty, peer_ttl_security_hops_unset (peer));
}

/* Address family configuration.  */
DEFUN (address_family_ipv4,
       address_family_ipv4_cmd,
       "address-family ipv4",
       "Enter Address Family command mode\n"
       "Address family\n")
{
  vty->node = BGP_IPV4_NODE;
  return CMD_SUCCESS;
}

DEFUN (address_family_ipv4_safi,
       address_family_ipv4_safi_cmd,
       "address-family ipv4 (unicast|multicast)",
       "Enter Address Family command mode\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    vty->node = BGP_IPV4M_NODE;
  else
    vty->node = BGP_IPV4_NODE;

  return CMD_SUCCESS;
}

DEFUN (address_family_ipv6,
       address_family_ipv6_cmd,
       "address-family ipv6",
       "Enter Address Family command mode\n"
       "Address family\n")
{
  vty->node = BGP_IPV6_NODE;
  return CMD_SUCCESS;
}

DEFUN (address_family_ipv6_safi,
       address_family_ipv6_safi_cmd,
       "address-family ipv6 (unicast|multicast)",
       "Enter Address Family command mode\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    vty->node = BGP_IPV6M_NODE;
  else
    vty->node = BGP_IPV6_NODE;

  return CMD_SUCCESS;
}

DEFUN (address_family_vpnv4,
       address_family_vpnv4_cmd,
       "address-family vpnv4",
       "Enter Address Family command mode\n"
       "Address family\n")
{
  vty->node = BGP_VPNV4_NODE;
  return CMD_SUCCESS;
}

ALIAS (address_family_vpnv4,
       address_family_vpnv4_unicast_cmd,
       "address-family vpnv4 unicast",
       "Enter Address Family command mode\n"
       "Address family\n"
       "Address Family Modifier\n")

DEFUN (exit_address_family,
       exit_address_family_cmd,
       "exit-address-family",
       "Exit from Address Family configuration mode\n")
{
  if (vty->node == BGP_IPV4_NODE
      || vty->node == BGP_IPV4M_NODE
      || vty->node == BGP_VPNV4_NODE
      || vty->node == BGP_IPV6_NODE
      || vty->node == BGP_IPV6M_NODE)
    vty->node = BGP_NODE;
  return CMD_SUCCESS;
}

/* BGP clear sort. */
enum clear_sort
{
  clear_all,
  clear_peer,
  clear_group,
  clear_external,
  clear_as
};

static void
bgp_clear_vty_error (struct vty *vty, struct peer *peer, afi_t afi,
		     safi_t safi, int error)
{
  switch (error)
    {
    case BGP_ERR_AF_UNCONFIGURED:
      vty_out (vty,
	       "%%BGP: Enable %s %s address family for the neighbor %s%s",
	       afi == AFI_IP6 ? "IPv6" : safi == SAFI_MPLS_VPN ? "VPNv4" : "IPv4",
	       safi == SAFI_MULTICAST ? "Multicast" : "Unicast",
	       peer->host, VTY_NEWLINE);
      break;
    case BGP_ERR_SOFT_RECONFIG_UNCONFIGURED:
      vty_out (vty, "%%BGP: Inbound soft reconfig for %s not possible as it%s      has neither refresh capability, nor inbound soft reconfig%s", peer->host, VTY_NEWLINE, VTY_NEWLINE);
      break;
    default:
      break;
    }
}

/* `clear ip bgp' functions. */
static int
bgp_clear (struct vty *vty, struct bgp *bgp,  afi_t afi, safi_t safi,
           enum clear_sort sort,enum bgp_clear_type stype, const char *arg)
{
  int ret;
  struct peer *peer;
  struct listnode *node, *nnode;

  /* Clear all neighbors. */
  if (sort == clear_all)
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (stype == BGP_CLEAR_SOFT_NONE)
	    ret = peer_clear (peer);
	  else
	    ret = peer_clear_soft (peer, afi, safi, stype);

	  if (ret < 0)
	    bgp_clear_vty_error (vty, peer, afi, safi, ret);
	}
      return CMD_SUCCESS;
    }

  /* Clear specified neighbors. */
  if (sort == clear_peer)
    {
      union sockunion su;
      int ret;

      /* Make sockunion for lookup. */
      ret = str2sockunion (arg, &su);
      if (ret < 0)
	{
	  vty_out (vty, "Malformed address: %s%s", arg, VTY_NEWLINE);
	  return CMD_WARNING;
	}
      peer = peer_lookup (bgp, &su);
      if (! peer)
	{
	  vty_out (vty, "%%BGP: Unknown neighbor - \"%s\"%s", arg, VTY_NEWLINE);
	  return CMD_WARNING;
	}

      if (stype == BGP_CLEAR_SOFT_NONE)
	ret = peer_clear (peer);
      else
	ret = peer_clear_soft (peer, afi, safi, stype);

      if (ret < 0)
	bgp_clear_vty_error (vty, peer, afi, safi, ret);

      return CMD_SUCCESS;
    }

  /* Clear all peer-group members. */
  if (sort == clear_group)
    {
      struct peer_group *group;

      group = peer_group_lookup (bgp, arg);
      if (! group)
	{
	  vty_out (vty, "%%BGP: No such peer-group %s%s", arg, VTY_NEWLINE);
	  return CMD_WARNING; 
	}

      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  if (stype == BGP_CLEAR_SOFT_NONE)
	    {
	      ret = peer_clear (peer);
	      continue;
	    }

	  if (! peer->af_group[afi][safi])
	    continue;

	  ret = peer_clear_soft (peer, afi, safi, stype);

	  if (ret < 0)
	    bgp_clear_vty_error (vty, peer, afi, safi, ret);
	}
      return CMD_SUCCESS;
    }

  if (sort == clear_external)
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (peer->sort == BGP_PEER_IBGP)
	    continue;

	  if (stype == BGP_CLEAR_SOFT_NONE)
	    ret = peer_clear (peer);
	  else
	    ret = peer_clear_soft (peer, afi, safi, stype);

	  if (ret < 0)
	    bgp_clear_vty_error (vty, peer, afi, safi, ret);
	}
      return CMD_SUCCESS;
    }

  if (sort == clear_as)
    {
      as_t as;
      int find = 0;

      VTY_GET_INTEGER_RANGE ("AS", as, arg, 1, BGP_AS4_MAX);
      
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (peer->as != as) 
	    continue;

	  find = 1;
	  if (stype == BGP_CLEAR_SOFT_NONE)
	    ret = peer_clear (peer);
	  else
	    ret = peer_clear_soft (peer, afi, safi, stype);

	  if (ret < 0)
	    bgp_clear_vty_error (vty, peer, afi, safi, ret);
	}
      if (! find)
	vty_out (vty, "%%BGP: No peer is configured with AS %s%s", arg,
		 VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  return CMD_SUCCESS;
}

static int
bgp_clear_vty (struct vty *vty, const char *name, afi_t afi, safi_t safi,
               enum clear_sort sort, enum bgp_clear_type stype, 
               const char *arg)
{
  struct bgp *bgp;

  /* BGP structure lookup. */
  if (name)
    {
      bgp = bgp_lookup_by_name (name);
      if (bgp == NULL)
        {
          vty_out (vty, "Can't find BGP view %s%s", name, VTY_NEWLINE);
          return CMD_WARNING;
        }
    }
  else
    {
      bgp = bgp_get_default ();
      if (bgp == NULL)
        {
          vty_out (vty, "No BGP process is configured%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
    }

  return bgp_clear (vty, bgp, afi, safi, sort, stype, arg);
}
  
DEFUN (clear_ip_bgp_all,
       clear_ip_bgp_all_cmd,
       "clear ip bgp *",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], 0, 0, clear_all, BGP_CLEAR_SOFT_NONE, NULL);    

  return bgp_clear_vty (vty, NULL, 0, 0, clear_all, BGP_CLEAR_SOFT_NONE, NULL);
}

ALIAS (clear_ip_bgp_all,
       clear_bgp_all_cmd,
       "clear bgp *",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n")

ALIAS (clear_ip_bgp_all,
       clear_bgp_ipv6_all_cmd,
       "clear bgp ipv6 *",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n")

ALIAS (clear_ip_bgp_all,
       clear_ip_bgp_instance_all_cmd,
       "clear ip bgp view WORD *",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n")

ALIAS (clear_ip_bgp_all,
       clear_bgp_instance_all_cmd,
       "clear bgp view WORD *",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n")

DEFUN (clear_ip_bgp_peer,
       clear_ip_bgp_peer_cmd, 
       "clear ip bgp (A.B.C.D|X:X::X:X)",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n")
{
  return bgp_clear_vty (vty, NULL, 0, 0, clear_peer, BGP_CLEAR_SOFT_NONE, argv[0]);
}

ALIAS (clear_ip_bgp_peer,
       clear_bgp_peer_cmd, 
       "clear bgp (A.B.C.D|X:X::X:X)",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n")

ALIAS (clear_ip_bgp_peer,
       clear_bgp_ipv6_peer_cmd, 
       "clear bgp ipv6 (A.B.C.D|X:X::X:X)",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n")

DEFUN (clear_ip_bgp_peer_group,
       clear_ip_bgp_peer_group_cmd, 
       "clear ip bgp peer-group WORD",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n")
{
  return bgp_clear_vty (vty, NULL, 0, 0, clear_group, BGP_CLEAR_SOFT_NONE, argv[0]);
}

ALIAS (clear_ip_bgp_peer_group,
       clear_bgp_peer_group_cmd, 
       "clear bgp peer-group WORD",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n")

ALIAS (clear_ip_bgp_peer_group,
       clear_bgp_ipv6_peer_group_cmd, 
       "clear bgp ipv6 peer-group WORD",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n")

DEFUN (clear_ip_bgp_external,
       clear_ip_bgp_external_cmd,
       "clear ip bgp external",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n")
{
  return bgp_clear_vty (vty, NULL, 0, 0, clear_external, BGP_CLEAR_SOFT_NONE, NULL);
}

ALIAS (clear_ip_bgp_external,
       clear_bgp_external_cmd, 
       "clear bgp external",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n")

ALIAS (clear_ip_bgp_external,
       clear_bgp_ipv6_external_cmd, 
       "clear bgp ipv6 external",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n")

DEFUN (clear_ip_bgp_as,
       clear_ip_bgp_as_cmd,
       "clear ip bgp " CMD_AS_RANGE,
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n")
{
  return bgp_clear_vty (vty, NULL, 0, 0, clear_as, BGP_CLEAR_SOFT_NONE, argv[0]);
}       

ALIAS (clear_ip_bgp_as,
       clear_bgp_as_cmd,
       "clear bgp " CMD_AS_RANGE,
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n")

ALIAS (clear_ip_bgp_as,
       clear_bgp_ipv6_as_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE,
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n")

/* Outbound soft-reconfiguration */
DEFUN (clear_ip_bgp_all_soft_out,
       clear_ip_bgp_all_soft_out_cmd,
       "clear ip bgp * soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_OUT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_ip_bgp_all_soft_out,
       clear_ip_bgp_all_out_cmd,
       "clear ip bgp * out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_ip_bgp_all_soft_out,
       clear_ip_bgp_instance_all_soft_out_cmd,
       "clear ip bgp view WORD * soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_all_ipv4_soft_out,
       clear_ip_bgp_all_ipv4_soft_out_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_all,
			  BGP_CLEAR_SOFT_OUT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_ip_bgp_all_ipv4_soft_out,
       clear_ip_bgp_all_ipv4_out_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_instance_all_ipv4_soft_out,
       clear_ip_bgp_instance_all_ipv4_soft_out_cmd,
       "clear ip bgp view WORD * ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_MULTICAST, clear_all,
                          BGP_CLEAR_SOFT_OUT, NULL);

  return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_OUT, NULL);
}

DEFUN (clear_ip_bgp_all_vpnv4_soft_out,
       clear_ip_bgp_all_vpnv4_soft_out_cmd,
       "clear ip bgp * vpnv4 unicast soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_all,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_ip_bgp_all_vpnv4_soft_out,
       clear_ip_bgp_all_vpnv4_out_cmd,
       "clear ip bgp * vpnv4 unicast out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_bgp_all_soft_out,
       clear_bgp_all_soft_out_cmd,
       "clear bgp * soft out",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_OUT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_bgp_all_soft_out,
       clear_bgp_instance_all_soft_out_cmd,
       "clear bgp view WORD * soft out",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_all_soft_out,
       clear_bgp_all_out_cmd,
       "clear bgp * out",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_all_soft_out,
       clear_bgp_ipv6_all_soft_out_cmd,
       "clear bgp ipv6 * soft out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_all_soft_out,
       clear_bgp_ipv6_all_out_cmd,
       "clear bgp ipv6 * out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_peer_soft_out,
       clear_ip_bgp_peer_soft_out_cmd,
       "clear ip bgp A.B.C.D soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_soft_out,
       clear_ip_bgp_peer_out_cmd,
       "clear ip bgp A.B.C.D out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_peer_ipv4_soft_out,
       clear_ip_bgp_peer_ipv4_soft_out_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_peer,
			  BGP_CLEAR_SOFT_OUT, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_ipv4_soft_out,
       clear_ip_bgp_peer_ipv4_out_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_peer_vpnv4_soft_out,
       clear_ip_bgp_peer_vpnv4_soft_out_cmd,
       "clear ip bgp A.B.C.D vpnv4 unicast soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_peer,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_vpnv4_soft_out,
       clear_ip_bgp_peer_vpnv4_out_cmd,
       "clear ip bgp A.B.C.D vpnv4 unicast out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_bgp_peer_soft_out,
       clear_bgp_peer_soft_out_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) soft out",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_bgp_peer_soft_out,
       clear_bgp_ipv6_peer_soft_out_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) soft out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_peer_soft_out,
       clear_bgp_peer_out_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) out",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_peer_soft_out,
       clear_bgp_ipv6_peer_out_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_peer_group_soft_out,
       clear_ip_bgp_peer_group_soft_out_cmd, 
       "clear ip bgp peer-group WORD soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_group_soft_out,
       clear_ip_bgp_peer_group_out_cmd, 
       "clear ip bgp peer-group WORD out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_peer_group_ipv4_soft_out,
       clear_ip_bgp_peer_group_ipv4_soft_out_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_group,
			  BGP_CLEAR_SOFT_OUT, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_group_ipv4_soft_out,
       clear_ip_bgp_peer_group_ipv4_out_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_bgp_peer_group_soft_out,
       clear_bgp_peer_group_soft_out_cmd,
       "clear bgp peer-group WORD soft out",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_bgp_peer_group_soft_out,
       clear_bgp_ipv6_peer_group_soft_out_cmd,
       "clear bgp ipv6 peer-group WORD soft out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_peer_group_soft_out,
       clear_bgp_peer_group_out_cmd,
       "clear bgp peer-group WORD out",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_peer_group_soft_out,
       clear_bgp_ipv6_peer_group_out_cmd,
       "clear bgp ipv6 peer-group WORD out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_external_soft_out,
       clear_ip_bgp_external_soft_out_cmd, 
       "clear ip bgp external soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_ip_bgp_external_soft_out,
       clear_ip_bgp_external_out_cmd, 
       "clear ip bgp external out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_external_ipv4_soft_out,
       clear_ip_bgp_external_ipv4_soft_out_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_external,
			  BGP_CLEAR_SOFT_OUT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_ip_bgp_external_ipv4_soft_out,
       clear_ip_bgp_external_ipv4_out_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_bgp_external_soft_out,
       clear_bgp_external_soft_out_cmd,
       "clear bgp external soft out",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_OUT, NULL);
}

ALIAS (clear_bgp_external_soft_out,
       clear_bgp_ipv6_external_soft_out_cmd,
       "clear bgp ipv6 external soft out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_external_soft_out,
       clear_bgp_external_out_cmd,
       "clear bgp external out",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_external_soft_out,
       clear_bgp_ipv6_external_out_cmd,
       "clear bgp ipv6 external WORD out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_as_soft_out,
       clear_ip_bgp_as_soft_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_as_soft_out,
       clear_ip_bgp_as_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_as_ipv4_soft_out,
       clear_ip_bgp_as_ipv4_soft_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_as,
			  BGP_CLEAR_SOFT_OUT, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_as_ipv4_soft_out,
       clear_ip_bgp_as_ipv4_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_ip_bgp_as_vpnv4_soft_out,
       clear_ip_bgp_as_vpnv4_soft_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " vpnv4 unicast soft out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_as,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_ip_bgp_as_vpnv4_soft_out,
       clear_ip_bgp_as_vpnv4_out_cmd,
       "clear ip bgp " CMD_AS_RANGE " vpnv4 unicast out",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Soft reconfig outbound update\n")

DEFUN (clear_bgp_as_soft_out,
       clear_bgp_as_soft_out_cmd,
       "clear bgp " CMD_AS_RANGE " soft out",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_OUT, argv[0]);
}

ALIAS (clear_bgp_as_soft_out,
       clear_bgp_ipv6_as_soft_out_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " soft out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_as_soft_out,
       clear_bgp_as_out_cmd,
       "clear bgp " CMD_AS_RANGE " out",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig outbound update\n")

ALIAS (clear_bgp_as_soft_out,
       clear_bgp_ipv6_as_out_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " out",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig outbound update\n")

/* Inbound soft-reconfiguration */
DEFUN (clear_ip_bgp_all_soft_in,
       clear_ip_bgp_all_soft_in_cmd,
       "clear ip bgp * soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_IN, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_ip_bgp_all_soft_in,
       clear_ip_bgp_instance_all_soft_in_cmd,
       "clear ip bgp view WORD * soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_ip_bgp_all_soft_in,
       clear_ip_bgp_all_in_cmd,
       "clear ip bgp * in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_all_in_prefix_filter,
       clear_ip_bgp_all_in_prefix_filter_cmd,
       "clear ip bgp * in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (argc== 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

ALIAS (clear_ip_bgp_all_in_prefix_filter,
       clear_ip_bgp_instance_all_in_prefix_filter_cmd,
       "clear ip bgp view WORD * in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")


DEFUN (clear_ip_bgp_all_ipv4_soft_in,
       clear_ip_bgp_all_ipv4_soft_in_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_all,
			  BGP_CLEAR_SOFT_IN, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_ip_bgp_all_ipv4_soft_in,
       clear_ip_bgp_all_ipv4_in_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_instance_all_ipv4_soft_in,
       clear_ip_bgp_instance_all_ipv4_soft_in_cmd,
       "clear ip bgp view WORD * ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_MULTICAST, clear_all,
                          BGP_CLEAR_SOFT_IN, NULL);

  return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_IN, NULL);
}

DEFUN (clear_ip_bgp_all_ipv4_in_prefix_filter,
       clear_ip_bgp_all_ipv4_in_prefix_filter_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_all,
			  BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

DEFUN (clear_ip_bgp_instance_all_ipv4_in_prefix_filter,
       clear_ip_bgp_instance_all_ipv4_in_prefix_filter_cmd,
       "clear ip bgp view WORD * ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_MULTICAST, clear_all,
                          BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);

  return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

DEFUN (clear_ip_bgp_all_vpnv4_soft_in,
       clear_ip_bgp_all_vpnv4_soft_in_cmd,
       "clear ip bgp * vpnv4 unicast soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_all,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_ip_bgp_all_vpnv4_soft_in,
       clear_ip_bgp_all_vpnv4_in_cmd,
       "clear ip bgp * vpnv4 unicast in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_all_soft_in,
       clear_bgp_all_soft_in_cmd,
       "clear bgp * soft in",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_IN, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_bgp_all_soft_in,
       clear_bgp_instance_all_soft_in_cmd,
       "clear bgp view WORD * soft in",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_all_soft_in,
       clear_bgp_ipv6_all_soft_in_cmd,
       "clear bgp ipv6 * soft in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_all_soft_in,
       clear_bgp_all_in_cmd,
       "clear bgp * in",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_all_soft_in,
       clear_bgp_ipv6_all_in_cmd,
       "clear bgp ipv6 * in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_all_in_prefix_filter,
       clear_bgp_all_in_prefix_filter_cmd,
       "clear bgp * in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

ALIAS (clear_bgp_all_in_prefix_filter,
       clear_bgp_ipv6_all_in_prefix_filter_cmd,
       "clear bgp ipv6 * in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")

DEFUN (clear_ip_bgp_peer_soft_in,
       clear_ip_bgp_peer_soft_in_cmd,
       "clear ip bgp A.B.C.D soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_peer_soft_in,
       clear_ip_bgp_peer_in_cmd,
       "clear ip bgp A.B.C.D in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig inbound update\n")
       
DEFUN (clear_ip_bgp_peer_in_prefix_filter,
       clear_ip_bgp_peer_in_prefix_filter_cmd,
       "clear ip bgp A.B.C.D in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig inbound update\n"
       "Push out the existing ORF prefix-list\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_ip_bgp_peer_ipv4_soft_in,
       clear_ip_bgp_peer_ipv4_soft_in_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_peer,
			  BGP_CLEAR_SOFT_IN, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_peer_ipv4_soft_in,
       clear_ip_bgp_peer_ipv4_in_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_peer_ipv4_in_prefix_filter,
       clear_ip_bgp_peer_ipv4_in_prefix_filter_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out the existing ORF prefix-list\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_peer,
			  BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_ip_bgp_peer_vpnv4_soft_in,
       clear_ip_bgp_peer_vpnv4_soft_in_cmd,
       "clear ip bgp A.B.C.D vpnv4 unicast soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_peer,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_peer_vpnv4_soft_in,
       clear_ip_bgp_peer_vpnv4_in_cmd,
       "clear ip bgp A.B.C.D vpnv4 unicast in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_peer_soft_in,
       clear_bgp_peer_soft_in_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) soft in",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_bgp_peer_soft_in,
       clear_bgp_ipv6_peer_soft_in_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) soft in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_peer_soft_in,
       clear_bgp_peer_in_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) in",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_peer_soft_in,
       clear_bgp_ipv6_peer_in_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_peer_in_prefix_filter,
       clear_bgp_peer_in_prefix_filter_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) in prefix-filter",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig inbound update\n"
       "Push out the existing ORF prefix-list\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

ALIAS (clear_bgp_peer_in_prefix_filter,
       clear_bgp_ipv6_peer_in_prefix_filter_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig inbound update\n"
       "Push out the existing ORF prefix-list\n")

DEFUN (clear_ip_bgp_peer_group_soft_in,
       clear_ip_bgp_peer_group_soft_in_cmd,
       "clear ip bgp peer-group WORD soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_peer_group_soft_in,
       clear_ip_bgp_peer_group_in_cmd,
       "clear ip bgp peer-group WORD in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_peer_group_in_prefix_filter,
       clear_ip_bgp_peer_group_in_prefix_filter_cmd,
       "clear ip bgp peer-group WORD in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_ip_bgp_peer_group_ipv4_soft_in,
       clear_ip_bgp_peer_group_ipv4_soft_in_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_group,
			  BGP_CLEAR_SOFT_IN, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_peer_group_ipv4_soft_in,
       clear_ip_bgp_peer_group_ipv4_in_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_peer_group_ipv4_in_prefix_filter,
       clear_ip_bgp_peer_group_ipv4_in_prefix_filter_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_group,
			  BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_bgp_peer_group_soft_in,
       clear_bgp_peer_group_soft_in_cmd,
       "clear bgp peer-group WORD soft in",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_bgp_peer_group_soft_in,
       clear_bgp_ipv6_peer_group_soft_in_cmd,
       "clear bgp ipv6 peer-group WORD soft in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_peer_group_soft_in,
       clear_bgp_peer_group_in_cmd,
       "clear bgp peer-group WORD in",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_peer_group_soft_in,
       clear_bgp_ipv6_peer_group_in_cmd,
       "clear bgp ipv6 peer-group WORD in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_peer_group_in_prefix_filter,
       clear_bgp_peer_group_in_prefix_filter_cmd,
       "clear bgp peer-group WORD in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

ALIAS (clear_bgp_peer_group_in_prefix_filter,
       clear_bgp_ipv6_peer_group_in_prefix_filter_cmd,
       "clear bgp ipv6 peer-group WORD in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")

DEFUN (clear_ip_bgp_external_soft_in,
       clear_ip_bgp_external_soft_in_cmd,
       "clear ip bgp external soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_ip_bgp_external_soft_in,
       clear_ip_bgp_external_in_cmd,
       "clear ip bgp external in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_external_in_prefix_filter,
       clear_ip_bgp_external_in_prefix_filter_cmd,
       "clear ip bgp external in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

DEFUN (clear_ip_bgp_external_ipv4_soft_in,
       clear_ip_bgp_external_ipv4_soft_in_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_external,
			  BGP_CLEAR_SOFT_IN, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_ip_bgp_external_ipv4_soft_in,
       clear_ip_bgp_external_ipv4_in_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_external_ipv4_in_prefix_filter,
       clear_ip_bgp_external_ipv4_in_prefix_filter_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_external,
			  BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

DEFUN (clear_bgp_external_soft_in,
       clear_bgp_external_soft_in_cmd,
       "clear bgp external soft in",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN, NULL);
}

ALIAS (clear_bgp_external_soft_in,
       clear_bgp_ipv6_external_soft_in_cmd,
       "clear bgp ipv6 external soft in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_external_soft_in,
       clear_bgp_external_in_cmd,
       "clear bgp external in",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_external_soft_in,
       clear_bgp_ipv6_external_in_cmd,
       "clear bgp ipv6 external WORD in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_external_in_prefix_filter,
       clear_bgp_external_in_prefix_filter_cmd,
       "clear bgp external in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, NULL);
}

ALIAS (clear_bgp_external_in_prefix_filter,
       clear_bgp_ipv6_external_in_prefix_filter_cmd,
       "clear bgp ipv6 external in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")

DEFUN (clear_ip_bgp_as_soft_in,
       clear_ip_bgp_as_soft_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_as_soft_in,
       clear_ip_bgp_as_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_as_in_prefix_filter,
       clear_ip_bgp_as_in_prefix_filter_cmd,
       "clear ip bgp " CMD_AS_RANGE " in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_ip_bgp_as_ipv4_soft_in,
       clear_ip_bgp_as_ipv4_soft_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_as,
			  BGP_CLEAR_SOFT_IN, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_as_ipv4_soft_in,
       clear_ip_bgp_as_ipv4_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_ip_bgp_as_ipv4_in_prefix_filter,
       clear_ip_bgp_as_ipv4_in_prefix_filter_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) in prefix-filter",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_as,
			  BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

DEFUN (clear_ip_bgp_as_vpnv4_soft_in,
       clear_ip_bgp_as_vpnv4_soft_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " vpnv4 unicast soft in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_as,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_ip_bgp_as_vpnv4_soft_in,
       clear_ip_bgp_as_vpnv4_in_cmd,
       "clear ip bgp " CMD_AS_RANGE " vpnv4 unicast in",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family modifier\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_as_soft_in,
       clear_bgp_as_soft_in_cmd,
       "clear bgp " CMD_AS_RANGE " soft in",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN, argv[0]);
}

ALIAS (clear_bgp_as_soft_in,
       clear_bgp_ipv6_as_soft_in_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " soft in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_as_soft_in,
       clear_bgp_as_in_cmd,
       "clear bgp " CMD_AS_RANGE " in",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n")

ALIAS (clear_bgp_as_soft_in,
       clear_bgp_ipv6_as_in_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " in",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n")

DEFUN (clear_bgp_as_in_prefix_filter,
       clear_bgp_as_in_prefix_filter_cmd,
       "clear bgp " CMD_AS_RANGE " in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_IN_ORF_PREFIX, argv[0]);
}

ALIAS (clear_bgp_as_in_prefix_filter,
       clear_bgp_ipv6_as_in_prefix_filter_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " in prefix-filter",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig inbound update\n"
       "Push out prefix-list ORF and do inbound soft reconfig\n")

/* Both soft-reconfiguration */
DEFUN (clear_ip_bgp_all_soft,
       clear_ip_bgp_all_soft_cmd,
       "clear ip bgp * soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_BOTH, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_BOTH, NULL);
}

ALIAS (clear_ip_bgp_all_soft,
       clear_ip_bgp_instance_all_soft_cmd,
       "clear ip bgp view WORD * soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n")


DEFUN (clear_ip_bgp_all_ipv4_soft,
       clear_ip_bgp_all_ipv4_soft_cmd,
       "clear ip bgp * ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_all,
			  BGP_CLEAR_SOFT_BOTH, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_BOTH, NULL);
}

DEFUN (clear_ip_bgp_instance_all_ipv4_soft,
       clear_ip_bgp_instance_all_ipv4_soft_cmd,
       "clear ip bgp view WORD * ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_all,
                          BGP_CLEAR_SOFT_BOTH, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_BOTH, NULL);
}

DEFUN (clear_ip_bgp_all_vpnv4_soft,
       clear_ip_bgp_all_vpnv4_soft_cmd,
       "clear ip bgp * vpnv4 unicast soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_all,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_bgp_all_soft,
       clear_bgp_all_soft_cmd,
       "clear bgp * soft",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_BOTH, argv[0]);
 
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_all,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

ALIAS (clear_bgp_all_soft,
       clear_bgp_instance_all_soft_cmd,
       "clear bgp view WORD * soft",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig\n")

ALIAS (clear_bgp_all_soft,
       clear_bgp_ipv6_all_soft_cmd,
       "clear bgp ipv6 * soft",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig\n")

DEFUN (clear_ip_bgp_peer_soft,
       clear_ip_bgp_peer_soft_cmd,
       "clear ip bgp A.B.C.D soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_ip_bgp_peer_ipv4_soft,
       clear_ip_bgp_peer_ipv4_soft_cmd,
       "clear ip bgp A.B.C.D ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_peer,
			  BGP_CLEAR_SOFT_BOTH, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_ip_bgp_peer_vpnv4_soft,
       clear_ip_bgp_peer_vpnv4_soft_cmd,
       "clear ip bgp A.B.C.D vpnv4 unicast soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_peer,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_bgp_peer_soft,
       clear_bgp_peer_soft_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) soft",
       CLEAR_STR
       BGP_STR
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_peer,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

ALIAS (clear_bgp_peer_soft,
       clear_bgp_ipv6_peer_soft_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) soft",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig\n")

DEFUN (clear_ip_bgp_peer_group_soft,
       clear_ip_bgp_peer_group_soft_cmd,
       "clear ip bgp peer-group WORD soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_ip_bgp_peer_group_ipv4_soft,
       clear_ip_bgp_peer_group_ipv4_soft_cmd,
       "clear ip bgp peer-group WORD ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_group,
			  BGP_CLEAR_SOFT_BOTH, argv[0]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_bgp_peer_group_soft,
       clear_bgp_peer_group_soft_cmd,
       "clear bgp peer-group WORD soft",
       CLEAR_STR
       BGP_STR
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_group,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

ALIAS (clear_bgp_peer_group_soft,
       clear_bgp_ipv6_peer_group_soft_cmd,
       "clear bgp ipv6 peer-group WORD soft",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all members of peer-group\n"
       "BGP peer-group name\n"
       "Soft reconfig\n")

DEFUN (clear_ip_bgp_external_soft,
       clear_ip_bgp_external_soft_cmd,
       "clear ip bgp external soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_BOTH, NULL);
}

DEFUN (clear_ip_bgp_external_ipv4_soft,
       clear_ip_bgp_external_ipv4_soft_cmd,
       "clear ip bgp external ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all external peers\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_external,
			  BGP_CLEAR_SOFT_BOTH, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_BOTH, NULL);
}

DEFUN (clear_bgp_external_soft,
       clear_bgp_external_soft_cmd,
       "clear bgp external soft",
       CLEAR_STR
       BGP_STR
       "Clear all external peers\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_external,
			BGP_CLEAR_SOFT_BOTH, NULL);
}

ALIAS (clear_bgp_external_soft,
       clear_bgp_ipv6_external_soft_cmd,
       "clear bgp ipv6 external soft",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all external peers\n"
       "Soft reconfig\n")

DEFUN (clear_ip_bgp_as_soft,
       clear_ip_bgp_as_soft_cmd,
       "clear ip bgp " CMD_AS_RANGE " soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_ip_bgp_as_ipv4_soft,
       clear_ip_bgp_as_ipv4_soft_cmd,
       "clear ip bgp " CMD_AS_RANGE " ipv4 (unicast|multicast) soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MULTICAST, clear_as,
			  BGP_CLEAR_SOFT_BOTH, argv[0]);

  return bgp_clear_vty (vty, NULL,AFI_IP, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_ip_bgp_as_vpnv4_soft,
       clear_ip_bgp_as_vpnv4_soft_cmd,
       "clear ip bgp " CMD_AS_RANGE " vpnv4 unicast soft",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Address family\n"
       "Address Family Modifier\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN, clear_as,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

DEFUN (clear_bgp_as_soft,
       clear_bgp_as_soft_cmd,
       "clear bgp " CMD_AS_RANGE " soft",
       CLEAR_STR
       BGP_STR
       "Clear peers with the AS number\n"
       "Soft reconfig\n")
{
  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_as,
			BGP_CLEAR_SOFT_BOTH, argv[0]);
}

ALIAS (clear_bgp_as_soft,
       clear_bgp_ipv6_as_soft_cmd,
       "clear bgp ipv6 " CMD_AS_RANGE " soft",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear peers with the AS number\n"
       "Soft reconfig\n")

/* RS-client soft reconfiguration. */
#ifdef HAVE_IPV6
DEFUN (clear_bgp_all_rsclient,
       clear_bgp_all_rsclient_cmd,
       "clear bgp * rsclient",
       CLEAR_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_RSCLIENT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_RSCLIENT, NULL);
}

ALIAS (clear_bgp_all_rsclient,
       clear_bgp_ipv6_all_rsclient_cmd,
       "clear bgp ipv6 * rsclient",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")

ALIAS (clear_bgp_all_rsclient,
       clear_bgp_instance_all_rsclient_cmd,
       "clear bgp view WORD * rsclient",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")

ALIAS (clear_bgp_all_rsclient,
       clear_bgp_ipv6_instance_all_rsclient_cmd,
       "clear bgp ipv6 view WORD * rsclient",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")
#endif /* HAVE_IPV6 */

DEFUN (clear_ip_bgp_all_rsclient,
       clear_ip_bgp_all_rsclient_cmd,
       "clear ip bgp * rsclient",
       CLEAR_STR
       IP_STR
       BGP_STR
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")
{
  if (argc == 1)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_all,
                          BGP_CLEAR_SOFT_RSCLIENT, NULL);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_all,
                        BGP_CLEAR_SOFT_RSCLIENT, NULL);
}

ALIAS (clear_ip_bgp_all_rsclient,
       clear_ip_bgp_instance_all_rsclient_cmd,
       "clear ip bgp view WORD * rsclient",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "Clear all peers\n"
       "Soft reconfig for rsclient RIB\n")

#ifdef HAVE_IPV6
DEFUN (clear_bgp_peer_rsclient,
       clear_bgp_peer_rsclient_cmd,
       "clear bgp (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       BGP_STR
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")
{
  if (argc == 2)
    return bgp_clear_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST, clear_peer,
                          BGP_CLEAR_SOFT_RSCLIENT, argv[1]);

  return bgp_clear_vty (vty, NULL, AFI_IP6, SAFI_UNICAST, clear_peer,
                        BGP_CLEAR_SOFT_RSCLIENT, argv[0]);
}

ALIAS (clear_bgp_peer_rsclient,
       clear_bgp_ipv6_peer_rsclient_cmd,
       "clear bgp ipv6 (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")

ALIAS (clear_bgp_peer_rsclient,
       clear_bgp_instance_peer_rsclient_cmd,
       "clear bgp view WORD (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")

ALIAS (clear_bgp_peer_rsclient,
       clear_bgp_ipv6_instance_peer_rsclient_cmd,
       "clear bgp ipv6 view WORD (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       BGP_STR
       "Address family\n"
       "BGP view\n"
       "view name\n"
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")
#endif /* HAVE_IPV6 */

DEFUN (clear_ip_bgp_peer_rsclient,
       clear_ip_bgp_peer_rsclient_cmd,
       "clear ip bgp (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")
{
  if (argc == 2)
    return bgp_clear_vty (vty, argv[0], AFI_IP, SAFI_UNICAST, clear_peer,
                          BGP_CLEAR_SOFT_RSCLIENT, argv[1]);

  return bgp_clear_vty (vty, NULL, AFI_IP, SAFI_UNICAST, clear_peer,
                        BGP_CLEAR_SOFT_RSCLIENT, argv[0]);
}

ALIAS (clear_ip_bgp_peer_rsclient,
       clear_ip_bgp_instance_peer_rsclient_cmd,
       "clear ip bgp view WORD (A.B.C.D|X:X::X:X) rsclient",
       CLEAR_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "view name\n"
       "BGP neighbor IP address to clear\n"
       "BGP IPv6 neighbor to clear\n"
       "Soft reconfig for rsclient RIB\n")

DEFUN (show_bgp_views,
       show_bgp_views_cmd,
       "show bgp views",
       SHOW_STR
       BGP_STR
       "Show the defined BGP views\n")
{
  struct list *inst = bm->bgp;
  struct listnode *node;
  struct bgp *bgp;

  if (!bgp_option_check (BGP_OPT_MULTIPLE_INSTANCE))
    {
      vty_out (vty, "Multiple BGP views are not defined%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  vty_out (vty, "Defined BGP views:%s", VTY_NEWLINE);
  for (ALL_LIST_ELEMENTS_RO(inst, node, bgp))
    vty_out (vty, "\t%s (AS%u)%s", 
             bgp->name ? bgp->name : "(null)",
             bgp->as, VTY_NEWLINE);
  
  return CMD_SUCCESS;
}

DEFUN (show_bgp_memory, 
       show_bgp_memory_cmd,
       "show bgp memory",
       SHOW_STR
       BGP_STR
       "Global BGP memory statistics\n")
{
  char memstrbuf[MTYPE_MEMSTR_LEN];
  unsigned long count;
  
  /* RIB related usage stats */
  count = mtype_stats_alloc (MTYPE_BGP_NODE);
  vty_out (vty, "%ld RIB nodes, using %s of memory%s", count,
           mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct bgp_node)),
           VTY_NEWLINE);
  
  count = mtype_stats_alloc (MTYPE_BGP_ROUTE);
  vty_out (vty, "%ld BGP routes, using %s of memory%s", count,
           mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct bgp_info)),
           VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_BGP_ROUTE_EXTRA)))
    vty_out (vty, "%ld BGP route ancillaries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct bgp_info_extra)),
             VTY_NEWLINE);
  
  if ((count = mtype_stats_alloc (MTYPE_BGP_STATIC)))
    vty_out (vty, "%ld Static routes, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct bgp_static)),
             VTY_NEWLINE);
  
  /* Adj-In/Out */
  if ((count = mtype_stats_alloc (MTYPE_BGP_ADJ_IN)))
    vty_out (vty, "%ld Adj-In entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct bgp_adj_in)),
             VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_BGP_ADJ_OUT)))
    vty_out (vty, "%ld Adj-Out entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct bgp_adj_out)),
             VTY_NEWLINE);
  
  if ((count = mtype_stats_alloc (MTYPE_BGP_NEXTHOP_CACHE)))
    vty_out (vty, "%ld Nexthop cache entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct bgp_nexthop_cache)),
             VTY_NEWLINE);

  if ((count = mtype_stats_alloc (MTYPE_BGP_DAMP_INFO)))
    vty_out (vty, "%ld Dampening entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct bgp_damp_info)),
             VTY_NEWLINE);

  /* Attributes */
  count = attr_count();
  vty_out (vty, "%ld BGP attributes, using %s of memory%s", count, 
           mtype_memstr (memstrbuf, sizeof (memstrbuf), 
                         count * sizeof(struct attr)), 
           VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_ATTR_EXTRA)))
    vty_out (vty, "%ld BGP extra attributes, using %s of memory%s", count, 
             mtype_memstr (memstrbuf, sizeof (memstrbuf), 
                           count * sizeof(struct attr_extra)), 
             VTY_NEWLINE);
  
  if ((count = attr_unknown_count()))
    vty_out (vty, "%ld unknown attributes%s", count, VTY_NEWLINE);
  
  /* AS_PATH attributes */
  count = aspath_count ();
  vty_out (vty, "%ld BGP AS-PATH entries, using %s of memory%s", count,
           mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct aspath)),
           VTY_NEWLINE);
  
  count = mtype_stats_alloc (MTYPE_AS_SEG);
  vty_out (vty, "%ld BGP AS-PATH segments, using %s of memory%s", count,
           mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct assegment)),
           VTY_NEWLINE);
  
  /* Other attributes */
  if ((count = community_count ()))
    vty_out (vty, "%ld BGP community entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct community)),
             VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_ECOMMUNITY)))
    vty_out (vty, "%ld BGP community entries, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct ecommunity)),
             VTY_NEWLINE);
  
  if ((count = mtype_stats_alloc (MTYPE_CLUSTER)))
    vty_out (vty, "%ld Cluster lists, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct cluster_list)),
             VTY_NEWLINE);
  
  /* Peer related usage */
  count = mtype_stats_alloc (MTYPE_BGP_PEER);
  vty_out (vty, "%ld peers, using %s of memory%s", count,
           mtype_memstr (memstrbuf, sizeof (memstrbuf),
                         count * sizeof (struct peer)),
           VTY_NEWLINE);
  
  if ((count = mtype_stats_alloc (MTYPE_PEER_GROUP)))
    vty_out (vty, "%ld peer groups, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct peer_group)),
             VTY_NEWLINE);
  
  /* Other */
  if ((count = mtype_stats_alloc (MTYPE_HASH)))
    vty_out (vty, "%ld hash tables, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct hash)),
             VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_HASH_BACKET)))
    vty_out (vty, "%ld hash buckets, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (struct hash_backet)),
             VTY_NEWLINE);
  if ((count = mtype_stats_alloc (MTYPE_BGP_REGEXP)))
    vty_out (vty, "%ld compiled regexes, using %s of memory%s", count,
             mtype_memstr (memstrbuf, sizeof (memstrbuf),
                           count * sizeof (regex_t)),
             VTY_NEWLINE);
  return CMD_SUCCESS;
}

/* Show BGP peer's summary information. */
static int
bgp_show_summary (struct vty *vty, struct bgp *bgp, int afi, int safi)
{
  struct peer *peer;
  struct listnode *node, *nnode;
  unsigned int count = 0;
  char timebuf[BGP_UPTIME_LEN];
  int len;

  /* Header string for each address family. */
  static char header[] = "Neighbor        V         AS MsgRcvd MsgSent   TblVer  InQ OutQ Up/Down  State/PfxRcd";
  
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      if (peer->afc[afi][safi])
	{
          if (!count)
            {
              unsigned long ents;
              char memstrbuf[MTYPE_MEMSTR_LEN];
              
              /* Usage summary and header */
              vty_out (vty,
                       "BGP router identifier %s, local AS number %u%s",
                       inet_ntoa (bgp->router_id), bgp->as, VTY_NEWLINE);

              ents = bgp_table_count (bgp->rib[afi][safi]);
              vty_out (vty, "RIB entries %ld, using %s of memory%s", ents,
                       mtype_memstr (memstrbuf, sizeof (memstrbuf),
                                     ents * sizeof (struct bgp_node)),
                       VTY_NEWLINE);
              
              /* Peer related usage */
              ents = listcount (bgp->peer);
              vty_out (vty, "Peers %ld, using %s of memory%s",
                       ents,
                       mtype_memstr (memstrbuf, sizeof (memstrbuf),
                                     ents * sizeof (struct peer)),
                       VTY_NEWLINE);
              
              if ((ents = listcount (bgp->rsclient)))
                vty_out (vty, "RS-Client peers %ld, using %s of memory%s",
                         ents,
                         mtype_memstr (memstrbuf, sizeof (memstrbuf),
                                       ents * sizeof (struct peer)),
                         VTY_NEWLINE);
              
              if ((ents = listcount (bgp->group)))
                vty_out (vty, "Peer groups %ld, using %s of memory%s", ents,
                         mtype_memstr (memstrbuf, sizeof (memstrbuf),
                                       ents * sizeof (struct peer_group)),
                         VTY_NEWLINE);

              if (CHECK_FLAG (bgp->af_flags[afi][safi], BGP_CONFIG_DAMPENING))
                vty_out (vty, "Dampening enabled.%s", VTY_NEWLINE);
              vty_out (vty, "%s", VTY_NEWLINE);
              vty_out (vty, "%s%s", header, VTY_NEWLINE);
            }
          
	  count++;

	  len = vty_out (vty, "%s", peer->host);
	  len = 16 - len;
	  if (len < 1)
	    vty_out (vty, "%s%*s", VTY_NEWLINE, 16, " ");
	  else
	    vty_out (vty, "%*s", len, " ");

	  vty_out (vty, "4 ");

	  vty_out (vty, "%5u %7d %7d %8d %4d %4lu ",
		   peer->as,
		   peer->open_in + peer->update_in + peer->keepalive_in
		   + peer->notify_in + peer->refresh_in + peer->dynamic_cap_in,
		   peer->open_out + peer->update_out + peer->keepalive_out
		   + peer->notify_out + peer->refresh_out
		   + peer->dynamic_cap_out,
		   0, 0, (unsigned long) peer->obuf->count);

	  vty_out (vty, "%8s", 
		   peer_uptime (peer->uptime, timebuf, BGP_UPTIME_LEN));

	  if (peer->status == Established)
	    {
	      vty_out (vty, " %8ld", peer->pcount[afi][safi]);
	    }
	  else
	    {
	      if (CHECK_FLAG (peer->flags, PEER_FLAG_SHUTDOWN))
		vty_out (vty, " Idle (Admin)");
	      else if (CHECK_FLAG (peer->sflags, PEER_STATUS_PREFIX_OVERFLOW))
		vty_out (vty, " Idle (PfxCt)");
	      else
		vty_out (vty, " %-11s", LOOKUP(bgp_status_msg, peer->status));
	    }

	  vty_out (vty, "%s", VTY_NEWLINE);
	}
    }

  if (count)
    vty_out (vty, "%sTotal number of neighbors %d%s", VTY_NEWLINE,
	     count, VTY_NEWLINE);
  else
    vty_out (vty, "No %s neighbor is configured%s",
	     afi == AFI_IP ? "IPv4" : "IPv6", VTY_NEWLINE);
  return CMD_SUCCESS;
}

static int 
bgp_show_summary_vty (struct vty *vty, const char *name, 
                      afi_t afi, safi_t safi)
{
  struct bgp *bgp;

  if (name)
    {
      bgp = bgp_lookup_by_name (name);
      
      if (! bgp)
	{
	  vty_out (vty, "%% No such BGP instance exist%s", VTY_NEWLINE); 
	  return CMD_WARNING;
	}

      bgp_show_summary (vty, bgp, afi, safi);
      return CMD_SUCCESS;
    }
  
  bgp = bgp_get_default ();

  if (bgp)
    bgp_show_summary (vty, bgp, afi, safi);    
 
  return CMD_SUCCESS;
}

/* `show ip bgp summary' commands. */
DEFUN (show_ip_bgp_summary, 
       show_ip_bgp_summary_cmd,
       "show ip bgp summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, NULL, AFI_IP, SAFI_UNICAST);
}

DEFUN (show_ip_bgp_instance_summary,
       show_ip_bgp_instance_summary_cmd,
       "show ip bgp view WORD summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, argv[0], AFI_IP, SAFI_UNICAST);  
}

DEFUN (show_ip_bgp_ipv4_summary, 
       show_ip_bgp_ipv4_summary_cmd,
       "show ip bgp ipv4 (unicast|multicast) summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_show_summary_vty (vty, NULL, AFI_IP, SAFI_MULTICAST);

  return bgp_show_summary_vty (vty, NULL, AFI_IP, SAFI_UNICAST);
}

ALIAS (show_ip_bgp_ipv4_summary,
       show_bgp_ipv4_safi_summary_cmd,
       "show bgp ipv4 (unicast|multicast) summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")

DEFUN (show_ip_bgp_instance_ipv4_summary,
       show_ip_bgp_instance_ipv4_summary_cmd,
       "show ip bgp view WORD ipv4 (unicast|multicast) summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_show_summary_vty (vty, argv[0], AFI_IP, SAFI_MULTICAST);
  else
    return bgp_show_summary_vty (vty, argv[0], AFI_IP, SAFI_UNICAST);
}

ALIAS (show_ip_bgp_instance_ipv4_summary,
       show_bgp_instance_ipv4_safi_summary_cmd,
       "show bgp view WORD ipv4 (unicast|multicast) summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")

DEFUN (show_ip_bgp_vpnv4_all_summary,
       show_ip_bgp_vpnv4_all_summary_cmd,
       "show ip bgp vpnv4 all summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN);
}

DEFUN (show_ip_bgp_vpnv4_rd_summary,
       show_ip_bgp_vpnv4_rd_summary_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n"
       "Summary of BGP neighbor status\n")
{
  int ret;
  struct prefix_rd prd;

  ret = str2prefix_rd (argv[0], &prd);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Route Distinguisher%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_show_summary_vty (vty, NULL, AFI_IP, SAFI_MPLS_VPN);
}

#ifdef HAVE_IPV6
DEFUN (show_bgp_summary, 
       show_bgp_summary_cmd,
       "show bgp summary",
       SHOW_STR
       BGP_STR
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, NULL, AFI_IP6, SAFI_UNICAST);
}

DEFUN (show_bgp_instance_summary,
       show_bgp_instance_summary_cmd,
       "show bgp view WORD summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST);
}

ALIAS (show_bgp_summary, 
       show_bgp_ipv6_summary_cmd,
       "show bgp ipv6 summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Summary of BGP neighbor status\n")

ALIAS (show_bgp_instance_summary,
       show_bgp_instance_ipv6_summary_cmd,
       "show bgp view WORD ipv6 summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Summary of BGP neighbor status\n")

DEFUN (show_bgp_ipv6_safi_summary,
       show_bgp_ipv6_safi_summary_cmd,
       "show bgp ipv6 (unicast|multicast) summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_show_summary_vty (vty, NULL, AFI_IP6, SAFI_MULTICAST);

  return bgp_show_summary_vty (vty, NULL, AFI_IP6, SAFI_UNICAST);
}

DEFUN (show_bgp_instance_ipv6_safi_summary,
       show_bgp_instance_ipv6_safi_summary_cmd,
       "show bgp view WORD ipv6 (unicast|multicast) summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Summary of BGP neighbor status\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_show_summary_vty (vty, argv[0], AFI_IP6, SAFI_MULTICAST);

  return bgp_show_summary_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST);
}

/* old command */
DEFUN (show_ipv6_bgp_summary, 
       show_ipv6_bgp_summary_cmd,
       "show ipv6 bgp summary",
       SHOW_STR
       IPV6_STR
       BGP_STR
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, NULL, AFI_IP6, SAFI_UNICAST);
}

/* old command */
DEFUN (show_ipv6_mbgp_summary, 
       show_ipv6_mbgp_summary_cmd,
       "show ipv6 mbgp summary",
       SHOW_STR
       IPV6_STR
       MBGP_STR
       "Summary of BGP neighbor status\n")
{
  return bgp_show_summary_vty (vty, NULL, AFI_IP6, SAFI_MULTICAST);
}
#endif /* HAVE_IPV6 */

const char *
afi_safi_print (afi_t afi, safi_t safi)
{
  if (afi == AFI_IP && safi == SAFI_UNICAST)
    return "IPv4 Unicast";
  else if (afi == AFI_IP && safi == SAFI_MULTICAST)
    return "IPv4 Multicast";
  else if (afi == AFI_IP && safi == SAFI_MPLS_VPN)
    return "VPNv4 Unicast";
  else if (afi == AFI_IP6 && safi == SAFI_UNICAST)
    return "IPv6 Unicast";
  else if (afi == AFI_IP6 && safi == SAFI_MULTICAST)
    return "IPv6 Multicast";
  else
    return "Unknown";
}

/* Show BGP peer's information. */
enum show_type
{
  show_all,
  show_peer
};

static void
bgp_show_peer_afi_orf_cap (struct vty *vty, struct peer *p,
			   afi_t afi, safi_t safi,
			   u_int16_t adv_smcap, u_int16_t adv_rmcap,
			   u_int16_t rcv_smcap, u_int16_t rcv_rmcap)
{
  /* Send-Mode */
  if (CHECK_FLAG (p->af_cap[afi][safi], adv_smcap)
      || CHECK_FLAG (p->af_cap[afi][safi], rcv_smcap))
    {
      vty_out (vty, "      Send-mode: ");
      if (CHECK_FLAG (p->af_cap[afi][safi], adv_smcap))
	vty_out (vty, "advertised");
      if (CHECK_FLAG (p->af_cap[afi][safi], rcv_smcap))
	vty_out (vty, "%sreceived",
		 CHECK_FLAG (p->af_cap[afi][safi], adv_smcap) ?
		 ", " : "");
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  /* Receive-Mode */
  if (CHECK_FLAG (p->af_cap[afi][safi], adv_rmcap)
      || CHECK_FLAG (p->af_cap[afi][safi], rcv_rmcap))
    {
      vty_out (vty, "      Receive-mode: ");
      if (CHECK_FLAG (p->af_cap[afi][safi], adv_rmcap))
	vty_out (vty, "advertised");
      if (CHECK_FLAG (p->af_cap[afi][safi], rcv_rmcap))
	vty_out (vty, "%sreceived",
		 CHECK_FLAG (p->af_cap[afi][safi], adv_rmcap) ?
		 ", " : "");
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

static void
bgp_show_peer_afi (struct vty *vty, struct peer *p, afi_t afi, safi_t safi)
{
  struct bgp_filter *filter;
  char orf_pfx_name[BUFSIZ];
  int orf_pfx_count;

  filter = &p->filter[afi][safi];

  vty_out (vty, " For address family: %s%s", afi_safi_print (afi, safi),
	   VTY_NEWLINE);

  if (p->af_group[afi][safi])
    vty_out (vty, "  %s peer-group member%s", p->group->name, VTY_NEWLINE);

  if (CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_RCV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_OLD_RCV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_RCV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_OLD_RCV))
    vty_out (vty, "  AF-dependant capabilities:%s", VTY_NEWLINE);

  if (CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_RCV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_RCV))
    {
      vty_out (vty, "    Outbound Route Filter (ORF) type (%d) Prefix-list:%s",
	       ORF_TYPE_PREFIX, VTY_NEWLINE);
      bgp_show_peer_afi_orf_cap (vty, p, afi, safi,
				 PEER_CAP_ORF_PREFIX_SM_ADV,
				 PEER_CAP_ORF_PREFIX_RM_ADV,
				 PEER_CAP_ORF_PREFIX_SM_RCV,
				 PEER_CAP_ORF_PREFIX_RM_RCV);
    }
  if (CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_OLD_RCV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_ADV)
      || CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_OLD_RCV))
    {
      vty_out (vty, "    Outbound Route Filter (ORF) type (%d) Prefix-list:%s",
	       ORF_TYPE_PREFIX_OLD, VTY_NEWLINE);
      bgp_show_peer_afi_orf_cap (vty, p, afi, safi,
				 PEER_CAP_ORF_PREFIX_SM_ADV,
				 PEER_CAP_ORF_PREFIX_RM_ADV,
				 PEER_CAP_ORF_PREFIX_SM_OLD_RCV,
				 PEER_CAP_ORF_PREFIX_RM_OLD_RCV);
    }

  sprintf (orf_pfx_name, "%s.%d.%d", p->host, afi, safi);
  orf_pfx_count =  prefix_bgp_show_prefix_list (NULL, afi, orf_pfx_name);

  if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND)
      || orf_pfx_count)
    {
      vty_out (vty, "  Outbound Route Filter (ORF):");
      if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND))
	  vty_out (vty, " sent;");
      if (orf_pfx_count)
	vty_out (vty, " received (%d entries)", orf_pfx_count);
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_ORF_WAIT_REFRESH))
      vty_out (vty, "  First update is deferred until ORF or ROUTE-REFRESH is received%s", VTY_NEWLINE);

  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_REFLECTOR_CLIENT))
    vty_out (vty, "  Route-Reflector Client%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT))
    vty_out (vty, "  Route-Server Client%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SOFT_RECONFIG))
    vty_out (vty, "  Inbound soft reconfiguration allowed%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_REMOVE_PRIVATE_AS))
    vty_out (vty, "  Private AS number removed from updates to this neighbor%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_NEXTHOP_SELF))
    vty_out (vty, "  NEXT_HOP is always this router%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_AS_PATH_UNCHANGED))
    vty_out (vty, "  AS_PATH is propagated unchanged to this neighbor%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_NEXTHOP_UNCHANGED))
    vty_out (vty, "  NEXT_HOP is propagated unchanged to this neighbor%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_MED_UNCHANGED))
    vty_out (vty, "  MED is propagated unchanged to this neighbor%s", VTY_NEWLINE);
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SEND_COMMUNITY)
      || CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY))
    {
      vty_out (vty, "  Community attribute sent to this neighbor");
      if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SEND_COMMUNITY)
	&& CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY))
	vty_out (vty, "(both)%s", VTY_NEWLINE);
      else if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY))
	vty_out (vty, "(extended)%s", VTY_NEWLINE);
      else 
	vty_out (vty, "(standard)%s", VTY_NEWLINE);
    }
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE))
    {
      vty_out (vty, "  Default information originate,");

      if (p->default_rmap[afi][safi].name)
	vty_out (vty, " default route-map %s%s,",
		 p->default_rmap[afi][safi].map ? "*" : "",
		 p->default_rmap[afi][safi].name);
      if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_DEFAULT_ORIGINATE))
	vty_out (vty, " default sent%s", VTY_NEWLINE);
      else
	vty_out (vty, " default not sent%s", VTY_NEWLINE);
    }

  if (filter->plist[FILTER_IN].name
      || filter->dlist[FILTER_IN].name
      || filter->aslist[FILTER_IN].name
      || filter->map[RMAP_IN].name)
    vty_out (vty, "  Inbound path policy configured%s", VTY_NEWLINE);
  if (filter->plist[FILTER_OUT].name
      || filter->dlist[FILTER_OUT].name
      || filter->aslist[FILTER_OUT].name
      || filter->map[RMAP_OUT].name
      || filter->usmap.name)
    vty_out (vty, "  Outbound path policy configured%s", VTY_NEWLINE);
  if (filter->map[RMAP_IMPORT].name)
    vty_out (vty, "  Import policy for this RS-client configured%s", VTY_NEWLINE);
  if (filter->map[RMAP_EXPORT].name)
    vty_out (vty, "  Export policy for this RS-client configured%s", VTY_NEWLINE);

  /* prefix-list */
  if (filter->plist[FILTER_IN].name)
    vty_out (vty, "  Incoming update prefix filter list is %s%s%s",
	     filter->plist[FILTER_IN].plist ? "*" : "",
	     filter->plist[FILTER_IN].name,
	     VTY_NEWLINE);
  if (filter->plist[FILTER_OUT].name)
    vty_out (vty, "  Outgoing update prefix filter list is %s%s%s",
	     filter->plist[FILTER_OUT].plist ? "*" : "",
	     filter->plist[FILTER_OUT].name,
	     VTY_NEWLINE);

  /* distribute-list */
  if (filter->dlist[FILTER_IN].name)
    vty_out (vty, "  Incoming update network filter list is %s%s%s",
	     filter->dlist[FILTER_IN].alist ? "*" : "",
	     filter->dlist[FILTER_IN].name,
	     VTY_NEWLINE);
  if (filter->dlist[FILTER_OUT].name)
    vty_out (vty, "  Outgoing update network filter list is %s%s%s",
	     filter->dlist[FILTER_OUT].alist ? "*" : "",
	     filter->dlist[FILTER_OUT].name,
	     VTY_NEWLINE);

  /* filter-list. */
  if (filter->aslist[FILTER_IN].name)
    vty_out (vty, "  Incoming update AS path filter list is %s%s%s",
	     filter->aslist[FILTER_IN].aslist ? "*" : "",
	     filter->aslist[FILTER_IN].name,
	     VTY_NEWLINE);
  if (filter->aslist[FILTER_OUT].name)
    vty_out (vty, "  Outgoing update AS path filter list is %s%s%s",
	     filter->aslist[FILTER_OUT].aslist ? "*" : "",
	     filter->aslist[FILTER_OUT].name,
	     VTY_NEWLINE);

  /* route-map. */
  if (filter->map[RMAP_IN].name)
    vty_out (vty, "  Route map for incoming advertisements is %s%s%s",
            filter->map[RMAP_IN].map ? "*" : "",
            filter->map[RMAP_IN].name,
	     VTY_NEWLINE);
  if (filter->map[RMAP_OUT].name)
    vty_out (vty, "  Route map for outgoing advertisements is %s%s%s",
            filter->map[RMAP_OUT].map ? "*" : "",
            filter->map[RMAP_OUT].name,
            VTY_NEWLINE);
  if (filter->map[RMAP_IMPORT].name)
    vty_out (vty, "  Route map for advertisements going into this RS-client's table is %s%s%s",
            filter->map[RMAP_IMPORT].map ? "*" : "",
            filter->map[RMAP_IMPORT].name,
            VTY_NEWLINE);
  if (filter->map[RMAP_EXPORT].name)
    vty_out (vty, "  Route map for advertisements coming from this RS-client is %s%s%s",
            filter->map[RMAP_EXPORT].map ? "*" : "",
            filter->map[RMAP_EXPORT].name,
	     VTY_NEWLINE);

  /* unsuppress-map */
  if (filter->usmap.name)
    vty_out (vty, "  Route map for selective unsuppress is %s%s%s",
	     filter->usmap.map ? "*" : "",
	     filter->usmap.name, VTY_NEWLINE);

  /* Receive prefix count */
  vty_out (vty, "  %ld accepted prefixes%s", p->pcount[afi][safi], VTY_NEWLINE);

  /* Maximum prefix */
  if (CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX))
    {
      vty_out (vty, "  Maximum prefixes allowed %ld%s%s", p->pmax[afi][safi],
	       CHECK_FLAG (p->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING)
	       ? " (warning-only)" : "", VTY_NEWLINE);
      vty_out (vty, "  Threshold for warning message %d%%",
	       p->pmax_threshold[afi][safi]);
      if (p->pmax_restart[afi][safi])
	vty_out (vty, ", restart interval %d min", p->pmax_restart[afi][safi]);
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  vty_out (vty, "%s", VTY_NEWLINE);
}

static void
bgp_show_peer (struct vty *vty, struct peer *p)
{
  struct bgp *bgp;
  char buf1[BUFSIZ];
  char timebuf[BGP_UPTIME_LEN];
  afi_t afi;
  safi_t safi;

  bgp = p->bgp;

  /* Configured IP address. */
  vty_out (vty, "BGP neighbor is %s, ", p->host);
  vty_out (vty, "remote AS %u, ", p->as);
  vty_out (vty, "local AS %u%s%s, ",
	   p->change_local_as ? p->change_local_as : p->local_as,
	   CHECK_FLAG (p->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND) ?
	   " no-prepend" : "",
	   CHECK_FLAG (p->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS) ?
	   " replace-as" : "");
  vty_out (vty, "%s link%s",
	   p->as == p->local_as ? "internal" : "external",
	   VTY_NEWLINE);

  /* Description. */
  if (p->desc)
    vty_out (vty, " Description: %s%s", p->desc, VTY_NEWLINE);
  
  /* Peer-group */
  if (p->group)
    vty_out (vty, " Member of peer-group %s for session parameters%s",
	     p->group->name, VTY_NEWLINE);

  /* Administrative shutdown. */
  if (CHECK_FLAG (p->flags, PEER_FLAG_SHUTDOWN))
    vty_out (vty, " Administratively shut down%s", VTY_NEWLINE);

  /* BGP Version. */
  vty_out (vty, "  BGP version 4");
  vty_out (vty, ", remote router ID %s%s", 
	   inet_ntop (AF_INET, &p->remote_id, buf1, BUFSIZ),
	   VTY_NEWLINE);

  /* Confederation */
  if (CHECK_FLAG (bgp->config, BGP_CONFIG_CONFEDERATION)
      && bgp_confederation_peers_check (bgp, p->as))
    vty_out (vty, "  Neighbor under common administration%s", VTY_NEWLINE);
  
  /* Status. */
  vty_out (vty, "  BGP state = %s",  
	   LOOKUP (bgp_status_msg, p->status));
  if (p->status == Established) 
    vty_out (vty, ", up for %8s", 
	     peer_uptime (p->uptime, timebuf, BGP_UPTIME_LEN));
  else if (p->status == Active)
    {
      if (CHECK_FLAG (p->flags, PEER_FLAG_PASSIVE))
	vty_out (vty, " (passive)"); 
      else if (CHECK_FLAG (p->sflags, PEER_STATUS_NSF_WAIT))
	vty_out (vty, " (NSF passive)"); 
    }
  vty_out (vty, "%s", VTY_NEWLINE);
  
  /* read timer */
  vty_out (vty, "  Last read %s", peer_uptime (p->readtime, timebuf, BGP_UPTIME_LEN));

  /* Configured timer values. */
  vty_out (vty, ", hold time is %d, keepalive interval is %d seconds%s",
	   p->v_holdtime, p->v_keepalive, VTY_NEWLINE);
  if (CHECK_FLAG (p->config, PEER_CONFIG_TIMER))
    {
      vty_out (vty, "  Configured hold time is %d", p->holdtime);
      vty_out (vty, ", keepalive interval is %d seconds%s",
	       p->keepalive, VTY_NEWLINE);
    }

  /* Capability. */
  if (p->status == Established) 
    {
      if (p->cap
	  || p->afc_adv[AFI_IP][SAFI_UNICAST]
	  || p->afc_recv[AFI_IP][SAFI_UNICAST]
	  || p->afc_adv[AFI_IP][SAFI_MULTICAST]
	  || p->afc_recv[AFI_IP][SAFI_MULTICAST]
#ifdef HAVE_IPV6
	  || p->afc_adv[AFI_IP6][SAFI_UNICAST]
	  || p->afc_recv[AFI_IP6][SAFI_UNICAST]
	  || p->afc_adv[AFI_IP6][SAFI_MULTICAST]
	  || p->afc_recv[AFI_IP6][SAFI_MULTICAST]
#endif /* HAVE_IPV6 */
	  || p->afc_adv[AFI_IP][SAFI_MPLS_VPN]
	  || p->afc_recv[AFI_IP][SAFI_MPLS_VPN])
	{
	  vty_out (vty, "  Neighbor capabilities:%s", VTY_NEWLINE);

	  /* AS4 */
	  if (CHECK_FLAG (p->cap, PEER_CAP_AS4_RCV)
	      || CHECK_FLAG (p->cap, PEER_CAP_AS4_ADV))
	    {
	      vty_out (vty, "    4 Byte AS:");
	      if (CHECK_FLAG (p->cap, PEER_CAP_AS4_ADV))
		vty_out (vty, " advertised");
	      if (CHECK_FLAG (p->cap, PEER_CAP_AS4_RCV))
		vty_out (vty, " %sreceived",
			 CHECK_FLAG (p->cap, PEER_CAP_AS4_ADV) ? "and " : "");
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }
	  /* Dynamic */
	  if (CHECK_FLAG (p->cap, PEER_CAP_DYNAMIC_RCV)
	      || CHECK_FLAG (p->cap, PEER_CAP_DYNAMIC_ADV))
	    {
	      vty_out (vty, "    Dynamic:");
	      if (CHECK_FLAG (p->cap, PEER_CAP_DYNAMIC_ADV))
		vty_out (vty, " advertised");
	      if (CHECK_FLAG (p->cap, PEER_CAP_DYNAMIC_RCV))
		vty_out (vty, " %sreceived",
			 CHECK_FLAG (p->cap, PEER_CAP_DYNAMIC_ADV) ? "and " : "");
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }

	  /* Route Refresh */
	  if (CHECK_FLAG (p->cap, PEER_CAP_REFRESH_ADV)
	      || CHECK_FLAG (p->cap, PEER_CAP_REFRESH_NEW_RCV)
	      || CHECK_FLAG (p->cap, PEER_CAP_REFRESH_OLD_RCV))
	    {
	      vty_out (vty, "    Route refresh:");
 	      if (CHECK_FLAG (p->cap, PEER_CAP_REFRESH_ADV))
		vty_out (vty, " advertised");
	      if (CHECK_FLAG (p->cap, PEER_CAP_REFRESH_NEW_RCV)
		  || CHECK_FLAG (p->cap, PEER_CAP_REFRESH_OLD_RCV))
		vty_out (vty, " %sreceived(%s)",
			 CHECK_FLAG (p->cap, PEER_CAP_REFRESH_ADV) ? "and " : "",
			 (CHECK_FLAG (p->cap, PEER_CAP_REFRESH_OLD_RCV)
			  && CHECK_FLAG (p->cap, PEER_CAP_REFRESH_NEW_RCV)) ?
			 "old & new" : CHECK_FLAG (p->cap, PEER_CAP_REFRESH_OLD_RCV) ? "old" : "new");

	      vty_out (vty, "%s", VTY_NEWLINE);
	    }

	  /* Multiprotocol Extensions */
	  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
	    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
	      if (p->afc_adv[afi][safi] || p->afc_recv[afi][safi])
		{
		  vty_out (vty, "    Address family %s:", afi_safi_print (afi, safi));
		  if (p->afc_adv[afi][safi]) 
		    vty_out (vty, " advertised");
		  if (p->afc_recv[afi][safi])
		    vty_out (vty, " %sreceived", p->afc_adv[afi][safi] ? "and " : "");
		  vty_out (vty, "%s", VTY_NEWLINE);
		} 

	  /* Gracefull Restart */
	  if (CHECK_FLAG (p->cap, PEER_CAP_RESTART_RCV)
	      || CHECK_FLAG (p->cap, PEER_CAP_RESTART_ADV))
	    {
	      vty_out (vty, "    Graceful Restart Capabilty:");
	      if (CHECK_FLAG (p->cap, PEER_CAP_RESTART_ADV))
		vty_out (vty, " advertised");
	      if (CHECK_FLAG (p->cap, PEER_CAP_RESTART_RCV))
		vty_out (vty, " %sreceived",
			 CHECK_FLAG (p->cap, PEER_CAP_RESTART_ADV) ? "and " : "");
	      vty_out (vty, "%s", VTY_NEWLINE);

	      if (CHECK_FLAG (p->cap, PEER_CAP_RESTART_RCV))
		{
		  int restart_af_count = 0;

		  vty_out (vty, "      Remote Restart timer is %d seconds%s",
			   p->v_gr_restart, VTY_NEWLINE);	
		  vty_out (vty, "      Address families by peer:%s        ", VTY_NEWLINE);

		  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
		    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
		      if (CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_RESTART_AF_RCV))
			{
			  vty_out (vty, "%s%s(%s)", restart_af_count ? ", " : "",
				   afi_safi_print (afi, safi),
				   CHECK_FLAG (p->af_cap[afi][safi], PEER_CAP_RESTART_AF_PRESERVE_RCV) ?
				   "preserved" : "not preserved");
			  restart_af_count++;
			}
		  if (! restart_af_count)
		    vty_out (vty, "none");
		  vty_out (vty, "%s", VTY_NEWLINE);
		}
	    }
	}
    }

  /* graceful restart information */
  if (CHECK_FLAG (p->cap, PEER_CAP_RESTART_RCV)
      || p->t_gr_restart
      || p->t_gr_stale)
    {
      int eor_send_af_count = 0;
      int eor_receive_af_count = 0;

      vty_out (vty, "  Graceful restart informations:%s", VTY_NEWLINE);
      if (p->status == Established) 
	{
	  vty_out (vty, "    End-of-RIB send: ");
	  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
	    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
	      if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_EOR_SEND))
		{
		  vty_out (vty, "%s%s", eor_send_af_count ? ", " : "",
			   afi_safi_print (afi, safi));
		  eor_send_af_count++;
		}
	  vty_out (vty, "%s", VTY_NEWLINE);

	  vty_out (vty, "    End-of-RIB received: ");
	  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
	    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
	      if (CHECK_FLAG (p->af_sflags[afi][safi], PEER_STATUS_EOR_RECEIVED))
		{
		  vty_out (vty, "%s%s", eor_receive_af_count ? ", " : "",
			   afi_safi_print (afi, safi));
		  eor_receive_af_count++;
		}
	  vty_out (vty, "%s", VTY_NEWLINE);
	}

      if (p->t_gr_restart)
        vty_out (vty, "    The remaining time of restart timer is %ld%s",
                 thread_timer_remain_second (p->t_gr_restart), VTY_NEWLINE);
      
      if (p->t_gr_stale)
        vty_out (vty, "    The remaining time of stalepath timer is %ld%s",
                 thread_timer_remain_second (p->t_gr_stale), VTY_NEWLINE);
    }

  /* Packet counts. */
  vty_out (vty, "  Message statistics:%s", VTY_NEWLINE);
  vty_out (vty, "    Inq depth is 0%s", VTY_NEWLINE);
  vty_out (vty, "    Outq depth is %lu%s", (unsigned long) p->obuf->count, VTY_NEWLINE);
  vty_out (vty, "                         Sent       Rcvd%s", VTY_NEWLINE);
  vty_out (vty, "    Opens:         %10d %10d%s", p->open_out, p->open_in, VTY_NEWLINE);
  vty_out (vty, "    Notifications: %10d %10d%s", p->notify_out, p->notify_in, VTY_NEWLINE);
  vty_out (vty, "    Updates:       %10d %10d%s", p->update_out, p->update_in, VTY_NEWLINE);
  vty_out (vty, "    Keepalives:    %10d %10d%s", p->keepalive_out, p->keepalive_in, VTY_NEWLINE);
  vty_out (vty, "    Route Refresh: %10d %10d%s", p->refresh_out, p->refresh_in, VTY_NEWLINE);
  vty_out (vty, "    Capability:    %10d %10d%s", p->dynamic_cap_out, p->dynamic_cap_in, VTY_NEWLINE);
  vty_out (vty, "    Total:         %10d %10d%s", p->open_out + p->notify_out +
	   p->update_out + p->keepalive_out + p->refresh_out + p->dynamic_cap_out,
	   p->open_in + p->notify_in + p->update_in + p->keepalive_in + p->refresh_in +
	   p->dynamic_cap_in, VTY_NEWLINE);

  /* advertisement-interval */
  vty_out (vty, "  Minimum time between advertisement runs is %d seconds%s",
	   p->v_routeadv, VTY_NEWLINE);

  /* Update-source. */
  if (p->update_if || p->update_source)
    {
      vty_out (vty, "  Update source is ");
      if (p->update_if)
	vty_out (vty, "%s", p->update_if);
      else if (p->update_source)
	vty_out (vty, "%s",
		 sockunion2str (p->update_source, buf1, SU_ADDRSTRLEN));
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  /* Default weight */
  if (CHECK_FLAG (p->config, PEER_CONFIG_WEIGHT))
    vty_out (vty, "  Default weight %d%s", p->weight,
	     VTY_NEWLINE);

  vty_out (vty, "%s", VTY_NEWLINE);

  /* Address Family Information */
  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
      if (p->afc[afi][safi])
	bgp_show_peer_afi (vty, p, afi, safi);

  vty_out (vty, "  Connections established %d; dropped %d%s",
	   p->established, p->dropped,
	   VTY_NEWLINE);

  if (! p->dropped)
    vty_out (vty, "  Last reset never%s", VTY_NEWLINE);
  else
    vty_out (vty, "  Last reset %s, due to %s%s",
            peer_uptime (p->resettime, timebuf, BGP_UPTIME_LEN),
            peer_down_str[(int) p->last_reset], VTY_NEWLINE);

  if (CHECK_FLAG (p->sflags, PEER_STATUS_PREFIX_OVERFLOW))
    {
      vty_out (vty, "  Peer had exceeded the max. no. of prefixes configured.%s", VTY_NEWLINE);

      if (p->t_pmax_restart)
	vty_out (vty, "  Reduce the no. of prefix from %s, will restart in %ld seconds%s",
		 p->host, thread_timer_remain_second (p->t_pmax_restart),
		 VTY_NEWLINE);
      else
	vty_out (vty, "  Reduce the no. of prefix and clear ip bgp %s to restore peering%s",
		 p->host, VTY_NEWLINE);
    }

  /* EBGP Multihop and GTSM */
  if (p->sort != BGP_PEER_IBGP)
    {
      if (p->gtsm_hops > 0)
	vty_out (vty, "  External BGP neighbor may be up to %d hops away.%s",
		 p->gtsm_hops, VTY_NEWLINE);
      else if (p->ttl > 1)
	vty_out (vty, "  External BGP neighbor may be up to %d hops away.%s",
		 p->ttl, VTY_NEWLINE);
    }
  else
    {
      if (p->gtsm_hops > 0)
	vty_out (vty, "  Internal BGP neighbor may be up to %d hops away.%s",
		 p->gtsm_hops, VTY_NEWLINE);
    }

  /* Local address. */
  if (p->su_local)
    {
      vty_out (vty, "Local host: %s, Local port: %d%s",
	       sockunion2str (p->su_local, buf1, SU_ADDRSTRLEN),
	       ntohs (p->su_local->sin.sin_port),
	       VTY_NEWLINE);
    }
      
  /* Remote address. */
  if (p->su_remote)
    {
      vty_out (vty, "Foreign host: %s, Foreign port: %d%s",
	       sockunion2str (p->su_remote, buf1, SU_ADDRSTRLEN),
	       ntohs (p->su_remote->sin.sin_port),
	       VTY_NEWLINE);
    }

  /* Nexthop display. */
  if (p->su_local)
    {
      vty_out (vty, "Nexthop: %s%s", 
	       inet_ntop (AF_INET, &p->nexthop.v4, buf1, BUFSIZ),
	       VTY_NEWLINE);
#ifdef HAVE_IPV6
      vty_out (vty, "Nexthop global: %s%s", 
	       inet_ntop (AF_INET6, &p->nexthop.v6_global, buf1, BUFSIZ),
	       VTY_NEWLINE);
      vty_out (vty, "Nexthop local: %s%s",
	       inet_ntop (AF_INET6, &p->nexthop.v6_local, buf1, BUFSIZ),
	       VTY_NEWLINE);
      vty_out (vty, "BGP connection: %s%s",
	       p->shared_network ? "shared network" : "non shared network",
	       VTY_NEWLINE);
#endif /* HAVE_IPV6 */
    }

  /* Timer information. */
  if (p->t_start)
    vty_out (vty, "Next start timer due in %ld seconds%s",
	     thread_timer_remain_second (p->t_start), VTY_NEWLINE);
  if (p->t_connect)
    vty_out (vty, "Next connect timer due in %ld seconds%s",
	     thread_timer_remain_second (p->t_connect), VTY_NEWLINE);
  
  vty_out (vty, "Read thread: %s  Write thread: %s%s", 
	   p->t_read ? "on" : "off",
	   p->t_write ? "on" : "off",
	   VTY_NEWLINE);

  if (p->notify.code == BGP_NOTIFY_OPEN_ERR
      && p->notify.subcode == BGP_NOTIFY_OPEN_UNSUP_CAPBL)
    bgp_capability_vty_out (vty, p);
 
  vty_out (vty, "%s", VTY_NEWLINE);
}

static int
bgp_show_neighbor (struct vty *vty, struct bgp *bgp,
		   enum show_type type, union sockunion *su)
{
  struct listnode *node, *nnode;
  struct peer *peer;
  int find = 0;

  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      switch (type)
	{
	case show_all:
	  bgp_show_peer (vty, peer);
	  break;
	case show_peer:
	  if (sockunion_same (&peer->su, su))
	    {
	      find = 1;
	      bgp_show_peer (vty, peer);
	    }
	  break;
	}
    }

  if (type == show_peer && ! find)
    vty_out (vty, "%% No such neighbor%s", VTY_NEWLINE);
  
  return CMD_SUCCESS;
}

static int 
bgp_show_neighbor_vty (struct vty *vty, const char *name, 
                       enum show_type type, const char *ip_str)
{
  int ret;
  struct bgp *bgp;
  union sockunion su;

  if (ip_str)
    {
      ret = str2sockunion (ip_str, &su);
      if (ret < 0)
        {
          vty_out (vty, "%% Malformed address: %s%s", ip_str, VTY_NEWLINE);
          return CMD_WARNING;
        }
    }

  if (name)
    {
      bgp = bgp_lookup_by_name (name);
      
      if (! bgp)
        {
          vty_out (vty, "%% No such BGP instance exist%s", VTY_NEWLINE); 
          return CMD_WARNING;
        }

      bgp_show_neighbor (vty, bgp, type, &su);

      return CMD_SUCCESS;
    }

  bgp = bgp_get_default ();

  if (bgp)
    bgp_show_neighbor (vty, bgp, type, &su);

  return CMD_SUCCESS;
}

/* "show ip bgp neighbors" commands.  */
DEFUN (show_ip_bgp_neighbors,
       show_ip_bgp_neighbors_cmd,
       "show ip bgp neighbors",
       SHOW_STR
       IP_STR
       BGP_STR
       "Detailed information on TCP and BGP neighbor connections\n")
{
  return bgp_show_neighbor_vty (vty, NULL, show_all, NULL);
}

ALIAS (show_ip_bgp_neighbors,
       show_ip_bgp_ipv4_neighbors_cmd,
       "show ip bgp ipv4 (unicast|multicast) neighbors",
       SHOW_STR
       IP_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Detailed information on TCP and BGP neighbor connections\n")

ALIAS (show_ip_bgp_neighbors,
       show_ip_bgp_vpnv4_all_neighbors_cmd,
       "show ip bgp vpnv4 all neighbors",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Detailed information on TCP and BGP neighbor connections\n")

ALIAS (show_ip_bgp_neighbors,
       show_ip_bgp_vpnv4_rd_neighbors_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn neighbors",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information for a route distinguisher\n"
       "VPN Route Distinguisher\n"
       "Detailed information on TCP and BGP neighbor connections\n")

ALIAS (show_ip_bgp_neighbors,
       show_bgp_neighbors_cmd,
       "show bgp neighbors",
       SHOW_STR
       BGP_STR
       "Detailed information on TCP and BGP neighbor connections\n")

ALIAS (show_ip_bgp_neighbors,
       show_bgp_ipv6_neighbors_cmd,
       "show bgp ipv6 neighbors",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Detailed information on TCP and BGP neighbor connections\n")

DEFUN (show_ip_bgp_neighbors_peer,
       show_ip_bgp_neighbors_peer_cmd,
       "show ip bgp neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       IP_STR
       BGP_STR
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")
{
  return bgp_show_neighbor_vty (vty, NULL, show_peer, argv[argc - 1]);
}

ALIAS (show_ip_bgp_neighbors_peer,
       show_ip_bgp_ipv4_neighbors_peer_cmd,
       "show ip bgp ipv4 (unicast|multicast) neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       IP_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")

ALIAS (show_ip_bgp_neighbors_peer,
       show_ip_bgp_vpnv4_all_neighbors_peer_cmd,
       "show ip bgp vpnv4 all neighbors A.B.C.D",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n")

ALIAS (show_ip_bgp_neighbors_peer,
       show_ip_bgp_vpnv4_rd_neighbors_peer_cmd,
       "show ip bgp vpnv4 rd ASN:nn_or_IP-address:nn neighbors A.B.C.D",
       SHOW_STR
       IP_STR
       BGP_STR
       "Display VPNv4 NLRI specific information\n"
       "Display information about all VPNv4 NLRIs\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n")

ALIAS (show_ip_bgp_neighbors_peer,
       show_bgp_neighbors_peer_cmd,
       "show bgp neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       BGP_STR
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")

ALIAS (show_ip_bgp_neighbors_peer,
       show_bgp_ipv6_neighbors_peer_cmd,
       "show bgp ipv6 neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")

DEFUN (show_ip_bgp_instance_neighbors,
       show_ip_bgp_instance_neighbors_cmd,
       "show ip bgp view WORD neighbors",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Detailed information on TCP and BGP neighbor connections\n")
{
  return bgp_show_neighbor_vty (vty, argv[0], show_all, NULL);
}

ALIAS (show_ip_bgp_instance_neighbors,
       show_bgp_instance_neighbors_cmd,
       "show bgp view WORD neighbors",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Detailed information on TCP and BGP neighbor connections\n")

ALIAS (show_ip_bgp_instance_neighbors,
       show_bgp_instance_ipv6_neighbors_cmd,
       "show bgp view WORD ipv6 neighbors",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Detailed information on TCP and BGP neighbor connections\n")

DEFUN (show_ip_bgp_instance_neighbors_peer,
       show_ip_bgp_instance_neighbors_peer_cmd,
       "show ip bgp view WORD neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")
{
  return bgp_show_neighbor_vty (vty, argv[0], show_peer, argv[1]);
}

ALIAS (show_ip_bgp_instance_neighbors_peer,
       show_bgp_instance_neighbors_peer_cmd,
       "show bgp view WORD neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")

ALIAS (show_ip_bgp_instance_neighbors_peer,
       show_bgp_instance_ipv6_neighbors_peer_cmd,
       "show bgp view WORD ipv6 neighbors (A.B.C.D|X:X::X:X)",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Detailed information on TCP and BGP neighbor connections\n"
       "Neighbor to display information about\n"
       "Neighbor to display information about\n")
       
/* Show BGP's AS paths internal data.  There are both `show ip bgp
   paths' and `show ip mbgp paths'.  Those functions results are the
   same.*/
DEFUN (show_ip_bgp_paths, 
       show_ip_bgp_paths_cmd,
       "show ip bgp paths",
       SHOW_STR
       IP_STR
       BGP_STR
       "Path information\n")
{
  vty_out (vty, "Address Refcnt Path%s", VTY_NEWLINE);
  aspath_print_all_vty (vty);
  return CMD_SUCCESS;
}

DEFUN (show_ip_bgp_ipv4_paths, 
       show_ip_bgp_ipv4_paths_cmd,
       "show ip bgp ipv4 (unicast|multicast) paths",
       SHOW_STR
       IP_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Path information\n")
{
  vty_out (vty, "Address Refcnt Path\r\n");
  aspath_print_all_vty (vty);

  return CMD_SUCCESS;
}

#include "hash.h"

static void
community_show_all_iterator (struct hash_backet *backet, struct vty *vty)
{
  struct community *com;

  com = (struct community *) backet->data;
  vty_out (vty, "[%p] (%ld) %s%s", backet, com->refcnt,
	   community_str (com), VTY_NEWLINE);
}

/* Show BGP's community internal data. */
DEFUN (show_ip_bgp_community_info, 
       show_ip_bgp_community_info_cmd,
       "show ip bgp community-info",
       SHOW_STR
       IP_STR
       BGP_STR
       "List all bgp community information\n")
{
  vty_out (vty, "Address Refcnt Community%s", VTY_NEWLINE);

  hash_iterate (community_hash (), 
		(void (*) (struct hash_backet *, void *))
		community_show_all_iterator,
		vty);

  return CMD_SUCCESS;
}

DEFUN (show_ip_bgp_attr_info, 
       show_ip_bgp_attr_info_cmd,
       "show ip bgp attribute-info",
       SHOW_STR
       IP_STR
       BGP_STR
       "List all bgp attribute information\n")
{
  attr_show_all (vty);
  return CMD_SUCCESS;
}

static int
bgp_write_rsclient_summary (struct vty *vty, struct peer *rsclient,
        afi_t afi, safi_t safi)
{
  char timebuf[BGP_UPTIME_LEN];
  char rmbuf[14];
  const char *rmname;
  struct peer *peer;
  struct listnode *node, *nnode;
  int len;
  int count = 0;

  if (CHECK_FLAG (rsclient->sflags, PEER_STATUS_GROUP))
    {
      for (ALL_LIST_ELEMENTS (rsclient->group->peer, node, nnode, peer))
        {
          count++;
          bgp_write_rsclient_summary (vty, peer, afi, safi);
        }
      return count;
    }

  len = vty_out (vty, "%s", rsclient->host);
  len = 16 - len;

  if (len < 1)
    vty_out (vty, "%s%*s", VTY_NEWLINE, 16, " ");
  else
    vty_out (vty, "%*s", len, " ");

  vty_out (vty, "4 ");

  vty_out (vty, "%10u ", rsclient->as);

  rmname = ROUTE_MAP_EXPORT_NAME(&rsclient->filter[afi][safi]);
  if ( rmname && strlen (rmname) > 13 )
    {
      sprintf (rmbuf, "%13s", "...");
      rmname = strncpy (rmbuf, rmname, 10);
    }
  else if (! rmname)
    rmname = "<none>";
  vty_out (vty, " %13s ", rmname);

  rmname = ROUTE_MAP_IMPORT_NAME(&rsclient->filter[afi][safi]);
  if ( rmname && strlen (rmname) > 13 )
    {
      sprintf (rmbuf, "%13s", "...");
      rmname = strncpy (rmbuf, rmname, 10);
    }
  else if (! rmname)
    rmname = "<none>";
  vty_out (vty, " %13s ", rmname);

  vty_out (vty, "%8s", peer_uptime (rsclient->uptime, timebuf, BGP_UPTIME_LEN));

  if (CHECK_FLAG (rsclient->flags, PEER_FLAG_SHUTDOWN))
    vty_out (vty, " Idle (Admin)");
  else if (CHECK_FLAG (rsclient->sflags, PEER_STATUS_PREFIX_OVERFLOW))
    vty_out (vty, " Idle (PfxCt)");
  else
    vty_out (vty, " %-11s", LOOKUP(bgp_status_msg, rsclient->status));

  vty_out (vty, "%s", VTY_NEWLINE);

  return 1;
}

static int
bgp_show_rsclient_summary (struct vty *vty, struct bgp *bgp, 
                           afi_t afi, safi_t safi)
{
  struct peer *peer;
  struct listnode *node, *nnode;
  int count = 0;

  /* Header string for each address family. */
  static char header[] = "Neighbor        V         AS  Export-Policy  Import-Policy  Up/Down  State";

  for (ALL_LIST_ELEMENTS (bgp->rsclient, node, nnode, peer))
    {
      if (peer->afc[afi][safi] &&
         CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT))
       {
         if (! count)
           {
             vty_out (vty,
                      "Route Server's BGP router identifier %s%s",
                      inet_ntoa (bgp->router_id), VTY_NEWLINE);
             vty_out (vty,
              "Route Server's local AS number %u%s", bgp->as,
                       VTY_NEWLINE);

             vty_out (vty, "%s", VTY_NEWLINE);
             vty_out (vty, "%s%s", header, VTY_NEWLINE);
           }

         count += bgp_write_rsclient_summary (vty, peer, afi, safi);
       }
    }

  if (count)
    vty_out (vty, "%sTotal number of Route Server Clients %d%s", VTY_NEWLINE,
            count, VTY_NEWLINE);
  else
    vty_out (vty, "No %s Route Server Client is configured%s",
            afi == AFI_IP ? "IPv4" : "IPv6", VTY_NEWLINE);

  return CMD_SUCCESS;
}

static int
bgp_show_rsclient_summary_vty (struct vty *vty, const char *name, 
                               afi_t afi, safi_t safi)
{
  struct bgp *bgp;

  if (name)
    {
      bgp = bgp_lookup_by_name (name);

      if (! bgp)
       {
         vty_out (vty, "%% No such BGP instance exist%s", VTY_NEWLINE);
         return CMD_WARNING;
       }

      bgp_show_rsclient_summary (vty, bgp, afi, safi);
      return CMD_SUCCESS;
    }

  bgp = bgp_get_default ();

  if (bgp)
    bgp_show_rsclient_summary (vty, bgp, afi, safi);

  return CMD_SUCCESS;
}

/* 'show bgp rsclient' commands. */
DEFUN (show_ip_bgp_rsclient_summary,
       show_ip_bgp_rsclient_summary_cmd,
       "show ip bgp rsclient summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP, SAFI_UNICAST);
}

DEFUN (show_ip_bgp_instance_rsclient_summary,
       show_ip_bgp_instance_rsclient_summary_cmd,
       "show ip bgp view WORD rsclient summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP, SAFI_UNICAST);
}

DEFUN (show_ip_bgp_ipv4_rsclient_summary,
      show_ip_bgp_ipv4_rsclient_summary_cmd,
      "show ip bgp ipv4 (unicast|multicast) rsclient summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  if (strncmp (argv[0], "m", 1) == 0)
    return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP, SAFI_MULTICAST);

  return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP, SAFI_UNICAST);
}

DEFUN (show_ip_bgp_instance_ipv4_rsclient_summary,
      show_ip_bgp_instance_ipv4_rsclient_summary_cmd,
      "show ip bgp view WORD ipv4 (unicast|multicast) rsclient summary",
       SHOW_STR
       IP_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  if (strncmp (argv[1], "m", 1) == 0)
    return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP, SAFI_MULTICAST);

  return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP, SAFI_UNICAST);
}

DEFUN (show_bgp_instance_ipv4_safi_rsclient_summary,
       show_bgp_instance_ipv4_safi_rsclient_summary_cmd,
       "show bgp view WORD ipv4 (unicast|multicast) rsclient summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  safi_t safi;

  if (argc == 2) {
    safi = (strncmp (argv[1], "m", 1) == 0) ? SAFI_MULTICAST : SAFI_UNICAST;
    return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP, safi);
  } else {
    safi = (strncmp (argv[0], "m", 1) == 0) ? SAFI_MULTICAST : SAFI_UNICAST;
    return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP, safi);
  }
}

ALIAS (show_bgp_instance_ipv4_safi_rsclient_summary,
       show_bgp_ipv4_safi_rsclient_summary_cmd,
       "show bgp ipv4 (unicast|multicast) rsclient summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")

#ifdef HAVE_IPV6
DEFUN (show_bgp_rsclient_summary,
       show_bgp_rsclient_summary_cmd,
       "show bgp rsclient summary",
       SHOW_STR
       BGP_STR
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP6, SAFI_UNICAST);
}

DEFUN (show_bgp_instance_rsclient_summary,
       show_bgp_instance_rsclient_summary_cmd,
       "show bgp view WORD rsclient summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP6, SAFI_UNICAST);
}

ALIAS (show_bgp_rsclient_summary,
      show_bgp_ipv6_rsclient_summary_cmd,
      "show bgp ipv6 rsclient summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")

ALIAS (show_bgp_instance_rsclient_summary,
      show_bgp_instance_ipv6_rsclient_summary_cmd,
       "show bgp view WORD ipv6 rsclient summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")

DEFUN (show_bgp_instance_ipv6_safi_rsclient_summary,
       show_bgp_instance_ipv6_safi_rsclient_summary_cmd,
       "show bgp view WORD ipv6 (unicast|multicast) rsclient summary",
       SHOW_STR
       BGP_STR
       "BGP view\n"
       "View name\n"
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")
{
  safi_t safi;

  if (argc == 2) {
    safi = (strncmp (argv[1], "m", 1) == 0) ? SAFI_MULTICAST : SAFI_UNICAST;
    return bgp_show_rsclient_summary_vty (vty, argv[0], AFI_IP6, safi);
  } else {
    safi = (strncmp (argv[0], "m", 1) == 0) ? SAFI_MULTICAST : SAFI_UNICAST;
    return bgp_show_rsclient_summary_vty (vty, NULL, AFI_IP6, safi);
  }
}

ALIAS (show_bgp_instance_ipv6_safi_rsclient_summary,
       show_bgp_ipv6_safi_rsclient_summary_cmd,
       "show bgp ipv6 (unicast|multicast) rsclient summary",
       SHOW_STR
       BGP_STR
       "Address family\n"
       "Address Family modifier\n"
       "Address Family modifier\n"
       "Information about Route Server Clients\n"
       "Summary of all Route Server Clients\n")

#endif /* HAVE IPV6 */

/* Redistribute VTY commands.  */

DEFUN (bgp_redistribute_ipv4,
       bgp_redistribute_ipv4_cmd,
       "redistribute " QUAGGA_IP_REDIST_STR_BGPD,
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD)
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return bgp_redistribute_set (vty->index, AFI_IP, type);
}

DEFUN (bgp_redistribute_ipv4_rmap,
       bgp_redistribute_ipv4_rmap_cmd,
       "redistribute " QUAGGA_IP_REDIST_STR_BGPD " route-map WORD",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_rmap_set (vty->index, AFI_IP, type, argv[1]);
  return bgp_redistribute_set (vty->index, AFI_IP, type);
}

DEFUN (bgp_redistribute_ipv4_metric,
       bgp_redistribute_ipv4_metric_cmd,
       "redistribute " QUAGGA_IP_REDIST_STR_BGPD " metric <0-4294967295>",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[1]);

  bgp_redistribute_metric_set (vty->index, AFI_IP, type, metric);
  return bgp_redistribute_set (vty->index, AFI_IP, type);
}

DEFUN (bgp_redistribute_ipv4_rmap_metric,
       bgp_redistribute_ipv4_rmap_metric_cmd,
       "redistribute " QUAGGA_IP_REDIST_STR_BGPD " route-map WORD metric <0-4294967295>",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n"
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[2]);

  bgp_redistribute_rmap_set (vty->index, AFI_IP, type, argv[1]);
  bgp_redistribute_metric_set (vty->index, AFI_IP, type, metric);
  return bgp_redistribute_set (vty->index, AFI_IP, type);
}

DEFUN (bgp_redistribute_ipv4_metric_rmap,
       bgp_redistribute_ipv4_metric_rmap_cmd,
       "redistribute " QUAGGA_IP_REDIST_STR_BGPD " metric <0-4294967295> route-map WORD",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[1]);

  bgp_redistribute_metric_set (vty->index, AFI_IP, type, metric);
  bgp_redistribute_rmap_set (vty->index, AFI_IP, type, argv[2]);
  return bgp_redistribute_set (vty->index, AFI_IP, type);
}

DEFUN (no_bgp_redistribute_ipv4,
       no_bgp_redistribute_ipv4_cmd,
       "no redistribute " QUAGGA_IP_REDIST_STR_BGPD,
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD)
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_redistribute_unset (vty->index, AFI_IP, type);
}

DEFUN (no_bgp_redistribute_ipv4_rmap,
       no_bgp_redistribute_ipv4_rmap_cmd,
       "no redistribute " QUAGGA_IP_REDIST_STR_BGPD " route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_routemap_unset (vty->index, AFI_IP, type);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_redistribute_ipv4_metric,
       no_bgp_redistribute_ipv4_metric_cmd,
       "no redistribute " QUAGGA_IP_REDIST_STR_BGPD " metric <0-4294967295>",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_metric_unset (vty->index, AFI_IP, type);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_redistribute_ipv4_rmap_metric,
       no_bgp_redistribute_ipv4_rmap_metric_cmd,
       "no redistribute " QUAGGA_IP_REDIST_STR_BGPD " route-map WORD metric <0-4294967295>",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n"
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;

  type = proto_redistnum (AFI_IP, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_metric_unset (vty->index, AFI_IP, type);
  bgp_redistribute_routemap_unset (vty->index, AFI_IP, type);
  return CMD_SUCCESS;
}

ALIAS (no_bgp_redistribute_ipv4_rmap_metric,
       no_bgp_redistribute_ipv4_metric_rmap_cmd,
       "no redistribute " QUAGGA_IP_REDIST_STR_BGPD " metric <0-4294967295> route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")

#ifdef HAVE_IPV6
DEFUN (bgp_redistribute_ipv6,
       bgp_redistribute_ipv6_cmd,
       "redistribute " QUAGGA_IP6_REDIST_STR_BGPD,
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD)
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_redistribute_set (vty->index, AFI_IP6, type);
}

DEFUN (bgp_redistribute_ipv6_rmap,
       bgp_redistribute_ipv6_rmap_cmd,
       "redistribute " QUAGGA_IP6_REDIST_STR_BGPD " route-map WORD",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_rmap_set (vty->index, AFI_IP6, type, argv[1]);
  return bgp_redistribute_set (vty->index, AFI_IP6, type);
}

DEFUN (bgp_redistribute_ipv6_metric,
       bgp_redistribute_ipv6_metric_cmd,
       "redistribute " QUAGGA_IP6_REDIST_STR_BGPD " metric <0-4294967295>",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[1]);

  bgp_redistribute_metric_set (vty->index, AFI_IP6, type, metric);
  return bgp_redistribute_set (vty->index, AFI_IP6, type);
}

DEFUN (bgp_redistribute_ipv6_rmap_metric,
       bgp_redistribute_ipv6_rmap_metric_cmd,
       "redistribute " QUAGGA_IP6_REDIST_STR_BGPD " route-map WORD metric <0-4294967295>",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n"
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[2]);

  bgp_redistribute_rmap_set (vty->index, AFI_IP6, type, argv[1]);
  bgp_redistribute_metric_set (vty->index, AFI_IP6, type, metric);
  return bgp_redistribute_set (vty->index, AFI_IP6, type);
}

DEFUN (bgp_redistribute_ipv6_metric_rmap,
       bgp_redistribute_ipv6_metric_rmap_cmd,
       "redistribute " QUAGGA_IP6_REDIST_STR_BGPD " metric <0-4294967295> route-map WORD",
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;
  u_int32_t metric;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  VTY_GET_INTEGER ("metric", metric, argv[1]);

  bgp_redistribute_metric_set (vty->index, AFI_IP6, type, metric);
  bgp_redistribute_rmap_set (vty->index, AFI_IP6, type, argv[2]);
  return bgp_redistribute_set (vty->index, AFI_IP6, type);
}

DEFUN (no_bgp_redistribute_ipv6,
       no_bgp_redistribute_ipv6_cmd,
       "no redistribute " QUAGGA_IP6_REDIST_STR_BGPD,
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD)
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return bgp_redistribute_unset (vty->index, AFI_IP6, type);
}

DEFUN (no_bgp_redistribute_ipv6_rmap,
       no_bgp_redistribute_ipv6_rmap_cmd,
       "no redistribute " QUAGGA_IP6_REDIST_STR_BGPD " route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_routemap_unset (vty->index, AFI_IP6, type);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_redistribute_ipv6_metric,
       no_bgp_redistribute_ipv6_metric_cmd,
       "no redistribute " QUAGGA_IP6_REDIST_STR_BGPD " metric <0-4294967295>",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_metric_unset (vty->index, AFI_IP6, type);
  return CMD_SUCCESS;
}

DEFUN (no_bgp_redistribute_ipv6_rmap_metric,
       no_bgp_redistribute_ipv6_rmap_metric_cmd,
       "no redistribute " QUAGGA_IP6_REDIST_STR_BGPD " route-map WORD metric <0-4294967295>",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Route map reference\n"
       "Pointer to route-map entries\n"
       "Metric for redistributed routes\n"
       "Default metric\n")
{
  int type;

  type = proto_redistnum (AFI_IP6, argv[0]);
  if (type < 0 || type == ZEBRA_ROUTE_BGP)
    {
      vty_out (vty, "%% Invalid route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  bgp_redistribute_metric_unset (vty->index, AFI_IP6, type);
  bgp_redistribute_routemap_unset (vty->index, AFI_IP6, type);
  return CMD_SUCCESS;
}

ALIAS (no_bgp_redistribute_ipv6_rmap_metric,
       no_bgp_redistribute_ipv6_metric_rmap_cmd,
       "no redistribute " QUAGGA_IP6_REDIST_STR_BGPD " metric <0-4294967295> route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       QUAGGA_IP6_REDIST_HELP_STR_BGPD
       "Metric for redistributed routes\n"
       "Default metric\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
#endif /* HAVE_IPV6 */

int
bgp_config_write_redistribute (struct vty *vty, struct bgp *bgp, afi_t afi,
			       safi_t safi, int *write)
{
  int i;

  /* Unicast redistribution only.  */
  if (safi != SAFI_UNICAST)
    return 0;

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      /* Redistribute BGP does not make sense.  */
      if (bgp->redist[afi][i] && i != ZEBRA_ROUTE_BGP)
	{
	  /* Display "address-family" when it is not yet diplayed.  */
	  bgp_config_write_family_header (vty, afi, safi, write);

	  /* "redistribute" configuration.  */
	  vty_out (vty, " redistribute %s", zebra_route_string(i));

	  if (bgp->redist_metric_flag[afi][i])
	    vty_out (vty, " metric %u", bgp->redist_metric[afi][i]);

	  if (bgp->rmap[afi][i].name)
	    vty_out (vty, " route-map %s", bgp->rmap[afi][i].name);

	  vty_out (vty, "%s", VTY_NEWLINE);
	}
    }
  return *write;
}

/* BGP node structure. */
static struct cmd_node bgp_node =
{
  BGP_NODE,
  "%s(config-router)# ",
  1,
};

static struct cmd_node bgp_ipv4_unicast_node =
{
  BGP_IPV4_NODE,
  "%s(config-router-af)# ",
  1,
};

static struct cmd_node bgp_ipv4_multicast_node =
{
  BGP_IPV4M_NODE,
  "%s(config-router-af)# ",
  1,
};

static struct cmd_node bgp_ipv6_unicast_node =
{
  BGP_IPV6_NODE,
  "%s(config-router-af)# ",
  1,
};

static struct cmd_node bgp_ipv6_multicast_node =
{
  BGP_IPV6M_NODE,
  "%s(config-router-af)# ",
  1,
};

static struct cmd_node bgp_vpnv4_node =
{
  BGP_VPNV4_NODE,
  "%s(config-router-af)# ",
  1
};

static void community_list_vty (void);

void
bgp_vty_init (void)
{
  /* Install bgp top node. */
  install_node (&bgp_node, bgp_config_write);
  install_node (&bgp_ipv4_unicast_node, NULL);
  install_node (&bgp_ipv4_multicast_node, NULL);
  install_node (&bgp_ipv6_unicast_node, NULL);
  install_node (&bgp_ipv6_multicast_node, NULL);
  install_node (&bgp_vpnv4_node, NULL);

  /* Install default VTY commands to new nodes.  */
  install_default (BGP_NODE);
  install_default (BGP_IPV4_NODE);
  install_default (BGP_IPV4M_NODE);
  install_default (BGP_IPV6_NODE);
  install_default (BGP_IPV6M_NODE);
  install_default (BGP_VPNV4_NODE);
  
  /* "bgp multiple-instance" commands. */
  install_element (CONFIG_NODE, &bgp_multiple_instance_cmd);
  install_element (CONFIG_NODE, &no_bgp_multiple_instance_cmd);

  /* "bgp config-type" commands. */
  install_element (CONFIG_NODE, &bgp_config_type_cmd);
  install_element (CONFIG_NODE, &no_bgp_config_type_cmd);

  /* Dummy commands (Currently not supported) */
  install_element (BGP_NODE, &no_synchronization_cmd);
  install_element (BGP_NODE, &no_auto_summary_cmd);

  /* "router bgp" commands. */
  install_element (CONFIG_NODE, &router_bgp_cmd);
  install_element (CONFIG_NODE, &router_bgp_view_cmd);

  /* "no router bgp" commands. */
  install_element (CONFIG_NODE, &no_router_bgp_cmd);
  install_element (CONFIG_NODE, &no_router_bgp_view_cmd);

  /* "bgp router-id" commands. */
  install_element (BGP_NODE, &bgp_router_id_cmd);
  install_element (BGP_NODE, &no_bgp_router_id_cmd);
  install_element (BGP_NODE, &no_bgp_router_id_val_cmd);

  /* "bgp cluster-id" commands. */
  install_element (BGP_NODE, &bgp_cluster_id_cmd);
  install_element (BGP_NODE, &bgp_cluster_id32_cmd);
  install_element (BGP_NODE, &no_bgp_cluster_id_cmd);
  install_element (BGP_NODE, &no_bgp_cluster_id_arg_cmd);

  /* "bgp confederation" commands. */
  install_element (BGP_NODE, &bgp_confederation_identifier_cmd);
  install_element (BGP_NODE, &no_bgp_confederation_identifier_cmd);
  install_element (BGP_NODE, &no_bgp_confederation_identifier_arg_cmd);

  /* "bgp confederation peers" commands. */
  install_element (BGP_NODE, &bgp_confederation_peers_cmd);
  install_element (BGP_NODE, &no_bgp_confederation_peers_cmd);

  /* "maximum-paths" commands. */
  install_element (BGP_NODE, &bgp_maxpaths_cmd);
  install_element (BGP_NODE, &no_bgp_maxpaths_cmd);
  install_element (BGP_NODE, &no_bgp_maxpaths_arg_cmd);
  install_element (BGP_IPV4_NODE, &bgp_maxpaths_cmd);
  install_element (BGP_IPV4_NODE, &no_bgp_maxpaths_cmd);
  install_element (BGP_IPV4_NODE, &no_bgp_maxpaths_arg_cmd);
  install_element (BGP_NODE, &bgp_maxpaths_ibgp_cmd);
  install_element (BGP_NODE, &no_bgp_maxpaths_ibgp_cmd);
  install_element (BGP_NODE, &no_bgp_maxpaths_ibgp_arg_cmd);
  install_element (BGP_IPV4_NODE, &bgp_maxpaths_ibgp_cmd);
  install_element (BGP_IPV4_NODE, &no_bgp_maxpaths_ibgp_cmd);
  install_element (BGP_IPV4_NODE, &no_bgp_maxpaths_ibgp_arg_cmd);

  /* "timers bgp" commands. */
  install_element (BGP_NODE, &bgp_timers_cmd);
  install_element (BGP_NODE, &no_bgp_timers_cmd);
  install_element (BGP_NODE, &no_bgp_timers_arg_cmd);

  /* "bgp client-to-client reflection" commands */
  install_element (BGP_NODE, &no_bgp_client_to_client_reflection_cmd);
  install_element (BGP_NODE, &bgp_client_to_client_reflection_cmd);

  /* "bgp always-compare-med" commands */
  install_element (BGP_NODE, &bgp_always_compare_med_cmd);
  install_element (BGP_NODE, &no_bgp_always_compare_med_cmd);
  
  /* "bgp deterministic-med" commands */
  install_element (BGP_NODE, &bgp_deterministic_med_cmd);
  install_element (BGP_NODE, &no_bgp_deterministic_med_cmd);

  /* "bgp graceful-restart" commands */
  install_element (BGP_NODE, &bgp_graceful_restart_cmd);
  install_element (BGP_NODE, &no_bgp_graceful_restart_cmd);
  install_element (BGP_NODE, &bgp_graceful_restart_stalepath_time_cmd);
  install_element (BGP_NODE, &no_bgp_graceful_restart_stalepath_time_cmd);
  install_element (BGP_NODE, &no_bgp_graceful_restart_stalepath_time_val_cmd);
 
  /* "bgp fast-external-failover" commands */
  install_element (BGP_NODE, &bgp_fast_external_failover_cmd);
  install_element (BGP_NODE, &no_bgp_fast_external_failover_cmd);

  /* "bgp enforce-first-as" commands */
  install_element (BGP_NODE, &bgp_enforce_first_as_cmd);
  install_element (BGP_NODE, &no_bgp_enforce_first_as_cmd);

  /* "bgp bestpath compare-routerid" commands */
  install_element (BGP_NODE, &bgp_bestpath_compare_router_id_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_compare_router_id_cmd);

  /* "bgp bestpath as-path ignore" commands */
  install_element (BGP_NODE, &bgp_bestpath_aspath_ignore_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_aspath_ignore_cmd);

  /* "bgp bestpath as-path confed" commands */
  install_element (BGP_NODE, &bgp_bestpath_aspath_confed_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_aspath_confed_cmd);

  /* "bgp bestpath as-path multipath-relax" commands */
  install_element (BGP_NODE, &bgp_bestpath_aspath_multipath_relax_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_aspath_multipath_relax_cmd);

  /* "bgp log-neighbor-changes" commands */
  install_element (BGP_NODE, &bgp_log_neighbor_changes_cmd);
  install_element (BGP_NODE, &no_bgp_log_neighbor_changes_cmd);

  /* "bgp bestpath med" commands */
  install_element (BGP_NODE, &bgp_bestpath_med_cmd);
  install_element (BGP_NODE, &bgp_bestpath_med2_cmd);
  install_element (BGP_NODE, &bgp_bestpath_med3_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_med_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_med2_cmd);
  install_element (BGP_NODE, &no_bgp_bestpath_med3_cmd);

  /* "no bgp default ipv4-unicast" commands. */
  install_element (BGP_NODE, &no_bgp_default_ipv4_unicast_cmd);
  install_element (BGP_NODE, &bgp_default_ipv4_unicast_cmd);
  
  /* "bgp network import-check" commands. */
  install_element (BGP_NODE, &bgp_network_import_check_cmd);
  install_element (BGP_NODE, &no_bgp_network_import_check_cmd);

  /* "bgp default local-preference" commands. */
  install_element (BGP_NODE, &bgp_default_local_preference_cmd);
  install_element (BGP_NODE, &no_bgp_default_local_preference_cmd);
  install_element (BGP_NODE, &no_bgp_default_local_preference_val_cmd);

  /* "neighbor remote-as" commands. */
  install_element (BGP_NODE, &neighbor_remote_as_cmd);
  install_element (BGP_NODE, &no_neighbor_cmd);
  install_element (BGP_NODE, &no_neighbor_remote_as_cmd);

  /* "neighbor peer-group" commands. */
  install_element (BGP_NODE, &neighbor_peer_group_cmd);
  install_element (BGP_NODE, &no_neighbor_peer_group_cmd);
  install_element (BGP_NODE, &no_neighbor_peer_group_remote_as_cmd);

  /* "neighbor local-as" commands. */
  install_element (BGP_NODE, &neighbor_local_as_cmd);
  install_element (BGP_NODE, &neighbor_local_as_no_prepend_cmd);
  install_element (BGP_NODE, &neighbor_local_as_no_prepend_replace_as_cmd);
  install_element (BGP_NODE, &no_neighbor_local_as_cmd);
  install_element (BGP_NODE, &no_neighbor_local_as_val_cmd);
  install_element (BGP_NODE, &no_neighbor_local_as_val2_cmd);
  install_element (BGP_NODE, &no_neighbor_local_as_val3_cmd);

  /* "neighbor password" commands. */
  install_element (BGP_NODE, &neighbor_password_cmd);
  install_element (BGP_NODE, &no_neighbor_password_cmd);

  /* "neighbor activate" commands. */
  install_element (BGP_NODE, &neighbor_activate_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_activate_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_activate_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_activate_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_activate_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_activate_cmd);

  /* "no neighbor activate" commands. */
  install_element (BGP_NODE, &no_neighbor_activate_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_activate_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_activate_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_activate_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_activate_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_activate_cmd);

  /* "neighbor peer-group set" commands. */
  install_element (BGP_NODE, &neighbor_set_peer_group_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_set_peer_group_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_set_peer_group_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_set_peer_group_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_set_peer_group_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_set_peer_group_cmd);
  
  /* "no neighbor peer-group unset" commands. */
  install_element (BGP_NODE, &no_neighbor_set_peer_group_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_set_peer_group_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_set_peer_group_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_set_peer_group_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_set_peer_group_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_set_peer_group_cmd);
  
  /* "neighbor softreconfiguration inbound" commands.*/
  install_element (BGP_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_NODE, &no_neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_soft_reconfiguration_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_soft_reconfiguration_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_soft_reconfiguration_cmd);

  /* "neighbor attribute-unchanged" commands.  */
  install_element (BGP_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_NODE, &no_neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_attr_unchanged10_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged1_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged2_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged3_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged4_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged5_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged6_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged7_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged8_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged9_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_attr_unchanged10_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged1_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged2_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged3_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged4_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged5_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged6_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged7_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged8_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged9_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_attr_unchanged10_cmd);

  /* "nexthop-local unchanged" commands */
  install_element (BGP_IPV6_NODE, &neighbor_nexthop_local_unchanged_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_nexthop_local_unchanged_cmd);

  /* "transparent-as" and "transparent-nexthop" for old version
     compatibility.  */
  install_element (BGP_NODE, &neighbor_transparent_as_cmd);
  install_element (BGP_NODE, &neighbor_transparent_nexthop_cmd);

  /* "neighbor next-hop-self" commands. */
  install_element (BGP_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_NODE, &no_neighbor_nexthop_self_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_nexthop_self_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_nexthop_self_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_nexthop_self_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_nexthop_self_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_nexthop_self_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_nexthop_self_cmd);

  /* "neighbor remove-private-AS" commands. */
  install_element (BGP_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_NODE, &no_neighbor_remove_private_as_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_remove_private_as_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_remove_private_as_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_remove_private_as_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_remove_private_as_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_remove_private_as_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_remove_private_as_cmd);

  /* "neighbor send-community" commands.*/
  install_element (BGP_NODE, &neighbor_send_community_cmd);
  install_element (BGP_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_NODE, &no_neighbor_send_community_type_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_send_community_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_send_community_type_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_send_community_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_send_community_type_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_send_community_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_send_community_type_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_send_community_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_send_community_type_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_send_community_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_send_community_type_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_send_community_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_send_community_type_cmd);

  /* "neighbor route-reflector" commands.*/
  install_element (BGP_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_NODE, &no_neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_route_reflector_client_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_route_reflector_client_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_route_reflector_client_cmd);

  /* "neighbor route-server" commands.*/
  install_element (BGP_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_NODE, &no_neighbor_route_server_client_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_route_server_client_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_route_server_client_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_route_server_client_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_route_server_client_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_route_server_client_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_route_server_client_cmd);

  /* "neighbor passive" commands. */
  install_element (BGP_NODE, &neighbor_passive_cmd);
  install_element (BGP_NODE, &no_neighbor_passive_cmd);

  /* "neighbor shutdown" commands. */
  install_element (BGP_NODE, &neighbor_shutdown_cmd);
  install_element (BGP_NODE, &no_neighbor_shutdown_cmd);

  /* Deprecated "neighbor capability route-refresh" commands.*/
  install_element (BGP_NODE, &neighbor_capability_route_refresh_cmd);
  install_element (BGP_NODE, &no_neighbor_capability_route_refresh_cmd);

  /* "neighbor capability orf prefix-list" commands.*/
  install_element (BGP_NODE, &neighbor_capability_orf_prefix_cmd);
  install_element (BGP_NODE, &no_neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_capability_orf_prefix_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_capability_orf_prefix_cmd);

  /* "neighbor capability dynamic" commands.*/
  install_element (BGP_NODE, &neighbor_capability_dynamic_cmd);
  install_element (BGP_NODE, &no_neighbor_capability_dynamic_cmd);

  /* "neighbor dont-capability-negotiate" commands. */
  install_element (BGP_NODE, &neighbor_dont_capability_negotiate_cmd);
  install_element (BGP_NODE, &no_neighbor_dont_capability_negotiate_cmd);

  /* "neighbor ebgp-multihop" commands. */
  install_element (BGP_NODE, &neighbor_ebgp_multihop_cmd);
  install_element (BGP_NODE, &neighbor_ebgp_multihop_ttl_cmd);
  install_element (BGP_NODE, &no_neighbor_ebgp_multihop_cmd);
  install_element (BGP_NODE, &no_neighbor_ebgp_multihop_ttl_cmd);

  /* "neighbor disable-connected-check" commands.  */
  install_element (BGP_NODE, &neighbor_disable_connected_check_cmd);
  install_element (BGP_NODE, &no_neighbor_disable_connected_check_cmd);
  install_element (BGP_NODE, &neighbor_enforce_multihop_cmd);
  install_element (BGP_NODE, &no_neighbor_enforce_multihop_cmd);

  /* "neighbor description" commands. */
  install_element (BGP_NODE, &neighbor_description_cmd);
  install_element (BGP_NODE, &no_neighbor_description_cmd);
  install_element (BGP_NODE, &no_neighbor_description_val_cmd);

  /* "neighbor update-source" commands. "*/
  install_element (BGP_NODE, &neighbor_update_source_cmd);
  install_element (BGP_NODE, &no_neighbor_update_source_cmd);

  /* "neighbor default-originate" commands. */
  install_element (BGP_NODE, &neighbor_default_originate_cmd);
  install_element (BGP_NODE, &neighbor_default_originate_rmap_cmd);
  install_element (BGP_NODE, &no_neighbor_default_originate_cmd);
  install_element (BGP_NODE, &no_neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_default_originate_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_default_originate_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_default_originate_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_default_originate_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_default_originate_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_default_originate_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_default_originate_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_default_originate_rmap_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_default_originate_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_default_originate_rmap_cmd);

  /* "neighbor port" commands. */
  install_element (BGP_NODE, &neighbor_port_cmd);
  install_element (BGP_NODE, &no_neighbor_port_cmd);
  install_element (BGP_NODE, &no_neighbor_port_val_cmd);

  /* "neighbor weight" commands. */
  install_element (BGP_NODE, &neighbor_weight_cmd);
  install_element (BGP_NODE, &no_neighbor_weight_cmd);
  install_element (BGP_NODE, &no_neighbor_weight_val_cmd);

  /* "neighbor override-capability" commands. */
  install_element (BGP_NODE, &neighbor_override_capability_cmd);
  install_element (BGP_NODE, &no_neighbor_override_capability_cmd);

  /* "neighbor strict-capability-match" commands. */
  install_element (BGP_NODE, &neighbor_strict_capability_cmd);
  install_element (BGP_NODE, &no_neighbor_strict_capability_cmd);

  /* "neighbor timers" commands. */
  install_element (BGP_NODE, &neighbor_timers_cmd);
  install_element (BGP_NODE, &no_neighbor_timers_cmd);

  /* "neighbor timers connect" commands. */
  install_element (BGP_NODE, &neighbor_timers_connect_cmd);
  install_element (BGP_NODE, &no_neighbor_timers_connect_cmd);
  install_element (BGP_NODE, &no_neighbor_timers_connect_val_cmd);

  /* "neighbor advertisement-interval" commands. */
  install_element (BGP_NODE, &neighbor_advertise_interval_cmd);
  install_element (BGP_NODE, &no_neighbor_advertise_interval_cmd);
  install_element (BGP_NODE, &no_neighbor_advertise_interval_val_cmd);

  /* "neighbor version" commands. */
  install_element (BGP_NODE, &neighbor_version_cmd);

  /* "neighbor interface" commands. */
  install_element (BGP_NODE, &neighbor_interface_cmd);
  install_element (BGP_NODE, &no_neighbor_interface_cmd);

  /* "neighbor distribute" commands. */
  install_element (BGP_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_NODE, &no_neighbor_distribute_list_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_distribute_list_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_distribute_list_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_distribute_list_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_distribute_list_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_distribute_list_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_distribute_list_cmd);

  /* "neighbor prefix-list" commands. */
  install_element (BGP_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_NODE, &no_neighbor_prefix_list_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_prefix_list_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_prefix_list_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_prefix_list_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_prefix_list_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_prefix_list_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_prefix_list_cmd);

  /* "neighbor filter-list" commands. */
  install_element (BGP_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_NODE, &no_neighbor_filter_list_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_filter_list_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_filter_list_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_filter_list_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_filter_list_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_filter_list_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_filter_list_cmd);

  /* "neighbor route-map" commands. */
  install_element (BGP_NODE, &neighbor_route_map_cmd);
  install_element (BGP_NODE, &no_neighbor_route_map_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_route_map_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_route_map_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_route_map_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_route_map_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_route_map_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_route_map_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_route_map_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_route_map_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_route_map_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_route_map_cmd);

  /* "neighbor unsuppress-map" commands. */
  install_element (BGP_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_NODE, &no_neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_unsuppress_map_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_unsuppress_map_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_unsuppress_map_cmd);  

  /* "neighbor maximum-prefix" commands. */
  install_element (BGP_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_maximum_prefix_threshold_restart_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_val_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_threshold_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_warning_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_threshold_warning_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_restart_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_maximum_prefix_threshold_restart_cmd);

  /* "neighbor allowas-in" */
  install_element (BGP_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_NODE, &no_neighbor_allowas_in_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_IPV4_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_IPV4_NODE, &no_neighbor_allowas_in_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_IPV4M_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_IPV4M_NODE, &no_neighbor_allowas_in_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_IPV6_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_IPV6_NODE, &no_neighbor_allowas_in_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_IPV6M_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_IPV6M_NODE, &no_neighbor_allowas_in_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_allowas_in_cmd);
  install_element (BGP_VPNV4_NODE, &neighbor_allowas_in_arg_cmd);
  install_element (BGP_VPNV4_NODE, &no_neighbor_allowas_in_cmd);

  /* address-family commands. */
  install_element (BGP_NODE, &address_family_ipv4_cmd);
  install_element (BGP_NODE, &address_family_ipv4_safi_cmd);
#ifdef HAVE_IPV6
  install_element (BGP_NODE, &address_family_ipv6_cmd);
  install_element (BGP_NODE, &address_family_ipv6_safi_cmd);
#endif /* HAVE_IPV6 */
  install_element (BGP_NODE, &address_family_vpnv4_cmd);
  install_element (BGP_NODE, &address_family_vpnv4_unicast_cmd);

  /* "exit-address-family" command. */
  install_element (BGP_IPV4_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV4M_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV6_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV6M_NODE, &exit_address_family_cmd);
  install_element (BGP_VPNV4_NODE, &exit_address_family_cmd);

  /* "clear ip bgp commands" */
  install_element (ENABLE_NODE, &clear_ip_bgp_all_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &clear_bgp_all_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_all_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_cmd);
#endif /* HAVE_IPV6 */

  /* "clear ip bgp neighbor soft in" */
  install_element (ENABLE_NODE, &clear_ip_bgp_all_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_vpnv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_vpnv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_vpnv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_vpnv4_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_vpnv4_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_vpnv4_in_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &clear_bgp_all_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_all_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_all_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_all_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_in_prefix_filter_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_soft_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_in_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_in_prefix_filter_cmd);
#endif /* HAVE_IPV6 */

  /* "clear ip bgp neighbor soft out" */
  install_element (ENABLE_NODE, &clear_ip_bgp_all_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_vpnv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_vpnv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_vpnv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_vpnv4_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_vpnv4_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_vpnv4_out_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &clear_bgp_all_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_all_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_all_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_soft_out_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_out_cmd);
#endif /* HAVE_IPV6 */

  /* "clear ip bgp neighbor soft" */
  install_element (ENABLE_NODE, &clear_ip_bgp_all_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_group_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_external_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_ipv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_all_vpnv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_vpnv4_soft_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_as_vpnv4_soft_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &clear_bgp_all_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_all_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_group_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_external_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_as_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_group_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_external_soft_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_as_soft_cmd);
#endif /* HAVE_IPV6 */

  /* "clear ip bgp neighbor rsclient" */
  install_element (ENABLE_NODE, &clear_ip_bgp_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_peer_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_ip_bgp_instance_peer_rsclient_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &clear_bgp_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_instance_all_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_peer_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_instance_peer_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_peer_rsclient_cmd);
  install_element (ENABLE_NODE, &clear_bgp_ipv6_instance_peer_rsclient_cmd);
#endif /* HAVE_IPV6 */

  /* "show ip bgp summary" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_ipv4_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv4_safi_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_ipv4_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv4_safi_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_summary_cmd);
#ifdef HAVE_IPV6
  install_element (VIEW_NODE, &show_bgp_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_safi_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_safi_summary_cmd);
#endif /* HAVE_IPV6 */
  install_element (RESTRICTED_NODE, &show_ip_bgp_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_instance_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_ipv4_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv4_safi_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_instance_ipv4_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv4_safi_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_vpnv4_all_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_vpnv4_rd_summary_cmd);
#ifdef HAVE_IPV6
  install_element (RESTRICTED_NODE, &show_bgp_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv6_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv6_safi_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv6_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv6_safi_summary_cmd);
#endif /* HAVE_IPV6 */
  install_element (ENABLE_NODE, &show_ip_bgp_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_ipv4_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv4_safi_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_ipv4_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv4_safi_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_summary_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &show_bgp_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_safi_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_safi_summary_cmd);
#endif /* HAVE_IPV6 */

  /* "show ip bgp neighbors" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_ipv4_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_ipv4_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_all_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_vpnv4_rd_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_neighbors_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_ipv4_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_vpnv4_all_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_vpnv4_rd_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_instance_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_neighbors_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_ipv4_neighbors_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_ipv4_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_neighbors_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_neighbors_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_all_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_vpnv4_rd_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_neighbors_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_neighbors_peer_cmd);

#ifdef HAVE_IPV6
  install_element (VIEW_NODE, &show_bgp_neighbors_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_neighbors_cmd);
  install_element (VIEW_NODE, &show_bgp_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_neighbors_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_neighbors_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_neighbors_peer_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv6_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_neighbors_peer_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv6_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_bgp_neighbors_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_neighbors_cmd);
  install_element (ENABLE_NODE, &show_bgp_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_neighbors_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_neighbors_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_neighbors_peer_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_neighbors_peer_cmd);

  /* Old commands.  */
  install_element (VIEW_NODE, &show_ipv6_bgp_summary_cmd);
  install_element (VIEW_NODE, &show_ipv6_mbgp_summary_cmd);
  install_element (ENABLE_NODE, &show_ipv6_bgp_summary_cmd);
  install_element (ENABLE_NODE, &show_ipv6_mbgp_summary_cmd);
#endif /* HAVE_IPV6 */

  /* "show ip bgp rsclient" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_ipv4_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_instance_ipv4_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv4_safi_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv4_safi_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_instance_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_ipv4_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_ip_bgp_instance_ipv4_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv4_safi_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv4_safi_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_ipv4_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_instance_ipv4_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv4_safi_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv4_safi_rsclient_summary_cmd);

#ifdef HAVE_IPV6
  install_element (VIEW_NODE, &show_bgp_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_instance_ipv6_safi_rsclient_summary_cmd);
  install_element (VIEW_NODE, &show_bgp_ipv6_safi_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv6_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv6_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_instance_ipv6_safi_rsclient_summary_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_ipv6_safi_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_instance_ipv6_safi_rsclient_summary_cmd);
  install_element (ENABLE_NODE, &show_bgp_ipv6_safi_rsclient_summary_cmd);
#endif /* HAVE_IPV6 */

  /* "show ip bgp paths" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_paths_cmd);
  install_element (VIEW_NODE, &show_ip_bgp_ipv4_paths_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_paths_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_ipv4_paths_cmd);

  /* "show ip bgp community" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_community_info_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_community_info_cmd);

  /* "show ip bgp attribute-info" commands. */
  install_element (VIEW_NODE, &show_ip_bgp_attr_info_cmd);
  install_element (ENABLE_NODE, &show_ip_bgp_attr_info_cmd);

  /* "redistribute" commands.  */
  install_element (BGP_NODE, &bgp_redistribute_ipv4_cmd);
  install_element (BGP_NODE, &no_bgp_redistribute_ipv4_cmd);
  install_element (BGP_NODE, &bgp_redistribute_ipv4_rmap_cmd);
  install_element (BGP_NODE, &no_bgp_redistribute_ipv4_rmap_cmd);
  install_element (BGP_NODE, &bgp_redistribute_ipv4_metric_cmd);
  install_element (BGP_NODE, &no_bgp_redistribute_ipv4_metric_cmd);
  install_element (BGP_NODE, &bgp_redistribute_ipv4_rmap_metric_cmd);
  install_element (BGP_NODE, &bgp_redistribute_ipv4_metric_rmap_cmd);
  install_element (BGP_NODE, &no_bgp_redistribute_ipv4_rmap_metric_cmd);
  install_element (BGP_NODE, &no_bgp_redistribute_ipv4_metric_rmap_cmd);
#ifdef HAVE_IPV6
  install_element (BGP_IPV6_NODE, &bgp_redistribute_ipv6_cmd);
  install_element (BGP_IPV6_NODE, &no_bgp_redistribute_ipv6_cmd);
  install_element (BGP_IPV6_NODE, &bgp_redistribute_ipv6_rmap_cmd);
  install_element (BGP_IPV6_NODE, &no_bgp_redistribute_ipv6_rmap_cmd);
  install_element (BGP_IPV6_NODE, &bgp_redistribute_ipv6_metric_cmd);
  install_element (BGP_IPV6_NODE, &no_bgp_redistribute_ipv6_metric_cmd);
  install_element (BGP_IPV6_NODE, &bgp_redistribute_ipv6_rmap_metric_cmd);
  install_element (BGP_IPV6_NODE, &bgp_redistribute_ipv6_metric_rmap_cmd);
  install_element (BGP_IPV6_NODE, &no_bgp_redistribute_ipv6_rmap_metric_cmd);
  install_element (BGP_IPV6_NODE, &no_bgp_redistribute_ipv6_metric_rmap_cmd);
#endif /* HAVE_IPV6 */

  /* ttl_security commands */
  install_element (BGP_NODE, &neighbor_ttl_security_cmd);
  install_element (BGP_NODE, &no_neighbor_ttl_security_cmd);

  /* "show bgp memory" commands. */
  install_element (VIEW_NODE, &show_bgp_memory_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_memory_cmd);
  install_element (ENABLE_NODE, &show_bgp_memory_cmd);
  
  /* "show bgp views" commands. */
  install_element (VIEW_NODE, &show_bgp_views_cmd);
  install_element (RESTRICTED_NODE, &show_bgp_views_cmd);
  install_element (ENABLE_NODE, &show_bgp_views_cmd);
  
  /* Community-list. */
  community_list_vty ();
}

#include "memory.h"
#include "bgp_regex.h"
#include "bgp_clist.h"
#include "bgp_ecommunity.h"

/* VTY functions.  */

/* Direction value to string conversion.  */
static const char *
community_direct_str (int direct)
{
  switch (direct)
    {
    case COMMUNITY_DENY:
      return "deny";
    case COMMUNITY_PERMIT:
      return "permit";
    default:
      return "unknown";
    }
}

/* Display error string.  */
static void
community_list_perror (struct vty *vty, int ret)
{
  switch (ret)
    {
    case COMMUNITY_LIST_ERR_CANT_FIND_LIST:
      vty_out (vty, "%% Can't find community-list%s", VTY_NEWLINE);
      break;
    case COMMUNITY_LIST_ERR_MALFORMED_VAL:
      vty_out (vty, "%% Malformed community-list value%s", VTY_NEWLINE);
      break;
    case COMMUNITY_LIST_ERR_STANDARD_CONFLICT:
      vty_out (vty, "%% Community name conflict, previously defined as standard community%s", VTY_NEWLINE);
      break;
    case COMMUNITY_LIST_ERR_EXPANDED_CONFLICT:
      vty_out (vty, "%% Community name conflict, previously defined as expanded community%s", VTY_NEWLINE);
      break;
    }
}

/* VTY interface for community_set() function.  */
static int
community_list_set_vty (struct vty *vty, int argc, const char **argv, 
                        int style, int reject_all_digit_name)
{
  int ret;
  int direct;
  char *str;

  /* Check the list type. */
  if (strncmp (argv[1], "p", 1) == 0)
    direct = COMMUNITY_PERMIT;
  else if (strncmp (argv[1], "d", 1) == 0)
    direct = COMMUNITY_DENY;
  else
    {
      vty_out (vty, "%% Matching condition must be permit or deny%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* All digit name check.  */
  if (reject_all_digit_name && all_digit (argv[0]))
    {
      vty_out (vty, "%% Community name cannot have all digits%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Concat community string argument.  */
  if (argc > 1)
    str = argv_concat (argv, argc, 2);
  else
    str = NULL;

  /* When community_list_set() return nevetive value, it means
     malformed community string.  */
  ret = community_list_set (bgp_clist, argv[0], str, direct, style);

  /* Free temporary community list string allocated by
     argv_concat().  */
  if (str)
    XFREE (MTYPE_TMP, str);

  if (ret < 0)
    {
      /* Display error string.  */
      community_list_perror (vty, ret);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

/* Communiyt-list entry delete.  */
static int
community_list_unset_vty (struct vty *vty, int argc, const char **argv,
			  int style)
{
  int ret;
  int direct = 0;
  char *str = NULL;

  if (argc > 1)
    {
      /* Check the list direct. */
      if (strncmp (argv[1], "p", 1) == 0)
	direct = COMMUNITY_PERMIT;
      else if (strncmp (argv[1], "d", 1) == 0)
	direct = COMMUNITY_DENY;
      else
	{
	  vty_out (vty, "%% Matching condition must be permit or deny%s",
		   VTY_NEWLINE);
	  return CMD_WARNING;
	}

      /* Concat community string argument.  */
      str = argv_concat (argv, argc, 2);
    }

  /* Unset community list.  */
  ret = community_list_unset (bgp_clist, argv[0], str, direct, style);

  /* Free temporary community list string allocated by
     argv_concat().  */
  if (str)
    XFREE (MTYPE_TMP, str);

  if (ret < 0)
    {
      community_list_perror (vty, ret);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

/* "community-list" keyword help string.  */
#define COMMUNITY_LIST_STR "Add a community list entry\n"
#define COMMUNITY_VAL_STR  "Community number in aa:nn format or internet|local-AS|no-advertise|no-export\n"

DEFUN (ip_community_list_standard,
       ip_community_list_standard_cmd,
       "ip community-list <1-99> (deny|permit) .AA:NN",
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       COMMUNITY_VAL_STR)
{
  return community_list_set_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD, 0);
}

ALIAS (ip_community_list_standard,
       ip_community_list_standard2_cmd,
       "ip community-list <1-99> (deny|permit)",
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n")

DEFUN (ip_community_list_expanded,
       ip_community_list_expanded_cmd,
       "ip community-list <100-500> (deny|permit) .LINE",
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (expanded)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return community_list_set_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED, 0);
}

DEFUN (ip_community_list_name_standard,
       ip_community_list_name_standard_cmd,
       "ip community-list standard WORD (deny|permit) .AA:NN",
       IP_STR
       COMMUNITY_LIST_STR
       "Add a standard community-list entry\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       COMMUNITY_VAL_STR)
{
  return community_list_set_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD, 1);
}

ALIAS (ip_community_list_name_standard,
       ip_community_list_name_standard2_cmd,
       "ip community-list standard WORD (deny|permit)",
       IP_STR
       COMMUNITY_LIST_STR
       "Add a standard community-list entry\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n")

DEFUN (ip_community_list_name_expanded,
       ip_community_list_name_expanded_cmd,
       "ip community-list expanded WORD (deny|permit) .LINE",
       IP_STR
       COMMUNITY_LIST_STR
       "Add an expanded community-list entry\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return community_list_set_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED, 1);
}

DEFUN (no_ip_community_list_standard_all,
       no_ip_community_list_standard_all_cmd,
       "no ip community-list <1-99>",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (standard)\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_community_list_expanded_all,
       no_ip_community_list_expanded_all_cmd,
       "no ip community-list <100-500>",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (expanded)\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_community_list_name_standard_all,
       no_ip_community_list_name_standard_all_cmd,
       "no ip community-list standard WORD",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Add a standard community-list entry\n"
       "Community list name\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_community_list_name_expanded_all,
       no_ip_community_list_name_expanded_all_cmd,
       "no ip community-list expanded WORD",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Add an expanded community-list entry\n"
       "Community list name\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_community_list_standard,
       no_ip_community_list_standard_cmd,
       "no ip community-list <1-99> (deny|permit) .AA:NN",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       COMMUNITY_VAL_STR)
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_community_list_expanded,
       no_ip_community_list_expanded_cmd,
       "no ip community-list <100-500> (deny|permit) .LINE",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Community list number (expanded)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_community_list_name_standard,
       no_ip_community_list_name_standard_cmd,
       "no ip community-list standard WORD (deny|permit) .AA:NN",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Specify a standard community-list\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       COMMUNITY_VAL_STR)
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_community_list_name_expanded,
       no_ip_community_list_name_expanded_cmd,
       "no ip community-list expanded WORD (deny|permit) .LINE",
       NO_STR
       IP_STR
       COMMUNITY_LIST_STR
       "Specify an expanded community-list\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return community_list_unset_vty (vty, argc, argv, COMMUNITY_LIST_EXPANDED);
}

static void
community_list_show (struct vty *vty, struct community_list *list)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry == list->head)
	{
	  if (all_digit (list->name))
	    vty_out (vty, "Community %s list %s%s",
		     entry->style == COMMUNITY_LIST_STANDARD ?
		     "standard" : "(expanded) access",
		     list->name, VTY_NEWLINE);
	  else
	    vty_out (vty, "Named Community %s list %s%s",
		     entry->style == COMMUNITY_LIST_STANDARD ?
		     "standard" : "expanded",
		     list->name, VTY_NEWLINE);
	}
      if (entry->any)
	vty_out (vty, "    %s%s",
		 community_direct_str (entry->direct), VTY_NEWLINE);
      else
	vty_out (vty, "    %s %s%s",
		 community_direct_str (entry->direct),
		 entry->style == COMMUNITY_LIST_STANDARD
		 ? community_str (entry->u.com) : entry->config,
		 VTY_NEWLINE);
    }
}

DEFUN (show_ip_community_list,
       show_ip_community_list_cmd,
       "show ip community-list",
       SHOW_STR
       IP_STR
       "List community-list\n")
{
  struct community_list *list;
  struct community_list_master *cm;

  cm = community_list_master_lookup (bgp_clist, COMMUNITY_LIST_MASTER);
  if (! cm)
    return CMD_SUCCESS;

  for (list = cm->num.head; list; list = list->next)
    community_list_show (vty, list);

  for (list = cm->str.head; list; list = list->next)
    community_list_show (vty, list);

  return CMD_SUCCESS;
}

DEFUN (show_ip_community_list_arg,
       show_ip_community_list_arg_cmd,
       "show ip community-list (<1-500>|WORD)",
       SHOW_STR
       IP_STR
       "List community-list\n"
       "Community-list number\n"
       "Community-list name\n")
{
  struct community_list *list;

  list = community_list_lookup (bgp_clist, argv[0], COMMUNITY_LIST_MASTER);
  if (! list)
    {
      vty_out (vty, "%% Can't find community-list%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  community_list_show (vty, list);

  return CMD_SUCCESS;
}

static int
extcommunity_list_set_vty (struct vty *vty, int argc, const char **argv, 
                           int style, int reject_all_digit_name)
{
  int ret;
  int direct;
  char *str;

  /* Check the list type. */
  if (strncmp (argv[1], "p", 1) == 0)
    direct = COMMUNITY_PERMIT;
  else if (strncmp (argv[1], "d", 1) == 0)
    direct = COMMUNITY_DENY;
  else
    {
      vty_out (vty, "%% Matching condition must be permit or deny%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* All digit name check.  */
  if (reject_all_digit_name && all_digit (argv[0]))
    {
      vty_out (vty, "%% Community name cannot have all digits%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Concat community string argument.  */
  if (argc > 1)
    str = argv_concat (argv, argc, 2);
  else
    str = NULL;

  ret = extcommunity_list_set (bgp_clist, argv[0], str, direct, style);

  /* Free temporary community list string allocated by
     argv_concat().  */
  if (str)
    XFREE (MTYPE_TMP, str);

  if (ret < 0)
    {
      community_list_perror (vty, ret);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

static int
extcommunity_list_unset_vty (struct vty *vty, int argc, const char **argv,
			     int style)
{
  int ret;
  int direct = 0;
  char *str = NULL;

  if (argc > 1)
    {
      /* Check the list direct. */
      if (strncmp (argv[1], "p", 1) == 0)
	direct = COMMUNITY_PERMIT;
      else if (strncmp (argv[1], "d", 1) == 0)
	direct = COMMUNITY_DENY;
      else
	{
	  vty_out (vty, "%% Matching condition must be permit or deny%s",
		   VTY_NEWLINE);
	  return CMD_WARNING;
	}

      /* Concat community string argument.  */
      str = argv_concat (argv, argc, 2);
    }

  /* Unset community list.  */
  ret = extcommunity_list_unset (bgp_clist, argv[0], str, direct, style);

  /* Free temporary community list string allocated by
     argv_concat().  */
  if (str)
    XFREE (MTYPE_TMP, str);

  if (ret < 0)
    {
      community_list_perror (vty, ret);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

/* "extcommunity-list" keyword help string.  */
#define EXTCOMMUNITY_LIST_STR "Add a extended community list entry\n"
#define EXTCOMMUNITY_VAL_STR  "Extended community attribute in 'rt aa:nn_or_IPaddr:nn' OR 'soo aa:nn_or_IPaddr:nn' format\n"

DEFUN (ip_extcommunity_list_standard,
       ip_extcommunity_list_standard_cmd,
       "ip extcommunity-list <1-99> (deny|permit) .AA:NN",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       EXTCOMMUNITY_VAL_STR)
{
  return extcommunity_list_set_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD, 0);
}

ALIAS (ip_extcommunity_list_standard,
       ip_extcommunity_list_standard2_cmd,
       "ip extcommunity-list <1-99> (deny|permit)",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n")

DEFUN (ip_extcommunity_list_expanded,
       ip_extcommunity_list_expanded_cmd,
       "ip extcommunity-list <100-500> (deny|permit) .LINE",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (expanded)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return extcommunity_list_set_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED, 0);
}

DEFUN (ip_extcommunity_list_name_standard,
       ip_extcommunity_list_name_standard_cmd,
       "ip extcommunity-list standard WORD (deny|permit) .AA:NN",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify standard extcommunity-list\n"
       "Extended Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       EXTCOMMUNITY_VAL_STR)
{
  return extcommunity_list_set_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD, 1);
}

ALIAS (ip_extcommunity_list_name_standard,
       ip_extcommunity_list_name_standard2_cmd,
       "ip extcommunity-list standard WORD (deny|permit)",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify standard extcommunity-list\n"
       "Extended Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n")

DEFUN (ip_extcommunity_list_name_expanded,
       ip_extcommunity_list_name_expanded_cmd,
       "ip extcommunity-list expanded WORD (deny|permit) .LINE",
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify expanded extcommunity-list\n"
       "Extended Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return extcommunity_list_set_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED, 1);
}

DEFUN (no_ip_extcommunity_list_standard_all,
       no_ip_extcommunity_list_standard_all_cmd,
       "no ip extcommunity-list <1-99>",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (standard)\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_extcommunity_list_expanded_all,
       no_ip_extcommunity_list_expanded_all_cmd,
       "no ip extcommunity-list <100-500>",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (expanded)\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_extcommunity_list_name_standard_all,
       no_ip_extcommunity_list_name_standard_all_cmd,
       "no ip extcommunity-list standard WORD",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify standard extcommunity-list\n"
       "Extended Community list name\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_extcommunity_list_name_expanded_all,
       no_ip_extcommunity_list_name_expanded_all_cmd,
       "no ip extcommunity-list expanded WORD",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify expanded extcommunity-list\n"
       "Extended Community list name\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_extcommunity_list_standard,
       no_ip_extcommunity_list_standard_cmd,
       "no ip extcommunity-list <1-99> (deny|permit) .AA:NN",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (standard)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       EXTCOMMUNITY_VAL_STR)
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_extcommunity_list_expanded,
       no_ip_extcommunity_list_expanded_cmd,
       "no ip extcommunity-list <100-500> (deny|permit) .LINE",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Extended Community list number (expanded)\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED);
}

DEFUN (no_ip_extcommunity_list_name_standard,
       no_ip_extcommunity_list_name_standard_cmd,
       "no ip extcommunity-list standard WORD (deny|permit) .AA:NN",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify standard extcommunity-list\n"
       "Extended Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       EXTCOMMUNITY_VAL_STR)
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_STANDARD);
}

DEFUN (no_ip_extcommunity_list_name_expanded,
       no_ip_extcommunity_list_name_expanded_cmd,
       "no ip extcommunity-list expanded WORD (deny|permit) .LINE",
       NO_STR
       IP_STR
       EXTCOMMUNITY_LIST_STR
       "Specify expanded extcommunity-list\n"
       "Community list name\n"
       "Specify community to reject\n"
       "Specify community to accept\n"
       "An ordered list as a regular-expression\n")
{
  return extcommunity_list_unset_vty (vty, argc, argv, EXTCOMMUNITY_LIST_EXPANDED);
}

static void
extcommunity_list_show (struct vty *vty, struct community_list *list)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry == list->head)
	{
	  if (all_digit (list->name))
	    vty_out (vty, "Extended community %s list %s%s",
		     entry->style == EXTCOMMUNITY_LIST_STANDARD ?
		     "standard" : "(expanded) access",
		     list->name, VTY_NEWLINE);
	  else
	    vty_out (vty, "Named extended community %s list %s%s",
		     entry->style == EXTCOMMUNITY_LIST_STANDARD ?
		     "standard" : "expanded",
		     list->name, VTY_NEWLINE);
	}
      if (entry->any)
	vty_out (vty, "    %s%s",
		 community_direct_str (entry->direct), VTY_NEWLINE);
      else
	vty_out (vty, "    %s %s%s",
		 community_direct_str (entry->direct),
		 entry->style == EXTCOMMUNITY_LIST_STANDARD ?
		 entry->u.ecom->str : entry->config,
		 VTY_NEWLINE);
    }
}

DEFUN (show_ip_extcommunity_list,
       show_ip_extcommunity_list_cmd,
       "show ip extcommunity-list",
       SHOW_STR
       IP_STR
       "List extended-community list\n")
{
  struct community_list *list;
  struct community_list_master *cm;

  cm = community_list_master_lookup (bgp_clist, EXTCOMMUNITY_LIST_MASTER);
  if (! cm)
    return CMD_SUCCESS;

  for (list = cm->num.head; list; list = list->next)
    extcommunity_list_show (vty, list);

  for (list = cm->str.head; list; list = list->next)
    extcommunity_list_show (vty, list);

  return CMD_SUCCESS;
}

DEFUN (show_ip_extcommunity_list_arg,
       show_ip_extcommunity_list_arg_cmd,
       "show ip extcommunity-list (<1-500>|WORD)",
       SHOW_STR
       IP_STR
       "List extended-community list\n"
       "Extcommunity-list number\n"
       "Extcommunity-list name\n")
{
  struct community_list *list;

  list = community_list_lookup (bgp_clist, argv[0], EXTCOMMUNITY_LIST_MASTER);
  if (! list)
    {
      vty_out (vty, "%% Can't find extcommunity-list%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  extcommunity_list_show (vty, list);

  return CMD_SUCCESS;
}

/* Return configuration string of community-list entry.  */
static const char *
community_list_config_str (struct community_entry *entry)
{
  const char *str;

  if (entry->any)
    str = "";
  else
    {
      if (entry->style == COMMUNITY_LIST_STANDARD)
	str = community_str (entry->u.com);
      else
	str = entry->config;
    }
  return str;
}

/* Display community-list and extcommunity-list configuration.  */
static int
community_list_config_write (struct vty *vty)
{
  struct community_list *list;
  struct community_entry *entry;
  struct community_list_master *cm;
  int write = 0;

  /* Community-list.  */
  cm = community_list_master_lookup (bgp_clist, COMMUNITY_LIST_MASTER);

  for (list = cm->num.head; list; list = list->next)
    for (entry = list->head; entry; entry = entry->next)
      {
	vty_out (vty, "ip community-list %s %s %s%s",
		 list->name, community_direct_str (entry->direct),
		 community_list_config_str (entry),
		 VTY_NEWLINE);
	write++;
      }
  for (list = cm->str.head; list; list = list->next)
    for (entry = list->head; entry; entry = entry->next)
      {
	vty_out (vty, "ip community-list %s %s %s %s%s",
		 entry->style == COMMUNITY_LIST_STANDARD
		 ? "standard" : "expanded",
		 list->name, community_direct_str (entry->direct),
		 community_list_config_str (entry),
		 VTY_NEWLINE);
	write++;
      }

  /* Extcommunity-list.  */
  cm = community_list_master_lookup (bgp_clist, EXTCOMMUNITY_LIST_MASTER);

  for (list = cm->num.head; list; list = list->next)
    for (entry = list->head; entry; entry = entry->next)
      {
	vty_out (vty, "ip extcommunity-list %s %s %s%s",
		 list->name, community_direct_str (entry->direct),
		 community_list_config_str (entry), VTY_NEWLINE);
	write++;
      }
  for (list = cm->str.head; list; list = list->next)
    for (entry = list->head; entry; entry = entry->next)
      {
	vty_out (vty, "ip extcommunity-list %s %s %s %s%s",
		 entry->style == EXTCOMMUNITY_LIST_STANDARD
		 ? "standard" : "expanded",
		 list->name, community_direct_str (entry->direct),
		 community_list_config_str (entry), VTY_NEWLINE);
	write++;
      }
  return write;
}

static struct cmd_node community_list_node =
{
  COMMUNITY_LIST_NODE,
  "",
  1				/* Export to vtysh.  */
};

static void
community_list_vty (void)
{
  install_node (&community_list_node, community_list_config_write);

  /* Community-list.  */
  install_element (CONFIG_NODE, &ip_community_list_standard_cmd);
  install_element (CONFIG_NODE, &ip_community_list_standard2_cmd);
  install_element (CONFIG_NODE, &ip_community_list_expanded_cmd);
  install_element (CONFIG_NODE, &ip_community_list_name_standard_cmd);
  install_element (CONFIG_NODE, &ip_community_list_name_standard2_cmd);
  install_element (CONFIG_NODE, &ip_community_list_name_expanded_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_standard_all_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_expanded_all_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_name_standard_all_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_name_expanded_all_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_standard_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_expanded_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_name_standard_cmd);
  install_element (CONFIG_NODE, &no_ip_community_list_name_expanded_cmd);
  install_element (VIEW_NODE, &show_ip_community_list_cmd);
  install_element (VIEW_NODE, &show_ip_community_list_arg_cmd);
  install_element (ENABLE_NODE, &show_ip_community_list_cmd);
  install_element (ENABLE_NODE, &show_ip_community_list_arg_cmd);

  /* Extcommunity-list.  */
  install_element (CONFIG_NODE, &ip_extcommunity_list_standard_cmd);
  install_element (CONFIG_NODE, &ip_extcommunity_list_standard2_cmd);
  install_element (CONFIG_NODE, &ip_extcommunity_list_expanded_cmd);
  install_element (CONFIG_NODE, &ip_extcommunity_list_name_standard_cmd);
  install_element (CONFIG_NODE, &ip_extcommunity_list_name_standard2_cmd);
  install_element (CONFIG_NODE, &ip_extcommunity_list_name_expanded_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_standard_all_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_expanded_all_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_name_standard_all_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_name_expanded_all_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_standard_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_expanded_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_name_standard_cmd);
  install_element (CONFIG_NODE, &no_ip_extcommunity_list_name_expanded_cmd);
  install_element (VIEW_NODE, &show_ip_extcommunity_list_cmd);
  install_element (VIEW_NODE, &show_ip_extcommunity_list_arg_cmd);
  install_element (ENABLE_NODE, &show_ip_extcommunity_list_cmd);
  install_element (ENABLE_NODE, &show_ip_extcommunity_list_arg_cmd);
}
