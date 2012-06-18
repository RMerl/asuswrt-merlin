/* pptp_options.h ...... various constants used in the PPTP protocol.
 *                       #define STANDARD to emulate NT 4.0 exactly.
 *                       C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: pptp_options.h,v 1.3 2004/11/09 01:42:32 quozl Exp $
 */

#ifndef INC_PPTP_OPTIONS_H
#define INC_PPTP_OPTIONS_H

#undef  PPTP_FIRMWARE_STRING
#undef  PPTP_FIRMWARE_VERSION
#define PPTP_BUF_MAX 65536
#define PPTP_TIMEOUT 60 /* seconds */
extern int idle_wait;
extern int max_echo_wait;
#define PPTP_CONNECT_SPEED 10000000
#define PPTP_WINDOW 3
#define PPTP_DELAY  0
#define PPTP_BPS_MIN 2400
#define PPTP_BPS_MAX 10000000

#ifndef STANDARD
#define PPTP_MAX_CHANNELS 65535
#define PPTP_FIRMWARE_STRING "0.01"
#define PPTP_FIRMWARE_VERSION 0x001
#define PPTP_HOSTNAME {'l','o','c','a','l',0}
#define PPTP_VENDOR   {'c','a','n','a','n','i','a','n',0}
#define PPTP_FRAME_CAP  PPTP_FRAME_ANY
#define PPTP_BEARER_CAP PPTP_BEARER_ANY
#else
#define PPTP_MAX_CHANNELS 5
#define PPTP_FIRMWARE_STRING "0.01"
#define PPTP_FIRMWARE_VERSION 0
#define PPTP_HOSTNAME {'l','o','c','a','l',0}
#define PPTP_VENDOR   {'N','T',0}
#define PPTP_FRAME_CAP  2
#define PPTP_BEARER_CAP 1
#endif

#endif /* INC_PPTP_OPTIONS_H */
