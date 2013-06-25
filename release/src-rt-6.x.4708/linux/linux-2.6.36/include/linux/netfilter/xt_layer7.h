#ifndef _XT_LAYER7_H
#define _XT_LAYER7_H

#define MAX_PATTERN_LEN 8192
#define MAX_PROTOCOL_LEN 256

struct xt_layer7_info {
    char protocol[MAX_PROTOCOL_LEN];
    char invert:1;
    char pattern[MAX_PATTERN_LEN];
};

#endif /* _XT_LAYER7_H */
