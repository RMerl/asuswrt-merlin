/********************************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**   All rights reserved.
**
********************************************************************************/

/********************************************************************************
* Filename: dct_type_iv_s.c
*
* Purpose:  Discrete Cosine Transform, Type IV used for inverse MLT
*
* The basis functions are
*
*	 cos(PI*(t+0.5)*(k+0.5)/block_length)
*
* for time t and basis function number k.  Due to the symmetry of the expression
* in t and k, it is clear that the forward and inverse transforms are the same.
*
*********************************************************************************/

/***************************************************************************
 Include files                                                           
***************************************************************************/
#include "defs.h"
#include "count.h"
#include "dct4_s.h"

/***************************************************************************
 External variable declarations                                          
***************************************************************************/
extern Word16    syn_bias_7khz[DCT_LENGTH];
extern Word16    dither[DCT_LENGTH];
extern Word16    max_dither[MAX_DCT_LENGTH];

extern Word16       dct_core_s[DCT_LENGTH_DIV_32][DCT_LENGTH_DIV_32];
extern cos_msin_t	s_cos_msin_2[DCT_LENGTH_DIV_32];
extern cos_msin_t	s_cos_msin_4[DCT_LENGTH_DIV_16];
extern cos_msin_t	s_cos_msin_8[DCT_LENGTH_DIV_8];
extern cos_msin_t	s_cos_msin_16[DCT_LENGTH_DIV_4];
extern cos_msin_t	s_cos_msin_32[DCT_LENGTH_DIV_2];
extern cos_msin_t	s_cos_msin_64[DCT_LENGTH];
extern cos_msin_t	*s_cos_msin_table[];

/********************************************************************************
 Function:    dct_type_iv_s

 Syntax:      void dct_type_iv_s (Word16 *input,Word16 *output,Word16 dct_length)
              

 Description: Discrete Cosine Transform, Type IV used for inverse MLT

 Design Notes:
 
 WMOPS:     7kHz |    24kbit    |    32kbit
          -------|--------------|----------------
            AVG  |     1.74     |     1.74
          -------|--------------|----------------  
            MAX  |     1.74     |     1.74
          -------|--------------|---------------- 
				
           14kHz |    24kbit    |    32kbit      |     48kbit
          -------|--------------|----------------|----------------
            AVG  |     3.62     |     3.62       |      3.62   
          -------|--------------|----------------|----------------
            MAX  |     3.62     |     3.62       |      3.62   
          -------|--------------|----------------|----------------

********************************************************************************/

void dct_type_iv_s (Word16 *input,Word16 *output,Word16 dct_length)
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
    Word16   set_span, set_count, set_count_log, pairs_left, sets_left;
    Word16   i,k;
    Word16   index;
    Word16   dummy;
    Word32 	 sum;
    cos_msin_t	**table_ptr_ptr, *cos_msin_ptr;

    Word32 acca;
    Word16 temp;

    Word16   dct_length_log;
    Word16   *dither_ptr;
    
    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Do the sum/difference butterflies, the first part of */
    /* converting one N-point transform into 32 - 10 point transforms  */
    /* transforms, where N = 1 << DCT_LENGTH_LOG.           */
    /*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    test();
    if (dct_length==DCT_LENGTH)
    {
        dct_length_log = DCT_LENGTH_LOG;
        move16();
        dither_ptr = dither;
        move16();
    }
    else
    {
        dct_length_log = MAX_DCT_LENGTH_LOG;
        move16();
        dither_ptr = max_dither;
        move16();
    }
    
    in_buffer  = input;
    move16();
    out_buffer = buffer_a;
    move16();
    
    index=0;
    move16();
    
    i=0;
    move16();

    for (set_count_log = 0;    set_count_log <= dct_length_log - 2;    set_count_log++) 
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
        temp = sub(index,1);
        test();
        if(temp < 0)
        {
            for (sets_left = set_count;sets_left > 0;sets_left--) 
            {
    
                /*||||||||||||||||||||||||||||||||||||||||||||*/
                /* Set up output pointers for the current set */
                /*||||||||||||||||||||||||||||||||||||||||||||*/
                /* pointer arithmetic */
                out_ptr_low    = next_out_base;
                move16();
                next_out_base += set_span;
                move16();
                out_ptr_high   = next_out_base;
                move16();

                /*||||||||||||||||||||||||||||||||||||||||||||||||||*/
                /* Loop over all the butterflies in the current set */
                /*||||||||||||||||||||||||||||||||||||||||||||||||||*/
                    
                do 
                {
                    in_val_low      = *in_ptr++;
                    move16();
                    in_val_high     = *in_ptr++;
                    move16();

                    /* BEST METHOD OF GETTING RID OF BIAS, BUT COMPUTATIONALLY UNPLEASANT */
                    /* ALTERNATIVE METHOD, SMEARS BIAS OVER THE ENTIRE FRAME, COMPUTATIONALLY SIMPLEST. */
                    /* IF THIS WORKS, IT'S PREFERABLE */
                        
                    dummy = add(in_val_low,dither_ptr[i++]);
		    // blp: addition of two 16bits vars, there's no way
		    //      they'll overflow a 32bit var
                    //acca = L_add(dummy,in_val_high);
		    acca = dummy + in_val_high;
                    out_val_low = extract_l(L_shr_nocheck(acca,1));
                    
                    dummy = add(in_val_low,dither_ptr[i++]);
		    // blp: addition of two 16bits vars, there's no way
		    //      they'll overflow a 32bit var
                    //acca = L_add(dummy,-in_val_high);
		    acca = dummy - in_val_high;
                    out_val_high = extract_l(L_shr_nocheck(acca,1));
                    
                    *out_ptr_low++  = out_val_low;
                    move16();
                    *--out_ptr_high = out_val_high;
                    move16();

                    test();
                    
                    /* this involves comparison of pointers */
                    /* pointer arithmetic */

                } while (out_ptr_low < out_ptr_high);
    
            } /* End of loop over sets of the current size */
        }
        else
        {
            for (sets_left = set_count;    sets_left > 0;    sets_left--) 
            {
                /*||||||||||||||||||||||||||||||||||||||||||||*/
                /* Set up output pointers for the current set */
                /*||||||||||||||||||||||||||||||||||||||||||||*/
                
                out_ptr_low    = next_out_base;
                move16();
                next_out_base += set_span;
                move16();
                out_ptr_high   = next_out_base;
                move16();

            	/*||||||||||||||||||||||||||||||||||||||||||||||||||*/
            	/* Loop over all the butterflies in the current set */
            	/*||||||||||||||||||||||||||||||||||||||||||||||||||*/
                
                do 
                {
                    in_val_low      = *in_ptr++;
                    move16();
                    in_val_high     = *in_ptr++;
                    move16();

                    out_val_low     = add(in_val_low,in_val_high);
                    out_val_high    = add(in_val_low,negate(in_val_high));
                    
                    *out_ptr_low++  = out_val_low;
                    move16();
                    *--out_ptr_high = out_val_high;
                    move16();

                    test();
                } while (out_ptr_low < out_ptr_high);
    
            } /* End of loop over sets of the current size */
        }

        /*============================================================*/
        /* Decide which buffers to use as input and output next time. */
        /* Except for the first time (when the input buffer is the    */
        /* subroutine input) we just alternate the local buffers.     */
        /*============================================================*/
        
        in_buffer = out_buffer;
        move16();
        
        test();
        if (out_buffer == buffer_a)
        {
            out_buffer = buffer_b;
            move16();
        }
        else
        {
            out_buffer = buffer_a;
            move16();
        }
        
        index = add(index,1);
    } /* End of loop over set sizes */


    /*++++++++++++++++++++++++++++++++*/
    /* Do 32 - 10 point transforms */
    /*++++++++++++++++++++++++++++++++*/
    
    pair_ptr = in_buffer;
    move16();
    buffer_swap = buffer_c;
    move16();

    for (pairs_left = 1 << (dct_length_log - 1);    pairs_left > 0;    pairs_left--) 
    {
        for ( k=0; k<CORE_SIZE; k++ )
        {
#if PJ_HAS_INT64
	    /* blp: danger danger! not really compatible but faster */
	    pj_int64_t sum64=0;
            move32();
            
            for ( i=0; i<CORE_SIZE; i++ )
            {
                sum64 += L_mult(pair_ptr[i], dct_core_s[i][k]);
            }
	    sum = L_saturate(sum64);
#else
            sum=0L;
            move32();
            
            for ( i=0; i<CORE_SIZE; i++ )
            {
                sum = L_mac(sum, pair_ptr[i],dct_core_s[i][k]);
            }
#endif
            buffer_swap[k] = itu_round(sum);
        }
        
        pair_ptr   += CORE_SIZE;
        move16();
        buffer_swap += CORE_SIZE;
        move16();
    }
    
    for (i=0;i<dct_length;i++)
    {
        in_buffer[i] = buffer_c[i];
        move16();
    }

    table_ptr_ptr = s_cos_msin_table;
    move16();

    /*++++++++++++++++++++++++++++++*/
    /* Perform rotation butterflies */
    /*++++++++++++++++++++++++++++++*/
    index=0;
    move16();
    
    for (set_count_log = dct_length_log - 2 ;    set_count_log >= 0;    set_count_log--) 
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
            move16();
        }
        else
        {
            next_out_base = out_buffer;
            move16();
        }
        
        /*=====================================*/
        /* Loop over all the sets of this size */
        /*=====================================*/

        for (sets_left = set_count;    sets_left > 0;    sets_left--) 
        {

            /*|||||||||||||||||||||||||||||||||||||||||*/
            /* Set up the pointers for the current set */
            /*|||||||||||||||||||||||||||||||||||||||||*/
            
            in_ptr_low     = next_in_base;
            move16();
            
            temp = shr_nocheck(set_span,1);
            in_ptr_high    = in_ptr_low + temp;
            move16();
            
            next_in_base  += set_span;
            move16();
            
            out_ptr_low    = next_out_base;
            move16();
            
            next_out_base += set_span;
            move16();
            out_ptr_high   = next_out_base;
            move16();
            
            cos_msin_ptr   = *table_ptr_ptr;
            move16();

            /*||||||||||||||||||||||||||||||||||||||||||||||||||||||*/
            /* Loop over all the butterfly pairs in the current set */
            /*||||||||||||||||||||||||||||||||||||||||||||||||||||||*/

	        do 
            {
                in_low_even     = *in_ptr_low++;
                move16();
                in_low_odd      = *in_ptr_low++;
                move16();
                in_high_even    = *in_ptr_high++;
                move16();
                in_high_odd     = *in_ptr_high++;
                move16();
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
                move32();
                
                sum = L_mac(sum,cos_even,in_low_even);
                sum = L_mac(sum,negate(msin_even),in_high_even);
                out_low_even = itu_round(L_shl_nocheck(sum,1));
                
                sum = 0L;
                move32();
                sum = L_mac(sum,msin_even,in_low_even);
                sum = L_mac(sum,cos_even,in_high_even);
                out_high_even = itu_round(L_shl_nocheck(sum,1));
                
                sum = 0L;
                move32();
                sum = L_mac(sum,cos_odd,in_low_odd);
                sum = L_mac(sum,msin_odd,in_high_odd);
                out_low_odd = itu_round(L_shl_nocheck(sum,1));
                
                sum = 0L;
                move32();
                sum = L_mac(sum,msin_odd,in_low_odd);
                sum = L_mac(sum,negate(cos_odd),in_high_odd);
                out_high_odd = itu_round(L_shl_nocheck(sum,1));
                
                *out_ptr_low++  = out_low_even;
                move16();
                *--out_ptr_high = out_high_even;
                move16();
                *out_ptr_low++  = out_low_odd;
                move16();
                *--out_ptr_high = out_high_odd;
                move16();
            
                test();
            } while (out_ptr_low < out_ptr_high);

	    } /* End of loop over sets of the current size */

        /*=============================================*/
        /* Swap input and output buffers for next time */
        /*=============================================*/
        
        buffer_swap = in_buffer;
        move16();
        in_buffer   = out_buffer;
        move16();
        out_buffer  = buffer_swap;
        move16();
        
        index = add(index,1);
        table_ptr_ptr++;
    }
    /*------------------------------------
    
         ADD IN BIAS FOR OUTPUT
         
    -----------------------------------*/
    if (dct_length==DCT_LENGTH)
    {
        for(i=0;i<320;i++) 
        {
	   // blp: addition of two 16bits vars, there's no way
	   //      they'll overflow a 32bit var
           //sum = L_add(output[i],syn_bias_7khz[i]);
	   sum = output[i] + syn_bias_7khz[i];
           acca = L_sub(sum,32767);
           test();
           if (acca > 0) 
           {
               sum = 32767L;
               move32();
           }
	   // blp: addition of two 16bits vars, there's no way
	   //      they'll overflow 32bit var
           //acca = L_add(sum,32768L);
	   acca = sum + 32768;
           test();
           if (acca < 0) 
           {
               sum = -32768L;
               move32();
           }
           output[i] = extract_l(sum);
        }
    }
}

