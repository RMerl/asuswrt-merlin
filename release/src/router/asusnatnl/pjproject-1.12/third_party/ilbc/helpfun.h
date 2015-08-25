
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       helpfun.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_HELPFUN_H
   #define __iLBC_HELPFUN_H

   void autocorr(
       float *r,       /* (o) autocorrelation vector */
       const float *x, /* (i) data vector */
       int N,          /* (i) length of data vector */
       int order       /* largest lag for calculated
                          autocorrelations */
   );

   void window(
       float *z,       /* (o) the windowed data */
       const float *x, /* (i) the original data vector */
       const float *y, /* (i) the window */
       int N           /* (i) length of all vectors */
   );

   void levdurb(
       float *a,       /* (o) lpc coefficient vector starting
                              with 1.0 */
       float *k,       /* (o) reflection coefficients */
       float *r,       /* (i) autocorrelation vector */
       int order       /* (i) order of lpc filter */
   );

   void interpolate(





       float *out,     /* (o) the interpolated vector */
       float *in1,     /* (i) the first vector for the
                              interpolation */
       float *in2,     /* (i) the second vector for the
                              interpolation */
       float coef,     /* (i) interpolation weights */
       int length      /* (i) length of all vectors */
   );

   void bwexpand(
       float *out,     /* (o) the bandwidth expanded lpc
                              coefficients */
       float *in,      /* (i) the lpc coefficients before bandwidth
                              expansion */
       float coef,     /* (i) the bandwidth expansion factor */
       int length      /* (i) the length of lpc coefficient vectors */
   );

   void vq(
       float *Xq,      /* (o) the quantized vector */
       int *index,     /* (o) the quantization index */
       const float *CB,/* (i) the vector quantization codebook */
       float *X,       /* (i) the vector to quantize */
       int n_cb,       /* (i) the number of vectors in the codebook */
       int dim         /* (i) the dimension of all vectors */
   );

   void SplitVQ(
       float *qX,      /* (o) the quantized vector */
       int *index,     /* (o) a vector of indexes for all vector
                              codebooks in the split */
       float *X,       /* (i) the vector to quantize */
       const float *CB,/* (i) the quantizer codebook */
       int nsplit,     /* the number of vector splits */
       const int *dim, /* the dimension of X and qX */
       const int *cbsize /* the number of vectors in the codebook */
   );


   void sort_sq(
       float *xq,      /* (o) the quantized value */
       int *index,     /* (o) the quantization index */
       float x,    /* (i) the value to quantize */
       const float *cb,/* (i) the quantization codebook */
       int cb_size     /* (i) the size of the quantization codebook */
   );

   int LSF_check(      /* (o) 1 for stable lsf vectors and 0 for





                              nonstable ones */
       float *lsf,     /* (i) a table of lsf vectors */
       int dim,    /* (i) the dimension of each lsf vector */
       int NoAn    /* (i) the number of lsf vectors in the
                              table */
   );

   #endif

