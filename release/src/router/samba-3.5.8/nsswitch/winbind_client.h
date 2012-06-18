#include "winbind_nss_config.h"
#include "winbind_struct_protocol.h"

void winbindd_init_request(struct winbindd_request *req,int rq_type);
void winbindd_free_response(struct winbindd_response *response);
NSS_STATUS winbindd_send_request(int req_type, int need_priv,
				 struct winbindd_request *request);
NSS_STATUS winbindd_get_response(struct winbindd_response *response);
NSS_STATUS winbindd_request_response(int req_type,
			    struct winbindd_request *request,
			    struct winbindd_response *response);
NSS_STATUS winbindd_priv_request_response(int req_type,
					  struct winbindd_request *request,
					  struct winbindd_response *response);
int winbindd_read_reply(struct winbindd_response *response);

#define winbind_env_set() \
	(strcmp(getenv(WINBINDD_DONT_ENV)?getenv(WINBINDD_DONT_ENV):"0","1") == 0)

#define winbind_off() \
	(setenv(WINBINDD_DONT_ENV, "1", 1) == 0)

#define winbind_on() \
	(setenv(WINBINDD_DONT_ENV, "0", 1) == 0)

int winbind_write_sock(void *buffer, int count, int recursing, int need_priv);
int winbind_read_sock(void *buffer, int count);
void winbind_close_sock(void);

const char *nss_err_str(NSS_STATUS ret);
