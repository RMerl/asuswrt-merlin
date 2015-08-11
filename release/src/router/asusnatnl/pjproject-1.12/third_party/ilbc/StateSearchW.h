
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       StateSearchW.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_STATESEARCHW_H
   #define __iLBC_STATESEARCHW_H

   void AbsQuantW(
       iLBC_Enc_Inst_t *iLBCenc_inst,
                           /* (i) Encoder instance */
       float *in,          /* (i) vector to encode */
       float *syntDenum,   /* (i) denominator of synthesis filter */
       float *weightDenum, /* (i) denominator of weighting filter */
       int *out,           /* (o) vector of quantizer indexes */
       int len,        /* (i) length of vector to encode and
                                  vector of quantizer indexes */
       int state_first     /* (i) position of start state in the
                                  80 vec */
   );

   void StateSearchW(
       iLBC_Enc_Inst_t *iLBCenc_inst,
                           /* (i) Encoder instance */
       float *residual,/* (i) target residual vector */
       float *syntDenum,   /* (i) lpc synthesis filter */
       float *weightDenum, /* (i) weighting filter denuminator */
       int *idxForMax,     /* (o) quantizer index for maximum
                                  amplitude */
       int *idxVec,    /* (o) vector of quantization indexes */
       int len,        /* (i) length of all vectors */
       int state_first     /* (i) position of start state in the
                                  80 vec */
   );


   #endif








