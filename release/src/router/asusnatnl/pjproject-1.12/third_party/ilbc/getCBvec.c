
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       getCBvec.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include "iLBC_define.h"
   #include "constants.h"
   #include <string.h>

   /*----------------------------------------------------------------*
    *  Construct codebook vector for given index.
    *---------------------------------------------------------------*/

   void getCBvec(





       float *cbvec,   /* (o) Constructed codebook vector */
       float *mem,     /* (i) Codebook buffer */
       int index,      /* (i) Codebook index */
       int lMem,       /* (i) Length of codebook buffer */
       int cbveclen/* (i) Codebook vector length */
   ){
       int j, k, n, memInd, sFilt;
       float tmpbuf[CB_MEML];
       int base_size;
       int ilow, ihigh;
       float alfa, alfa1;

       /* Determine size of codebook sections */

       base_size=lMem-cbveclen+1;

       if (cbveclen==SUBL) {
           base_size+=cbveclen/2;
       }

       /* No filter -> First codebook section */

       if (index<lMem-cbveclen+1) {

           /* first non-interpolated vectors */

           k=index+cbveclen;
           /* get vector */
           memcpy(cbvec, mem+lMem-k, cbveclen*sizeof(float));

       } else if (index < base_size) {

           k=2*(index-(lMem-cbveclen+1))+cbveclen;

           ihigh=k/2;
           ilow=ihigh-5;

           /* Copy first noninterpolated part */

           memcpy(cbvec, mem+lMem-k/2, ilow*sizeof(float));

           /* interpolation */

           alfa1=(float)0.2;
           alfa=0.0;
           for (j=ilow; j<ihigh; j++) {
               cbvec[j]=((float)1.0-alfa)*mem[lMem-k/2+j]+
                   alfa*mem[lMem-k+j];





               alfa+=alfa1;
           }

           /* Copy second noninterpolated part */

           memcpy(cbvec+ihigh, mem+lMem-k+ihigh,
               (cbveclen-ihigh)*sizeof(float));

       }

       /* Higher codebook section based on filtering */

       else {

           /* first non-interpolated vectors */

           if (index-base_size<lMem-cbveclen+1) {
               float tempbuff2[CB_MEML+CB_FILTERLEN+1];
               float *pos;
               float *pp, *pp1;

               memset(tempbuff2, 0,
                   CB_HALFFILTERLEN*sizeof(float));
               memcpy(&tempbuff2[CB_HALFFILTERLEN], mem,
                   lMem*sizeof(float));
               memset(&tempbuff2[lMem+CB_HALFFILTERLEN], 0,
                   (CB_HALFFILTERLEN+1)*sizeof(float));

               k=index-base_size+cbveclen;
               sFilt=lMem-k;
               memInd=sFilt+1-CB_HALFFILTERLEN;

               /* do filtering */
               pos=cbvec;
               memset(pos, 0, cbveclen*sizeof(float));
               for (n=0; n<cbveclen; n++) {
                   pp=&tempbuff2[memInd+n+CB_HALFFILTERLEN];
                   pp1=&cbfiltersTbl[CB_FILTERLEN-1];
                   for (j=0; j<CB_FILTERLEN; j++) {
                       (*pos)+=(*pp++)*(*pp1--);
                   }
                   pos++;
               }
           }

           /* interpolated vectors */

           else {





               float tempbuff2[CB_MEML+CB_FILTERLEN+1];

               float *pos;
               float *pp, *pp1;
               int i;

               memset(tempbuff2, 0,
                   CB_HALFFILTERLEN*sizeof(float));
               memcpy(&tempbuff2[CB_HALFFILTERLEN], mem,
                   lMem*sizeof(float));
               memset(&tempbuff2[lMem+CB_HALFFILTERLEN], 0,
                   (CB_HALFFILTERLEN+1)*sizeof(float));

               k=2*(index-base_size-
                   (lMem-cbveclen+1))+cbveclen;
               sFilt=lMem-k;
               memInd=sFilt+1-CB_HALFFILTERLEN;

               /* do filtering */
               pos=&tmpbuf[sFilt];
               memset(pos, 0, k*sizeof(float));
               for (i=0; i<k; i++) {
                   pp=&tempbuff2[memInd+i+CB_HALFFILTERLEN];
                   pp1=&cbfiltersTbl[CB_FILTERLEN-1];
                   for (j=0; j<CB_FILTERLEN; j++) {
                       (*pos)+=(*pp++)*(*pp1--);
                   }
                   pos++;
               }

               ihigh=k/2;
               ilow=ihigh-5;

               /* Copy first noninterpolated part */

               memcpy(cbvec, tmpbuf+lMem-k/2,
                   ilow*sizeof(float));

               /* interpolation */

               alfa1=(float)0.2;
               alfa=0.0;
               for (j=ilow; j<ihigh; j++) {
                   cbvec[j]=((float)1.0-alfa)*
                       tmpbuf[lMem-k/2+j]+alfa*tmpbuf[lMem-k+j];
                   alfa+=alfa1;
               }






               /* Copy second noninterpolated part */

               memcpy(cbvec+ihigh, tmpbuf+lMem-k+ihigh,
                   (cbveclen-ihigh)*sizeof(float));
           }
       }
   }

