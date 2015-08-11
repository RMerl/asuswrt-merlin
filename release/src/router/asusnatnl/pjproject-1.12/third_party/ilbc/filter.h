
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       filter.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/






   #ifndef __iLBC_FILTER_H
   #define __iLBC_FILTER_H

   void AllPoleFilter(
       float *InOut,   /* (i/o) on entrance InOut[-orderCoef] to
                              InOut[-1] contain the state of the
                              filter (delayed samples). InOut[0] to
                              InOut[lengthInOut-1] contain the filter
                              input, on en exit InOut[-orderCoef] to
                              InOut[-1] is unchanged and InOut[0] to
                              InOut[lengthInOut-1] contain filtered
                              samples */
       float *Coef,/* (i) filter coefficients, Coef[0] is assumed
                              to be 1.0 */
       int lengthInOut,/* (i) number of input/output samples */
       int orderCoef   /* (i) number of filter coefficients */
   );

   void AllZeroFilter(
       float *In,      /* (i) In[0] to In[lengthInOut-1] contain
                              filter input samples */
       float *Coef,/* (i) filter coefficients (Coef[0] is assumed
                              to be 1.0) */
       int lengthInOut,/* (i) number of input/output samples */
       int orderCoef,  /* (i) number of filter coefficients */
       float *Out      /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                              contain the filter state, on exit Out[0]
                              to Out[lengthInOut-1] contain filtered
                              samples */
   );

   void ZeroPoleFilter(
       float *In,      /* (i) In[0] to In[lengthInOut-1] contain filter
                              input samples In[-orderCoef] to In[-1]
                              contain state of all-zero section */
       float *ZeroCoef,/* (i) filter coefficients for all-zero
                              section (ZeroCoef[0] is assumed to
                              be 1.0) */
       float *PoleCoef,/* (i) filter coefficients for all-pole section
                              (ZeroCoef[0] is assumed to be 1.0) */
       int lengthInOut,/* (i) number of input/output samples */
       int orderCoef,  /* (i) number of filter coefficients */
       float *Out      /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                              contain state of all-pole section. On
                              exit Out[0] to Out[lengthInOut-1]
                              contain filtered samples */
   );






   void DownSample (
       float  *In,     /* (i) input samples */
       float  *Coef,   /* (i) filter coefficients */
       int lengthIn,   /* (i) number of input samples */
       float  *state,  /* (i) filter state */
       float  *Out     /* (o) downsampled output */
   );

   #endif

