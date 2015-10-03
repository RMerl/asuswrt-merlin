/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef BAND_H
#define BAND_H

#include "util.h"

#define BAND_NMAX 10000
#define BAND_ALPHA 45
#define BAND_BETA 2
#define BAND_GAMMA 10
#define BAND_TXC 4
#define BAND_FRAME_TIME 6.667
#define BAND_BLOCK_TIME 300

// Want 6.67 ms per frame. Equals 20/3.
#define BAND_MUL_FRAME(_x) (((_x) * 20 +2) / 3)

/* BAND algorithm state */
typedef struct {
    uint32_t Ni;
    uint32_t r;
    bool_t begun;
} band_t;

#endif /* BAND_H */
