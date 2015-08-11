/* $Id: extra-exports.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <linux/module.h>
#include <linux/syscalls.h>

EXPORT_SYMBOL(sys_select);

EXPORT_SYMBOL(sys_epoll_create);
EXPORT_SYMBOL(sys_epoll_ctl);
EXPORT_SYMBOL(sys_epoll_wait);

EXPORT_SYMBOL(sys_socket);
EXPORT_SYMBOL(sys_bind);
EXPORT_SYMBOL(sys_getpeername);
EXPORT_SYMBOL(sys_getsockname);
EXPORT_SYMBOL(sys_sendto);
EXPORT_SYMBOL(sys_recvfrom);
EXPORT_SYMBOL(sys_getsockopt);
EXPORT_SYMBOL(sys_setsockopt);
EXPORT_SYMBOL(sys_listen);
EXPORT_SYMBOL(sys_shutdown);
EXPORT_SYMBOL(sys_connect);
EXPORT_SYMBOL(sys_accept);

