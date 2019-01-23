#ifndef VSF_PRIVOPS_H
#define VSF_PRIVOPS_H

struct mystr;
struct vsf_session;

/* vsf_privop_get_ftp_port_sock()
 * PURPOSE
 * Return a network socket potentially bound to a privileged port (less than
 * 1024) and connected to the remote.
 * PARAMETERS
 * p_sess            - the current session object
 * remote_port       - the remote port to connect to
 * use_port_sockaddr - true if we should use the specific sockaddr for connect
 * RETURNS
 * A file descriptor which is a socket bound to the privileged port, and
 * connected to the remote on the specified port.
 * Kills the process / session if the bind() fails.
 * Returns -1 if the bind() worked but the connect() was not possible.
 */
int vsf_privop_get_ftp_port_sock(struct vsf_session* p_sess,
                                 unsigned short remote_port,
                                 int use_port_sockaddr);

/* vsf_privop_pasv_cleanup()
 * PURPOSE
 * Makes sure any listening passive socket is closed.
 * PARAMETERS
 * p_sess       - the current session object
 */
void vsf_privop_pasv_cleanup(struct vsf_session* p_sess);

/* vsf_privop_pasv_listen()
 * PURPOSE
 * Start listening for an FTP data connection.
 * PARAMETERS
 * p_sess       - the current session object
 * RETURNS
 * The port we ended up listening on.
 */
unsigned short vsf_privop_pasv_listen(struct vsf_session* p_sess);

/* vsf_privop_pasv_active()
 * PURPOSE
 * Determine whether there is a passive listening socket active.
 * PARAMETERS
 * p_sess       - the current session object
 * RETURNS
 * 1 if active, 0 if not.
 */
int vsf_privop_pasv_active(struct vsf_session* p_sess);

/* vsf_privop_accept_pasv()
 * PURPOSE
 * Accept a connection on the listening data socket.
 * PARAMETERS
 * p_sess       - the current session object
 * RETURNS
 * The file descriptor of the accepted incoming connection; or -1 if a
 * network error occurred or -2 if the incoming connection was from the
 * wrong IP (security issue).
 */
int vsf_privop_accept_pasv(struct vsf_session* p_sess);

/* vsf_privop_do_file_chown()
 * PURPOSE
 * Takes a file owned by the unprivileged FTP user, and change the ownership
 * to the value defined in the config file.
 * PARAMETERS
 * p_sess       - the current session object
 * fd           - the file descriptor of the regular file
 */
void vsf_privop_do_file_chown(struct vsf_session* p_sess, int fd);

enum EVSFPrivopLoginResult
{
  kVSFLoginNull = 0,
  kVSFLoginFail,
  kVSFLoginAnon,
  kVSFLoginReal
};
/* vsf_privop_do_login()
 * PURPOSE
 * Check if the supplied username/password combination is valid. This
 * interface caters for checking both anonymous and real logins.
 * PARAMETERS
 * p_sess       - the current session object
 * p_pass_str   - the proposed password
 * RETURNS
 * kVSFLoginFail - access denied
 * kVSFLoginAnon - anonymous login credentials OK
 * kVSFLoginReal - real login credentials OK
 */
enum EVSFPrivopLoginResult vsf_privop_do_login(
  struct vsf_session* p_sess, const struct mystr* p_pass_str);

#endif /* VSF_PRIVOPS_H */

