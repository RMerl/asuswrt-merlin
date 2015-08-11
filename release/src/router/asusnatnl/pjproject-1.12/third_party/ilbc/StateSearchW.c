
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       StateSearchW.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <string.h>

   #include "iLBC_define.h"
   #include "constants.h"
   #include "filter.h"
   #include "helpfun.h"

   /*----------------------------------------------------------------*
    *  predictive noise shaping encoding of scaled start state
    *  (subrutine for StateSearchW)
    *---------------------------------------------------------------*/

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
   ){
       float *syntOut;
       float syntOutBuf[LPC_FILTERORDER+STATE_SHORT_LEN_30MS];
       float toQ, xq;
       int n;
       int index;

       /* initialization of buffer for filtering */

       memset(syntOutBuf, 0, LPC_FILTERORDER*sizeof(float));






       /* initialization of pointer for filtering */

       syntOut = &syntOutBuf[LPC_FILTERORDER];

       /* synthesis and weighting filters on input */

       if (state_first) {
           AllPoleFilter (in, weightDenum, SUBL, LPC_FILTERORDER);
       } else {
           AllPoleFilter (in, weightDenum,
               iLBCenc_inst->state_short_len-SUBL,
               LPC_FILTERORDER);
       }

       /* encoding loop */

       for (n=0; n<len; n++) {

           /* time update of filter coefficients */

           if ((state_first)&&(n==SUBL)){
               syntDenum += (LPC_FILTERORDER+1);
               weightDenum += (LPC_FILTERORDER+1);

               /* synthesis and weighting filters on input */
               AllPoleFilter (&in[n], weightDenum, len-n,
                   LPC_FILTERORDER);

           } else if ((state_first==0)&&
               (n==(iLBCenc_inst->state_short_len-SUBL))) {
               syntDenum += (LPC_FILTERORDER+1);
               weightDenum += (LPC_FILTERORDER+1);

               /* synthesis and weighting filters on input */
               AllPoleFilter (&in[n], weightDenum, len-n,
                   LPC_FILTERORDER);

           }

           /* prediction of synthesized and weighted input */

           syntOut[n] = 0.0;
           AllPoleFilter (&syntOut[n], weightDenum, 1,
               LPC_FILTERORDER);

           /* quantization */

           toQ = in[n]-syntOut[n];





           sort_sq(&xq, &index, toQ, state_sq3Tbl, 8);
           out[n]=index;
           syntOut[n] = state_sq3Tbl[out[n]];

           /* update of the prediction filter */

           AllPoleFilter(&syntOut[n], weightDenum, 1,
               LPC_FILTERORDER);
       }
   }

   /*----------------------------------------------------------------*
    *  encoding of start state
    *---------------------------------------------------------------*/

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
   ){
       float dtmp, maxVal;
       float tmpbuf[LPC_FILTERORDER+2*STATE_SHORT_LEN_30MS];
       float *tmp, numerator[1+LPC_FILTERORDER];
       float foutbuf[LPC_FILTERORDER+2*STATE_SHORT_LEN_30MS], *fout;
       int k;
       float qmax, scal;

       /* initialization of buffers and filter coefficients */

       memset(tmpbuf, 0, LPC_FILTERORDER*sizeof(float));
       memset(foutbuf, 0, LPC_FILTERORDER*sizeof(float));
       for (k=0; k<LPC_FILTERORDER; k++) {
           numerator[k]=syntDenum[LPC_FILTERORDER-k];
       }
       numerator[LPC_FILTERORDER]=syntDenum[0];
       tmp = &tmpbuf[LPC_FILTERORDER];
       fout = &foutbuf[LPC_FILTERORDER];

       /* circular convolution with the all-pass filter */






       memcpy(tmp, residual, len*sizeof(float));
       memset(tmp+len, 0, len*sizeof(float));
       ZeroPoleFilter(tmp, numerator, syntDenum, 2*len,
           LPC_FILTERORDER, fout);
       for (k=0; k<len; k++) {
           fout[k] += fout[k+len];
       }

       /* identification of the maximum amplitude value */

       maxVal = fout[0];
       for (k=1; k<len; k++) {

           if (fout[k]*fout[k] > maxVal*maxVal){
               maxVal = fout[k];
           }
       }
       maxVal=(float)fabs(maxVal);

       /* encoding of the maximum amplitude value */

       if (maxVal < 10.0) {
           maxVal = 10.0;
       }
       maxVal = (float)log10(maxVal);
       sort_sq(&dtmp, idxForMax, maxVal, state_frgqTbl, 64);

       /* decoding of the maximum amplitude representation value,
          and corresponding scaling of start state */

       maxVal=state_frgqTbl[*idxForMax];
       qmax = (float)pow(10,maxVal);
       scal = (float)(4.5)/qmax;
       for (k=0; k<len; k++){
           fout[k] *= scal;
       }

       /* predictive noise shaping encoding of scaled start state */

       AbsQuantW(iLBCenc_inst, fout,syntDenum,
           weightDenum,idxVec, len, state_first);
   }











