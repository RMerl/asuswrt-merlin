#ifndef VSF_PRELOGIN_H
#define VSF_PRELOGIN_H

struct vsf_session;

/* init_connection()
 * PURPOSE
 * Called as an entry point to FTP protocol processing, when a client connects.
 * This function will emit the FTP greeting, then start talking FTP protocol
 * to the client.
 * PARAMETERS
 * p_sess         - the current session object
 */
void init_connection(struct vsf_session* p_sess);

#endif /* VSF_PRELOGIN_H */
