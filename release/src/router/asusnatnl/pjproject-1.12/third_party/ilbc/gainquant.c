
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code






       gainquant.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <string.h>
   #include <math.h>
   #include "constants.h"
   #include "filter.h"

   /*----------------------------------------------------------------*
    *  quantizer for the gain in the gain-shape coding of residual
    *---------------------------------------------------------------*/

   float gainquant(/* (o) quantized gain value */
       float in,       /* (i) gain value */
       float maxIn,/* (i) maximum of gain value */
       int cblen,      /* (i) number of quantization indices */
       int *index      /* (o) quantization index */
   ){
       int i, tindex;
       float minmeasure,measure, *cb, scale;

       /* ensure a lower bound on the scaling factor */

       scale=maxIn;

       if (scale<0.1) {
           scale=(float)0.1;
       }

       /* select the quantization table */

       if (cblen == 8) {
           cb = gain_sq3Tbl;
       } else if (cblen == 16) {
           cb = gain_sq4Tbl;
       } else  {
           cb = gain_sq5Tbl;
       }

       /* select the best index in the quantization table */

       minmeasure=10000000.0;
       tindex=0;
       for (i=0; i<cblen; i++) {





           measure=(in-scale*cb[i])*(in-scale*cb[i]);

           if (measure<minmeasure) {
               tindex=i;
               minmeasure=measure;
           }
       }
       *index=tindex;

       /* return the quantized value */

       return scale*cb[tindex];
   }

   /*----------------------------------------------------------------*
    *  decoder for quantized gains in the gain-shape coding of
    *  residual
    *---------------------------------------------------------------*/

   float gaindequant(  /* (o) quantized gain value */
       int index,      /* (i) quantization index */
       float maxIn,/* (i) maximum of unquantized gain */
       int cblen       /* (i) number of quantization indices */
   ){
       float scale;

       /* obtain correct scale factor */

       scale=(float)fabs(maxIn);

       if (scale<0.1) {
           scale=(float)0.1;
       }

       /* select the quantization table and return the decoded value */

       if (cblen==8) {
           return scale*gain_sq3Tbl[index];
       } else if (cblen==16) {
           return scale*gain_sq4Tbl[index];
       }
       else if (cblen==32) {
           return scale*gain_sq5Tbl[index];
       }

       return 0.0;
   }






