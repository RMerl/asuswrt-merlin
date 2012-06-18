/* BGP open message handling
   Copyright (C) 1998, 1999 Kunihiro Ishiguro

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

#include "linklist.h"
#include "prefix.h"
#include "stream.h"
#include "thread.h"
#include "log.h"
#include "command.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_vty.h"

/* BGP-4 Multiprotocol Extentions lead us to the complex world. We can
   negotiate remote peer supports extentions or not. But if
   remote-peer doesn't supports negotiation process itself.  We would
   like to do manual configuration.

   So there is many configurable point.  First of all we want set each
   peer whether we send capability negotiation to the peer or not.
   Next, if we send capability to the peer we want to set my capabilty
   inforation at each peer. */

void
bgp_capability_vty_out (struct vty *vty, struct peer *peer)
{
  u_char *pnt;
  u_char *end;
  struct capability cap;

  pnt = peer->notify.data;
  end = pnt + peer->notify.length;

  while (pnt < end)
    {
      memcpy(&cap, pnt, sizeof(struct capability));

      if (pnt + 2 > end)
	return;
      if (pnt + (cap.length + 2) > end)
	return;

      if (cap.code == CAPABILITY_CODE_MP)
	{
	  vty_out (vty, "  Capability error for: Multi protocol ");

	  switch (ntohs (cap.mpc.afi))
	    {
	    case AFI_IP:
	      vty_out (vty, "AFI IPv4, ");
	      break;
	    case AFI_IP6:
	      vty_out (vty, "AFI IPv6, ");
	      break;
	    default:
	      vty_out (vty, "AFI Unknown %d, ", ntohs (cap.mpc.afi));
	      break;
	    }
	  switch (cap.mpc.safi)
	    {
	    case SAFI_UNICAST:
	      vty_out (vty, "SAFI Unicast");
	      break;
	    case SAFI_MULTICAST:
	      vty_out (vty, "SAFI Multicast");
	      break;
	    case SAFI_UNICAST_MULTICAST:
	      vty_out (vty, "SAFI Unicast Multicast");
	      break;
	    case BGP_SAFI_VPNV4:
	      vty_out (vty, "SAFI MPLS-VPN");
	      break;
	    default:
	      vty_out (vty, "SAFI Unknown %d ", cap.mpc.safi);
	      break;
	    }
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
      else if (cap.code >= 128)
	vty_out (vty, "  Capability error: vendor specific capability code %d",
		 cap.code);
      else
	vty_out (vty, "  Capability error: unknown capability code %d", 
		 cap.code);

      pnt += cap.length + 2;
    }
}

/* Set negotiated capability value. */
int
bgp_capability_mp (struct peer *peer, struct capability *cap)
{
  if (ntohs (cap->mpc.afi) == AFI_IP)
    {
      if (cap->mpc.safi == SAFI_UNICAST)
	{
	  peer->afc_recv[AFI_IP][SAFI_UNICAST] = 1;

	  if (peer->afc[AFI_IP][SAFI_UNICAST])
	    peer->afc_nego[AFI_IP][SAFI_UNICAST] = 1;
	  else
	    return -1;
	}
      else if (cap->mpc.safi == SAFI_MULTICAST) 
	{
	  peer->afc_recv[AFI_IP][SAFI_MULTICAST] = 1;

	  if (peer->afc[AFI_IP][SAFI_MULTICAST])
	    peer->afc_nego[AFI_IP][SAFI_MULTICAST] = 1;
	  else
	    return -1;
	}
      else if (cap->mpc.safi == BGP_SAFI_VPNV4)
	{
	  peer->afc_recv[AFI_IP][SAFI_MPLS_VPN] = 1;

	  if (peer->afc[AFI_IP][SAFI_MPLS_VPN])
	    peer->afc_nego[AFI_IP][SAFI_MPLS_VPN] = 1;
	  else
	    return -1;
	}
      else
	return -1;
    }
#ifdef HAVE_IPV6
  else if (ntohs (cap->mpc.afi) == AFI_IP6)
    {
      if (cap->mpc.safi == SAFI_UNICAST)
	{
	  peer->afc_recv[AFI_IP6][SAFI_UNICAST] = 1;

	  if (peer->afc[AFI_IP6][SAFI_UNICAST])
	    peer->afc_nego[AFI_IP6][SAFI_UNICAST] = 1;
	  else
	    return -1;
	}
      else if (cap->mpc.safi == SAFI_MULTICAST)
	{
	  peer->afc_recv[AFI_IP6][SAFI_MULTICAST] = 1;

	  if (peer->afc[AFI_IP6][SAFI_MULTICAST])
	    peer->afc_nego[AFI_IP6][SAFI_MULTICAST] = 1;
	  else
	    return -1;
	}
      else
	return -1;
    }
#endif /* HAVE_IPV6 */
  else
    {
      /* Unknown Address Family. */
      return -1;
    }

  return 0;
}

void
bgp_capability_orf_not_support (struct peer *peer, afi_t afi, safi_t safi,
				u_char type, u_char mode)
{
  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s Addr-family %d/%d has ORF type/mode %d/%d not supported",
	       peer->host, afi, safi, type, mode);
}

int
bgp_capability_orf (struct peer *peer, struct capability *cap,
		    u_char *pnt)
{
  afi_t afi = ntohs(cap->mpc.afi);
  safi_t safi = cap->mpc.safi;
  u_char number_of_orfs;
  u_char type;
  u_char mode;
  u_int16_t sm_cap = 0; /* capability send-mode receive */
  u_int16_t rm_cap = 0; /* capability receive-mode receive */ 
  int i;

  /* Check length. */
  if (cap->length < 7)
    {
      zlog_info ("%s ORF Capability length error %d",
		 peer->host, cap->length);
		 bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
      return -1;
    }

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s OPEN has ORF CAP(%s) for afi/safi: %u/%u",
	       peer->host, (cap->code == CAPABILITY_CODE_ORF ?
                       "new" : "old"), afi, safi);

  /* Check AFI and SAFI. */
  if ((afi != AFI_IP && afi != AFI_IP6)
      || (safi != SAFI_UNICAST && safi != SAFI_MULTICAST
	  && safi != BGP_SAFI_VPNV4))
    {
      zlog_info ("%s Addr-family %d/%d not supported. Ignoring the ORF capability",
                 peer->host, afi, safi);
      return -1;
    }

  number_of_orfs = *pnt++;

  for (i = 0 ; i < number_of_orfs ; i++)
    {
      type = *pnt++;
      mode = *pnt++;

      /* ORF Mode error check */
      if (mode != ORF_MODE_BOTH && mode != ORF_MODE_SEND
	  && mode != ORF_MODE_RECEIVE)
	{
	  bgp_capability_orf_not_support (peer, afi, safi, type, mode);
	  continue;
	}

      /* ORF Type and afi/safi error check */
      if (cap->code == CAPABILITY_CODE_ORF)
	{
	  if (type == ORF_TYPE_PREFIX &&
	      ((afi == AFI_IP && safi == SAFI_UNICAST)
		|| (afi == AFI_IP && safi == SAFI_MULTICAST)
		|| (afi == AFI_IP6 && safi == SAFI_UNICAST)))
	    {
	      sm_cap = PEER_CAP_ORF_PREFIX_SM_RCV;
	      rm_cap = PEER_CAP_ORF_PREFIX_RM_RCV;
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_info ("%s OPEN has Prefixlist ORF(%d) capability as %s for afi/safi: %d/%d",
			   peer->host, ORF_TYPE_PREFIX, (mode == ORF_MODE_SEND ? "SEND" :
			   mode == ORF_MODE_RECEIVE ? "RECEIVE" : "BOTH") , afi, safi);
	    }
	  else
	    {
	      bgp_capability_orf_not_support (peer, afi, safi, type, mode);
	      continue;
	    }
	}
      else if (cap->code == CAPABILITY_CODE_ORF_OLD)
	{
	  if (type == ORF_TYPE_PREFIX_OLD &&
	      ((afi == AFI_IP && safi == SAFI_UNICAST)
		|| (afi == AFI_IP && safi == SAFI_MULTICAST)
		|| (afi == AFI_IP6 && safi == SAFI_UNICAST)))
	    {
	      sm_cap = PEER_CAP_ORF_PREFIX_SM_OLD_RCV;
	      rm_cap = PEER_CAP_ORF_PREFIX_RM_OLD_RCV;
	      if (BGP_DEBUG (normal, NORMAL))
		zlog_info ("%s OPEN has Prefixlist ORF(%d) capability as %s for afi/safi: %d/%d",
			   peer->host, ORF_TYPE_PREFIX_OLD, (mode == ORF_MODE_SEND ? "SEND" :
			   mode == ORF_MODE_RECEIVE ? "RECEIVE" : "BOTH") , afi, safi);
	    }
	  else
	    {
	      bgp_capability_orf_not_support (peer, afi, safi, type, mode);
	      continue;
	    }
	}
      else
	{
	  bgp_capability_orf_not_support (peer, afi, safi, type, mode);
	  continue;
	}

      switch (mode)
	{
	  case ORF_MODE_BOTH:
	    SET_FLAG (peer->af_cap[afi][safi], sm_cap);
	    SET_FLAG (peer->af_cap[afi][safi], rm_cap);
	    break;
	  case ORF_MODE_SEND:
	    SET_FLAG (peer->af_cap[afi][safi], sm_cap);
	    break;
	  case ORF_MODE_RECEIVE:
	    SET_FLAG (peer->af_cap[afi][safi], rm_cap);
	    break;
	}
    }
  return 0;
}

/* Parse given capability. */
int
bgp_capability_parse (struct peer *peer, u_char *pnt, u_char length,
		      u_char **error)
{
  int ret;
  u_char *end;
  struct capability cap;

  end = pnt + length;

  while (pnt < end)
    {
      afi_t afi;
      safi_t safi;

      /* Fetch structure to the byte stream. */
      memcpy (&cap, pnt, sizeof (struct capability));

      afi = ntohs(cap.mpc.afi);
      safi = cap.mpc.safi;

      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s OPEN has CAPABILITY code: %d, length %d",
		   peer->host, cap.code, cap.length);

      /* We need at least capability code and capability length. */
      if (pnt + 2 > end)
	{
	  zlog_info ("%s Capability length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      /* Capability length check. */
      if (pnt + (cap.length + 2) > end)
	{
	  zlog_info ("%s Capability length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      /* We know MP Capability Code. */
      if (cap.code == CAPABILITY_CODE_MP)
	{
	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("%s OPEN has MP_EXT CAP for afi/safi: %u/%u",
		       peer->host, afi, safi);

	  /* Ignore capability when override-capability is set. */
	  if (! CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
	    {
	      /* Set negotiated value. */
	      ret = bgp_capability_mp (peer, &cap);

	      /* Unsupported Capability. */
	      if (ret < 0)
		{
		  /* Store return data. */
		  memcpy (*error, &cap, cap.length + 2);
		  *error += cap.length + 2;
		}
	    }
	}
      else if (cap.code == CAPABILITY_CODE_REFRESH
	       || cap.code == CAPABILITY_CODE_REFRESH_OLD)
	{
	  /* Check length. */
	  if (cap.length != CAPABILITY_CODE_REFRESH_LEN)
	    {
	      zlog_info ("%s Route Refresh Capability length error %d",
			 peer->host, cap.length);
	      bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	      return -1;
	    }

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("%s OPEN has ROUTE-REFRESH capability(%s) for all address-families",
		       peer->host,
		       cap.code == CAPABILITY_CODE_REFRESH_OLD ? "old" : "new");

	  /* BGP refresh capability */
	  if (cap.code == CAPABILITY_CODE_REFRESH_OLD)
	    SET_FLAG (peer->cap, PEER_CAP_REFRESH_OLD_RCV);
	  else
	    SET_FLAG (peer->cap, PEER_CAP_REFRESH_NEW_RCV);
	}
      else if (cap.code == CAPABILITY_CODE_ORF
	       || cap.code == CAPABILITY_CODE_ORF_OLD)
	bgp_capability_orf (peer, &cap, pnt + sizeof (struct capability));
      else if (cap.code == CAPABILITY_CODE_RESTART)
	{
	  struct graceful_restart_af graf;
	  u_int16_t restart_flag_time;
	  int restart_bit = 0;
	  u_char *restart_pnt;
	  u_char *restart_end;

	  /* Check length. */
	  if (cap.length < CAPABILITY_CODE_RESTART_LEN)
	    {
	      zlog_info ("%s Graceful Restart Capability length error %d",
			 peer->host, cap.length);
	      bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	      return -1;
	    }

	  SET_FLAG (peer->cap, PEER_CAP_RESTART_RCV);
	  restart_flag_time = ntohs(cap.mpc.afi);
	  if (CHECK_FLAG (restart_flag_time, RESTART_R_BIT))
	    restart_bit = 1;
	  UNSET_FLAG (restart_flag_time, 0xF000); 
	  peer->v_gr_restart = restart_flag_time;

	  if (BGP_DEBUG (normal, NORMAL))
	    {
	      zlog_info ("%s OPEN has Graceful Restart capability", peer->host);
	      zlog_info ("%s Peer has%srestarted. Restart Time : %d",
			 peer->host, restart_bit ? " " : " not ",
			 peer->v_gr_restart);
	    }

	  restart_pnt = pnt + 4;
	  restart_end = pnt + cap.length + 2;

	  while (restart_pnt < restart_end)
	    {
	      memcpy (&graf, restart_pnt, sizeof (struct graceful_restart_af));

	      afi = ntohs(graf.afi);
	      safi = graf.safi;

	      if (CHECK_FLAG (graf.flag, RESTART_F_BIT))
		SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_RESTART_AF_PRESERVE_RCV);

	      if (strcmp (afi_safi_print (afi, safi), "Unknown") == 0)
		{
		   if (BGP_DEBUG (normal, NORMAL))
		     zlog_info ("%s Addr-family %d/%d(afi/safi) not supported. Ignore the Graceful Restart capability",
				peer->host, afi, safi);
		}
	      else if (! peer->afc[afi][safi])
		{
		   if (BGP_DEBUG (normal, NORMAL))
		      zlog_info ("%s Addr-family %d/%d(afi/safi) not enabled. Ignore the Graceful Restart capability",
				 peer->host, afi, safi);
		}
	      else
		{
		  if (BGP_DEBUG (normal, NORMAL))
		    zlog_info ("%s Address family %s is%spreserved", peer->host,
			       afi_safi_print (afi, safi),
			       CHECK_FLAG (peer->af_cap[afi][safi], PEER_CAP_RESTART_AF_PRESERVE_RCV)
				 ? " " : " not ");

		    SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_RESTART_AF_RCV);
		}
	      restart_pnt += 4;
	    }
	}
      else if (cap.code == CAPABILITY_CODE_DYNAMIC)
	{
	  /* Check length. */
	  if (cap.length != CAPABILITY_CODE_DYNAMIC_LEN)
	    {
	      zlog_info ("%s Dynamic Capability length error %d",
			 peer->host, cap.length);
	      bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	      return -1;
	    }

	  if (BGP_DEBUG (normal, NORMAL))
	    zlog_info ("%s OPEN has DYNAMIC capability", peer->host);

	  SET_FLAG (peer->cap, PEER_CAP_DYNAMIC_RCV);
	}
      else if (cap.code > 128)
	{
	  /* We don't send Notification for unknown vendor specific
	     capabilities.  It seems reasonable for now...  */
	  zlog_warn ("%s Vendor specific capability %d",
		     peer->host, cap.code);
	}
      else
	{
	  zlog_warn ("%s unrecognized capability code: %d - ignored",
		     peer->host, cap.code);
	  memcpy (*error, &cap, cap.length + 2);
	  *error += cap.length + 2;
	}

      pnt += cap.length + 2;
    }
  return 0;
}

int
bgp_auth_parse (struct peer *peer, u_char *pnt, size_t length)
{
  bgp_notify_send (peer, 
		   BGP_NOTIFY_OPEN_ERR, 
		   BGP_NOTIFY_OPEN_AUTH_FAILURE); 
  return -1;
}

int
strict_capability_same (struct peer *peer)
{
  int i, j;

  for (i = AFI_IP; i < AFI_MAX; i++)
    for (j = SAFI_UNICAST; j < SAFI_MAX; j++)
      if (peer->afc[i][j] != peer->afc_nego[i][j])
	return 0;
  return 1;
}

/* Parse open option */
int
bgp_open_option_parse (struct peer *peer, u_char length, int *capability)
{
  int ret;
  u_char *end;
  u_char opt_type;
  u_char opt_length;
  u_char *pnt;
  u_char *error;
  u_char error_data[BGP_MAX_PACKET_SIZE];

  /* Fetch pointer. */
  pnt = stream_pnt (peer->ibuf);

  ret = 0;
  opt_type = 0;
  opt_length = 0;
  end = pnt + length;
  error = error_data;

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s rcv OPEN w/ OPTION parameter len: %u",
	       peer->host, length);
  
  while (pnt < end) 
    {
      /* Check the length. */
      if (pnt + 2 > end)
	{
	  zlog_info ("%s Option length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      /* Fetch option type and length. */
      opt_type = *pnt++;
      opt_length = *pnt++;
      
      /* Option length check. */
      if (pnt + opt_length > end)
	{
	  zlog_info ("%s Option length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      if (BGP_DEBUG (normal, NORMAL))
	zlog_info ("%s rcvd OPEN w/ optional parameter type %u (%s) len %u",
		   peer->host, opt_type,
		   opt_type == BGP_OPEN_OPT_AUTH ? "Authentication" :
		   opt_type == BGP_OPEN_OPT_CAP ? "Capability" : "Unknown",
		   opt_length);
  
      switch (opt_type)
	{
	case BGP_OPEN_OPT_AUTH:
	  ret = bgp_auth_parse (peer, pnt, opt_length);
	  break;
	case BGP_OPEN_OPT_CAP:
	  ret = bgp_capability_parse (peer, pnt, opt_length, &error);
	  *capability = 1;
	  break;
	default:
	  bgp_notify_send (peer, 
			   BGP_NOTIFY_OPEN_ERR, 
			   BGP_NOTIFY_OPEN_UNSUP_PARAM); 
	  ret = -1;
	  break;
	}

      /* Parse error.  To accumulate all unsupported capability codes,
         bgp_capability_parse does not return -1 when encounter
         unsupported capability code.  To detect that, please check
         error and erro_data pointer, like below.  */
      if (ret < 0)
	return -1;

      /* Forward pointer. */
      pnt += opt_length;
    }

  /* All OPEN option is parsed.  Check capability when strict compare
     flag is enabled.*/
  if (CHECK_FLAG (peer->flags, PEER_FLAG_STRICT_CAP_MATCH))
    {
      /* If Unsupported Capability exists. */
      if (error != error_data)
	{
	  bgp_notify_send_with_data (peer, 
				     BGP_NOTIFY_OPEN_ERR, 
				     BGP_NOTIFY_OPEN_UNSUP_CAPBL, 
				     error_data, error - error_data);
	  return -1;
	}

      /* Check local capability does not negotiated with remote
         peer. */
      if (! strict_capability_same (peer))
	{
	  bgp_notify_send (peer, 
			   BGP_NOTIFY_OPEN_ERR, 
			   BGP_NOTIFY_OPEN_UNSUP_CAPBL);
	  return -1;
	}
    }

  /* Check there is no common capability send Unsupported Capability
     error. */
  if (*capability && ! CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
    {
      if (! peer->afc_nego[AFI_IP][SAFI_UNICAST] 
	  && ! peer->afc_nego[AFI_IP][SAFI_MULTICAST]
	  && ! peer->afc_nego[AFI_IP][SAFI_MPLS_VPN]
	  && ! peer->afc_nego[AFI_IP6][SAFI_UNICAST]
	  && ! peer->afc_nego[AFI_IP6][SAFI_MULTICAST])
	{
	  plog_err (peer->log, "%s [Error] No common capability", peer->host);

	  if (error != error_data)

	    bgp_notify_send_with_data (peer, 
				       BGP_NOTIFY_OPEN_ERR, 
				       BGP_NOTIFY_OPEN_UNSUP_CAPBL, 
				       error_data, error - error_data);
	  else
	    bgp_notify_send (peer, 
			     BGP_NOTIFY_OPEN_ERR, 
			     BGP_NOTIFY_OPEN_UNSUP_CAPBL);
	  return -1;
	}
    }
  return 0;
}

void
bgp_open_capability_orf (struct stream *s, struct peer *peer,
                         afi_t afi, safi_t safi, u_char code)
{
  u_char cap_len;
  u_char orf_len;
  unsigned long capp;
  unsigned long orfp;
  unsigned long numberp;
  int number_of_orfs = 0;

  if (safi == SAFI_MPLS_VPN)
    safi = BGP_SAFI_VPNV4;

  stream_putc (s, BGP_OPEN_OPT_CAP);
  capp = stream_get_putp (s);           /* Set Capability Len Pointer */
  stream_putc (s, 0);                   /* Capability Length */
  stream_putc (s, code);                /* Capability Code */
  orfp = stream_get_putp (s);           /* Set ORF Len Pointer */
  stream_putc (s, 0);                   /* ORF Length */
  stream_putw (s, afi);
  stream_putc (s, 0);
  stream_putc (s, safi);
  numberp = stream_get_putp (s);        /* Set Number Pointer */
  stream_putc (s, 0);                   /* Number of ORFs */

  /* Address Prefix ORF */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM)
      || CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_RM))
    {
      stream_putc (s, (code == CAPABILITY_CODE_ORF ?
		   ORF_TYPE_PREFIX : ORF_TYPE_PREFIX_OLD));

      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM)
	  && CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_RM))
	{
	  SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV);
	  SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_ADV);
	  stream_putc (s, ORF_MODE_BOTH);
	}
      else if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM))
	{
	  SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV);
	  stream_putc (s, ORF_MODE_SEND);
	}
      else
	{
	  SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_ADV);
	  stream_putc (s, ORF_MODE_RECEIVE);
	}
      number_of_orfs++;
    }

  /* Total Number of ORFs. */
  stream_putc_at (s, numberp, number_of_orfs);

  /* Total ORF Len. */
  orf_len = stream_get_putp (s) - orfp - 1;
  stream_putc_at (s, orfp, orf_len);

  /* Total Capability Len. */
  cap_len = stream_get_putp (s) - capp - 1;
  stream_putc_at (s, capp, cap_len);
}

/* Fill in capability open option to the packet. */
void
bgp_open_capability (struct stream *s, struct peer *peer)
{
  u_char len;
  unsigned long cp;
  afi_t afi;
  safi_t safi;

  /* Remember current pointer for Opt Parm Len. */
  cp = stream_get_putp (s);

  /* Opt Parm Len. */
  stream_putc (s, 0);

  /* Do not send capability. */
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_CAPABILITY_OPEN) 
      || CHECK_FLAG (peer->flags, PEER_FLAG_DONT_CAPABILITY))
    return;

  /* IPv4 unicast. */
  if (peer->afc[AFI_IP][SAFI_UNICAST])
    {
      peer->afc_adv[AFI_IP][SAFI_UNICAST] = 1;
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, AFI_IP);
      stream_putc (s, 0);
      stream_putc (s, SAFI_UNICAST);
    }
  /* IPv4 multicast. */
  if (peer->afc[AFI_IP][SAFI_MULTICAST])
    {
      peer->afc_adv[AFI_IP][SAFI_MULTICAST] = 1;
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, AFI_IP);
      stream_putc (s, 0);
      stream_putc (s, SAFI_MULTICAST);
    }
  /* IPv4 VPN */
  if (peer->afc[AFI_IP][SAFI_MPLS_VPN])
    {
      peer->afc_adv[AFI_IP][SAFI_MPLS_VPN] = 1;
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, AFI_IP);
      stream_putc (s, 0);
      stream_putc (s, BGP_SAFI_VPNV4);
    }
#ifdef HAVE_IPV6
  /* IPv6 unicast. */
  if (peer->afc[AFI_IP6][SAFI_UNICAST])
    {
      peer->afc_adv[AFI_IP6][SAFI_UNICAST] = 1;
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, AFI_IP6);
      stream_putc (s, 0);
      stream_putc (s, SAFI_UNICAST);
    }
  /* IPv6 multicast. */
  if (peer->afc[AFI_IP6][SAFI_MULTICAST])
    {
      peer->afc_adv[AFI_IP6][SAFI_MULTICAST] = 1;
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_MP);
      stream_putc (s, CAPABILITY_CODE_MP_LEN);
      stream_putw (s, AFI_IP6);
      stream_putc (s, 0);
      stream_putc (s, SAFI_MULTICAST);
    }
#endif /* HAVE_IPV6 */

  /* Route refresh. */
  SET_FLAG (peer->cap, PEER_CAP_REFRESH_ADV);
  stream_putc (s, BGP_OPEN_OPT_CAP);
  stream_putc (s, CAPABILITY_CODE_REFRESH_LEN + 2);
  stream_putc (s, CAPABILITY_CODE_REFRESH_OLD);
  stream_putc (s, CAPABILITY_CODE_REFRESH_LEN);
  stream_putc (s, BGP_OPEN_OPT_CAP);
  stream_putc (s, CAPABILITY_CODE_REFRESH_LEN + 2);
  stream_putc (s, CAPABILITY_CODE_REFRESH);
  stream_putc (s, CAPABILITY_CODE_REFRESH_LEN);

  /* ORF capability. */
  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
    for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM)
	  || CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_RM))
	{
	  bgp_open_capability_orf (s, peer, afi, safi, CAPABILITY_CODE_ORF_OLD);
	  bgp_open_capability_orf (s, peer, afi, safi, CAPABILITY_CODE_ORF);
	}

  /* Dynamic capability. */
  if (CHECK_FLAG (peer->flags, PEER_FLAG_DYNAMIC_CAPABILITY))
    {
      SET_FLAG (peer->cap, PEER_CAP_DYNAMIC_ADV);
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_DYNAMIC_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_DYNAMIC);
      stream_putc (s, CAPABILITY_CODE_DYNAMIC_LEN);
    }

  /* Graceful restart capability */
  if (bgp_flag_check (peer->bgp, BGP_FLAG_GRACEFUL_RESTART))
    {
      SET_FLAG (peer->cap, PEER_CAP_RESTART_ADV);
      stream_putc (s, BGP_OPEN_OPT_CAP);
      stream_putc (s, CAPABILITY_CODE_RESTART_LEN + 2);
      stream_putc (s, CAPABILITY_CODE_RESTART);
      stream_putc (s, CAPABILITY_CODE_RESTART_LEN);
      stream_putw (s, peer->bgp->restart_time);
    }

  /* Total Opt Parm Len. */
  len = stream_get_putp (s) - cp - 1;
  stream_putc_at (s, cp, len);
}
