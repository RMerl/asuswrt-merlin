/*
 * IS-IS Rout(e)ing protocol - isis_dlpi.c
 *
 * Copyright (C) 2001,2002    Sampo Saaristo
 *                            Tampere University of Technology      
 *                            Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>
#if ISIS_METHOD == ISIS_METHOD_DLPI
#include <net/if.h>
#include <netinet/if_ether.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stropts.h>
#include <poll.h>
#include <sys/dlpi.h>
#include <sys/pfmod.h>

#include "log.h"
#include "stream.h"
#include "if.h"

#include "isisd/dict.h"
#include "isisd/include-netbsd/iso.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_flags.h"
#include "isisd/isisd.h"
#include "isisd/isis_network.h"

#include "privs.h"

extern struct zebra_privs_t isisd_privs;

static t_uscalar_t dlpi_ctl[1024];	/* DLPI control messages */

/*
 * Table 9 - Architectural constants for use with ISO 8802 subnetworks
 * ISO 10589 - 8.4.8
 */

u_char ALL_L1_ISS[6] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x14 };
u_char ALL_L2_ISS[6] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x15 };
u_char ALL_ISS[6] = { 0x09, 0x00, 0x2B, 0x00, 0x00, 0x05 };
u_char ALL_ESS[6] = { 0x09, 0x00, 0x2B, 0x00, 0x00, 0x04 };

static u_char sock_buff[8192];

static u_short pf_filter[] =
{
  ENF_PUSHWORD + 0,		/* Get the SSAP/DSAP values */
  ENF_PUSHLIT | ENF_CAND,	/* Check them */
  ISO_SAP | (ISO_SAP << 8),
  ENF_PUSHWORD + 1,		/* Get the control value */
  ENF_PUSHLIT | ENF_AND,	/* Isolate it */
#ifdef _BIG_ENDIAN
  0xFF00,
#else
  0x00FF,
#endif
  ENF_PUSHLIT | ENF_CAND,	/* Test for expected value */
#ifdef _BIG_ENDIAN
  0x0300
#else
  0x0003
#endif
};

/*
 * We would like to use something like libdlpi here, but that's not present on
 * all versions of Solaris or on any non-Solaris system, so it's nowhere near
 * as portable as we'd like.  Thus, we use the standards-conformant DLPI
 * interfaces plus the (optional; not needed) Solaris packet filter module.
 */

static void
dlpisend (int fd, const void *cbuf, size_t cbuflen,
  const void *dbuf, size_t dbuflen, int flags)
{
  const struct strbuf *ctlptr = NULL;
  const struct strbuf *dataptr = NULL;
  struct strbuf ctlbuf, databuf;

  if (cbuf != NULL)
    {
      memset (&ctlbuf, 0, sizeof (ctlbuf));
      ctlbuf.len = cbuflen;
      ctlbuf.buf = (void *)cbuf;
      ctlptr = &ctlbuf;
    }

  if (dbuf != NULL)
    {
      memset (&databuf, 0, sizeof (databuf));
      databuf.len = dbuflen;
      databuf.buf = (void *)dbuf;
      dataptr = &databuf;
    }

  /* We assume this doesn't happen often and isn't operationally significant */
  if (putmsg (fd, ctlptr, dataptr, flags) == -1)
    zlog_debug ("%s: putmsg: %s", __func__, safe_strerror (errno));
}

static ssize_t
dlpirctl (int fd)
{
  struct pollfd fds[1];
  struct strbuf ctlbuf, databuf;
  int flags, retv;

  do
    {
      /* Poll is used here in case the device doesn't speak DLPI correctly */
      memset (fds, 0, sizeof (fds));
      fds[0].fd = fd;
      fds[0].events = POLLIN | POLLPRI;
      if (poll (fds, 1, 1000) <= 0)
	return -1;

      memset (&ctlbuf, 0, sizeof (ctlbuf));
      memset (&databuf, 0, sizeof (databuf));
      ctlbuf.maxlen = sizeof (dlpi_ctl);
      ctlbuf.buf = (void *)dlpi_ctl;
      databuf.maxlen = sizeof (sock_buff);
      databuf.buf = (void *)sock_buff;
      flags = 0;
      retv = getmsg (fd, &ctlbuf, &databuf, &flags);

      if (retv < 0)
	return -1;
    }
  while (ctlbuf.len == 0);

  if (!(retv & MORECTL))
    {
      while (retv & MOREDATA)
	{
	  flags = 0;
	  retv = getmsg (fd, NULL, &databuf, &flags);
	}
      return ctlbuf.len;
    }

  while (retv & MORECTL)
    {
      flags = 0;
      retv = getmsg (fd, &ctlbuf, &databuf, &flags);
    }
  return -1;
}

static int
dlpiok (int fd, t_uscalar_t oprim)
{
  int retv;
  dl_ok_ack_t *doa = (dl_ok_ack_t *)dlpi_ctl;

  retv = dlpirctl (fd);
  if (retv < DL_OK_ACK_SIZE || doa->dl_primitive != DL_OK_ACK ||
    doa->dl_correct_primitive != oprim)
    {
      return -1;
    }
  else
    {
      return 0;
    }
}

static int
dlpiinfo (int fd)
{
  dl_info_req_t dir;
  ssize_t retv;

  memset (&dir, 0, sizeof (dir));
  dir.dl_primitive = DL_INFO_REQ;
  /* Info_req uses M_PCPROTO. */
  dlpisend (fd, &dir, sizeof (dir), NULL, 0, RS_HIPRI);
  retv = dlpirctl (fd);
  if (retv < DL_INFO_ACK_SIZE || dlpi_ctl[0] != DL_INFO_ACK)
    return -1;
  else
    return retv;
}

static int
dlpiopen (const char *devpath, ssize_t *acklen)
{
  int fd, flags;

  fd = open (devpath, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if (fd == -1)
    return -1;

  /* All that we want is for the open itself to be non-blocking, not I/O. */
  flags = fcntl (fd, F_GETFL, 0);
  if (flags != -1)
    fcntl (fd, F_SETFL, flags & ~O_NONBLOCK);

  /* After opening, ask for information */
  if ((*acklen = dlpiinfo (fd)) == -1)
    {
      close (fd);
      return -1;
    }

  return fd;
}

static int
dlpiattach (int fd, int unit)
{
  dl_attach_req_t dar;

  memset (&dar, 0, sizeof (dar));
  dar.dl_primitive = DL_ATTACH_REQ;
  dar.dl_ppa = unit;
  dlpisend (fd, &dar, sizeof (dar), NULL, 0, 0);
  return dlpiok (fd, dar.dl_primitive);
}

static int
dlpibind (int fd)
{
  dl_bind_req_t dbr;
  int retv;
  dl_bind_ack_t *dba = (dl_bind_ack_t *)dlpi_ctl;

  memset (&dbr, 0, sizeof (dbr));
  dbr.dl_primitive = DL_BIND_REQ;
  dbr.dl_service_mode = DL_CLDLS;
  dlpisend (fd, &dbr, sizeof (dbr), NULL, 0, 0);

  retv = dlpirctl (fd);
  if (retv < DL_BIND_ACK_SIZE || dba->dl_primitive != DL_BIND_ACK)
    return -1;
  else
    return 0;
}

static int
dlpimcast (int fd, const u_char *mcaddr)
{
  struct {
    dl_enabmulti_req_t der;
    u_char addr[ETHERADDRL];
  } dler;

  memset (&dler, 0, sizeof (dler));
  dler.der.dl_primitive = DL_ENABMULTI_REQ;
  dler.der.dl_addr_length = sizeof (dler.addr);
  dler.der.dl_addr_offset = dler.addr - (u_char *)&dler;
  memcpy (dler.addr, mcaddr, sizeof (dler.addr));
  dlpisend (fd, &dler, sizeof (dler), NULL, 0, 0);
  return dlpiok (fd, dler.der.dl_primitive);
}

static int
dlpiaddr (int fd, u_char *addr)
{
  dl_phys_addr_req_t dpar;
  dl_phys_addr_ack_t *dpaa = (dl_phys_addr_ack_t *)dlpi_ctl;
  int retv;

  memset (&dpar, 0, sizeof (dpar));
  dpar.dl_primitive = DL_PHYS_ADDR_REQ;
  dpar.dl_addr_type = DL_CURR_PHYS_ADDR;
  dlpisend (fd, &dpar, sizeof (dpar), NULL, 0, 0);

  retv = dlpirctl (fd);
  if (retv < DL_PHYS_ADDR_ACK_SIZE || dpaa->dl_primitive != DL_PHYS_ADDR_ACK)
    return -1;

  if (dpaa->dl_addr_offset < DL_PHYS_ADDR_ACK_SIZE ||
    dpaa->dl_addr_length != ETHERADDRL ||
    dpaa->dl_addr_offset + dpaa->dl_addr_length > retv)
    return -1;

  bcopy((char *)dpaa + dpaa->dl_addr_offset, addr, ETHERADDRL);
  return 0;
}

static int
open_dlpi_dev (struct isis_circuit *circuit)
{
  int fd, unit, retval;
  char devpath[MAXPATHLEN];
  dl_info_ack_t *dia = (dl_info_ack_t *)dlpi_ctl;
  ssize_t acklen;

  /* Only broadcast-type are supported at the moment */
  if (circuit->circ_type != CIRCUIT_T_BROADCAST)
    {
      zlog_warn ("%s: non-broadcast interface %s", __func__,
	circuit->interface->name);
      return ISIS_WARNING;
    }
  
  /* Try the vanity node first, if permitted */
  if (getenv("DLPI_DEVONLY") == NULL)
    {
      (void) snprintf (devpath, sizeof(devpath), "/dev/net/%s",
                      circuit->interface->name);
      fd = dlpiopen (devpath, &acklen);
    }
  
  /* Now try as an ordinary Style 1 node */
  if (fd == -1)
    {
      (void) snprintf (devpath, sizeof (devpath), "/dev/%s",
                      circuit->interface->name);
      unit = -1;
      fd = dlpiopen (devpath, &acklen);
    }

  /* If that fails, try again as Style 2 */
  if (fd == -1)
    {
      char *cp;

      cp = devpath + strlen (devpath);
      while (--cp >= devpath && isdigit(*cp))
	;
      unit = strtol(cp, NULL, 0);
      *cp = '\0';
      fd = dlpiopen (devpath, &acklen);

      /* If that too fails, then the device really doesn't exist */
      if (fd == -1)
	{
	  zlog_warn ("%s: unknown interface %s", __func__,
	    circuit->interface->name);
	  return ISIS_WARNING;
	}

      /* Double check the DLPI style */
      if (dia->dl_provider_style != DL_STYLE2)
	{
	  zlog_warn ("open_dlpi_dev(): interface %s: %s is not style 2",
	    circuit->interface->name, devpath);
	  close (fd);
	  return ISIS_WARNING;
	}

      /* If it succeeds, then we need to attach to the unit specified */
      dlpiattach (fd, unit);

      /* Reget the information, as it may be different per node */
      if ((acklen = dlpiinfo (fd)) == -1)
	{
	  close (fd);
	  return ISIS_WARNING;
	}
    }
  else
    {
      /* Double check the DLPI style */
      if (dia->dl_provider_style != DL_STYLE1)
	{
	  zlog_warn ("open_dlpi_dev(): interface %s: %s is not style 1",
	    circuit->interface->name, devpath);
	  close (fd);
	  return ISIS_WARNING;
	}
    }

  /* Check that the interface we've got is the kind we expect */
  if ((dia->dl_sap_length != 2 && dia->dl_sap_length != -2) ||
    dia->dl_service_mode != DL_CLDLS || dia->dl_addr_length != ETHERADDRL + 2 ||
    dia->dl_brdcst_addr_length != ETHERADDRL)
    {
      zlog_warn ("%s: unsupported interface type for %s", __func__,
	circuit->interface->name);
      close (fd);
      return ISIS_WARNING;
    }
  switch (dia->dl_mac_type)
    {
    case DL_CSMACD:
    case DL_ETHER:
    case DL_100VG:
    case DL_100VGTPR:
    case DL_ETH_CSMA:
    case DL_100BT:
      break;
    default:
      zlog_warn ("%s: unexpected mac type on %s: %d", __func__,
	circuit->interface->name, dia->dl_mac_type);
      close (fd);
      return ISIS_WARNING;
    }

  circuit->sap_length = dia->dl_sap_length;

  /*
   * The local hardware address is something that should be provided by way of
   * sockaddr_dl for the interface, but isn't on Solaris.  We set it here based
   * on DLPI's reported address to avoid roto-tilling the world.
   * (Note that isis_circuit_if_add on Solaris doesn't set the snpa.)
   *
   * Unfortunately, GLD is broken and doesn't provide the address after attach,
   * so we need to be careful and use DL_PHYS_ADDR_REQ instead.
   */
  if (dlpiaddr (fd, circuit->u.bc.snpa) == -1)
    {
      zlog_warn ("open_dlpi_dev(): interface %s: unable to get MAC address",
	circuit->interface->name);
      close (fd);
      return ISIS_WARNING;
    }

  /* Now bind to SAP 0.  This gives us 802-type traffic. */
  if (dlpibind (fd) == -1)
    {
      zlog_warn ("%s: cannot bind SAP 0 on %s", __func__,
	circuit->interface->name);
      close (fd);
      return ISIS_WARNING;
    }

  /*
   * Join to multicast groups according to
   * 8.4.2 - Broadcast subnetwork IIH PDUs
   */
  retval = 0;
  retval |= dlpimcast (fd, ALL_L1_ISS);
  retval |= dlpimcast (fd, ALL_ISS);
  retval |= dlpimcast (fd, ALL_L2_ISS);

  if (retval != 0)
    {
      zlog_warn ("%s: unable to join multicast on %s", __func__,
	circuit->interface->name);
      close (fd);
      return ISIS_WARNING;
    }

  /* Push on the packet filter to avoid stray 802 packets */
  if (ioctl (fd, I_PUSH, "pfmod") == 0)
    {
      struct packetfilt pfil;
      struct strioctl sioc;

      pfil.Pf_Priority = 0;
      pfil.Pf_FilterLen = sizeof (pf_filter) / sizeof (u_short);
      memcpy (pfil.Pf_Filter, pf_filter, sizeof (pf_filter));
      /* pfmod does not support transparent ioctls */
      sioc.ic_cmd = PFIOCSETF;
      sioc.ic_timout = 5;
      sioc.ic_len = sizeof (struct packetfilt);
      sioc.ic_dp = (char *)&pfil;
      if (ioctl (fd, I_STR, &sioc) == -1)
         zlog_warn("%s: could not perform PF_IOCSETF on %s",
           __func__, circuit->interface->name); 
    }

  circuit->fd = fd;

  return ISIS_OK;
}

/*
 * Create the socket and set the tx/rx funcs
 */
int
isis_sock_init (struct isis_circuit *circuit)
{
  int retval = ISIS_OK;

  if (isisd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs, %s", __func__, safe_strerror (errno));

  retval = open_dlpi_dev (circuit);

  if (retval != ISIS_OK)
    {
      zlog_warn ("%s: could not initialize the socket", __func__);
      goto end;
    }

  if (circuit->circ_type == CIRCUIT_T_BROADCAST)
    {
      circuit->tx = isis_send_pdu_bcast;
      circuit->rx = isis_recv_pdu_bcast;
    }
  else
    {
      zlog_warn ("isis_sock_init(): unknown circuit type");
      retval = ISIS_WARNING;
      goto end;
    }

end:
  if (isisd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs, %s", __func__, safe_strerror (errno));

  return retval;
}

int
isis_recv_pdu_bcast (struct isis_circuit *circuit, u_char * ssnpa)
{
  struct pollfd fds[1];
  struct strbuf ctlbuf, databuf;
  int flags, retv;
  dl_unitdata_ind_t *dui = (dl_unitdata_ind_t *)dlpi_ctl;

  memset (fds, 0, sizeof (fds));
  fds[0].fd = circuit->fd;
  fds[0].events = POLLIN | POLLPRI;
  if (poll (fds, 1, 0) <= 0)
    return ISIS_WARNING;

  memset (&ctlbuf, 0, sizeof (ctlbuf));
  memset (&databuf, 0, sizeof (databuf));
  ctlbuf.maxlen = sizeof (dlpi_ctl);
  ctlbuf.buf = (void *)dlpi_ctl;
  databuf.maxlen = sizeof (sock_buff);
  databuf.buf = (void *)sock_buff;
  flags = 0;
  retv = getmsg (circuit->fd, &ctlbuf, &databuf, &flags);

  if (retv < 0)
    {
      zlog_warn ("isis_recv_pdu_bcast: getmsg failed: %s",
		 safe_strerror (errno));
      return ISIS_WARNING;
    }

  if (retv & (MORECTL | MOREDATA))
    {
      while (retv & (MORECTL | MOREDATA))
	{
	  flags = 0;
	  retv = getmsg (circuit->fd, &ctlbuf, &databuf, &flags);
	}
      return ISIS_WARNING;
    }

  if (ctlbuf.len < DL_UNITDATA_IND_SIZE ||
    dui->dl_primitive != DL_UNITDATA_IND)
    return ISIS_WARNING;

  if (dui->dl_src_addr_length != ETHERADDRL + 2 ||
    dui->dl_src_addr_offset < DL_UNITDATA_IND_SIZE ||
    dui->dl_src_addr_offset + dui->dl_src_addr_length > ctlbuf.len)
    return ISIS_WARNING;

  memcpy (ssnpa, (char *)dui + dui->dl_src_addr_offset +
    (circuit->sap_length > 0 ? circuit->sap_length : 0), ETHERADDRL);

  if (databuf.len < LLC_LEN || sock_buff[0] != ISO_SAP ||
    sock_buff[1] != ISO_SAP || sock_buff[2] != 3)
    return ISIS_WARNING;

  stream_write (circuit->rcv_stream, sock_buff + LLC_LEN,
                databuf.len - LLC_LEN);
  stream_set_getp (circuit->rcv_stream, 0);

  return ISIS_OK;
}

int
isis_send_pdu_bcast (struct isis_circuit *circuit, int level)
{
  dl_unitdata_req_t *dur = (dl_unitdata_req_t *)dlpi_ctl;
  char *dstaddr;
  u_short *dstsap;
  int buflen;

  buflen = stream_get_endp (circuit->snd_stream) + LLC_LEN;
  if (buflen > sizeof (sock_buff))
    {
      zlog_warn ("isis_send_pdu_bcast: sock_buff size %lu is less than "
		 "output pdu size %d on circuit %s",
		 sizeof (sock_buff), buflen, circuit->interface->name);
      return ISIS_WARNING;
    }

  stream_set_getp (circuit->snd_stream, 0);

  memset (dur, 0, sizeof (*dur));
  dur->dl_primitive = DL_UNITDATA_REQ;
  dur->dl_dest_addr_length = ETHERADDRL + 2;
  dur->dl_dest_addr_offset = sizeof (*dur);

  dstaddr = (char *)(dur + 1);
  if (circuit->sap_length < 0)
    {
      dstsap = (u_short *)(dstaddr + ETHERADDRL);
    }
  else
    {
      dstsap = (u_short *)dstaddr;
      dstaddr += circuit->sap_length;
    }
  if (level == 1)
    memcpy (dstaddr, ALL_L1_ISS, ETHERADDRL);
  else
    memcpy (dstaddr, ALL_L2_ISS, ETHERADDRL);
  /* Note: DLPI SAP values are in host byte order */
  *dstsap = buflen;

  sock_buff[0] = ISO_SAP;
  sock_buff[1] = ISO_SAP;
  sock_buff[2] = 0x03;
  memcpy (sock_buff + LLC_LEN, circuit->snd_stream->data,
	  stream_get_endp (circuit->snd_stream));
  dlpisend (circuit->fd, dur, sizeof (*dur) + dur->dl_dest_addr_length,
	    sock_buff, buflen, 0);
  return ISIS_OK;
}

#endif /* ISIS_METHOD == ISIS_METHOD_DLPI */
