
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       iLBC_decode.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_ILBCDECODE_H
   #define __iLBC_ILBCDECODE_H

   #include "iLBC_define.h"

   short initDecode(                   /* (o) Number of decoded
                                              samples */
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) Decoder instance */
       int mode,                       /* (i) frame size mode */
       int use_enhancer                /* (i) 1 to use enhancer
                                              0 to run without
                                                enhancer */
   );

   void iLBC_decode(
       float *decblock,            /* (o) decoded signal block */
       unsigned char *bytes,           /* (i) encoded signal bits */
       iLBC_Dec_Inst_t *iLBCdec_inst,  /* (i/o) the decoder state
                                                structure */
       int mode                    /* (i) 0: bad packet, PLC,
                                              1: normal */





   );

   #endif

