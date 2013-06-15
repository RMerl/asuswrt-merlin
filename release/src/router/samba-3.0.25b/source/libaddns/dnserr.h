/*
  Error codes for Linux DNS client library implementation
  
  Copyright (C) 2006 Krishna Ganugapati <krishnag@centeris.com>
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
  02110-1301  USA
*/

#ifndef _DNSERR_H
#define _DNSERR_H


/* The Splint code analysis tool (http://www.splint.org.) doesn't 
   like immediate structures. */
   
#ifdef _SPLINT_
#undef HAVE_IMMEDIATE_STRUCTURES
#endif

/* Setup the DNS_ERROR typedef.  Technique takes from nt_status.h */

#if defined(HAVE_IMMEDIATE_STRUCTURES)
typedef struct {uint32 v;} DNS_ERROR;
#define ERROR_DNS(x) ((DNS_ERROR) { x })
#define ERROR_DNS_V(x) ((x).v)
#else
typedef uint32 DNS_ERROR;
#define ERROR_DNS(x) (x)
#define ERROR_DNS_V(x) (x)
#endif

#define ERR_DNS_IS_OK(x)   (ERROR_DNS_V(x) == 0)
#define ERR_DNS_EQUAL(x,y) (ERROR_DNS_V(x) == ERROR_DNS_V(y))

/*************************************************
 * Define the error codes here
 *************************************************/

#define ERROR_DNS_SUCCESS		ERROR_DNS(0) 
#define ERROR_DNS_RECORD_NOT_FOUND	ERROR_DNS(1)
#define ERROR_DNS_BAD_RESPONSE		ERROR_DNS(2)
#define ERROR_DNS_INVALID_PARAMETER	ERROR_DNS(3)
#define ERROR_DNS_NO_MEMORY		ERROR_DNS(4)
#define ERROR_DNS_INVALID_NAME_SERVER	ERROR_DNS(5)
#define ERROR_DNS_CONNECTION_FAILED	ERROR_DNS(6)
#define ERROR_DNS_GSS_ERROR		ERROR_DNS(7)
#define ERROR_DNS_INVALID_NAME		ERROR_DNS(8)
#define ERROR_DNS_INVALID_MESSAGE	ERROR_DNS(9)
#define ERROR_DNS_SOCKET_ERROR		ERROR_DNS(10)
#define ERROR_DNS_UPDATE_FAILED		ERROR_DNS(11)

/*
 * About to be removed, transitional error
 */
#define ERROR_DNS_UNSUCCESSFUL		ERROR_DNS(999)


#define ERROR_BAD_RESPONSE		1
#define ERROR_RECORD_NOT_FOUND		2
#define ERROR_OUTOFMEMORY		8
#if !defined(ERROR_INVALID_PARAMETER)
#define ERROR_INVALID_PARAMETER         87
#endif

/*
 * About to be removed, transitional error
 */
#define ERROR_UNSUCCESSFUL 999

#endif	/* _DNSERR_H */

