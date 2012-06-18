/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
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
    vsf_cmdio_write_raw(p_sess, " AUTH SSL\r\n");
    vsf_cmdio_write_raw(p_sess, " AUTH TLS\r\n");
  }
  vsf_cmdio_write_raw(p_sess, " EPRT\r\n");
  vsf_cmdio_write_raw(p_sess, " EPSV\r\n");
  vsf_cmdio_write_raw(p_sess, " MDTM\r\n");
  vsf_cmdio_write_raw(p_sess, " PASV\r\n");
  vsf_cmdio_write_raw(p_sess, " ICNV\r\n");	// Jiahao
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

