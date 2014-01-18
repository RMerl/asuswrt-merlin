/** @file lldp_tx_sm.h
 *
 * \brief OpenLLDP TX Statemachine Header
 *
 * This header file contains declarations, variables, and defines 
 * specific to the OpenLLDP transmit state machine.
 *
 * See LICENSE file for more info.
 * 
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef LLDP_TX_SM_H
#define LLDP_TX_SM_H

#ifdef WIN32
#include "stdintwin.h"
#else
#include <stdint.h>
#endif

#include "lldp_port.h"


// States from the transmit state machine diagram (IEEE 802.1AB 10.5.4.3)
#define TX_LLDP_INITIALIZE 0 /**< Initialize state from IEEE 802.1AB 10.5.4.3 */
#define TX_IDLE            1 /**< Idle state from IEEE 802.1AB 10.5.4.3 */
#define TX_SHUTDOWN_FRAME  2 /**< Shutdown state from IEEE 802.1AB 10.5.4.3 */
#define TX_INFO_FRAME      3 /**< Transmit state from IEEE 802.1AB 10.5.4.3 */
// End states from the trnsmit state machine diagram

/* Defined by the 802.1AB specification */
uint8_t txInitializeLLDP(struct lldp_port *lldp_port);
void mibConstrInfoLLDPDU(struct lldp_port *lldp_port);
void mibConstrShutdownLLDPDU();
uint8_t txFrame();
/* End Defined by the 802.1AB specification */

/* Utility functions not defined in the specification */
void txChangeToState(struct lldp_port *lldp_port, uint8_t state);
char *txStateFromID(uint8_t state);
void txGlobalStatemachineRun(struct lldp_port *lldp_port);
void txStatemachineRun(struct lldp_port *lldp_port);
/* End Utility functions not defined in the specification */

void tx_do_tx_lldp_initialize(struct lldp_port *lldp_port);
void tx_do_update_timers(struct lldp_port *lldp_port);
void tx_do_tx_idle(struct lldp_port *lldp_port);
void tx_do_tx_shutdown(struct lldp_port *lldp_port);
void tx_do_tx_info_frame(struct lldp_port *lldp_port);
void tx_display_timers(struct lldp_port *lldp_port);
void tx_do_tx_shutdown_frame(struct lldp_port *lldp_port);

void tx_decrement_timer(uint16_t *timer);

#ifndef WIN32
uint16_t min(uint16_t value1, uint16_t value2);
uint16_t max(uint16_t value1, uint16_t value2);
#endif // WIN32

#endif /* LLDP_TX_SM_H */
