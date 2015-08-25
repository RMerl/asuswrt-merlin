
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       doCPLC.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <string.h>
   #include <stdio.h>





   #include "iLBC_define.h"

   /*----------------------------------------------------------------*
    *  Compute cross correlation and pitch gain for pitch prediction
    *  of last subframe at given lag.
    *---------------------------------------------------------------*/

   void compCorr(
       float *cc,      /* (o) cross correlation coefficient */
       float *gc,      /* (o) gain */
       float *pm,
       float *buffer,  /* (i) signal buffer */
       int lag,    /* (i) pitch lag */
       int bLen,       /* (i) length of buffer */
       int sRange      /* (i) correlation search length */
   ){
       int i;
       float ftmp1, ftmp2, ftmp3;

       /* Guard against getting outside buffer */
       if ((bLen-sRange-lag)<0) {
           sRange=bLen-lag;
       }

       ftmp1 = 0.0;
       ftmp2 = 0.0;
       ftmp3 = 0.0;
       for (i=0; i<sRange; i++) {
           ftmp1 += buffer[bLen-sRange+i] *
               buffer[bLen-sRange+i-lag];
           ftmp2 += buffer[bLen-sRange+i-lag] *
                   buffer[bLen-sRange+i-lag];
           ftmp3 += buffer[bLen-sRange+i] *
                   buffer[bLen-sRange+i];
       }

       if (ftmp2 > 0.0) {
           *cc = ftmp1*ftmp1/ftmp2;
           *gc = (float)fabs(ftmp1/ftmp2);
           *pm=(float)fabs(ftmp1)/
               ((float)sqrt(ftmp2)*(float)sqrt(ftmp3));
       }
       else {
           *cc = 0.0;
           *gc = 0.0;
           *pm=0.0;
       }
   }





   /*----------------------------------------------------------------*
    *  Packet loss concealment routine. Conceals a residual signal
    *  and LP parameters. If no packet loss, update state.
    *---------------------------------------------------------------*/

   void doThePLC(
       float *PLCresidual, /* (o) concealed residual */
       float *PLClpc,      /* (o) concealed LP parameters */
       int PLI,        /* (i) packet loss indicator
                                  0 - no PL, 1 = PL */
       float *decresidual, /* (i) decoded residual */
       float *lpc,         /* (i) decoded LPC (only used for no PL) */
       int inlag,          /* (i) pitch lag */
       iLBC_Dec_Inst_t *iLBCdec_inst
                           /* (i/o) decoder instance */
   ){
       int lag=20, randlag;
       float gain, maxcc;
       float use_gain;
       float gain_comp, maxcc_comp, per, max_per=0;
       int i, pick, use_lag;
       float ftmp, randvec[BLOCKL_MAX], pitchfact, energy;

       /* Packet Loss */

       if (PLI == 1) {

           iLBCdec_inst->consPLICount += 1;

           /* if previous frame not lost,
              determine pitch pred. gain */

           if (iLBCdec_inst->prevPLI != 1) {

               /* Search around the previous lag to find the
                  best pitch period */

               lag=inlag-3;
               compCorr(&maxcc, &gain, &max_per,
                   iLBCdec_inst->prevResidual,
                   lag, iLBCdec_inst->blockl, 60);
               for (i=inlag-2;i<=inlag+3;i++) {
                   compCorr(&maxcc_comp, &gain_comp, &per,
                       iLBCdec_inst->prevResidual,
                       i, iLBCdec_inst->blockl, 60);

                   if (maxcc_comp>maxcc) {
                       maxcc=maxcc_comp;





                       gain=gain_comp;
                       lag=i;
                       max_per=per;
                   }
               }

           }

           /* previous frame lost, use recorded lag and periodicity */

           else {
               lag=iLBCdec_inst->prevLag;
               max_per=iLBCdec_inst->per;
           }

           /* downscaling */

           use_gain=1.0;
           if (iLBCdec_inst->consPLICount*iLBCdec_inst->blockl>320)
               use_gain=(float)0.9;
           else if (iLBCdec_inst->consPLICount*
                           iLBCdec_inst->blockl>2*320)
               use_gain=(float)0.7;
           else if (iLBCdec_inst->consPLICount*
                           iLBCdec_inst->blockl>3*320)
               use_gain=(float)0.5;
           else if (iLBCdec_inst->consPLICount*
                           iLBCdec_inst->blockl>4*320)
               use_gain=(float)0.0;

           /* mix noise and pitch repeatition */
           ftmp=(float)sqrt(max_per);
           if (ftmp>(float)0.7)
               pitchfact=(float)1.0;
           else if (ftmp>(float)0.4)
               pitchfact=(ftmp-(float)0.4)/((float)0.7-(float)0.4);
           else
               pitchfact=0.0;


           /* avoid repetition of same pitch cycle */
           use_lag=lag;
           if (lag<80) {
               use_lag=2*lag;
           }

           /* compute concealed residual */






           energy = 0.0;
           for (i=0; i<iLBCdec_inst->blockl; i++) {

               /* noise component */

               iLBCdec_inst->seed=(iLBCdec_inst->seed*69069L+1) &
                   (0x80000000L-1);
               randlag = 50 + ((signed long) iLBCdec_inst->seed)%70;
               pick = i - randlag;

               if (pick < 0) {
                   randvec[i] =
                       iLBCdec_inst->prevResidual[
                                   iLBCdec_inst->blockl+pick];
               } else {
                   randvec[i] =  randvec[pick];
               }

               /* pitch repeatition component */
               pick = i - use_lag;

               if (pick < 0) {
                   PLCresidual[i] =
                       iLBCdec_inst->prevResidual[
                                   iLBCdec_inst->blockl+pick];
               } else {
                   PLCresidual[i] = PLCresidual[pick];
               }

               /* mix random and periodicity component */

               if (i<80)
                   PLCresidual[i] = use_gain*(pitchfact *
                               PLCresidual[i] +
                               ((float)1.0 - pitchfact) * randvec[i]);
               else if (i<160)
                   PLCresidual[i] = (float)0.95*use_gain*(pitchfact *
                               PLCresidual[i] +
                               ((float)1.0 - pitchfact) * randvec[i]);
               else
                   PLCresidual[i] = (float)0.9*use_gain*(pitchfact *
                               PLCresidual[i] +
                               ((float)1.0 - pitchfact) * randvec[i]);

               energy += PLCresidual[i] * PLCresidual[i];
           }

           /* less than 30 dB, use only noise */






           if (sqrt(energy/(float)iLBCdec_inst->blockl) < 30.0) {
               gain=0.0;
               for (i=0; i<iLBCdec_inst->blockl; i++) {
                   PLCresidual[i] = randvec[i];
               }
           }

           /* use old LPC */

           memcpy(PLClpc,iLBCdec_inst->prevLpc,
               (LPC_FILTERORDER+1)*sizeof(float));

       }

       /* no packet loss, copy input */

       else {
           memcpy(PLCresidual, decresidual,
               iLBCdec_inst->blockl*sizeof(float));
           memcpy(PLClpc, lpc, (LPC_FILTERORDER+1)*sizeof(float));
           iLBCdec_inst->consPLICount = 0;
       }

       /* update state */

       if (PLI) {
           iLBCdec_inst->prevLag = lag;
           iLBCdec_inst->per=max_per;
       }

       iLBCdec_inst->prevPLI = PLI;
       memcpy(iLBCdec_inst->prevLpc, PLClpc,
           (LPC_FILTERORDER+1)*sizeof(float));
       memcpy(iLBCdec_inst->prevResidual, PLCresidual,
           iLBCdec_inst->blockl*sizeof(float));
   }

