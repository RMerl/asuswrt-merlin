/*
 * $Id: $
 *
 * Created by Ron Pedde on 2007-07-01.
 * Copyright (C) 2007 Firefly Media Services. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _IO_ERRORS_H_
#define _IO_ERRORS_H_


#define IO_E_OTHER           0x00000000 /**< Native error */
#define IO_E_BADPROTO        0x01000001 /**< Bad protocol type, unhandled URI */
#define IO_E_NOTINIT         0x01000002 /**< io object not initialized with new */
#define IO_E_BADFN           0x01000003 /**< uniplemented io function */
#define IO_E_INTERNAL        0x01000004 /**< something fatal internally */

#define IO_E_FILE_OTHER      0x00000000 /**< Native error */
#define IO_E_FILE_NOTOPEN    0x02000001 /**< operation on closed file */
#define IO_E_FILE_UNKNOWN    0x02000002 /**< other inernal error */

#define IO_E_SOCKET_OTHER    0x00000000 /**< Native error */
#define IO_E_SOCKET_NOTOPEN  0x03000001 /**< operation on closed socket */
#define IO_E_SOCKET_UNKNOWN  0x03000002 /**< other internal error */
#define IO_E_SOCKET_BADHOST  0x03000003 /**< can't resolve address */
#define IO_E_SOCKET_NOTINIT  0x03000004 /**< socket not initialized with new */
#define IO_E_SOCKET_BADFN    0x03000005 /**< unimplemented io function */
#define IO_E_SOCKET_NOMCAST  0x03000006 /**< can't set up multicast */
#define IO_E_SOCKET_INVALID  0x03000007 /**< ? */
#define IO_E_SOCKET_INUSE    0x03000008 /**< EADDRINUSE */

#endif /* _IO_ERRORS_H_ */



