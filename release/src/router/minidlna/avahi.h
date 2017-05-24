#include "config.h"

#if defined(TIVO_SUPPORT) && defined(HAVE_AVAHI)
void tivo_bonjour_register(void);
void tivo_bonjour_unregister(void);
#else
static inline void tivo_bonjour_register(void) {};
static inline void tivo_bonjour_unregister(void) {};
#endif
