
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iLBC_encode.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_ILBCENCODE_H
   #define __iLBC_ILBCENCODE_H

   #include "iLBC_define.h"

   short initEncode(                   /* (o) Number of bytes
                                              encoded */
       iLBC_Enc_Inst_t *iLBCenc_inst,  /* (i/o) Encoder instance */
       int mode                    /* (i) frame size mode */
   );

   void iLBC_encode(

       unsigned char *bytes,           /* (o) encoded data bits iLBC */
       float *block,                   /* (o) speech vector to
                                              encode */
       iLBC_Enc_Inst_t *iLBCenc_inst   /* (i/o) the general encoder
                                              state */
   );

   #endif






