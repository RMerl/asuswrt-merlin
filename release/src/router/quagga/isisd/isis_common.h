/*
 * IS-IS Rout(e)ing protocol - isis_common.h  
 *                             some common data structures
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
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

#ifndef ISIS_COMMON_H
#define ISIS_COMMON_H

/*
 * Area Address
 */
struct area_addr
{
  u_char addr_len;
  u_char area_addr[20];
};

struct isis_passwd
{
  u_char len;
#define ISIS_PASSWD_TYPE_UNUSED   0
#define ISIS_PASSWD_TYPE_CLEARTXT 1
#define ISIS_PASSWD_TYPE_HMAC_MD5 54
#define ISIS_PASSWD_TYPE_PRIVATE  255
  u_char type;
  /* Authenticate SNPs? */
#define SNP_AUTH_SEND   0x01
#define SNP_AUTH_RECV   0x02
  u_char snp_auth;
  u_char passwd[255];
};

/*
 * (Dynamic) Hostname
 * one struct for cache list
 * one struct for LSP TLV
 */
struct hostname
{
  u_char namelen;
  u_char name[255];
};

/*
 * Supported Protocol IDs
 */
struct nlpids
{
  u_char count;
  u_char nlpids[4];		/* FIXME: enough ? */
};

#endif
