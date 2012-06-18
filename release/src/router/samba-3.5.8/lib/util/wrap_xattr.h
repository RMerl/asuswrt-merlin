#ifndef __LIB_UTIL_WRAP_XATTR_H__
#define __LIB_UTIL_WRAP_XATTR_H__

ssize_t wrap_fgetxattr(int fd, const char *name, void *value, size_t size);
ssize_t wrap_getxattr(const char *path, const char *name, void *value, size_t size);
int wrap_fsetxattr(int fd, const char *name, void *value, size_t size, int flags);
int wrap_setxattr(const char *path, const char *name, void *value, size_t size, int flags);
int wrap_fremovexattr(int fd, const char *name);
int wrap_removexattr(const char *path, const char *name);

#endif /* __LIB_UTIL_WRAP_XATTR_H__ */

