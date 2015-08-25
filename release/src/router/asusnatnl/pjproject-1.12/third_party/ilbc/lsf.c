
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       lsf.c

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #include <string.h>





   #include <math.h>

   #include "iLBC_define.h"

   /*----------------------------------------------------------------*
    *  conversion from lpc coefficients to lsf coefficients
    *---------------------------------------------------------------*/

   void a2lsf(
       float *freq,/* (o) lsf coefficients */
       float *a    /* (i) lpc coefficients */
   ){
       float steps[LSF_NUMBER_OF_STEPS] =
           {(float)0.00635, (float)0.003175, (float)0.0015875,
           (float)0.00079375};
       float step;
       int step_idx;
       int lsp_index;
       float p[LPC_HALFORDER];
       float q[LPC_HALFORDER];
       float p_pre[LPC_HALFORDER];
       float q_pre[LPC_HALFORDER];
       float old_p, old_q, *old;
       float *pq_coef;
       float omega, old_omega;
       int i;
       float hlp, hlp1, hlp2, hlp3, hlp4, hlp5;

       for (i=0; i<LPC_HALFORDER; i++) {
           p[i] = (float)-1.0 * (a[i + 1] + a[LPC_FILTERORDER - i]);
           q[i] = a[LPC_FILTERORDER - i] - a[i + 1];
       }

       p_pre[0] = (float)-1.0 - p[0];
       p_pre[1] = - p_pre[0] - p[1];
       p_pre[2] = - p_pre[1] - p[2];
       p_pre[3] = - p_pre[2] - p[3];
       p_pre[4] = - p_pre[3] - p[4];
       p_pre[4] = p_pre[4] / 2;

       q_pre[0] = (float)1.0 - q[0];
       q_pre[1] = q_pre[0] - q[1];
       q_pre[2] = q_pre[1] - q[2];
       q_pre[3] = q_pre[2] - q[3];
       q_pre[4] = q_pre[3] - q[4];
       q_pre[4] = q_pre[4] / 2;

       omega = 0.0;





       old_omega = 0.0;

       old_p = FLOAT_MAX;
       old_q = FLOAT_MAX;

       /* Here we loop through lsp_index to find all the
          LPC_FILTERORDER roots for omega. */

       for (lsp_index = 0; lsp_index<LPC_FILTERORDER; lsp_index++) {

           /* Depending on lsp_index being even or odd, we
           alternatively solve the roots for the two LSP equations. */


           if ((lsp_index & 0x1) == 0) {
               pq_coef = p_pre;
               old = &old_p;
           } else {
               pq_coef = q_pre;
               old = &old_q;
           }

           /* Start with low resolution grid */

           for (step_idx = 0, step = steps[step_idx];
               step_idx < LSF_NUMBER_OF_STEPS;){

               /*  cos(10piw) + pq(0)cos(8piw) + pq(1)cos(6piw) +
               pq(2)cos(4piw) + pq(3)cod(2piw) + pq(4) */

               hlp = (float)cos(omega * TWO_PI);
               hlp1 = (float)2.0 * hlp + pq_coef[0];
               hlp2 = (float)2.0 * hlp * hlp1 - (float)1.0 +
                   pq_coef[1];
               hlp3 = (float)2.0 * hlp * hlp2 - hlp1 + pq_coef[2];
               hlp4 = (float)2.0 * hlp * hlp3 - hlp2 + pq_coef[3];
               hlp5 = hlp * hlp4 - hlp3 + pq_coef[4];


               if (((hlp5 * (*old)) <= 0.0) || (omega >= 0.5)){

                   if (step_idx == (LSF_NUMBER_OF_STEPS - 1)){

                       if (fabs(hlp5) >= fabs(*old)) {
                           freq[lsp_index] = omega - step;
                       } else {
                           freq[lsp_index] = omega;
                       }







                       if ((*old) >= 0.0){
                           *old = (float)-1.0 * FLOAT_MAX;
                       } else {
                           *old = FLOAT_MAX;
                       }

                       omega = old_omega;
                       step_idx = 0;

                       step_idx = LSF_NUMBER_OF_STEPS;
                   } else {

                       if (step_idx == 0) {
                           old_omega = omega;
                       }

                       step_idx++;
                       omega -= steps[step_idx];

                       /* Go back one grid step */

                       step = steps[step_idx];
                   }
               } else {

               /* increment omega until they are of different sign,
               and we know there is at least one root between omega
               and old_omega */
                   *old = hlp5;
                   omega += step;
               }
           }
       }

       for (i = 0; i<LPC_FILTERORDER; i++) {
           freq[i] = freq[i] * TWO_PI;
       }
   }

   /*----------------------------------------------------------------*
    *  conversion from lsf coefficients to lpc coefficients
    *---------------------------------------------------------------*/

   void lsf2a(
       float *a_coef,  /* (o) lpc coefficients */
       float *freq     /* (i) lsf coefficients */





   ){
       int i, j;
       float hlp;
       float p[LPC_HALFORDER], q[LPC_HALFORDER];
       float a[LPC_HALFORDER + 1], a1[LPC_HALFORDER],
           a2[LPC_HALFORDER];
       float b[LPC_HALFORDER + 1], b1[LPC_HALFORDER],
           b2[LPC_HALFORDER];

       for (i=0; i<LPC_FILTERORDER; i++) {
           freq[i] = freq[i] * PI2;
       }

       /* Check input for ill-conditioned cases.  This part is not
       found in the TIA standard.  It involves the following 2 IF
       blocks.  If "freq" is judged ill-conditioned, then we first
       modify freq[0] and freq[LPC_HALFORDER-1] (normally
       LPC_HALFORDER = 10 for LPC applications), then we adjust
       the other "freq" values slightly */


       if ((freq[0] <= 0.0) || (freq[LPC_FILTERORDER - 1] >= 0.5)){


           if (freq[0] <= 0.0) {
               freq[0] = (float)0.022;
           }


           if (freq[LPC_FILTERORDER - 1] >= 0.5) {
               freq[LPC_FILTERORDER - 1] = (float)0.499;
           }

           hlp = (freq[LPC_FILTERORDER - 1] - freq[0]) /
               (float) (LPC_FILTERORDER - 1);

           for (i=1; i<LPC_FILTERORDER; i++) {
               freq[i] = freq[i - 1] + hlp;
           }
       }

       memset(a1, 0, LPC_HALFORDER*sizeof(float));
       memset(a2, 0, LPC_HALFORDER*sizeof(float));
       memset(b1, 0, LPC_HALFORDER*sizeof(float));
       memset(b2, 0, LPC_HALFORDER*sizeof(float));
       memset(a, 0, (LPC_HALFORDER+1)*sizeof(float));
       memset(b, 0, (LPC_HALFORDER+1)*sizeof(float));






       /* p[i] and q[i] compute cos(2*pi*omega_{2j}) and
       cos(2*pi*omega_{2j-1} in eqs. 4.2.2.2-1 and 4.2.2.2-2.
       Note that for this code p[i] specifies the coefficients
       used in .Q_A(z) while q[i] specifies the coefficients used
       in .P_A(z) */

       for (i=0; i<LPC_HALFORDER; i++) {
           p[i] = (float)cos(TWO_PI * freq[2 * i]);
           q[i] = (float)cos(TWO_PI * freq[2 * i + 1]);
       }

       a[0] = 0.25;
       b[0] = 0.25;

       for (i= 0; i<LPC_HALFORDER; i++) {
           a[i + 1] = a[i] - 2 * p[i] * a1[i] + a2[i];
           b[i + 1] = b[i] - 2 * q[i] * b1[i] + b2[i];
           a2[i] = a1[i];
           a1[i] = a[i];
           b2[i] = b1[i];
           b1[i] = b[i];
       }

       for (j=0; j<LPC_FILTERORDER; j++) {

           if (j == 0) {
               a[0] = 0.25;
               b[0] = -0.25;
           } else {
               a[0] = b[0] = 0.0;
           }

           for (i=0; i<LPC_HALFORDER; i++) {
               a[i + 1] = a[i] - 2 * p[i] * a1[i] + a2[i];
               b[i + 1] = b[i] - 2 * q[i] * b1[i] + b2[i];
               a2[i] = a1[i];
               a1[i] = a[i];
               b2[i] = b1[i];
               b1[i] = b[i];
           }

           a_coef[j + 1] = 2 * (a[LPC_HALFORDER] + b[LPC_HALFORDER]);
       }

       a_coef[0] = 1.0;
   }







