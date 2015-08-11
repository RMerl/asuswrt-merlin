
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iCBSearch.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_ICBSEARCH_H
   #define __iLBC_ICBSEARCH_H






   void iCBSearch(
       iLBC_Enc_Inst_t *iLBCenc_inst,
                           /* (i) the encoder state structure */
       int *index,         /* (o) Codebook indices */
       int *gain_index,/* (o) Gain quantization indices */
       float *intarget,/* (i) Target vector for encoding */
       float *mem,         /* (i) Buffer for codebook construction */
       int lMem,           /* (i) Length of buffer */
       int lTarget,    /* (i) Length of vector */
       int nStages,    /* (i) Number of codebook stages */
       float *weightDenum, /* (i) weighting filter coefficients */
       float *weightState, /* (i) weighting filter state */
       int block           /* (i) the sub-block number */
   );

   #endif

