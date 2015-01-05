#ifndef VSF_PRIVSOCK_H
#define VSF_PRIVSOCK_H

struct mystr;
struct vsf_session;

/* priv_sock_init()
 * PURPOSE
 * Initialize the priv_sock system, by opening the communications sockets.
 *
 * PARAMETERS
 * p_sess       - the current session object
 */
void priv_sock_init(struct vsf_session* p_sess);

/* priv_sock_close()
 * PURPOSE
 * Closes any open file descriptors relating to the priv_sock system.
 *
 * PARAMETERS
 * p_sess       - the current session object
 */
void priv_sock_close(struct vsf_session* p_sess);

/* priv_sock_set_parent_context()
 * PURPOSE
 * Closes the child's fd, e.g. p_sess->child_fd.
 *
 * PARAMETERS
 * p_sess       - the current session object
 */
void priv_sock_set_parent_context(struct vsf_session* p_sess);

/* priv_sock_set_child_context()
 * PURPOSE
 * Closes the parent's fd, e.g. p_sess->parent_fd.
 *
 * PARAMETERS
 * p_sess       - the current session object
 */
void priv_sock_set_child_context(struct vsf_session* p_sess);

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

/* priv_sock_send_buf()
 * PURPOSE
 * Sends a buffer to the other side of the channel. The protocol used is the
 * same as priv_sock_send_str()
 * PARAMETERS
 * fd           - the fd on which to send the buffer
 * p_buf        - the buffer to send
 * len          - length of the buffer
 */
void priv_sock_send_buf(int fd, const char* p_buf, unsigned int len);

/* priv_sock_recv_buf()
 * PURPOSE
 * Receives a buffer from the other side of the channel. The protocol used is
 * the same as priv_sock_recv_str()
 * PARAMETERS
 * fd           - the fd on which to receive the buffer
 * p_buf        - the buffer to write into
 * len          - length of the buffer
 */
void priv_sock_recv_buf(int fd, char* p_buf, unsigned int len);

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
#define PRIV_SOCK_DO_SSL_HANDSHAKE  6
#define PRIV_SOCK_DO_SSL_CLOSE      7
#define PRIV_SOCK_DO_SSL_READ       8
#define PRIV_SOCK_DO_SSL_WRITE      9
#define PRIV_SOCK_PASV_CLEANUP      10
#define PRIV_SOCK_PASV_ACTIVE       11
#define PRIV_SOCK_PASV_LISTEN       12
#define PRIV_SOCK_PASV_ACCEPT       13

#define PRIV_SOCK_RESULT_OK         1
#define PRIV_SOCK_RESULT_BAD        2

#endif /* VSF_PRIVSOCK_H */

