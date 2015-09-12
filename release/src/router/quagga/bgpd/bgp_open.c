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
#include "memory.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_aspath.h"
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
  char *pnt;
  char *end;
  struct capability_mp_data mpc;
  struct capability_header *hdr;

  pnt = peer->notify.data;
  end = pnt + peer->notify.length;
  
  while (pnt < end)
    {
      if (pnt + sizeof (struct capability_mp_data) + 2 > end)
	return;
      
      hdr = (struct capability_header *)pnt;
      if (pnt + hdr->length + 2 > end)
	return;

      memcpy (&mpc, pnt + 2, sizeof(struct capability_mp_data));

      if (hdr->code == CAPABILITY_CODE_MP)
	{
	  vty_out (vty, "  Capability error for: Multi protocol ");

	  switch (ntohs (mpc.afi))
	    {
	    case AFI_IP:
	      vty_out (vty, "AFI IPv4, ");
	      break;
	    case AFI_IP6:
	      vty_out (vty, "AFI IPv6, ");
	      break;
	    default:
	      vty_out (vty, "AFI Unknown %d, ", ntohs (mpc.afi));
	      break;
	    }
	  switch (mpc.safi)
	    {
	    case SAFI_UNICAST:
	      vty_out (vty, "SAFI Unicast");
	      break;
	    case SAFI_MULTICAST:
	      vty_out (vty, "SAFI Multicast");
	      break;
	    case SAFI_MPLS_LABELED_VPN:
	      vty_out (vty, "SAFI MPLS-labeled VPN");
	      break;
	    default:
	      vty_out (vty, "SAFI Unknown %d ", mpc.safi);
	      break;
	    }
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
      else if (hdr->code >= 128)
	vty_out (vty, "  Capability error: vendor specific capability code %d",
		 hdr->code);
      else
	vty_out (vty, "  Capability error: unknown capability code %d", 
		 hdr->code);

      pnt += hdr->length + 2;
    }
}

static void 
bgp_capability_mp_data (struct stream *s, struct capability_mp_data *mpc)
{
  mpc->afi = stream_getw (s);
  mpc->reserved = stream_getc (s);
  mpc->safi = stream_getc (s);
}

int
bgp_afi_safi_valid_indices (afi_t afi, safi_t *safi)
{
  switch (afi)
    {
      case AFI_IP:
#ifdef HAVE_IPV6
      case AFI_IP6:
#endif
        switch (*safi)
          {
            /* BGP MPLS-labeled VPN SAFI isn't contigious with others, remap */
            case SAFI_MPLS_LABELED_VPN:
              *safi = SAFI_MPLS_VPN;
            case SAFI_UNICAST:
            case SAFI_MULTICAST:
            case SAFI_MPLS_VPN:
              return 1;
          }
    }
  zlog_debug ("unknown afi/safi (%u/%u)", afi, *safi);
  
  return 0;
}

/* Set negotiated capability value. */
static int
bgp_capability_mp (struct peer *peer, struct capability_header *hdr)
{
  struct capability_mp_data mpc;
  struct stream *s = BGP_INPUT (peer);
  
  bgp_capability_mp_data (s, &mpc);
  
  if (BGP_DEBUG (normal, NORMAL))
    zlog_debug ("%s OPEN has MP_EXT CAP for afi/safi: %u/%u",
               peer->host, mpc.afi, mpc.safi);
  
  if (!bgp_afi_safi_valid_indices (mpc.afi, &mpc.safi))
    return -1;
   
  /* Now safi remapped, and afi/safi are valid array indices */
  peer->afc_recv[mpc.afi][mpc.safi] = 1;
  
  if (peer->afc[mpc.afi][mpc.safi])
    peer->afc_nego[mpc.afi][mpc.safi] = 1;
  else 
    return -1;

  return 0;
}

static void
bgp_capability_orf_not_support (struct peer *peer, afi_t afi, safi_t safi,
				u_char type, u_char mode)
{
  if (BGP_DEBUG (normal, NORMAL))
    zlog_debug ("%s Addr-family %d/%d has ORF type/mode %d/%d not supported",
	       peer->host, afi, safi, type, mode);
}

static const struct message orf_type_str[] =
{
  { ORF_TYPE_PREFIX,		"Prefixlist"		},
  { ORF_TYPE_PREFIX_OLD,	"Prefixlist (old)"	},
};
static const int orf_type_str_max = array_size(orf_type_str);

static const struct message orf_mode_str[] =
{
  { ORF_MODE_RECEIVE,	"Receive"	},
  { ORF_MODE_SEND,	"Send"		},
  { ORF_MODE_BOTH,	"Both"		},
};
static const int orf_mode_str_max = array_size(orf_mode_str);

static int
bgp_capability_orf_entry (struct peer *peer, struct capability_header *hdr)
{
  struct stream *s = BGP_INPUT (peer);
  struct capability_orf_entry entry;
  afi_t afi;
  safi_t safi;
  u_char type;
  u_char mode;
  u_int16_t sm_cap = 0; /* capability send-mode receive */
  u_int16_t rm_cap = 0; /* capability receive-mode receive */ 
  int i;

  /* ORF Entry header */
  bgp_capability_mp_data (s, &entry.mpc);
  entry.num = stream_getc (s);
  afi = entry.mpc.afi;
  safi = entry.mpc.safi;
  
  if (BGP_DEBUG (normal, NORMAL))
    zlog_debug ("%s ORF Cap entry for afi/safi: %u/%u",
	        peer->host, entry.mpc.afi, entry.mpc.safi);

  /* Check AFI and SAFI. */
  if (!bgp_afi_safi_valid_indices (entry.mpc.afi, &safi))
    {
      zlog_info ("%s Addr-family %d/%d not supported."
                 " Ignoring the ORF capability",
                 peer->host, entry.mpc.afi, entry.mpc.safi);
      return 0;
    }
  
  /* validate number field */
  if (sizeof (struct capability_orf_entry) + (entry.num * 2) > hdr->length)
    {
      zlog_info ("%s ORF Capability entry length error,"
                 " Cap length %u, num %u",
                 peer->host, hdr->length, entry.num);
      bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
      return -1;
    }

  for (i = 0 ; i < entry.num ; i++)
    {
      type = stream_getc(s);
      mode = stream_getc(s);
      
      /* ORF Mode error check */
      switch (mode)
        {
          case ORF_MODE_BOTH:
          case ORF_MODE_SEND:
          case ORF_MODE_RECEIVE:
            break;
          default:
	    bgp_capability_orf_not_support (peer, afi, safi, type, mode);
	    continue;
	}
      /* ORF Type and afi/safi error checks */
      /* capcode versus type */
      switch (hdr->code)
        {
          case CAPABILITY_CODE_ORF:
            switch (type)
              {
                case ORF_TYPE_PREFIX:
                  break;
                default:
                  bgp_capability_orf_not_support (peer, afi, safi, type, mode);
                  continue;
              }
            break;
          case CAPABILITY_CODE_ORF_OLD:
            switch (type)
              {
                case ORF_TYPE_PREFIX_OLD:
                  break;
                default:
                  bgp_capability_orf_not_support (peer, afi, safi, type, mode);
                  continue;
              }
            break;
          default:
            bgp_capability_orf_not_support (peer, afi, safi, type, mode);
            continue;
        }
                
      /* AFI vs SAFI */
      if (!((afi == AFI_IP && safi == SAFI_UNICAST)
            || (afi == AFI_IP && safi == SAFI_MULTICAST)
            || (afi == AFI_IP6 && safi == SAFI_UNICAST)))
        {
          bgp_capability_orf_not_support (peer, afi, safi, type, mode);
          continue;
        }
      
      if (BGP_DEBUG (normal, NORMAL))
        zlog_debug ("%s OPEN has %s ORF capability"
                    " as %s for afi/safi: %d/%d",
                    peer->host, LOOKUP (orf_type_str, type),
                    LOOKUP (orf_mode_str, mode),
                    entry.mpc.afi, safi);

      if (hdr->code == CAPABILITY_CODE_ORF)
	{
          sm_cap = PEER_CAP_ORF_PREFIX_SM_RCV;
          rm_cap = PEER_CAP_ORF_PREFIX_RM_RCV;
	}
      else if (hdr->code == CAPABILITY_CODE_ORF_OLD)
	{
          sm_cap = PEER_CAP_ORF_PREFIX_SM_OLD_RCV;
          rm_cap = PEER_CAP_ORF_PREFIX_RM_OLD_RCV;
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

static int
bgp_capability_restart (struct peer *peer, struct capability_header *caphdr)
{
  struct stream *s = BGP_INPUT (peer);
  u_int16_t restart_flag_time;
  size_t end = stream_get_getp (s) + caphdr->length;

  SET_FLAG (peer->cap, PEER_CAP_RESTART_RCV);
  restart_flag_time = stream_getw(s);
  if (CHECK_FLAG (restart_flag_time, RESTART_R_BIT))
    SET_FLAG (peer->cap, PEER_CAP_RESTART_BIT_RCV);
  
  UNSET_FLAG (restart_flag_time, 0xF000);
  peer->v_gr_restart = restart_flag_time;

  if (BGP_DEBUG (normal, NORMAL))
    {
      zlog_debug ("%s OPEN has Graceful Restart capability", peer->host);
      zlog_debug ("%s Peer has%srestarted. Restart Time : %d",
                  peer->host,
                  CHECK_FLAG (peer->cap, PEER_CAP_RESTART_BIT_RCV) ? " " 
                                                                   : " not ",
                  peer->v_gr_restart);
    }

  while (stream_get_getp (s) + 4 <= end)
    {
      afi_t afi = stream_getw (s);
      safi_t safi = stream_getc (s);
      u_char flag = stream_getc (s);
      
      if (!bgp_afi_safi_valid_indices (afi, &safi))
        {
          if (BGP_DEBUG (normal, NORMAL))
            zlog_debug ("%s Addr-family %d/%d(afi/safi) not supported."
                        " Ignore the Graceful Restart capability",
                        peer->host, afi, safi);
        }
      else if (!peer->afc[afi][safi])
        {
          if (BGP_DEBUG (normal, NORMAL))
            zlog_debug ("%s Addr-family %d/%d(afi/safi) not enabled."
                        " Ignore the Graceful Restart capability",
                        peer->host, afi, safi);
        }
      else
        {
          if (BGP_DEBUG (normal, NORMAL))
            zlog_debug ("%s Address family %s is%spreserved", peer->host,
                        afi_safi_print (afi, safi),
                        CHECK_FLAG (peer->af_cap[afi][safi],
                                    PEER_CAP_RESTART_AF_PRESERVE_RCV)
                        ? " " : " not ");

          SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_RESTART_AF_RCV);
          if (CHECK_FLAG (flag, RESTART_F_BIT))
            SET_FLAG (peer->af_cap[afi][safi], PEER_CAP_RESTART_AF_PRESERVE_RCV);
          
        }
    }
  return 0;
}

static as_t
bgp_capability_as4 (struct peer *peer, struct capability_header *hdr)
{
  SET_FLAG (peer->cap, PEER_CAP_AS4_RCV);
  
  if (hdr->length != CAPABILITY_CODE_AS4_LEN)
    {
      zlog_err ("%s AS4 capability has incorrect data length %d",
                peer->host, hdr->length);
      return 0;
    }
  
  as_t as4 = stream_getl (BGP_INPUT(peer));
  
  if (BGP_DEBUG (as4, AS4))
    zlog_debug ("%s [AS4] about to set cap PEER_CAP_AS4_RCV, got as4 %u",
                peer->host, as4);
  return as4;
}

static const struct message capcode_str[] =
{
  { CAPABILITY_CODE_MP,			"MultiProtocol Extensions"	},
  { CAPABILITY_CODE_REFRESH,		"Route Refresh"			},
  { CAPABILITY_CODE_ORF,		"Cooperative Route Filtering" 	},
  { CAPABILITY_CODE_RESTART,		"Graceful Restart"		},
  { CAPABILITY_CODE_AS4,		"4-octet AS number"		},
  { CAPABILITY_CODE_DYNAMIC,		"Dynamic"			},
  { CAPABILITY_CODE_REFRESH_OLD,	"Route Refresh (Old)"		},
  { CAPABILITY_CODE_ORF_OLD,		"ORF (Old)"			},
};
static const int capcode_str_max = array_size(capcode_str);

/* Minimum sizes for length field of each cap (so not inc. the header) */
static const size_t cap_minsizes[] = 
{
  [CAPABILITY_CODE_MP]		= sizeof (struct capability_mp_data),
  [CAPABILITY_CODE_REFRESH]	= CAPABILITY_CODE_REFRESH_LEN,
  [CAPABILITY_CODE_ORF]		= sizeof (struct capability_orf_entry),
  [CAPABILITY_CODE_RESTART]	= sizeof (struct capability_gr),
  [CAPABILITY_CODE_AS4]		= CAPABILITY_CODE_AS4_LEN,
  [CAPABILITY_CODE_DYNAMIC]	= CAPABILITY_CODE_DYNAMIC_LEN,
  [CAPABILITY_CODE_REFRESH_OLD]	= CAPABILITY_CODE_REFRESH_LEN,
  [CAPABILITY_CODE_ORF_OLD]	= sizeof (struct capability_orf_entry),
};

/**
 * Parse given capability.
 * XXX: This is reading into a stream, but not using stream API
 *
 * @param[out] mp_capability Set to 1 on return iff one or more Multiprotocol
 *                           capabilities were encountered.
 */
static int
bgp_capability_parse (struct peer *peer, size_t length, int *mp_capability,
		      u_char **error)
{
  int ret;
  struct stream *s = BGP_INPUT (peer);
  size_t end = stream_get_getp (s) + length;
  
  assert (STREAM_READABLE (s) >= length);
  
  while (stream_get_getp (s) < end)
    {
      size_t start;
      u_char *sp = stream_pnt (s);
      struct capability_header caphdr;
      
      /* We need at least capability code and capability length. */
      if (stream_get_getp(s) + 2 > end)
	{
	  zlog_info ("%s Capability length error (< header)", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}
      
      caphdr.code = stream_getc (s);
      caphdr.length = stream_getc (s);
      start = stream_get_getp (s);
      
      /* Capability length check sanity check. */
      if (start + caphdr.length > end)
	{
	  zlog_info ("%s Capability length error (< length)", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}
      
      if (BGP_DEBUG (normal, NORMAL))
	zlog_debug ("%s OPEN has %s capability (%u), length %u",
		   peer->host,
		   LOOKUP (capcode_str, caphdr.code),
		   caphdr.code, caphdr.length);
      
      /* Length sanity check, type-specific, for known capabilities */
      switch (caphdr.code)
        {
          case CAPABILITY_CODE_MP:
          case CAPABILITY_CODE_REFRESH:
          case CAPABILITY_CODE_REFRESH_OLD:
          case CAPABILITY_CODE_ORF:
          case CAPABILITY_CODE_ORF_OLD:
          case CAPABILITY_CODE_RESTART:
          case CAPABILITY_CODE_AS4:
          case CAPABILITY_CODE_DYNAMIC:
              /* Check length. */
              if (caphdr.length < cap_minsizes[caphdr.code])
                {
                  zlog_info ("%s %s Capability length error: got %u,"
                             " expected at least %u",
                             peer->host, 
                             LOOKUP (capcode_str, caphdr.code),
                             caphdr.length, 
			     (unsigned) cap_minsizes[caphdr.code]);
                  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
                  return -1;
                }
          /* we deliberately ignore unknown codes, see below */
          default:
            break;
        }
      
      switch (caphdr.code)
        {
          case CAPABILITY_CODE_MP:
            {
	      *mp_capability = 1;

              /* Ignore capability when override-capability is set. */
              if (! CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
                {
                  /* Set negotiated value. */
                  ret = bgp_capability_mp (peer, &caphdr);

                  /* Unsupported Capability. */
                  if (ret < 0)
                    {
                      /* Store return data. */
                      memcpy (*error, sp, caphdr.length + 2);
                      *error += caphdr.length + 2;
                    }
                }
            }
            break;
          case CAPABILITY_CODE_REFRESH:
          case CAPABILITY_CODE_REFRESH_OLD:
            {
              /* BGP refresh capability */
              if (caphdr.code == CAPABILITY_CODE_REFRESH_OLD)
                SET_FLAG (peer->cap, PEER_CAP_REFRESH_OLD_RCV);
              else
                SET_FLAG (peer->cap, PEER_CAP_REFRESH_NEW_RCV);
            }
            break;
          case CAPABILITY_CODE_ORF:
          case CAPABILITY_CODE_ORF_OLD:
            if (bgp_capability_orf_entry (peer, &caphdr))
              return -1;
            break;
          case CAPABILITY_CODE_RESTART:
            if (bgp_capability_restart (peer, &caphdr))
              return -1;
            break;
          case CAPABILITY_CODE_DYNAMIC:
            SET_FLAG (peer->cap, PEER_CAP_DYNAMIC_RCV);
            break;
          case CAPABILITY_CODE_AS4:
              /* Already handled as a special-case parsing of the capabilities
               * at the beginning of OPEN processing. So we care not a jot
               * for the value really, only error case.
               */
              if (!bgp_capability_as4 (peer, &caphdr))
                return -1;
              break;            
          default:
            if (caphdr.code > 128)
              {
                /* We don't send Notification for unknown vendor specific
                   capabilities.  It seems reasonable for now...  */
                zlog_warn ("%s Vendor specific capability %d",
                           peer->host, caphdr.code);
              }
            else
              {
                zlog_warn ("%s unrecognized capability code: %d - ignored",
                           peer->host, caphdr.code);
                memcpy (*error, sp, caphdr.length + 2);
                *error += caphdr.length + 2;
              }
          }
      if (stream_get_getp(s) != (start + caphdr.length))
        {
          if (stream_get_getp(s) > (start + caphdr.length))
            zlog_warn ("%s Cap-parser for %s read past cap-length, %u!",
                       peer->host, LOOKUP (capcode_str, caphdr.code),
                       caphdr.length);
          stream_set_getp (s, start + caphdr.length);
        }
    }
  return 0;
}

static int
bgp_auth_parse (struct peer *peer, size_t length)
{
  bgp_notify_send (peer, 
		   BGP_NOTIFY_OPEN_ERR, 
		   BGP_NOTIFY_OPEN_AUTH_FAILURE); 
  return -1;
}

static int
strict_capability_same (struct peer *peer)
{
  int i, j;

  for (i = AFI_IP; i < AFI_MAX; i++)
    for (j = SAFI_UNICAST; j < SAFI_MAX; j++)
      if (peer->afc[i][j] != peer->afc_nego[i][j])
	return 0;
  return 1;
}

/* peek into option, stores ASN to *as4 if the AS4 capability was found.
 * Returns  0 if no as4 found, as4cap value otherwise.
 */
as_t
peek_for_as4_capability (struct peer *peer, u_char length)
{
  struct stream *s = BGP_INPUT (peer);
  size_t orig_getp = stream_get_getp (s);
  size_t end = orig_getp + length;
  as_t as4 = 0;
  
  /* The full capability parser will better flag the error.. */
  if (STREAM_READABLE(s) < length)
    return 0;

  if (BGP_DEBUG (as4, AS4))
    zlog_info ("%s [AS4] rcv OPEN w/ OPTION parameter len: %u,"
                " peeking for as4",
	        peer->host, length);
  /* the error cases we DONT handle, we ONLY try to read as4 out of
   * correctly formatted options.
   */
  while (stream_get_getp(s) < end) 
    {
      u_char opt_type;
      u_char opt_length;
      
      /* Check the length. */
      if (stream_get_getp (s) + 2 > end)
        goto end;
      
      /* Fetch option type and length. */
      opt_type = stream_getc (s);
      opt_length = stream_getc (s);
      
      /* Option length check. */
      if (stream_get_getp (s) + opt_length > end)
        goto end;
      
      if (opt_type == BGP_OPEN_OPT_CAP)
        {
          unsigned long capd_start = stream_get_getp (s);
          unsigned long capd_end = capd_start + opt_length;
          
          assert (capd_end <= end);
          
	  while (stream_get_getp (s) < capd_end)
	    {
	      struct capability_header hdr;
	      
	      if (stream_get_getp (s) + 2 > capd_end)
                goto end;
              
              hdr.code = stream_getc (s);
              hdr.length = stream_getc (s);
              
	      if ((stream_get_getp(s) +  hdr.length) > capd_end)
		goto end;

	      if (hdr.code == CAPABILITY_CODE_AS4)
	        {
	          if (BGP_DEBUG (as4, AS4))
	            zlog_info ("[AS4] found AS4 capability, about to parse");
	          as4 = bgp_capability_as4 (peer, &hdr);
	          
	          goto end;
                }
              stream_forward_getp (s, hdr.length);
	    }
	}
    }

end:
  stream_set_getp (s, orig_getp);
  return as4;
}

/**
 * Parse open option.
 *
 * @param[out] mp_capability @see bgp_capability_parse() for semantics.
 */
int
bgp_open_option_parse (struct peer *peer, u_char length, int *mp_capability)
{
  int ret;
  u_char *error;
  u_char error_data[BGP_MAX_PACKET_SIZE];
  struct stream *s = BGP_INPUT(peer);
  size_t end = stream_get_getp (s) + length;

  ret = 0;
  error = error_data;

  if (BGP_DEBUG (normal, NORMAL))
    zlog_debug ("%s rcv OPEN w/ OPTION parameter len: %u",
	       peer->host, length);
  
  while (stream_get_getp(s) < end)
    {
      u_char opt_type;
      u_char opt_length;
      
      /* Must have at least an OPEN option header */
      if (STREAM_READABLE(s) < 2)
	{
	  zlog_info ("%s Option length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      /* Fetch option type and length. */
      opt_type = stream_getc (s);
      opt_length = stream_getc (s);
      
      /* Option length check. */
      if (STREAM_READABLE (s) < opt_length)
	{
	  zlog_info ("%s Option length error", peer->host);
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
	  return -1;
	}

      if (BGP_DEBUG (normal, NORMAL))
	zlog_debug ("%s rcvd OPEN w/ optional parameter type %u (%s) len %u",
		   peer->host, opt_type,
		   opt_type == BGP_OPEN_OPT_AUTH ? "Authentication" :
		   opt_type == BGP_OPEN_OPT_CAP ? "Capability" : "Unknown",
		   opt_length);
  
      switch (opt_type)
	{
	case BGP_OPEN_OPT_AUTH:
	  ret = bgp_auth_parse (peer, opt_length);
	  break;
	case BGP_OPEN_OPT_CAP:
	  ret = bgp_capability_parse (peer, opt_length, mp_capability, &error);
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

  /* Check there are no common AFI/SAFIs and send Unsupported Capability
     error. */
  if (*mp_capability &&
      ! CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
    {
      if (! peer->afc_nego[AFI_IP][SAFI_UNICAST] 
	  && ! peer->afc_nego[AFI_IP][SAFI_MULTICAST]
	  && ! peer->afc_nego[AFI_IP][SAFI_MPLS_VPN]
	  && ! peer->afc_nego[AFI_IP6][SAFI_UNICAST]
	  && ! peer->afc_nego[AFI_IP6][SAFI_MULTICAST])
	{
	  plog_err (peer->log, "%s [Error] Configured AFI/SAFIs do not "
		    "overlap with received MP capabilities",
		    peer->host);

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

static void
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
    safi = SAFI_MPLS_LABELED_VPN;

  stream_putc (s, BGP_OPEN_OPT_CAP);
  capp = stream_get_endp (s);           /* Set Capability Len Pointer */
  stream_putc (s, 0);                   /* Capability Length */
  stream_putc (s, code);                /* Capability Code */
  orfp = stream_get_endp (s);           /* Set ORF Len Pointer */
  stream_putc (s, 0);                   /* ORF Length */
  stream_putw (s, afi);
  stream_putc (s, 0);
  stream_putc (s, safi);
  numberp = stream_get_endp (s);        /* Set Number Pointer */
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
  orf_len = stream_get_endp (s) - orfp - 1;
  stream_putc_at (s, orfp, orf_len);

  /* Total Capability Len. */
  cap_len = stream_get_endp (s) - capp - 1;
  stream_putc_at (s, capp, cap_len);
}

/* Fill in capability open option to the packet. */
void
bgp_open_capability (struct stream *s, struct peer *peer)
{
  u_char len;
  unsigned long cp, capp, rcapp;
  afi_t afi;
  safi_t safi;
  as_t local_as;
  u_int32_t restart_time;

  /* Remember current pointer for Opt Parm Len. */
  cp = stream_get_endp (s);

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
      stream_putc (s, SAFI_MPLS_LABELED_VPN);
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

  /* AS4 */
  SET_FLAG (peer->cap, PEER_CAP_AS4_ADV);
  stream_putc (s, BGP_OPEN_OPT_CAP);
  stream_putc (s, CAPABILITY_CODE_AS4_LEN + 2);
  stream_putc (s, CAPABILITY_CODE_AS4);
  stream_putc (s, CAPABILITY_CODE_AS4_LEN);
  if ( peer->change_local_as )
    local_as = peer->change_local_as;
  else
    local_as = peer->local_as;
  stream_putl (s, local_as );

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

  /* Sending base graceful-restart capability irrespective of the config */
  SET_FLAG (peer->cap, PEER_CAP_RESTART_ADV);
  stream_putc (s, BGP_OPEN_OPT_CAP);
  capp = stream_get_endp (s);           /* Set Capability Len Pointer */
  stream_putc (s, 0);                   /* Capability Length */
  stream_putc (s, CAPABILITY_CODE_RESTART);
  rcapp = stream_get_endp (s);          /* Set Restart Capability Len Pointer */
  stream_putc (s, 0);
  restart_time = peer->bgp->restart_time;
  if (peer->bgp->t_startup)
    {
      SET_FLAG (restart_time, RESTART_R_BIT);
      SET_FLAG (peer->cap, PEER_CAP_RESTART_BIT_ADV);
    }
  stream_putw (s, restart_time);

  /* Send address-family specific graceful-restart capability only when GR config
     is present */
  if (bgp_flag_check (peer->bgp, BGP_FLAG_GRACEFUL_RESTART))
    {
      for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
        for (safi = SAFI_UNICAST ; safi < SAFI_MAX ; safi++)
          if (peer->afc[afi][safi])
            {
              stream_putw (s, afi);
              stream_putc (s, safi);
              stream_putc (s, 0); //Forwarding is not retained as of now.
            }
    }

  /* Total Graceful restart capability Len. */
  len = stream_get_endp (s) - rcapp - 1;
  stream_putc_at (s, rcapp, len);

  /* Total Capability Len. */
  len = stream_get_endp (s) - capp - 1;
  stream_putc_at (s, capp, len);

  /* Total Opt Parm Len. */
  len = stream_get_endp (s) - cp - 1;
  stream_putc_at (s, cp, len);
}
