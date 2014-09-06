/*
 * snmpd.h
 */

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_AGENT 0
#define SUB_AGENT    1
extern int      agent_role;

extern int      snmp_dump_packet;
extern int      verbose;
extern int      (*sd_handlers[]) (int);
extern int      smux_listen_sd;

extern int      snmp_read_packet(int);

/*
 * config file parsing routines 
 */
void            agentBoots_conf(char *, char *);

#ifdef __cplusplus
}
#endif
