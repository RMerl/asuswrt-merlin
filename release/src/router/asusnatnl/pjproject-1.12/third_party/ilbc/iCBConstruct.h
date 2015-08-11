
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iCBConstruct.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.






   ******************************************************************/

   #ifndef __iLBC_ICBCONSTRUCT_H
   #define __iLBC_ICBCONSTRUCT_H

   void index_conv_enc(
       int *index          /* (i/o) Codebook indexes */
   );

   void index_conv_dec(
       int *index          /* (i/o) Codebook indexes */
   );

   void iCBConstruct(
       float *decvector,   /* (o) Decoded vector */
       int *index,         /* (i) Codebook indices */
       int *gain_index,/* (i) Gain quantization indices */
       float *mem,         /* (i) Buffer for codevector construction */
       int lMem,           /* (i) Length of buffer */
       int veclen,         /* (i) Length of vector */
       int nStages         /* (i) Number of codebook stages */
   );

   #endif

