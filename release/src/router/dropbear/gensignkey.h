#ifndef _GENSIGNKEY_H
#define _GENSIGNKEY_H

#include "signkey.h"

int signkey_generate(enum signkey_type type, int bits, const char* filename);

#endif
