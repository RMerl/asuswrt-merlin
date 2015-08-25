
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       createCB.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_CREATECB_H
   #define __iLBC_CREATECB_H

   void filteredCBvecs(
       float *cbvectors,   /* (o) Codebook vector for the
                                  higher section */





       float *mem,         /* (i) Buffer to create codebook
                                  vectors from */
       int lMem        /* (i) Length of buffer */
   );

   void searchAugmentedCB(
       int low,        /* (i) Start index for the search */
       int high,           /* (i) End index for the search */
       int stage,          /* (i) Current stage */
       int startIndex,     /* (i) CB index for the first
                                  augmented vector */
       float *target,      /* (i) Target vector for encoding */
       float *buffer,      /* (i) Pointer to the end of the
                                  buffer for augmented codebook
                                  construction */
       float *max_measure, /* (i/o) Currently maximum measure */
       int *best_index,/* (o) Currently the best index */
       float *gain,    /* (o) Currently the best gain */
       float *energy,      /* (o) Energy of augmented
                                  codebook vectors */
       float *invenergy/* (o) Inv energy of aug codebook
                                  vectors */
   );

   void createAugmentedVec(
       int index,          /* (i) Index for the aug vector
                                  to be created */
       float *buffer,      /* (i) Pointer to the end of the
                                  buffer for augmented codebook
                                  construction */
       float *cbVec    /* (o) The construced codebook vector */
   );

   #endif

