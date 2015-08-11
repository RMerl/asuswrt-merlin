
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iLBC_encode.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <stdlib.h>
   #include <string.h>

   #include "iLBC_define.h"
   #include "LPCencode.h"
   #include "FrameClassify.h"
   #include "StateSearchW.h"
   #include "StateConstructW.h"
   #include "helpfun.h"
   #include "constants.h"
   #include "packing.h"
   #include "iCBSearch.h"
   #include "iCBConstruct.h"
   #include "hpInput.h"
   #include "anaFilter.h"
   #include "syntFilter.h"

   /*----------------------------------------------------------------*
    *  Initiation of encoder instance.
    *---------------------------------------------------------------*/

   short initEncode(                   /* (o) Number of bytes
                                              encoded */
       iLBC_Enc_Inst_t *iLBCenc_inst,  /* (i/o) Encoder instance */
       int mode                    /* (i) frame size mode */
   ){
       iLBCenc_inst->mode = mode;
       if (mode==30) {
           iLBCenc_inst->blockl = BLOCKL_30MS;
           iLBCenc_inst->nsub = NSUB_30MS;
           iLBCenc_inst->nasub = NASUB_30MS;
           iLBCenc_inst->lpc_n = LPC_N_30MS;
           iLBCenc_inst->no_of_bytes = NO_OF_BYTES_30MS;
           iLBCenc_inst->no_of_words = NO_OF_WORDS_30MS;





           iLBCenc_inst->state_short_len=STATE_SHORT_LEN_30MS;
           /* ULP init */
           iLBCenc_inst->ULP_inst=&ULP_30msTbl;
       }
       else if (mode==20) {
           iLBCenc_inst->blockl = BLOCKL_20MS;
           iLBCenc_inst->nsub = NSUB_20MS;
           iLBCenc_inst->nasub = NASUB_20MS;
           iLBCenc_inst->lpc_n = LPC_N_20MS;
           iLBCenc_inst->no_of_bytes = NO_OF_BYTES_20MS;
           iLBCenc_inst->no_of_words = NO_OF_WORDS_20MS;
           iLBCenc_inst->state_short_len=STATE_SHORT_LEN_20MS;
           /* ULP init */
           iLBCenc_inst->ULP_inst=&ULP_20msTbl;
       }
       else {
           exit(2);
       }

       memset((*iLBCenc_inst).anaMem, 0,
           LPC_FILTERORDER*sizeof(float));
       memcpy((*iLBCenc_inst).lsfold, lsfmeanTbl,
           LPC_FILTERORDER*sizeof(float));
       memcpy((*iLBCenc_inst).lsfdeqold, lsfmeanTbl,
           LPC_FILTERORDER*sizeof(float));
       memset((*iLBCenc_inst).lpc_buffer, 0,
           (LPC_LOOKBACK+BLOCKL_MAX)*sizeof(float));
       memset((*iLBCenc_inst).hpimem, 0, 4*sizeof(float));

       return (short)(iLBCenc_inst->no_of_bytes);
   }

   /*----------------------------------------------------------------*
    *  main encoder function
    *---------------------------------------------------------------*/

   void iLBC_encode(
       unsigned char *bytes,           /* (o) encoded data bits iLBC */
       float *block,                   /* (o) speech vector to
                                              encode */
       iLBC_Enc_Inst_t *iLBCenc_inst   /* (i/o) the general encoder
                                              state */
   ){

       float data[BLOCKL_MAX];
       float residual[BLOCKL_MAX], reverseResidual[BLOCKL_MAX];

       int start, idxForMax, idxVec[STATE_LEN];





       float reverseDecresidual[BLOCKL_MAX], mem[CB_MEML];
       int n, k, meml_gotten, Nfor, Nback, i, pos;
       int gain_index[CB_NSTAGES*NASUB_MAX],
           extra_gain_index[CB_NSTAGES];
       int cb_index[CB_NSTAGES*NASUB_MAX],extra_cb_index[CB_NSTAGES];
       int lsf_i[LSF_NSPLIT*LPC_N_MAX];
       unsigned char *pbytes;
       int diff, start_pos, state_first;
       float en1, en2;
       int index, ulp, firstpart;
       int subcount, subframe;
       float weightState[LPC_FILTERORDER];
       float syntdenum[NSUB_MAX*(LPC_FILTERORDER+1)];
       float weightdenum[NSUB_MAX*(LPC_FILTERORDER+1)];
       float decresidual[BLOCKL_MAX];

       /* high pass filtering of input signal if such is not done
              prior to calling this function */

       hpInput(block, iLBCenc_inst->blockl,
                   data, (*iLBCenc_inst).hpimem);

       /* otherwise simply copy */

       /*memcpy(data,block,iLBCenc_inst->blockl*sizeof(float));*/

       /* LPC of hp filtered input data */

       LPCencode(syntdenum, weightdenum, lsf_i, data, iLBCenc_inst);


       /* inverse filter to get residual */

       for (n=0; n<iLBCenc_inst->nsub; n++) {
           anaFilter(&data[n*SUBL], &syntdenum[n*(LPC_FILTERORDER+1)],
               SUBL, &residual[n*SUBL], iLBCenc_inst->anaMem);
       }

       /* find state location */

       start = FrameClassify(iLBCenc_inst, residual);

       /* check if state should be in first or last part of the
       two subframes */

       diff = STATE_LEN - iLBCenc_inst->state_short_len;
       en1 = 0;
       index = (start-1)*SUBL;





       for (i = 0; i < iLBCenc_inst->state_short_len; i++) {
           en1 += residual[index+i]*residual[index+i];
       }
       en2 = 0;
       index = (start-1)*SUBL+diff;
       for (i = 0; i < iLBCenc_inst->state_short_len; i++) {
           en2 += residual[index+i]*residual[index+i];
       }


       if (en1 > en2) {
           state_first = 1;
           start_pos = (start-1)*SUBL;
       } else {
           state_first = 0;
           start_pos = (start-1)*SUBL + diff;
       }

       /* scalar quantization of state */

       StateSearchW(iLBCenc_inst, &residual[start_pos],
           &syntdenum[(start-1)*(LPC_FILTERORDER+1)],
           &weightdenum[(start-1)*(LPC_FILTERORDER+1)], &idxForMax,
           idxVec, iLBCenc_inst->state_short_len, state_first);

       StateConstructW(idxForMax, idxVec,
           &syntdenum[(start-1)*(LPC_FILTERORDER+1)],
           &decresidual[start_pos], iLBCenc_inst->state_short_len);

       /* predictive quantization in state */

       if (state_first) { /* put adaptive part in the end */

           /* setup memory */

           memset(mem, 0,
               (CB_MEML-iLBCenc_inst->state_short_len)*sizeof(float));
           memcpy(mem+CB_MEML-iLBCenc_inst->state_short_len,
               decresidual+start_pos,
               iLBCenc_inst->state_short_len*sizeof(float));
           memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

           /* encode sub-frames */

           iCBSearch(iLBCenc_inst, extra_cb_index, extra_gain_index,
               &residual[start_pos+iLBCenc_inst->state_short_len],
               mem+CB_MEML-stMemLTbl,
               stMemLTbl, diff, CB_NSTAGES,





               &weightdenum[start*(LPC_FILTERORDER+1)],
               weightState, 0);

           /* construct decoded vector */

           iCBConstruct(
               &decresidual[start_pos+iLBCenc_inst->state_short_len],
               extra_cb_index, extra_gain_index,
               mem+CB_MEML-stMemLTbl,
               stMemLTbl, diff, CB_NSTAGES);

       }
       else { /* put adaptive part in the beginning */

           /* create reversed vectors for prediction */

           for (k=0; k<diff; k++) {
               reverseResidual[k] = residual[(start+1)*SUBL-1
                   -(k+iLBCenc_inst->state_short_len)];
           }

           /* setup memory */

           meml_gotten = iLBCenc_inst->state_short_len;
           for (k=0; k<meml_gotten; k++) {
               mem[CB_MEML-1-k] = decresidual[start_pos + k];
           }
           memset(mem, 0, (CB_MEML-k)*sizeof(float));
           memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

           /* encode sub-frames */

           iCBSearch(iLBCenc_inst, extra_cb_index, extra_gain_index,
               reverseResidual, mem+CB_MEML-stMemLTbl, stMemLTbl,
               diff, CB_NSTAGES,
               &weightdenum[(start-1)*(LPC_FILTERORDER+1)],
               weightState, 0);

           /* construct decoded vector */

           iCBConstruct(reverseDecresidual, extra_cb_index,
               extra_gain_index, mem+CB_MEML-stMemLTbl, stMemLTbl,
               diff, CB_NSTAGES);

           /* get decoded residual from reversed vector */

           for (k=0; k<diff; k++) {
               decresidual[start_pos-1-k] = reverseDecresidual[k];





           }
       }

       /* counter for predicted sub-frames */

       subcount=0;

       /* forward prediction of sub-frames */

       Nfor = iLBCenc_inst->nsub-start-1;


       if ( Nfor > 0 ) {

           /* setup memory */

           memset(mem, 0, (CB_MEML-STATE_LEN)*sizeof(float));
           memcpy(mem+CB_MEML-STATE_LEN, decresidual+(start-1)*SUBL,
               STATE_LEN*sizeof(float));
           memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

           /* loop over sub-frames to encode */

           for (subframe=0; subframe<Nfor; subframe++) {

               /* encode sub-frame */

               iCBSearch(iLBCenc_inst, cb_index+subcount*CB_NSTAGES,
                   gain_index+subcount*CB_NSTAGES,
                   &residual[(start+1+subframe)*SUBL],
                   mem+CB_MEML-memLfTbl[subcount],
                   memLfTbl[subcount], SUBL, CB_NSTAGES,
                   &weightdenum[(start+1+subframe)*
                               (LPC_FILTERORDER+1)],
                   weightState, subcount+1);

               /* construct decoded vector */

               iCBConstruct(&decresidual[(start+1+subframe)*SUBL],
                   cb_index+subcount*CB_NSTAGES,
                   gain_index+subcount*CB_NSTAGES,
                   mem+CB_MEML-memLfTbl[subcount],
                   memLfTbl[subcount], SUBL, CB_NSTAGES);

               /* update memory */

               memcpy(mem, mem+SUBL, (CB_MEML-SUBL)*sizeof(float));
               memcpy(mem+CB_MEML-SUBL,





                   &decresidual[(start+1+subframe)*SUBL],
                   SUBL*sizeof(float));
               memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

               subcount++;
           }
       }


       /* backward prediction of sub-frames */

       Nback = start-1;


       if ( Nback > 0 ) {

           /* create reverse order vectors */

           for (n=0; n<Nback; n++) {
               for (k=0; k<SUBL; k++) {
                   reverseResidual[n*SUBL+k] =
                       residual[(start-1)*SUBL-1-n*SUBL-k];
                   reverseDecresidual[n*SUBL+k] =
                       decresidual[(start-1)*SUBL-1-n*SUBL-k];
               }
           }

           /* setup memory */

           meml_gotten = SUBL*(iLBCenc_inst->nsub+1-start);


           if ( meml_gotten > CB_MEML ) {
               meml_gotten=CB_MEML;
           }
           for (k=0; k<meml_gotten; k++) {
               mem[CB_MEML-1-k] = decresidual[(start-1)*SUBL + k];
           }
           memset(mem, 0, (CB_MEML-k)*sizeof(float));
           memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

           /* loop over sub-frames to encode */

           for (subframe=0; subframe<Nback; subframe++) {

               /* encode sub-frame */

               iCBSearch(iLBCenc_inst, cb_index+subcount*CB_NSTAGES,





                   gain_index+subcount*CB_NSTAGES,
                   &reverseResidual[subframe*SUBL],
                   mem+CB_MEML-memLfTbl[subcount],
                   memLfTbl[subcount], SUBL, CB_NSTAGES,
                   &weightdenum[(start-2-subframe)*
                               (LPC_FILTERORDER+1)],
                   weightState, subcount+1);

               /* construct decoded vector */

               iCBConstruct(&reverseDecresidual[subframe*SUBL],
                   cb_index+subcount*CB_NSTAGES,
                   gain_index+subcount*CB_NSTAGES,
                   mem+CB_MEML-memLfTbl[subcount],
                   memLfTbl[subcount], SUBL, CB_NSTAGES);

               /* update memory */

               memcpy(mem, mem+SUBL, (CB_MEML-SUBL)*sizeof(float));
               memcpy(mem+CB_MEML-SUBL,
                   &reverseDecresidual[subframe*SUBL],
                   SUBL*sizeof(float));
               memset(weightState, 0, LPC_FILTERORDER*sizeof(float));

               subcount++;

           }

           /* get decoded residual from reversed vector */

           for (i=0; i<SUBL*Nback; i++) {
               decresidual[SUBL*Nback - i - 1] =
                   reverseDecresidual[i];
           }
       }
       /* end encoding part */

       /* adjust index */
       index_conv_enc(cb_index);

       /* pack bytes */

       pbytes=bytes;
       pos=0;

       /* loop over the 3 ULP classes */

       for (ulp=0; ulp<3; ulp++) {






           /* LSF */
           for (k=0; k<LSF_NSPLIT*iLBCenc_inst->lpc_n; k++) {
               packsplit(&lsf_i[k], &firstpart, &lsf_i[k],
                   iLBCenc_inst->ULP_inst->lsf_bits[k][ulp],
                   iLBCenc_inst->ULP_inst->lsf_bits[k][ulp]+
                   iLBCenc_inst->ULP_inst->lsf_bits[k][ulp+1]+
                   iLBCenc_inst->ULP_inst->lsf_bits[k][ulp+2]);
               dopack( &pbytes, firstpart,
                   iLBCenc_inst->ULP_inst->lsf_bits[k][ulp], &pos);
           }

           /* Start block info */

           packsplit(&start, &firstpart, &start,
               iLBCenc_inst->ULP_inst->start_bits[ulp],
               iLBCenc_inst->ULP_inst->start_bits[ulp]+
               iLBCenc_inst->ULP_inst->start_bits[ulp+1]+
               iLBCenc_inst->ULP_inst->start_bits[ulp+2]);
           dopack( &pbytes, firstpart,
               iLBCenc_inst->ULP_inst->start_bits[ulp], &pos);

           packsplit(&state_first, &firstpart, &state_first,
               iLBCenc_inst->ULP_inst->startfirst_bits[ulp],
               iLBCenc_inst->ULP_inst->startfirst_bits[ulp]+
               iLBCenc_inst->ULP_inst->startfirst_bits[ulp+1]+
               iLBCenc_inst->ULP_inst->startfirst_bits[ulp+2]);
           dopack( &pbytes, firstpart,
               iLBCenc_inst->ULP_inst->startfirst_bits[ulp], &pos);

           packsplit(&idxForMax, &firstpart, &idxForMax,
               iLBCenc_inst->ULP_inst->scale_bits[ulp],
               iLBCenc_inst->ULP_inst->scale_bits[ulp]+
               iLBCenc_inst->ULP_inst->scale_bits[ulp+1]+
               iLBCenc_inst->ULP_inst->scale_bits[ulp+2]);
           dopack( &pbytes, firstpart,
               iLBCenc_inst->ULP_inst->scale_bits[ulp], &pos);

           for (k=0; k<iLBCenc_inst->state_short_len; k++) {
               packsplit(idxVec+k, &firstpart, idxVec+k,
                   iLBCenc_inst->ULP_inst->state_bits[ulp],
                   iLBCenc_inst->ULP_inst->state_bits[ulp]+
                   iLBCenc_inst->ULP_inst->state_bits[ulp+1]+
                   iLBCenc_inst->ULP_inst->state_bits[ulp+2]);
               dopack( &pbytes, firstpart,
                   iLBCenc_inst->ULP_inst->state_bits[ulp], &pos);
           }






           /* 23/22 (20ms/30ms) sample block */

           for (k=0;k<CB_NSTAGES;k++) {
               packsplit(extra_cb_index+k, &firstpart,
                   extra_cb_index+k,
                   iLBCenc_inst->ULP_inst->extra_cb_index[k][ulp],
                   iLBCenc_inst->ULP_inst->extra_cb_index[k][ulp]+
                   iLBCenc_inst->ULP_inst->extra_cb_index[k][ulp+1]+
                   iLBCenc_inst->ULP_inst->extra_cb_index[k][ulp+2]);
               dopack( &pbytes, firstpart,
                   iLBCenc_inst->ULP_inst->extra_cb_index[k][ulp],
                   &pos);
           }

           for (k=0;k<CB_NSTAGES;k++) {
               packsplit(extra_gain_index+k, &firstpart,
                   extra_gain_index+k,
                   iLBCenc_inst->ULP_inst->extra_cb_gain[k][ulp],
                   iLBCenc_inst->ULP_inst->extra_cb_gain[k][ulp]+
                   iLBCenc_inst->ULP_inst->extra_cb_gain[k][ulp+1]+
                   iLBCenc_inst->ULP_inst->extra_cb_gain[k][ulp+2]);
               dopack( &pbytes, firstpart,
                   iLBCenc_inst->ULP_inst->extra_cb_gain[k][ulp],
                   &pos);
           }

           /* The two/four (20ms/30ms) 40 sample sub-blocks */

           for (i=0; i<iLBCenc_inst->nasub; i++) {
               for (k=0; k<CB_NSTAGES; k++) {
                   packsplit(cb_index+i*CB_NSTAGES+k, &firstpart,
                       cb_index+i*CB_NSTAGES+k,
                       iLBCenc_inst->ULP_inst->cb_index[i][k][ulp],
                       iLBCenc_inst->ULP_inst->cb_index[i][k][ulp]+
                       iLBCenc_inst->ULP_inst->cb_index[i][k][ulp+1]+
                       iLBCenc_inst->ULP_inst->cb_index[i][k][ulp+2]);
                   dopack( &pbytes, firstpart,
                       iLBCenc_inst->ULP_inst->cb_index[i][k][ulp],
                       &pos);
               }
           }

           for (i=0; i<iLBCenc_inst->nasub; i++) {
               for (k=0; k<CB_NSTAGES; k++) {
                   packsplit(gain_index+i*CB_NSTAGES+k, &firstpart,
                       gain_index+i*CB_NSTAGES+k,
                       iLBCenc_inst->ULP_inst->cb_gain[i][k][ulp],
                       iLBCenc_inst->ULP_inst->cb_gain[i][k][ulp]+





                       iLBCenc_inst->ULP_inst->cb_gain[i][k][ulp+1]+
                       iLBCenc_inst->ULP_inst->cb_gain[i][k][ulp+2]);
                   dopack( &pbytes, firstpart,
                       iLBCenc_inst->ULP_inst->cb_gain[i][k][ulp],
                       &pos);
               }
           }
       }

       /* set the last bit to zero (otherwise the decoder
          will treat it as a lost frame) */
       dopack( &pbytes, 0, 1, &pos);
   }

