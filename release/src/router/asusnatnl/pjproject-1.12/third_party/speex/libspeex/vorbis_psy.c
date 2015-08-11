/* Copyright (C) 2005 Jean-Marc Valin, CSIRO, Christopher Montgomery
   File: vorbis_psy.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef VORBIS_PSYCHO

#include "arch.h"
#include "smallft.h"
#include "lpc.h"
#include "vorbis_psy.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* psychoacoustic setup ********************************************/

static VorbisPsyInfo example_tuning = {
  
  .5,.5,  
  3,3,25,
  
  /*63     125     250     500      1k      2k      4k      8k     16k*/
  // vorbis mode 4 style
  //{-32,-32,-32,-32,-28,-24,-22,-20,-20, -20, -20, -8, -6, -6, -6, -6, -6},
  { -4, -6, -6, -6, -6, -6, -6, -6, -8, -8,-10,-10, -8, -6, -4, -4, -2},
  
  {
    0, 1, 2, 3, 4, 5, 5,  5,     /* 7dB */
    6, 6, 6, 5, 4, 4, 4,  4,     /* 15dB */
    4, 4, 5, 5, 5, 6, 6,  6,     /* 23dB */
    7, 7, 7, 8, 8, 8, 9, 10,     /* 31dB */
    11,12,13,14,15,16,17, 18,     /* 39dB */
  }

};



/* there was no great place to put this.... */
#include <stdio.h>
static void _analysis_output(char *base,int i,float *v,int n,int bark,int dB){
  int j;
  FILE *of;
  char buffer[80];

  sprintf(buffer,"%s_%d.m",base,i);
  of=fopen(buffer,"w");
  
  if(!of)perror("failed to open data dump file");
  
  for(j=0;j<n;j++){
    if(bark){
      float b=toBARK((4000.f*j/n)+.25);
      fprintf(of,"%f ",b);
    }else
      fprintf(of,"%f ",(double)j);
    
    if(dB){
      float val;
      if(v[j]==0.)
	val=-140.;
      else
	val=todB(v[j]);
      fprintf(of,"%f\n",val);
    }else{
      fprintf(of,"%f\n",v[j]);
    }
  }
  fclose(of);
}

static void bark_noise_hybridmp(int n,const long *b,
                                const float *f,
                                float *noise,
                                const float offset,
                                const int fixed){
  
  float *N=alloca(n*sizeof(*N));
  float *X=alloca(n*sizeof(*N));
  float *XX=alloca(n*sizeof(*N));
  float *Y=alloca(n*sizeof(*N));
  float *XY=alloca(n*sizeof(*N));

  float tN, tX, tXX, tY, tXY;
  int i;

  int lo, hi;
  float R, A, B, D;
  float w, x, y;

  tN = tX = tXX = tY = tXY = 0.f;

  y = f[0] + offset;
  if (y < 1.f) y = 1.f;

  w = y * y * .5;
    
  tN += w;
  tX += w;
  tY += w * y;

  N[0] = tN;
  X[0] = tX;
  XX[0] = tXX;
  Y[0] = tY;
  XY[0] = tXY;

  for (i = 1, x = 1.f; i < n; i++, x += 1.f) {
    
    y = f[i] + offset;
    if (y < 1.f) y = 1.f;

    w = y * y;
    
    tN += w;
    tX += w * x;
    tXX += w * x * x;
    tY += w * y;
    tXY += w * x * y;

    N[i] = tN;
    X[i] = tX;
    XX[i] = tXX;
    Y[i] = tY;
    XY[i] = tXY;
  }
  
  for (i = 0, x = 0.f;; i++, x += 1.f) {
    
    lo = b[i] >> 16;
    if( lo>=0 ) break;
    hi = b[i] & 0xffff;
    
    tN = N[hi] + N[-lo];
    tX = X[hi] - X[-lo];
    tXX = XX[hi] + XX[-lo];
    tY = Y[hi] + Y[-lo];    
    tXY = XY[hi] - XY[-lo];
    
    A = tY * tXX - tX * tXY;
    B = tN * tXY - tX * tY;
    D = tN * tXX - tX * tX;
    R = (A + x * B) / D;
    if (R < 0.f)
      R = 0.f;
    
    noise[i] = R - offset;
  }
  
  for ( ;; i++, x += 1.f) {
    
    lo = b[i] >> 16;
    hi = b[i] & 0xffff;
    if(hi>=n)break;
    
    tN = N[hi] - N[lo];
    tX = X[hi] - X[lo];
    tXX = XX[hi] - XX[lo];
    tY = Y[hi] - Y[lo];
    tXY = XY[hi] - XY[lo];
    
    A = tY * tXX - tX * tXY;
    B = tN * tXY - tX * tY;
    D = tN * tXX - tX * tX;
    R = (A + x * B) / D;
    if (R < 0.f) R = 0.f;
    
    noise[i] = R - offset;
  }
  for ( ; i < n; i++, x += 1.f) {
    
    R = (A + x * B) / D;
    if (R < 0.f) R = 0.f;
    
    noise[i] = R - offset;
  }
  
  if (fixed <= 0) return;
  
  for (i = 0, x = 0.f;; i++, x += 1.f) {
    hi = i + fixed / 2;
    lo = hi - fixed;
    if(lo>=0)break;

    tN = N[hi] + N[-lo];
    tX = X[hi] - X[-lo];
    tXX = XX[hi] + XX[-lo];
    tY = Y[hi] + Y[-lo];
    tXY = XY[hi] - XY[-lo];
    
    
    A = tY * tXX - tX * tXY;
    B = tN * tXY - tX * tY;
    D = tN * tXX - tX * tX;
    R = (A + x * B) / D;

    if (R - offset < noise[i]) noise[i] = R - offset;
  }
  for ( ;; i++, x += 1.f) {
    
    hi = i + fixed / 2;
    lo = hi - fixed;
    if(hi>=n)break;
    
    tN = N[hi] - N[lo];
    tX = X[hi] - X[lo];
    tXX = XX[hi] - XX[lo];
    tY = Y[hi] - Y[lo];
    tXY = XY[hi] - XY[lo];
    
    A = tY * tXX - tX * tXY;
    B = tN * tXY - tX * tY;
    D = tN * tXX - tX * tX;
    R = (A + x * B) / D;
    
    if (R - offset < noise[i]) noise[i] = R - offset;
  }
  for ( ; i < n; i++, x += 1.f) {
    R = (A + x * B) / D;
    if (R - offset < noise[i]) noise[i] = R - offset;
  }
}

static void _vp_noisemask(VorbisPsy *p,
			  float *logfreq, 
			  float *logmask){

  int i,n=p->n/2;
  float *work=alloca(n*sizeof(*work));

  bark_noise_hybridmp(n,p->bark,logfreq,logmask,
		      140.,-1);

  for(i=0;i<n;i++)work[i]=logfreq[i]-logmask[i];

  bark_noise_hybridmp(n,p->bark,work,logmask,0.,
		      p->vi->noisewindowfixed);

  for(i=0;i<n;i++)work[i]=logfreq[i]-work[i];
  
  {
    static int seq=0;
    
    float work2[n];
    for(i=0;i<n;i++){
      work2[i]=logmask[i]+work[i];
    }
    
    //_analysis_output("logfreq",seq,logfreq,n,0,0);
    //_analysis_output("median",seq,work,n,0,0);
    //_analysis_output("envelope",seq,work2,n,0,0);
    seq++;
  }

  for(i=0;i<n;i++){
    int dB=logmask[i]+.5;
    if(dB>=NOISE_COMPAND_LEVELS)dB=NOISE_COMPAND_LEVELS-1;
    if(dB<0)dB=0;
    logmask[i]= work[i]+p->vi->noisecompand[dB]+p->noiseoffset[i];
  }

}

VorbisPsy *vorbis_psy_init(int rate, int n)
{
  long i,j,lo=-99,hi=1;
  VorbisPsy *p = speex_alloc(sizeof(VorbisPsy));
  memset(p,0,sizeof(*p));
  
  p->n = n;
  spx_drft_init(&p->lookup, n);
  p->bark = speex_alloc(n*sizeof(*p->bark));
  p->rate=rate;
  p->vi = &example_tuning;

  /* BH4 window */
  p->window = speex_alloc(sizeof(*p->window)*n);
  float a0 = .35875f;
  float a1 = .48829f;
  float a2 = .14128f;
  float a3 = .01168f;
  for(i=0;i<n;i++)
    p->window[i] = //a0 - a1*cos(2.*M_PI/n*(i+.5)) + a2*cos(4.*M_PI/n*(i+.5)) - a3*cos(6.*M_PI/n*(i+.5));
      sin((i+.5)/n * M_PI)*sin((i+.5)/n * M_PI);
  /* bark scale lookups */
  for(i=0;i<n;i++){
    float bark=toBARK(rate/(2*n)*i); 
    
    for(;lo+p->vi->noisewindowlomin<i && 
	  toBARK(rate/(2*n)*lo)<(bark-p->vi->noisewindowlo);lo++);
    
    for(;hi<=n && (hi<i+p->vi->noisewindowhimin ||
	  toBARK(rate/(2*n)*hi)<(bark+p->vi->noisewindowhi));hi++);
    
    p->bark[i]=((lo-1)<<16)+(hi-1);

  }

  /* set up rolling noise median */
  p->noiseoffset=speex_alloc(n*sizeof(*p->noiseoffset));
  
  for(i=0;i<n;i++){
    float halfoc=toOC((i+.5)*rate/(2.*n))*2.;
    int inthalfoc;
    float del;
    
    if(halfoc<0)halfoc=0;
    if(halfoc>=P_BANDS-1)halfoc=P_BANDS-1;
    inthalfoc=(int)halfoc;
    del=halfoc-inthalfoc;
    
    p->noiseoffset[i]=
      p->vi->noiseoff[inthalfoc]*(1.-del) + 
      p->vi->noiseoff[inthalfoc+1]*del;
    
  }
#if 0
  _analysis_output_always("noiseoff0",ls,p->noiseoffset,n,1,0,0);
#endif

   return p;
}

void vorbis_psy_destroy(VorbisPsy *p)
{
  if(p){
    spx_drft_clear(&p->lookup);
    if(p->bark)
      speex_free(p->bark);
    if(p->noiseoffset)
      speex_free(p->noiseoffset);
    if(p->window)
      speex_free(p->window);
    memset(p,0,sizeof(*p));
    speex_free(p);
  }
}

void compute_curve(VorbisPsy *psy, float *audio, float *curve)
{
   int i;
   float work[psy->n];

   float scale=4.f/psy->n;
   float scale_dB;

   scale_dB=todB(scale);
  
   /* window the PCM data; use a BH4 window, not vorbis */
   for(i=0;i<psy->n;i++)
     work[i]=audio[i] * psy->window[i];

   {
     static int seq=0;
     
     //_analysis_output("win",seq,work,psy->n,0,0);

     seq++;
   }

    /* FFT yields more accurate tonal estimation (not phase sensitive) */
    spx_drft_forward(&psy->lookup,work);

    /* magnitudes */
    work[0]=scale_dB+todB(work[0]);
    for(i=1;i<psy->n-1;i+=2){
      float temp = work[i]*work[i] + work[i+1]*work[i+1];
      work[(i+1)>>1] = scale_dB+.5f * todB(temp);
    }

    /* derive a noise curve */
    _vp_noisemask(psy,work,curve);
#define SIDEL 12
    for (i=0;i<SIDEL;i++)
    {
       curve[i]=curve[SIDEL];
    }
#define SIDEH 12
    for (i=0;i<SIDEH;i++)
    {
       curve[(psy->n>>1)-i-1]=curve[(psy->n>>1)-SIDEH];
    }
    for(i=0;i<((psy->n)>>1);i++)
       curve[i] = fromdB(1.2*curve[i]+.2*i);
       //curve[i] = fromdB(0.8*curve[i]+.35*i);
       //curve[i] = fromdB(0.9*curve[i])*pow(1.0*i+45,1.3);
}

/* Transform a masking curve (power spectrum) into a pole-zero filter */
void curve_to_lpc(VorbisPsy *psy, float *curve, float *awk1, float *awk2, int ord)
{
   int i;
   float ac[psy->n];
   float tmp;
   int len = psy->n >> 1;
   for (i=0;i<2*len;i++)
      ac[i] = 0;
   for (i=1;i<len;i++)
      ac[2*i-1] = curve[i];
   ac[0] = curve[0];
   ac[2*len-1] = curve[len-1];
   
   spx_drft_backward(&psy->lookup, ac);
   _spx_lpc(awk1, ac, ord);
   tmp = 1.;
   for (i=0;i<ord;i++)
   {
      tmp *= .99;
      awk1[i] *= tmp;
   }
#if 0
   for (i=0;i<ord;i++)
      awk2[i] = 0;
#else
   /* Use the second (awk2) filter to correct the first one */
   for (i=0;i<2*len;i++)
      ac[i] = 0;   
   for (i=0;i<ord;i++)
      ac[i+1] = awk1[i];
   ac[0] = 1;
   spx_drft_forward(&psy->lookup, ac);
   /* Compute (power) response of awk1 (all zero) */
   ac[0] *= ac[0];
   for (i=1;i<len;i++)
      ac[i] = ac[2*i-1]*ac[2*i-1] + ac[2*i]*ac[2*i];
   ac[len] = ac[2*len-1]*ac[2*len-1];
   /* Compute correction required */
   for (i=0;i<len;i++)
      curve[i] = 1. / (1e-6f+curve[i]*ac[i]);

   for (i=0;i<2*len;i++)
      ac[i] = 0;
   for (i=1;i<len;i++)
      ac[2*i-1] = curve[i];
   ac[0] = curve[0];
   ac[2*len-1] = curve[len-1];
   
   spx_drft_backward(&psy->lookup, ac);
   _spx_lpc(awk2, ac, ord);
   tmp = 1;
   for (i=0;i<ord;i++)
   {
      tmp *= .99;
      awk2[i] *= tmp;
   }
#endif
}

#if 0
#include <stdio.h>
#include <math.h>

#define ORDER 10
#define CURVE_SIZE 24

int main()
{
   int i;
   float curve[CURVE_SIZE];
   float awk1[ORDER], awk2[ORDER];
   for (i=0;i<CURVE_SIZE;i++)
      scanf("%f ", &curve[i]);
   for (i=0;i<CURVE_SIZE;i++)
      curve[i] = pow(10.f, .1*curve[i]);
   curve_to_lpc(curve, CURVE_SIZE, awk1, awk2, ORDER);
   for (i=0;i<ORDER;i++)
      printf("%f ", awk1[i]);
   printf ("\n");
   for (i=0;i<ORDER;i++)
      printf("%f ", awk2[i]);
   printf ("\n");
   return 0;
}
#endif

#endif
