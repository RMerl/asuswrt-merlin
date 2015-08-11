
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       syntFilter.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_SYNTFILTER_H
   #define __iLBC_SYNTFILTER_H

   void syntFilter(
       float *Out,     /* (i/o) Signal to be filtered */
       float *a,       /* (i) LP parameters */
       int len,    /* (i) Length of signal */
       float *mem      /* (i/o) Filter state */
   );

   #endif

