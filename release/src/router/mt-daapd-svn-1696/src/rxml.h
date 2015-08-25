/*
 * $Id: rxml.h 779 2006-02-26 08:46:24Z rpedde $
 */

#ifndef _RXML_H_
#define _RXML_H_

#define RXML_EVT_OPEN     0x0
#define RXML_EVT_CLOSE    0x1
#define RXML_EVT_BEGIN    0x2
#define RXML_EVT_END      0x3
#define RXML_EVT_TEXT     0x4

typedef void* RXMLHANDLE;
typedef void(*RXML_EVTHANDLER)(int,void*,char*);

extern int rxml_open(RXMLHANDLE *handle, char *file, 
                     RXML_EVTHANDLER handler, void* udata);
extern int rxml_close(RXMLHANDLE handle);
extern char *rxml_errorstring(RXMLHANDLE handle);
extern int rxml_parse(RXMLHANDLE handle);

#endif /* _ RXML_H_ */
