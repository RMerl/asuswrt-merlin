#ifndef VSF_SECCOMPSANDBOX_H
#define VSF_SECCOMPSANDBOX_H

struct vsf_session;

void seccomp_sandbox_init();

void seccomp_sandbox_setup_prelogin(const struct vsf_session* p_sess);

void seccomp_sandbox_setup_postlogin(const struct vsf_session* p_sess);

void seccomp_sandbox_setup_postlogin_broker();

void seccomp_sandbox_lockdown();

#endif /* VSF_SECCOMPSANDBOX_H */

