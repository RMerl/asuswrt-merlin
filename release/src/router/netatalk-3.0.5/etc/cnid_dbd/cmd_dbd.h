#ifndef CMD_DBD_H
#define CMD_DBD_H

#include <signal.h>
#include <limits.h>

#include <atalk/netatalk_conf.h>
#include "dbif.h"

enum logtype {LOGSTD, LOGDEBUG};
typedef unsigned int dbd_flags_t;

#define DBD_FLAGS_SCAN     (1 << 0)
#define DBD_FLAGS_FORCE    (1 << 1)
#define DBD_FLAGS_STATS    (1 << 2)
#define DBD_FLAGS_V2TOEA   (1 << 3) /* Convert adouble:v2 to adouble:ea */
#define DBD_FLAGS_VERBOSE  (1 << 4)

#define ADv2_DIRNAME ".AppleDouble"

#define DIR_DOT_OR_DOTDOT(a) \
        ((strcmp(a, ".") == 0) || (strcmp(a, "..") == 0))

extern volatile sig_atomic_t alarmed;

extern void dbd_log(enum logtype lt, char *fmt, ...);
extern int cmd_dbd_scanvol(struct vol *vol, dbd_flags_t flags);

#endif /* CMD_DBD_H */
