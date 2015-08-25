
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       LPCencode.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <string.h>

   #include "iLBC_define.h"
   #include "helpfun.h"
   #include "lsf.h"
   #include "constants.h"





   /*----------------------------------------------------------------*
    *  lpc analysis (subrutine to LPCencode)
    *---------------------------------------------------------------*/

   void SimpleAnalysis(
       float *lsf,         /* (o) lsf coefficients */
       float *data,    /* (i) new data vector */
       iLBC_Enc_Inst_t *iLBCenc_inst
                           /* (i/o) the encoder state structure */
   ){
       int k, is;
       float temp[BLOCKL_MAX], lp[LPC_FILTERORDER + 1];
       float lp2[LPC_FILTERORDER + 1];
       float r[LPC_FILTERORDER + 1];

       is=LPC_LOOKBACK+BLOCKL_MAX-iLBCenc_inst->blockl;
       memcpy(iLBCenc_inst->lpc_buffer+is,data,
           iLBCenc_inst->blockl*sizeof(float));

       /* No lookahead, last window is asymmetric */

       for (k = 0; k < iLBCenc_inst->lpc_n; k++) {

           is = LPC_LOOKBACK;

           if (k < (iLBCenc_inst->lpc_n - 1)) {
               window(temp, lpc_winTbl,
                   iLBCenc_inst->lpc_buffer, BLOCKL_MAX);
           } else {
               window(temp, lpc_asymwinTbl,
                   iLBCenc_inst->lpc_buffer + is, BLOCKL_MAX);
           }

           autocorr(r, temp, BLOCKL_MAX, LPC_FILTERORDER);
           window(r, r, lpc_lagwinTbl, LPC_FILTERORDER + 1);

           levdurb(lp, temp, r, LPC_FILTERORDER);
           bwexpand(lp2, lp, LPC_CHIRP_SYNTDENUM, LPC_FILTERORDER+1);

           a2lsf(lsf + k*LPC_FILTERORDER, lp2);
       }
       is=LPC_LOOKBACK+BLOCKL_MAX-iLBCenc_inst->blockl;
       memmove(iLBCenc_inst->lpc_buffer,
           iLBCenc_inst->lpc_buffer+LPC_LOOKBACK+BLOCKL_MAX-is,
           is*sizeof(float));
   }

   /*----------------------------------------------------------------*





    *  lsf interpolator and conversion from lsf to a coefficients
    *  (subrutine to SimpleInterpolateLSF)
    *---------------------------------------------------------------*/

   void LSFinterpolate2a_enc(
       float *a,       /* (o) lpc coefficients */
       float *lsf1,/* (i) first set of lsf coefficients */
       float *lsf2,/* (i) second set of lsf coefficients */
       float coef,     /* (i) weighting coefficient to use between
                              lsf1 and lsf2 */
       long length      /* (i) length of coefficient vectors */
   ){
       float  lsftmp[LPC_FILTERORDER];

       interpolate(lsftmp, lsf1, lsf2, coef, length);
       lsf2a(a, lsftmp);
   }

   /*----------------------------------------------------------------*
    *  lsf interpolator (subrutine to LPCencode)
    *---------------------------------------------------------------*/

   void SimpleInterpolateLSF(
       float *syntdenum,   /* (o) the synthesis filter denominator
                                  resulting from the quantized
                                  interpolated lsf */
       float *weightdenum, /* (o) the weighting filter denominator
                                  resulting from the unquantized
                                  interpolated lsf */
       float *lsf,         /* (i) the unquantized lsf coefficients */
       float *lsfdeq,      /* (i) the dequantized lsf coefficients */
       float *lsfold,      /* (i) the unquantized lsf coefficients of
                                  the previous signal frame */
       float *lsfdeqold, /* (i) the dequantized lsf coefficients of
                                  the previous signal frame */
       int length,         /* (i) should equate LPC_FILTERORDER */
       iLBC_Enc_Inst_t *iLBCenc_inst
                           /* (i/o) the encoder state structure */
   ){
       int    i, pos, lp_length;
       float  lp[LPC_FILTERORDER + 1], *lsf2, *lsfdeq2;

       lsf2 = lsf + length;
       lsfdeq2 = lsfdeq + length;
       lp_length = length + 1;

       if (iLBCenc_inst->mode==30) {
           /* sub-frame 1: Interpolation between old and first





              set of lsf coefficients */

           LSFinterpolate2a_enc(lp, lsfdeqold, lsfdeq,
               lsf_weightTbl_30ms[0], length);
           memcpy(syntdenum,lp,lp_length*sizeof(float));
           LSFinterpolate2a_enc(lp, lsfold, lsf,
               lsf_weightTbl_30ms[0], length);
           bwexpand(weightdenum, lp, LPC_CHIRP_WEIGHTDENUM, lp_length);

           /* sub-frame 2 to 6: Interpolation between first
              and second set of lsf coefficients */

           pos = lp_length;
           for (i = 1; i < iLBCenc_inst->nsub; i++) {
               LSFinterpolate2a_enc(lp, lsfdeq, lsfdeq2,
                   lsf_weightTbl_30ms[i], length);
               memcpy(syntdenum + pos,lp,lp_length*sizeof(float));

               LSFinterpolate2a_enc(lp, lsf, lsf2,
                   lsf_weightTbl_30ms[i], length);
               bwexpand(weightdenum + pos, lp,
                   LPC_CHIRP_WEIGHTDENUM, lp_length);
               pos += lp_length;
           }
       }
       else {
           pos = 0;
           for (i = 0; i < iLBCenc_inst->nsub; i++) {
               LSFinterpolate2a_enc(lp, lsfdeqold, lsfdeq,
                   lsf_weightTbl_20ms[i], length);
               memcpy(syntdenum+pos,lp,lp_length*sizeof(float));
               LSFinterpolate2a_enc(lp, lsfold, lsf,
                   lsf_weightTbl_20ms[i], length);
               bwexpand(weightdenum+pos, lp,
                   LPC_CHIRP_WEIGHTDENUM, lp_length);
               pos += lp_length;
           }
       }

       /* update memory */

       if (iLBCenc_inst->mode==30) {
           memcpy(lsfold, lsf2, length*sizeof(float));
           memcpy(lsfdeqold, lsfdeq2, length*sizeof(float));
       }
       else {
           memcpy(lsfold, lsf, length*sizeof(float));
           memcpy(lsfdeqold, lsfdeq, length*sizeof(float));





       }
   }

   /*----------------------------------------------------------------*
    *  lsf quantizer (subrutine to LPCencode)
    *---------------------------------------------------------------*/

   void SimplelsfQ(
       float *lsfdeq,    /* (o) dequantized lsf coefficients
                              (dimension FILTERORDER) */
       int *index,     /* (o) quantization index */
       float *lsf,      /* (i) the lsf coefficient vector to be
                              quantized (dimension FILTERORDER ) */
       int lpc_n     /* (i) number of lsf sets to quantize */
   ){
       /* Quantize first LSF with memoryless split VQ */
       SplitVQ(lsfdeq, index, lsf, lsfCbTbl, LSF_NSPLIT,
           dim_lsfCbTbl, size_lsfCbTbl);

       if (lpc_n==2) {
           /* Quantize second LSF with memoryless split VQ */
           SplitVQ(lsfdeq + LPC_FILTERORDER, index + LSF_NSPLIT,
               lsf + LPC_FILTERORDER, lsfCbTbl, LSF_NSPLIT,
               dim_lsfCbTbl, size_lsfCbTbl);
       }
   }

   /*----------------------------------------------------------------*
    *  lpc encoder
    *---------------------------------------------------------------*/

   void LPCencode(
       float *syntdenum, /* (i/o) synthesis filter coefficients
                                  before/after encoding */
       float *weightdenum, /* (i/o) weighting denumerator
                                  coefficients before/after
                                  encoding */
       int *lsf_index,     /* (o) lsf quantization index */
       float *data,    /* (i) lsf coefficients to quantize */
       iLBC_Enc_Inst_t *iLBCenc_inst
                           /* (i/o) the encoder state structure */
   ){
       float lsf[LPC_FILTERORDER * LPC_N_MAX];
       float lsfdeq[LPC_FILTERORDER * LPC_N_MAX];
       int change=0;

       SimpleAnalysis(lsf, data, iLBCenc_inst);
       SimplelsfQ(lsfdeq, lsf_index, lsf, iLBCenc_inst->lpc_n);





       change=LSF_check(lsfdeq, LPC_FILTERORDER, iLBCenc_inst->lpc_n);
       SimpleInterpolateLSF(syntdenum, weightdenum,
           lsf, lsfdeq, iLBCenc_inst->lsfold,
           iLBCenc_inst->lsfdeqold, LPC_FILTERORDER, iLBCenc_inst);
   }

