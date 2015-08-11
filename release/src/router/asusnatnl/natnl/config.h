#ifndef __CONFIG_DLL_H__
#define __CONFIG_DLL_H__

#include <getopt.h>
#include <natnl_lib.h>

#ifdef  __cplusplus
extern "C" {
#endif

extern char callee[128],
	registrar_uri[128];

#define DEV_ID1 "80bd8155a2540ef1e87ea2f811390e5d"
#define DEV_ID2 "ab8806d6e27ea16cd8c4557f8cb3179c"
#define SIP_SERVER "ec2-50-17-15-111.compute-1.amazonaws.com"
#define REGISTRAR_URI "sip:"SIP_SERVER
#define STUN_SERVER "stun.xten.com"
#define TURN_SERVER "numb.viagenie.ca"
#define TURN_USR "dean_li@asus.com"
#define TURN_PWD "asus"

extern int my_read_config_file(const char *filename, 
			    int *app_argc, char ***app_argv);
extern int my_parse_args(int argc, char *argv[],
					struct natnl_config *natnl_cfg,
					int *srv_port_count,
					struct natnl_srv_port srv_port_cfg[]);

#ifdef  __cplusplus
}
#endif

#endif 	/* __CONFIG_DLL_H__ */