
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       FrameClassify.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_FRAMECLASSIFY_H
   #define __iLBC_FRAMECLASSIFY_H

   int FrameClassify(      /* index to the max-energy sub-frame */
       iLBC_Enc_Inst_t *iLBCenc_inst,
                           /* (i/o) the encoder state structure */
       float *residual     /* (i) lpc residual signal */
   );





   #endif

