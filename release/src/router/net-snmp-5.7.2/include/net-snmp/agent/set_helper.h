#ifndef SET_HELPER_H
#define SET_HELPER_H

#ifdef __cplusplus
extern          "C" {
#endif

typedef struct netsnmp_set_info_s {
    int             action;
    void           *stateRef;

    /*
     * don't use yet: 
     */
    void          **oldData;
    int             setCleanupFlags;    /* XXX: client sets this to: */
#define AUTO_FREE_STATEREF 0x01 /* calls free(stateRef) */
#define AUTO_FREE_OLDDATA  0x02 /* calls free(*oldData) */
#define AUTO_UNDO          0x04 /* ... */
} netsnmp_set_info;

#ifdef __cplusplus
}
#endif
#endif
