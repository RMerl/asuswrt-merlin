/*
 * Sample runtime overlay manager.
 */

#ifdef NO_PROTOTYPES
#define PARAMS(paramlist) ()
#else
#define PARAMS(paramlist) paramlist
#endif

typedef enum { FALSE, TRUE } bool;

/* Entry Points: */

bool OverlayLoad   PARAMS((unsigned long ovlyno));
bool OverlayUnload PARAMS((unsigned long ovlyno));

