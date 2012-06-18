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
#ifndef VSF_PRIVSOCK_H
#define VSF_PRIVSOCK_H

struct mystr;
struct vsf_session;

/* priv_sock_init()
 * PURPOSE
 * Initialize the priv_sock system, by opening the communications sockets.
 * PARAMETERS
 * p_sess       - the current session object
 */
void priv_sock_init(struct vsf_session* p_sess);

/* priv_sock_send_cmd()
 * PURPOSE
 * Sends a command, typically to the privileged side of the channel.
 * PARAMETERS
 * fd           - the fd on which to send the command
 * cmd          - the command to send
 */
void priv_sock_send_cmd(int fd, char cmd);

/* priv_sock_send_str()
 * PURPOSE
 * Sends a string to the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to send the string
 * p_str        - the string to send
 */
void priv_sock_send_str(int fd, const struct mystr* p_str);

/* priv_sock_get_result()
 * PURPOSE
 * Receives a response, typically from the privileged side of the channel.
 * PARAMETERS
 * fd           - the fd on which to receive the response
 * RETURNS
 * The response code.
 */
char priv_sock_get_result(int fd);

/* priv_sock_get_cmd()
 * PURPOSE
 * Receives a command, typically on the privileged side of the channel.
 * PARAMETERS
 * fd           - the fd on which to receive the command.
 * RETURNS
 * The command that was sent.
 */
char priv_sock_get_cmd(int fd);

/* priv_sock_get_str()
 * PURPOSE
 * Receives a string from the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to receive the string
 * p_dest       - where to copy the received string
 */
void priv_sock_get_str(int fd, struct mystr* p_dest);

/* priv_sock_send_result()
 * PURPOSE
 * Sends a command result, typically to the unprivileged side of the channel.
 * PARAMETERS
 * fd           - the fd on which to send the result
 * res          - the result to send
 */
void priv_sock_send_result(int fd, char res);

/* priv_sock_send_fd()
 * PURPOSE
 * Sends a file descriptor to the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to send the descriptor 
 * send_fd      - the descriptor to send
 */
void priv_sock_send_fd(int fd, int send_fd);

/* priv_sock_recv_fd()
 * PURPOSE
 * Receives a file descriptor from the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to receive the descriptor
 * RETURNS
 * The received file descriptor
 */
int priv_sock_recv_fd(int fd);

/* priv_sock_send_int()
 * PURPOSE
 * Sends an integer to the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to send the integer
 * the_int      - the integer to send
 */
void priv_sock_send_int(int fd, int the_int);

/* priv_sock_get_int()
 * PURPOSE
 * Receives an integer from the other side of the channel.
 * PARAMETERS
 * fd           - the fd on which to receive the integer
 * RETURNS
 * The integer that was sent.
 */
int priv_sock_get_int(int fd);

#define PRIV_SOCK_LOGIN             1
#define PRIV_SOCK_CHOWN             2
#define PRIV_SOCK_GET_DATA_SOCK     3
#define PRIV_SOCK_GET_USER_CMD      4
#define PRIV_SOCK_WRITE_USER_RESP   5

#define PRIV_SOCK_RESULT_OK         1
#define PRIV_SOCK_RESULT_BAD        2

#endif /* VSF_PRIVSOCK_H */

