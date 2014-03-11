/** @file lldp_platform.h
 * 
 * See LICENSE file for more info.
 *
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 **/

#ifndef LLDP_PLATFORM_H

ssize_t lldp_read(struct lldp_port *lldp_port);
ssize_t lldp_write(struct lldp_port *lldp_port);

int socketInitializeLLDP(struct lldp_port *lldp_port);
void socketCleanupLLDP(struct lldp_port *lldp_port);

void refreshInterfaceData(struct lldp_port *lldp_port);

#endif // LLDP_PLATFORM_H
