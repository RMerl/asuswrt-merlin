/*
 * $Id: $
 */

#ifndef _OUT_DAAP_PROTO_H_
#define _OUT_DAAP_PROTO_H_

#include "out-daap.h"

typedef struct tag_daap_items {
    int type;
    char *tag;
    char *description;
} DAAP_ITEMS;

extern DAAP_ITEMS taglist[];

/* metatag parsing */
extern MetaField_t daap_encode_meta(char *meta);
extern int daap_wantsmeta(MetaField_t meta, MetaFieldName_t fieldNo);

/* dmap helper functions */
extern int dmap_add_char(unsigned char *where, char *tag, char value);
extern int dmap_add_short(unsigned char *where, char *tag, short value);
extern int dmap_add_int(unsigned char *where, char *tag, int value);
extern int dmap_add_long(unsigned char *where, char *tag, uint64_t value);
extern int dmap_add_string(unsigned char *where, char *tag, char *value);
extern int dmap_add_literal(unsigned char *where, char *tag, char *value, int size);
extern int dmap_add_container(unsigned char *where, char *tag, int size);

extern int daap_get_next_session(void);


extern int daap_enum_size(char **pe, PRIVINFO *pinfo, int *count, int *total_size);
extern int daap_enum_fetch(char **pe, PRIVINFO *pinfo, int *size, unsigned char **pdmap);

#endif /* _OUT_DAAP_PROTO_H_ */
