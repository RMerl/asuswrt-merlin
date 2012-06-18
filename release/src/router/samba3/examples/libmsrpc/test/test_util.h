#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "libmsrpc.h"

/*prototypes*/
void cactest_GetAuthDataFn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword);


void cactest_print_usage(char **argv);
void cac_parse_cmd_line(int argc, char **argv, CacServerHandle *hnd);
void print_value(uint32 type, REG_VALUE_DATA *data);
void cactest_readline(FILE *in, fstring line);
void cactest_reg_input_val(TALLOC_CTX *mem_ctx, int *type, char **name, REG_VALUE_DATA *data);
void print_cac_user_info(CacUserInfo *info);
void edit_cac_user_info(TALLOC_CTX *mem_ctx, CacUserInfo *info);
void print_cac_group_info(CacGroupInfo *info);
void edit_cac_group_info(TALLOC_CTX *mem_ctx, CacGroupInfo *info);
void print_cac_domain_info(CacDomainInfo *info);
void print_cac_service(CacService svc);
void print_service_status(SERVICE_STATUS status);
void print_service_config(CacServiceConfig *config);

#endif /*TEST_UTIL_H*/
