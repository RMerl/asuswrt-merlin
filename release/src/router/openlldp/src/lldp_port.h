/** @file lldp_port.h
 *
 * OpenLLDP Port Header
 *
 * See LICENSE file for more info.
 * 
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef LLDP_PORT_H
#define LLDP_PORT_H

#ifdef WIN32
#include "stdintwin.h"
#include <BaseTsd.h>
#define ssize_t SSIZE_T
#else
#include <stdint.h>
#include <unistd.h>
#endif // WIN32

#include <time.h>

/**
  Maxiumum ethernet interfaces.

  Used to iterate through the interface list.
  */ 
#define MIN_INTERFACES   1
#define MAX_INTERFACES 254

/** 
  LLDP interface transmission statistics.

  Part of lldp_tx_port.  Tied to the tx state machine.
  */
struct lldp_tx_port_statistics {
    uint64_t statsFramesOutTotal; /**< Defined by IEEE 802.1AB Secion 10.5.2.1 */
};


/**
  LLDP interface transmission timers.

  Part of lldp_tx_port.

  These timers are per-interface, and have a resolution of:
  0 < n < 6553 as per the IEEE 802.1AB specification.
  */
struct lldp_tx_port_timers {
    uint16_t reinitDelay;   /**< IEEE 802.1AB 10.5.3 */
    uint16_t msgTxHold;     /**< IEEE 802.1AB 10.5.3 */
    uint16_t msgTxInterval; /**< IEEE 802.1AB 10.5.3 */
    uint16_t txDelay;       /**< IEEE 802.1AB 10.5.3 */

    uint16_t txTTR;         /**< IEEE 802.1AB 10.5.3 - transmit on expire. */

    /* Not sure what this is for */
    uint16_t txShutdownWhile;
    uint16_t txDelayWhile;
};

/** 
  The LLDP transmit state machine guts.

  This is a per-interface structure.  Part of lldp_port.
  */
struct lldp_tx_port {
    uint8_t *frame;    /**< The tx frame buffer */
    uint64_t sendsize; /**< The size of our tx frame */
    uint8_t state;     /**< The tx state for this interface */
    uint8_t somethingChangedLocal; /**< IEEE 802.1AB var (from where?) */
    uint16_t txTTL;/**< IEEE 802.1AB var (from where?) */
    struct lldp_tx_port_timers timers; /**< The lldp tx state machine timers for this interface */
    struct lldp_tx_port_statistics statistics; /**< The lldp tx statistics for this interface */
};



struct lldp_rx_port_statistics {
    uint64_t statsAgeoutsTotal;
    uint64_t statsFramesDiscardedTotal;
    uint64_t statsFramesInErrorsTotal;
    uint64_t statsFramesInTotal;
    uint64_t statsTLVsDiscardedTotal;
    uint64_t statsTLVsUnrecognizedTotal;
};

struct lldp_rx_port_timers {
    uint16_t tooManyNeighborsTimer;
    uint16_t rxTTL;
};

struct lldp_rx_port {
    uint8_t *frame;
    ssize_t recvsize;
    uint8_t state;
    uint8_t badFrame;
    uint8_t rcvFrame;
    //uint8_t rxChanges; /* This belongs in the MSAP cache */
    uint8_t rxInfoAge;
    uint8_t somethingChangedRemote;
    uint8_t tooManyNeighbors;
    struct lldp_rx_port_timers timers;
    struct lldp_rx_port_statistics statistics;
  //    struct lldp_msap_cache *msap;
};

struct eth_hdr {
    char dst[6];
    char src[6];
    uint16_t ethertype;
};

enum portAdminStatus {
    disabled,
    enabledTxOnly,
    enabledRxOnly,
    enabledRxTx,
};

struct lldp_port {
  struct lldp_port *next;
  int socket;        // The socket descriptor for this interface.
  char *if_name;     // The interface name.
  uint32_t if_index; // The interface index.
  uint32_t mtu;      // The interface MTU.
  uint8_t source_mac[6];
  uint8_t source_ipaddr[4];
  struct lldp_rx_port rx;
  struct lldp_tx_port tx;
  uint8_t portEnabled;
  uint8_t adminStatus;
  
  /* I'm not sure where this goes... the state machine indicates it's per-port */
  uint8_t rxChanges;
  
  // I'm really unsure about the best way to handle this... 
  uint8_t tick;
  time_t last_tick;
  
  struct lldp_msap *msap_cache;


  // 802.1AB Appendix G flag variables.
  uint8_t  auto_neg_status;
  uint16_t auto_neg_advertized_capabilities;
  uint16_t operational_mau_type;
};

struct lldp_msap {
  struct lldp_msap *next;
  uint8_t *id;
  uint8_t length;
  struct lldp_tlv_list *tlv_list;
  
  // XXX Revisit this
  // A pointer to the TTL TLV
  // This is a hack to decrement
  // the timer properly for 
  // lldpneighbors output
  struct lldp_tlv *ttl_tlv;

  /* IEEE 802.1AB MSAP-specific counters */
  uint16_t rxInfoTTL;
};

#endif /* LLDP_PORT_H */
