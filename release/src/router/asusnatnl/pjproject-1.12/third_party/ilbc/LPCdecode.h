
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       LPC_decode.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_LPC_DECODE_H
   #define __iLBC_LPC_DECODE_H

   void LSFinterpolate2a_dec(
       float *a,           /* (o) lpc coefficients for a sub-frame */
       float *lsf1,    /* (i) first lsf coefficient vector */
       float *lsf2,    /* (i) second lsf coefficient vector */
       float coef,         /* (i) interpolation weight */
       int length          /* (i) length of lsf vectors */
   );

   void SimplelsfDEQ(
       float *lsfdeq,      /* (o) dequantized lsf coefficients */
       int *index,         /* (i) quantization index */
       int lpc_n           /* (i) number of LPCs */
   );

   void DecoderInterpolateLSF(
       float *syntdenum,   /* (o) synthesis filter coefficients */
       float *weightdenum, /* (o) weighting denumerator
                                  coefficients */
       float *lsfdeq,      /* (i) dequantized lsf coefficients */
       int length,         /* (i) length of lsf coefficient vector */
       iLBC_Dec_Inst_t *iLBCdec_inst
                           /* (i) the decoder state structure */
   );

   #endif












