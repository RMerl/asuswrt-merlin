
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       enhancer.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <math.h>
   #include <string.h>
   #include "iLBC_define.h"
   #include "constants.h"
   #include "filter.h"

   /*----------------------------------------------------------------*
    * Find index in array such that the array element with said
    * index is the element of said array closest to "value"
    * according to the squared-error criterion
    *---------------------------------------------------------------*/

   void NearestNeighbor(





       int   *index,   /* (o) index of array element closest
                              to value */
       float *array,   /* (i) data array */
       float value,/* (i) value */
       int arlength/* (i) dimension of data array */
   ){
       int i;
       float bestcrit,crit;

       crit=array[0]-value;
       bestcrit=crit*crit;
       *index=0;
       for (i=1; i<arlength; i++) {
           crit=array[i]-value;
           crit=crit*crit;

           if (crit<bestcrit) {
               bestcrit=crit;
               *index=i;
           }
       }
   }

   /*----------------------------------------------------------------*
    * compute cross correlation between sequences
    *---------------------------------------------------------------*/

   void mycorr1(
       float* corr,    /* (o) correlation of seq1 and seq2 */
       float* seq1,    /* (i) first sequence */
       int dim1,           /* (i) dimension first seq1 */
       const float *seq2,  /* (i) second sequence */
       int dim2        /* (i) dimension seq2 */
   ){
       int i,j;

       for (i=0; i<=dim1-dim2; i++) {
           corr[i]=0.0;
           for (j=0; j<dim2; j++) {
               corr[i] += seq1[i+j] * seq2[j];
           }
       }
   }

   /*----------------------------------------------------------------*
    * upsample finite array assuming zeros outside bounds
    *---------------------------------------------------------------*/






   void enh_upsample(
       float* useq1,   /* (o) upsampled output sequence */
       float* seq1,/* (i) unupsampled sequence */
       int dim1,       /* (i) dimension seq1 */
       int hfl         /* (i) polyphase filter length=2*hfl+1 */
   ){
       float *pu,*ps;
       int i,j,k,q,filterlength,hfl2;
       const float *polyp[ENH_UPS0]; /* pointers to
                                        polyphase columns */
       const float *pp;

       /* define pointers for filter */

       filterlength=2*hfl+1;

       if ( filterlength > dim1 ) {
           hfl2=(int) (dim1/2);
           for (j=0; j<ENH_UPS0; j++) {
               polyp[j]=polyphaserTbl+j*filterlength+hfl-hfl2;
           }
           hfl=hfl2;
           filterlength=2*hfl+1;
       }
       else {
           for (j=0; j<ENH_UPS0; j++) {
               polyp[j]=polyphaserTbl+j*filterlength;
           }
       }

       /* filtering: filter overhangs left side of sequence */

       pu=useq1;
       for (i=hfl; i<filterlength; i++) {
           for (j=0; j<ENH_UPS0; j++) {
               *pu=0.0;
               pp = polyp[j];
               ps = seq1+i;
               for (k=0; k<=i; k++) {
                   *pu += *ps-- * *pp++;
               }
               pu++;
           }
       }

       /* filtering: simple convolution=inner products */

       for (i=filterlength; i<dim1; i++) {





           for (j=0;j<ENH_UPS0; j++){
               *pu=0.0;
               pp = polyp[j];
               ps = seq1+i;
               for (k=0; k<filterlength; k++) {
                   *pu += *ps-- * *pp++;
               }
               pu++;
           }
       }

       /* filtering: filter overhangs right side of sequence */

       for (q=1; q<=hfl; q++) {
           for (j=0; j<ENH_UPS0; j++) {
               *pu=0.0;
               pp = polyp[j]+q;
               ps = seq1+dim1-1;
               for (k=0; k<filterlength-q; k++) {
                   *pu += *ps-- * *pp++;
               }
               pu++;
           }
       }
   }


   /*----------------------------------------------------------------*
    * find segment starting near idata+estSegPos that has highest
    * correlation with idata+centerStartPos through
    * idata+centerStartPos+ENH_BLOCKL-1 segment is found at a
    * resolution of ENH_UPSO times the original of the original
    * sampling rate
    *---------------------------------------------------------------*/

   void refiner(
       float *seg,         /* (o) segment array */
       float *updStartPos, /* (o) updated start point */
       float* idata,       /* (i) original data buffer */
       int idatal,         /* (i) dimension of idata */
       int centerStartPos, /* (i) beginning center segment */
       float estSegPos,/* (i) estimated beginning other segment */
       float period    /* (i) estimated pitch period */
   ){
       int estSegPosRounded,searchSegStartPos,searchSegEndPos,corrdim;
       int tloc,tloc2,i,st,en,fraction;
       float vect[ENH_VECTL],corrVec[ENH_CORRDIM],maxv;
       float corrVecUps[ENH_CORRDIM*ENH_UPS0];

       (void)period;



       /* defining array bounds */

       estSegPosRounded=(int)(estSegPos - 0.5);

       searchSegStartPos=estSegPosRounded-ENH_SLOP;

       if (searchSegStartPos<0) {
           searchSegStartPos=0;
       }
       searchSegEndPos=estSegPosRounded+ENH_SLOP;

       if (searchSegEndPos+ENH_BLOCKL >= idatal) {
           searchSegEndPos=idatal-ENH_BLOCKL-1;
       }
       corrdim=searchSegEndPos-searchSegStartPos+1;

       /* compute upsampled correlation (corr33) and find
          location of max */

       mycorr1(corrVec,idata+searchSegStartPos,
           corrdim+ENH_BLOCKL-1,idata+centerStartPos,ENH_BLOCKL);
       enh_upsample(corrVecUps,corrVec,corrdim,ENH_FL0);
       tloc=0; maxv=corrVecUps[0];
       for (i=1; i<ENH_UPS0*corrdim; i++) {

           if (corrVecUps[i]>maxv) {
               tloc=i;
               maxv=corrVecUps[i];
           }
       }

       /* make vector can be upsampled without ever running outside
          bounds */

       *updStartPos= (float)searchSegStartPos +
           (float)tloc/(float)ENH_UPS0+(float)1.0;
       tloc2=(int)(tloc/ENH_UPS0);

       if (tloc>tloc2*ENH_UPS0) {
           tloc2++;
       }
       st=searchSegStartPos+tloc2-ENH_FL0;

       if (st<0) {
           memset(vect,0,-st*sizeof(float));
           memcpy(&vect[-st],idata, (ENH_VECTL+st)*sizeof(float));
       }
       else {





           en=st+ENH_VECTL;

           if (en>idatal) {
               memcpy(vect, &idata[st],
                   (ENH_VECTL-(en-idatal))*sizeof(float));
               memset(&vect[ENH_VECTL-(en-idatal)], 0,
                   (en-idatal)*sizeof(float));
           }
           else {
               memcpy(vect, &idata[st], ENH_VECTL*sizeof(float));
           }
       }
       fraction=tloc2*ENH_UPS0-tloc;

       /* compute the segment (this is actually a convolution) */

       mycorr1(seg,vect,ENH_VECTL,polyphaserTbl+(2*ENH_FL0+1)*fraction,
           2*ENH_FL0+1);
   }

   /*----------------------------------------------------------------*
    * find the smoothed output data
    *---------------------------------------------------------------*/

   void smath(
       float *odata,   /* (o) smoothed output */
       float *sseq,/* (i) said second sequence of waveforms */
       int hl,         /* (i) 2*hl+1 is sseq dimension */
       float alpha0/* (i) max smoothing energy fraction */
   ){
       int i,k;
       float w00,w10,w11,A,B,C,*psseq,err,errs;
       float surround[BLOCKL_MAX]; /* shape contributed by other than
                                      current */
       float wt[2*ENH_HL+1];       /* waveform weighting to get
                                      surround shape */
       float denom;

       /* create shape of contribution from all waveforms except the
          current one */

       for (i=1; i<=2*hl+1; i++) {
           wt[i-1] = (float)0.5*(1 - (float)cos(2*PI*i/(2*hl+2)));
       }
       wt[hl]=0.0; /* for clarity, not used */
       for (i=0; i<ENH_BLOCKL; i++) {
           surround[i]=sseq[i]*wt[0];
       }





       for (k=1; k<hl; k++) {
           psseq=sseq+k*ENH_BLOCKL;
           for(i=0;i<ENH_BLOCKL; i++) {
               surround[i]+=psseq[i]*wt[k];
           }
       }
       for (k=hl+1; k<=2*hl; k++) {
           psseq=sseq+k*ENH_BLOCKL;
           for(i=0;i<ENH_BLOCKL; i++) {
               surround[i]+=psseq[i]*wt[k];
           }
       }

       /* compute some inner products */

       w00 = w10 = w11 = 0.0;
       psseq=sseq+hl*ENH_BLOCKL; /* current block  */
       for (i=0; i<ENH_BLOCKL;i++) {
           w00+=psseq[i]*psseq[i];
           w11+=surround[i]*surround[i];
           w10+=surround[i]*psseq[i];
       }

       if (fabs(w11) < 1.0) {
           w11=1.0;
       }
       C = (float)sqrt( w00/w11);

       /* first try enhancement without power-constraint */

       errs=0.0;
       psseq=sseq+hl*ENH_BLOCKL;
       for (i=0; i<ENH_BLOCKL; i++) {
           odata[i]=C*surround[i];
           err=psseq[i]-odata[i];
           errs+=err*err;
       }

       /* if constraint violated by first try, add constraint */

       if (errs > alpha0 * w00) {
           if ( w00 < 1) {
               w00=1;
           }
           denom = (w11*w00-w10*w10)/(w00*w00);

           if (denom > 0.0001) { /* eliminates numerical problems
                                    for if smooth */





               A = (float)sqrt( (alpha0- alpha0*alpha0/4)/denom);
               B = -alpha0/2 - A * w10/w00;
               B = B+1;
           }
           else { /* essentially no difference between cycles;
                     smoothing not needed */
               A= 0.0;
               B= 1.0;
           }

           /* create smoothed sequence */

           psseq=sseq+hl*ENH_BLOCKL;
           for (i=0; i<ENH_BLOCKL; i++) {
               odata[i]=A*surround[i]+B*psseq[i];
           }
       }
   }

   /*----------------------------------------------------------------*
    * get the pitch-synchronous sample sequence
    *---------------------------------------------------------------*/

   void getsseq(
       float *sseq,    /* (o) the pitch-synchronous sequence */
       float *idata,       /* (i) original data */
       int idatal,         /* (i) dimension of data */
       int centerStartPos, /* (i) where current block starts */
       float *period,      /* (i) rough-pitch-period array */
       float *plocs,       /* (i) where periods of period array
                                  are taken */
       int periodl,    /* (i) dimension period array */
       int hl              /* (i) 2*hl+1 is the number of sequences */
   ){
       int i,centerEndPos,q;
       float blockStartPos[2*ENH_HL+1];
       int lagBlock[2*ENH_HL+1];
       float plocs2[ENH_PLOCSL];
       float *psseq;

       centerEndPos=centerStartPos+ENH_BLOCKL-1;

       /* present */

       NearestNeighbor(lagBlock+hl,plocs,
           (float)0.5*(centerStartPos+centerEndPos),periodl);

       blockStartPos[hl]=(float)centerStartPos;





       psseq=sseq+ENH_BLOCKL*hl;
       memcpy(psseq, idata+centerStartPos, ENH_BLOCKL*sizeof(float));

       /* past */

       for (q=hl-1; q>=0; q--) {
           blockStartPos[q]=blockStartPos[q+1]-period[lagBlock[q+1]];
           NearestNeighbor(lagBlock+q,plocs,
               blockStartPos[q]+
               ENH_BLOCKL_HALF-period[lagBlock[q+1]], periodl);


           if (blockStartPos[q]-ENH_OVERHANG>=0) {
               refiner(sseq+q*ENH_BLOCKL, blockStartPos+q, idata,
                   idatal, centerStartPos, blockStartPos[q],
                   period[lagBlock[q+1]]);
           } else {
               psseq=sseq+q*ENH_BLOCKL;
               memset(psseq, 0, ENH_BLOCKL*sizeof(float));
           }
       }

       /* future */

       for (i=0; i<periodl; i++) {
           plocs2[i]=plocs[i]-period[i];
       }
       for (q=hl+1; q<=2*hl; q++) {
           NearestNeighbor(lagBlock+q,plocs2,
               blockStartPos[q-1]+ENH_BLOCKL_HALF,periodl);

           blockStartPos[q]=blockStartPos[q-1]+period[lagBlock[q]];
           if (blockStartPos[q]+ENH_BLOCKL+ENH_OVERHANG<idatal) {
               refiner(sseq+ENH_BLOCKL*q, blockStartPos+q, idata,
                   idatal, centerStartPos, blockStartPos[q],
                   period[lagBlock[q]]);
           }
           else {
               psseq=sseq+q*ENH_BLOCKL;
               memset(psseq, 0, ENH_BLOCKL*sizeof(float));
           }
       }
   }

   /*----------------------------------------------------------------*
    * perform enhancement on idata+centerStartPos through
    * idata+centerStartPos+ENH_BLOCKL-1
    *---------------------------------------------------------------*/





   void enhancer(
       float *odata,       /* (o) smoothed block, dimension blockl */
       float *idata,       /* (i) data buffer used for enhancing */
       int idatal,         /* (i) dimension idata */
       int centerStartPos, /* (i) first sample current block
                                  within idata */
       float alpha0,       /* (i) max correction-energy-fraction
                                 (in [0,1]) */
       float *period,      /* (i) pitch period array */
       float *plocs,       /* (i) locations where period array
                                  values valid */
       int periodl         /* (i) dimension of period and plocs */
   ){
       float sseq[(2*ENH_HL+1)*ENH_BLOCKL];

       /* get said second sequence of segments */

       getsseq(sseq,idata,idatal,centerStartPos,period,
           plocs,periodl,ENH_HL);

       /* compute the smoothed output from said second sequence */

       smath(odata,sseq,ENH_HL,alpha0);

   }

   /*----------------------------------------------------------------*
    * cross correlation
    *---------------------------------------------------------------*/

   float xCorrCoef(
       float *target,      /* (i) first array */
       float *regressor,   /* (i) second array */
       int subl        /* (i) dimension arrays */
   ){
       int i;
       float ftmp1, ftmp2;

       ftmp1 = 0.0;
       ftmp2 = 0.0;
       for (i=0; i<subl; i++) {
           ftmp1 += target[i]*regressor[i];
           ftmp2 += regressor[i]*regressor[i];
       }

       if (ftmp1 > 0.0) {
           return (float)(ftmp1*ftmp1/ftmp2);
       }





       else {
           return (float)0.0;
       }
   }

   /*----------------------------------------------------------------*
    * interface for enhancer
    *---------------------------------------------------------------*/

   int enhancerInterface(
       float *out,                     /* (o) enhanced signal */
       float *in,                      /* (i) unenhanced signal */
       iLBC_Dec_Inst_t *iLBCdec_inst   /* (i) buffers etc */
   ){
       float *enh_buf, *enh_period;
       int iblock, isample;
       int lag=0, ilag, i, ioffset;
       float cc, maxcc;
       float ftmp1, ftmp2;
       float *inPtr, *enh_bufPtr1, *enh_bufPtr2;
       float plc_pred[ENH_BLOCKL];

       float lpState[6], downsampled[(ENH_NBLOCKS*ENH_BLOCKL+120)/2];
       int inLen=ENH_NBLOCKS*ENH_BLOCKL+120;
       int start, plc_blockl, inlag;

       enh_buf=iLBCdec_inst->enh_buf;
       enh_period=iLBCdec_inst->enh_period;

       memmove(enh_buf, &enh_buf[iLBCdec_inst->blockl],
           (ENH_BUFL-iLBCdec_inst->blockl)*sizeof(float));

       memcpy(&enh_buf[ENH_BUFL-iLBCdec_inst->blockl], in,
           iLBCdec_inst->blockl*sizeof(float));

       if (iLBCdec_inst->mode==30)
           plc_blockl=ENH_BLOCKL;
       else
           plc_blockl=40;

       /* when 20 ms frame, move processing one block */
       ioffset=0;
       if (iLBCdec_inst->mode==20) ioffset=1;

       i=3-ioffset;
       memmove(enh_period, &enh_period[i],
           (ENH_NBLOCKS_TOT-i)*sizeof(float));






       /* Set state information to the 6 samples right before
          the samples to be downsampled. */

       memcpy(lpState,
           enh_buf+(ENH_NBLOCKS_EXTRA+ioffset)*ENH_BLOCKL-126,
           6*sizeof(float));

       /* Down sample a factor 2 to save computations */

       DownSample(enh_buf+(ENH_NBLOCKS_EXTRA+ioffset)*ENH_BLOCKL-120,
                   lpFilt_coefsTbl, inLen-ioffset*ENH_BLOCKL,
                   lpState, downsampled);

       /* Estimate the pitch in the down sampled domain. */
       for (iblock = 0; iblock<ENH_NBLOCKS-ioffset; iblock++) {

           lag = 10;
           maxcc = xCorrCoef(downsampled+60+iblock*
               ENH_BLOCKL_HALF, downsampled+60+iblock*
               ENH_BLOCKL_HALF-lag, ENH_BLOCKL_HALF);
           for (ilag=11; ilag<60; ilag++) {
               cc = xCorrCoef(downsampled+60+iblock*
                   ENH_BLOCKL_HALF, downsampled+60+iblock*
                   ENH_BLOCKL_HALF-ilag, ENH_BLOCKL_HALF);

               if (cc > maxcc) {
                   maxcc = cc;
                   lag = ilag;
               }
           }

           /* Store the estimated lag in the non-downsampled domain */
           enh_period[iblock+ENH_NBLOCKS_EXTRA+ioffset] = (float)lag*2;


       }


       /* PLC was performed on the previous packet */
       if (iLBCdec_inst->prev_enh_pl==1) {

           inlag=(int)enh_period[ENH_NBLOCKS_EXTRA+ioffset];

           lag = inlag-1;
           maxcc = xCorrCoef(in, in+lag, plc_blockl);
           for (ilag=inlag; ilag<=inlag+1; ilag++) {
               cc = xCorrCoef(in, in+ilag, plc_blockl);






               if (cc > maxcc) {
                   maxcc = cc;
                   lag = ilag;
               }
           }

           enh_period[ENH_NBLOCKS_EXTRA+ioffset-1]=(float)lag;

           /* compute new concealed residual for the old lookahead,
              mix the forward PLC with a backward PLC from
              the new frame */

           inPtr=&in[lag-1];

           enh_bufPtr1=&plc_pred[plc_blockl-1];

           if (lag>plc_blockl) {
               start=plc_blockl;
           } else {
               start=lag;
           }

           for (isample = start; isample>0; isample--) {
               *enh_bufPtr1-- = *inPtr--;
           }

           enh_bufPtr2=&enh_buf[ENH_BUFL-1-iLBCdec_inst->blockl];
           for (isample = (plc_blockl-1-lag); isample>=0; isample--) {
               *enh_bufPtr1-- = *enh_bufPtr2--;
           }

           /* limit energy change */
           ftmp2=0.0;
           ftmp1=0.0;
           for (i=0;i<plc_blockl;i++) {
               ftmp2+=enh_buf[ENH_BUFL-1-iLBCdec_inst->blockl-i]*
                   enh_buf[ENH_BUFL-1-iLBCdec_inst->blockl-i];
               ftmp1+=plc_pred[i]*plc_pred[i];
           }
           ftmp1=(float)sqrt(ftmp1/(float)plc_blockl);
           ftmp2=(float)sqrt(ftmp2/(float)plc_blockl);
           if (ftmp1>(float)2.0*ftmp2 && ftmp1>0.0) {
               for (i=0;i<plc_blockl-10;i++) {
                   plc_pred[i]*=(float)2.0*ftmp2/ftmp1;
               }
               for (i=plc_blockl-10;i<plc_blockl;i++) {
                   plc_pred[i]*=(float)(i-plc_blockl+10)*
                       ((float)1.0-(float)2.0*ftmp2/ftmp1)/(float)(10)+





                       (float)2.0*ftmp2/ftmp1;
               }
           }

           enh_bufPtr1=&enh_buf[ENH_BUFL-1-iLBCdec_inst->blockl];
           for (i=0; i<plc_blockl; i++) {
               ftmp1 = (float) (i+1) / (float) (plc_blockl+1);
               *enh_bufPtr1 *= ftmp1;
               *enh_bufPtr1 += ((float)1.0-ftmp1)*
                                   plc_pred[plc_blockl-1-i];
               enh_bufPtr1--;
           }
       }

       if (iLBCdec_inst->mode==20) {
           /* Enhancer with 40 samples delay */
           for (iblock = 0; iblock<2; iblock++) {
               enhancer(out+iblock*ENH_BLOCKL, enh_buf,
                   ENH_BUFL, (5+iblock)*ENH_BLOCKL+40,
                   ENH_ALPHA0, enh_period, enh_plocsTbl,
                       ENH_NBLOCKS_TOT);
           }
       } else if (iLBCdec_inst->mode==30) {
           /* Enhancer with 80 samples delay */
           for (iblock = 0; iblock<3; iblock++) {
               enhancer(out+iblock*ENH_BLOCKL, enh_buf,
                   ENH_BUFL, (4+iblock)*ENH_BLOCKL,
                   ENH_ALPHA0, enh_period, enh_plocsTbl,
                       ENH_NBLOCKS_TOT);
           }
       }

       return (lag*2);
   }

