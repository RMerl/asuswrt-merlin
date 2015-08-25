
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iLBC_decode.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <stdlib.h>

   #include "iLBC_define.h"
   #include "StateConstructW.h"
   #include "LPCdecode.h"
   #include "iCBConstruct.h"
   #include "doCPLC.h"
   #include "helpfun.h"
   #include "constants.h"
   #include "packing.h"
   #include "string.h"
   #include "enhancer.h"
   #include "hpOutput.h"
   #include "syntFilter.h"

   /*----------------------------------------------------------------*
    *  Initiation of decoder instance.
    *---------------------------------------------------------------*/

   short initDecode(                   /* (o) Number of decoded
                                              samples */
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) Decoder instance */
       int mode,                       /* (i) frame size mode */
       int use_enhancer                /* (i) 1 to use enhancer
                                              0 to run without
                                                enhancer */
   ){
       int i;

       iLBCdec_inst->mode = mode;





       if (mode==30) {
           iLBCdec_inst->blockl = BLOCKL_30MS;
           iLBCdec_inst->nsub = NSUB_30MS;
           iLBCdec_inst->nasub = NASUB_30MS;
           iLBCdec_inst->lpc_n = LPC_N_30MS;
           iLBCdec_inst->no_of_bytes = NO_OF_BYTES_30MS;
           iLBCdec_inst->no_of_words = NO_OF_WORDS_30MS;
           iLBCdec_inst->state_short_len=STATE_SHORT_LEN_30MS;
           /* ULP init */
           iLBCdec_inst->ULP_inst=&ULP_30msTbl;
       }
       else if (mode==20) {
           iLBCdec_inst->blockl = BLOCKL_20MS;
           iLBCdec_inst->nsub = NSUB_20MS;
           iLBCdec_inst->nasub = NASUB_20MS;
           iLBCdec_inst->lpc_n = LPC_N_20MS;
           iLBCdec_inst->no_of_bytes = NO_OF_BYTES_20MS;
           iLBCdec_inst->no_of_words = NO_OF_WORDS_20MS;
           iLBCdec_inst->state_short_len=STATE_SHORT_LEN_20MS;
           /* ULP init */
           iLBCdec_inst->ULP_inst=&ULP_20msTbl;
       }
       else {
           exit(2);
       }

       memset(iLBCdec_inst->syntMem, 0,
           LPC_FILTERORDER*sizeof(float));
       memcpy((*iLBCdec_inst).lsfdeqold, lsfmeanTbl,
           LPC_FILTERORDER*sizeof(float));

       memset(iLBCdec_inst->old_syntdenum, 0,
           ((LPC_FILTERORDER + 1)*NSUB_MAX)*sizeof(float));
       for (i=0; i<NSUB_MAX; i++)
           iLBCdec_inst->old_syntdenum[i*(LPC_FILTERORDER+1)]=1.0;

       iLBCdec_inst->last_lag = 20;

       iLBCdec_inst->prevLag = 120;
       iLBCdec_inst->per = 0.0;
       iLBCdec_inst->consPLICount = 0;
       iLBCdec_inst->prevPLI = 0;
       iLBCdec_inst->prevLpc[0] = 1.0;
       memset(iLBCdec_inst->prevLpc+1,0,
           LPC_FILTERORDER*sizeof(float));
       memset(iLBCdec_inst->prevResidual, 0, BLOCKL_MAX*sizeof(float));
       iLBCdec_inst->seed=777;






       memset(iLBCdec_inst->hpomem, 0, 4*sizeof(float));

       iLBCdec_inst->use_enhancer = use_enhancer;
       memset(iLBCdec_inst->enh_buf, 0, ENH_BUFL*sizeof(float));
       for (i=0;i<ENH_NBLOCKS_TOT;i++)
           iLBCdec_inst->enh_period[i]=(float)40.0;

       iLBCdec_inst->prev_enh_pl = 0;

       return (short)(iLBCdec_inst->blockl);
   }

   /*----------------------------------------------------------------*
    *  frame residual decoder function (subrutine to iLBC_decode)
    *---------------------------------------------------------------*/

   void Decode(
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) the decoder state
                                                structure */
       float *decresidual,             /* (o) decoded residual frame */
       int start,                      /* (i) location of start
                                              state */
       int idxForMax,                  /* (i) codebook index for the
                                              maximum value */
       int *idxVec,                /* (i) codebook indexes for the
                                              samples  in the start
                                              state */
       float *syntdenum,               /* (i) the decoded synthesis
                                              filter coefficients */
       int *cb_index,                  /* (i) the indexes for the
                                              adaptive codebook */
       int *gain_index,            /* (i) the indexes for the
                                              corresponding gains */
       int *extra_cb_index,        /* (i) the indexes for the
                                              adaptive codebook part
                                              of start state */
       int *extra_gain_index,          /* (i) the indexes for the
                                              corresponding gains */
       int state_first                 /* (i) 1 if non adaptive part
                                              of start state comes
                                              first 0 if that part
                                              comes last */
   ){
       float reverseDecresidual[BLOCKL_MAX], mem[CB_MEML];
       int k, meml_gotten, Nfor, Nback, i;
       int diff, start_pos;
       int subcount, subframe;






       diff = STATE_LEN - iLBCdec_inst->state_short_len;

       if (state_first == 1) {
           start_pos = (start-1)*SUBL;
       } else {
           start_pos = (start-1)*SUBL + diff;
       }

       /* decode scalar part of start state */

       StateConstructW(idxForMax, idxVec,
           &syntdenum[(start-1)*(LPC_FILTERORDER+1)],
           &decresidual[start_pos], iLBCdec_inst->state_short_len);


       if (state_first) { /* put adaptive part in the end */

           /* setup memory */

           memset(mem, 0,
               (CB_MEML-iLBCdec_inst->state_short_len)*sizeof(float));
           memcpy(mem+CB_MEML-iLBCdec_inst->state_short_len,
               decresidual+start_pos,
               iLBCdec_inst->state_short_len*sizeof(float));

           /* construct decoded vector */

           iCBConstruct(
               &decresidual[start_pos+iLBCdec_inst->state_short_len],
               extra_cb_index, extra_gain_index, mem+CB_MEML-stMemLTbl,
               stMemLTbl, diff, CB_NSTAGES);

       }
       else {/* put adaptive part in the beginning */

           /* create reversed vectors for prediction */

           for (k=0; k<diff; k++) {
               reverseDecresidual[k] =
                   decresidual[(start+1)*SUBL-1-
                           (k+iLBCdec_inst->state_short_len)];
           }

           /* setup memory */

           meml_gotten = iLBCdec_inst->state_short_len;
           for (k=0; k<meml_gotten; k++){
               mem[CB_MEML-1-k] = decresidual[start_pos + k];





           }
           memset(mem, 0, (CB_MEML-k)*sizeof(float));

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

       Nfor = iLBCdec_inst->nsub-start-1;

       if ( Nfor > 0 ){

           /* setup memory */

           memset(mem, 0, (CB_MEML-STATE_LEN)*sizeof(float));
           memcpy(mem+CB_MEML-STATE_LEN, decresidual+(start-1)*SUBL,
               STATE_LEN*sizeof(float));

           /* loop over sub-frames to encode */

           for (subframe=0; subframe<Nfor; subframe++) {

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

               subcount++;

           }

       }

       /* backward prediction of sub-frames */

       Nback = start-1;

       if ( Nback > 0 ) {

           /* setup memory */

           meml_gotten = SUBL*(iLBCdec_inst->nsub+1-start);

           if ( meml_gotten > CB_MEML ) {
               meml_gotten=CB_MEML;
           }
           for (k=0; k<meml_gotten; k++) {
               mem[CB_MEML-1-k] = decresidual[(start-1)*SUBL + k];
           }
           memset(mem, 0, (CB_MEML-k)*sizeof(float));

           /* loop over subframes to decode */

           for (subframe=0; subframe<Nback; subframe++) {

               /* construct decoded vector */

               iCBConstruct(&reverseDecresidual[subframe*SUBL],
                   cb_index+subcount*CB_NSTAGES,
                   gain_index+subcount*CB_NSTAGES,
                   mem+CB_MEML-memLfTbl[subcount], memLfTbl[subcount],
                   SUBL, CB_NSTAGES);

               /* update memory */

               memcpy(mem, mem+SUBL, (CB_MEML-SUBL)*sizeof(float));
               memcpy(mem+CB_MEML-SUBL,
                   &reverseDecresidual[subframe*SUBL],
                   SUBL*sizeof(float));

               subcount++;
           }





           /* get decoded residual from reversed vector */

           for (i=0; i<SUBL*Nback; i++)
               decresidual[SUBL*Nback - i - 1] =
               reverseDecresidual[i];
       }
   }

   /*----------------------------------------------------------------*
    *  main decoder function
    *---------------------------------------------------------------*/

   void iLBC_decode(
       float *decblock,            /* (o) decoded signal block */
       unsigned char *bytes,           /* (i) encoded signal bits */
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) the decoder state
                                                structure */
       int mode                    /* (i) 0: bad packet, PLC,
                                              1: normal */
   ){
       float data[BLOCKL_MAX];
       float lsfdeq[LPC_FILTERORDER*LPC_N_MAX];
       float PLCresidual[BLOCKL_MAX], PLClpc[LPC_FILTERORDER + 1];
       float zeros[BLOCKL_MAX], one[LPC_FILTERORDER + 1];
       int k, i, start, idxForMax, pos, lastpart, ulp;
       int lag, ilag;
       float cc, maxcc;
       int idxVec[STATE_LEN];
       int check;
       int gain_index[NASUB_MAX*CB_NSTAGES],
           extra_gain_index[CB_NSTAGES];
       int cb_index[CB_NSTAGES*NASUB_MAX], extra_cb_index[CB_NSTAGES];
       int lsf_i[LSF_NSPLIT*LPC_N_MAX];
       int state_first;
       int last_bit;
       unsigned char *pbytes;
       float weightdenum[(LPC_FILTERORDER + 1)*NSUB_MAX];
       int order_plus_one;
       float syntdenum[NSUB_MAX*(LPC_FILTERORDER+1)];
       float decresidual[BLOCKL_MAX];

       if (mode>0) { /* the data are good */

           /* decode data */

           pbytes=bytes;
           pos=0;






           /* Set everything to zero before decoding */

           for (k=0; k<LSF_NSPLIT*LPC_N_MAX; k++) {
               lsf_i[k]=0;
           }
           start=0;
           state_first=0;
           idxForMax=0;
           for (k=0; k<iLBCdec_inst->state_short_len; k++) {
               idxVec[k]=0;
           }
           for (k=0; k<CB_NSTAGES; k++) {
               extra_cb_index[k]=0;
           }
           for (k=0; k<CB_NSTAGES; k++) {
               extra_gain_index[k]=0;
           }
           for (i=0; i<iLBCdec_inst->nasub; i++) {
               for (k=0; k<CB_NSTAGES; k++) {
                   cb_index[i*CB_NSTAGES+k]=0;
               }
           }
           for (i=0; i<iLBCdec_inst->nasub; i++) {
               for (k=0; k<CB_NSTAGES; k++) {
                   gain_index[i*CB_NSTAGES+k]=0;
               }
           }

           /* loop over ULP classes */

           for (ulp=0; ulp<3; ulp++) {

               /* LSF */
               for (k=0; k<LSF_NSPLIT*iLBCdec_inst->lpc_n; k++){
                   unpack( &pbytes, &lastpart,
                       iLBCdec_inst->ULP_inst->lsf_bits[k][ulp], &pos);
                   packcombine(&lsf_i[k], lastpart,
                       iLBCdec_inst->ULP_inst->lsf_bits[k][ulp]);
               }

               /* Start block info */

               unpack( &pbytes, &lastpart,
                   iLBCdec_inst->ULP_inst->start_bits[ulp], &pos);
               packcombine(&start, lastpart,
                   iLBCdec_inst->ULP_inst->start_bits[ulp]);

               unpack( &pbytes, &lastpart,





                   iLBCdec_inst->ULP_inst->startfirst_bits[ulp], &pos);
               packcombine(&state_first, lastpart,
                   iLBCdec_inst->ULP_inst->startfirst_bits[ulp]);

               unpack( &pbytes, &lastpart,
                   iLBCdec_inst->ULP_inst->scale_bits[ulp], &pos);
               packcombine(&idxForMax, lastpart,
                   iLBCdec_inst->ULP_inst->scale_bits[ulp]);

               for (k=0; k<iLBCdec_inst->state_short_len; k++) {
                   unpack( &pbytes, &lastpart,
                       iLBCdec_inst->ULP_inst->state_bits[ulp], &pos);
                   packcombine(idxVec+k, lastpart,
                       iLBCdec_inst->ULP_inst->state_bits[ulp]);
               }

               /* 23/22 (20ms/30ms) sample block */

               for (k=0; k<CB_NSTAGES; k++) {
                   unpack( &pbytes, &lastpart,
                       iLBCdec_inst->ULP_inst->extra_cb_index[k][ulp],
                       &pos);
                   packcombine(extra_cb_index+k, lastpart,
                       iLBCdec_inst->ULP_inst->extra_cb_index[k][ulp]);
               }
               for (k=0; k<CB_NSTAGES; k++) {
                   unpack( &pbytes, &lastpart,
                       iLBCdec_inst->ULP_inst->extra_cb_gain[k][ulp],
                       &pos);
                   packcombine(extra_gain_index+k, lastpart,
                       iLBCdec_inst->ULP_inst->extra_cb_gain[k][ulp]);
               }

               /* The two/four (20ms/30ms) 40 sample sub-blocks */

               for (i=0; i<iLBCdec_inst->nasub; i++) {
                   for (k=0; k<CB_NSTAGES; k++) {
                       unpack( &pbytes, &lastpart,
                       iLBCdec_inst->ULP_inst->cb_index[i][k][ulp],
                           &pos);
                       packcombine(cb_index+i*CB_NSTAGES+k, lastpart,
                       iLBCdec_inst->ULP_inst->cb_index[i][k][ulp]);
                   }
               }

               for (i=0; i<iLBCdec_inst->nasub; i++) {
                   for (k=0; k<CB_NSTAGES; k++) {
                       unpack( &pbytes, &lastpart,





                       iLBCdec_inst->ULP_inst->cb_gain[i][k][ulp],
                           &pos);
                       packcombine(gain_index+i*CB_NSTAGES+k, lastpart,
                           iLBCdec_inst->ULP_inst->cb_gain[i][k][ulp]);
                   }
               }
           }
           /* Extract last bit. If it is 1 this indicates an
              empty/lost frame */
           unpack( &pbytes, &last_bit, 1, &pos);

           /* Check for bit errors or empty/lost frames */
           if (start<1)
               mode = 0;
           if (iLBCdec_inst->mode==20 && start>3)
               mode = 0;
           if (iLBCdec_inst->mode==30 && start>5)
               mode = 0;
           if (last_bit==1)
               mode = 0;

           if (mode==1) { /* No bit errors was detected,
                             continue decoding */

               /* adjust index */
               index_conv_dec(cb_index);

               /* decode the lsf */

               SimplelsfDEQ(lsfdeq, lsf_i, iLBCdec_inst->lpc_n);
               check=LSF_check(lsfdeq, LPC_FILTERORDER,
                   iLBCdec_inst->lpc_n);
               DecoderInterpolateLSF(syntdenum, weightdenum,
                   lsfdeq, LPC_FILTERORDER, iLBCdec_inst);

               Decode(iLBCdec_inst, decresidual, start, idxForMax,
                   idxVec, syntdenum, cb_index, gain_index,
                   extra_cb_index, extra_gain_index,
                   state_first);

               /* preparing the plc for a future loss! */

               doThePLC(PLCresidual, PLClpc, 0, decresidual,
                   syntdenum +
                   (LPC_FILTERORDER + 1)*(iLBCdec_inst->nsub - 1),
                   (*iLBCdec_inst).last_lag, iLBCdec_inst);







               memcpy(decresidual, PLCresidual,
                   iLBCdec_inst->blockl*sizeof(float));
           }

       }

       if (mode == 0) {
           /* the data is bad (either a PLC call
            * was made or a severe bit error was detected)
            */

           /* packet loss conceal */

           memset(zeros, 0, BLOCKL_MAX*sizeof(float));

           one[0] = 1;
           memset(one+1, 0, LPC_FILTERORDER*sizeof(float));

           start=0;

           doThePLC(PLCresidual, PLClpc, 1, zeros, one,
               (*iLBCdec_inst).last_lag, iLBCdec_inst);
           memcpy(decresidual, PLCresidual,
               iLBCdec_inst->blockl*sizeof(float));

           order_plus_one = LPC_FILTERORDER + 1;
           for (i = 0; i < iLBCdec_inst->nsub; i++) {
               memcpy(syntdenum+(i*order_plus_one), PLClpc,
                   order_plus_one*sizeof(float));
           }
       }

       if (iLBCdec_inst->use_enhancer == 1) {

           /* post filtering */

           iLBCdec_inst->last_lag =
               enhancerInterface(data, decresidual, iLBCdec_inst);

           /* synthesis filtering */

           if (iLBCdec_inst->mode==20) {
               /* Enhancer has 40 samples delay */
               i=0;
               syntFilter(data + i*SUBL,
                   iLBCdec_inst->old_syntdenum +
                   (i+iLBCdec_inst->nsub-1)*(LPC_FILTERORDER+1),
                   SUBL, iLBCdec_inst->syntMem);





               for (i=1; i < iLBCdec_inst->nsub; i++) {
                   syntFilter(data + i*SUBL,
                       syntdenum + (i-1)*(LPC_FILTERORDER+1),
                       SUBL, iLBCdec_inst->syntMem);
               }
           } else if (iLBCdec_inst->mode==30) {
               /* Enhancer has 80 samples delay */
               for (i=0; i < 2; i++) {
                   syntFilter(data + i*SUBL,
                       iLBCdec_inst->old_syntdenum +
                       (i+iLBCdec_inst->nsub-2)*(LPC_FILTERORDER+1),
                       SUBL, iLBCdec_inst->syntMem);
               }
               for (i=2; i < iLBCdec_inst->nsub; i++) {
                   syntFilter(data + i*SUBL,
                       syntdenum + (i-2)*(LPC_FILTERORDER+1), SUBL,
                       iLBCdec_inst->syntMem);
               }
           }

       } else {

           /* Find last lag */
           lag = 20;
           maxcc = xCorrCoef(&decresidual[BLOCKL_MAX-ENH_BLOCKL],
               &decresidual[BLOCKL_MAX-ENH_BLOCKL-lag], ENH_BLOCKL);

           for (ilag=21; ilag<120; ilag++) {
               cc = xCorrCoef(&decresidual[BLOCKL_MAX-ENH_BLOCKL],
                   &decresidual[BLOCKL_MAX-ENH_BLOCKL-ilag],
                   ENH_BLOCKL);

               if (cc > maxcc) {
                   maxcc = cc;
                   lag = ilag;
               }
           }
           iLBCdec_inst->last_lag = lag;

           /* copy data and run synthesis filter */

           memcpy(data, decresidual,
               iLBCdec_inst->blockl*sizeof(float));
           for (i=0; i < iLBCdec_inst->nsub; i++) {
               syntFilter(data + i*SUBL,
                   syntdenum + i*(LPC_FILTERORDER+1), SUBL,
                   iLBCdec_inst->syntMem);
           }





       }

       /* high pass filtering on output if desired, otherwise
          copy to out */

       hpOutput(data, iLBCdec_inst->blockl,
                   decblock,iLBCdec_inst->hpomem);

       /* memcpy(decblock,data,iLBCdec_inst->blockl*sizeof(float));*/

       memcpy(iLBCdec_inst->old_syntdenum, syntdenum,

           iLBCdec_inst->nsub*(LPC_FILTERORDER+1)*sizeof(float));

       iLBCdec_inst->prev_enh_pl=0;

       if (mode==0) { /* PLC was used */
           iLBCdec_inst->prev_enh_pl=1;
       }
   }

