
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       helpfun.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>

   #include "iLBC_define.h"
   #include "constants.h"

   /*----------------------------------------------------------------*
    *  calculation of auto correlation
    *---------------------------------------------------------------*/

   void autocorr(
       float *r,       /* (o) autocorrelation vector */
       const float *x, /* (i) data vector */
       int N,          /* (i) length of data vector */
       int order       /* largest lag for calculated
                          autocorrelations */
   ){
       int     lag, n;
       float   sum;

       for (lag = 0; lag <= order; lag++) {
           sum = 0;
           for (n = 0; n < N - lag; n++) {
               sum += x[n] * x[n+lag];
           }
           r[lag] = sum;
       }





   }

   /*----------------------------------------------------------------*
    *  window multiplication
    *---------------------------------------------------------------*/

   void window(
       float *z,       /* (o) the windowed data */
       const float *x, /* (i) the original data vector */
       const float *y, /* (i) the window */
       int N           /* (i) length of all vectors */
   ){
       int     i;

       for (i = 0; i < N; i++) {
           z[i] = x[i] * y[i];
       }
   }

   /*----------------------------------------------------------------*
    *  levinson-durbin solution for lpc coefficients
    *---------------------------------------------------------------*/

   void levdurb(
       float *a,       /* (o) lpc coefficient vector starting
                              with 1.0 */
       float *k,       /* (o) reflection coefficients */
       float *r,       /* (i) autocorrelation vector */
       int order       /* (i) order of lpc filter */
   ){
       float  sum, alpha;
       int     m, m_h, i;

       a[0] = 1.0;

       if (r[0] < EPS) { /* if r[0] <= 0, set LPC coeff. to zero */
           for (i = 0; i < order; i++) {
               k[i] = 0;
               a[i+1] = 0;
           }
       } else {
           a[1] = k[0] = -r[1]/r[0];
           alpha = r[0] + r[1] * k[0];
           for (m = 1; m < order; m++){
               sum = r[m + 1];
               for (i = 0; i < m; i++){
                   sum += a[i+1] * r[m - i];
               }





               k[m] = -sum / alpha;
               alpha += k[m] * sum;
               m_h = (m + 1) >> 1;
               for (i = 0; i < m_h; i++){
                   sum = a[i+1] + k[m] * a[m - i];
                   a[m - i] += k[m] * a[i+1];
                   a[i+1] = sum;
               }
               a[m+1] = k[m];
           }
       }
   }

   /*----------------------------------------------------------------*
    *  interpolation between vectors
    *---------------------------------------------------------------*/

   void interpolate(
       float *out,      /* (o) the interpolated vector */
       float *in1,     /* (i) the first vector for the
                              interpolation */
       float *in2,     /* (i) the second vector for the
                              interpolation */
       float coef,      /* (i) interpolation weights */
       int length      /* (i) length of all vectors */
   ){
       int i;
       float invcoef;

       invcoef = (float)1.0 - coef;
       for (i = 0; i < length; i++) {
           out[i] = coef * in1[i] + invcoef * in2[i];
       }
   }

   /*----------------------------------------------------------------*
    *  lpc bandwidth expansion
    *---------------------------------------------------------------*/

   void bwexpand(
       float *out,      /* (o) the bandwidth expanded lpc
                              coefficients */
       float *in,      /* (i) the lpc coefficients before bandwidth
                              expansion */
       float coef,     /* (i) the bandwidth expansion factor */
       int length      /* (i) the length of lpc coefficient vectors */
   ){
       int i;





       float  chirp;

       chirp = coef;

       out[0] = in[0];
       for (i = 1; i < length; i++) {
           out[i] = chirp * in[i];
           chirp *= coef;
       }
   }

   /*----------------------------------------------------------------*
    *  vector quantization
    *---------------------------------------------------------------*/

   void vq(
       float *Xq,      /* (o) the quantized vector */
       int *index,     /* (o) the quantization index */
       const float *CB,/* (i) the vector quantization codebook */
       float *X,       /* (i) the vector to quantize */
       int n_cb,       /* (i) the number of vectors in the codebook */
       int dim         /* (i) the dimension of all vectors */
   ){
       int     i, j;
       int     pos, minindex;
       float   dist, tmp, mindist;

       pos = 0;
       mindist = FLOAT_MAX;
       minindex = 0;
       for (j = 0; j < n_cb; j++) {
           dist = X[0] - CB[pos];
           dist *= dist;
           for (i = 1; i < dim; i++) {
               tmp = X[i] - CB[pos + i];
               dist += tmp*tmp;
           }

           if (dist < mindist) {
               mindist = dist;
               minindex = j;
           }
           pos += dim;
       }
       for (i = 0; i < dim; i++) {
           Xq[i] = CB[minindex*dim + i];
       }
       *index = minindex;





   }

   /*----------------------------------------------------------------*
    *  split vector quantization
    *---------------------------------------------------------------*/

   void SplitVQ(
       float *qX,      /* (o) the quantized vector */
       int *index,     /* (o) a vector of indexes for all vector
                              codebooks in the split */
       float *X,       /* (i) the vector to quantize */
       const float *CB,/* (i) the quantizer codebook */
       int nsplit,     /* the number of vector splits */
       const int *dim, /* the dimension of X and qX */
       const int *cbsize /* the number of vectors in the codebook */
   ){
       int    cb_pos, X_pos, i;

       cb_pos = 0;
       X_pos= 0;
       for (i = 0; i < nsplit; i++) {
           vq(qX + X_pos, index + i, CB + cb_pos, X + X_pos,
               cbsize[i], dim[i]);
           X_pos += dim[i];
           cb_pos += dim[i] * cbsize[i];
       }
   }

   /*----------------------------------------------------------------*
    *  scalar quantization
    *---------------------------------------------------------------*/

   void sort_sq(
       float *xq,      /* (o) the quantized value */
       int *index,     /* (o) the quantization index */
       float x,    /* (i) the value to quantize */
       const float *cb,/* (i) the quantization codebook */
       int cb_size      /* (i) the size of the quantization codebook */
   ){
       int i;

       if (x <= cb[0]) {
           *index = 0;
           *xq = cb[0];
       } else {
           i = 0;
           while ((x > cb[i]) && i < cb_size - 1) {
               i++;





           }

           if (x > ((cb[i] + cb[i - 1])/2)) {
               *index = i;
               *xq = cb[i];
           } else {
               *index = i - 1;
               *xq = cb[i - 1];
           }
       }
   }

   /*----------------------------------------------------------------*
    *  check for stability of lsf coefficients
    *---------------------------------------------------------------*/

   int LSF_check(    /* (o) 1 for stable lsf vectors and 0 for
                              nonstable ones */
       float *lsf,     /* (i) a table of lsf vectors */
       int dim,    /* (i) the dimension of each lsf vector */
       int NoAn    /* (i) the number of lsf vectors in the
                              table */
   ){
       int k,n,m, Nit=2, change=0,pos;
       float tmp;
       static float eps=(float)0.039; /* 50 Hz */
       static float eps2=(float)0.0195;
       static float maxlsf=(float)3.14; /* 4000 Hz */
       static float minlsf=(float)0.01; /* 0 Hz */

       /* LSF separation check*/

       for (n=0; n<Nit; n++) { /* Run through a couple of times */
           for (m=0; m<NoAn; m++) { /* Number of analyses per frame */
               for (k=0; k<(dim-1); k++) {
                   pos=m*dim+k;

                   if ((lsf[pos+1]-lsf[pos])<eps) {

                       if (lsf[pos+1]<lsf[pos]) {
                           tmp=lsf[pos+1];
                           lsf[pos+1]= lsf[pos]+eps2;
                           lsf[pos]= lsf[pos+1]-eps2;
                       } else {
                           lsf[pos]-=eps2;
                           lsf[pos+1]+=eps2;
                       }
                       change=1;





                   }

                   if (lsf[pos]<minlsf) {
                       lsf[pos]=minlsf;
                       change=1;
                   }

                   if (lsf[pos]>maxlsf) {
                       lsf[pos]=maxlsf;
                       change=1;
                   }
               }
           }
       }

       return change;
   }

