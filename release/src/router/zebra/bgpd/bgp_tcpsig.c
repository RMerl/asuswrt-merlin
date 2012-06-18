/* BGP TCP signature related functions
   Copyright (C) 2004 Hiroki Nakano; Thanks to Okabe lab. (Kyoto University)

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

#ifdef HAVE_OPENBSD_TCP_SIGNATURE
#include <net/pfkeyv2.h>
#endif /* HAVE_OPENBSD_TCP_SIGNATURE */

#include "log.h"
#include "memory.h"
#include "sockunion.h"
#include "vty.h"
#include "bgpd/bgpd.h"
#include "bgpd/bgp_network.h"
#include "bgpd/bgp_tcpsig.h"

#ifdef HAVE_OPENBSD_TCP_SIGNATURE

#define	IOVEC_SIZE 20
#define MAX_KEY_LEN 80

#define	SADBALIGN sizeof(u_int64_t)
#define LENUNIT(x) ((x) / SADBALIGN)
#define EXTLEN(x) (((struct sadb_ext *)(x))->sadb_ext_len * SADBALIGN)
#define	PADUP(x) (((x) + (SADBALIGN - 1)) & ~(SADBALIGN - 1))

static u_int32_t	bgp_pfkey_sadb_msg_seq = 1;
static int		bgp_pfkey_sd = -1;

int bgp_pfkey_init(void);

#define PUSH_IOVEC(base,len) ((*iovsizep) > 0 ? \
			      ((*iovp)->iov_base = (base), \
                               (*iovp)->iov_len = (len), \
                               (*iovp)++, \
                               (*iovsizep)--) : \
			      ((*iovsizep) = -1))

struct bgp_pfkey_sadb_msg {
  struct sadb_msg header;
};

int
bgp_pfkey_build_sadb_msg(u_int8_t mtype,
			 struct bgp_pfkey_sadb_msg *sp,
			 struct iovec **iovp, int *iovsizep)
{
  memset(sp, 0, sizeof(*sp));

  sp->header.sadb_msg_version = PF_KEY_V2;
  sp->header.sadb_msg_type = mtype;
  sp->header.sadb_msg_satype = SADB_X_SATYPE_TCPSIGNATURE;
  sp->header.sadb_msg_len = LENUNIT(sizeof(sp->header));
  sp->header.sadb_msg_seq = bgp_pfkey_sadb_msg_seq++;
  sp->header.sadb_msg_pid = getpid();

  PUSH_IOVEC(&sp->header, sizeof(sp->header));
  
  return sp->header.sadb_msg_len;
}

struct bgp_pfkey_sadb_sa {
  struct sadb_sa header;
};

int
bgp_pfkey_build_sadb_sa(u_int32_t spi,
			struct bgp_pfkey_sadb_sa *sp,
			struct iovec **iovp, int *iovsizep)
{
  memset(sp, 0, sizeof(*sp));

  sp->header.sadb_sa_len = LENUNIT(sizeof(sp->header));
  sp->header.sadb_sa_exttype = SADB_EXT_SA;
  sp->header.sadb_sa_spi = spi;
  sp->header.sadb_sa_replay = 0;
  sp->header.sadb_sa_state = SADB_SASTATE_MATURE;
  sp->header.sadb_sa_auth = 0;
  sp->header.sadb_sa_encrypt = 0;

  PUSH_IOVEC(&sp->header, sizeof(sp->header));
  
  return sp->header.sadb_sa_len;
}

struct bgp_pfkey_sadb_address {
  struct sadb_address header;
  struct sockaddr_storage ss;
};

int
bgp_pfkey_build_sadb_address(union sockunion *su,
			     struct bgp_pfkey_sadb_address *sp,
			     struct iovec **iovp, int *iovsizep)
{
  int paddedsslen;

  memset(sp, 0, sizeof(*sp));

  switch (su->sa.sa_family) {
  case AF_INET:
    ((struct sockaddr_in *)&sp->ss)->sin_family = AF_INET;
    ((struct sockaddr_in *)&sp->ss)->sin_len = sizeof(struct sockaddr_in);
    ((struct sockaddr_in *)&sp->ss)->sin_addr = su->sin.sin_addr;
    break;
  case AF_INET6:
    ((struct sockaddr_in6 *)&sp->ss)->sin6_family = AF_INET6;
    ((struct sockaddr_in6 *)&sp->ss)->sin6_len = sizeof(struct sockaddr_in6);
    ((struct sockaddr_in6 *)&sp->ss)->sin6_addr = su->sin6.sin6_addr;
    break;
  default:
    return -1;
  }
  paddedsslen = PADUP(((struct sockaddr *)&sp->ss)->sa_len);

  sp->header.sadb_address_len = LENUNIT(sizeof(sp->header) + paddedsslen);
  sp->header.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;

  PUSH_IOVEC(&sp->header, sizeof(sp->header));
  PUSH_IOVEC(&sp->ss, paddedsslen);
  
  return sp->header.sadb_address_len;
}

struct bgp_pfkey_sadb_key {
  struct sadb_key header;
  char keybuf[PADUP(MAX_KEY_LEN)];
};

int
bgp_pfkey_build_sadb_key(char *key, int keylen,
			 struct bgp_pfkey_sadb_key *sp,
			 struct iovec **iovp, int *iovsizep)
{
  int paddedkeylen;

  if (keylen < 0 || keylen > MAX_KEY_LEN)
    return -1; 

  memset(sp, 0, sizeof(*sp));

  memcpy(sp->keybuf, key, keylen);
  paddedkeylen = PADUP(keylen);

  sp->header.sadb_key_len = LENUNIT(sizeof(sp->header) + paddedkeylen);
  sp->header.sadb_key_exttype = SADB_EXT_KEY_AUTH;
  sp->header.sadb_key_bits = 8 * keylen;

  PUSH_IOVEC(&sp->header, sizeof(sp->header));
  PUSH_IOVEC(sp->keybuf, paddedkeylen);

  return sp->header.sadb_key_len;
}

struct bgp_pfkey_sadb_spirange {
  struct sadb_spirange header;
};

int
bgp_pfkey_build_sadb_spirange(struct bgp_pfkey_sadb_spirange *sp,
			      struct iovec **iovp, int *iovsizep)
{
  memset(sp, 0, sizeof(*sp));

  sp->header.sadb_spirange_len = LENUNIT(sizeof(sp->header));
  sp->header.sadb_spirange_exttype = SADB_EXT_SPIRANGE;
  sp->header.sadb_spirange_min = 0x100;
  sp->header.sadb_spirange_max = 0xffffffff;

  PUSH_IOVEC(&sp->header, sizeof(sp->header));
  
  return sp->header.sadb_spirange_len;
}


int
bgp_pfkey_write(int fd, struct iovec *iov, int iovcnt)
{
  int i, len, rlen;

  len = 0;
  for (i = 0; i < iovcnt; i++)
    len += iov[i].iov_len;

  rlen = writev(fd, iov, iovcnt);
  if (rlen == -1)
    {
      zlog_err("pfkey writev: %s", strerror(errno));
      return -1;
    }
  else if (rlen != len)
    {
      zlog_err("pfkey writev: len=%d ret=%d", len, rlen);
      return -1;
    }
 
  return 0;
}

int
bgp_pfkey_read(int fd, void **bufp, int *lenp)
{
  struct sadb_msg smsg;
  void *msgbuf;
  int msglen, rlen;

  rlen = recv(fd, &smsg, sizeof(smsg), MSG_PEEK);
  if (rlen == -1)
    {
      zlog_err("pfkey recv: %s", strerror(errno));
      return -1;
    }
  if (rlen != sizeof(smsg))
    {
      zlog_err("pfkey recv: len=%d ret=%d", sizeof(smsg), rlen);
      return -1;
    }

  if (smsg.sadb_msg_errno != 0 ||
      smsg.sadb_msg_errno != ESRCH)
    {
      zlog_err("pfkey read msg: %s", strerror(smsg.sadb_msg_errno));
      read(fd, &smsg, sizeof(smsg)); /* get rid of error message */
      return -1;
    }

  msglen = smsg.sadb_msg_len * SADBALIGN;
  msgbuf = XMALLOC(MTYPE_TMP, msglen);

  rlen = read(fd, msgbuf, msglen);
  if (rlen == -1)
    {
      zlog_err("pfkey read: %s", strerror(errno));
      XFREE(MTYPE_TMP, msgbuf);
      return -1;
    }
  if (rlen != msglen)
    {
      zlog_err("pfkey read: len=%d ret=%d", msglen, rlen);
      XFREE(MTYPE_TMP, msgbuf);
      return -1;
    }

  *bufp = msgbuf;
  *lenp = msglen;

  return 0;
}

int
bgp_pfkey_search_ext(void *buf, u_int16_t exttype, void **prevp)
{
  struct sadb_msg *msg;
  struct sadb_ext *p, *last;
  u_int32_t len;

  msg = buf;
  len = msg->sadb_msg_len * SADBALIGN;
  last = (struct sadb_ext *)(((u_int8_t *)msg) + len);

  p = *prevp ? *prevp : (struct sadb_ext *)(msg + 1);
  while ((u_int8_t *)p < (u_int8_t *)last)
    {
      if (p->sadb_ext_type == exttype)
	{
	  *prevp = p;
	  return 0; /* found */
	}

      p = (struct sadb_ext *)(((u_int8_t *)p) + EXTLEN(p));
    }

  return -1; /* not found */
}

int
bgp_pfkey_getspi(union sockunion *src, union sockunion *dst, u_int32_t *spip)
{
  struct iovec iovbuf[IOVEC_SIZE], *iov;
  int iovcnt, iovrest;
  struct bgp_pfkey_sadb_msg s_msg;
  struct bgp_pfkey_sadb_spirange s_spirange;
  struct bgp_pfkey_sadb_address s_srcaddr, s_dstaddr;
  int len;
  void *buf;
  int buflen;
  struct sadb_sa *ext_sa;

  if (bgp_pfkey_sd == -1)
    {
      bgp_pfkey_init();
      if (bgp_pfkey_sd == -1)
	return -1;
    }

  iov = iovbuf;
  iovcnt = iovrest = sizeof(iov) / sizeof(iov[0]);

  len = bgp_pfkey_build_sadb_msg(SADB_GETSPI, &s_msg, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_spirange(&s_spirange, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(dst, &s_dstaddr, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(src, &s_srcaddr, &iov, &iovrest);

  s_msg.header.sadb_msg_len = len;
  iovcnt -= iovrest;

  if (bgp_pfkey_write(bgp_pfkey_sd, iov, iovcnt) != 0)
    {
      zlog_err("pfkey getspi: fail to write request");
      return -1;
    }

  if (bgp_pfkey_read(bgp_pfkey_sd, &buf, &buflen) != 0)
    {
      zlog_err("pfkey getspi: fail to read reply");
      return -1;
    }

  if (bgp_pfkey_search_ext(buf, SADB_EXT_SA, (void **)&ext_sa) != 0)
    {
      zlog_err("pfkey getspi: no SA extention");
      XFREE(MTYPE_TMP, buf);
      return -1;
    }

  *spip = ext_sa->sadb_sa_spi;

  XFREE(MTYPE_TMP, buf);
  return 0;
}

int
bgp_pfkey_update(u_int32_t spi, union sockunion *src, union sockunion *dst,
		 char *key)
{
  struct iovec iovbuf[IOVEC_SIZE], *iov;
  int iovcnt, iovrest;
  struct bgp_pfkey_sadb_msg s_msg;
  struct bgp_pfkey_sadb_sa s_sa;
  struct bgp_pfkey_sadb_address s_srcaddr, s_dstaddr;
  struct bgp_pfkey_sadb_key s_key;
  int len;
  void *buf;
  int buflen;

  if (bgp_pfkey_sd == -1)
    {
      bgp_pfkey_init();
      if (bgp_pfkey_sd == -1)
	return -1;
    }

  iov = iovbuf;
  iovcnt = iovrest = sizeof(iov) / sizeof(iov[0]);

  len = bgp_pfkey_build_sadb_msg(SADB_UPDATE, &s_msg, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_sa(spi, &s_sa, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(dst, &s_dstaddr, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(src, &s_srcaddr, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_key(key, strlen(key), &s_key, &iov, &iovrest);

  s_msg.header.sadb_msg_len = len;
  iovcnt -= iovrest;

  if (bgp_pfkey_write(bgp_pfkey_sd, iov, iovcnt) != 0)
    {
      zlog_err("pfkey update: fail to write request");
      return -1;
    }

  if (bgp_pfkey_read(bgp_pfkey_sd, &buf, &buflen) != 0)
    {
      zlog_err("pfkey update: fail to read reply");
      return -1;
    }

  XFREE(MTYPE_TMP, buf);

  return 0;
}

int
bgp_pfkey_delete(u_int32_t spi, union sockunion *src, union sockunion *dst)
{
  struct iovec iovbuf[IOVEC_SIZE], *iov;
  int iovcnt, iovrest;
  struct bgp_pfkey_sadb_msg s_msg;
  struct bgp_pfkey_sadb_sa s_sa;
  struct bgp_pfkey_sadb_address s_srcaddr, s_dstaddr;
  int len;
  void *buf;
  int buflen;

  if (bgp_pfkey_sd == -1)
    {
      bgp_pfkey_init();
      if (bgp_pfkey_sd == -1)
	return -1;
    }

  iov = iovbuf;
  iovcnt = iovrest = sizeof(iov) / sizeof(iov[0]);

  len = bgp_pfkey_build_sadb_msg(SADB_DELETE, &s_msg, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_sa(spi, &s_sa, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(dst, &s_dstaddr, &iov, &iovrest);
  len += bgp_pfkey_build_sadb_address(src, &s_srcaddr, &iov, &iovrest);

  s_msg.header.sadb_msg_len = len;
  iovcnt -= iovrest;

  if (bgp_pfkey_write(bgp_pfkey_sd, iov, iovcnt) != 0)
    {
      zlog_err("pfkey delete: fail to write request");
      return -1;
    }

  if (bgp_pfkey_read(bgp_pfkey_sd, &buf, &buflen) != 0)
    {
      zlog_err("pfkey delete: fail to read reply");
      return -1;
    }

  XFREE(MTYPE_TMP, buf);

  return 0;
}

int
bgp_pfkey_init(void)
{
  bgp_pfkey_sd = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
  if (bgp_pfkey_sd == -1)
    {
      zlog_warn("pfkey socket: %s", strerror(errno));
      return -1;
    }
  return 0;
}

/*----------------------------------------------------------------*/

int
bgp_tcpsig_pfkey_set(struct peer *p)
{
  if (!p->update_source)
    return -1;
  
  if (p->spi_out == 0)
    if (bgp_pfkey_getspi(p->update_source, &p->su, &p->spi_out) != 0)
      return -1;

  if (bgp_pfkey_update(p->spi_out,
			 p->update_source, &p->su, p->password) != 0)
    return -1;

  if (p->spi_in == 0)
    if (bgp_pfkey_getspi(&p->su, p->update_source, &p->spi_in) != 0)
      return -1;

  if (bgp_pfkey_update(p->spi_in,
			 &p->su, p->update_source, p->password) != 0)
    return -1;
  
  return 0;
}

int
bgp_tcpsig_pfkey_unset(struct peer *p)
{
  if (!p->update_source)
    return -1;
  
  if (p->spi_out != 0)
    if (bgp_pfkey_delete(p->spi_out, p->update_source, &p->su) != 0)
      return -1;
  p->spi_out = 0;

  if (p->spi_in != 0)
    if (bgp_pfkey_delete(p->spi_in, &p->su, p->update_source) != 0)
      return -1;
  p->spi_in = 0;
  
  return 0;
}

#endif /* HAVE_OPENBSD_TCP_SIGNATURE */

/*----------------------------------------------------------------*/
#ifdef HAVE_LINUX_TCP_SIGNATURE
int
bgp_tcpsig_set (int sock, struct peer *peer)
{
  struct tcp_rfc2385_cmd cmd;
  int ret;

  if (sockunion_family (&peer->su) != AF_INET)
    return 0; /* XXX */

  cmd.command = TCP_MD5_AUTH_ADD;
  cmd.address = peer->su.sin.sin_addr.s_addr;
  cmd.keylen = strlen (peer->password);
  cmd.key = peer->password;

  ret = setsockopt (sock, IPPROTO_TCP, TCP_MD5_AUTH, &cmd, sizeof cmd);

  return ret;
}
#endif /* HAVE_LINUX_TCP_SIGNATURE */

#ifdef HAVE_OPENBSD_TCP_SIGNATURE
int
bgp_tcpsig_set (int sock, struct peer *peer)
{
  int cmd = 1;
  int ret = 0;

  if (peer)
    ret = bgp_tcpsig_pfkey_set(peer);
  
  if (ret == 0 && sock != -1)
    ret = setsockopt (sock, IPPROTO_TCP, TCP_MD5SIG, &cmd, sizeof cmd);

  return ret;
}
#endif /* HAVE_OPENBSD_TCP_SIGNATURE */

#ifdef HAVE_LINUX_TCP_SIGNATURE
int
bgp_tcpsig_unset (int sock, struct peer *peer)
{
  struct tcp_rfc2385_cmd cmd;
  int ret;

  if (sockunion_family (&peer->su) != AF_INET)
    return 0; /* XXX */

  cmd.command = TCP_MD5_AUTH_DEL;
  cmd.address = peer->su.sin.sin_addr.s_addr;
  cmd.keylen = strlen (peer->password);
  cmd.key = peer->password;

  ret = setsockopt (sock, IPPROTO_TCP, TCP_MD5_AUTH, &cmd, sizeof cmd);

  return ret;
}
#endif /* HAVE_LINUX_TCP_SIGNATURE */

#ifdef HAVE_OPENBSD_TCP_SIGNATURE
int
bgp_tcpsig_unset (int sock, struct peer *peer)
{
  return bgp_tcpsig_pfkey_unset(peer);
}
#endif /* HAVE_OPENBSD_TCP_SIGNATURE */
