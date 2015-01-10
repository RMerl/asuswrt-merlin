#ifndef VSF_POSTLOGIN_H
#define VSF_POSTLOGIN_H

struct vsf_session;

/* process_post_login()
 * PURPOSE
 * Called to begin FTP protocol parsing for a logged in session.
 * PARAMETERS
 * p_sess       - the current session object
 */
void process_post_login(struct vsf_session* p_sess);

#endif /* VSF_POSTLOGIN_H */

