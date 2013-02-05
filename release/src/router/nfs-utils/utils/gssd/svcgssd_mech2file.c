/*
  linux_downcall.c

  Copyright (c) 2000 The Regents of the University of Michigan.
  All rights reserved.

  Copyright (c) 2004 Andy Adamson <andros@UMICH.EDU>.
  All rights reserved, all wrongs reversed.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#include <gssapi/gssapi.h>
#include <string.h>


#define g_OID_equal(o1,o2) \
   (((o1)->length == (o2)->length) && \
    (memcmp((o1)->elements,(o2)->elements,(int) (o1)->length) == 0))

struct mech2file {
	gss_OID_desc mech;
	char         filename[8];
};

struct mech2file m2f[] = {
	{{9, "\052\206\110\206\367\022\001\002\002"}, "krb5"},
	{{7, "\053\006\001\005\005\001\003"}, "spkm3"},
	{{7, "\053\006\001\005\005\001\009"}, "lipkey"},
	{{0,0},""},
};

/*
 * Find the Linux svcgssd downcall file name given the mechanism
 */
char *
mech2file(gss_OID mech)
{
	struct mech2file *m2fp = m2f;

	while(m2fp->mech.length != 0) {
		if (g_OID_equal(mech,&m2fp->mech))
			return(m2fp->filename);
		m2fp++;
	}
	return NULL;
}
