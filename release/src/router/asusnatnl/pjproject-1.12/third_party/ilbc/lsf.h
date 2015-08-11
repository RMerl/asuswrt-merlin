
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       lsf.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_LSF_H
   #define __iLBC_LSF_H

   void a2lsf(
       float *freq,/* (o) lsf coefficients */
       float *a    /* (i) lpc coefficients */
   );

   void lsf2a(
       float *a_coef,  /* (o) lpc coefficients */
       float *freq     /* (i) lsf coefficients */
   );

   #endif

