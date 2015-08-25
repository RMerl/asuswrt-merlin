
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code





       hpInput.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include "constants.h"

   /*----------------------------------------------------------------*
    *  Input high-pass filter
    *---------------------------------------------------------------*/

   void hpInput(
       float *In,  /* (i) vector to filter */
       int len,    /* (i) length of vector to filter */
       float *Out, /* (o) the resulting filtered vector */
       float *mem  /* (i/o) the filter state */
   ){
       int i;
       float *pi, *po;

       /* all-zero section*/

       pi = &In[0];
       po = &Out[0];
       for (i=0; i<len; i++) {
           *po = hpi_zero_coefsTbl[0] * (*pi);
           *po += hpi_zero_coefsTbl[1] * mem[0];
           *po += hpi_zero_coefsTbl[2] * mem[1];

           mem[1] = mem[0];
           mem[0] = *pi;
           po++;
           pi++;

       }

       /* all-pole section*/

       po = &Out[0];
       for (i=0; i<len; i++) {
           *po -= hpi_pole_coefsTbl[1] * mem[2];
           *po -= hpi_pole_coefsTbl[2] * mem[3];

           mem[3] = mem[2];
           mem[2] = *po;
           po++;





       }
   }

