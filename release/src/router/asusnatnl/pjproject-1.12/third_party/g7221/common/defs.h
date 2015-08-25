/***********************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**   All rights reserved.
**
***********************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "g7221/common/typedef.h"
#include "g7221/common/basic_op.h"

#define  PI             3.141592653589793238462

#define MAX_DCT_LENGTH      640
#define DCT_LENGTH          320
#define DCT_LENGTH_DIV_2    160
#define DCT_LENGTH_DIV_4     80
#define DCT_LENGTH_DIV_8     40
#define DCT_LENGTH_DIV_16    20
#define DCT_LENGTH_DIV_32    10
#define DCT_LENGTH_DIV_64     5


#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)

#define NUM_CATEGORIES                  8
#define NUM_CATEGORIZATION_CONTROL_BITS           4
#define NUM_CATEGORIZATION_CONTROL_POSSIBILITIES  16
#define CORE_SIZE       10
#define DCT_LENGTH_LOG  6
#define MAX_DCT_LENGTH_LOG  7

/*  region_size = (BLOCK_SIZE * 0.875)/NUM_REGIONS; */
#define NUMBER_OF_REGIONS       14
#define MAX_NUMBER_OF_REGIONS   28
#define REGION_SIZE             20
#define NUMBER_OF_VALID_COEFS   (NUMBER_OF_REGIONS * REGION_SIZE)
#define MAX_NUMBER_OF_VALID_COEFS   (MAX_NUMBER_OF_REGIONS * REGION_SIZE)

#define REGION_POWER_TABLE_SIZE 64
#define REGION_POWER_TABLE_NUM_NEGATIVES 24

#define MAX_NUM_CATEGORIZATION_CONTROL_BITS 5
#define MAX_NUM_CATEGORIZATION_CONTROL_POSSIBILITIES 32

#define ENCODER_SCALE_FACTOR 18318.0

/* The MLT output is incorrectly scaled by the factor
   product of ENCODER_SCALE_FACTOR and sqrt(160.)
   This is now (9/30/96) 1.0/2^(4.5) or 1/22.627.
   In the current implementation this  
   must be an integer power of sqrt(2). The
   integer power is ESF_ADJUSTMENT_TO_RMS_INDEX.
   The -2 is to conform with the range defined in the spec. */

 
#define ESF_ADJUSTMENT_TO_RMS_INDEX (9-2)
 

#define INTERMEDIATE_FILES_FLAG 0

/* Max bit rate is 48000 bits/sec. */
#define MAX_BITS_PER_FRAME 960


/***************************************************************************/
/* Type definitions                                                        */
/***************************************************************************/
typedef struct
{
    Word16 code_bit_count;      /* bit count of the current word */
    Word16 current_word;        /* current word in the bitstream being processed */
    Word16 *code_word_ptr;      /* pointer to the bitstream */
    Word16 number_of_bits_left; /* number of bits left in the current word */
    Word16 next_bit;            /* next bit in the current word */
}Bit_Obj;

typedef struct
{
    Word16 seed0;
    Word16 seed1;
    Word16 seed2;
    Word16 seed3;
}Rand_Obj;

/***************************************************************************/
/* Function definitions                                                    */
/***************************************************************************/
extern Word16  compute_region_powers(Word16  *mlt_coefs,
                             Word16  mag_shift,
                             Word16  *drp_num_bits,
                             UWord16 *drp_code_bits,
                             Word16  *absolute_region_power_index,
                             Word16  number_of_regions);

void    vector_quantize_mlts(Word16 number_of_available_bits,
                          Word16 number_of_regions,
                          Word16 num_categorization_control_possibilities,
                          Word16 *mlt_coefs,
                          Word16 *absolute_region_power_index,
                          Word16 *power_categories,
                          Word16 *category_balances,
                          Word16 *p_categorization_control,
                          Word16 *region_mlt_bit_counts,
                          UWord32 *region_mlt_bits);

Word16  vector_huffman(Word16 category,
                      Word16 power_index,
                      Word16 *raw_mlt_ptr,
                      UWord32 *word_ptr);


void    adjust_abs_region_power_index(Word16 *absolute_region_power_index,Word16 *mlt_coefs,Word16 number_of_regions);

void    bits_to_words(UWord32 *region_mlt_bits,Word16 *region_mlt_bit_counts,
                      Word16 *drp_num_bits,UWord16 *drp_code_bits,Word16 *out_words,
                      Word16 categorization_control, Word16  number_of_regions,
                      Word16  num_categorization_control_bits, Word16 number_of_bits_per_frame);

void    encoder(Word16  number_of_available_bits,
                Word16  number_of_regions,
                Word16  *mlt_coefs,
                Word16  mag_shift,
                Word16  *out_words);

void decoder(Bit_Obj *bitobj,
    	     Rand_Obj *randobj,
             Word16 number_of_regions,
             Word16 *decoder_mlt_coefs,
	         Word16 *p_mag_shift,
	         Word16 *p_old_mag_shift,
	         Word16 *old_decoder_mlt_coefs,
	         Word16 frame_error_flag);

Word16  samples_to_rmlt_coefs(const Word16 *new_samples,Word16 *history,Word16 *coefs,Word16 dct_length);
void rmlt_coefs_to_samples(Word16 *coefs,     
                           Word16 *old_samples,           
                           Word16 *out_samples,           
                           Word16 dct_length,           
                           Word16 mag_shift);

Word16  index_to_array(Word16 index,Word16 *array,Word16 category);
void    categorize(Word16 number_of_available_bits,
                   Word16 number_of_regions,
				   Word16 num_categorization_control_possibilities,
		           Word16 *rms_index,
		           Word16 *power_categories,
		           Word16 *category_balances);

Word16 calc_offset(Word16 *rms_index,Word16 number_of_regions,Word16 available_bits);    
void   compute_raw_pow_categories(Word16 *power_categories,Word16 *rms_index,Word16 number_of_regions,Word16 offset);
void   comp_powercat_and_catbalance(Word16 *power_categories,
                                    Word16 *category_balances,
                                    Word16 *rms_index,
                                    Word16 number_of_available_bits,
                                    Word16 number_of_regions,
                                    Word16 num_categorization_control_possibilities,
                                    Word16 offset);
 
void dct_type_iv_a (Word16 *input,Word16 *output,Word16 dct_length);
void dct_type_iv_s(Word16 *input,Word16 *output,Word16 dct_length);
void decode_envelope(Bit_Obj *bitobj,
                     Word16  number_of_regions,
                     Word16  *decoder_region_standard_deviation,
		             Word16  *absolute_region_power_index,
		             Word16  *p_mag_shift);

void decode_vector_quantized_mlt_indices(Bit_Obj  *bitobj,
                                         Rand_Obj *randobj,
                                         Word16   number_of_regions,
                                         Word16   *decoder_region_standard_deviation,
					                     Word16   *dedecoder_power_categories,
					                     Word16   *dedecoder_mlt_coefs);

void rate_adjust_categories(Word16 categorization_control,
			                Word16 *decoder_power_categories,
			                Word16 *decoder_category_balances);

void get_next_bit(Bit_Obj *bitobj);
Word16 get_rand(Rand_Obj *randobj);

void test_4_frame_errors(Bit_Obj *bitobj,
                         Word16 number_of_regions,
                         Word16 num_categorization_control_possibilities,
                         Word16 *frame_error_flag,
                         Word16 categorization_control,
                         Word16 *absolute_region_power_index);

void error_handling(Word16 number_of_coefs,
                    Word16 number_of_valid_coefs,
                    Word16 *frame_error_flag,
                    Word16 *decoder_mlt_coefs,
                    Word16 *old_decoder_mlt_coefs,
                    Word16 *p_mag_shift,
                    Word16 *p_old_mag_shift);


