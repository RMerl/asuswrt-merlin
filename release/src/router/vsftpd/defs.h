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
#ifndef VSF_DEFS_H
#define VSF_DEFS_H

#define VSFTP_DEFAULT_CONFIG "/tmp/vsftpd.conf" //Yen

#define VSFTP_COMMAND_FD        0
#define VSFTP_PASSWORD_MAX      128
#define VSFTP_USERNAME_MAX      32
#define VSFTP_MAX_COMMAND_LINE  4096
#define VSFTP_DATA_BUFSIZE      65536
#define VSFTP_DIR_BUFSIZE       16384
#define VSFTP_PATH_MAX          4096
#define VSFTP_CONF_FILE_MAX     100000
#define VSFTP_LISTEN_BACKLOG    32
// 2008.05 James. {
//#define VSFTP_SECURE_UMASK      077
#define VSFTP_SECURE_UMASK      000
// 2008.05 James. }
#define VSFTP_ROOT_UID          0
/* Must be greater than both VSFTP_MAX_COMMAND_LINE and VSFTP_DIR_BUFSIZE */
#define VSFTP_PRIVSOCK_MAXSTR   VSFTP_DIR_BUFSIZE

#endif /* VSF_DEFS_H */

