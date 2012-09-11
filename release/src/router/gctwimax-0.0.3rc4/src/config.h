#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>

#include "utils/includes.h"
#include "utils/common.h"
#include "eap_common/eap_defs.h"
#include "eap_peer/eap_methods.h"
#include "eap_peer/eap.h"

#ifndef BIT
#define BIT(x) (1 << (x))
#endif


struct gct_config {
	uint32_t	nspid;	
	int 		wimax_verbose_level;
	int 		wpa_debug_level;
	char * 		log_file;
	char * 		event_script;

	int 		use_pkm;
	int 		use_nv;
	int 		eap_type;
	int 		ca_cert_null;
	int 		dev_cert_null;
	int 		cert_nv;
	uint8_t *	anonymous_identity;
	size_t 		anonymous_identity_len;
	uint8_t *	identity;	
	size_t 		identity_len;
	uint8_t * 	password;
	size_t 		password_len;
	uint8_t * 	ca_cert;
	uint8_t * 	client_cert;
	uint8_t * 	private_key;
	uint8_t * 	private_key_passwd;
	struct eap_method_type *eap_methods;
	char * 		phase1;
	char * 		phase2;
#define EAP_CONFIG_FLAGS_PASSWORD_NTHASH BIT(0)
	uint32_t 	flags;	
};


struct gct_config * gct_config_read(const char *name);
void gct_config_free(struct gct_config *config);
char * rel2abs_path(const char *rel_path);

#endif /*_CONFIG_H*/
