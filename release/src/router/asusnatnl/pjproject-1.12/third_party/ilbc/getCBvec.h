
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       getCBvec.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_GETCBVEC_H
   #define __iLBC_GETCBVEC_H

   void getCBvec(
       float *cbvec,   /* (o) Constructed codebook vector */
       float *mem,     /* (i) Codebook buffer */
       int index,      /* (i) Codebook index */
       int lMem,       /* (i) Length of codebook buffer */
       int cbveclen/* (i) Codebook vector length */
   );

   #endif

