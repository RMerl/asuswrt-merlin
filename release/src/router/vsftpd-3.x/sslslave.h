#ifndef VSF_SSLSLAVE_H
#define VSF_SSLSLAVE_H

struct vsf_session;

/* ssl_slave()
 * PURPOSE
 * An internal function that takes care of running the "SSL slave" process. It
 * is needed because the initial SSL handshake state may belong to a different
 * process that the process running the FTP protocol.
 * PARAMETERS
 * p_sess       - the session object
 */
void ssl_slave(struct vsf_session* p_sess);

#endif /* VSF_SSLSLAVE_H */

