#ifndef SNMPTRAPD_DS_H
#define SNMPTRAPD_DS_H

/* booleans
 *
 * WARNING: These must not conflict with the agent's DS booleans
 * If you define additional entries here, check in <agent/ds_agent.h> first
 *  (and consider repeating the definitions there) */

#define NETSNMP_DS_APP_NUMERIC_IP       16
#define NETSNMP_DS_APP_NO_AUTHORIZATION 17

/*
 * NB: The NETSNMP_DS_APP_NO_AUTHORIZATION definition is repeated
 *     in the code file agent/mibgroup/mibII/vacm_conf.c
 *     If this definition is changed, it should be updated there too.
 */

#endif /* SNMPTRAPD_DS_H */
