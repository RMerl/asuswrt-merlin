/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * features.c
 *
 * Routines to tell the client what features we support.
 */

#include "features.h"
#include "ftpcodes.h"
#include "ftpcmdio.h"
#include "tunables.h"

void
handle_feat(struct vsf_session* p_sess)
{
  vsf_cmdio_write_hyphen(p_sess, FTP_FEAT, "Features:");
  if (tunable_ssl_enable)
  {
    if (tunable_sslv2 || tunable_sslv3)
    {
      vsf_cmdio_write_raw(p_sess, " AUTH SSL\r\n");
    }
    if (tunable_tlsv1)
    {
      vsf_cmdio_write_raw(p_sess, " AUTH TLS\r\n");
    }
  }
  if (tunable_port_enable)
  {
    vsf_cmdio_write_raw(p_sess, " EPRT\r\n");
  }
  if (tunable_pasv_enable)
  {
    vsf_cmdio_write_raw(p_sess, " EPSV\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " MDTM\r\n");
  if (tunable_pasv_enable)
  {
    vsf_cmdio_write_raw(p_sess, " PASV\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " ICNV\r\n");
  if (tunable_ssl_enable)
  {
    vsf_cmdio_write_raw(p_sess, " PBSZ\r\n");
    vsf_cmdio_write_raw(p_sess, " PROT\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " REST STREAM\r\n");
  vsf_cmdio_write_raw(p_sess, " SIZE\r\n");
  vsf_cmdio_write_raw(p_sess, " TVFS\r\n");
  vsf_cmdio_write_raw(p_sess, " UTF8\r\n");
  vsf_cmdio_write(p_sess, FTP_FEAT, "End");
}

