/* $Id: amr_helper.h 3589 2011-06-20 04:06:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#ifndef __PJMEDIA_CODECS_AMR_HELPER_H__
#define __PJMEDIA_CODECS_AMR_HELPER_H__

/**
 * @file pjmedia-codec/amr_helper.h
 * @brief Common tables and helper functions for AMR codec (NB & WB).
 */


#ifdef _MSC_VER
#   pragma warning(disable:4214)    // bit field types other than int
#endif

/**
 * @defgroup PJMED_AMR_CODEC_HELPER AMR Codec Helper
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief AMR common tables and helper functions.
 * @{
 *
 * This sections describes common AMR constants tables (e.g: bits sensitivity 
 * order map, frame lengths, bitrates) and helper functions (e.g: pack AMR
 * payload in octet-aligned mode or bandwidth-efficient mode, payload parser,
 * reorder AMR bitstream).
 */

PJ_BEGIN_DECL


/* AMR bits sensitivity order maps */

const pj_int16_t pjmedia_codec_amrnb_ordermap122[244] =
{
      0,   1,   2,   3,   4,   5,   6,   7,   8,  9,
     10,  11,  12,  13,  14,  23,  15,  16,  17,  18,
     19,  20,  21,  22,  24,  25,  26,  27,  28,  38,
    141,  39, 142,  40, 143,  41, 144,  42, 145,  43,
    146,  44, 147,  45, 148,  46, 149,  47,  97, 150,
    200,  48,  98, 151, 201,  49,  99, 152, 202,  86,
    136, 189, 239,  87, 137, 190, 240,  88, 138, 191,
    241,  91, 194,  92, 195,  93, 196,  94, 197,  95,
    198,  29,  30,  31,  32,  33,  34,  35,  50, 100,
    153, 203,  89, 139, 192, 242,  51, 101, 154, 204,
     55, 105, 158, 208,  90, 140, 193, 243,  59, 109,
    162, 212,  63, 113, 166, 216,  67, 117, 170, 220,
     36,  37,  54,  53,  52,  58,  57,  56,  62,  61,
     60,  66,  65,  64,  70,  69,  68, 104, 103, 102,
    108, 107, 106, 112, 111, 110, 116, 115, 114, 120,
    119, 118, 157, 156, 155, 161, 160, 159, 165, 164,
    163, 169, 168, 167, 173, 172, 171, 207, 206, 205,
    211, 210, 209, 215, 214, 213, 219, 218, 217, 223,
    222, 221,  73,  72,  71,  76,  75,  74,  79,  78,
     77,  82,  81,  80,  85,  84,  83, 123, 122, 121,
    126, 125, 124, 129, 128, 127, 132, 131, 130, 135,
    134, 133, 176, 175, 174, 179, 178, 177, 182, 181,
    180, 185, 184, 183, 188, 187, 186, 226, 225, 224,
    229, 228, 227, 232, 231, 230, 235, 234, 233, 238,
    237, 236,  96, 199
};

const pj_int16_t pjmedia_codec_amrnb_ordermap102[204] =
{
      7,   6,   5,   4,   3,   2,   1,   0,  16,  15,
     14,  13,  12,  11,  10,   9,   8,  26,  27,  28,
     29,  30,  31, 115, 116, 117, 118, 119, 120,  72,
     73, 161, 162,  65,  68,  69, 108, 111, 112, 154,
    157, 158, 197, 200, 201,  32,  33, 121, 122,  74,
     75, 163, 164,  66, 109, 155, 198,  19,  23,  21,
     22,  18,  17,  20,  24,  25,  37,  36,  35,  34,
     80,  79,  78,  77, 126, 125, 124, 123, 169, 168,
    167, 166,  70,  67,  71, 113, 110, 114, 159, 156,
    160, 202, 199, 203,  76, 165,  81,  82,  92,  91,
     93,  83,  95,  85,  84,  94, 101, 102,  96, 104,
     86, 103,  87,  97, 127, 128, 138, 137, 139, 129,
    141, 131, 130, 140, 147, 148, 142, 150, 132, 149,
    133, 143, 170, 171, 181, 180, 182, 172, 184, 174,
    173, 183, 190, 191, 185, 193, 175, 192, 176, 186,
     38,  39,  49,  48,  50,  40,  52,  42,  41,  51,
     58,  59,  53,  61,  43,  60,  44,  54, 194, 179,
    189, 196, 177, 195, 178, 187, 188, 151, 136, 146,
    153, 134, 152, 135, 144, 145, 105,  90, 100, 107,
     88, 106,  89,  98,  99,  62,  47,  57,  64,  45,
     63,  46,  55,  56
};

const pj_int16_t pjmedia_codec_amrnb_ordermap795[159] =
{
      8,   7,   6,   5,   4,   3,   2,  14,  16,   9,
     10,  12,  13,  15,  11,  17,  20,  22,  24,  23,
     19,  18,  21,  56,  88, 122, 154,  57,  89, 123,
    155,  58,  90, 124, 156,  52,  84, 118, 150,  53,
     85, 119, 151,  27,  93,  28,  94,  29,  95,  30,
     96,  31,  97,  61, 127,  62, 128,  63, 129,  59,
     91, 125, 157,  32,  98,  64, 130,   1,   0,  25,
     26,  33,  99,  34, 100,  65, 131,  66, 132,  54,
     86, 120, 152,  60,  92, 126, 158,  55,  87, 121,
    153, 117, 116, 115,  46,  78, 112, 144,  43,  75,
    109, 141,  40,  72, 106, 138,  36,  68, 102, 134,
    114, 149, 148, 147, 146,  83,  82,  81,  80,  51,
     50,  49,  48,  47,  45,  44,  42,  39,  35,  79,
    77,   76,  74,  71,  67, 113, 111, 110, 108, 105,
    101, 145, 143, 142, 140, 137, 133,  41,  73, 107,
    139,  37,  69, 103, 135,  38,  70, 104, 136
};

const pj_int16_t pjmedia_codec_amrnb_ordermap74[148] =
{
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
     10,  11,  12,  13,  14,  15,  16,  26,  87,  27,
     88,  28,  89,  29,  90,  30,  91,  51,  80, 112,
    141,  52,  81, 113, 142,  54,  83, 115, 144,  55,
     84, 116, 145,  58, 119,  59, 120,  21,  22,  23,
     17,  18,  19,  31,  60,  92, 121,  56,  85, 117,
    146,  20,  24,  25,  50,  79, 111, 140,  57,  86,
    118, 147,  49,  78, 110, 139,  48,  77,  53,  82,
    114, 143, 109, 138,  47,  76, 108, 137,  32,  33,
     61,  62,  93,  94, 122, 123,  41,  42,  43,  44,
     45,  46,  70,  71,  72,  73,  74,  75, 102, 103,
    104, 105, 106, 107, 131, 132, 133, 134, 135, 136,
     34,  63,  95, 124,  35,  64,  96, 125,  36,  65,
     97, 126,  37,  66,  98, 127,  38,  67,  99, 128,
     39,  68, 100, 129,  40,  69, 101, 130
};

const pj_int16_t pjmedia_codec_amrnb_ordermap67[134] =
{
      0,   1,   4,   3,   5,   6,  13,   7,   2,   8,
      9,  11,  15,  12,  14,  10,  28,  82,  29,  83,
     27,  81,  26,  80,  30,  84,  16,  55, 109,  56,
    110,  31,  85,  57, 111,  48,  73, 102, 127,  32,
     86,  51,  76, 105, 130,  52,  77, 106, 131,  58,
    112,  33,  87,  19,  23,  53,  78, 107, 132,  21,
     22,  18,  17,  20,  24,  25,  50,  75, 104, 129,
     47,  72, 101, 126,  54,  79, 108, 133,  46,  71,
    100, 125, 128, 103,  74,  49,  45,  70,  99, 124,
     42,  67,  96, 121,  39,  64,  93, 118,  38,  63,
     92, 117,  35,  60,  89, 114,  34,  59,  88, 113,
     44,  69,  98, 123,  43,  68,  97, 122,  41,  66,
     95, 120,  40,  65,  94, 119,  37,  62,  91, 116,
     36,  61,  90, 115
};

const pj_int16_t pjmedia_codec_amrnb_ordermap59[118] =
{
      0,   1,   4,   5,   3,   6,   7,   2,  13,  15,
      8,   9,  11,  12,  14,  10,  16,  28,  74,  29,
     75,  27,  73,  26,  72,  30,  76,  51,  97,  50,
     71,  96, 117,  31,  77,  52,  98,  49,  70,  95,
    116,  53,  99,  32,  78,  33,  79,  48,  69,  94,
    115,  47,  68,  93, 114,  46,  67,  92, 113,  19,
     21,  23,  22,  18,  17,  20,  24, 111,  43,  89,
    110,  64,  65,  44,  90,  25,  45,  66,  91, 112,
     54, 100,  40,  61,  86, 107,  39,  60,  85, 106,
     36,  57,  82, 103,  35,  56,  81, 102,  34,  55,
     80, 101,  42,  63,  88, 109,  41,  62,  87, 108,
     38,  59,  84, 105,  37,  58,  83, 104
};

const pj_int16_t pjmedia_codec_amrnb_ordermap515[103] =
{
     7,   6,   5,   4,   3,   2,   1,   0,  15,  14,
    13,  12,  11,  10,   9,   8,  23,  24,  25,  26,
    27,  46,  65,  84,  45,  44,  43,  64,  63,  62,
    83,  82,  81, 102, 101, 100,  42,  61,  80,  99,
    28,  47,  66,  85,  18,  41,  60,  79,  98,  29,
    48,  67,  17,  20,  22,  40,  59,  78,  97,  21,
    30,  49,  68,  86,  19,  16,  87,  39,  38,  58,
    57,  77,  35,  54,  73,  92,  76,  96,  95,  36,
    55,  74,  93,  32,  51,  33,  52,  70,  71,  89,
    90,  31,  50,  69,  88,  37,  56,  75,  94,  34,
    53,  72,  91
};

const pj_int16_t pjmedia_codec_amrnb_ordermap475[95] =
{
     0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  23,  24,  25,  26,
    27,  28,  48,  49,  61,  62,  82,  83,  47,  46,
    45,  44,  81,  80,  79,  78,  17,  18,  20,  22,
    77,  76,  75,  74,  29,  30,  43,  42,  41,  40,
    38,  39,  16,  19,  21,  50,  51,  59,  60,  63,
    64,  72,  73,  84,  85,  93,  94,  32,  33,  35,
    36,  53,  54,  56,  57,  66,  67,  69,  70,  87,
    88,  90,  91,  34,  55,  68,  89,  37,  58,  71,
    92,  31,  52,  65,  86
};


const pj_int16_t pjmedia_codec_amrwb_ordermap_660[] =
{
      0,   5,   6,   7,  61,  84, 107, 130,  62,  85,
      8,   4,  37,  38,  39,  40,  58,  81, 104, 127,
     60,  83, 106, 129, 108, 131, 128,  41,  42,  80,
    126,   1,   3,  57, 103,  82, 105,  59,   2,  63,
    109, 110,  86,  19,  22,  23,  64,  87,  18,  20,
     21,  17,  13,  88,  43,  89,  65, 111,  14,  24,
     25,  26,  27,  28,  15,  16,  44,  90,  66, 112,
      9,  11,  10,  12,  67, 113,  29,  30,  31,  32,
     34,  33,  35,  36,  45,  51,  68,  74,  91,  97,
    114, 120,  46,  69,  92, 115,  52,  75,  98, 121,
     47,  70,  93, 116,  53,  76,  99, 122,  48,  71,
     94, 117,  54,  77, 100, 123,  49,  72,  95, 118,
     55,  78, 101, 124,  50,  73,  96, 119,  56,  79,
    102, 125
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_885[] =
{
      0,   4,   6,   7,   5,   3,  47,  48,  49, 112,
    113, 114,  75, 106, 140, 171,  80, 111, 145, 176,
     77, 108, 142, 173,  78, 109, 143, 174,  79, 110,
    144, 175,  76, 107, 141, 172,  50, 115,  51,   2,
      1,  81, 116, 146,  19,  21,  12,  17,  18,  20,
     16,  25,  13,  10,  14,  24,  23,  22,  26,   8,
     15,  52, 117,  31,  82, 147,   9,  33,  11,  83,
    148,  53, 118,  28,  27,  84, 149,  34,  35,  29,
     46,  32,  30,  54, 119,  37,  36,  39,  38,  40,
     85, 150,  41,  42,  43,  44,  45,  55,  60,  65,
     70,  86,  91,  96, 101, 120, 125, 130, 135, 151,
    156, 161, 166,  56,  87, 121, 152,  61,  92, 126,
    157,  66,  97, 131, 162,  71, 102, 136, 167,  57,
     88, 122, 153,  62,  93, 127, 158,  67,  98, 132,
    163,  72, 103, 137, 168,  58,  89, 123, 154,  63,
     94, 128, 159,  68,  99, 133, 164,  73, 104, 138,
    169,  59,  90, 124, 155,  64,  95, 129, 160,  69,
    100, 134, 165,  74, 105, 139, 170
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_1265[] =
{
      0,   4,   6,  93, 143, 196, 246,   7,   5,   3,
     47,  48,  49,  50,  51, 150, 151, 152, 153, 154,
     94, 144, 197, 247,  99, 149, 202, 252,  96, 146,
    199, 249,  97, 147, 200, 250, 100, 203,  98, 148,
    201, 251,  95, 145, 198, 248,  52,   2,   1, 101,
    204, 155,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    156,  31, 102, 205,   9,  33,  11, 103, 206,  54,
    157,  28,  27, 104, 207,  34,  35,  29,  46,  32,
     30,  55, 158,  37,  36,  39,  38,  40, 105, 208,
     41,  42,  43,  44,  45,  56, 106, 159, 209,  57,
     66,  75,  84, 107, 116, 125, 134, 160, 169, 178,
    187, 210, 219, 228, 237,  58, 108, 161, 211,  62,
    112, 165, 215,  67, 117, 170, 220,  71, 121, 174,
    224,  76, 126, 179, 229,  80, 130, 183, 233,  85,
    135, 188, 238,  89, 139, 192, 242,  59, 109, 162,
    212,  63, 113, 166, 216,  68, 118, 171, 221,  72,
    122, 175, 225,  77, 127, 180, 230,  81, 131, 184,
    234,  86, 136, 189, 239,  90, 140, 193, 243,  60,
    110, 163, 213,  64, 114, 167, 217,  69, 119, 172,
    222,  73, 123, 176, 226,  78, 128, 181, 231,  82,
    132, 185, 235,  87, 137, 190, 240,  91, 141, 194,
    244,  61, 111, 164, 214,  65, 115, 168, 218,  70,
    120, 173, 223,  74, 124, 177, 227,  79, 129, 182,
    232,  83, 133, 186, 236,  88, 138, 191, 241,  92,
    142, 195, 245
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_1425[] =
{
      0,   4,   6, 101, 159, 220, 278,   7,   5,   3,
     47,  48,  49,  50,  51, 166, 167, 168, 169, 170,
    102, 160, 221, 279, 107, 165, 226, 284, 104, 162,
    223, 281, 105, 163, 224, 282, 108, 227, 106, 164,
    225, 283, 103, 161, 222, 280,  52,   2,   1, 109,
    228, 171,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    172,  31, 110, 229,   9,  33,  11, 111, 230,  54,
    173,  28,  27, 112, 231,  34,  35,  29,  46,  32,
     30,  55, 174,  37,  36,  39,  38,  40, 113, 232,
     41,  42,  43,  44,  45,  56, 114, 175, 233,  62,
    120, 181, 239,  75, 133, 194, 252,  57, 115, 176,
    234,  63, 121, 182, 240,  70, 128, 189, 247,  76,
    134, 195, 253,  83, 141, 202, 260,  92, 150, 211,
    269,  84, 142, 203, 261,  93, 151, 212, 270,  85,
    143, 204, 262,  94, 152, 213, 271,  86, 144, 205,
    263,  95, 153, 214, 272,  64, 122, 183, 241,  77,
    135, 196, 254,  65, 123, 184, 242,  78, 136, 197,
    255,  87, 145, 206, 264,  96, 154, 215, 273,  58,
    116, 177, 235,  66, 124, 185, 243,  71, 129, 190,
    248,  79, 137, 198, 256,  88, 146, 207, 265,  97,
    155, 216, 274,  59, 117, 178, 236,  67, 125, 186,
    244,  72, 130, 191, 249,  80, 138, 199, 257,  89,
    147, 208, 266,  98, 156, 217, 275,  60, 118, 179,
    237,  68, 126, 187, 245,  73, 131, 192, 250,  81,
    139, 200, 258,  90, 148, 209, 267,  99, 157, 218,
    276,  61, 119, 180, 238,  69, 127, 188, 246,  74,
    132, 193, 251,  82, 140, 201, 259,  91, 149, 210,
    268, 100, 158, 219, 277
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_1585[] =
{
      0,   4,   6, 109, 175, 244, 310,   7,   5,   3,
     47,  48,  49,  50,  51, 182, 183, 184, 185, 186,
    110, 176, 245, 311, 115, 181, 250, 316, 112, 178,
    247, 313, 113, 179, 248, 314, 116, 251, 114, 180,
    249, 315, 111, 177, 246, 312,  52,   2,   1, 117,
    252, 187,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    188,  31, 118, 253,   9,  33,  11, 119, 254,  54,
    189,  28,  27, 120, 255,  34,  35,  29,  46,  32,
     30,  55, 190,  37,  36,  39,  38,  40, 121, 256,
     41,  42,  43,  44,  45,  56, 122, 191, 257,  63,
    129, 198, 264,  76, 142, 211, 277,  89, 155, 224,
    290, 102, 168, 237, 303,  57, 123, 192, 258,  70,
    136, 205, 271,  83, 149, 218, 284,  96, 162, 231,
    297,  62, 128, 197, 263,  75, 141, 210, 276,  88,
    154, 223, 289, 101, 167, 236, 302,  58, 124, 193,
    259,  71, 137, 206, 272,  84, 150, 219, 285,  97,
    163, 232, 298,  59, 125, 194, 260,  64, 130, 199,
    265,  67, 133, 202, 268,  72, 138, 207, 273,  77,
    143, 212, 278,  80, 146, 215, 281,  85, 151, 220,
    286,  90, 156, 225, 291,  93, 159, 228, 294,  98,
    164, 233, 299, 103, 169, 238, 304, 106, 172, 241,
    307,  60, 126, 195, 261,  65, 131, 200, 266,  68,
    134, 203, 269,  73, 139, 208, 274,  78, 144, 213,
    279,  81, 147, 216, 282,  86, 152, 221, 287,  91,
    157, 226, 292,  94, 160, 229, 295,  99, 165, 234,
    300, 104, 170, 239, 305, 107, 173, 242, 308,  61,
    127, 196, 262,  66, 132, 201, 267,  69, 135, 204,
    270,  74, 140, 209, 275,  79, 145, 214, 280,  82,
    148, 217, 283,  87, 153, 222, 288,  92, 158, 227,
    293,  95, 161, 230, 296, 100, 166, 235, 301, 105,
    171, 240, 306, 108, 174, 243, 309
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_1825[] =
{
      0,   4,   6, 121, 199, 280, 358,   7,   5,   3,
     47,  48,  49,  50,  51, 206, 207, 208, 209, 210,
    122, 200, 281, 359, 127, 205, 286, 364, 124, 202,
    283, 361, 125, 203, 284, 362, 128, 287, 126, 204,
    285, 363, 123, 201, 282, 360,  52,   2,   1, 129,
    288, 211,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    212,  31, 130, 289,   9,  33,  11, 131, 290,  54,
    213,  28,  27, 132, 291,  34,  35,  29,  46,  32,
     30,  55, 214,  37,  36,  39,  38,  40, 133, 292,
     41,  42,  43,  44,  45,  56, 134, 215, 293, 198,
    299, 136, 120, 138,  60, 279,  58,  62, 357, 139,
    140, 295, 156,  57, 219, 297,  63, 217, 137, 170,
    300, 222,  64, 106,  61,  78, 294,  92, 142, 141,
    135, 221, 296, 301, 343,  59, 298, 184, 329, 315,
    220, 216, 265, 251, 218, 237, 352, 223, 157,  86,
    171,  87, 164, 351, 111, 302,  65, 178, 115, 323,
     72, 192, 101, 179,  93,  73, 193, 151, 337, 309,
    143, 274,  69, 324, 165, 150,  97, 338, 110, 310,
    330, 273,  68, 107, 175, 245, 114,  79, 113, 189,
    246, 259, 174,  71, 185,  96, 344, 100, 322,  83,
    334, 316, 333, 252, 161, 348, 147,  82, 269, 232,
    260, 308, 353, 347, 163, 231, 306, 320, 188, 270,
    146, 177, 266, 350, 256,  85, 149, 116, 191, 160,
    238, 258, 336, 305, 255,  88, 224,  99, 339, 230,
    228, 227, 272, 242, 241, 319, 233, 311, 102,  74,
    180, 275,  66, 194, 152, 325, 172, 247, 244, 261,
    117, 158, 166, 354,  75, 144, 108, 312,  94, 186,
    303,  80, 234,  89, 195, 112, 340, 181, 345, 317,
    326, 276, 239, 167, 118, 313,  70, 355, 327, 253,
    190, 176, 271, 104,  98, 153, 103,  90,  76, 267,
    277, 248, 225, 262, 182,  84, 154, 235, 335, 168,
    331, 196, 341, 249, 162, 307, 148, 349, 263, 321,
    257, 243, 229, 356, 159, 119,  67, 187, 173, 145,
    240,  77, 304, 332, 314, 342, 109, 254,  81, 278,
    105,  91, 346, 318, 183, 250, 197, 328,  95, 155,
    169, 268, 226, 236, 264
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_1985[] =
{
      0,   4,   6, 129, 215, 304, 390,   7,   5,   3,
     47,  48,  49,  50,  51, 222, 223, 224, 225, 226,
    130, 216, 305, 391, 135, 221, 310, 396, 132, 218,
    307, 393, 133, 219, 308, 394, 136, 311, 134, 220,
    309, 395, 131, 217, 306, 392,  52,   2,   1, 137,
    312, 227,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    228,  31, 138, 313,   9,  33,  11, 139, 314,  54,
    229,  28,  27, 140, 315,  34,  35,  29,  46,  32,
     30,  55, 230,  37,  36,  39,  38,  40, 141, 316,
     41,  42,  43,  44,  45,  56, 142, 231, 317,  63,
     73,  92, 340,  82, 324, 149, 353, 159, 334, 165,
    338, 178, 163, 254,  77, 168, 257, 153, 343,  57,
    248, 238,  79, 252, 166,  67,  80, 201, 101, 267,
    143, 164, 341, 255, 339, 187, 376, 318,  78, 328,
    362, 115, 232, 242, 253, 290, 276,  62,  58, 158,
     68,  93, 179, 319, 148, 169, 154,  72, 385, 329,
    333, 344, 102,  83, 144, 233, 323, 124, 243, 192,
    354, 237,  64, 247, 202, 209, 150, 116, 335, 268,
    239, 299, 188, 196, 298,  94, 195, 258, 123, 363,
    384, 109, 325, 371, 170, 370,  84, 110, 295, 180,
     74, 210, 191, 106, 291, 205, 367, 381, 377, 206,
    355, 122, 119, 120, 383, 160, 105, 108, 277, 380,
    294, 284, 285, 345, 208, 269, 249, 366, 386, 300,
    297, 259, 125, 369, 197,  97, 194, 286, 211, 281,
    280, 183, 372,  87, 155, 283,  59, 348, 327, 184,
     76, 111, 330, 203, 349,  69,  98, 152, 145, 189,
     66, 320, 337, 173, 358, 251, 198, 174, 263, 262,
    126, 241, 193,  88, 388, 117,  95, 387, 112, 359,
    287, 244, 103, 272, 301, 171, 162, 234, 273, 127,
    373, 181, 292,  85, 378, 302, 121, 107, 364, 346,
    356, 212, 278, 213,  65, 382, 288, 207, 113, 175,
     99, 296, 374, 368, 199, 260, 185, 336, 331, 161,
    270, 264, 250, 240,  75, 350, 151,  60,  89, 321,
    156, 274, 360, 326,  70, 282, 167, 146, 352,  81,
     91, 389, 266, 245, 177, 235, 190, 256, 204, 342,
    128, 118, 303, 104, 379, 182, 114, 375, 200,  96,
    293, 172, 214, 365, 279,  86, 289, 351, 347, 357,
    261, 186, 176, 271,  90, 100, 147, 322, 275, 361,
     71, 332,  61, 265, 157, 246, 236
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_2305[] =
{
      0,   4,   6, 145, 247, 352, 454,   7,   5,   3,
     47,  48,  49,  50,  51, 254, 255, 256, 257, 258,
    146, 248, 353, 455, 151, 253, 358, 460, 148, 250,
    355, 457, 149, 251, 356, 458, 152, 359, 150, 252,
    357, 459, 147, 249, 354, 456,  52,   2,   1, 153,
    360, 259,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    260,  31, 154, 361,   9,  33,  11, 155, 362,  54,
    261,  28,  27, 156, 363,  34,  35,  29,  46,  32,
     30,  55, 262,  37,  36,  39,  38,  40, 157, 364,
     41,  42,  43,  44,  45,  56, 158, 263, 365, 181,
    192, 170,  79,  57, 399,  90, 159, 297, 377, 366,
    275,  68, 183, 388, 286, 194, 299,  92,  70, 182,
    401, 172,  59,  91,  58, 400, 368, 161,  81, 160,
    264, 171,  80, 389, 390, 378, 379, 193, 298,  69,
    266, 265, 367, 277, 288, 276, 287, 184,  60, 195,
     82,  93,  71, 369, 402, 173, 162, 444, 300, 391,
     98,  76, 278,  61, 267, 374, 135, 411, 167, 102,
    380, 200,  87, 178,  65,  94, 204, 124,  72, 342,
    189, 305, 381, 396, 433, 301, 226, 407, 289, 237,
    113, 215, 185, 128, 309, 403, 116, 320, 196, 331,
    370, 422, 174,  64, 392,  83, 425, 219, 134, 188,
    432, 112, 427, 139, 279, 163, 436, 208, 447, 218,
    236, 229,  97, 294, 385, 230, 166, 268, 177, 443,
    225, 426, 101, 272, 138, 127, 290, 117, 347, 199,
    414,  95, 140, 240, 410, 395, 209, 129, 283, 346,
    105, 241, 437,  86, 308, 448, 203, 345, 186, 107,
    220, 415, 334, 319, 106, 313, 118, 123,  73, 207,
    421, 214, 384, 373, 438,  62, 371, 341,  75, 449,
    168, 323, 164, 242, 416, 324, 304, 197, 335, 404,
    271,  63, 191, 325,  96, 169, 231, 280, 312, 187,
    406,  84, 201, 100,  67, 382, 175, 336, 202, 330,
    269, 393, 376, 383, 293, 307, 409, 179, 285, 314,
    302, 372, 398, 190, 180,  89,  99, 103, 232,  78,
     88,  77, 136, 387, 165, 198, 394, 125, 176, 428,
     74, 375, 238, 227,  66, 273, 282, 141, 306, 412,
    114,  85, 130, 348, 119, 291, 296, 386, 233, 397,
    303, 405, 284, 445, 423, 221, 210, 205, 450, 108,
    274, 434, 216, 343, 337, 142, 243, 321, 408, 451,
    310, 292, 120, 109, 281, 439, 270, 429, 332, 295,
    418, 211, 315, 222, 326, 131, 430, 244, 327, 349,
    417, 316, 143, 338, 440, 234, 110, 212, 452, 245,
    121, 419, 350, 223, 132, 441, 328, 413, 317, 339,
    126, 104, 137, 446, 344, 239, 435, 115, 333, 206,
    322, 217, 228, 424, 453, 311, 351, 111, 442, 224,
    213, 122, 431, 340, 235, 246, 133, 144, 420, 329,
    318
};

const pj_int16_t pjmedia_codec_amrwb_ordermap_2385[] =
{
      0,   4,   6, 145, 251, 360, 466,   7,   5,   3,
     47,  48,  49,  50,  51, 262, 263, 264, 265, 266,
    146, 252, 361, 467, 151, 257, 366, 472, 148, 254,
    363, 469, 149, 255, 364, 470, 156, 371, 150, 256,
    365, 471, 147, 253, 362, 468,  52,   2,   1, 157,
    372, 267,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    268,  31, 152, 153, 154, 155, 258, 259, 260, 261,
    367, 368, 369, 370, 473, 474, 475, 476, 158, 373,
      9,  33,  11, 159, 374,  54, 269,  28,  27, 160,
    375,  34,  35,  29,  46,  32,  30,  55, 270,  37,
     36,  39,  38,  40, 161, 376,  41,  42,  43,  44,
     45,  56, 162, 271, 377, 185, 196, 174,  79,  57,
    411,  90, 163, 305, 389, 378, 283,  68, 187, 400,
    294, 198, 307,  92,  70, 186, 413, 176,  59,  91,
     58, 412, 380, 165,  81, 164, 272, 175,  80, 401,
    402, 390, 391, 197, 306,  69, 274, 273, 379, 285,
    296, 284, 295, 188,  60, 199,  82,  93,  71, 381,
    414, 177, 166, 456, 308, 403,  98,  76, 286,  61,
    275, 386, 135, 423, 171, 102, 392, 204,  87, 182,
     65,  94, 208, 124,  72, 350, 193, 313, 393, 408,
    445, 309, 230, 419, 297, 241, 113, 219, 189, 128,
    317, 415, 116, 328, 200, 339, 382, 434, 178,  64,
    404,  83, 437, 223, 134, 192, 444, 112, 439, 139,
    287, 167, 448, 212, 459, 222, 240, 233,  97, 302,
    397, 234, 170, 276, 181, 455, 229, 438, 101, 280,
    138, 127, 298, 117, 355, 203, 426,  95, 140, 244,
    422, 407, 213, 129, 291, 354, 105, 245, 449,  86,
    316, 460, 207, 353, 190, 107, 224, 427, 342, 327,
    106, 321, 118, 123,  73, 211, 433, 218, 396, 385,
    450,  62, 383, 349,  75, 461, 172, 331, 168, 246,
    428, 332, 312, 201, 343, 416, 279,  63, 195, 333,
     96, 173, 235, 288, 320, 191, 418,  84, 205, 100,
     67, 394, 179, 344, 206, 338, 277, 405, 388, 395,
    301, 315, 421, 183, 293, 322, 310, 384, 410, 194,
    184,  89,  99, 103, 236,  78,  88,  77, 136, 399,
    169, 202, 406, 125, 180, 440,  74, 387, 242, 231,
     66, 281, 290, 141, 314, 424, 114,  85, 130, 356,
    119, 299, 304, 398, 237, 409, 311, 417, 292, 457,
    435, 225, 214, 209, 462, 108, 282, 446, 220, 351,
    345, 142, 247, 329, 420, 463, 318, 300, 120, 109,
    289, 451, 278, 441, 340, 303, 430, 215, 323, 226,
    334, 131, 442, 248, 335, 357, 429, 324, 143, 346,
    452, 238, 110, 216, 464, 249, 121, 431, 358, 227,
    132, 453, 336, 425, 325, 347, 126, 104, 137, 458,
    352, 243, 447, 115, 341, 210, 330, 221, 232, 436,
    465, 319, 359, 111, 454, 228, 217, 122, 443, 348,
    239, 250, 133, 144, 432, 337, 326
};

/**
 * AMR-NB bitstream sensitivity order maps.
 */
const pj_int16_t* const pjmedia_codec_amrnb_ordermaps[8] =
{
    pjmedia_codec_amrnb_ordermap475,
    pjmedia_codec_amrnb_ordermap515,
    pjmedia_codec_amrnb_ordermap59,
    pjmedia_codec_amrnb_ordermap67,
    pjmedia_codec_amrnb_ordermap74,
    pjmedia_codec_amrnb_ordermap795,
    pjmedia_codec_amrnb_ordermap102,
    pjmedia_codec_amrnb_ordermap122
};

/**
 * AMR-WB bitstream sensitivity order maps.
 */
const pj_int16_t* const pjmedia_codec_amrwb_ordermaps[9] =
{
    pjmedia_codec_amrwb_ordermap_660,
    pjmedia_codec_amrwb_ordermap_885,
    pjmedia_codec_amrwb_ordermap_1265,
    pjmedia_codec_amrwb_ordermap_1425,
    pjmedia_codec_amrwb_ordermap_1585,
    pjmedia_codec_amrwb_ordermap_1825,
    pjmedia_codec_amrwb_ordermap_1985,
    pjmedia_codec_amrwb_ordermap_2305,
    pjmedia_codec_amrwb_ordermap_2385
};

/**
 * Constant of AMR-NB frame lengths in bytes.
 */
const pj_uint8_t  pjmedia_codec_amrnb_framelen[16] = 
    {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
/**
 * Constant of AMR-NB frame lengths in bits.
 */
const pj_uint16_t pjmedia_codec_amrnb_framelenbits[9] = 
    {95, 103, 118, 134, 148, 159, 204, 244, 39};
/**
 * Constant of AMR-NB bitrates.
 */
const pj_uint16_t pjmedia_codec_amrnb_bitrates[8] = 
    {4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200};

/**
 * Constant of AMR-WB frame lengths in bytes.
 */
const pj_uint8_t  pjmedia_codec_amrwb_framelen[16] = 
    {17, 23, 32, 37, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 0, 0};
/**
 * Constant of AMR-WB frame lengths in bits.
 */
const pj_uint16_t pjmedia_codec_amrwb_framelenbits[10] = 
    {132, 177, 253, 285, 317, 365, 397, 461, 477, 40};
/**
 * Constant of AMR-WB bitrates.
 */
const pj_uint16_t pjmedia_codec_amrwb_bitrates[9] = 
    {6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850};


/**
 * This structure describes AMR frame info, to be fitted into #pjmedia_frame
 * bit info.
 */
#pragma pack(1)
typedef struct pjmedia_codec_amr_bit_info {
    pj_uint8_t frame_type;	/**< AMR frame type.			*/
    pj_int8_t  mode;		/**< AMR mode.				*/
    pj_uint8_t start_bit;	/**< Frame start bit.			*/
    pj_uint8_t good_quality:1;	/**< Flag if frame is good/degraded.	*/
    pj_uint8_t STI:1;		/**< STI mode (first/update).		*/
} pjmedia_codec_amr_bit_info;
#pragma pack()


/**
 * This structure describes AMR settings.
 */
typedef struct pjmedia_codec_amr_pack_setting {
    pj_uint8_t amr_nb:1;	/**< Set 1 for AMR-NB, 0 for AMR-WB.	*/
    pj_uint8_t reorder:1;	/**< Reorder bitstream into descending 
				     sensitivity order or vice versa.	*/
    pj_uint8_t octet_aligned:1;	/**< TRUE if payload is in octet-aligned mode,
				     FALSE if payload is in bandwidth 
				     efficient mode.			*/
    pj_uint8_t cmr:4;		/**< Change Mode Request for remote
				     encoder.				*/
} pjmedia_codec_amr_pack_setting;


/**
 * Get AMR mode based on bitrate.
 *
 * @param bitrate   AMR bitrate.
 *
 * @return	    AMR mode.
 */
PJ_INLINE(pj_int8_t) pjmedia_codec_amr_get_mode(unsigned bitrate)
{
    pj_int8_t mode = -1;

    if(bitrate==4750){
	mode = 0;
    } else if(bitrate==5150){
	mode = 1;
    } else if(bitrate==5900){
	mode = 2;
    } else if(bitrate==6700){
	mode = 3;
    } else if(bitrate==7400){
	mode = 4;
    } else if(bitrate==7950){
	mode = 5;
    } else if(bitrate==10200){
	mode = 6;
    } else if(bitrate==12200){
	mode = 7;

    /* AMRWB */
    } else if(bitrate==6600){
	mode = 0;
    } else if(bitrate==8850){
	mode = 1;
    } else if(bitrate==12650){
	mode = 2;
    } else if(bitrate==14250){
	mode = 3;
    } else if(bitrate==15850){
	mode = 4;
    } else if(bitrate==18250){
	mode = 5;
    } else if(bitrate==19850){
	mode = 6;
    } else if(bitrate==23050){
	mode = 7;
    } else if(bitrate==23850){
	mode = 8;
    }
    return mode;
}

/**
 * Get AMR mode based on frame length.
 *
 * @param amrnb	    Set to PJ_TRUE for AMR-NB domain or PJ_FALSE for AMR-WB.
 * @param frame_len The frame length.
 *
 * @return	    AMR mode.
 */

PJ_INLINE(pj_int8_t) pjmedia_codec_amr_get_mode2(pj_bool_t amrnb,
						 unsigned frame_len)
{
    int i;

    if (amrnb) {
	for (i = 0; i < 9; ++i)
	    if (frame_len == pjmedia_codec_amrnb_framelen[i])
		return (pj_int8_t)i;
    } else {
	for (i = 0; i < 10; ++i) {
	    if (frame_len == pjmedia_codec_amrwb_framelen[i])
		return (pj_int8_t)i;
	}
    }
    
    pj_assert(!"Invalid AMR frame length");
    return -1;
}

/**
 * Prepare a frame before pass it to decoder. This function will do:
 * - reorder AMR bitstream from descending sensitivity order into 
 *   encoder bits order. This can be enabled/disabled via param 
 *   'setting' by setting/resetting field 'reorder'.
 * - align left the start bit (make the start_bit to be 0).
 *
 * @param in	    Input frame.
 * @param setting   Settings, see #pjmedia_codec_amr_pack_setting.
 * @param out	    Output frame.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_codec_amr_predecode(
			    const pjmedia_frame *in,
			    const pjmedia_codec_amr_pack_setting *setting,
			    pjmedia_frame *out)
{
    pj_int8_t    amr_bits[477 + 7] = {0};
    pj_int8_t   *p_amr_bits = &amr_bits[0];

    pj_uint8_t  *r = (pj_uint8_t*) in->buf;  /* read cursor */
    pj_uint8_t  *w = (pj_uint8_t*) out->buf; /* write cursor */

    /* env vars for AMR or AMRWB */
    pj_uint8_t		     SID_FT;
    const pj_uint8_t	    *framelen_tbl;
    const pj_uint16_t	    *framelenbit_tbl;
    const pj_uint16_t	    *bitrate_tbl;
    const pj_int16_t* const *order_maps;

    pjmedia_codec_amr_bit_info *in_info =
			    (pjmedia_codec_amr_bit_info*) &in->bit_info;
    pjmedia_codec_amr_bit_info *out_info =
			    (pjmedia_codec_amr_bit_info*) &out->bit_info;

    unsigned i;

    *out_info = *in_info;

    if (setting->amr_nb) {
	SID_FT		= 8;
	framelen_tbl	= pjmedia_codec_amrnb_framelen;
	framelenbit_tbl = pjmedia_codec_amrnb_framelenbits;
	bitrate_tbl	= pjmedia_codec_amrnb_bitrates;
	order_maps	= pjmedia_codec_amrnb_ordermaps;
    } else {
	SID_FT		= 9;
	framelen_tbl	= pjmedia_codec_amrwb_framelen;
	framelenbit_tbl = pjmedia_codec_amrwb_framelenbits;
	bitrate_tbl	= pjmedia_codec_amrwb_bitrates;
	order_maps	= pjmedia_codec_amrwb_ordermaps;
    }

    /* unpack AMR bitstream if there is any data */
    if (in_info->frame_type <= SID_FT) {
	i = 0;
	if (in_info->start_bit) {
	    for (; i < (unsigned)(8-in_info->start_bit); ++i)
		*p_amr_bits++ = (pj_uint8_t)
				((*r >> (7-in_info->start_bit-i)) & 1);
	    ++r;
	}
	for(; i < framelenbit_tbl[in_info->frame_type]; i += 8, ++r) {
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 7) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 6) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 5) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 4) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 3) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 2) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 1) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r ) & 1);
	}
    }

    if (in_info->frame_type < SID_FT) {

	/* Speech */
	out_info->mode = in_info->frame_type;
	out->size = framelen_tbl[out_info->mode];

	pj_bzero(out->buf, out->size);

	if (setting->reorder) {
	    const pj_int16_t *order_map;

	    order_map = order_maps[out_info->mode];
	    for(i = 0; i < framelenbit_tbl[out_info->mode]; ++i) {
		if (amr_bits[i]) {
		    pj_uint16_t bitpos;
		    bitpos = order_map[i];
		    w[bitpos>>3] |= 1 << (7 - (bitpos % 8));
		}
	    }
	} else {
	    for(i = 0; i < framelenbit_tbl[out_info->mode]; ++i) {
		if (amr_bits[i])
		    w[i >> 3] |= 1 << (7 - (i % 8));
	    }
	}

    } else if (in_info->frame_type == SID_FT) {

	/* SID */
	pj_uint8_t w_bitptr = 0;
	pj_uint8_t FT_;

	if (setting->amr_nb)
	    FT_ = (pj_uint8_t)((amr_bits[36] << 2) | (amr_bits[37] << 1) | 
	                       amr_bits[38]);
	else
	    FT_ = (pj_uint8_t)((amr_bits[36] << 3) | (amr_bits[37] << 2) |
		               (amr_bits[38] << 1) | amr_bits[39]);

	out_info->mode = FT_;
	out->size = 5;

	pj_bzero(out->buf, out->size);
	for(i = 0; i < framelenbit_tbl[SID_FT]; ++i) {
	    if (amr_bits[i])
		*w |= (1 << (7-w_bitptr));

	    if (++w_bitptr == 8) {
		++w;
		w_bitptr = 0;
	    }
	}

    } else {

	/* NO DATA */
	out->size = 0;
	out_info->mode = -1;
    }

    out_info->start_bit = 0;

    return PJ_SUCCESS;
}


/**
 * Pack encoded AMR frame(s) into an RTP payload.
 *
 * @param frames    AMR frames to be packed.
 * @param nframes   Number of frames to be packed.
 * @param setting   Settings, see #pjmedia_codec_amr_pack_setting.
 * @param pkt	    Payload.
 * @param pkt_size  Payload size, as input this specifies payload maximum size,
 *		    as output this specifies payload packed size.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE (pj_status_t) pjmedia_codec_amr_pack(
			    const pjmedia_frame frames[],
			    unsigned nframes,
			    const pjmedia_codec_amr_pack_setting *setting,
			    void *pkt, 
			    pj_size_t *pkt_size)
{
    /* Write cursor */
    pj_uint8_t *w = (pj_uint8_t*)pkt;
    pj_uint8_t w_bitptr = 0;

    /* Read cursor */
    pj_uint8_t *r;

    /* env vars for AMR or AMRWB */
    pj_uint8_t		     SID_FT;
    const pj_uint8_t	    *framelen_tbl;
    const pj_uint16_t	    *framelenbit_tbl;
    const pj_uint16_t	    *bitrate_tbl;
    const pj_int16_t* const *order_maps;

    /* frame info */
    pjmedia_codec_amr_bit_info *info;

    unsigned i, max_pkt_size;

    max_pkt_size = *pkt_size;

    if (setting->amr_nb) {
	SID_FT		= 8;
	framelen_tbl	= pjmedia_codec_amrnb_framelen;
	framelenbit_tbl = pjmedia_codec_amrnb_framelenbits;
	bitrate_tbl	= pjmedia_codec_amrnb_bitrates;
	order_maps	= pjmedia_codec_amrnb_ordermaps;
    } else {
	SID_FT		= 9;
	framelen_tbl	= pjmedia_codec_amrwb_framelen;
	framelenbit_tbl = pjmedia_codec_amrwb_framelenbits;
	bitrate_tbl	= pjmedia_codec_amrwb_bitrates;
	order_maps	= pjmedia_codec_amrwb_ordermaps;
    }

    /* Code Mode Request, 4 bits */
    *w = (pj_uint8_t)(setting->cmr << 4);
    w_bitptr = 4;
    if (setting->octet_aligned) {
	++w;
	w_bitptr = 0;
    }

    /* Table Of Contents, 6 bits each */
    for (i = 0; i < nframes; ++i) {
	pj_uint8_t TOC, FT, Q;
	pj_bool_t F;

	info = (pjmedia_codec_amr_bit_info*)&frames[i].bit_info;

	F = (i != nframes-1);
	FT = info->frame_type;
	Q  = (pj_uint8_t)(info->good_quality == 1);
	pj_assert(FT <= SID_FT || FT == 14 || FT == 15);

	/* Check buffer availability */
	*pkt_size = w - (pj_uint8_t*)pkt + 1;
	PJ_ASSERT_RETURN(*pkt_size <= max_pkt_size, PJ_ETOOSMALL);

	TOC = (pj_uint8_t)((F<<5) | (FT<<1) | Q);
	if (w_bitptr == 0) {
	    *w = (pj_uint8_t)(TOC<<2);
	    w_bitptr = 6;
	} else if (w_bitptr == 2) {
	    *w++ |= TOC;
	    w_bitptr = 0;
	} else if (w_bitptr == 4) {
	    *w++ |= TOC>>2;
	    *w = (pj_uint8_t)(TOC<<6);
	    w_bitptr = 2;
	} else if (w_bitptr == 6) {
	    *w++ |= TOC>>4;
	    *w = (pj_uint8_t)(TOC<<4);
	    w_bitptr = 4;
	}
	if (setting->octet_aligned) {
	    ++w;
	    w_bitptr = 0;
	}
    }

    /* Encoded data */
    for (i = 0; i < nframes; ++i) {
	pj_int8_t amr_bits[477 + 7] = {0};
	pj_int8_t *p_amr_bits = &amr_bits[0];
	unsigned j;

	info = (pjmedia_codec_amr_bit_info*)&frames[i].bit_info;

	/* Check buffer availability */
	*pkt_size = w - (pj_uint8_t*)pkt;
	if (info->frame_type <= SID_FT)
	    *pkt_size += framelen_tbl[info->frame_type] + 1;
	PJ_ASSERT_RETURN(*pkt_size <= max_pkt_size, PJ_ETOOSMALL);

	/* Skip if there is no data */
	if (info->frame_type > SID_FT)
	    continue;

	/* Unpack bits */
	r = (pj_uint8_t*) frames[i].buf;
	j = 0;
	if (info->start_bit) {
	    for (; j < (unsigned)(8-info->start_bit); ++j)
		*p_amr_bits++ = (pj_uint8_t)
				((*r >> (7-info->start_bit-j)) & 1);
	    ++r;
	}
	for(; j < framelenbit_tbl[info->frame_type]; j+=8, ++r) {
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 7) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 6) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 5) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 4) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 3) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 2) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 1) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r ) & 1);
	}

	if (info->frame_type < SID_FT) {

	    /* Speech */
	    if (w_bitptr == 0) *w = 0;

	    if (setting->reorder) {
		const pj_int16_t *order_map;

		/* Put bits in the packet, sensitivity descending ordered */
		order_map = order_maps[info->frame_type];
		for(j = 0; j < framelenbit_tbl[info->frame_type]; ++j) {
		    if (amr_bits[order_map[j]])
			*w |= (1 << (7-w_bitptr));

		    if (++w_bitptr == 8) {
			w_bitptr = 0;
			++w;
			*w = 0;
		    }
		}
	    } else {
		for(j = 0; j < framelenbit_tbl[info->frame_type]; ++j) {
		    if (amr_bits[j])
			*w |= (1 << (7-w_bitptr));

		    if (++w_bitptr == 8) {
			w_bitptr = 0;
			++w;
			*w = 0;
		    }
		}
	    }

	} else if (info->frame_type == SID_FT) {

	    /* SID */
	    amr_bits[35] |= info->STI;

	    if (setting->amr_nb) {
		amr_bits[36] = (pj_uint8_t)((info->mode >> 2) & 1);
		amr_bits[37] = (pj_uint8_t)((info->mode >> 1) & 1);
		amr_bits[38] = (pj_uint8_t)((info->mode) & 1);
	    } else {
		amr_bits[36] = (pj_uint8_t)((info->mode >> 3) & 1);
		amr_bits[37] = (pj_uint8_t)((info->mode >> 2) & 1);
		amr_bits[38] = (pj_uint8_t)((info->mode >> 1) & 1);
		amr_bits[39] = (pj_uint8_t)((info->mode) & 1);
	    }

	    if (w_bitptr == 0) *w = 0;
	    for(j = 0; j < framelenbit_tbl[info->frame_type]; ++j) {
		if (amr_bits[j])
		    *w |= (1 << (7-w_bitptr));

		if (++w_bitptr == 8) {
		    w_bitptr = 0;
		    ++w;
		    *w = 0;
		}
	    }
	}

	if (setting->octet_aligned) {
	    ++w;
	    w_bitptr = 0;
	}
    }

    *pkt_size = w - (pj_uint8_t*)pkt;
    if (w_bitptr)
	*pkt_size += 1;

    return PJ_SUCCESS;
}


/**
 * Parse AMR payload into frames.
 *
 * @param pkt	    Payload.
 * @param pkt_size  Payload size.
 * @param ts	    Base timestamp.
 * @param setting   Settings, see #pjmedia_codec_amr_pack_setting.
 * @param frames    Frames parsed.
 * @param nframes   Number of frames parsed.
 * @param cmr	    Change Mode Request message for local encoder.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_codec_amr_parse(
			     void *pkt, 
			     pj_size_t pkt_size, 
			     const pj_timestamp *ts,
			     const pjmedia_codec_amr_pack_setting* setting,
			     pjmedia_frame frames[], 
			     unsigned *nframes,
			     pj_uint8_t *cmr)
{
    unsigned cnt = 0;
    pj_timestamp ts_ = *ts;

    /* Read cursor */
    pj_uint8_t r_bitptr = 0;
    pj_uint8_t *r = (pj_uint8_t*)pkt;

    /* env vars for AMR or AMRWB */
    pj_uint8_t		     SID_FT;
    const pj_uint8_t	    *framelen_tbl;
    const pj_uint16_t	    *framelenbit_tbl;
    const pj_int16_t* const *order_maps;

    /* frame info */
    pjmedia_codec_amr_bit_info *info;

    if (setting->amr_nb) {
	SID_FT		= 8;
	framelen_tbl	= pjmedia_codec_amrnb_framelen;
	framelenbit_tbl = pjmedia_codec_amrnb_framelenbits;
	order_maps	= pjmedia_codec_amrnb_ordermaps;
    } else {
	SID_FT		= 9;
	framelen_tbl	= pjmedia_codec_amrwb_framelen;
	framelenbit_tbl = pjmedia_codec_amrwb_framelenbits;
	order_maps	= pjmedia_codec_amrwb_ordermaps;
    }

    PJ_UNUSED_ARG(pkt_size);

    /* Code Mode Request, 4 bits */
    *cmr = (pj_uint8_t)((*r >> 4) & 0x0F);
    r_bitptr = 4;
    if (setting->octet_aligned) {
	++r;
	r_bitptr = 0;
    }

    /* Table Of Contents, 6 bits each */
    for (;;) {
	pj_uint8_t TOC = 0;
	pj_uint8_t F, FT, Q;

	if (r_bitptr == 0) {
	    TOC = (pj_uint8_t)(*r >> 2);
	    r_bitptr = 6;
	} else if (r_bitptr == 2) {
	    TOC = (pj_uint8_t)(*r++ & 0x3F);
	    r_bitptr = 0;
	} else if (r_bitptr == 4) {
	    TOC = (pj_uint8_t)((*r++ & 0x0f) << 2);
	    TOC |= *r >> 6;
	    r_bitptr = 2;
	} else if (r_bitptr == 6) {
	    TOC = (pj_uint8_t)((*r++ & 0x03) << 4);
	    TOC |= *r >> 4;
	    r_bitptr = 4;
	}

	F = (pj_uint8_t)(TOC >> 5);
	FT = (pj_uint8_t)((TOC >> 1) & 0x0F);
	Q = (pj_uint8_t)(TOC & 1);

	if (FT > SID_FT && FT < 14) {
	    pj_assert(!"Invalid AMR frametype, stream may be corrupted!");
	    break;
	}

	if (setting->octet_aligned) {
	    ++r;
	    r_bitptr = 0;
	}

	/* Set frame attributes */
	info = (pjmedia_codec_amr_bit_info*) &frames[cnt].bit_info;
	info->frame_type = FT;
	info->mode = (pj_int8_t)((FT < SID_FT)? FT : -1);
	info->good_quality = (pj_uint8_t)(Q == 1);
	info->start_bit = 0;
	info->STI = 0;
	frames[cnt].timestamp = ts_;
	frames[cnt].type = PJMEDIA_FRAME_TYPE_AUDIO;

	/* AMR frame length is 20ms */
	ts_.u64 += setting->amr_nb? 160 : 320;
	
	if (++cnt == *nframes || !F)
	    break;
    }
    *nframes = cnt;

    cnt = 0;

    /* Speech frames */
    while (cnt < *nframes) {
	pj_uint8_t FT;

	info = (pjmedia_codec_amr_bit_info*) &frames[cnt].bit_info;
	FT = info->frame_type;

	frames[cnt].buf = r;
	info->start_bit = r_bitptr;

	if (FT == SID_FT) {
	    unsigned sti_bitptr;
	    sti_bitptr = r_bitptr + 35;
	    info->STI = (pj_uint8_t)
			(r[sti_bitptr >> 3] >> (7 - (sti_bitptr % 8))) & 1;
	}

	if (setting->octet_aligned) {
	    r += framelen_tbl[FT];
	    frames[cnt].size = framelen_tbl[FT];
	} else {
	    if (FT == 14 || FT == 15) {
		/* NO DATA */
		frames[cnt].size = 0;
	    } else {
		unsigned adv_bit;

		adv_bit = framelenbit_tbl[FT] + r_bitptr;
		r += adv_bit >> 3;
		r_bitptr = (pj_uint8_t)(adv_bit % 8);

		frames[cnt].size = adv_bit >> 3;
		if (r_bitptr)
		    ++frames[cnt].size;
	    }
	}
	++cnt;
    }

    return PJ_SUCCESS;
}


PJ_END_DECL

/**
 * @}
 */

#endif /* __PJMEDIA_CODECS_AMR_HELPER_H__ */
