
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       LPCencode.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_LPCENCOD_H
   #define __iLBC_LPCENCOD_H

   void LPCencode(
       float *syntdenum,   /* (i/o) synthesis filter coefficients
                                  before/after encoding */
       float *weightdenum, /* (i/o) weighting denumerator coefficients
                                  before/after encoding */
       int *lsf_index,     /* (o) lsf quantization index */
       float *data,    /* (i) lsf coefficients to quantize */
       iLBC_Enc_Inst_t *iLBCenc_inst
                           /* (i/o) the encoder state structure */
   );

   #endif

