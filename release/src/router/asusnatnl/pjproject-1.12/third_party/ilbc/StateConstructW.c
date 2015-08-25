
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       StateConstructW.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <string.h>

   #include "iLBC_define.h"
   #include "constants.h"
   #include "filter.h"

   /*----------------------------------------------------------------*
    *  decoding of the start state
    *---------------------------------------------------------------*/

   void StateConstructW(
       int idxForMax,      /* (i) 6-bit index for the quantization of
                                  max amplitude */
       int *idxVec,    /* (i) vector of quantization indexes */
       float *syntDenum,   /* (i) synthesis filter denumerator */





       float *out,         /* (o) the decoded state vector */
       int len             /* (i) length of a state vector */
   ){
       float maxVal, tmpbuf[LPC_FILTERORDER+2*STATE_LEN], *tmp,
           numerator[LPC_FILTERORDER+1];
       float foutbuf[LPC_FILTERORDER+2*STATE_LEN], *fout;
       int k,tmpi;

       /* decoding of the maximum value */

       maxVal = state_frgqTbl[idxForMax];
       maxVal = (float)pow(10,maxVal)/(float)4.5;

       /* initialization of buffers and coefficients */

       memset(tmpbuf, 0, LPC_FILTERORDER*sizeof(float));
       memset(foutbuf, 0, LPC_FILTERORDER*sizeof(float));
       for (k=0; k<LPC_FILTERORDER; k++) {
           numerator[k]=syntDenum[LPC_FILTERORDER-k];
       }
       numerator[LPC_FILTERORDER]=syntDenum[0];
       tmp = &tmpbuf[LPC_FILTERORDER];
       fout = &foutbuf[LPC_FILTERORDER];

       /* decoding of the sample values */

       for (k=0; k<len; k++) {
           tmpi = len-1-k;
           /* maxVal = 1/scal */
           tmp[k] = maxVal*state_sq3Tbl[idxVec[tmpi]];
       }

       /* circular convolution with all-pass filter */

       memset(tmp+len, 0, len*sizeof(float));
       ZeroPoleFilter(tmp, numerator, syntDenum, 2*len,
           LPC_FILTERORDER, fout);
       for (k=0;k<len;k++) {
           out[k] = fout[len-1-k]+fout[2*len-1-k];
       }
   }












