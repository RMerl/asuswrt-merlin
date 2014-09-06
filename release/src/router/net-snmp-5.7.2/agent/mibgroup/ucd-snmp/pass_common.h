#ifndef NETSNMP_AGENT_MIBGROUP_PASS_COMMON_H
#define NETSNMP_AGENT_MIBGROUP_PASS_COMMON_H

/*
 * This is an internal header file. The functions declared here might change
 * or disappear at any time
 */

int
netsnmp_internal_pass_str_to_errno(const char *buf);

unsigned char *
netsnmp_internal_pass_parse(char *buf, char *buf2, size_t *var_len,
                            struct variable *vp);

void
netsnmp_internal_pass_set_format(char *buf, const u_char *var_val,
                                 u_char var_val_type, size_t var_val_len);

#endif /* !NETSNMP_AGENT_MIBGROUP_PASS_COMMON_H */
