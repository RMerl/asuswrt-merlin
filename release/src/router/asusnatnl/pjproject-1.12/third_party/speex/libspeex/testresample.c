/* Copyright (C) 2007 Jean-Marc Valin
      
   File: testresample.c
   Testing the resampling code

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "speex/speex_resampler.h"
#include <math.h>
#include <stdlib.h>

#define NN 256

int main()
{
   spx_uint32_t i;
   short *in;
   short *out;
   float *fin, *fout;
   int count = 0;
   SpeexResamplerState *st = speex_resampler_init(1, 8000, 12000, 10, NULL);
   speex_resampler_set_rate(st, 96000, 44100);
   speex_resampler_skip_zeros(st);
   
   in = malloc(NN*sizeof(short));
   out = malloc(2*NN*sizeof(short));
   fin = malloc(NN*sizeof(float));
   fout = malloc(2*NN*sizeof(float));
   while (1)
   {
      spx_uint32_t in_len;
      spx_uint32_t out_len;
      fread(in, sizeof(short), NN, stdin);
      if (feof(stdin))
         break;
      for (i=0;i<NN;i++)
         fin[i]=in[i];
      in_len = NN;
      out_len = 2*NN;
      /*if (count==2)
         speex_resampler_set_quality(st, 10);*/
      speex_resampler_process_float(st, 0, fin, &in_len, fout, &out_len);
      for (i=0;i<out_len;i++)
         out[i]=floor(.5+fout[i]);
      /*speex_warning_int("writing", out_len);*/
      fwrite(out, sizeof(short), out_len, stdout);
      count++;
   }
   speex_resampler_destroy(st);
   free(in);
   free(out);
   free(fin);
   free(fout);
   return 0;
}

