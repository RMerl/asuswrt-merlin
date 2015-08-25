
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       anaFilter.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <string.h>
   #include "iLBC_define.h"

   /*----------------------------------------------------------------*
    *  LP analysis filter.
    *---------------------------------------------------------------*/

   void anaFilter(
       float *In,  /* (i) Signal to be filtered */
       float *a,   /* (i) LP parameters */
       int len,/* (i) Length of signal */
       float *Out, /* (o) Filtered signal */
       float *mem  /* (i/o) Filter state */
   ){
       int i, j;
       float *po, *pi, *pm, *pa;

       po = Out;

       /* Filter first part using memory from past */

       for (i=0; i<LPC_FILTERORDER; i++) {
           pi = &In[i];
           pm = &mem[LPC_FILTERORDER-1];
           pa = a;
           *po=0.0;





           for (j=0; j<=i; j++) {
               *po+=(*pa++)*(*pi--);
           }
           for (j=i+1; j<LPC_FILTERORDER+1; j++) {

               *po+=(*pa++)*(*pm--);
           }
           po++;
       }

       /* Filter last part where the state is entirely
          in the input vector */

       for (i=LPC_FILTERORDER; i<len; i++) {
           pi = &In[i];
           pa = a;
           *po=0.0;
           for (j=0; j<LPC_FILTERORDER+1; j++) {
               *po+=(*pa++)*(*pi--);
           }
           po++;
       }

       /* Update state vector */

       memcpy(mem, &In[len-LPC_FILTERORDER],
           LPC_FILTERORDER*sizeof(float));
   }

