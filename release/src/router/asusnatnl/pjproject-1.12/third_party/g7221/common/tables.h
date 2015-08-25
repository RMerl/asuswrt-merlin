/***********************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 1999 PictureTel Coporation
**          Andover, MA, USA  
**
**	    All rights reserved.
**
***********************************************************************/

/***********************************************************************
  Filename:    tables.h    

  Purpose:     Contains table definitions used by G.722.1 Annex C
		
  Design Notes:

***********************************************************************/

/***********************************************************************
 Include files                                                           
***********************************************************************/
#define REGION_POWER_TABLE_SIZE 64
#define NUM_CATEGORIES          8
#define DCT_LENGTH              320
#define MAX_DCT_LENGTH          640

extern Word16 int_region_standard_deviation_table[REGION_POWER_TABLE_SIZE];
extern Word16 standard_deviation_inverse_table[REGION_POWER_TABLE_SIZE];
extern Word16 step_size_inverse_table[NUM_CATEGORIES];
extern Word16 vector_dimension[NUM_CATEGORIES];
extern Word16 number_of_vectors[NUM_CATEGORIES];
/* The last category isn't really coded with scalar quantization. */
extern Word16 max_bin[NUM_CATEGORIES];
extern Word16 max_bin_plus_one_inverse[NUM_CATEGORIES];
extern Word16 int_dead_zone[NUM_CATEGORIES];
extern Word16 samples_to_rmlt_window[DCT_LENGTH];
extern Word16 rmlt_to_samples_window[DCT_LENGTH];

/* Add next line in Release 1.2 */
extern Word16 int_dead_zone_low_bits[NUM_CATEGORIES];

extern Word16 max_samples_to_rmlt_window[MAX_DCT_LENGTH];
extern Word16 max_rmlt_to_samples_window[MAX_DCT_LENGTH];
