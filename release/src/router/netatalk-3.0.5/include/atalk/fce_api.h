/* 
 * File:   fce_api.h
 * Author: mw
 *
 * Created on 1. Oktober 2010, 21:35
 *
 * API calls for file change event api
 */

#ifndef _FCE_API_H
#define	_FCE_API_H

#include <atalk/globals.h>

/* fce_packet.mode */
#define FCE_FILE_MODIFY     1
#define FCE_FILE_DELETE     2
#define FCE_DIR_DELETE      3
#define FCE_FILE_CREATE     4
#define FCE_DIR_CREATE      5
#define FCE_CONN_START     42
#define FCE_CONN_BROKEN    99

#define FCE_FIRST_EVENT     FCE_FILE_MODIFY /* keep in sync with last file event above */
#define FCE_LAST_EVENT      FCE_DIR_CREATE  /* keep in sync with last file event above */

/* fce_packet.fce_magic */
#define FCE_PACKET_MAGIC  "at_fcapi"

/* This packet goes over the network, so we want to
 * be shure about datastructs and type sizes between platforms.
 * Format is network byte order.
 */
#define FCE_PACKET_HEADER_SIZE 8+1+1+4+2

struct fce_packet
{
    char magic[8];
    unsigned char version;
    unsigned char mode;
    uint32_t event_id;
    uint16_t datalen;
    char data[MAXPATHLEN];
};

typedef uint32_t fce_ev_t;
typedef enum { fce_file, fce_dir } fce_obj_t;

struct path;
struct ofork;

void fce_pending_events(AFPObj *obj);
int fce_register(fce_ev_t event, const char *path, const char *oldpath, fce_obj_t type);
int fce_add_udp_socket(const char *target );  // IP or IP:Port
int fce_set_coalesce(const char *coalesce_opt ); // all|delete|create
int fce_set_events(const char *events);     /* fmod,fdel,ddel,fcre,dcre */

#define FCE_DEFAULT_PORT 12250
#define FCE_DEFAULT_PORT_STRING "12250"

#endif	/* _FCE_API_H */

