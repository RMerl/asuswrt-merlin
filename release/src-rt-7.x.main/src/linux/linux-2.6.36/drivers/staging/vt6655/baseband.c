/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: baseband.c
 *
 * Purpose: Implement functions to access baseband
 *
 * Author: Kyle Hsu
 *
 * Date: Aug.22, 2002
 *
 * Functions:
 *      BBuGetFrameTime        - Calculate data frame transmitting time
 *      BBvCaculateParameter   - Caculate PhyLength, PhyService and Phy Signal parameter for baseband Tx
 *      BBbReadEmbeded         - Embeded read baseband register via MAC
 *      BBbWriteEmbeded        - Embeded write baseband register via MAC
 *      BBbIsRegBitsOn         - Test if baseband register bits on
 *      BBbIsRegBitsOff        - Test if baseband register bits off
 *      BBbVT3253Init          - VIA VT3253 baseband chip init code
 *      BBvReadAllRegs         - Read All Baseband Registers
 *      BBvLoopbackOn          - Turn on BaseBand Loopback mode
 *      BBvLoopbackOff         - Turn off BaseBand Loopback mode
 *
 * Revision History:
 *      06-10-2003 Bryan YC Fan:  Re-write codes to support VT3253 spec.
 *      08-07-2003 Bryan YC Fan:  Add MAXIM2827/2825 and RFMD2959 support.
 *      08-26-2003 Kyle Hsu    :  Modify BBuGetFrameTime() and BBvCaculateParameter().
 *                                cancel the setting of MAC_REG_SOFTPWRCTL on BBbVT3253Init().
 *                                Add the comments.
 *      09-01-2003 Bryan YC Fan:  RF & BB tables updated.
 *                                Modified BBvLoopbackOn & BBvLoopbackOff().
 *
 *
 */

#include "tmacro.h"
#include "tether.h"
#include "mac.h"
#include "baseband.h"
#include "srom.h"
#include "rf.h"

/*---------------------  Static Definitions -------------------------*/
//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

//#define	PLICE_DEBUG

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/

/*---------------------  Static Functions  --------------------------*/

/*---------------------  Export Variables  --------------------------*/

/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/



#define CB_VT3253_INIT_FOR_RFMD 446
unsigned char byVT3253InitTab_RFMD[CB_VT3253_INIT_FOR_RFMD][2] = {
    {0x00, 0x30},
    {0x01, 0x00},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x00},
    {0x06, 0x00},
    {0x07, 0x00},
    {0x08, 0x70},
    {0x09, 0x45},
    {0x0a, 0x2a},
    {0x0b, 0x76},
    {0x0c, 0x00},
    {0x0d, 0x01},
    {0x0e, 0x80},
    {0x0f, 0x00},
    {0x10, 0x00},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0x00},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x18, 0x00},
    {0x19, 0x00},
    {0x1a, 0x00},
    {0x1b, 0x9d},
    {0x1c, 0x05},
    {0x1d, 0x00},
    {0x1e, 0x00},
    {0x1f, 0x00},
    {0x20, 0x00},
    {0x21, 0x00},
    {0x22, 0x00},
    {0x23, 0x00},
    {0x24, 0x00},
    {0x25, 0x4a},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x29, 0x00},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x2c, 0x00},
    {0x2d, 0xa8},
    {0x2e, 0x1a},
    {0x2f, 0x0c},
    {0x30, 0x26},
    {0x31, 0x5b},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0xaa},
    {0x37, 0xaa},
    {0x38, 0xff},
    {0x39, 0xff},
    {0x3a, 0x00},
    {0x3b, 0x00},
    {0x3c, 0x00},
    {0x3d, 0x0d},
    {0x3e, 0x51},
    {0x3f, 0x04},
    {0x40, 0x00},
    {0x41, 0x08},
    {0x42, 0x00},
    {0x43, 0x08},
    {0x44, 0x06},
    {0x45, 0x14},
    {0x46, 0x05},
    {0x47, 0x08},
    {0x48, 0x00},
    {0x49, 0x00},
    {0x4a, 0x00},
    {0x4b, 0x00},
    {0x4c, 0x09},
    {0x4d, 0x80},
    {0x4e, 0x00},
    {0x4f, 0xc5},
    {0x50, 0x14},
    {0x51, 0x19},
    {0x52, 0x00},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x56, 0x00},
    {0x57, 0x00},
    {0x58, 0x00},
    {0x59, 0xb0},
    {0x5a, 0x00},
    {0x5b, 0x00},
    {0x5c, 0x00},
    {0x5d, 0x00},
    {0x5e, 0x00},
    {0x5f, 0x00},
    {0x60, 0x44},
    {0x61, 0x04},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0x04},
    {0x67, 0xb7},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6a, 0x00},
    {0x6b, 0x00},
    {0x6c, 0x00},
    {0x6d, 0x03},
    {0x6e, 0x01},
    {0x6f, 0x00},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x73, 0x00},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x00},
    {0x79, 0x00},
    {0x7a, 0x00},
    {0x7b, 0x00},
    {0x7c, 0x00},
    {0x7d, 0x00},
    {0x7e, 0x00},
    {0x7f, 0x00},
    {0x80, 0x0b},
    {0x81, 0x00},
    {0x82, 0x3c},
    {0x83, 0x00},
    {0x84, 0x00},
    {0x85, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x08},
    {0x89, 0x00},
    {0x8a, 0x08},
    {0x8b, 0xa6},
    {0x8c, 0x84},
    {0x8d, 0x47},
    {0x8e, 0xbb},
    {0x8f, 0x02},
    {0x90, 0x21},
    {0x91, 0x0c},
    {0x92, 0x04},
    {0x93, 0x22},
    {0x94, 0x00},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0xeb},
    {0x98, 0x00},
    {0x99, 0x00},
    {0x9a, 0x00},
    {0x9b, 0x00},
    {0x9c, 0x00},
    {0x9d, 0x00},
    {0x9e, 0x00},
    {0x9f, 0x00},
    {0xa0, 0x00},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0xa3, 0x00},
    {0xa4, 0x00},
    {0xa5, 0x00},
    {0xa6, 0x10},
    {0xa7, 0x04},
    {0xa8, 0x10},
    {0xa9, 0x00},
    {0xaa, 0x8f},
    {0xab, 0x00},
    {0xac, 0x00},
    {0xad, 0x00},
    {0xae, 0x00},
    {0xaf, 0x80},
    {0xb0, 0x38},
    {0xb1, 0x00},
    {0xb2, 0x00},
    {0xb3, 0x00},
    {0xb4, 0xee},
    {0xb5, 0xff},
    {0xb6, 0x10},
    {0xb7, 0x00},
    {0xb8, 0x00},
    {0xb9, 0x00},
    {0xba, 0x00},
    {0xbb, 0x03},
    {0xbc, 0x00},
    {0xbd, 0x00},
    {0xbe, 0x00},
    {0xbf, 0x00},
    {0xc0, 0x10},
    {0xc1, 0x10},
    {0xc2, 0x18},
    {0xc3, 0x20},
    {0xc4, 0x10},
    {0xc5, 0x00},
    {0xc6, 0x22},
    {0xc7, 0x14},
    {0xc8, 0x0f},
    {0xc9, 0x08},
    {0xca, 0xa4},
    {0xcb, 0xa7},
    {0xcc, 0x3c},
    {0xcd, 0x10},
    {0xce, 0x20},
    {0xcf, 0x00},
    {0xd0, 0x00},
    {0xd1, 0x10},
    {0xd2, 0x00},
    {0xd3, 0x00},
    {0xd4, 0x10},
    {0xd5, 0x33},
    {0xd6, 0x70},
    {0xd7, 0x01},
    {0xd8, 0x00},
    {0xd9, 0x00},
    {0xda, 0x00},
    {0xdb, 0x00},
    {0xdc, 0x00},
    {0xdd, 0x00},
    {0xde, 0x00},
    {0xdf, 0x00},
    {0xe0, 0x00},
    {0xe1, 0x00},
    {0xe2, 0xcc},
    {0xe3, 0x04},
    {0xe4, 0x08},
    {0xe5, 0x10},
    {0xe6, 0x00},
    {0xe7, 0x0e},
    {0xe8, 0x88},
    {0xe9, 0xd4},
    {0xea, 0x05},
    {0xeb, 0xf0},
    {0xec, 0x79},
    {0xed, 0x0f},
    {0xee, 0x04},
    {0xef, 0x04},
    {0xf0, 0x00},
    {0xf1, 0x00},
    {0xf2, 0x00},
    {0xf3, 0x00},
    {0xf4, 0x00},
    {0xf5, 0x00},
    {0xf6, 0x00},
    {0xf7, 0x00},
    {0xf8, 0x00},
    {0xf9, 0x00},
    {0xF0, 0x00},
    {0xF1, 0xF8},
    {0xF0, 0x80},
    {0xF0, 0x00},
    {0xF1, 0xF4},
    {0xF0, 0x81},
    {0xF0, 0x01},
    {0xF1, 0xF0},
    {0xF0, 0x82},
    {0xF0, 0x02},
    {0xF1, 0xEC},
    {0xF0, 0x83},
    {0xF0, 0x03},
    {0xF1, 0xE8},
    {0xF0, 0x84},
    {0xF0, 0x04},
    {0xF1, 0xE4},
    {0xF0, 0x85},
    {0xF0, 0x05},
    {0xF1, 0xE0},
    {0xF0, 0x86},
    {0xF0, 0x06},
    {0xF1, 0xDC},
    {0xF0, 0x87},
    {0xF0, 0x07},
    {0xF1, 0xD8},
    {0xF0, 0x88},
    {0xF0, 0x08},
    {0xF1, 0xD4},
    {0xF0, 0x89},
    {0xF0, 0x09},
    {0xF1, 0xD0},
    {0xF0, 0x8A},
    {0xF0, 0x0A},
    {0xF1, 0xCC},
    {0xF0, 0x8B},
    {0xF0, 0x0B},
    {0xF1, 0xC8},
    {0xF0, 0x8C},
    {0xF0, 0x0C},
    {0xF1, 0xC4},
    {0xF0, 0x8D},
    {0xF0, 0x0D},
    {0xF1, 0xC0},
    {0xF0, 0x8E},
    {0xF0, 0x0E},
    {0xF1, 0xBC},
    {0xF0, 0x8F},
    {0xF0, 0x0F},
    {0xF1, 0xB8},
    {0xF0, 0x90},
    {0xF0, 0x10},
    {0xF1, 0xB4},
    {0xF0, 0x91},
    {0xF0, 0x11},
    {0xF1, 0xB0},
    {0xF0, 0x92},
    {0xF0, 0x12},
    {0xF1, 0xAC},
    {0xF0, 0x93},
    {0xF0, 0x13},
    {0xF1, 0xA8},
    {0xF0, 0x94},
    {0xF0, 0x14},
    {0xF1, 0xA4},
    {0xF0, 0x95},
    {0xF0, 0x15},
    {0xF1, 0xA0},
    {0xF0, 0x96},
    {0xF0, 0x16},
    {0xF1, 0x9C},
    {0xF0, 0x97},
    {0xF0, 0x17},
    {0xF1, 0x98},
    {0xF0, 0x98},
    {0xF0, 0x18},
    {0xF1, 0x94},
    {0xF0, 0x99},
    {0xF0, 0x19},
    {0xF1, 0x90},
    {0xF0, 0x9A},
    {0xF0, 0x1A},
    {0xF1, 0x8C},
    {0xF0, 0x9B},
    {0xF0, 0x1B},
    {0xF1, 0x88},
    {0xF0, 0x9C},
    {0xF0, 0x1C},
    {0xF1, 0x84},
    {0xF0, 0x9D},
    {0xF0, 0x1D},
    {0xF1, 0x80},
    {0xF0, 0x9E},
    {0xF0, 0x1E},
    {0xF1, 0x7C},
    {0xF0, 0x9F},
    {0xF0, 0x1F},
    {0xF1, 0x78},
    {0xF0, 0xA0},
    {0xF0, 0x20},
    {0xF1, 0x74},
    {0xF0, 0xA1},
    {0xF0, 0x21},
    {0xF1, 0x70},
    {0xF0, 0xA2},
    {0xF0, 0x22},
    {0xF1, 0x6C},
    {0xF0, 0xA3},
    {0xF0, 0x23},
    {0xF1, 0x68},
    {0xF0, 0xA4},
    {0xF0, 0x24},
    {0xF1, 0x64},
    {0xF0, 0xA5},
    {0xF0, 0x25},
    {0xF1, 0x60},
    {0xF0, 0xA6},
    {0xF0, 0x26},
    {0xF1, 0x5C},
    {0xF0, 0xA7},
    {0xF0, 0x27},
    {0xF1, 0x58},
    {0xF0, 0xA8},
    {0xF0, 0x28},
    {0xF1, 0x54},
    {0xF0, 0xA9},
    {0xF0, 0x29},
    {0xF1, 0x50},
    {0xF0, 0xAA},
    {0xF0, 0x2A},
    {0xF1, 0x4C},
    {0xF0, 0xAB},
    {0xF0, 0x2B},
    {0xF1, 0x48},
    {0xF0, 0xAC},
    {0xF0, 0x2C},
    {0xF1, 0x44},
    {0xF0, 0xAD},
    {0xF0, 0x2D},
    {0xF1, 0x40},
    {0xF0, 0xAE},
    {0xF0, 0x2E},
    {0xF1, 0x3C},
    {0xF0, 0xAF},
    {0xF0, 0x2F},
    {0xF1, 0x38},
    {0xF0, 0xB0},
    {0xF0, 0x30},
    {0xF1, 0x34},
    {0xF0, 0xB1},
    {0xF0, 0x31},
    {0xF1, 0x30},
    {0xF0, 0xB2},
    {0xF0, 0x32},
    {0xF1, 0x2C},
    {0xF0, 0xB3},
    {0xF0, 0x33},
    {0xF1, 0x28},
    {0xF0, 0xB4},
    {0xF0, 0x34},
    {0xF1, 0x24},
    {0xF0, 0xB5},
    {0xF0, 0x35},
    {0xF1, 0x20},
    {0xF0, 0xB6},
    {0xF0, 0x36},
    {0xF1, 0x1C},
    {0xF0, 0xB7},
    {0xF0, 0x37},
    {0xF1, 0x18},
    {0xF0, 0xB8},
    {0xF0, 0x38},
    {0xF1, 0x14},
    {0xF0, 0xB9},
    {0xF0, 0x39},
    {0xF1, 0x10},
    {0xF0, 0xBA},
    {0xF0, 0x3A},
    {0xF1, 0x0C},
    {0xF0, 0xBB},
    {0xF0, 0x3B},
    {0xF1, 0x08},
    {0xF0, 0x00},
    {0xF0, 0x3C},
    {0xF1, 0x04},
    {0xF0, 0xBD},
    {0xF0, 0x3D},
    {0xF1, 0x00},
    {0xF0, 0xBE},
    {0xF0, 0x3E},
    {0xF1, 0x00},
    {0xF0, 0xBF},
    {0xF0, 0x3F},
    {0xF1, 0x00},
    {0xF0, 0xC0},
    {0xF0, 0x00},
};

#define CB_VT3253B0_INIT_FOR_RFMD 256
unsigned char byVT3253B0_RFMD[CB_VT3253B0_INIT_FOR_RFMD][2] = {
    {0x00, 0x31},
    {0x01, 0x00},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x81},
    {0x06, 0x00},
    {0x07, 0x00},
    {0x08, 0x38},
    {0x09, 0x45},
    {0x0a, 0x2a},
    {0x0b, 0x76},
    {0x0c, 0x00},
    {0x0d, 0x00},
    {0x0e, 0x80},
    {0x0f, 0x00},
    {0x10, 0x00},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0x00},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x18, 0x00},
    {0x19, 0x00},
    {0x1a, 0x00},
    {0x1b, 0x8e},
    {0x1c, 0x06},
    {0x1d, 0x00},
    {0x1e, 0x00},
    {0x1f, 0x00},
    {0x20, 0x00},
    {0x21, 0x00},
    {0x22, 0x00},
    {0x23, 0x00},
    {0x24, 0x00},
    {0x25, 0x4a},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x29, 0x00},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x2c, 0x00},
    {0x2d, 0x34},
    {0x2e, 0x18},
    {0x2f, 0x0c},
    {0x30, 0x26},
    {0x31, 0x5b},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0xaa},
    {0x37, 0xaa},
    {0x38, 0xff},
    {0x39, 0xff},
    {0x3a, 0xf8},
    {0x3b, 0x00},
    {0x3c, 0x00},
    {0x3d, 0x09},
    {0x3e, 0x0d},
    {0x3f, 0x04},
    {0x40, 0x00},
    {0x41, 0x08},
    {0x42, 0x00},
    {0x43, 0x08},
    {0x44, 0x08},
    {0x45, 0x14},
    {0x46, 0x05},
    {0x47, 0x08},
    {0x48, 0x00},
    {0x49, 0x00},
    {0x4a, 0x00},
    {0x4b, 0x00},
    {0x4c, 0x09},
    {0x4d, 0x80},
    {0x4e, 0x00},
    {0x4f, 0xc5},
    {0x50, 0x14},
    {0x51, 0x19},
    {0x52, 0x00},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x56, 0x00},
    {0x57, 0x00},
    {0x58, 0x00},
    {0x59, 0xb0},
    {0x5a, 0x00},
    {0x5b, 0x00},
    {0x5c, 0x00},
    {0x5d, 0x00},
    {0x5e, 0x00},
    {0x5f, 0x00},
    {0x60, 0x39},
    {0x61, 0x83},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0xc0},
    {0x67, 0x49},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6a, 0x00},
    {0x6b, 0x00},
    {0x6c, 0x00},
    {0x6d, 0x03},
    {0x6e, 0x01},
    {0x6f, 0x00},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x73, 0x00},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x00},
    {0x79, 0x00},
    {0x7a, 0x00},
    {0x7b, 0x00},
    {0x7c, 0x00},
    {0x7d, 0x00},
    {0x7e, 0x00},
    {0x7f, 0x00},
    {0x80, 0x89},
    {0x81, 0x00},
    {0x82, 0x0e},
    {0x83, 0x00},
    {0x84, 0x00},
    {0x85, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x08},
    {0x89, 0x00},
    {0x8a, 0x0e},
    {0x8b, 0xa7},
    {0x8c, 0x88},
    {0x8d, 0x47},
    {0x8e, 0xaa},
    {0x8f, 0x02},
    {0x90, 0x23},
    {0x91, 0x0c},
    {0x92, 0x06},
    {0x93, 0x08},
    {0x94, 0x00},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0xeb},
    {0x98, 0x00},
    {0x99, 0x00},
    {0x9a, 0x00},
    {0x9b, 0x00},
    {0x9c, 0x00},
    {0x9d, 0x00},
    {0x9e, 0x00},
    {0x9f, 0x00},
    {0xa0, 0x00},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0xa3, 0xcd},
    {0xa4, 0x07},
    {0xa5, 0x33},
    {0xa6, 0x18},
    {0xa7, 0x00},
    {0xa8, 0x18},
    {0xa9, 0x00},
    {0xaa, 0x28},
    {0xab, 0x00},
    {0xac, 0x00},
    {0xad, 0x00},
    {0xae, 0x00},
    {0xaf, 0x18},
    {0xb0, 0x38},
    {0xb1, 0x30},
    {0xb2, 0x00},
    {0xb3, 0x00},
    {0xb4, 0x00},
    {0xb5, 0x00},
    {0xb6, 0x84},
    {0xb7, 0xfd},
    {0xb8, 0x00},
    {0xb9, 0x00},
    {0xba, 0x00},
    {0xbb, 0x03},
    {0xbc, 0x00},
    {0xbd, 0x00},
    {0xbe, 0x00},
    {0xbf, 0x00},
    {0xc0, 0x10},
    {0xc1, 0x20},
    {0xc2, 0x18},
    {0xc3, 0x20},
    {0xc4, 0x10},
    {0xc5, 0x2c},
    {0xc6, 0x1e},
    {0xc7, 0x10},
    {0xc8, 0x12},
    {0xc9, 0x01},
    {0xca, 0x6f},
    {0xcb, 0xa7},
    {0xcc, 0x3c},
    {0xcd, 0x10},
    {0xce, 0x00},
    {0xcf, 0x22},
    {0xd0, 0x00},
    {0xd1, 0x10},
    {0xd2, 0x00},
    {0xd3, 0x00},
    {0xd4, 0x10},
    {0xd5, 0x33},
    {0xd6, 0x80},
    {0xd7, 0x21},
    {0xd8, 0x00},
    {0xd9, 0x00},
    {0xda, 0x00},
    {0xdb, 0x00},
    {0xdc, 0x00},
    {0xdd, 0x00},
    {0xde, 0x00},
    {0xdf, 0x00},
    {0xe0, 0x00},
    {0xe1, 0xB3},
    {0xe2, 0x00},
    {0xe3, 0x00},
    {0xe4, 0x00},
    {0xe5, 0x10},
    {0xe6, 0x00},
    {0xe7, 0x18},
    {0xe8, 0x08},
    {0xe9, 0xd4},
    {0xea, 0x00},
    {0xeb, 0xff},
    {0xec, 0x79},
    {0xed, 0x10},
    {0xee, 0x30},
    {0xef, 0x02},
    {0xf0, 0x00},
    {0xf1, 0x09},
    {0xf2, 0x00},
    {0xf3, 0x00},
    {0xf4, 0x00},
    {0xf5, 0x00},
    {0xf6, 0x00},
    {0xf7, 0x00},
    {0xf8, 0x00},
    {0xf9, 0x00},
    {0xfa, 0x00},
    {0xfb, 0x00},
    {0xfc, 0x00},
    {0xfd, 0x00},
    {0xfe, 0x00},
    {0xff, 0x00},
};

#define CB_VT3253B0_AGC_FOR_RFMD2959 195
// For RFMD2959
unsigned char byVT3253B0_AGC4_RFMD2959[CB_VT3253B0_AGC_FOR_RFMD2959][2] = {
    {0xF0, 0x00},
    {0xF1, 0x3E},
    {0xF0, 0x80},
    {0xF0, 0x00},
    {0xF1, 0x3E},
    {0xF0, 0x81},
    {0xF0, 0x01},
    {0xF1, 0x3E},
    {0xF0, 0x82},
    {0xF0, 0x02},
    {0xF1, 0x3E},
    {0xF0, 0x83},
    {0xF0, 0x03},
    {0xF1, 0x3B},
    {0xF0, 0x84},
    {0xF0, 0x04},
    {0xF1, 0x39},
    {0xF0, 0x85},
    {0xF0, 0x05},
    {0xF1, 0x38},
    {0xF0, 0x86},
    {0xF0, 0x06},
    {0xF1, 0x37},
    {0xF0, 0x87},
    {0xF0, 0x07},
    {0xF1, 0x36},
    {0xF0, 0x88},
    {0xF0, 0x08},
    {0xF1, 0x35},
    {0xF0, 0x89},
    {0xF0, 0x09},
    {0xF1, 0x35},
    {0xF0, 0x8A},
    {0xF0, 0x0A},
    {0xF1, 0x34},
    {0xF0, 0x8B},
    {0xF0, 0x0B},
    {0xF1, 0x34},
    {0xF0, 0x8C},
    {0xF0, 0x0C},
    {0xF1, 0x33},
    {0xF0, 0x8D},
    {0xF0, 0x0D},
    {0xF1, 0x32},
    {0xF0, 0x8E},
    {0xF0, 0x0E},
    {0xF1, 0x31},
    {0xF0, 0x8F},
    {0xF0, 0x0F},
    {0xF1, 0x30},
    {0xF0, 0x90},
    {0xF0, 0x10},
    {0xF1, 0x2F},
    {0xF0, 0x91},
    {0xF0, 0x11},
    {0xF1, 0x2F},
    {0xF0, 0x92},
    {0xF0, 0x12},
    {0xF1, 0x2E},
    {0xF0, 0x93},
    {0xF0, 0x13},
    {0xF1, 0x2D},
    {0xF0, 0x94},
    {0xF0, 0x14},
    {0xF1, 0x2C},
    {0xF0, 0x95},
    {0xF0, 0x15},
    {0xF1, 0x2B},
    {0xF0, 0x96},
    {0xF0, 0x16},
    {0xF1, 0x2B},
    {0xF0, 0x97},
    {0xF0, 0x17},
    {0xF1, 0x2A},
    {0xF0, 0x98},
    {0xF0, 0x18},
    {0xF1, 0x29},
    {0xF0, 0x99},
    {0xF0, 0x19},
    {0xF1, 0x28},
    {0xF0, 0x9A},
    {0xF0, 0x1A},
    {0xF1, 0x27},
    {0xF0, 0x9B},
    {0xF0, 0x1B},
    {0xF1, 0x26},
    {0xF0, 0x9C},
    {0xF0, 0x1C},
    {0xF1, 0x25},
    {0xF0, 0x9D},
    {0xF0, 0x1D},
    {0xF1, 0x24},
    {0xF0, 0x9E},
    {0xF0, 0x1E},
    {0xF1, 0x24},
    {0xF0, 0x9F},
    {0xF0, 0x1F},
    {0xF1, 0x23},
    {0xF0, 0xA0},
    {0xF0, 0x20},
    {0xF1, 0x22},
    {0xF0, 0xA1},
    {0xF0, 0x21},
    {0xF1, 0x21},
    {0xF0, 0xA2},
    {0xF0, 0x22},
    {0xF1, 0x20},
    {0xF0, 0xA3},
    {0xF0, 0x23},
    {0xF1, 0x20},
    {0xF0, 0xA4},
    {0xF0, 0x24},
    {0xF1, 0x1F},
    {0xF0, 0xA5},
    {0xF0, 0x25},
    {0xF1, 0x1E},
    {0xF0, 0xA6},
    {0xF0, 0x26},
    {0xF1, 0x1D},
    {0xF0, 0xA7},
    {0xF0, 0x27},
    {0xF1, 0x1C},
    {0xF0, 0xA8},
    {0xF0, 0x28},
    {0xF1, 0x1B},
    {0xF0, 0xA9},
    {0xF0, 0x29},
    {0xF1, 0x1B},
    {0xF0, 0xAA},
    {0xF0, 0x2A},
    {0xF1, 0x1A},
    {0xF0, 0xAB},
    {0xF0, 0x2B},
    {0xF1, 0x1A},
    {0xF0, 0xAC},
    {0xF0, 0x2C},
    {0xF1, 0x19},
    {0xF0, 0xAD},
    {0xF0, 0x2D},
    {0xF1, 0x18},
    {0xF0, 0xAE},
    {0xF0, 0x2E},
    {0xF1, 0x17},
    {0xF0, 0xAF},
    {0xF0, 0x2F},
    {0xF1, 0x16},
    {0xF0, 0xB0},
    {0xF0, 0x30},
    {0xF1, 0x15},
    {0xF0, 0xB1},
    {0xF0, 0x31},
    {0xF1, 0x15},
    {0xF0, 0xB2},
    {0xF0, 0x32},
    {0xF1, 0x15},
    {0xF0, 0xB3},
    {0xF0, 0x33},
    {0xF1, 0x14},
    {0xF0, 0xB4},
    {0xF0, 0x34},
    {0xF1, 0x13},
    {0xF0, 0xB5},
    {0xF0, 0x35},
    {0xF1, 0x12},
    {0xF0, 0xB6},
    {0xF0, 0x36},
    {0xF1, 0x11},
    {0xF0, 0xB7},
    {0xF0, 0x37},
    {0xF1, 0x10},
    {0xF0, 0xB8},
    {0xF0, 0x38},
    {0xF1, 0x0F},
    {0xF0, 0xB9},
    {0xF0, 0x39},
    {0xF1, 0x0E},
    {0xF0, 0xBA},
    {0xF0, 0x3A},
    {0xF1, 0x0D},
    {0xF0, 0xBB},
    {0xF0, 0x3B},
    {0xF1, 0x0C},
    {0xF0, 0xBC},
    {0xF0, 0x3C},
    {0xF1, 0x0B},
    {0xF0, 0xBD},
    {0xF0, 0x3D},
    {0xF1, 0x0B},
    {0xF0, 0xBE},
    {0xF0, 0x3E},
    {0xF1, 0x0A},
    {0xF0, 0xBF},
    {0xF0, 0x3F},
    {0xF1, 0x09},
    {0xF0, 0x00},
};

#define CB_VT3253B0_INIT_FOR_AIROHA2230 256
// For AIROHA
unsigned char byVT3253B0_AIROHA2230[CB_VT3253B0_INIT_FOR_AIROHA2230][2] = {
    {0x00, 0x31},
    {0x01, 0x00},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x80},
    {0x06, 0x00},
    {0x07, 0x00},
    {0x08, 0x70},
    {0x09, 0x41},
    {0x0a, 0x2A},
    {0x0b, 0x76},
    {0x0c, 0x00},
    {0x0d, 0x00},
    {0x0e, 0x80},
    {0x0f, 0x00},
    {0x10, 0x00},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0x00},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x18, 0x00},
    {0x19, 0x00},
    {0x1a, 0x00},
    {0x1b, 0x8f},
    {0x1c, 0x09},
    {0x1d, 0x00},
    {0x1e, 0x00},
    {0x1f, 0x00},
    {0x20, 0x00},
    {0x21, 0x00},
    {0x22, 0x00},
    {0x23, 0x00},
    {0x24, 0x00},
    {0x25, 0x4a},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x29, 0x00},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x2c, 0x00},
    {0x2d, 0x4a},
    {0x2e, 0x00},
    {0x2f, 0x0a},
    {0x30, 0x26},
    {0x31, 0x5b},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0xaa},
    {0x37, 0xaa},
    {0x38, 0xff},
    {0x39, 0xff},
    {0x3a, 0x79},
    {0x3b, 0x00},
    {0x3c, 0x00},
    {0x3d, 0x0b},
    {0x3e, 0x48},
    {0x3f, 0x04},
    {0x40, 0x00},
    {0x41, 0x08},
    {0x42, 0x00},
    {0x43, 0x08},
    {0x44, 0x08},
    {0x45, 0x14},
    {0x46, 0x05},
    {0x47, 0x09},
    {0x48, 0x00},
    {0x49, 0x00},
    {0x4a, 0x00},
    {0x4b, 0x00},
    {0x4c, 0x09},
    {0x4d, 0x73},
    {0x4e, 0x00},
    {0x4f, 0xc5},
    {0x50, 0x15},
    {0x51, 0x19},
    {0x52, 0x00},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x56, 0x00},
    {0x57, 0x00},
    {0x58, 0x00},
    {0x59, 0xb0},
    {0x5a, 0x00},
    {0x5b, 0x00},
    {0x5c, 0x00},
    {0x5d, 0x00},
    {0x5e, 0x00},
    {0x5f, 0x00},
    {0x60, 0xe4},
    {0x61, 0x80},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0x98},
    {0x67, 0x0a},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6a, 0x00},
    {0x6b, 0x00},
    //{0x6c, 0x80},
    {0x6c, 0x00}, //RobertYu:20050125, request by JJSue
    {0x6d, 0x03},
    {0x6e, 0x01},
    {0x6f, 0x00},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x73, 0x00},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x00},
    {0x79, 0x00},
    {0x7a, 0x00},
    {0x7b, 0x00},
    {0x7c, 0x00},
    {0x7d, 0x00},
    {0x7e, 0x00},
    {0x7f, 0x00},
    {0x80, 0x8c},
    {0x81, 0x01},
    {0x82, 0x09},
    {0x83, 0x00},
    {0x84, 0x00},
    {0x85, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x08},
    {0x89, 0x00},
    {0x8a, 0x0f},
    {0x8b, 0xb7},
    {0x8c, 0x88},
    {0x8d, 0x47},
    {0x8e, 0xaa},
    {0x8f, 0x02},
    {0x90, 0x22},
    {0x91, 0x00},
    {0x92, 0x00},
    {0x93, 0x00},
    {0x94, 0x00},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0xeb},
    {0x98, 0x00},
    {0x99, 0x00},
    {0x9a, 0x00},
    {0x9b, 0x00},
    {0x9c, 0x00},
    {0x9d, 0x00},
    {0x9e, 0x00},
    {0x9f, 0x01},
    {0xa0, 0x00},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0xa3, 0x00},
    {0xa4, 0x00},
    {0xa5, 0x00},
    {0xa6, 0x10},
    {0xa7, 0x00},
    {0xa8, 0x18},
    {0xa9, 0x00},
    {0xaa, 0x00},
    {0xab, 0x00},
    {0xac, 0x00},
    {0xad, 0x00},
    {0xae, 0x00},
    {0xaf, 0x18},
    {0xb0, 0x38},
    {0xb1, 0x30},
    {0xb2, 0x00},
    {0xb3, 0x00},
    {0xb4, 0xff},
    {0xb5, 0x0f},
    {0xb6, 0xe4},
    {0xb7, 0xe2},
    {0xb8, 0x00},
    {0xb9, 0x00},
    {0xba, 0x00},
    {0xbb, 0x03},
    {0xbc, 0x01},
    {0xbd, 0x00},
    {0xbe, 0x00},
    {0xbf, 0x00},
    {0xc0, 0x18},
    {0xc1, 0x20},
    {0xc2, 0x07},
    {0xc3, 0x18},
    {0xc4, 0xff},
    {0xc5, 0x2c},
    {0xc6, 0x0c},
    {0xc7, 0x0a},
    {0xc8, 0x0e},
    {0xc9, 0x01},
    {0xca, 0x68},
    {0xcb, 0xa7},
    {0xcc, 0x3c},
    {0xcd, 0x10},
    {0xce, 0x00},
    {0xcf, 0x25},
    {0xd0, 0x40},
    {0xd1, 0x12},
    {0xd2, 0x00},
    {0xd3, 0x00},
    {0xd4, 0x10},
    {0xd5, 0x28},
    {0xd6, 0x80},
    {0xd7, 0x2A},
    {0xd8, 0x00},
    {0xd9, 0x00},
    {0xda, 0x00},
    {0xdb, 0x00},
    {0xdc, 0x00},
    {0xdd, 0x00},
    {0xde, 0x00},
    {0xdf, 0x00},
    {0xe0, 0x00},
    {0xe1, 0xB3},
    {0xe2, 0x00},
    {0xe3, 0x00},
    {0xe4, 0x00},
    {0xe5, 0x10},
    {0xe6, 0x00},
    {0xe7, 0x1C},
    {0xe8, 0x00},
    {0xe9, 0xf4},
    {0xea, 0x00},
    {0xeb, 0xff},
    {0xec, 0x79},
    {0xed, 0x20},
    {0xee, 0x30},
    {0xef, 0x01},
    {0xf0, 0x00},
    {0xf1, 0x3e},
    {0xf2, 0x00},
    {0xf3, 0x00},
    {0xf4, 0x00},
    {0xf5, 0x00},
    {0xf6, 0x00},
    {0xf7, 0x00},
    {0xf8, 0x00},
    {0xf9, 0x00},
    {0xfa, 0x00},
    {0xfb, 0x00},
    {0xfc, 0x00},
    {0xfd, 0x00},
    {0xfe, 0x00},
    {0xff, 0x00},
};



#define CB_VT3253B0_INIT_FOR_UW2451 256
//For UW2451
unsigned char byVT3253B0_UW2451[CB_VT3253B0_INIT_FOR_UW2451][2] = {
    {0x00, 0x31},
    {0x01, 0x00},
    {0x02, 0x00},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x81},
    {0x06, 0x00},
    {0x07, 0x00},
    {0x08, 0x38},
    {0x09, 0x45},
    {0x0a, 0x28},
    {0x0b, 0x76},
    {0x0c, 0x00},
    {0x0d, 0x00},
    {0x0e, 0x80},
    {0x0f, 0x00},
    {0x10, 0x00},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0x00},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x18, 0x00},
    {0x19, 0x00},
    {0x1a, 0x00},
    {0x1b, 0x8f},
    {0x1c, 0x0f},
    {0x1d, 0x00},
    {0x1e, 0x00},
    {0x1f, 0x00},
    {0x20, 0x00},
    {0x21, 0x00},
    {0x22, 0x00},
    {0x23, 0x00},
    {0x24, 0x00},
    {0x25, 0x4a},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x29, 0x00},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x2c, 0x00},
    {0x2d, 0x18},
    {0x2e, 0x00},
    {0x2f, 0x0a},
    {0x30, 0x26},
    {0x31, 0x5b},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0xaa},
    {0x37, 0xaa},
    {0x38, 0xff},
    {0x39, 0xff},
    {0x3a, 0x00},
    {0x3b, 0x00},
    {0x3c, 0x00},
    {0x3d, 0x03},
    {0x3e, 0x1d},
    {0x3f, 0x04},
    {0x40, 0x00},
    {0x41, 0x08},
    {0x42, 0x00},
    {0x43, 0x08},
    {0x44, 0x08},
    {0x45, 0x14},
    {0x46, 0x05},
    {0x47, 0x09},
    {0x48, 0x00},
    {0x49, 0x00},
    {0x4a, 0x00},
    {0x4b, 0x00},
    {0x4c, 0x09},
    {0x4d, 0x90},
    {0x4e, 0x00},
    {0x4f, 0xc5},
    {0x50, 0x15},
    {0x51, 0x19},
    {0x52, 0x00},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x56, 0x00},
    {0x57, 0x00},
    {0x58, 0x00},
    {0x59, 0xb0},
    {0x5a, 0x00},
    {0x5b, 0x00},
    {0x5c, 0x00},
    {0x5d, 0x00},
    {0x5e, 0x00},
    {0x5f, 0x00},
    {0x60, 0xb3},
    {0x61, 0x81},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0x57},
    {0x67, 0x6c},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6a, 0x00},
    {0x6b, 0x00},
    //{0x6c, 0x80},
    {0x6c, 0x00}, //RobertYu:20050125, request by JJSue
    {0x6d, 0x03},
    {0x6e, 0x01},
    {0x6f, 0x00},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x73, 0x00},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x00},
    {0x79, 0x00},
    {0x7a, 0x00},
    {0x7b, 0x00},
    {0x7c, 0x00},
    {0x7d, 0x00},
    {0x7e, 0x00},
    {0x7f, 0x00},
    {0x80, 0x8c},
    {0x81, 0x00},
    {0x82, 0x0e},
    {0x83, 0x00},
    {0x84, 0x00},
    {0x85, 0x00},
    {0x86, 0x00},
    {0x87, 0x00},
    {0x88, 0x08},
    {0x89, 0x00},
    {0x8a, 0x0e},
    {0x8b, 0xa7},
    {0x8c, 0x88},
    {0x8d, 0x47},
    {0x8e, 0xaa},
    {0x8f, 0x02},
    {0x90, 0x00},
    {0x91, 0x00},
    {0x92, 0x00},
    {0x93, 0x00},
    {0x94, 0x00},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0xe3},
    {0x98, 0x00},
    {0x99, 0x00},
    {0x9a, 0x00},
    {0x9b, 0x00},
    {0x9c, 0x00},
    {0x9d, 0x00},
    {0x9e, 0x00},
    {0x9f, 0x00},
    {0xa0, 0x00},
    {0xa1, 0x00},
    {0xa2, 0x00},
    {0xa3, 0x00},
    {0xa4, 0x00},
    {0xa5, 0x00},
    {0xa6, 0x10},
    {0xa7, 0x00},
    {0xa8, 0x18},
    {0xa9, 0x00},
    {0xaa, 0x00},
    {0xab, 0x00},
    {0xac, 0x00},
    {0xad, 0x00},
    {0xae, 0x00},
    {0xaf, 0x18},
    {0xb0, 0x18},
    {0xb1, 0x30},
    {0xb2, 0x00},
    {0xb3, 0x00},
    {0xb4, 0x00},
    {0xb5, 0x00},
    {0xb6, 0x00},
    {0xb7, 0x00},
    {0xb8, 0x00},
    {0xb9, 0x00},
    {0xba, 0x00},
    {0xbb, 0x03},
    {0xbc, 0x01},
    {0xbd, 0x00},
    {0xbe, 0x00},
    {0xbf, 0x00},
    {0xc0, 0x10},
    {0xc1, 0x20},
    {0xc2, 0x00},
    {0xc3, 0x20},
    {0xc4, 0x00},
    {0xc5, 0x2c},
    {0xc6, 0x1c},
    {0xc7, 0x10},
    {0xc8, 0x10},
    {0xc9, 0x01},
    {0xca, 0x68},
    {0xcb, 0xa7},
    {0xcc, 0x3c},
    {0xcd, 0x09},
    {0xce, 0x00},
    {0xcf, 0x20},
    {0xd0, 0x40},
    {0xd1, 0x10},
    {0xd2, 0x00},
    {0xd3, 0x00},
    {0xd4, 0x20},
    {0xd5, 0x28},
    {0xd6, 0xa0},
    {0xd7, 0x2a},
    {0xd8, 0x00},
    {0xd9, 0x00},
    {0xda, 0x00},
    {0xdb, 0x00},
    {0xdc, 0x00},
    {0xdd, 0x00},
    {0xde, 0x00},
    {0xdf, 0x00},
    {0xe0, 0x00},
    {0xe1, 0xd3},
    {0xe2, 0xc0},
    {0xe3, 0x00},
    {0xe4, 0x00},
    {0xe5, 0x10},
    {0xe6, 0x00},
    {0xe7, 0x12},
    {0xe8, 0x12},
    {0xe9, 0x34},
    {0xea, 0x00},
    {0xeb, 0xff},
    {0xec, 0x79},
    {0xed, 0x20},
    {0xee, 0x30},
    {0xef, 0x01},
    {0xf0, 0x00},
    {0xf1, 0x3e},
    {0xf2, 0x00},
    {0xf3, 0x00},
    {0xf4, 0x00},
    {0xf5, 0x00},
    {0xf6, 0x00},
    {0xf7, 0x00},
    {0xf8, 0x00},
    {0xf9, 0x00},
    {0xfa, 0x00},
    {0xfb, 0x00},
    {0xfc, 0x00},
    {0xfd, 0x00},
    {0xfe, 0x00},
    {0xff, 0x00},
};

#define CB_VT3253B0_AGC 193
// For AIROHA
unsigned char byVT3253B0_AGC[CB_VT3253B0_AGC][2] = {
    {0xF0, 0x00},
    {0xF1, 0x00},
    {0xF0, 0x80},
    {0xF0, 0x01},
    {0xF1, 0x00},
    {0xF0, 0x81},
    {0xF0, 0x02},
    {0xF1, 0x02},
    {0xF0, 0x82},
    {0xF0, 0x03},
    {0xF1, 0x04},
    {0xF0, 0x83},
    {0xF0, 0x03},
    {0xF1, 0x04},
    {0xF0, 0x84},
    {0xF0, 0x04},
    {0xF1, 0x06},
    {0xF0, 0x85},
    {0xF0, 0x05},
    {0xF1, 0x06},
    {0xF0, 0x86},
    {0xF0, 0x06},
    {0xF1, 0x06},
    {0xF0, 0x87},
    {0xF0, 0x07},
    {0xF1, 0x08},
    {0xF0, 0x88},
    {0xF0, 0x08},
    {0xF1, 0x08},
    {0xF0, 0x89},
    {0xF0, 0x09},
    {0xF1, 0x0A},
    {0xF0, 0x8A},
    {0xF0, 0x0A},
    {0xF1, 0x0A},
    {0xF0, 0x8B},
    {0xF0, 0x0B},
    {0xF1, 0x0C},
    {0xF0, 0x8C},
    {0xF0, 0x0C},
    {0xF1, 0x0C},
    {0xF0, 0x8D},
    {0xF0, 0x0D},
    {0xF1, 0x0E},
    {0xF0, 0x8E},
    {0xF0, 0x0E},
    {0xF1, 0x0E},
    {0xF0, 0x8F},
    {0xF0, 0x0F},
    {0xF1, 0x10},
    {0xF0, 0x90},
    {0xF0, 0x10},
    {0xF1, 0x10},
    {0xF0, 0x91},
    {0xF0, 0x11},
    {0xF1, 0x12},
    {0xF0, 0x92},
    {0xF0, 0x12},
    {0xF1, 0x12},
    {0xF0, 0x93},
    {0xF0, 0x13},
    {0xF1, 0x14},
    {0xF0, 0x94},
    {0xF0, 0x14},
    {0xF1, 0x14},
    {0xF0, 0x95},
    {0xF0, 0x15},
    {0xF1, 0x16},
    {0xF0, 0x96},
    {0xF0, 0x16},
    {0xF1, 0x16},
    {0xF0, 0x97},
    {0xF0, 0x17},
    {0xF1, 0x18},
    {0xF0, 0x98},
    {0xF0, 0x18},
    {0xF1, 0x18},
    {0xF0, 0x99},
    {0xF0, 0x19},
    {0xF1, 0x1A},
    {0xF0, 0x9A},
    {0xF0, 0x1A},
    {0xF1, 0x1A},
    {0xF0, 0x9B},
    {0xF0, 0x1B},
    {0xF1, 0x1C},
    {0xF0, 0x9C},
    {0xF0, 0x1C},
    {0xF1, 0x1C},
    {0xF0, 0x9D},
    {0xF0, 0x1D},
    {0xF1, 0x1E},
    {0xF0, 0x9E},
    {0xF0, 0x1E},
    {0xF1, 0x1E},
    {0xF0, 0x9F},
    {0xF0, 0x1F},
    {0xF1, 0x20},
    {0xF0, 0xA0},
    {0xF0, 0x20},
    {0xF1, 0x20},
    {0xF0, 0xA1},
    {0xF0, 0x21},
    {0xF1, 0x22},
    {0xF0, 0xA2},
    {0xF0, 0x22},
    {0xF1, 0x22},
    {0xF0, 0xA3},
    {0xF0, 0x23},
    {0xF1, 0x24},
    {0xF0, 0xA4},
    {0xF0, 0x24},
    {0xF1, 0x24},
    {0xF0, 0xA5},
    {0xF0, 0x25},
    {0xF1, 0x26},
    {0xF0, 0xA6},
    {0xF0, 0x26},
    {0xF1, 0x26},
    {0xF0, 0xA7},
    {0xF0, 0x27},
    {0xF1, 0x28},
    {0xF0, 0xA8},
    {0xF0, 0x28},
    {0xF1, 0x28},
    {0xF0, 0xA9},
    {0xF0, 0x29},
    {0xF1, 0x2A},
    {0xF0, 0xAA},
    {0xF0, 0x2A},
    {0xF1, 0x2A},
    {0xF0, 0xAB},
    {0xF0, 0x2B},
    {0xF1, 0x2C},
    {0xF0, 0xAC},
    {0xF0, 0x2C},
    {0xF1, 0x2C},
    {0xF0, 0xAD},
    {0xF0, 0x2D},
    {0xF1, 0x2E},
    {0xF0, 0xAE},
    {0xF0, 0x2E},
    {0xF1, 0x2E},
    {0xF0, 0xAF},
    {0xF0, 0x2F},
    {0xF1, 0x30},
    {0xF0, 0xB0},
    {0xF0, 0x30},
    {0xF1, 0x30},
    {0xF0, 0xB1},
    {0xF0, 0x31},
    {0xF1, 0x32},
    {0xF0, 0xB2},
    {0xF0, 0x32},
    {0xF1, 0x32},
    {0xF0, 0xB3},
    {0xF0, 0x33},
    {0xF1, 0x34},
    {0xF0, 0xB4},
    {0xF0, 0x34},
    {0xF1, 0x34},
    {0xF0, 0xB5},
    {0xF0, 0x35},
    {0xF1, 0x36},
    {0xF0, 0xB6},
    {0xF0, 0x36},
    {0xF1, 0x36},
    {0xF0, 0xB7},
    {0xF0, 0x37},
    {0xF1, 0x38},
    {0xF0, 0xB8},
    {0xF0, 0x38},
    {0xF1, 0x38},
    {0xF0, 0xB9},
    {0xF0, 0x39},
    {0xF1, 0x3A},
    {0xF0, 0xBA},
    {0xF0, 0x3A},
    {0xF1, 0x3A},
    {0xF0, 0xBB},
    {0xF0, 0x3B},
    {0xF1, 0x3C},
    {0xF0, 0xBC},
    {0xF0, 0x3C},
    {0xF1, 0x3C},
    {0xF0, 0xBD},
    {0xF0, 0x3D},
    {0xF1, 0x3E},
    {0xF0, 0xBE},
    {0xF0, 0x3E},
    {0xF1, 0x3E},
    {0xF0, 0xBF},
    {0xF0, 0x00},
};

const unsigned short awcFrameTime[MAX_RATE] =
{10, 20, 55, 110, 24, 36, 48, 72, 96, 144, 192, 216};


/*---------------------  Static Functions  --------------------------*/

static
unsigned long
s_ulGetRatio(PSDevice pDevice);

static
void
s_vChangeAntenna(
    PSDevice pDevice
    );

static
void
s_vChangeAntenna (
    PSDevice pDevice
    )
{

#ifdef	PLICE_DEBUG
	//printk("Enter s_vChangeAntenna:original RxMode is %d,TxMode is %d\n",pDevice->byRxAntennaMode,pDevice->byTxAntennaMode);
#endif
    if ( pDevice->dwRxAntennaSel == 0) {
        pDevice->dwRxAntennaSel=1;
        if (pDevice->bTxRxAntInv == true)
            BBvSetRxAntennaMode(pDevice->PortOffset, ANT_A);
        else
            BBvSetRxAntennaMode(pDevice->PortOffset, ANT_B);
    } else {
        pDevice->dwRxAntennaSel=0;
        if (pDevice->bTxRxAntInv == true)
            BBvSetRxAntennaMode(pDevice->PortOffset, ANT_B);
        else
            BBvSetRxAntennaMode(pDevice->PortOffset, ANT_A);
    }
    if ( pDevice->dwTxAntennaSel == 0) {
        pDevice->dwTxAntennaSel=1;
        BBvSetTxAntennaMode(pDevice->PortOffset, ANT_B);
    } else {
        pDevice->dwTxAntennaSel=0;
        BBvSetTxAntennaMode(pDevice->PortOffset, ANT_A);
    }
}


/*---------------------  Export Variables  --------------------------*/
/*
 * Description: Calculate data frame transmitting time
 *
 * Parameters:
 *  In:
 *      byPreambleType  - Preamble Type
 *      byPktType        - PK_TYPE_11A, PK_TYPE_11B, PK_TYPE_11GB, PK_TYPE_11GA
 *      cbFrameLength   - Baseband Type
 *      wRate           - Tx Rate
 *  Out:
 *
 * Return Value: FrameTime
 *
 */
unsigned int
BBuGetFrameTime (
    unsigned char byPreambleType,
    unsigned char byPktType,
    unsigned int cbFrameLength,
    unsigned short wRate
    )
{
    unsigned int uFrameTime;
    unsigned int uPreamble;
    unsigned int uTmp;
    unsigned int uRateIdx = (unsigned int) wRate;
    unsigned int uRate = 0;


    if (uRateIdx > RATE_54M) {
	    ASSERT(0);
        return 0;
    }

    uRate = (unsigned int) awcFrameTime[uRateIdx];

    if (uRateIdx <= 3) {          //CCK mode

        if (byPreambleType == 1) {//Short
            uPreamble = 96;
        } else {
            uPreamble = 192;
        }
        uFrameTime = (cbFrameLength * 80) / uRate;  //?????
        uTmp = (uFrameTime * uRate) / 80;
        if (cbFrameLength != uTmp) {
            uFrameTime ++;
        }

        return (uPreamble + uFrameTime);
    }
    else {
        uFrameTime = (cbFrameLength * 8 + 22) / uRate;   //????????
        uTmp = ((uFrameTime * uRate) - 22) / 8;
        if(cbFrameLength != uTmp) {
            uFrameTime ++;
        }
        uFrameTime = uFrameTime * 4;    //???????
        if(byPktType != PK_TYPE_11A) {
            uFrameTime += 6;     //??????
        }
        return (20 + uFrameTime); //??????
    }
}

/*
 * Description: Caculate Length, Service, and Signal fields of Phy for Tx
 *
 * Parameters:
 *  In:
 *      pDevice         - Device Structure
 *      cbFrameLength   - Tx Frame Length
 *      wRate           - Tx Rate
 *  Out:
 *      pwPhyLen        - pointer to Phy Length field
 *      pbyPhySrv       - pointer to Phy Service field
 *      pbyPhySgn       - pointer to Phy Signal field
 *
 * Return Value: none
 *
 */
void
BBvCaculateParameter (
    PSDevice pDevice,
    unsigned int cbFrameLength,
    unsigned short wRate,
    unsigned char byPacketType,
    unsigned short *pwPhyLen,
    unsigned char *pbyPhySrv,
    unsigned char *pbyPhySgn
    )
{
    unsigned int cbBitCount;
    unsigned int cbUsCount = 0;
    unsigned int cbTmp;
    bool bExtBit;
    unsigned char byPreambleType = pDevice->byPreambleType;
    bool bCCK = pDevice->bCCK;

    cbBitCount = cbFrameLength * 8;
    bExtBit = false;

    switch (wRate) {
    case RATE_1M :
        cbUsCount = cbBitCount;
        *pbyPhySgn = 0x00;
        break;

    case RATE_2M :
        cbUsCount = cbBitCount / 2;
        if (byPreambleType == 1)
            *pbyPhySgn = 0x09;
        else // long preamble
            *pbyPhySgn = 0x01;
        break;

    case RATE_5M :
        if (bCCK == false)
            cbBitCount ++;
        cbUsCount = (cbBitCount * 10) / 55;
        cbTmp = (cbUsCount * 55) / 10;
        if (cbTmp != cbBitCount)
            cbUsCount ++;
        if (byPreambleType == 1)
            *pbyPhySgn = 0x0a;
        else // long preamble
            *pbyPhySgn = 0x02;
        break;

    case RATE_11M :

        if (bCCK == false)
            cbBitCount ++;
        cbUsCount = cbBitCount / 11;
        cbTmp = cbUsCount * 11;
        if (cbTmp != cbBitCount) {
            cbUsCount ++;
            if ((cbBitCount - cbTmp) <= 3)
                bExtBit = true;
        }
        if (byPreambleType == 1)
            *pbyPhySgn = 0x0b;
        else // long preamble
            *pbyPhySgn = 0x03;
        break;

    case RATE_6M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9B; //1001 1011
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8B; //1000 1011
        }
        break;

    case RATE_9M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9F; //1001 1111
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8F; //1000 1111
        }
        break;

    case RATE_12M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9A; //1001 1010
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8A; //1000 1010
        }
        break;

    case RATE_18M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9E; //1001 1110
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8E; //1000 1110
        }
        break;

    case RATE_24M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x99; //1001 1001
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x89; //1000 1001
        }
        break;

    case RATE_36M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9D; //1001 1101
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8D; //1000 1101
        }
        break;

    case RATE_48M :
        if(byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x98; //1001 1000
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x88; //1000 1000
        }
        break;

    case RATE_54M :
        if (byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9C; //1001 1100
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8C; //1000 1100
        }
        break;

    default :
        if (byPacketType == PK_TYPE_11A) {//11a, 5GHZ
            *pbyPhySgn = 0x9C; //1001 1100
        }
        else {//11g, 2.4GHZ
            *pbyPhySgn = 0x8C; //1000 1100
        }
        break;
    }

    if (byPacketType == PK_TYPE_11B) {
        *pbyPhySrv = 0x00;
        if (bExtBit)
            *pbyPhySrv = *pbyPhySrv | 0x80;
        *pwPhyLen = (unsigned short)cbUsCount;
    }
    else {
        *pbyPhySrv = 0x00;
        *pwPhyLen = (unsigned short)cbFrameLength;
    }
}

/*
 * Description: Read a byte from BASEBAND, by embeded programming
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      byBBAddr    - address of register in Baseband
 *  Out:
 *      pbyData     - data read
 *
 * Return Value: true if succeeded; false if failed.
 *
 */
bool BBbReadEmbeded (unsigned long dwIoBase, unsigned char byBBAddr, unsigned char *pbyData)
{
    unsigned short ww;
    unsigned char byValue;

    // BB reg offset
    VNSvOutPortB(dwIoBase + MAC_REG_BBREGADR, byBBAddr);

    // turn on REGR
    MACvRegBitsOn(dwIoBase, MAC_REG_BBREGCTL, BBREGCTL_REGR);
    // W_MAX_TIMEOUT is the timeout period
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_BBREGCTL, &byValue);
        if (byValue & BBREGCTL_DONE)
            break;
    }

    // get BB data
    VNSvInPortB(dwIoBase + MAC_REG_BBREGDATA, pbyData);

    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x30);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x30)\n");
        return false;
    }
    return true;
}


/*
 * Description: Write a Byte to BASEBAND, by embeded programming
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      byBBAddr    - address of register in Baseband
 *      byData      - data to write
 *  Out:
 *      none
 *
 * Return Value: true if succeeded; false if failed.
 *
 */
bool BBbWriteEmbeded (unsigned long dwIoBase, unsigned char byBBAddr, unsigned char byData)
{
    unsigned short ww;
    unsigned char byValue;

    // BB reg offset
    VNSvOutPortB(dwIoBase + MAC_REG_BBREGADR, byBBAddr);
    // set BB data
    VNSvOutPortB(dwIoBase + MAC_REG_BBREGDATA, byData);

    // turn on BBREGCTL_REGW
    MACvRegBitsOn(dwIoBase, MAC_REG_BBREGCTL, BBREGCTL_REGW);
    // W_MAX_TIMEOUT is the timeout period
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_BBREGCTL, &byValue);
        if (byValue & BBREGCTL_DONE)
            break;
    }

    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x31);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x31)\n");
        return false;
    }
    return true;
}


/*
 * Description: Test if all bits are set for the Baseband register
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      byBBAddr    - address of register in Baseband
 *      byTestBits  - TestBits
 *  Out:
 *      none
 *
 * Return Value: true if all TestBits are set; false otherwise.
 *
 */
bool BBbIsRegBitsOn (unsigned long dwIoBase, unsigned char byBBAddr, unsigned char byTestBits)
{
    unsigned char byOrgData;

    BBbReadEmbeded(dwIoBase, byBBAddr, &byOrgData);
    return (byOrgData & byTestBits) == byTestBits;
}


/*
 * Description: Test if all bits are clear for the Baseband register
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      byBBAddr    - address of register in Baseband
 *      byTestBits  - TestBits
 *  Out:
 *      none
 *
 * Return Value: true if all TestBits are clear; false otherwise.
 *
 */
bool BBbIsRegBitsOff (unsigned long dwIoBase, unsigned char byBBAddr, unsigned char byTestBits)
{
    unsigned char byOrgData;

    BBbReadEmbeded(dwIoBase, byBBAddr, &byOrgData);
    return (byOrgData & byTestBits) == 0;
}

/*
 * Description: VIA VT3253 Baseband chip init function
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      byRevId     - Revision ID
 *      byRFType    - RF type
 *  Out:
 *      none
 *
 * Return Value: true if succeeded; false if failed.
 *
 */

bool BBbVT3253Init (PSDevice pDevice)
{
    bool bResult = true;
    int        ii;
    unsigned long dwIoBase = pDevice->PortOffset;
    unsigned char byRFType = pDevice->byRFType;
    unsigned char byLocalID = pDevice->byLocalID;

    if (byRFType == RF_RFMD2959) {
        if (byLocalID <= REV_ID_VT3253_A1) {
            for (ii = 0; ii < CB_VT3253_INIT_FOR_RFMD; ii++) {
                bResult &= BBbWriteEmbeded(dwIoBase,byVT3253InitTab_RFMD[ii][0],byVT3253InitTab_RFMD[ii][1]);
            }
        } else {
            for (ii = 0; ii < CB_VT3253B0_INIT_FOR_RFMD; ii++) {
                bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_RFMD[ii][0],byVT3253B0_RFMD[ii][1]);
            }
            for (ii = 0; ii < CB_VT3253B0_AGC_FOR_RFMD2959; ii++) {
        	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC4_RFMD2959[ii][0],byVT3253B0_AGC4_RFMD2959[ii][1]);
            }
            VNSvOutPortD(dwIoBase + MAC_REG_ITRTMSET, 0x23);
            MACvRegBitsOn(dwIoBase, MAC_REG_PAPEDELAY, BIT0);
        }
        pDevice->abyBBVGA[0] = 0x18;
        pDevice->abyBBVGA[1] = 0x0A;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -50;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    } else if ((byRFType == RF_AIROHA) || (byRFType == RF_AL2230S) ) {
        for (ii = 0; ii < CB_VT3253B0_INIT_FOR_AIROHA2230; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AIROHA2230[ii][0],byVT3253B0_AIROHA2230[ii][1]);
    	}
        for (ii = 0; ii < CB_VT3253B0_AGC; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC[ii][0],byVT3253B0_AGC[ii][1]);
    	}
        pDevice->abyBBVGA[0] = 0x1C;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    } else if (byRFType == RF_UW2451) {
        for (ii = 0; ii < CB_VT3253B0_INIT_FOR_UW2451; ii++) {
    	        bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_UW2451[ii][0],byVT3253B0_UW2451[ii][1]);
    	}
        for (ii = 0; ii < CB_VT3253B0_AGC; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC[ii][0],byVT3253B0_AGC[ii][1]);
    	}
        VNSvOutPortB(dwIoBase + MAC_REG_ITRTMSET, 0x23);
        MACvRegBitsOn(dwIoBase, MAC_REG_PAPEDELAY, BIT0);

        pDevice->abyBBVGA[0] = 0x14;
        pDevice->abyBBVGA[1] = 0x0A;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -60;
        pDevice->ldBmThreshold[1] = -50;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    } else if (byRFType == RF_UW2452) {
        for (ii = 0; ii < CB_VT3253B0_INIT_FOR_UW2451; ii++) {
            bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_UW2451[ii][0],byVT3253B0_UW2451[ii][1]);
    	}
        // Init ANT B select,TX Config CR09 = 0x61->0x45, 0x45->0x41(VC1/VC2 define, make the ANT_A, ANT_B inverted)
        //bResult &= BBbWriteEmbeded(dwIoBase,0x09,0x41);
        // Init ANT B select,RX Config CR10 = 0x28->0x2A, 0x2A->0x28(VC1/VC2 define, make the ANT_A, ANT_B inverted)
        //bResult &= BBbWriteEmbeded(dwIoBase,0x0a,0x28);
        // Select VC1/VC2, CR215 = 0x02->0x06
        bResult &= BBbWriteEmbeded(dwIoBase,0xd7,0x06);

        //{{RobertYu:20050125, request by Jack
        bResult &= BBbWriteEmbeded(dwIoBase,0x90,0x20);
        bResult &= BBbWriteEmbeded(dwIoBase,0x97,0xeb);
        //}}

        //{{RobertYu:20050221, request by Jack
        bResult &= BBbWriteEmbeded(dwIoBase,0xa6,0x00);
        bResult &= BBbWriteEmbeded(dwIoBase,0xa8,0x30);
        //}}
        bResult &= BBbWriteEmbeded(dwIoBase,0xb0,0x58);

        for (ii = 0; ii < CB_VT3253B0_AGC; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC[ii][0],byVT3253B0_AGC[ii][1]);
    	}
        //VNSvOutPortB(dwIoBase + MAC_REG_ITRTMSET, 0x23); // RobertYu: 20050104, 20050131 disable PA_Delay
        //MACvRegBitsOn(dwIoBase, MAC_REG_PAPEDELAY, BIT0); // RobertYu: 20050104, 20050131 disable PA_Delay

        pDevice->abyBBVGA[0] = 0x14;
        pDevice->abyBBVGA[1] = 0x0A;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -60;
        pDevice->ldBmThreshold[1] = -50;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    //}} RobertYu

    } else if (byRFType == RF_VT3226) {
        for (ii = 0; ii < CB_VT3253B0_INIT_FOR_AIROHA2230; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AIROHA2230[ii][0],byVT3253B0_AIROHA2230[ii][1]);
    	}
        for (ii = 0; ii < CB_VT3253B0_AGC; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC[ii][0],byVT3253B0_AGC[ii][1]);
    	}
        pDevice->abyBBVGA[0] = 0x1C;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
        // Fix VT3226 DFC system timing issue
        MACvSetRFLE_LatchBase(dwIoBase);
         //{{ RobertYu: 20050104
    } else if (byRFType == RF_AIROHA7230) {
        for (ii = 0; ii < CB_VT3253B0_INIT_FOR_AIROHA2230; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AIROHA2230[ii][0],byVT3253B0_AIROHA2230[ii][1]);
    	}

        //{{ RobertYu:20050223, request by JerryChung
        // Init ANT B select,TX Config CR09 = 0x61->0x45, 0x45->0x41(VC1/VC2 define, make the ANT_A, ANT_B inverted)
        //bResult &= BBbWriteEmbeded(dwIoBase,0x09,0x41);
        // Init ANT B select,RX Config CR10 = 0x28->0x2A, 0x2A->0x28(VC1/VC2 define, make the ANT_A, ANT_B inverted)
        //bResult &= BBbWriteEmbeded(dwIoBase,0x0a,0x28);
        // Select VC1/VC2, CR215 = 0x02->0x06
        bResult &= BBbWriteEmbeded(dwIoBase,0xd7,0x06);
        //}}

        for (ii = 0; ii < CB_VT3253B0_AGC; ii++) {
    	    bResult &= BBbWriteEmbeded(dwIoBase,byVT3253B0_AGC[ii][0],byVT3253B0_AGC[ii][1]);
    	}
        pDevice->abyBBVGA[0] = 0x1C;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    //}} RobertYu
    } else {
    	// No VGA Table now
    	pDevice->bUpdateBBVGA = false;
        pDevice->abyBBVGA[0] = 0x1C;
    }

    if (byLocalID > REV_ID_VT3253_A1) {
        BBbWriteEmbeded(dwIoBase, 0x04, 0x7F);
        BBbWriteEmbeded(dwIoBase, 0x0D, 0x01);
    }

    return bResult;
}



/*
 * Description: Read All Baseband Registers
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      pbyBBRegs   - Point to struct that stores Baseband Registers
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void BBvReadAllRegs (unsigned long dwIoBase, unsigned char *pbyBBRegs)
{
    int  ii;
    unsigned char byBase = 1;
    for (ii = 0; ii < BB_MAX_CONTEXT_SIZE; ii++) {
        BBbReadEmbeded(dwIoBase, (unsigned char)(ii*byBase), pbyBBRegs);
        pbyBBRegs += byBase;
    }
}

/*
 * Description: Turn on BaseBand Loopback mode
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *      bCCK        - If CCK is set
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */


void BBvLoopbackOn (PSDevice pDevice)
{
    unsigned char byData;
    unsigned long dwIoBase = pDevice->PortOffset;

    //CR C9 = 0x00
    BBbReadEmbeded(dwIoBase, 0xC9, &pDevice->byBBCRc9);//CR201
    BBbWriteEmbeded(dwIoBase, 0xC9, 0);
    BBbReadEmbeded(dwIoBase, 0x4D, &pDevice->byBBCR4d);//CR77
    BBbWriteEmbeded(dwIoBase, 0x4D, 0x90);

    //CR 88 = 0x02(CCK), 0x03(OFDM)
    BBbReadEmbeded(dwIoBase, 0x88, &pDevice->byBBCR88);//CR136

    if (pDevice->uConnectionRate <= RATE_11M) { //CCK
        // Enable internal digital loopback: CR33 |= 0000 0001
        BBbReadEmbeded(dwIoBase, 0x21, &byData);//CR33
        BBbWriteEmbeded(dwIoBase, 0x21, (unsigned char)(byData | 0x01));//CR33
        // CR154 = 0x00
        BBbWriteEmbeded(dwIoBase, 0x9A, 0);   //CR154

        BBbWriteEmbeded(dwIoBase, 0x88, 0x02);//CR239
    }
    else { //OFDM
        // Enable internal digital loopback:CR154 |= 0000 0001
        BBbReadEmbeded(dwIoBase, 0x9A, &byData);//CR154
        BBbWriteEmbeded(dwIoBase, 0x9A, (unsigned char)(byData | 0x01));//CR154
        // CR33 = 0x00
        BBbWriteEmbeded(dwIoBase, 0x21, 0);   //CR33

        BBbWriteEmbeded(dwIoBase, 0x88, 0x03);//CR239
    }

    //CR14 = 0x00
    BBbWriteEmbeded(dwIoBase, 0x0E, 0);//CR14

    // Disable TX_IQUN
    BBbReadEmbeded(pDevice->PortOffset, 0x09, &pDevice->byBBCR09);
    BBbWriteEmbeded(pDevice->PortOffset, 0x09, (unsigned char)(pDevice->byBBCR09 & 0xDE));
}

/*
 * Description: Turn off BaseBand Loopback mode
 *
 * Parameters:
 *  In:
 *      pDevice         - Device Structure
 *
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void BBvLoopbackOff (PSDevice pDevice)
{
    unsigned char byData;
    unsigned long dwIoBase = pDevice->PortOffset;

    BBbWriteEmbeded(dwIoBase, 0xC9, pDevice->byBBCRc9);//CR201
    BBbWriteEmbeded(dwIoBase, 0x88, pDevice->byBBCR88);//CR136
    BBbWriteEmbeded(dwIoBase, 0x09, pDevice->byBBCR09);//CR136
    BBbWriteEmbeded(dwIoBase, 0x4D, pDevice->byBBCR4d);//CR77

    if (pDevice->uConnectionRate <= RATE_11M) { // CCK
        // Set the CR33 Bit2 to disable internal Loopback.
        BBbReadEmbeded(dwIoBase, 0x21, &byData);//CR33
        BBbWriteEmbeded(dwIoBase, 0x21, (unsigned char)(byData & 0xFE));//CR33
    }
    else { // OFDM
        BBbReadEmbeded(dwIoBase, 0x9A, &byData);//CR154
        BBbWriteEmbeded(dwIoBase, 0x9A, (unsigned char)(byData & 0xFE));//CR154
    }
    BBbReadEmbeded(dwIoBase, 0x0E, &byData);//CR14
    BBbWriteEmbeded(dwIoBase, 0x0E, (unsigned char)(byData | 0x80));//CR14

}



/*
 * Description: Set ShortSlotTime mode
 *
 * Parameters:
 *  In:
 *      pDevice     - Device Structure
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void
BBvSetShortSlotTime (PSDevice pDevice)
{
    unsigned char byBBRxConf=0;
    unsigned char byBBVGA=0;

    BBbReadEmbeded(pDevice->PortOffset, 0x0A, &byBBRxConf);//CR10

    if (pDevice->bShortSlotTime) {
        byBBRxConf &= 0xDF;//1101 1111
    } else {
        byBBRxConf |= 0x20;//0010 0000
    }

    // patch for 3253B0 Baseband with Cardbus module
    BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byBBVGA);
    if (byBBVGA == pDevice->abyBBVGA[0]) {
        byBBRxConf |= 0x20;//0010 0000
    }

    BBbWriteEmbeded(pDevice->PortOffset, 0x0A, byBBRxConf);//CR10

}

void BBvSetVGAGainOffset(PSDevice pDevice, unsigned char byData)
{
    unsigned char byBBRxConf=0;

    BBbWriteEmbeded(pDevice->PortOffset, 0xE7, byData);

    BBbReadEmbeded(pDevice->PortOffset, 0x0A, &byBBRxConf);//CR10
    // patch for 3253B0 Baseband with Cardbus module
    if (byData == pDevice->abyBBVGA[0]) {
        byBBRxConf |= 0x20;//0010 0000
    } else if (pDevice->bShortSlotTime) {
        byBBRxConf &= 0xDF;//1101 1111
    } else {
        byBBRxConf |= 0x20;//0010 0000
    }
    pDevice->byBBVGACurrent = byData;
    BBbWriteEmbeded(pDevice->PortOffset, 0x0A, byBBRxConf);//CR10
}


/*
 * Description: Baseband SoftwareReset
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void
BBvSoftwareReset (unsigned long dwIoBase)
{
    BBbWriteEmbeded(dwIoBase, 0x50, 0x40);
    BBbWriteEmbeded(dwIoBase, 0x50, 0);
    BBbWriteEmbeded(dwIoBase, 0x9C, 0x01);
    BBbWriteEmbeded(dwIoBase, 0x9C, 0);
}

/*
 * Description: Baseband Power Save Mode ON
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void
BBvPowerSaveModeON (unsigned long dwIoBase)
{
    unsigned char byOrgData;

    BBbReadEmbeded(dwIoBase, 0x0D, &byOrgData);
    byOrgData |= BIT0;
    BBbWriteEmbeded(dwIoBase, 0x0D, byOrgData);
}

/*
 * Description: Baseband Power Save Mode OFF
 *
 * Parameters:
 *  In:
 *      dwIoBase    - I/O base address
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void
BBvPowerSaveModeOFF (unsigned long dwIoBase)
{
    unsigned char byOrgData;

    BBbReadEmbeded(dwIoBase, 0x0D, &byOrgData);
    byOrgData &= ~(BIT0);
    BBbWriteEmbeded(dwIoBase, 0x0D, byOrgData);
}

/*
 * Description: Set Tx Antenna mode
 *
 * Parameters:
 *  In:
 *      pDevice          - Device Structure
 *      byAntennaMode    - Antenna Mode
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */

void
BBvSetTxAntennaMode (unsigned long dwIoBase, unsigned char byAntennaMode)
{
    unsigned char byBBTxConf;

#ifdef	PLICE_DEBUG
	//printk("Enter BBvSetTxAntennaMode\n");
#endif
    BBbReadEmbeded(dwIoBase, 0x09, &byBBTxConf);//CR09
    if (byAntennaMode == ANT_DIVERSITY) {
        // bit 1 is diversity
        byBBTxConf |= 0x02;
    } else if (byAntennaMode == ANT_A) {
        // bit 2 is ANTSEL
        byBBTxConf &= 0xF9; // 1111 1001
    } else if (byAntennaMode == ANT_B) {
#ifdef	PLICE_DEBUG
	//printk("BBvSetTxAntennaMode:ANT_B\n");
#endif
        byBBTxConf &= 0xFD; // 1111 1101
        byBBTxConf |= 0x04;
    }
    BBbWriteEmbeded(dwIoBase, 0x09, byBBTxConf);//CR09
}




/*
 * Description: Set Rx Antenna mode
 *
 * Parameters:
 *  In:
 *      pDevice          - Device Structure
 *      byAntennaMode    - Antenna Mode
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */

void
BBvSetRxAntennaMode (unsigned long dwIoBase, unsigned char byAntennaMode)
{
    unsigned char byBBRxConf;

    BBbReadEmbeded(dwIoBase, 0x0A, &byBBRxConf);//CR10
    if (byAntennaMode == ANT_DIVERSITY) {
        byBBRxConf |= 0x01;

    } else if (byAntennaMode == ANT_A) {
        byBBRxConf &= 0xFC; // 1111 1100
    } else if (byAntennaMode == ANT_B) {
        byBBRxConf &= 0xFE; // 1111 1110
        byBBRxConf |= 0x02;
    }
    BBbWriteEmbeded(dwIoBase, 0x0A, byBBRxConf);//CR10
}


/*
 * Description: BBvSetDeepSleep
 *
 * Parameters:
 *  In:
 *      pDevice          - Device Structure
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void
BBvSetDeepSleep (unsigned long dwIoBase, unsigned char byLocalID)
{
    BBbWriteEmbeded(dwIoBase, 0x0C, 0x17);//CR12
    BBbWriteEmbeded(dwIoBase, 0x0D, 0xB9);//CR13
}

void
BBvExitDeepSleep (unsigned long dwIoBase, unsigned char byLocalID)
{
    BBbWriteEmbeded(dwIoBase, 0x0C, 0x00);//CR12
    BBbWriteEmbeded(dwIoBase, 0x0D, 0x01);//CR13
}



static
unsigned long
s_ulGetRatio (PSDevice pDevice)
{
unsigned long ulRatio = 0;
unsigned long ulMaxPacket;
unsigned long ulPacketNum;

    //This is a thousand-ratio
    ulMaxPacket = pDevice->uNumSQ3[RATE_54M];
    if ( pDevice->uNumSQ3[RATE_54M] != 0 ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_54M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_54M;
    }
    if ( pDevice->uNumSQ3[RATE_48M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M] + pDevice->uNumSQ3[RATE_48M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_48M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_48M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_48M];
    }
    if ( pDevice->uNumSQ3[RATE_36M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M] + pDevice->uNumSQ3[RATE_48M] +
                      pDevice->uNumSQ3[RATE_36M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_36M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_36M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_36M];
    }
    if ( pDevice->uNumSQ3[RATE_24M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M] + pDevice->uNumSQ3[RATE_48M] +
                      pDevice->uNumSQ3[RATE_36M] + pDevice->uNumSQ3[RATE_24M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_24M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_24M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_24M];
    }
    if ( pDevice->uNumSQ3[RATE_18M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M] + pDevice->uNumSQ3[RATE_48M] +
                      pDevice->uNumSQ3[RATE_36M] + pDevice->uNumSQ3[RATE_24M] +
                      pDevice->uNumSQ3[RATE_18M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_18M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_18M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_18M];
    }
    if ( pDevice->uNumSQ3[RATE_12M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uNumSQ3[RATE_54M] + pDevice->uNumSQ3[RATE_48M] +
                      pDevice->uNumSQ3[RATE_36M] + pDevice->uNumSQ3[RATE_24M] +
                      pDevice->uNumSQ3[RATE_18M] + pDevice->uNumSQ3[RATE_12M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_12M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_12M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_12M];
    }
    if ( pDevice->uNumSQ3[RATE_11M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt - pDevice->uNumSQ3[RATE_1M] -
                      pDevice->uNumSQ3[RATE_2M] - pDevice->uNumSQ3[RATE_5M] -
                      pDevice->uNumSQ3[RATE_6M] - pDevice->uNumSQ3[RATE_9M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_11M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_11M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_11M];
    }
    if ( pDevice->uNumSQ3[RATE_9M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt - pDevice->uNumSQ3[RATE_1M] -
                      pDevice->uNumSQ3[RATE_2M] - pDevice->uNumSQ3[RATE_5M] -
                      pDevice->uNumSQ3[RATE_6M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_9M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_9M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_9M];
    }
    if ( pDevice->uNumSQ3[RATE_6M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt - pDevice->uNumSQ3[RATE_1M] -
                      pDevice->uNumSQ3[RATE_2M] - pDevice->uNumSQ3[RATE_5M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_6M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_6M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_6M];
    }
    if ( pDevice->uNumSQ3[RATE_5M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt - pDevice->uNumSQ3[RATE_1M] -
                      pDevice->uNumSQ3[RATE_2M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_5M] * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_55M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_5M];
    }
    if ( pDevice->uNumSQ3[RATE_2M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt - pDevice->uNumSQ3[RATE_1M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_2M]  * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_2M;
        ulMaxPacket = pDevice->uNumSQ3[RATE_2M];
    }
    if ( pDevice->uNumSQ3[RATE_1M] > ulMaxPacket ) {
        ulPacketNum = pDevice->uDiversityCnt;
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        //ulRatio = (pDevice->uNumSQ3[RATE_1M]  * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_1M;
    }

    return ulRatio;
}


void
BBvClearAntDivSQ3Value (PSDevice pDevice)
{
    unsigned int ii;

    pDevice->uDiversityCnt = 0;
    for (ii = 0; ii < MAX_RATE; ii++) {
        pDevice->uNumSQ3[ii] = 0;
    }
}


/*
 * Description: Antenna Diversity
 *
 * Parameters:
 *  In:
 *      pDevice          - Device Structure
 *      byRSR            - RSR from received packet
 *      bySQ3            - SQ3 value from received packet
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */

void
BBvAntennaDiversity (PSDevice pDevice, unsigned char byRxRate, unsigned char bySQ3)
{

    if ((byRxRate >= MAX_RATE) || (pDevice->wAntDiversityMaxRate >= MAX_RATE)) {
        return;
    }
    pDevice->uDiversityCnt++;
   // DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "pDevice->uDiversityCnt = %d\n", (int)pDevice->uDiversityCnt);

    pDevice->uNumSQ3[byRxRate]++;

    if (pDevice->byAntennaState == 0) {

        if (pDevice->uDiversityCnt > pDevice->ulDiversityNValue) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ulDiversityNValue=[%d],54M-[%d]\n",
                          (int)pDevice->ulDiversityNValue, (int)pDevice->uNumSQ3[(int)pDevice->wAntDiversityMaxRate]);

            if (pDevice->uNumSQ3[pDevice->wAntDiversityMaxRate] < pDevice->uDiversityCnt/2) {

                pDevice->ulRatio_State0 = s_ulGetRatio(pDevice);
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"SQ3_State0, rate = [%08x]\n", (int)pDevice->ulRatio_State0);

                if ( pDevice->byTMax == 0 )
                    return;
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"1.[%08x], uNumSQ3[%d]=%d, %d\n",
                              (int)pDevice->ulRatio_State0, (int)pDevice->wAntDiversityMaxRate,
                              (int)pDevice->uNumSQ3[(int)pDevice->wAntDiversityMaxRate], (int)pDevice->uDiversityCnt);
#ifdef	PLICE_DEBUG
		//printk("BBvAntennaDiversity1:call s_vChangeAntenna\n");
#endif
		s_vChangeAntenna(pDevice);
                pDevice->byAntennaState = 1;
                del_timer(&pDevice->TimerSQ3Tmax3);
                del_timer(&pDevice->TimerSQ3Tmax2);
                pDevice->TimerSQ3Tmax1.expires =  RUN_AT(pDevice->byTMax * HZ);
                add_timer(&pDevice->TimerSQ3Tmax1);

            } else {

                pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
                add_timer(&pDevice->TimerSQ3Tmax3);
            }
            BBvClearAntDivSQ3Value(pDevice);

        }
    } else { //byAntennaState == 1

        if (pDevice->uDiversityCnt > pDevice->ulDiversityMValue) {

            del_timer(&pDevice->TimerSQ3Tmax1);

            pDevice->ulRatio_State1 = s_ulGetRatio(pDevice);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"RX:SQ3_State1, rate0 = %08x,rate1 = %08x\n",
                          (int)pDevice->ulRatio_State0,(int)pDevice->ulRatio_State1);

            if (pDevice->ulRatio_State1 < pDevice->ulRatio_State0) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"2.[%08x][%08x], uNumSQ3[%d]=%d, %d\n",
                              (int)pDevice->ulRatio_State0, (int)pDevice->ulRatio_State1,
                              (int)pDevice->wAntDiversityMaxRate,
                              (int)pDevice->uNumSQ3[(int)pDevice->wAntDiversityMaxRate], (int)pDevice->uDiversityCnt);
#ifdef	PLICE_DEBUG
		//printk("BBvAntennaDiversity2:call s_vChangeAntenna\n");
#endif
				s_vChangeAntenna(pDevice);
                pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
                pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
                add_timer(&pDevice->TimerSQ3Tmax3);
                add_timer(&pDevice->TimerSQ3Tmax2);
            }
            pDevice->byAntennaState = 0;
            BBvClearAntDivSQ3Value(pDevice);
        }
    } //byAntennaState
}

/*+
 *
 * Description:
 *  Timer for SQ3 antenna diversity
 *
 * Parameters:
 *  In:
 *  Out:
 *      none
 *
 * Return Value: none
 *
-*/

void
TimerSQ3CallBack (
    void *hDeviceContext
    )
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TimerSQ3CallBack...");
    spin_lock_irq(&pDevice->lock);

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"3.[%08x][%08x], %d\n",(int)pDevice->ulRatio_State0, (int)pDevice->ulRatio_State1, (int)pDevice->uDiversityCnt);
#ifdef	PLICE_DEBUG
		//printk("TimerSQ3CallBack1:call s_vChangeAntenna\n");
#endif

    s_vChangeAntenna(pDevice);
    pDevice->byAntennaState = 0;
    BBvClearAntDivSQ3Value(pDevice);

    pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
    pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
    add_timer(&pDevice->TimerSQ3Tmax3);
    add_timer(&pDevice->TimerSQ3Tmax2);


    spin_unlock_irq(&pDevice->lock);
    return;
}


/*+
 *
 * Description:
 *  Timer for SQ3 antenna diversity
 *
 * Parameters:
 *  In:
 *      pvSysSpec1
 *      hDeviceContext - Pointer to the adapter
 *      pvSysSpec2
 *      pvSysSpec3
 *  Out:
 *      none
 *
 * Return Value: none
 *
-*/

void
TimerState1CallBack (
    void *hDeviceContext
    )
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TimerState1CallBack...");

    spin_lock_irq(&pDevice->lock);
    if (pDevice->uDiversityCnt < pDevice->ulDiversityMValue/100) {
#ifdef	PLICE_DEBUG
		//printk("TimerSQ3CallBack2:call s_vChangeAntenna\n");
#endif

		s_vChangeAntenna(pDevice);
        pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
        pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
        add_timer(&pDevice->TimerSQ3Tmax3);
        add_timer(&pDevice->TimerSQ3Tmax2);
    } else {
        pDevice->ulRatio_State1 = s_ulGetRatio(pDevice);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"SQ3_State1, rate0 = %08x,rate1 = %08x\n",
                      (int)pDevice->ulRatio_State0,(int)pDevice->ulRatio_State1);

        if ( pDevice->ulRatio_State1 < pDevice->ulRatio_State0 ) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"2.[%08x][%08x], uNumSQ3[%d]=%d, %d\n",
                          (int)pDevice->ulRatio_State0, (int)pDevice->ulRatio_State1,
                          (int)pDevice->wAntDiversityMaxRate,
                          (int)pDevice->uNumSQ3[(int)pDevice->wAntDiversityMaxRate], (int)pDevice->uDiversityCnt);
#ifdef	PLICE_DEBUG
		//printk("TimerSQ3CallBack3:call s_vChangeAntenna\n");
#endif

			s_vChangeAntenna(pDevice);

            pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
            pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
            add_timer(&pDevice->TimerSQ3Tmax3);
            add_timer(&pDevice->TimerSQ3Tmax2);
        }
    }
    pDevice->byAntennaState = 0;
    BBvClearAntDivSQ3Value(pDevice);
    spin_unlock_irq(&pDevice->lock);

    return;
}
