#ifndef AFPD_STATUS_H
#define AFPD_STATUS_H 1

#include <atalk/dsi.h>
#include <atalk/globals.h>

#include "afp_config.h"

/* we use these to prevent whacky alignment problems */
#define AFPSTATUS_MACHOFF     0
#define AFPSTATUS_VERSOFF     2
#define AFPSTATUS_UAMSOFF     4
#define AFPSTATUS_ICONOFF     6
#define AFPSTATUS_FLAGOFF     8

/* AFPSTATUS_PRELEN is the number of bytes for status data prior to 
 * the ServerName field.
 *
 * This is two bytes of offset space for the MachineType, AFPVersionCount,
 * UAMCount, VolumeIconAndMask, and the 16-bit "Fixed" status flags.
 */
#define AFPSTATUS_PRELEN     10

/* AFPSTATUS_POSTLEN is the number of bytes for offset records
 * after the ServerName field.
 *
 * Right now, this is 2 bytes each for ServerSignature, networkAddressCount,
 * DirectoryNameCount, and UTF-8 ServerName
 */
#define AFPSTATUS_POSTLEN     8
#define AFPSTATUS_LEN        (AFPSTATUS_PRELEN + AFPSTATUS_POSTLEN)

/* AFPSTATUS_MACHLEN is the number of characters for the MachineType. */
#define AFPSTATUS_MACHLEN     16

extern void status_versions (char * /*status*/, const DSI *);
extern void status_uams (char * /*status*/, const char * /*authlist*/);
extern void status_init (AFPObj *, DSI *dsi);
extern void set_signature(struct afp_options *);

/* FP functions */
int afp_getsrvrinfo (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

#endif
