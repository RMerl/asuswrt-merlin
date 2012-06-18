/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006-2008, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __UPNPDESCGEN_H__
#define __UPNPDESCGEN_H__

#include "config.h"

/* for the root description 
 * The child list reference is stored in "data" member using the
 * INITHELPER macro with index/nchild always in the
 * same order, whatever the endianness */
struct XMLElt {
	const char * eltname;	/* begin with '/' if no child */
	const char * data;	/* Value */
};

/* for service description */
struct serviceDesc {
	const struct action * actionList;
	const struct stateVar * serviceStateTable;
};

struct action {
	const char * name;
	const struct argument * args;
};

struct argument {
	const char * name;		/* the name of the argument */
	unsigned char dir;		/* 1 = in, 2 = out */
	unsigned char relatedVar;	/* index of the related variable */
};

struct stateVar {
	const char * name;
	unsigned char itype;	/* MSB: sendEvent flag, 7 LSB: index in upnptypes */
	unsigned char idefault;	/* default value */
	unsigned char iallowedlist;	/* index in allowed values list */
	unsigned char ieventvalue;	/* fixed value returned or magical values */
};

/* little endian 
 * The code has now be tested on big endian architecture */
#define INITHELPER(i, n) ((char *)((n<<16)|i))

/* char * genRootDesc(int *);
 * returns: NULL on error, string allocated on the heap */
char *
genRootDesc(int * len);

char *
genRootDescSamsung(int * len);

/* for the two following functions */
char *
genContentDirectory(int * len);

char *
genConnectionManager(int * len);

char *
genX_MS_MediaReceiverRegistrar(int * len);

char *
getVarsContentDirectory(int * len);

char *
getVarsConnectionManager(int * len);

char *
getVarsX_MS_MediaReceiverRegistrar(int * len);

#endif

