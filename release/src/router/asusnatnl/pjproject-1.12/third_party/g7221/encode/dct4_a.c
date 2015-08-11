/*********************************************************************************
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**	 All rights reserved.
**
*********************************************************************************/

/*********************************************************************************
* Filename: dct_type_iv_a.c
*
* Purpose:  Discrete Cosine Transform, Type IV used for MLT
*
* The basis functions are
*
*	 cos(PI*(t+0.5)*(k+0.5)/block_length)
*
* for time t and basis function number k.  Due to the symmetry of the expression
* in t and k, it is clear that the forward and inverse transforms are the same.
*
*********************************************************************************/

/*********************************************************************************
 Include files                                                           
*********************************************************************************/
#include "defs.h"
#include "count.h"
#include "dct4_a.h"

/*********************************************************************************
 External variable declarations                                          
*********************************************************************************/
extern Word16       anal_bias[DCT_LENGTH];
extern Word16       dct_core_a[DCT_LENGTH_DIV_32][DCT_LENGTH_DIV_32];
extern cos_msin_t   a_cos_msin_2 [DCT_LENGTH_DIV_32];
extern cos_msin_t   a_cos_msin_4 [DCT_LENGTH_DIV_16];
extern cos_msin_t   a_cos_msin_8 [DCT_LENGTH_DIV_8];
extern cos_msin_t   a_cos_msin_16[DCT_LENGTH_DIV_4];
extern cos_msin_t   a_cos_msin_32[DCT_LENGTH_DIV_2];
extern cos_msin_t   a_cos_msin_64[DCT_LENGTH];
extern cos_msin_t   *a_cos_msin_table[];

/*********************************************************************************
 Function:    dct_type_iv_a

 Syntax:      void dct_type_iv_a (input, output, dct_length) 
                        Word16   input[], output[], dct_length;              

 Description: Discrete Cosine Transform, Type IV used for MLT

 Design Notes:
                
 WMOPS:          |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    1.14      |     1.14
          -------|--------------|----------------  
            MAX  |    1.14      |     1.14
          -------|--------------|---------------- 
                
           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    2.57      |     2.57       |     2.57
          -------|--------------|----------------|----------------
            MAX  |    2.57      |     2.57       |     2.57
          -------|--------------|----------------|----------------

*********************************************************************************/

void dct_type_iv_a (Word16 *input,Word16 *output,Word16 dct_length)
{
    Word16   buffer_a[MAX_DCT_LENGTH], buffer_b[MAX_DCT_LENGTH], buffer_c[MAX_DCT_LENGTH];
    Word16   *in_ptr, *in_ptr_low, *in_ptr_high, *next_in_base;
    Word16   *out_ptr_low, *out_ptr_high, *next_out_base;
    Word16   *out_buffer, *in_buffer, *buffer_swap;
    Word16   in_val_low, in_val_high;
    Word16   out_val_low, out_val_high;
    Word16   in_low_even, in_low_odd;
    Word16   in_high_even, in_high_odd;
    Word16   out_low_even, out_low_odd;
    Word16   out_high_even, out_high_odd;
    Word16   *pair_ptr;
    Word16   cos_even, cos_odd, msin_even, msin_odd;
    Word16   neg_cos_odd;
    Word16   neg_msin_even;
    Word32   sum;
    Word16   set_span, set_count, set_count_log, pairs_left, sets_left;
    Word16   i,k;
    Word16   index;
    cos_msin_t  **table_ptr_ptr, *cos_msin_ptr;
    
    Word16   temp;
    Word32   acca;

    Word16   dct_length_log;


    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Do the sum/difference butterflies, the first part of */
    /* converting one N-point transform into N/2 two-point  */
    /* transforms, where N = 1 << DCT_LENGTH_LOG. = 64/128  */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    test();
    if (dct_length==DCT_LENGTH)
    {
        dct_length_log = DCT_LENGTH_LOG;

        /* Add bias offsets */
        for (i=0;i<dct_length;i++)
        {
            input[i] = add(input[i],anal_bias[i]);
            move16();
        }
    }
    else
        dct_length_log = MAX_DCT_LENGTH_LOG;

    index = 0L;
    move16();

    in_buffer  = input;
    move16();

    out_buffer = buffer_a;
    move16();

    temp = sub(dct_length_log,2);
    for (set_count_log=0;set_count_log<=temp;set_count_log++)
    {

        /*===========================================================*/
        /* Initialization for the loop over sets at the current size */
        /*===========================================================*/

        /*    set_span      = 1 << (DCT_LENGTH_LOG - set_count_log); */
        set_span = shr_nocheck(dct_length,set_count_log);

        set_count     = shl_nocheck(1,set_count_log);

        in_ptr        = in_buffer;
        move16();

        next_out_base = out_buffer;
        move16();

        /*=====================================*/
        /* Loop over all the sets of this size */
        /*=====================================*/

        for (sets_left=set_count;sets_left>0;sets_left--)
        {

            /*||||||||||||||||||||||||||||||||||||||||||||*/
            /* Set up output pointers for the current set */
            /*||||||||||||||||||||||||||||||||||||||||||||*/

            out_ptr_low    = next_out_base;
            next_out_base  = next_out_base + set_span;
            out_ptr_high   = next_out_base;

            /*||||||||||||||||||||||||||||||||||||||||||||||||||*/
            /* Loop over all the butterflies in the current set */
            /*||||||||||||||||||||||||||||||||||||||||||||||||||*/

            do 
            {
                in_val_low      = *in_ptr++;
                in_val_high     = *in_ptr++;
		// blp: addition of two 16bits vars, there's no way
		//      they'll overflow a 32bit var
                //acca            = L_add(in_val_low,in_val_high);
		acca = (in_val_low + in_val_high);
		acca            = L_shr_nocheck(acca,1);
                out_val_low     = extract_l(acca);

                acca            = L_sub(in_val_low,in_val_high);
                acca            = L_shr_nocheck(acca,1);
                out_val_high    = extract_l(acca);

                *out_ptr_low++  = out_val_low;
                *--out_ptr_high = out_val_high;

                test();
            } while (out_ptr_low < out_ptr_high);

        } /* End of loop over sets of the current size */

        /*============================================================*/
        /* Decide which buffers to use as input and output next time. */
        /* Except for the first time (when the input buffer is the    */
        /* subroutine input) we just alternate the local buffers.     */
        /*============================================================*/

        in_buffer = out_buffer;
        move16();
        if (out_buffer == buffer_a)
            out_buffer = buffer_b;
        else
            out_buffer = buffer_a;
        index = add(index,1);

    } /* End of loop over set sizes */


    /*++++++++++++++++++++++++++++++++*/
    /* Do N/2 two-point transforms,   */
    /* where N =  1 << DCT_LENGTH_LOG */
    /*++++++++++++++++++++++++++++++++*/

    pair_ptr = in_buffer;
    move16();

    buffer_swap = buffer_c;
    move16();

    temp = sub(dct_length_log,1);
    temp = shl_nocheck(1,temp);

    for (pairs_left=temp; pairs_left > 0; pairs_left--)
    {
        for ( k=0; k<CORE_SIZE; k++ )
        {
#if PJ_HAS_INT64
	    /* blp: danger danger! not really compatible but faster */
	    pj_int64_t sum64=0;
            move32();
            
            for ( i=0; i<CORE_SIZE; i++ )
            {
                sum64 += L_mult(pair_ptr[i], dct_core_a[i][k]);
            }
	    sum = L_saturate(sum64);
#else
            sum=0L;
            move32();
            for ( i=0; i<CORE_SIZE; i++ )
            {
                sum = L_mac(sum, pair_ptr[i],dct_core_a[i][k]);
            }
#endif
            buffer_swap[k] = itu_round(sum);
        }
        /* address arithmetic */
        pair_ptr   += CORE_SIZE;
        buffer_swap += CORE_SIZE;
    }

    for (i=0;i<dct_length;i++)
    {
        in_buffer[i] = buffer_c[i];
        move16();
    }
    
    table_ptr_ptr = a_cos_msin_table;

    /*++++++++++++++++++++++++++++++*/
    /* Perform rotation butterflies */
    /*++++++++++++++++++++++++++++++*/
    temp = sub(dct_length_log,2);
    for (set_count_log = temp; set_count_log >= 0;    set_count_log--)
    {
        /*===========================================================*/
        /* Initialization for the loop over sets at the current size */
        /*===========================================================*/
        /*    set_span      = 1 << (DCT_LENGTH_LOG - set_count_log); */
        set_span = shr_nocheck(dct_length,set_count_log);

        set_count     = shl_nocheck(1,set_count_log);
        next_in_base  = in_buffer;
        move16();

        test();
        if (set_count_log == 0)
        {
            next_out_base = output;
        }
        else
        {
            next_out_base = out_buffer;
        }


        /*=====================================*/
        /* Loop over all the sets of this size */
        /*=====================================*/
        for (sets_left = set_count; sets_left > 0;sets_left--)
        {
            /*|||||||||||||||||||||||||||||||||||||||||*/
            /* Set up the pointers for the current set */
            /*|||||||||||||||||||||||||||||||||||||||||*/
            in_ptr_low     = next_in_base;
            move16();
            temp           = shr_nocheck(set_span,1);

            /* address arithmetic */
            in_ptr_high    = in_ptr_low + temp;
            next_in_base  += set_span;
            out_ptr_low    = next_out_base;
            next_out_base += set_span;
            out_ptr_high   = next_out_base;
            cos_msin_ptr   = *table_ptr_ptr;

            /*||||||||||||||||||||||||||||||||||||||||||||||||||||||*/
            /* Loop over all the butterfly pairs in the current set */
            /*||||||||||||||||||||||||||||||||||||||||||||||||||||||*/

            do 
            {
                /* address arithmetic */
                in_low_even     = *in_ptr_low++;
                in_low_odd      = *in_ptr_low++;
                in_high_even    = *in_ptr_high++;
                in_high_odd     = *in_ptr_high++;
                cos_even        = cos_msin_ptr[0].cosine;
                move16();
                msin_even       = cos_msin_ptr[0].minus_sine;
                move16();
                cos_odd         = cos_msin_ptr[1].cosine;
                move16();
                msin_odd        = cos_msin_ptr[1].minus_sine;
                move16();
                cos_msin_ptr   += 2;

                sum = 0L;
                sum=L_mac(sum,cos_even,in_low_even);
                neg_msin_even = negate(msin_even);
                sum=L_mac(sum,neg_msin_even,in_high_even);
                out_low_even = itu_round(sum);

                sum = 0L;
                sum=L_mac(sum,msin_even,in_low_even);
                sum=L_mac(sum,cos_even,in_high_even);
                out_high_even= itu_round(sum);

                sum = 0L;
                sum=L_mac(sum,cos_odd,in_low_odd);
                sum=L_mac(sum,msin_odd,in_high_odd);
                out_low_odd= itu_round(sum);

                sum = 0L;
                sum=L_mac(sum,msin_odd,in_low_odd);
                neg_cos_odd = negate(cos_odd);
                sum=L_mac(sum,neg_cos_odd,in_high_odd);
                out_high_odd= itu_round(sum);

                *out_ptr_low++  = out_low_even;
                *--out_ptr_high = out_high_even;
                *out_ptr_low++  = out_low_odd;
                *--out_ptr_high = out_high_odd;
                test();
            } while (out_ptr_low < out_ptr_high);

        } /* End of loop over sets of the current size */

        /*=============================================*/
        /* Swap input and output buffers for next time */
        /*=============================================*/

        buffer_swap = in_buffer;
        in_buffer   = out_buffer;
        out_buffer  = buffer_swap;
        table_ptr_ptr++;
    }
}

