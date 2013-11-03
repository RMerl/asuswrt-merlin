/*
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <atalk/util.h>

#ifndef HAVE_DLFCN_H
#ifdef MACOSX_SERVER
#include <mach-o/dyld.h>

void *mod_open(const char *path)
{
  NSObjectFileImage file;

  if (NSCreateObjectFileImageFromFile(path, &file) != 
      NSObjectFileImageSuccess)
    return NULL;
  return NSLinkModule(file, path, TRUE);
}

void *mod_symbol(void *module, const char *name)
{
   NSSymbol symbol;
   char *underscore;

   if ((underscore = (char *) malloc(strlen(name) + 2)) == NULL)
     return NULL;
   strcpy(underscore, "_");
   strcat(underscore, name);
   symbol = NSLookupAndBindSymbol(underscore);
   free(underscore);

   return NSAddressOfSymbol(symbol);
}

void mod_close(void *module)
{
  NSUnLinkModule(module, FALSE);
}
#endif /* MACOSX_SERVER */

#else /* HAVE_DLFCN_H */

#include <dlfcn.h>

#ifdef DLSYM_PREPEND_UNDERSCORE
void *mod_symbol(void *module, const char *name)
{
   void *symbol;
   char *underscore;

   if (!module)
     return NULL;

   if ((underscore = (char *) malloc(strlen(name) + 2)) == NULL)
     return NULL;

   strcpy(underscore, "_");
   strcat(underscore, name);
   symbol = dlsym(module, underscore);
   free(underscore);

   return symbol;
}
#endif /* DLSYM_PREPEND_UNDERSCORE */
#endif /* HAVE_DLFCN_H */
