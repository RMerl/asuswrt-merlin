#ifndef SNMP_GET_STATISTIC_H
#define SNMP_GET_STATISTIC_H

/** Registers a scalar group with statistics from @ref snmp_get_statistic.
 *  as reginfo.[start, start + end - begin].
 *  @param reginfo registration to register the items under
 *  @param start offset to the begin item
 *  @param begin first snmp_get_statistic key to return
 *  @param end last snmp_get_statistic key to return
 */
int
netsnmp_register_statistic_handler(netsnmp_handler_registration *reginfo,
                                   oid start, int begin, int end);

#define NETSNMP_REGISTER_STATISTIC_HANDLER(reginfo, start, stat)        \
  netsnmp_register_statistic_handler(reginfo, start,                    \
                                     STAT_ ## stat ## _STATS_START,     \
                                     STAT_ ## stat ## _STATS_END)

#endif
