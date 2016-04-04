int ej_dsl_get_parameter(int eid, webs_t wp, int argc, char_t **argv);
int wanlink_hook_dsl(int eid, webs_t wp, int argc, char_t **argv);
void do_adsllog_cgi(char *path, FILE *stream);
int ej_get_isp_list(int eid, webs_t wp, int argc, char_t **argv);
int ej_get_isp_dhcp_opt_list(int eid, webs_t wp, int argc, char_t **argv);
int ej_get_DSL_WAN_list(int eid, webs_t wp, int argc, char_t **argv);
int update_dsl_iptv_variables(void);
