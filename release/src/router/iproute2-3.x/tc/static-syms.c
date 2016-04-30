/*
 * This file creates a dummy version of dynamic loading
 * for environments where dynamic linking
 * is not used or available.
 */

#include <string.h>
#include "dlfcn.h"

void *_dlsym(const char *sym)
{
#include "static-syms.h"
	return NULL;
}
