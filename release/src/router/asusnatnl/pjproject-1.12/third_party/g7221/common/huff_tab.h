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

extern Word16  differential_region_power_bits[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS];
extern UWord16 differential_region_power_codes[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS];
extern Word16  differential_region_power_decoder_tree[MAX_NUMBER_OF_REGIONS][DIFF_REGION_POWER_LEVELS-1][2];
extern Word16  mlt_quant_centroid[NUM_CATEGORIES][MAX_NUM_BINS];
extern Word16  expected_bits_table[NUM_CATEGORIES];
extern Word16  mlt_sqvh_bitcount_category_0[196];
extern UWord16 mlt_sqvh_code_category_0[196];
extern Word16  mlt_sqvh_bitcount_category_1[100];
extern UWord16 mlt_sqvh_code_category_1[100];
extern Word16  mlt_sqvh_bitcount_category_2[49];
extern UWord16 mlt_sqvh_code_category_2[49];
extern Word16  mlt_sqvh_bitcount_category_3[625];
extern UWord16 mlt_sqvh_code_category_3[625];
extern Word16  mlt_sqvh_bitcount_category_4[256];
extern UWord16 mlt_sqvh_code_category_4[256];
extern Word16  mlt_sqvh_bitcount_category_5[243];
extern UWord16 mlt_sqvh_code_category_5[243];
extern Word16  mlt_sqvh_bitcount_category_6[32];
extern UWord16 mlt_sqvh_code_category_6[32];
extern Word16  *table_of_bitcount_tables[NUM_CATEGORIES-1];
extern UWord16 *table_of_code_tables[NUM_CATEGORIES-1];
extern Word16  mlt_decoder_tree_category_0[180][2];
extern Word16  mlt_decoder_tree_category_1[93][2];
extern Word16  mlt_decoder_tree_category_2[47][2];
extern Word16  mlt_decoder_tree_category_3[519][2];
extern Word16  mlt_decoder_tree_category_4[208][2];
extern Word16  mlt_decoder_tree_category_5[191][2];
extern Word16  mlt_decoder_tree_category_6[31][2];
extern Word16  *table_of_decoder_tables[NUM_CATEGORIES-1];

