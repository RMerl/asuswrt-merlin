

#ifndef _FF_PLUGIN_EVENTS_H_
#define _FF_PLUGIN_EVENTS_H_

/* Plugin types */
#define PLUGIN_OUTPUT     1
#define PLUGIN_SCANNER    2
#define PLUGIN_DATABASE   4
#define PLUGIN_EVENT      8
#define PLUGIN_TRANSCODE 16

/* plugin event types */
#define PLUGIN_EVENT_LOG            0
#define PLUGIN_EVENT_FULLSCAN_START 1
#define PLUGIN_EVENT_FULLSCAN_END   2
#define PLUGIN_EVENT_STARTING       3
#define PLUGIN_EVENT_SHUTDOWN       4
#define PLUGIN_EVENT_STARTSTREAM    5
#define PLUGIN_EVENT_ABORTSTREAM    6
#define PLUGIN_EVENT_ENDSTREAM      7

#define PLUGIN_VERSION   2


#endif /* _FF_PLUGIN_EVENTS_ */
