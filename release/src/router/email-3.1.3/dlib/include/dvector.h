#ifndef __DVECTOR_H
#define __DVECTOR_H

#include <stdlib.h>

typedef void **dvector;

typedef void (*dvDestructor)(void *);

dvector dvCreate(size_t size, dvDestructor destr);
void dvAddItem(dvector *vec, void *item);
void dvDestroy(dvector vec);
size_t dvSize(dvector vec);
size_t dvLength(dvector vec);

#endif

