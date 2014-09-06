/*
 * auto_nlist.h
 */
#ifndef AUTO_NLIST_H
#define AUTO_NLIST_H
#ifdef __cplusplus
extern          "C" {
#endif

#if defined(irix6) && defined(IRIX64)
#define nlist nlist64
#endif

#ifdef NETSNMP_CAN_USE_NLIST
int             auto_nlist(const char *, char *, size_t);
long            auto_nlist_value(const char *);
int             KNLookup(struct nlist *, int, char *, size_t);
#else
int             auto_nlist_noop(void);
#	define auto_nlist(x,y,z) auto_nlist_noop()
#	define auto_nlist_value(z) auto_nlist_noop()
#	define KNLookup(w,x,y,z) auto_nlist_noop()
#endif

#ifdef __cplusplus
}
#endif
#endif
