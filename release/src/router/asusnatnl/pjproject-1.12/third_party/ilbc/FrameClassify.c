
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       FrameClassify.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include "iLBC_define.h"

   /*---------------------------------------------------------------*
    *  Classification of subframes to localize start state
    *--------------------------------------------------------------*/

   int FrameClassify(      /* index to the max-energy sub-frame */
       iLBC_Enc_Inst_t *iLBCenc_inst,
                           /* (i/o) the encoder state structure */
       float *residual     /* (i) lpc residual signal */
   ) {
       float max_ssqEn, fssqEn[NSUB_MAX], bssqEn[NSUB_MAX], *pp;
       int n, l, max_ssqEn_n;
       const float ssqEn_win[NSUB_MAX-1]={(float)0.8,(float)0.9,
           (float)1.0,(float)0.9,(float)0.8};
       const float sampEn_win[5]={(float)1.0/(float)6.0,
           (float)2.0/(float)6.0, (float)3.0/(float)6.0,
           (float)4.0/(float)6.0, (float)5.0/(float)6.0};

       /* init the front and back energies to zero */

       memset(fssqEn, 0, NSUB_MAX*sizeof(float));
       memset(bssqEn, 0, NSUB_MAX*sizeof(float));

       /* Calculate front of first seqence */

       n=0;
       pp=residual;
       for (l=0; l<5; l++) {
           fssqEn[n] += sampEn_win[l] * (*pp) * (*pp);
           pp++;
       }
       for (l=5; l<SUBL; l++) {





           fssqEn[n] += (*pp) * (*pp);
           pp++;
       }

       /* Calculate front and back of all middle sequences */

       for (n=1; n<iLBCenc_inst->nsub-1; n++) {
           pp=residual+n*SUBL;
           for (l=0; l<5; l++) {
               fssqEn[n] += sampEn_win[l] * (*pp) * (*pp);
               bssqEn[n] += (*pp) * (*pp);
               pp++;
           }
           for (l=5; l<SUBL-5; l++) {
               fssqEn[n] += (*pp) * (*pp);
               bssqEn[n] += (*pp) * (*pp);
               pp++;
           }
           for (l=SUBL-5; l<SUBL; l++) {
               fssqEn[n] += (*pp) * (*pp);
               bssqEn[n] += sampEn_win[SUBL-l-1] * (*pp) * (*pp);
               pp++;
           }
       }

       /* Calculate back of last seqence */

       n=iLBCenc_inst->nsub-1;
       pp=residual+n*SUBL;
       for (l=0; l<SUBL-5; l++) {
           bssqEn[n] += (*pp) * (*pp);
           pp++;
       }
       for (l=SUBL-5; l<SUBL; l++) {
           bssqEn[n] += sampEn_win[SUBL-l-1] * (*pp) * (*pp);
           pp++;
       }

       /* find the index to the weighted 80 sample with
          most energy */

       if (iLBCenc_inst->mode==20) l=1;
       else                        l=0;

       max_ssqEn=(fssqEn[0]+bssqEn[1])*ssqEn_win[l];
       max_ssqEn_n=1;
       for (n=2; n<iLBCenc_inst->nsub; n++) {






           l++;
           if ((fssqEn[n-1]+bssqEn[n])*ssqEn_win[l] > max_ssqEn) {
               max_ssqEn=(fssqEn[n-1]+bssqEn[n]) *
                               ssqEn_win[l];
               max_ssqEn_n=n;
           }
       }

       return max_ssqEn_n;
   }

