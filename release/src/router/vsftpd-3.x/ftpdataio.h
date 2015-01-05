#ifndef VSF_FTPDATAIO_H
#define VSF_FTPDATAIO_H

#include "filesize.h"

struct mystr;
struct vsf_sysutil_sockaddr;
struct vsf_sysutil_dir;
struct vsf_session;

/* vsf_ftpdataio_dispose_transfer_fd()
 * PURPOSE
 * Close down the remote data transfer file descriptor. If unsent data reamins
 * on the connection, this method blocks until it is transferred (or the data
 * timeout goes off, or the connection is severed).
 * PARAMETERS
 * p_sess       - the current FTP session object
 * RETURNS
 * 1 on success, 0 otherwise.
 * 
 */
int vsf_ftpdataio_dispose_transfer_fd(struct vsf_session* p_sess);

/* vsf_ftpdataio_get_pasv_fd()
 * PURPOSE
 * Return a connection data file descriptor obtained by the PASV connection
 * method. This includes accept()'ing a connection from the remote.
 * PARAMETERS
 * p_sess       - the current FTP session object
 * RETURNS
 * The file descriptor upon success, or -1 upon error.
 */
int vsf_ftpdataio_get_pasv_fd(struct vsf_session* p_sess);

/* vsf_ftpdataio_get_pasv_fd()
 * PURPOSE
 * Return a connection data file descriptor obtained by the PORT connection
 * method. This includes connect()'ing to the remote.
 * PARAMETERS
 * p_sess       - the current FTP session object
 * RETURNS
 * The file descriptor upon success, or -1 upon error.
 */
int vsf_ftpdataio_get_port_fd(struct vsf_session* p_sess);

/* vsf_ftpdataio_post_mark_connect()
 * PURPOSE
 * Perform any post-150-status-mark setup on the data connection. For example,
 * the negotiation of SSL.
 * PARAMETERS
 * p_sess       - the current FTP session object
 * RETURNS
 * 1 on success, 0 otherwise.
 */
int vsf_ftpdataio_post_mark_connect(struct vsf_session* p_sess);

/* vsf_ftpdataio_transfer_file()
 * PURPOSE
 * Send data between the network and a local file. Send and receive are
 * supported, as well as ASCII mangling.
 * PARAMETERS
 * remote_fd    - the file descriptor of the remote data connection
 * file_fd      - the file descriptor of the local file
 * is_recv      - 0 for sending to the remote, otherwise receive
 * is_ascii     - non zero for ASCII mangling
 * RETURNS
 * A structure, containing
 * retval       - 0 for success, failure otherwise
 *                (-1 = local problem -2 = remote problem)
 * transferred  - number of bytes transferred
 */
struct vsf_transfer_ret
{
  int retval;
  filesize_t transferred;
};
struct vsf_transfer_ret vsf_ftpdataio_transfer_file(
  struct vsf_session* p_sess,
  int remote_fd, int file_fd, int is_recv, int is_ascii);

/* vsf_ftpdataio_transfer_dir()
 * PURPOSE
 * Send an ASCII directory lising of the requested directory to the remote
 * client.
 * PARAMETERS
 * p_sess         - the current session object
 * is_control     - whether to send on the control connection or data connection
 * p_dir          - the local directory object
 * p_base_dir_str - the directory we opened relative to the current one
 * p_option_str   - the options list provided to "ls"
 * p_filter_str   - the filter string provided to "ls"
 * is_verbose     - set to 0 if NLST used, 1 if LIST used
 */
int vsf_ftpdataio_transfer_dir(struct vsf_session* p_sess, int is_control,
                               struct vsf_sysutil_dir* p_dir,
                               const struct mystr* p_base_dir_str,
                               const struct mystr* p_option_str,
                               const struct mystr* p_filter_str,
                               int is_verbose);

#endif /* VSF_FTPDATAIO_H */

