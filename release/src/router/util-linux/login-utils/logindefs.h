#ifndef UTIL_LINUX_LOGINDEFS_H
#define UTIL_LINUX_LOGINDEFS_H

extern int getlogindefs_bool(const char *name, int dflt);
extern long getlogindefs_num(const char *name, long dflt);
extern const char *getlogindefs_str(const char *name, const char *dflt);
extern void free_getlogindefs_data(void);

#endif /* UTIL_LINUX_LOGINDEFS_H */
