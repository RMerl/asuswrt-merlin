/*
 * Stub dlfcn implementation for systems that lack shared library support
 * but obviously can still reference compiled-in symbols.
 */

#ifndef NO_SHARED_LIBS
#include_next <dlfcn.h>
#else

#define RTLD_LAZY 0
#define _FAKE_DLFCN_HDL (void *)0xbeefcafe

static inline void *dlopen(const char *file, int flag)
{
	if (file == NULL)
		return _FAKE_DLFCN_HDL;
	else
		return NULL;
}

extern void *_dlsym(const char *sym);
static inline void *dlsym(void *handle, const char *sym)
{
	if (handle != _FAKE_DLFCN_HDL)
		return NULL;
	return _dlsym(sym);
}

static inline char *dlerror(void)
{
	return NULL;
}

static inline int dlclose(void *handle)
{
	return (handle == _FAKE_DLFCN_HDL) ? 0 : 1;
}

#endif
