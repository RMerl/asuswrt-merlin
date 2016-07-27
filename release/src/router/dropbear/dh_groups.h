#ifndef DROPBEAR_DH_GROUPS_H
#define DROPBEAR_DH_GROUPS_H
#include "options.h"

#if DROPBEAR_DH_GROUP1
#define DH_P_1_LEN 128
extern const unsigned char dh_p_1[DH_P_1_LEN];
#endif

#if DROPBEAR_DH_GROUP14
#define DH_P_14_LEN 256
extern const unsigned char dh_p_14[DH_P_14_LEN];
#endif

#if DROPBEAR_DH_GROUP16
#define DH_P_16_LEN 512
extern const unsigned char dh_p_16[DH_P_16_LEN];
#endif


extern const int DH_G_VAL;


#endif
