#ifndef VSF_LOGINPRIVPARENT_H
#define VSF_LOGINPRIVPARENT_H

struct vsf_session;

/* vsf_priv_parent_postlogin()
 * PURPOSE
 * Called in the two process security model to commence "listening" for
 * requests from the unprivileged child.
 * PARAMETERS
 * p_sess       - the current session object
 */
void vsf_priv_parent_postlogin(struct vsf_session* p_sess);

#endif /* VSF_LOGINPRIVPARENT_H */

