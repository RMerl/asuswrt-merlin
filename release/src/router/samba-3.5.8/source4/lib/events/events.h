#ifndef __LIB_EVENTS_H__
#define __LIB_EVENTS_H__
#define TEVENT_COMPAT_DEFINES 1
#include <../lib/tevent/tevent.h>
struct tevent_context *s4_event_context_init(TALLOC_CTX *mem_ctx);
struct tevent_context *event_context_find(TALLOC_CTX *mem_ctx) _DEPRECATED_;
#endif /* __LIB_EVENTS_H__ */
