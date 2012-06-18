/*
 * $Id: $
 * SSL Routines
 *
 * Copyright (C) 2006 Ron Pedde (rpedde@users.sourceforge.net)
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

#ifndef _SSL_H_
#define _SSL_H_

#ifdef USE_SSL

extern int ws_ssl_init(char *keyfile, char *cert, char *password);
extern void ws_ssl_deinit(void);
extern int ws_ssl_write(WS_CONNINFO *pwsc, unsigned char *buffer, int len);
extern int ws_ssl_read(WS_CONNINFO *pwsc, unsigned char *buffer, int len);
extern void ws_ssl_shutdown(WS_CONNINFO *pwsc);

#endif /* SSL */
#endif /* _SSL_H_ */
