
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       anaFilter.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_ANAFILTER_H
   #define __iLBC_ANAFILTER_H

   void anaFilter(





       float *In,  /* (i) Signal to be filtered */
       float *a,   /* (i) LP parameters */
       int len,/* (i) Length of signal */
       float *Out, /* (o) Filtered signal */
       float *mem  /* (i/o) Filter state */
   );

   #endif

