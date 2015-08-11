
   /******************************************************************

       iLBC Speech Coder ANSI-C Source Code

       constants.h

       Copyright (C) The Internet Society (2004).
       All Rights Reserved.

   ******************************************************************/

   #ifndef __iLBC_CONSTANTS_H
   #define __iLBC_CONSTANTS_H

   #include "iLBC_define.h"


   /* ULP bit allocation */






   extern const iLBC_ULP_Inst_t ULP_20msTbl;
   extern const iLBC_ULP_Inst_t ULP_30msTbl;

   /* high pass filters */

   extern float hpi_zero_coefsTbl[];
   extern float hpi_pole_coefsTbl[];
   extern float hpo_zero_coefsTbl[];
   extern float hpo_pole_coefsTbl[];

   /* low pass filters */
   extern float lpFilt_coefsTbl[];

   /* LPC analysis and quantization */

   extern float lpc_winTbl[];
   extern float lpc_asymwinTbl[];
   extern float lpc_lagwinTbl[];
   extern float lsfCbTbl[];
   extern float lsfmeanTbl[];
   extern int   dim_lsfCbTbl[];
   extern int   size_lsfCbTbl[];
   extern float lsf_weightTbl_30ms[];
   extern float lsf_weightTbl_20ms[];

   /* state quantization tables */

   extern float state_sq3Tbl[];
   extern float state_frgqTbl[];

   /* gain quantization tables */

   extern float gain_sq3Tbl[];
   extern float gain_sq4Tbl[];
   extern float gain_sq5Tbl[];

   /* adaptive codebook definitions */

   extern int search_rangeTbl[5][CB_NSTAGES];
   extern int memLfTbl[];
   extern int stMemLTbl;
   extern float cbfiltersTbl[CB_FILTERLEN];

   /* enhancer definitions */

   extern float polyphaserTbl[];
   extern float enh_plocsTbl[];






   #endif

