/** @file lldp_rx_sm.h
 *
 * OpenLLDP RX Statemachine Header
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_sm_rx.h
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef LLDP_RX_SM_H
#define LLDP_RX_SM_H

#ifdef WIN32
#include "stdintwin.h"
#else
#include <stdint.h>
#endif // WIN32
#include "lldp_port.h"

// States from the receive state machine diagram (IEEE 802.1AB 10.5.5.3)
#define LLDP_WAIT_PORT_OPERATIONAL 4
#define DELETE_AGED_INFO           5
#define RX_LLDP_INITIALIZE         6
#define RX_WAIT_FOR_FRAME          7
#define RX_FRAME                   8
#define DELETE_INFO                9
#define UPDATE_INFO                10


/* Defined by the 802.1AB specification */
uint8_t mibDeleteObjects(struct lldp_port *lldp);
uint8_t mibUpdateObjects(struct lldp_port *lldp);
uint8_t rxInitializeLLDP(struct lldp_port *lldp);
int rxProcessFrame(struct lldp_port *lldp_port);
/* End Defined by the 802.1AB specification */

/* Utility functions not defined in the specification */
void rxChangeToState(struct lldp_port *lldp_port, uint8_t state);
void rxBadFrameInfo(uint8_t badFrameCount);
char *rxStateFromID(uint8_t state);
void rxStatemachineRun(struct lldp_port *lldp_port);
/* End Utility functions not defined in the specification */

/* Receive sm "do" functions */
void rx_do_lldp_wait_port_operational(struct lldp_port *lldp_port);
void rx_do_delete_aged_info(struct lldp_port *lldp_port);
void rx_do_rx_lldp_initialize(struct lldp_port *lldp_port);
void rx_do_rx_wait_for_frame(struct lldp_port *lldp_port);
void rx_do_rx_frame(struct lldp_port *port);
void rx_do_rx_delete_info(struct lldp_port *lldp_port);
void rx_do_rx_update_info(struct lldp_port *lldp_port);
void rx_do_update_timers(struct lldp_port *lldp_port);

/* End Receive sm "do" functions */

void rx_display_timers(struct lldp_port *lldp_port);

#endif /* LLDP_RX_SM_H */
