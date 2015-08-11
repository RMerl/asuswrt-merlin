/***************************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**   All rights reserved.
**
***************************************************************************/

/***************************************************************************
  Filename:    encoder.c    

  Purpose:     Contains files used to implement the G.722.1 Annex C encoder

  Design Notes:

***************************************************************************/

/***************************************************************************
 Include files                                                           
***************************************************************************/

#include <stdio.h>
#include <math.h>
#include "defs.h"
#include "huff_def.h"
#include "tables.h"
#include "count.h"

/***************************************************************************
 Function:    encoder                                                     
                                                                         
 Syntax:      void encoder(Word16 number_of_available_bits,                     
                           Word16 number_of_regions,
                           Word16 mlt_coefs,                                        
                           Word16 mag_shift,                                        
                           Word16 out_words)                                        
              
              inputs:   number_of_available_bits
                        number_of_regions
                        mag_shift
                        mlt_coefs[DCT_LENGTH]
                        
              outputs:  out_words[MAX_BITS_PER_FRAME/16]
                                                                         
                                                                         
 Description: Encodes the mlt coefs into out_words using G.722.1 Annex C
                                                           
                                                                         
 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.93      |     1.04
          -------|--------------|----------------  
            MAX  |    1.20      |     1.28
          -------|--------------|---------------- 
                                                                        
           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    1.39      |     1.71       |     2.01
          -------|--------------|----------------|----------------
            MAX  |    2.00      |     2.30       |     2.52
          -------|--------------|----------------|----------------
                                                                         
***************************************************************************/

void encoder(Word16  number_of_available_bits,
             Word16  number_of_regions,
             Word16  *mlt_coefs,
             Word16  mag_shift,
             Word16  *out_words)
{

    Word16  num_categorization_control_bits;
    Word16  num_categorization_control_possibilities;
    Word16  number_of_bits_per_frame;
    Word16  number_of_envelope_bits;
    Word16  categorization_control;
    Word16  region;
    Word16  absolute_region_power_index[MAX_NUMBER_OF_REGIONS];
    Word16  power_categories[MAX_NUMBER_OF_REGIONS];
    Word16  category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES-1];
    Word16  drp_num_bits[MAX_NUMBER_OF_REGIONS+1];
    UWord16 drp_code_bits[MAX_NUMBER_OF_REGIONS+1];
    Word16  region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS];
    UWord32 region_mlt_bits[4*MAX_NUMBER_OF_REGIONS];
    Word16  mag_shift_offset;

    Word16 temp;

    /* initialize variables */
    test();
    if (number_of_regions == NUMBER_OF_REGIONS)
    {
        num_categorization_control_bits = NUM_CATEGORIZATION_CONTROL_BITS;
        move16();
        num_categorization_control_possibilities = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
        move16();
    } 
    else
    {
        num_categorization_control_bits = MAX_NUM_CATEGORIZATION_CONTROL_BITS;
        move16();
        num_categorization_control_possibilities = MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
        move16();
    }

    number_of_bits_per_frame = number_of_available_bits;
    move16();

    for (region=0; region<number_of_regions; region++)
    {
        region_mlt_bit_counts[region] = 0;
        move16();
    }
    
    /* Estimate power envelope. */
    number_of_envelope_bits = compute_region_powers(mlt_coefs,
                                                    mag_shift,
                                                    drp_num_bits,
                                                    drp_code_bits,
                                                    absolute_region_power_index,
                                                    number_of_regions);

    /* Adjust number of available bits based on power envelope estimate */
    temp = sub(number_of_available_bits,number_of_envelope_bits);
    number_of_available_bits = sub(temp,num_categorization_control_bits);

    /* get categorizations */
    categorize(number_of_available_bits,
               number_of_regions,
               num_categorization_control_possibilities,
               absolute_region_power_index,
               power_categories,
               category_balances);

    /* Adjust absolute_region_category_index[] for mag_shift.
       This assumes that REGION_POWER_STEPSIZE_DB is defined
       to be exactly 3.010299957 or 20.0 times log base 10
       of square root of 2. */
    temp = shl_nocheck(mag_shift,1);
    mag_shift_offset = add(temp,REGION_POWER_TABLE_NUM_NEGATIVES);
    
    for (region=0; region<number_of_regions; region++)
    {
        absolute_region_power_index[region] = add(absolute_region_power_index[region],mag_shift_offset);
        move16();
    }

    /* adjust the absolute power region index based on the mlt coefs */
    adjust_abs_region_power_index(absolute_region_power_index,mlt_coefs,number_of_regions);


    /* quantize and code the mlt coefficients based on categorizations */
    vector_quantize_mlts(number_of_available_bits,
                         number_of_regions,
                         num_categorization_control_possibilities,
                         mlt_coefs,
                         absolute_region_power_index,
                         power_categories,
                         category_balances,
                         &categorization_control,
                         region_mlt_bit_counts,
                         region_mlt_bits);

    /* stuff bits into words */
    bits_to_words(region_mlt_bits,
                  region_mlt_bit_counts,
                  drp_num_bits,
                  drp_code_bits,
                  out_words,
                  categorization_control,
                  number_of_regions,
                  num_categorization_control_bits,
                  number_of_bits_per_frame);

}

/***************************************************************************
 Function:    bits_to_words

 Syntax:      bits_to_words(UWord32 *region_mlt_bits,              
                            Word16  *region_mlt_bit_counts,                                                             
                            Word16  *drp_num_bits,                                                                      
                            UWord16 *drp_code_bits,                                                                     
                            Word16  *out_words,                                                                         
                            Word16  categorization_control,                                                                         
                            Word16  number_of_regions,
                            Word16  num_categorization_control_bits,
                            Word16  number_of_bits_per_frame)                                                           
                                                                   
                                                                   
 Description: Stuffs the bits into words for output

 WMOPS:     7kHz |    24kbit    |      32kbit
          -------|--------------|----------------
            AVG  |    0.09      |     0.12
          -------|--------------|----------------  
            MAX  |    0.10      |     0.13
          -------|--------------|---------------- 
          
           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.12      |     0.15       |     0.19
          -------|--------------|----------------|----------------
            MAX  |    0.14      |     0.17       |     0.21
          -------|--------------|----------------|----------------

***************************************************************************/
void bits_to_words(UWord32 *region_mlt_bits,
                   Word16  *region_mlt_bit_counts,
                   Word16  *drp_num_bits,
                   UWord16 *drp_code_bits,
                   Word16  *out_words,
                   Word16  categorization_control,
                   Word16  number_of_regions,
                   Word16  num_categorization_control_bits,
                   Word16  number_of_bits_per_frame)
{
    Word16  out_word_index = 0;
    Word16  j;
    Word16  region;
    Word16  out_word;
    Word16  region_bit_count;
    Word16  current_word_bits_left;
    UWord16 slice;
    Word16  out_word_bits_free = 16;
    UWord32 *in_word_ptr;
    UWord32 current_word;
    
    Word32  acca = 0;
    Word32  accb;
    Word16  temp;

    /* First set up the categorization control bits to look like one more set of region power bits. */
    out_word = 0;
    move16();

    drp_num_bits[number_of_regions] = num_categorization_control_bits;
    move16();
    
    drp_code_bits[number_of_regions] = (UWord16)categorization_control;
    move16();

    /* These code bits are right justified. */
    for (region=0; region <= number_of_regions; region++)
    {
        current_word_bits_left = drp_num_bits[region];
        move16();
        
        current_word = (UWord32)drp_code_bits[region];
        move16();
        
        j = sub(current_word_bits_left,out_word_bits_free);

        test();
        if (j >= 0)
        {
            temp = extract_l(L_shr_nocheck(current_word,j));
            out_word = add(out_word,temp);

            out_words[out_word_index++] = out_word;
            move16();
            
            out_word_bits_free = 16;
            move16();
            
            out_word_bits_free = sub(out_word_bits_free,j);
            
            acca = (current_word << out_word_bits_free);
            out_word = extract_l(acca);
        }
        else
        {
            j = negate(j);

            acca = (current_word << j);
            accb = L_deposit_l(out_word);
            acca = L_add(accb,acca);
            out_word = extract_l(acca);
            
            out_word_bits_free = sub(out_word_bits_free,current_word_bits_left);
        }
    }

    /* These code bits are left justified. */
    
    for (region=0;region<number_of_regions; region++)
    {
        accb = L_deposit_l(out_word_index);
        accb = L_shl_nocheck(accb,4);
        accb = L_sub(accb,number_of_bits_per_frame);
        test();
        if(accb < 0)        
        {
            temp = shl_nocheck(region,2);
            in_word_ptr = &region_mlt_bits[temp];
            region_bit_count = region_mlt_bit_counts[region];
            move16();

            temp = sub(32,region_bit_count);
            test();
            if(temp > 0)
                current_word_bits_left = region_bit_count;
            else
                current_word_bits_left = 32;

            current_word = *in_word_ptr++;
    
            acca = L_deposit_l(out_word_index);
            acca = L_shl_nocheck(acca,4);
            acca = L_sub(acca,number_of_bits_per_frame);
            
            /* from while loop */
            test();
            test();
            logic16(); 
            while ((region_bit_count > 0) && (acca < 0))
            {
                /* from while loop */
                test();
                test();
                logic16(); 
                
                temp = sub(current_word_bits_left,out_word_bits_free);
                test();
                if (temp >= 0)
                {
                    temp = sub(32,out_word_bits_free);
                    accb = LU_shr(current_word,temp);
                    slice = (UWord16)extract_l(accb);
                    
                    out_word = add(out_word,slice);
    
                    test();
                    current_word <<= out_word_bits_free;

                    current_word_bits_left = sub(current_word_bits_left,out_word_bits_free);
                    out_words[out_word_index++] = extract_l(out_word);
                    move16();

                    out_word = 0;
                    move16();

                    out_word_bits_free = 16;
                    move16();
                }
                else
                {
                    temp = sub(32,current_word_bits_left);
                    accb = LU_shr(current_word,temp);
                    slice = (UWord16)extract_l(accb);
                    
                    temp = sub(out_word_bits_free,current_word_bits_left);
                    test();
                    accb = slice << temp;
                    acca = L_deposit_l(out_word);
                    acca = L_add(acca,accb);
                    out_word = extract_l(acca);
                    out_word_bits_free = sub(out_word_bits_free,current_word_bits_left);
                    
                    current_word_bits_left = 0;
                    move16();
                }
    
                test();
                if (current_word_bits_left == 0)
                {
                    current_word = *in_word_ptr++;
                    region_bit_count = sub(region_bit_count,32);
                    
                    /* current_word_bits_left = MIN(32,region_bit_count); */
                    temp = sub(32,region_bit_count);
                    test();
                    if(temp > 0)
                        current_word_bits_left = region_bit_count;
                    else
                        current_word_bits_left = 32;
                    
                }
                acca = L_deposit_l(out_word_index);
                acca = L_shl_nocheck(acca,4);
                acca = L_sub(acca,number_of_bits_per_frame);
            }
            accb = L_deposit_l(out_word_index);
            accb = L_shl_nocheck(accb,4);
            accb = L_sub(accb,number_of_bits_per_frame);
        }
    }

    /* Fill out with 1's. */

    test();
    while (acca < 0)
    {
        test();
        current_word = 0x0000ffff;
        move32();

        temp = sub(16,out_word_bits_free);
        acca = LU_shr(current_word,temp);
        slice = (UWord16)extract_l(acca);

        out_word = add(out_word,slice);
        out_words[out_word_index++] = out_word;
        move16();

        out_word = 0;
        move16();
        
        out_word_bits_free = 16;
        move16();
        
        acca = L_deposit_l(out_word_index);
        acca = L_shl_nocheck(acca,4);
        acca = L_sub(acca,number_of_bits_per_frame);
    }
}
/***************************************************************************
 Function:    adjust_abs_region_power_index

 Syntax:      adjust_abs_region_power_index(Word16 *absolute_region_power_index,
                                            Word16 *mlt_coefs,
                                            Word16 number_of_regions)

              inputs:   *mlt_coefs
                        *absolute_region_power_index
                        number_of_regions
            
              outputs:  *absolute_region_power_index
 
 Description: Adjusts the absolute power index
 
 
 WMOPS:     7kHz |    24kbit    |      32kbit
          -------|--------------|----------------
            AVG  |    0.03      |      0.03
          -------|--------------|----------------  
            MAX  |    0.12      |      0.12
          -------|--------------|---------------- 

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.03      |     0.03       |     0.03
          -------|--------------|----------------|----------------
            MAX  |    0.14      |     0.14       |     0.14
          -------|--------------|----------------|----------------

***************************************************************************/
void adjust_abs_region_power_index(Word16 *absolute_region_power_index,Word16 *mlt_coefs,Word16 number_of_regions)
{
    Word16 n,i;
    Word16 region;
    Word16 *raw_mlt_ptr;
    
    Word32 acca;
    Word16 temp;

    for (region=0; region<number_of_regions; region++)
    {
        n = sub(absolute_region_power_index[region],39);
        n = shr_nocheck(n,1);
        
        test();
        if (n > 0)
        {
            temp = extract_l(L_mult0(region,REGION_SIZE));

            raw_mlt_ptr = &mlt_coefs[temp];

            for (i=0; i<REGION_SIZE; i++)
            {
                acca = L_shl_nocheck(*raw_mlt_ptr,16);
                acca = L_add(acca,32768L);
                acca = L_shr_nocheck(acca,n);
                acca = L_shr_nocheck(acca,16);
                *raw_mlt_ptr++ = extract_l(acca);
            }

            temp = shl_nocheck(n,1);
            temp = sub(absolute_region_power_index[region],temp);
            absolute_region_power_index[region] = temp;
            move16();
        }
    }
}

/***************************************************************************
 Function:    compute_region_powers

 Syntax:      Word16 compute_region_powers(Word16  *mlt_coefs,                         
                                           Word16  mag_shift,                         
                                           Word16  *drp_num_bits,                      
                                           UWord16 *drp_code_bits,                     
                                           Word16  *absolute_region_power_index,       
                                           Word16  number_of_regions)
                mlt_coefs[DCT_LENGTH];                      
                mag_shift;                                          
                drp_num_bits[MAX_NUMBER_OF_REGIONS];                      
                drp_code_bits[MAX_NUMBER_OF_REGIONS];                     
                absolute_region_power_index[MAX_NUMBER_OF_REGIONS];       
                number_of_regions;
                                                                   
 Description: Computes the power for each of the regions

 
 WMOPS:     7kHz |    24kbit    |    32kbit
          -------|--------------|----------------
            AVG  |    0.09      |    0.09
          -------|--------------|----------------  
            MAX  |    0.13      |    0.13
          -------|--------------|---------------- 

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.20      |     0.20       |     0.20
          -------|--------------|----------------|----------------
            MAX  |    0.29      |     0.29       |     0.29
          -------|--------------|----------------|----------------

***************************************************************************/

Word16 compute_region_powers(Word16  *mlt_coefs,
                             Word16  mag_shift,
                             Word16  *drp_num_bits,
                             UWord16 *drp_code_bits,
                             Word16  *absolute_region_power_index,
                             Word16  number_of_regions)
{

    Word16 *input_ptr;
    Word32 long_accumulator;
    Word16 itemp1;
    Word16 power_shift;
    Word16 region;
    Word16 j;
    Word16 differential_region_power_index[MAX_NUMBER_OF_REGIONS];
    Word16 number_of_bits;
    
    Word32 acca;
    Word16 temp;
    Word16 temp1;
    Word16 temp2;


    input_ptr = mlt_coefs;
    for (region=0; region<number_of_regions; region++)
    {
        long_accumulator = L_deposit_l(0);

        for (j=0; j<REGION_SIZE; j++)
        {
            itemp1 = *input_ptr++;
            move16();
            long_accumulator = L_mac0(long_accumulator,itemp1,itemp1);
        }

        power_shift = 0;
        move16();

        acca = (long_accumulator & 0x7fff0000L);
        logic32();

        test();
        while (acca > 0)
        {
            test();
            long_accumulator = L_shr_nocheck(long_accumulator,1);
            
            acca = (long_accumulator & 0x7fff0000L);
            logic32();

            power_shift = add(power_shift,1);
        }
        
        acca = L_sub(long_accumulator,32767);
        
        temp = add(power_shift,15);
        test();
        test();
        logic16();
        while ((acca <= 0) && (temp >= 0))
        {
            test();
            test();
            logic16();
            
            long_accumulator = L_shl_nocheck(long_accumulator,1);
            acca = L_sub(long_accumulator,32767);
            power_shift--;
            temp = add(power_shift,15);
        }
        long_accumulator = L_shr_nocheck(long_accumulator,1);
        /* 28963 corresponds to square root of 2 times REGION_SIZE(20). */
        acca = L_sub(long_accumulator,28963);
        
        test();
        if (acca >= 0)
            power_shift = add(power_shift,1);
        
        acca = L_deposit_l(mag_shift);
        acca = L_shl_nocheck(acca,1);
        acca = L_sub(power_shift,acca);
        acca = L_add(35,acca);
        acca = L_sub(acca,REGION_POWER_TABLE_NUM_NEGATIVES);
        absolute_region_power_index[region] = extract_l(acca);
    }


    /* Before we differentially encode the quantized region powers, adjust upward the
    valleys to make sure all the peaks can be accurately represented. */
    temp = sub(number_of_regions,2);

    for (region = temp; region >= 0; region--)
    {
        temp1 = sub(absolute_region_power_index[region+1],DRP_DIFF_MAX);
        temp2 = sub(absolute_region_power_index[region],temp1);
        test();
        if (temp2 < 0)
        {
            absolute_region_power_index[region] = temp1;
            move16();
        }
    }

    /* The MLT is currently scaled too low by the factor
       ENCODER_SCALE_FACTOR(=18318)/32768 * (1./sqrt(160).
       This is the ninth power of 1 over the square root of 2.
       So later we will add ESF_ADJUSTMENT_TO_RMS_INDEX (now 9)
       to drp_code_bits[0]. */

    /* drp_code_bits[0] can range from 1 to 31. 0 will be used only as an escape sequence. */
    temp1 = sub(1,ESF_ADJUSTMENT_TO_RMS_INDEX);
    temp2 = sub(absolute_region_power_index[0],temp1);
    test();
    if (temp2 < 0)
    {
        absolute_region_power_index[0] = temp1;
        move16();
    }
    
    temp1 = sub(31,ESF_ADJUSTMENT_TO_RMS_INDEX);

	/*
	 * The next line was corrected in Release 1.2 
	 */

    temp2 = sub(absolute_region_power_index[0], temp1);  
    test();
    if (temp2 > 0)
    {
        absolute_region_power_index[0] = temp1;
        move16();
    }

    differential_region_power_index[0] = absolute_region_power_index[0];
    move16();
    
    number_of_bits = 5;
    move16();
    
    drp_num_bits[0] = 5;
    move16();
    
    drp_code_bits[0] = (UWord16)add(absolute_region_power_index[0],ESF_ADJUSTMENT_TO_RMS_INDEX);
    move16();

    /* Lower limit the absolute region power indices to -8 and upper limit them to 31. Such extremes
     may be mathematically impossible anyway.*/
    for (region=1; region<number_of_regions; region++)
    {
        temp1 = sub(-8,ESF_ADJUSTMENT_TO_RMS_INDEX);
        temp2 = sub(absolute_region_power_index[region],temp1);
        test();
        if (temp2 < 0)
        {
            absolute_region_power_index[region] = temp1;
            move16();
        }

        temp1 = sub(31,ESF_ADJUSTMENT_TO_RMS_INDEX);
        temp2 = sub(absolute_region_power_index[region],temp1);
        test();
        if (temp2 > 0)
        {
            absolute_region_power_index[region] = temp1;
            move16();
        }
    }

    for (region=1; region<number_of_regions; region++)
    {
        j = sub(absolute_region_power_index[region],absolute_region_power_index[region-1]);
        temp = sub(j,DRP_DIFF_MIN);
        test();
        if (temp < 0)
        {
            j = DRP_DIFF_MIN;
        }
        j = sub(j,DRP_DIFF_MIN);
        move16();
        differential_region_power_index[region] = j;
        move16();
        
        temp = add(absolute_region_power_index[region-1],differential_region_power_index[region]);
        temp = add(temp,DRP_DIFF_MIN);
        absolute_region_power_index[region] = temp;
        move16();

        number_of_bits = add(number_of_bits,differential_region_power_bits[region][j]);
        drp_num_bits[region] = differential_region_power_bits[region][j];
        move16();
        drp_code_bits[region] = differential_region_power_codes[region][j];
        move16();
    }

    return (number_of_bits);
}

/***************************************************************************
 Function:    vector_quantize_mlts

 Syntax:      void vector_quantize_mlts(number_of_available_bits,
                                        number_of_regions,                     
                                        num_categorization_control_possibilities,
                                        mlt_coefs,                                    
                                        absolute_region_power_index,                  
                                        power_categories,                             
                                        category_balances,                            
                                        p_categorization_control,                               
                                        region_mlt_bit_counts,                        
                                        region_mlt_bits)                              
                                                                                    
              Word16 number_of_available_bits;                                        
              Word16 number_of_regions;
              Word16 num_categorization_control_possibilities;
              Word16 mlt_coefs[DCT_LENGTH];                                         
              Word16 absolute_region_power_index[MAX_NUMBER_OF_REGIONS];                  
              Word16 power_categories[MAX_NUMBER_OF_REGIONS];                             
              Word16 category_balances[MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES-1];         
              Word16 *p_categorization_control;                                                 
              Word16 region_mlt_bit_counts[MAX_NUMBER_OF_REGIONS];                        
              Word32 region_mlt_bits[4*MAX_NUMBER_OF_REGIONS];                            

 Description: Scalar quantized vector Huffman coding (SQVH)

 
 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.57      |     0.65
          -------|--------------|----------------  
            MAX  |    0.78      |     0.83
          -------|--------------|---------------- 

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.62      |     0.90       |     1.11
          -------|--------------|----------------|----------------
            MAX  |    1.16      |     1.39       |     1.54
          -------|--------------|----------------|----------------

***************************************************************************/

void vector_quantize_mlts(Word16 number_of_available_bits,
                          Word16 number_of_regions,
                          Word16 num_categorization_control_possibilities,
                          Word16 *mlt_coefs,
                          Word16 *absolute_region_power_index,
                          Word16 *power_categories,
                          Word16 *category_balances,
                          Word16 *p_categorization_control,
                          Word16 *region_mlt_bit_counts,
                          UWord32 *region_mlt_bits)
{

    Word16 *raw_mlt_ptr;
    Word16 region;
    Word16 category;
    Word16 total_mlt_bits = 0;

    Word16 temp;
    Word16 temp1;
    Word16 temp2;

    /* Start in the middle of the categorization control range. */
    temp = shr_nocheck(num_categorization_control_possibilities,1);
    temp = sub(temp,1);
    for (*p_categorization_control = 0; *p_categorization_control < temp; (*p_categorization_control)++)
    {
        region = category_balances[*p_categorization_control];
        move16();
        power_categories[region] = add(power_categories[region],1);
        move16();
    }

    for (region=0; region<number_of_regions; region++)
    {
        category = power_categories[region];
        move16();
        temp = extract_l(L_mult0(region,REGION_SIZE));
        raw_mlt_ptr = &mlt_coefs[temp];
        move16();
        temp = sub(category,(NUM_CATEGORIES-1));
        test();
        if (temp < 0)
        {
            region_mlt_bit_counts[region] =
            vector_huffman(category, absolute_region_power_index[region],raw_mlt_ptr,
                           &region_mlt_bits[shl_nocheck(region,2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
            move16();
        }
        total_mlt_bits = add(total_mlt_bits,region_mlt_bit_counts[region]);
    }


    /* If too few bits... */
    temp = sub(total_mlt_bits,number_of_available_bits);
    test();
    test();
    logic16();
    while ((temp < 0) && (*p_categorization_control > 0))
    {
        test();
        test();
        logic16();
        (*p_categorization_control)--;
        region = category_balances[*p_categorization_control];
        move16();
        
        power_categories[region] = sub(power_categories[region],1);
        move16();

        total_mlt_bits = sub(total_mlt_bits,region_mlt_bit_counts[region]);
        category = power_categories[region];
        move16();
        
        raw_mlt_ptr = &mlt_coefs[region*REGION_SIZE];
        move16();
        
        temp = sub(category,(NUM_CATEGORIES-1));
        test();
        if (temp < 0)
        {
            region_mlt_bit_counts[region] =
                vector_huffman(category, absolute_region_power_index[region],raw_mlt_ptr,
                           &region_mlt_bits[shl_nocheck(region,2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
            move16();
        }
        total_mlt_bits = add(total_mlt_bits,region_mlt_bit_counts[region]);
        temp = sub(total_mlt_bits,number_of_available_bits);
    }

    /* If too many bits... */
    /* Set up for while loop test */
    temp1 = sub(total_mlt_bits,number_of_available_bits);
    temp2 = sub(*p_categorization_control,sub(num_categorization_control_possibilities,1));
    test();
    test();
    logic16();
    
    while ((temp1 > 0) && (temp2 < 0))
    {
        /* operations for while contitions */
        test();
        test();
        logic16();
        
        region = category_balances[*p_categorization_control];
        move16();
        
        power_categories[region] = add(power_categories[region],1);
        move16();

        total_mlt_bits = sub(total_mlt_bits,region_mlt_bit_counts[region]);
        category = power_categories[region];
        move16();
        
        temp = extract_l(L_mult0(region,REGION_SIZE));
        raw_mlt_ptr = &mlt_coefs[temp];
        move16();
        
        temp = sub(category,(NUM_CATEGORIES-1));
        test();
        if (temp < 0)
        {
            region_mlt_bit_counts[region] =
                vector_huffman(category, absolute_region_power_index[region],raw_mlt_ptr,
                           &region_mlt_bits[shl_nocheck(region,2)]);
        }
        else
        {
            region_mlt_bit_counts[region] = 0;
            move16();
        }
        total_mlt_bits = add(total_mlt_bits,region_mlt_bit_counts[region]);
        (*p_categorization_control)++;
        
        temp1 = sub(total_mlt_bits,number_of_available_bits);
        temp2 = sub(*p_categorization_control,sub(num_categorization_control_possibilities,1));
    }
}

/***************************************************************************
 Function:    vector_huffman

 Syntax:      Word16 vector_huffman(Word16  category,     
                                    Word16  power_index,  
                                    Word16  *raw_mlt_ptr,  
                                    UWord32 *word_ptr)     
                                              
              inputs:     Word16  category
                          Word16  power_index
                          Word16  *raw_mlt_ptr
             
              outputs:    number_of_region_bits
                          *word_ptr
                                      

 Description: Huffman encoding for each region based on category and power_index  

 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    0.03      |     0.03
          -------|--------------|----------------  
            MAX  |    0.04      |     0.04
          -------|--------------|---------------- 

           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    0.03      |     0.03       |     0.03
          -------|--------------|----------------|----------------
            MAX  |    0.04      |     0.04       |     0.04
          -------|--------------|----------------|----------------

***************************************************************************/
Word16 vector_huffman(Word16 category,
                      Word16 power_index,
                      Word16 *raw_mlt_ptr,
                      UWord32 *word_ptr)
{
 

    Word16  inv_of_step_size_times_std_dev;
    Word16  j,n;
    Word16  k;
    Word16  number_of_region_bits;
    Word16  number_of_non_zero;
    Word16  vec_dim;
    Word16  num_vecs;
    Word16  kmax, kmax_plus_one;
    Word16  index,signs_index;
    Word16  *bitcount_table_ptr;
    UWord16 *code_table_ptr;
    Word32  code_bits;
    Word16  number_of_code_bits;
    UWord32 current_word;
    Word16  current_word_bits_free;
    
    Word32 acca;
    Word32 accb;
    Word16 temp;

    Word16 mytemp;			 /* new variable in Release 1.2 */
    Word16 myacca;			 /* new variable in Release 1.2 */


    /* initialize variables */
    vec_dim = vector_dimension[category];
    move16();

    num_vecs = number_of_vectors[category];
    move16();

    kmax = max_bin[category];
    move16();

    kmax_plus_one = add(kmax,1);
    move16();

    current_word = 0L;
    move16();

    current_word_bits_free = 32;
    move16();

    number_of_region_bits = 0;
    move16();

    /* set up table pointers */
    bitcount_table_ptr = (Word16 *)table_of_bitcount_tables[category];
    code_table_ptr = (UWord16 *) table_of_code_tables[category];

    /* compute inverse of step size * standard deviation */
    acca = L_mult(step_size_inverse_table[category],standard_deviation_inverse_table[power_index]);
    acca = L_shr_nocheck(acca,1);
    acca = L_add(acca,4096);
    acca = L_shr_nocheck(acca,13);

	/*
	 *  The next two lines are new to Release 1.2 
	 */
     
	mytemp = (Word16)(acca & 0x3);
    acca = L_shr_nocheck(acca,2);

    inv_of_step_size_times_std_dev = extract_l(acca);


    for (n=0; n<num_vecs; n++)
    {
        index = 0;
        move16();
        
        signs_index = 0;
        move16();
        
        number_of_non_zero = 0;
        move16();
        
        for (j=0; j<vec_dim; j++)
        {
            k = abs_s(*raw_mlt_ptr);
            
            acca = L_mult(k,inv_of_step_size_times_std_dev);
            acca = L_shr_nocheck(acca,1);
		    
			/*
			 *  The next four lines are new to Release 1.2
			 */

			myacca = (Word16)L_mult(k,mytemp);
			myacca = (Word16)L_shr_nocheck(myacca,1);
			myacca = (Word16)L_add(myacca,int_dead_zone_low_bits[category]);
			myacca = (Word16)L_shr_nocheck(myacca,2);

            acca = L_add(acca,int_dead_zone[category]);

			/*
			 *  The next two lines are new to Release 1.2
			 */

			acca = L_add(acca,myacca);
			acca = L_shr_nocheck(acca,13);

            k = extract_l(acca);

            test();
            if (k != 0)
            {
                number_of_non_zero = add(number_of_non_zero,1);
                signs_index = shl_nocheck(signs_index,1);
                
                test();
                if (*raw_mlt_ptr > 0)
                {
                    signs_index = add(signs_index,1);
                }
                
                temp = sub(k,kmax);
                test();
                if (temp > 0)
                {
                    k = kmax;
                    move16();
                }
            }
            acca = L_shr_nocheck(L_mult(index,(kmax_plus_one)),1);
            index = extract_l(acca);
            index = add(index,k);
            raw_mlt_ptr++;
        }

        code_bits = *(code_table_ptr+index);
        number_of_code_bits = add((*(bitcount_table_ptr+index)),number_of_non_zero);
        number_of_region_bits = add(number_of_region_bits,number_of_code_bits);

        acca = code_bits << number_of_non_zero;
        accb = L_deposit_l(signs_index);
        acca = L_add(acca,accb);
        code_bits = acca;
        move32();

        /* msb of codebits is transmitted first. */
        j = sub(current_word_bits_free,number_of_code_bits);
        test();
        if (j >= 0)
        {
            test();
            acca = code_bits << j;
            current_word = L_add(current_word,acca);
            current_word_bits_free = j;
            move16();
        }
        else
        {
            j = negate(j);
            acca = L_shr_nocheck(code_bits,j);
            current_word = L_add(current_word,acca);
            
            *word_ptr++ = current_word;
            move16();

            current_word_bits_free = sub(32,j);
            test();
            current_word = code_bits << current_word_bits_free;
        }
    }

    *word_ptr++ = current_word;
    move16();

    return (number_of_region_bits);
}


