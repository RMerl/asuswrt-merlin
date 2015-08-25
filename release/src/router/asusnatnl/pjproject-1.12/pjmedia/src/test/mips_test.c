/* $Id: mips_test.c 3816 2011-10-14 04:15:15Z bennylp $ */
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
#include "test.h"
#include <pjmedia-codec.h>

/* Define your CPU MIPS here!! */

#ifndef CPU_IPS
    /*
    For complete table see:
    http://en.wikipedia.org/wiki/Million_instructions_per_second

    Processor			    IPS/MHz
    -------------------------------------------
    PowerPC G3			    2.253 MIPS/MHz
    Intel Pentium III		    2.708 MIPS/MHz
    AMD Athlon			    2.967 MIPS/MHz
    Pentium 4 Extreme Edition	    3.039 MIPS/MHz
    AMD Athlon FX-57		    4.285 MIPS/MHz


    From: http://en.wikipedia.org/wiki/ARM_architecture

    Family	Arch		Core	    IPS/MHz
    -------------------------------------------------------
    ARM7TDMI	ARMv4T		ARM7TDMI    0.892 MIPS/MHz
				ARM710T	    0.900 MIPS/MHz
				ARM720T	    1.003 MIPS/MHz
    ARM9TDMI	ARMv4T		ARM920T	    1.111 MIPS/MHz
    ARM9E	ARMv5TEJ	ARM926EJ-S  1.100 MIPS/MHz
    XScale	ARMv5TE		PXA255	    1.000 MIPS/MHz (?)
				PXA27x	    1.282 MIPS/MHz
    Cortex	ARMv7-A		Cortex-A8   2.000 MIPS/MHz
				Cortex-A9   2.000 MIPS/MHz
		ARMv7-M		Cortex-M3   1.250 MIPS/MHz
    */

#   define CPU_MHZ	    (2666)
#   define CPU_IPS	    (3.039 * CPU_MHZ * MEGA)	/* P4 2.6GHz	*/

//#   define CPU_MHZ	    700
//#   define CPU_IPS	    (700 * MEGA * 2.708)	/* P3 700Mhz	*/

//#   define CPU_MHZ	    180
//#   define CPU_IPS	    (CPU_MHZ * MEGA * 1.1)	/* ARM926EJ-S */

//#   define CPU_MHZ	    312
//#   define CPU_IPS	    (CPU_MHZ * MEGA * 1.282)	/* Dell Axim PDA */

#endif



/* Sample speech data, 360ms length, encoded at 8Khz */
static const pj_int16_t ref_signal[] = {
         0,    -4,     0,     0,     1,     8,     8,     7,    12,    16,
        20,    29,    28,    48,    40,    48,    56,    76,    68,    76,
        96,   100,   104,   124,   117,   120,   144,   120,   136,   168,
       184,   188,   176,   216,   237,   235,   268,   301,   312,   316,
       367,   356,   384,   400,   344,   409,   392,   416,   380,   432,
       404,   444,   457,   456,   453,   512,   499,   512,   584,   584,
       544,   584,   608,   596,   552,   628,   600,   667,   728,   672,
       681,   755,   736,   764,   752,   764,   724,   769,   792,   840,
       820,   895,   841,   856,   852,   867,   944,   944,   939,   944,
       907,   928,   920,   960,   833,   881,  1004,  1007,  1000,  1057,
      1032,  1056,  1016,  1056,  1072,  1080,  1108,  1028,  1076,  1072,
       956,  1020,  1080,  1008,  1095,   992,  1056,  1028,   940,   976,
      1008,   940,   916,   968,   849,   868,   956,   940,   852,   892,
       968,   860,   777,   904,   825,   716,   764,   708,   635,   728,
       620,   648,   472,   724,   548,   448,   472,   372,   272,   437,
       419,   260,   237,   371,   196,   136,   177,   264,    49,   120,
        40,   124,    32,   136,    71,   120,   -95,   -20,   -76,  -140,
      -304,    12,  -148,  -168,  -192,   -63,  -212,   -36,  -288,  -232,
      -352,  -128,  -397,  -308,  -431,  -280,  -675,  -497,  -761,  -336,
      -760,  -471, -1088,  3013, -1596, -2000,   412,  -968,   213,  -668,
     -1096, -1048, -1039,  -825,  -580,  -612, -1056,  -888, -1324, -1064,
     -1164,  -819,  -940,  -780, -1236,  -688, -1931,    -1,  -464, -1804,
      1088, -1605, -1208,  -664,  -912,  -905,  -832, -1167,  -512,  1832,
      -924,   -60,   660, -2143,   248, -1660,  1496, -3464,  -164,  2072,
     -3040,   539,  2904,  2040, -3488,  2600,  2412,   820,  -551, -1401,
      1068,  -385,   397, -2112,  -980,  1064, -1244,  -736, -1335,   332,
     -1232, -1852,   -12, -1073, -1747, -3328,  -796, -2241, -4901, -3248,
     -3124, -3432, -5892, -3840, -3968, -4752, -5668, -4796, -3580, -5909,
     -5412, -6144, -5800, -5908, -6696, -6460, -8609, -3804, -5668, -8499,
     -4636, -5744, -2377, -7132, -3712, -7221, -6608,  1656,-11728, -6656,
     -3736,  -204, -8580, -4808,  -591, -5752,  -472,-10308, -2116,  -257,
     -4720, -7640, -1279,  6412,-16404, -1495,  6204, -8072,  -335, -3877,
     -2363,   464,   441, -6872,  1447,  7884,-13197,   936,  5009, -4416,
     -4445,  3956,  2280, -2844,  2036, -4285,   744,  4161, -7216,  5548,
       172,  -964, -2873,  3296,  2184, -7424,  4300,  1855,  1453,   -32,
      1585,  2160, -3440,   448,  4084, -1617,  1928,  3944, -3728,  4699,
      4556, -5556,  4096, 12928, -8287, -4320, 10739,  3172, -6068,  3684,
      6484,  1652, -1104, -1820, 11964, -1567, -4117,  7197,  5112, -2608,
     -2363,  7379,   936,  1596,   217,  6508,  3284,  3960,     0,  2691,
     11324, -6140,  6720,  6188,  3596, -1120,  5319,  9420, -9360,  5780,
      5135,   623, -1844,  3473,  8488, -4532,  2844,  8368,  4387, -8628,
     14180,  3196, -4852,  9092,  5540,  -600,  3088,  9864, -4992, 13984,
      2536, -5932, 10584,  7044, -2548,   388, 12597, -4776,  -247,  7728,
      6048, -6152,  6449,  9644, -8924,  8993,  6319,   877,  1108,  9804,
      -696,  2584,  9097, -3236,  4308,  5444, -2660,  -365, 11427, -6900,
      -803,  9388, -2444, -1068,  9160,   703, -5632, 12088, -2964, -1077,
      9804, -1263, -3679, 10161,  3337, -9755, 11601,  -160, -6421, 11008,
     -1952, -3411,  6792, -1665, -1344,  9116, -2545, -4100, 11577,   240,
     -3612,  5140,   603, -2100,  4284,  -784,   108,   968, -1244,  3500,
      3696,  -108,  3780,  3836,   -16,  4035,  2772,  1300,  -688,  1544,
      2268, -2144,  1156,  -564,   628, -1040,  -168,   491,   -72,  -408,
     -1812,  3460, -2083,   -72,   797,  1436, -3824,  1200,   308, -3512,
       572, -4060,   540,   -84, -4492, -1808,  4644, -4340, -3224,  5832,
     -2180, -2544,  1475,  2224, -2588,  1312,  1452, -1676,  -428, -1596,
      -860,  -116, -4000,  -364,   148, -3680, -2229, -1632,   236, -3004,
     -1917,   124, -1748, -2991,  -644,  -752, -3691, -1945, -3236, -2005,
     -4388, -2084, -2052, -3788, -3100,  -824, -2432, -3419, -1905, -2520,
     -2120, -2904, -2200, -1712, -2500, -2796, -1140, -2460, -2955,  -984,
     -1519,  -400,  -412,   356,    97,  -389,  1708,  -115,   452,  1140,
      -820,  1912,  1421, -1604,   556,  -632, -1991, -1409, -1904, -3604,
     -3292, -3405, -5584, -4891, -5436, -8940, -6280, -6604,-11764, -6389,
     -9081,-10928, -7784, -8492,-11263, -8292, -8829, -9632, -7828, -8920,
    -10224, -8912, -9836, -7313, -2916,-10240, -3748,  2860, -3308, -1236,
      6816,  4580,  1585,  9808,  7484,  5612,  6868,  7399,  6244,  5064,
      3823,  5159,  4940,   316,  4496,  4864,  1020,  3997,  6236,  3316,
      5712,  7032,  5568,  7329,  6135,  6904,  6431,  3768,  2580,  3724,
       504, -2213,  -661, -3320, -6660, -6696, -7971,-11208,-11316,-11784,
    -14212,-13651,-16312,-15876,-15984,-20283,-15168,  2073,-23888, -5839,
     13360, -8568,  1240, 18480, 11440,  4236, 23916, 15388, 14072, 15960,
     15064, 14840,  9395,  6981,  8584,  6540, -5453,  3568,   928, -7741,
     -5260,   -12, -5692, -7608,  1408,  2136,   776,  1775, 13568, 10992,
      8445, 17527, 21084, 14851, 15820, 23060, 15988, 11560, 15088, 14164,
      3376,  4059,  5212, -2520, -5891, -3596, -5080,-11752, -8861, -8124,
    -12104,-12076,-10028,-12333,-14180,-12516,-16036,-15559,-20416, -4240,
     -1077,-31052, 14840,  7405,-12660, 11896, 23572,  2829, 10932, 28444,
     10268, 15412, 13896, 16976, 10161,  6172,  5336,  9639, -2208, -7160,
      6544, -7188,-11280, -3308, -2428,-13447, -4880,  -824, -6664, -1436,
      4608,  7473,  2688, 14275, 14921, 13564, 15960, 20393, 16976, 14832,
     17540, 13524, 10744,  6876,  7328,  1772, -2340, -3528, -4516, -9576,
    -10872, -8640,-13092,-12825,-14076,-12192,-16620,-16207,-17004,-17548,
    -22088,-21700,-20320,  2836,-29576,-15860, 25811,-22555, -1868, 23624,
      9872, -4044, 29472, 16037,  7433, 16640, 14577, 13119,  3120,  7072,
      5828,  2285,-12087,  3180, -4031,-17840, -6349, -5300,-15452,-13852,
     -2659,-12079, -8568, -4492,  -672,   660,  5424,  3040, 16488, 11408,
      8996, 24976, 15120,  9940, 21400, 16885,  2624, 13939,  8644, -2815,
       332,  -160, -9312,-10172, -8320,-14033,-13056,-16200,-14113,-15712,
    -18153,-18664,-15937,-21692,-23500,-18597,-25948, -8597,-10368,-32768,
     16916, -4469,-17121, 18448, 14791, -4252, 18880, 22312,  4347, 17672,
     12672, 12964,  7384,  5404,  5176,  5668, -7692, -2356,  1148,-14872,
     -8920, -5593,-12753,-14600, -6429,-10608,-10372, -6757, -4857, -2044,
     -2720,  8995,  5088,  6516, 18384, 12853, 14952, 18048, 17439, 13920,
     15512, 10960, 10268,  5136,  2888,  1184, -4271, -7104, -7376, -9688,
    -14691,-11336,-14073,-17056,-14268,-16776,-17957,-19460,-18068,-23056,
    -20512,-24004, -3396,-19239,-27272, 22283,-16439, -7300, 19484,  9020,
     -1088, 22895, 15868,  9640, 17344, 11443, 17912,  6084,  6712,  9988,
      6104, -8559,  6403, -1196,-13152, -3632, -5388,-11924,-11973, -5748,
    -10292, -8420, -8468, -2256, -2829, -4132,  6344,  8785,  7444,  9508,
     22152, 15108, 13151, 22536, 20725, 10672, 17028, 17052,  5536,  6192,
      7484,   403, -5520, -2132, -5532,-11527,-10644, -9572,-13316,-16124,
    -10883,-15965,-17335,-17316,-16064,-20436,-21660, -8547, -3732,-32768,
     14672,  2213,-17200, 17964, 14387,  4232, 14800, 24296, 11288, 21368,
     11144, 22992, 13599,  6973, 14444, 12312, -2340,  4436,  8060, -9008,
     -2188, -2164, -5756,-10268, -5076, -6292, -6472, -7916, -2336,   327,
     -4492,  7144,  7696,  5691, 16352, 14244, 17764, 19852, 17296, 23160,
     18496, 14197, 19564, 13356,  5779, 10559,  4576, -2736,  -528, -3211,
     -8796, -8428, -9153,-10928,-13296,-12101,-12528,-14985,-16036,-14600,
    -15888,-18792,-19604, -3176, -8887,-29240, 21405, -6999, -9568, 19052,
     11952,  3037, 20055, 18376, 14501, 18672, 11023, 24776,  9488,  7921,
     15896, 11068, -4484,  9667,  4328, -7544, -1240, -1456, -7204, -9192,
     -5084, -5816, -6864, -9444,   276, -2316, -2852,  4640,  9600,  4412,
     13300, 16856, 12836, 18892, 17632, 18336, 16031, 14808, 13860, 12052,
      4284,  7372,  2623, -4284, -2172, -5036,-10163, -9788,-10384,-13205,
    -13180,-13453,-14460,-15540,-16580,-15472,-17961,-19148,-18824, -8063,
     -8620,-28300, 14323, -6748,-10204, 13100, 10548,   956, 16056, 14640,
     12680, 14171,  9672, 19741,  7524,  6615, 11825,  8788, -5080,  7224,
      1112, -6024, -4176, -1912, -7968, -8576, -7184, -5640, -8200, -9388,
     -1980, -2064, -4040,   240,  9327,  2192,  8451, 13604, 13124, 10057,
     16505, 15099, 11008, 10344, 12405,  7244,  1948,  4420,   132, -5312,
     -6072, -5772,-11608,-11756,-11660,-12684,-14335,-14548,-12400,-15268,
    -15277,-14949,-14889,-17376,-16640,-15656, -1128,-23476, -6084,  7268,
    -13880,   400, 10984,  1468,  4388, 14156,  6600, 13684,  5428, 12240,
     11815,  5460,  3663, 13164, -1269,   772,  3884,  -788, -5536, -1652,
     -4857, -4596, -7912, -6152, -4132, -7201, -6288, -1196, -1332, -4236,
      5020,  5020,  1648,  8572, 10224,  6256,  9816,  9404,  8124,  6644,
      4380,  4707,   636, -3300, -3208, -4395, -9716, -7540, -8175, -9980,
    -10237, -7680,-12172, -9981,-10459, -9712, -8451,-13008,-10196, -9308,
    -13109,-11700,-11636, -6143, -9472,-12117,   985, -3627, -6120,  2828,
      5268,    33,  6984,  6488,  7803,  6265,  6992,  8032,  7892,  3408,
      6021,  6224,  1016,  2053,  2632,  -648, -1936, -1796, -2504, -2865,
     -4400, -2524, -2388, -2524, -1432,   283,   696,  1180,  2912,  3996,
      3328,  3948,  5101,  4905,  3396,  3500,  3323,  2472,  -152,  1580,
      -860, -2109, -1331, -2460, -2960, -3396, -3476, -2616, -5172, -3352,
     -4036, -4440, -5480, -4028, -4220, -5876, -4656, -5233, -4621, -5465,
     -6417, -4936, -5092, -1860, -5651, -2699,  1273,  -920,  -888,  4279,
      3260,  2952,  5861,  5584,  5980,  6428,  5732,  6516,  6825,  4156,
      5000,  5071,  2669,  1764,  3273,  1148,  1312,   880,  1788,  1457,
      1299,   648,  3172,  2004,  1060,  3544,  1963,  2516,  3192,  3057,
      2936,  2892,  2896,  2224,  3188,  1524,  1541,  3120,   624,   917,
      1472,  1128,  -317,   687,   400, -1131,   164,  -812, -1232, -1120,
     -1311, -1364, -1500, -1660, -2380, -1835, -2456, -2468, -2168, -2785,
     -2824, -2408, -3316,  -552, -1204, -3153,  1188,  1572,  -752,  1756,
      4108,  2344,  3595,  4504,  4152,  4556,  4224,  3568,  4801,  3165,
      2776,  2808,  3233,  1300,  2411,  1536,  1828,  1424,  1576,  1412,
      1880,   895,  1601,  1916,  1763,   944,  2599,  1349,  1873,  1988,
      1744,  1956,  1667,  1548,  1812,  1048,  1528,   488,  1428,   832,
       232,   207,   632,  -152,  -520,    20,    15, -1580,  -841,  -948,
     -1120, -1455, -2004, -1244, -1344, -2236, -1312, -1344, -2156, -1420,
     -1856, -1637, -1847, -1460, -1780, -1372,  -508, -1256,  -752,     0,
       600,   859,  1156,  1532,  2364,  2204,  2059,  2269,  2240,  1928,
      1889,  2055,  1205,  1068,  1268,   892,  1371,  1036,   413,  1552,
       572,   -84,  1364,   260,   624,   820,  -160,  1492,   188,   204,
       796,   468,   552,   945,  -504,   697,  -936,   284, -1116,  -204,
     -1052,  -700, -1637,  -673, -2744,   -25, -2163, -1728, -1704, -1260,
     -2228, -2512,  -496, -3992,  -824, -2699, -2172, -2196, -1684, -3376,
      -540, -3047, -2220,  -376, -3416,     8, -2424,  -428, -1111,  -927,
        68, -1152,   640, -1308,   276,   536,   592,  -292,  2256,  -368,
       680,  2532,  -536,  1548,   780,   -92,  1596,    56,   444,   348,
       356,   500, -2168,  1527, -1632,  -677,  -980,  -904,  -868,   480,
     -1476,  -804, -1515,  -335, -2472, -1533, -1007,  -644, -2308, -1592,
      -104, -3860,  -984, -2364,     0, -1372, -2500, -2548,  1280, -3767,
     -2228,  -440, -2168, -2691,   628, -2013, -3773, -4292,  3796, -6452,
     -1268,  -156, -1320, -3779,  2612, -2924,  -864,  -619,  1227, -3408,
      3016,  -200, -1432,  2219,   -45, -1356,  5744, -2069,  4396,   488,
      3048,  -801,  3876,   857,  2064,    80,  4240,  -700,  1928,  1568,
     -1992,  3431,   600,  2221,   657, -3116,  5888, -2668,  4871, -7963,
      8016, -4907,  1264, -1969,  3688, -4396, -1276,  2448, -3760,  2156,
     -3039, -3064,   940,  2384, -7907,  4520, -2876,  2384, -5784,  4484,
     -5487,  1907, -4064,  1991, -3496,  1304, -4521,  5255, -4189,  1136,
     -2397,  -152,   768, -1671,  2084, -2709,  6413, -1460,  1952,   448,
      7064,  -444,  6256,   600,  8872,  2115,  4672,  7628,  6264,  2993,
      8920,  2876,  7744,  3956,  4848,  7236,   804,  7684,  5201,  2216,
      6360,  4900,  -340,  6885,  1307,  2468,  1884,  4812, -1376,  4740,
      1751,   135,  1124,  3284, -3228,  3968,  1748, -4453,  1587, -1515,
      3084, -3300, -2548,  -544,  -296,   396, -7808,  4956, -5164,  -292,
     -4947,   212, -4055,  -108, -4301, -2088, -2912,  3016,   952, -1888,
      4972,  4441,  6136,  1464,  9387,  4137,  6812,  6281,  2440,  6940,
      3928,  2704, -1204,  4260, -2289,  -712, -1768,  -383, -1195,   920,
     -1200,  -336,  4372,   720,  4392,  1291,  5819,  4528,  7532,   992,
      8176,  5588,  2760,  2836,  3412,  1132,   531,  2236, -5044,   621,
     -2916, -3964, -5849,  -864, -6300,-10019, -3964, -5524, -7004, -6833,
     -7236, -9484, -2528,-10112,-12900, -4199, -8580,-12427,-10924, -8604,
    -11520, -9636, -6560, -1647, -6948,  -460,  1752,  2952,  4196,  4360,
      4215,  8156,  4528,  2464,  2500,  3299, -2224, -3812, -2568, -5875,
     -5556, -7728, -8288, -5765, -6448, -7620, -5288, -2680, -4368,  -972,
       472,  1716,  2467,  4408,  5141,  4724,  7316,  4132,  3493,  5935,
      3564,    96,  1068,   868, -2160, -2736, -3449, -5428, -3339, -5200,
     -7156, -4189, -7928, -8064, -7532, -7999,-12124, -8509, -9888,-12420,
    -13568,-13187,-15384,-14996,-15152,-15284,-17059,-15292,-11792, -1160,
     -7300, -8284,  7237,  7241,  1616,  6327, 12064,  7920,  9564,  3556,
      4612,  6980,   261, -6365, -2028, -1701,-10136, -9573, -6901, -7747,
     -7868, -8076, -6123, -1508,  -100, -3048,  2004,  6704,  4507,  3256,
      9432,  8672,  7673,  6804,  7632,  8777,  6908,  3332,  3771,  6552,
      3153,   400,  3029,  4388,  1328,   160,  2304,  2023,  1325, -2640,
     -2356, -1544, -3436, -8584, -6939, -7180,-10455,-12928,-12296,-14653,
    -15243,-16436,-15240,-16672,-15476,-14628,  7004, -1360,-10100, 16344,
     18300,  9108, 12869, 22541, 16119, 17856, 10697,  6720, 12128,  6904,
     -8184, -3440,  2592,-10440,-11735, -4739, -4455, -5457, -2432, -1476,
      4520, 10045,  5512,  7988, 17032, 15052,  9211, 13309, 14624, 10324,
     10488,  7809,  6908,  9896,  5861,  3284,  8348, 10505,  5189,  8144,
     13280, 11732, 10035, 12559, 12104, 12456, 10148,  6520,  5944,  5603,
     -1848, -4196, -2544, -5876,-11416,-10032,-10248,-12753,-13344,-14900,
    -14320,-11265,-14220,-17067, -1440, 20120, -9884,  2783, 32220, 22208,
      9032, 22661, 26820, 19916, 17747,  5288,  8628, 14293, -3331,-15672,
      1252,  -324,-18236,-11592, -1172, -3384, -3864,  1052,  3640, 13099,
     13691,  6520, 14320, 22856, 12887,  7152, 14764, 13276,  4060,  2568,
      2268,  2224,  3312, -3336,  -875,  9000,  6180,  1872, 10851, 17464,
     12312, 11197, 15388, 17816, 12024,  8332,  7119,  8096,  1608, -5611,
     -5964, -4729,-11317,-14784,-12833,-11272,-14888,-16128,-15012,-12028,
    -14472,-16227,-15356,-14484,-15056, 11496,   352,-14108, 19216, 24616,
      3724,  7872, 25948, 13832,  9680,  7492,  2052,  5220,  1188,-16112,
    -11340,   703,-15400,-21572, -5816, -3320,-12072, -5664,  2296,  3101,
      6708,  5396,  5735, 13601, 12040,  1924,  6071, 10420,   984, -4904,
      -204, -1945, -6229, -7460, -5636,  2864,  -476, -2832,  6104, 13160,
      7151,  7148, 13063, 13596,  8796,  5092,  5976,  5668,  -431, -7624,
     -6741, -5676,-14332,-18700,-13396,-12387,-18576,-17516,-14184,-14124,
    -15972,-17456,-16323,-14712,-18056,-23213,-10744, 12016,-14824,-12636,
     21656, 14112, -4085,  9255, 20864,  8196,  6384,  1223,  2244,  5304,
     -6660,-19192, -4961, -2875,-22564,-18400, -3220, -8488,-14544, -5040,
      -324,   820,  2732,   628,  5484, 11924,  4813,  -852,  8656,  7160,
     -3924, -2955,  1337, -3268, -7359, -2552, -2528,  -532,   128,   411,
      5324,  9301,  5601,  6200, 11684, 10072,  4924,  5508,  6660,  1568,
     -2332, -4268, -5628, -7987,-12004,-13760,-11567,-12104,-16539,-14437,
    -12012,-14309,-16736,-14573,-13604,-15468,-18204,-19103, -9140, 10132,
    -13631, -9568, 22580, 13756, -3548, 12112, 23891,  8144,  5964,  7240,
      7216,  4284, -4800,-11761, -1308, -3044,-19584,-13808,  -759, -7968,
    -14524, -1503,  3072,  -396,  1936,  5900,  9264, 10769,  7240,  5961,
     13112,  8788,   660,  2807,  7980,  -449, -2477,  3940,  2792,  1584,
      2791,  5603,  7528,  9692,  5924,  9123, 15240,  9636,  4924, 11044,
     11113,   956,   756,  2812, -1832, -6920, -7120, -7192, -7711, -9717,
    -12704, -8736, -7508,-12067,-13176, -8133, -9304,-13160,-13437,-13268,
     -4084, 11400,-12785,  -700, 24992, 12168, -1268, 19404, 25183,  8373,
     10256, 13664, 11200,  5879,   -60, -3656,  4556, -2972,-14688, -4932,
      2432, -9279,-10691,  4280,  3180, -2444,  4088,  9992,  9176,  9156,
      9520, 11164, 14484,  8608,  4919, 10556,  9792,  2740,  3456,  8840,
      6424,  2348,  5696,  9420,  6596,  5380,  8364, 10952,  8499,  6800,
      8728,  9641,  5412,  2340,  3596,  2039, -2864, -5489, -3616, -5596,
     -9232, -8744, -7788, -9860,-11104, -9356, -9464,-11188,-11312,-11036,
    -11736,-13564, -6016,  8744,-11784, -1196, 18972,  9512,  -572, 17407,
     20316,  7472,  9784, 13369,  8952,  5092,  1003, -2004,  2755, -3952,
    -12761, -4648,  -744,-11667,-10240,  1556, -1572, -5872,  2196,  6011,
      3900,  5384,  7529,  8924,  9629,  6324,  5744,  9484,  7829,  3420,
      4384,  8644,  4360,  1500,  5248,  5921,  2200,  2564,  5212,  5037,
      2849,  2836,  3985,  3952,   875,  -560,   416, -1052, -5228, -5185,
     -4996, -7820, -9616, -9076,-10644,-11239,-11816,-12360,-12228,-12420,
    -13560,-12940,-13044,-15648,-11664,  1945, -9676, -9088,  9676,  6708,
     -3048,  8185, 15520,  4620,  5764, 10716,  6584,  2684,  2276, -1436,
       -56, -2948, -9140, -6611, -2868, -9897,-10565, -2012, -3948, -7916,
     -1440,  2420,  -241,  1164,  4428,  4932,  5461,  3884,  4476,  6620,
      7724,  1779,  3172,  8256,  3132,  -749,  5192,  4300, -1388,  1192,
      3575,   789,  -228,  1185,   995,   937,  -952, -2624,  -449, -1992,
     -6204, -4648, -3000, -7604, -8536, -5868, -9024,-10507,-10064, -9296,
    -12896,-11120,-11776,-13288,-14137,-12668,-15780,-14157, -8392, -7444,
    -11156, -2300,  2828, -1747,  1164,  8152,  6280,  4876,  7912,  7604,
      5609,  5164,  2600,  1620,  1592, -3237, -4441, -2068, -5052, -8268,
     -4503, -3304, -6332, -4460,  -388,  -297,  -319,  1911,  4071,  4272,
      4659,  8368,  6933,  6720,  8764,  8640,  6412,  6384,  5927,  3820,
      3488,  2648,  1104,  1220,   884,  -692,   327,   616,  -972,  -160,
       713,  -593,  -652,   179,  -651, -2005,  -656, -1536, -2968, -3748,
     -2640, -5052, -5548, -3476, -6151, -6388, -5168, -6099, -7416, -5752,
     -7579, -8220, -8312, -8472, -5287, -8056, -3527, -2356, -1704,  1892,
      2408,  2893,  5965,  8121,  5136,  8480,  8928,  7364,  6408,  7960,
      4315,  4392,  3864,  1353,   928,  1436, -1480,  -488,  1640,  -380,
       -36,  3420,  4044,  4432,  5185,  8044,  8740,  7983,  7912,  9588,
      8588,  6804,  6944,  6700,  4308,  2852,  3252,  2192,  -136,   876,
      1008,   244,   160,   205,   992,  1684,  -136,   984,  3312,   853,
      -772,  2372,   436, -3008, -1024,  -136, -3800, -2263, -3212, -2749,
     -3688, -2424, -5372, -2136, -3288, -4952, -3596, -2028, -4640, -5797,
     -2696, -4040, -7152, -4055, -2568, -6460, -4228, -1092, -2780, -2492,
       468,  -235,  1620,  3500,  2040,  2840,  6300,  4488,  2488,  5707,
      5576,  3537,  2291,  4301,  2844,  3364,  1153,  2500,  3340,  3160,
      1224,  3220,  4016,  2228,  1788,  4199,  3604,  2096,  1763,  3237,
      2044,  -564,  1280,   876,  -584, -1904,    24,   -60, -2948, -1440,
     -1228, -1824, -2092, -1945, -3912,  -227, -2411, -3219, -2252, -1808,
     -3044, -1035, -3092, -1456, -3724, -2284, -3149, -3028, -2788, -1804,
     -3360, -1276, -4097, -2531, -2248, -1635, -3215, -2376, -2468, -2596,
     -2825, -2792, -1980, -4036, -1721, -2059, -4117,   364, -1452, -2772,
     -1336,   480, -1043,   244, -2904,   924, -1329,   968, -1891,   523,
      -624,  -464,  -564,   187,  -852,   584,  -764,  -260,  -147,   160,
       339,   -32,   936,  -896,   288,   136,    56,   -36,  -736,  -683,
      -332,   696, -2319,  -259,   564, -2196,  -860,  1108, -2177,  -728,
      1344, -2520,  -440,  1080,  -780, -3513,  3272, -1635, -1597,  -188,
       744, -1944,   140,  -636, -1644,  -141,  -596, -1132,  -816,  1168,
     -2836,   196,   312,   136, -1381,   628,  -223,  -368,  -425,   604,
      -776,   595,  -628,  -128,  -884,   960, -1092,    76,   144,     8,
       161,  -504,   760,  -808,   336,   185,   100,   404,   120,   236,
        68,  -148,   -64,   312,   320,  -560,   117,   -28,   236,  -231,
       -92,    60,   356,  -176,   176,   212,   124,   -57,   -76,   168,
        88,  -140,   -37,   160,     0,   -92,    96,    24,   -84,     0,
};

#define THIS_FILE	    "mips_test.c"
#define DURATION	    5000
#define PTIME		    20	/* MUST be 20! */
#define MEGA		    1000000
#define GIGA		    1000000000

enum op
{
    OP_GET  = 1,
    OP_PUT  = 2,
    OP_GET_PUT = 4,
    OP_PUT_GET = 8
};

enum clock_rate
{
    K8	= 1,
    K16	= 2,
    K32	= 4,
};


struct test_entry
{
    const char	    *title;
    unsigned	     valid_op;
    unsigned	     valid_clock_rate;

    pjmedia_port*  (*init)(pj_pool_t *pool,
			  unsigned clock_rate,
			  unsigned channel_count,
			  unsigned samples_per_frame,
			  unsigned flags,
			  struct test_entry *);
    void	   (*custom_run)(struct test_entry*);
    void	   (*custom_deinit)(struct test_entry*);

    void	    *pdata[4];
    unsigned	     idata[4];
};


/***************************************************************************/
/* pjmedia_port to supply with continuous frames */
static pjmedia_port* create_gen_port(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned pct_level)
{
    pjmedia_port *port;
    pj_status_t status;

    if (pct_level == 100 && channel_count==1) {
	status = pjmedia_mem_player_create(pool, ref_signal, 
					   sizeof(ref_signal), clock_rate, 
					   channel_count, samples_per_frame,
					   16, 0, &port);
    } else {
	pj_int16_t *buf;
	unsigned c;

	buf = (pj_int16_t*)
	      pj_pool_alloc(pool, sizeof(ref_signal)*channel_count);
	for (c=0; c<channel_count; ++c) {
	    unsigned i;
	    pj_int16_t *p;

	    p = buf+c;
	    for (i=0; i<PJ_ARRAY_SIZE(ref_signal); ++i) {
		*p = (pj_int16_t)(ref_signal[i] * pct_level / 100);
		p += channel_count;
	    }
	}
	status = pjmedia_mem_player_create(pool, buf, 
					   sizeof(ref_signal)*channel_count, 
					   clock_rate,  channel_count, 
					   samples_per_frame,
					   16, 0, &port);
    }

    return status==PJ_SUCCESS? port : NULL;
}

/***************************************************************************/
static pjmedia_port* gen_port_test_init(pj_pool_t *pool,
				        unsigned clock_rate,
				        unsigned channel_count,
				        unsigned samples_per_frame,
				        unsigned flags,
				        struct test_entry *te)
{
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);
    return create_gen_port(pool, clock_rate, channel_count, samples_per_frame,
			   100);
}


/***************************************************************************/
static pjmedia_port* init_conf_port(unsigned nb_participant,
				    pj_pool_t *pool,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned flags,
				    struct test_entry *te)
{
    pjmedia_conf *conf;
    unsigned i;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    /* Create conf */
    status = pjmedia_conf_create(pool, 2+nb_participant*2, clock_rate, 
				 channel_count, samples_per_frame, 16, 
				 PJMEDIA_CONF_NO_DEVICE, &conf);
    if (status != PJ_SUCCESS)
	return NULL;

    for (i=0; i<nb_participant; ++i) {
	pjmedia_port *gen_port, *null_port;
	unsigned slot1, slot2;

	/* Create gen_port for source audio */
	gen_port = create_gen_port(pool, clock_rate, channel_count,
				   samples_per_frame, 100 / nb_participant);
	if (!gen_port)
	    return NULL;

	/* Add port */
	status = pjmedia_conf_add_port(conf, pool, gen_port, NULL, &slot1);
	if (status != PJ_SUCCESS)
	    return NULL;

	/* Connect gen_port to sound dev */
	status = pjmedia_conf_connect_port(conf, slot1, 0, 0);
	if (status != PJ_SUCCESS)
	    return NULL;

	/* Create null sink frame */
	status = pjmedia_null_port_create(pool, clock_rate, channel_count,
					  samples_per_frame, 16, &null_port);
	if (status != PJ_SUCCESS)
	    return NULL;

	/* add null port */
	status = pjmedia_conf_add_port(conf, pool, null_port, NULL, &slot2);
	if (status != PJ_SUCCESS)
	    return NULL;

	/* connect sound to null sink port */
	status = pjmedia_conf_connect_port(conf, 0, slot2, 0);
	if (status != PJ_SUCCESS)
	    return NULL;

	/* connect gen_port to null sink port */
#if 0
	status = pjmedia_conf_connect_port(conf, slot1, slot2, 0);
	if (status != PJ_SUCCESS)
	    return NULL;
#endif	
    }

    return pjmedia_conf_get_master_port(conf);
}


/***************************************************************************/
/* Benchmark conf with 1 participant, no mixing */
static pjmedia_port* conf1_test_init(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return init_conf_port(1, pool, clock_rate, channel_count, 
			  samples_per_frame, flags, te);
}


/***************************************************************************/
/* Benchmark conf with 2 participants, mixing to/from snd dev */
static pjmedia_port* conf2_test_init(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return init_conf_port(2, pool, clock_rate, channel_count, 
			  samples_per_frame, flags, te);
}

/***************************************************************************/
/* Benchmark conf with 4 participants, mixing to/from snd dev */
static pjmedia_port* conf4_test_init(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return init_conf_port(4, pool, clock_rate, channel_count, 
			  samples_per_frame, flags, te);
}

/***************************************************************************/
/* Benchmark conf with 8 participants, mixing to/from snd dev */
static pjmedia_port* conf8_test_init(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return init_conf_port(8, pool, clock_rate, channel_count, 
			  samples_per_frame, flags, te);
}

/***************************************************************************/
/* Benchmark conf with 16 participants, mixing to/from snd dev */
static pjmedia_port* conf16_test_init(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    PJ_UNUSED_ARG(flags);
    return init_conf_port(16, pool, clock_rate, channel_count, 
			  samples_per_frame, flags, te);
}

/***************************************************************************/
/* Up and downsample */
static pjmedia_port* updown_resample_get(pj_pool_t *pool,
					 pj_bool_t high_quality,
					 pj_bool_t large_filter,
				         unsigned clock_rate,
				         unsigned channel_count,
				         unsigned samples_per_frame,
				         unsigned flags,
				         struct test_entry *te)
{
    pjmedia_port *gen_port, *up, *down;
    unsigned opt = 0;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    if (!high_quality)
	opt |= PJMEDIA_RESAMPLE_USE_LINEAR;
    if (!large_filter)
	opt |= PJMEDIA_RESAMPLE_USE_SMALL_FILTER;

    gen_port = create_gen_port(pool, clock_rate, channel_count,
			       samples_per_frame, 100);
    status = pjmedia_resample_port_create(pool, gen_port, clock_rate*2, opt, &up);
    if (status != PJ_SUCCESS)
	return NULL;
    status = pjmedia_resample_port_create(pool, up, clock_rate, opt, &down);
    if (status != PJ_SUCCESS)
	return NULL;

    return down;
}

/* Linear resampling */
static pjmedia_port* linear_resample( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return updown_resample_get(pool, PJ_FALSE, PJ_FALSE, clock_rate,
			       channel_count, samples_per_frame, flags, te);
}

/* Small filter resampling */
static pjmedia_port* small_filt_resample( pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return updown_resample_get(pool, PJ_TRUE, PJ_FALSE, clock_rate,
			       channel_count, samples_per_frame, flags, te);
}

/* Larger filter resampling */
static pjmedia_port* large_filt_resample( pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return updown_resample_get(pool, PJ_TRUE, PJ_TRUE, clock_rate,
			       channel_count, samples_per_frame, flags, te);
}


/***************************************************************************/
/* Codec encode/decode */

struct codec_port
{
    pjmedia_port     base;
    pjmedia_endpt   *endpt;
    pjmedia_codec   *codec;
    pj_status_t	   (*codec_deinit)();
    pj_uint8_t	     pkt[640];
    pj_uint16_t	     pcm[32000 * PTIME / 1000];
};


static pj_status_t codec_put_frame(struct pjmedia_port *this_port, 
				   const pjmedia_frame *frame)
{
    struct codec_port *cp = (struct codec_port*)this_port;
    pjmedia_frame out_frame;
    pj_status_t status;

    out_frame.buf = cp->pkt;
    out_frame.size = sizeof(cp->pkt);
    status = cp->codec->op->encode(cp->codec, frame, sizeof(cp->pkt),
				   &out_frame);
    pj_assert(status == PJ_SUCCESS);

    if (out_frame.size != 0) {
	pjmedia_frame parsed_frm[2], pcm_frm;
	unsigned frame_cnt = PJ_ARRAY_SIZE(parsed_frm);
	unsigned i;

	status = cp->codec->op->parse(cp->codec, out_frame.buf, 
				      out_frame.size, &out_frame.timestamp,
				      &frame_cnt, parsed_frm);
	pj_assert(status == PJ_SUCCESS);
	
	for (i=0; i<frame_cnt; ++i) {
	    pcm_frm.buf = cp->pcm;
	    pcm_frm.size = sizeof(cp->pkt);
	    status = cp->codec->op->decode(cp->codec, &parsed_frm[i], 
					   sizeof(cp->pcm), &pcm_frm);
	    pj_assert(status == PJ_SUCCESS);
	}
    }

    return PJ_SUCCESS;
}

static pj_status_t codec_on_destroy(struct pjmedia_port *this_port)
{
    struct codec_port *cp = (struct codec_port*)this_port;

    cp->codec->op->close(cp->codec);
    pjmedia_codec_mgr_dealloc_codec(pjmedia_endpt_get_codec_mgr(cp->endpt),
				    cp->codec);
    cp->codec_deinit();
    pjmedia_endpt_destroy(cp->endpt);
    return PJ_SUCCESS;
}

static pjmedia_port* codec_encode_decode( pj_pool_t *pool,
					  const char *codec,
					  pj_status_t (*codec_init)(pjmedia_endpt*),
					  pj_status_t (*codec_deinit)(),
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    struct codec_port *cp;
    pj_str_t codec_id;
    const pjmedia_codec_info *ci[1];
    unsigned count;
    pjmedia_codec_param codec_param;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    codec_id = pj_str((char*)codec);
    cp = PJ_POOL_ZALLOC_T(pool, struct codec_port);
    pjmedia_port_info_init(&cp->base.info, &codec_id, 0x123456, clock_rate,
			   channel_count, 16, samples_per_frame);
    cp->base.put_frame = &codec_put_frame;
    cp->base.on_destroy = &codec_on_destroy;
    cp->codec_deinit = codec_deinit;

    status = pjmedia_endpt_create(0, mem, NULL, 0, 0, &cp->endpt);
    if (status != PJ_SUCCESS)
	return NULL;

    status = codec_init(cp->endpt);
    if (status != PJ_SUCCESS)
	return NULL;

    count = 1;
    status = pjmedia_codec_mgr_find_codecs_by_id(pjmedia_endpt_get_codec_mgr(cp->endpt),
						 &codec_id, &count, ci, NULL);
    if (status != PJ_SUCCESS)
	return NULL;

    status = pjmedia_codec_mgr_alloc_codec(pjmedia_endpt_get_codec_mgr(cp->endpt),
					   ci[0], &cp->codec);
    if (status != PJ_SUCCESS)
	return NULL;

    status = pjmedia_codec_mgr_get_default_param(pjmedia_endpt_get_codec_mgr(cp->endpt),
						 ci[0], &codec_param);
    if (status != PJ_SUCCESS)
	return NULL;

    status = (*cp->codec->op->init)(cp->codec, pool);
    if (status != PJ_SUCCESS)
	return NULL;

    status = cp->codec->op->open(cp->codec, &codec_param);
    if (status != PJ_SUCCESS)
	return NULL;

    return &cp->base;
}

/* G.711 benchmark */
static pjmedia_port* g711_encode_decode(  pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return codec_encode_decode(pool, "pcmu", &pjmedia_codec_g711_init, 
			       &pjmedia_codec_g711_deinit,
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

/* GSM benchmark */
static pjmedia_port* gsm_encode_decode(  pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return codec_encode_decode(pool, "gsm", &pjmedia_codec_gsm_init, 
			       &pjmedia_codec_gsm_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

static pj_status_t ilbc_init(pjmedia_endpt *endpt)
{
    return pjmedia_codec_ilbc_init(endpt, 20);
}

/* iLBC benchmark */
static pjmedia_port* ilbc_encode_decode( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    samples_per_frame = 30 * clock_rate / 1000;
    return codec_encode_decode(pool, "ilbc", &ilbc_init, 
			       &pjmedia_codec_ilbc_deinit, clock_rate, 
			       channel_count, samples_per_frame, flags, te);
}

/* Speex narrowband benchmark */
static pjmedia_port* speex8_encode_decode(pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return codec_encode_decode(pool, "speex/8000", 
			       &pjmedia_codec_speex_init_default, 
			       &pjmedia_codec_speex_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

/* Speex wideband benchmark */
static pjmedia_port* speex16_encode_decode(pj_pool_t *pool,
					   unsigned clock_rate,
					   unsigned channel_count,
					   unsigned samples_per_frame,
					   unsigned flags,
					   struct test_entry *te)
{
    return codec_encode_decode(pool, "speex/16000", 
			       &pjmedia_codec_speex_init_default, 
			       &pjmedia_codec_speex_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

/* G.722 benchmark benchmark */
static pjmedia_port* g722_encode_decode(pj_pool_t *pool,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned flags,
					struct test_entry *te)
{
    return codec_encode_decode(pool, "g722", &pjmedia_codec_g722_init, 
			       &pjmedia_codec_g722_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

#if PJMEDIA_HAS_G7221_CODEC
/* G.722.1 benchmark benchmark */
static pjmedia_port* g7221_encode_decode(pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return codec_encode_decode(pool, "g7221/16000", 
			       &pjmedia_codec_g7221_init,
			       &pjmedia_codec_g7221_deinit,
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

/* G.722.1 Annex C benchmark benchmark */
static pjmedia_port* g7221c_encode_decode(pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return codec_encode_decode(pool, "g7221/32000", 
			       &pjmedia_codec_g7221_init,
			       &pjmedia_codec_g7221_deinit,
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}
#endif	/* PJMEDIA_HAS_G7221_CODEC */

#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
/* AMR-NB benchmark benchmark */
static pjmedia_port* amr_encode_decode(pj_pool_t *pool,
                                       unsigned clock_rate,
                                       unsigned channel_count,
                                       unsigned samples_per_frame,
                                       unsigned flags,
                                       struct test_entry *te)
{
    return codec_encode_decode(pool, "AMR/8000", 
			       &pjmedia_codec_opencore_amrnb_init,
			       &pjmedia_codec_opencore_amrnb_deinit,
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}
#endif	/* PJMEDIA_HAS_OPENCORE_AMRNB_CODEC */

#if defined(PJMEDIA_HAS_L16_CODEC) && PJMEDIA_HAS_L16_CODEC!=0
static pj_status_t init_l16_default(pjmedia_endpt *endpt)
{
    return pjmedia_codec_l16_init(endpt, 0);
}

/* L16/8000/1 benchmark */
static pjmedia_port* l16_8_encode_decode(pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return codec_encode_decode(pool, "L16/8000/1", &init_l16_default, 
			       &pjmedia_codec_l16_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}

/* L16/16000/1 benchmark */
static pjmedia_port* l16_16_encode_decode(pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return codec_encode_decode(pool, "L16/16000/1", &init_l16_default, 
			       &pjmedia_codec_l16_deinit, 
			       clock_rate, channel_count,
			       samples_per_frame, flags, te);
}
#endif

/***************************************************************************/
/* WSOLA PLC mode */

struct wsola_plc_port
{
    pjmedia_port     base;
    pjmedia_wsola   *wsola;
    pjmedia_port    *gen_port;
    int		     loss_pct;
    pj_bool_t	     prev_lost;
};


static pj_status_t wsola_plc_get_frame(struct pjmedia_port *this_port, 
				       pjmedia_frame *frame)
{
    struct wsola_plc_port *wp = (struct wsola_plc_port*)this_port;
    pj_status_t status;


    if ((pj_rand() % 100) > wp->loss_pct) {
	status = pjmedia_port_get_frame(wp->gen_port, frame);
	pj_assert(status == PJ_SUCCESS);

	status = pjmedia_wsola_save(wp->wsola, (short*)frame->buf, 
				    wp->prev_lost);
	pj_assert(status == PJ_SUCCESS);

	wp->prev_lost = PJ_FALSE;
    } else {
	status = pjmedia_wsola_generate(wp->wsola, (short*)frame->buf);
	wp->prev_lost = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

static pj_status_t wsola_plc_on_destroy(struct pjmedia_port *this_port)
{
    struct wsola_plc_port *wp = (struct wsola_plc_port*)this_port;
    pjmedia_port_destroy(wp->gen_port);
    pjmedia_wsola_destroy(wp->wsola);
    return PJ_SUCCESS;
}

static pjmedia_port* create_wsola_plc(unsigned loss_pct,
				      pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    struct wsola_plc_port *wp;
    pj_str_t name = pj_str("wsola");
    unsigned opt = 0;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    wp = PJ_POOL_ZALLOC_T(pool, struct wsola_plc_port);
    wp->loss_pct = loss_pct;
    wp->base.get_frame = &wsola_plc_get_frame;
    wp->base.on_destroy = &wsola_plc_on_destroy;
    pjmedia_port_info_init(&wp->base.info, &name, 0x4123, clock_rate, 
			   channel_count, 16, samples_per_frame);

    if (loss_pct == 0)
	opt |= PJMEDIA_WSOLA_NO_PLC;

    status = pjmedia_wsola_create(pool, clock_rate, samples_per_frame,
				  channel_count, 0, &wp->wsola);
    if (status != PJ_SUCCESS)
	return NULL;

    wp->gen_port = create_gen_port(pool, clock_rate, channel_count, 
				   samples_per_frame, 100);
    if (wp->gen_port == NULL)
	return NULL;

    return &wp->base;
}


/* WSOLA PLC with 0% packet loss */
static pjmedia_port* wsola_plc_0( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(0, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}


/* WSOLA PLC with 2% packet loss */
static pjmedia_port* wsola_plc_2( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(2, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}

/* WSOLA PLC with 5% packet loss */
static pjmedia_port* wsola_plc_5( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(5, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}

/* WSOLA PLC with 10% packet loss */
static pjmedia_port* wsola_plc_10(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(10, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}

/* WSOLA PLC with 20% packet loss */
static pjmedia_port* wsola_plc_20(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(20, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}

/* WSOLA PLC with 50% packet loss */
static pjmedia_port* wsola_plc_50(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_wsola_plc(50, pool, clock_rate, channel_count, 
			    samples_per_frame, flags, te);
}



/***************************************************************************/
/* WSOLA discard mode */
enum { CIRC_BUF_FRAME_CNT = 4 };
struct wsola_discard_port
{
    pjmedia_port	 base;
    pjmedia_port	*gen_port;
    pjmedia_wsola	*wsola;
    pjmedia_circ_buf	*circbuf;
    unsigned		 discard_pct;
};


static pj_status_t wsola_discard_get_frame(struct pjmedia_port *this_port, 
					   pjmedia_frame *frame)
{
    struct wsola_discard_port *wp = (struct wsola_discard_port*)this_port;
    pj_status_t status;

    while (pjmedia_circ_buf_get_len(wp->circbuf) <
		wp->base.info.samples_per_frame * (CIRC_BUF_FRAME_CNT-1))
    {
	status = pjmedia_port_get_frame(wp->gen_port, frame);
	pj_assert(status==PJ_SUCCESS);

	status = pjmedia_circ_buf_write(wp->circbuf, (short*)frame->buf, 
					wp->base.info.samples_per_frame);
	pj_assert(status==PJ_SUCCESS);
    }
    
    if ((pj_rand() % 100) < (int)wp->discard_pct) {
	pj_int16_t *reg1, *reg2;
	unsigned reg1_len, reg2_len;
	unsigned del_cnt;

	pjmedia_circ_buf_get_read_regions(wp->circbuf, &reg1, &reg1_len,
					  &reg2, &reg2_len);

	del_cnt = wp->base.info.samples_per_frame;
	status = pjmedia_wsola_discard(wp->wsola, reg1, reg1_len, reg2, 
				       reg2_len, &del_cnt);
	pj_assert(status==PJ_SUCCESS);

	status = pjmedia_circ_buf_adv_read_ptr(wp->circbuf, del_cnt);
	pj_assert(status==PJ_SUCCESS);
    }

    return PJ_SUCCESS;
}

static pj_status_t wsola_discard_on_destroy(struct pjmedia_port *this_port)
{
    struct wsola_discard_port *wp = (struct wsola_discard_port*)this_port;
    pjmedia_port_destroy(wp->gen_port);
    pjmedia_wsola_destroy(wp->wsola);
    return PJ_SUCCESS;
}


static pjmedia_port* create_wsola_discard(unsigned discard_pct,
					  pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    struct wsola_discard_port *wp;
    pj_str_t name = pj_str("wsola");
    unsigned i, opt = 0;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    wp = PJ_POOL_ZALLOC_T(pool, struct wsola_discard_port);
    wp->discard_pct = discard_pct;
    wp->base.get_frame = &wsola_discard_get_frame;
    wp->base.on_destroy = &wsola_discard_on_destroy;
    pjmedia_port_info_init(&wp->base.info, &name, 0x4123, clock_rate, 
			   channel_count, 16, samples_per_frame);

    if (discard_pct == 0)
	opt |= PJMEDIA_WSOLA_NO_DISCARD;

    status = pjmedia_wsola_create(pool, clock_rate, samples_per_frame,
				  channel_count, 0, &wp->wsola);
    if (status != PJ_SUCCESS)
	return NULL;

    wp->gen_port = create_gen_port(pool, clock_rate, channel_count, 
				   samples_per_frame, 100);
    if (wp->gen_port == NULL)
	return NULL;

    status = pjmedia_circ_buf_create(pool, samples_per_frame * CIRC_BUF_FRAME_CNT, 
				     &wp->circbuf);
    if (status != PJ_SUCCESS)
	return NULL;

    /* fill up the circbuf */
    for (i=0; i<CIRC_BUF_FRAME_CNT; ++i) {
	short pcm[320];
	pjmedia_frame frm;

	pj_assert(PJ_ARRAY_SIZE(pcm) >= samples_per_frame);
	frm.buf = pcm;
	frm.size = samples_per_frame * 2;

	status = pjmedia_port_get_frame(wp->gen_port, &frm);
	pj_assert(status==PJ_SUCCESS);

	status = pjmedia_circ_buf_write(wp->circbuf, pcm, samples_per_frame);
	pj_assert(status==PJ_SUCCESS);
    }

    return &wp->base;
}


/* WSOLA with 2% discard rate */
static pjmedia_port* wsola_discard_2( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return create_wsola_discard(2, pool, clock_rate, channel_count, 
			        samples_per_frame, flags, te);
}

/* WSOLA with 5% discard rate */
static pjmedia_port* wsola_discard_5( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return create_wsola_discard(5, pool, clock_rate, channel_count, 
			        samples_per_frame, flags, te);
}

/* WSOLA with 10% discard rate */
static pjmedia_port* wsola_discard_10(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return create_wsola_discard(10, pool, clock_rate, channel_count, 
			        samples_per_frame, flags, te);
}

/* WSOLA with 20% discard rate */
static pjmedia_port* wsola_discard_20(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return create_wsola_discard(20, pool, clock_rate, channel_count, 
			        samples_per_frame, flags, te);
}

/* WSOLA with 50% discard rate */
static pjmedia_port* wsola_discard_50(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned flags,
				      struct test_entry *te)
{
    return create_wsola_discard(50, pool, clock_rate, channel_count, 
			        samples_per_frame, flags, te);
}



/***************************************************************************/

static pjmedia_port* ec_create(unsigned ec_tail_msec,
			       pj_pool_t *pool,
			       unsigned clock_rate,
			       unsigned channel_count,
			       unsigned samples_per_frame,
			       unsigned flags,
			       struct test_entry *te)
{
    pjmedia_port *gen_port, *ec_port;
    pj_status_t status;

    PJ_UNUSED_ARG(te);

    gen_port = create_gen_port(pool, clock_rate, channel_count, 
			       samples_per_frame, 100);
    if (gen_port == NULL)
	return NULL;

    status = pjmedia_echo_port_create(pool, gen_port, ec_tail_msec, 0,
				      flags, &ec_port);
    if (status != PJ_SUCCESS)
	return NULL;

    return ec_port;
}

/* EC with 100ms tail length */
static pjmedia_port* ec_create_100(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(100, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 128ms tail length */
static pjmedia_port* ec_create_128(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(128, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 200ms tail length */
static pjmedia_port* ec_create_200(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(200, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 256ms tail length */
static pjmedia_port* ec_create_256(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(256, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}


/* EC with 400ms tail length */
static pjmedia_port* ec_create_400(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(400, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 500ms tail length */
static pjmedia_port* ec_create_500(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(500, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 512ms tail length */
static pjmedia_port* ec_create_512(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(512, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 600ms tail length */
static pjmedia_port* ec_create_600(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(600, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* EC with 800ms tail length */
static pjmedia_port* ec_create_800(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = 0;
    return ec_create(800, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}



/* Echo suppressor with 100ms tail length */
static pjmedia_port* es_create_100(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(100, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 128ms tail length */
static pjmedia_port* es_create_128(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(128, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 200ms tail length */
static pjmedia_port* es_create_200(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(200, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 256ms tail length */
static pjmedia_port* es_create_256(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(256, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}


/* Echo suppressor with 400ms tail length */
static pjmedia_port* es_create_400(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(400, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 500ms tail length */
static pjmedia_port* es_create_500(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(500, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 512ms tail length */
static pjmedia_port* es_create_512(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(512, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 600ms tail length */
static pjmedia_port* es_create_600(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(600, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}

/* Echo suppressor with 800ms tail length */
static pjmedia_port* es_create_800(pj_pool_t *pool,
				   unsigned clock_rate,
				   unsigned channel_count,
				   unsigned samples_per_frame,
				   unsigned flags,
				   struct test_entry *te)
{
    flags = PJMEDIA_ECHO_SIMPLE;
    return ec_create(800, pool, clock_rate, channel_count, samples_per_frame,
		     flags, te);
}


/***************************************************************************/
/* Tone generator, single frequency */
static pjmedia_port* create_tonegen(unsigned freq1,
				    unsigned freq2,
				    pj_pool_t *pool,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned flags,
				    struct test_entry *te)
{
    pjmedia_port *tonegen;
    pjmedia_tone_desc tones[2];
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    status = pjmedia_tonegen_create(pool, clock_rate, channel_count,
				    samples_per_frame, 16, 
				    PJMEDIA_TONEGEN_LOOP, &tonegen);
    if (status != PJ_SUCCESS)
	return NULL;

    pj_bzero(tones, sizeof(tones));
    tones[0].freq1 = (short)freq1;
    tones[0].freq2 = (short)freq2;
    tones[0].on_msec = 400;
    tones[0].off_msec = 0;
    tones[1].freq1 = (short)freq1;
    tones[1].freq2 = (short)freq2;
    tones[1].on_msec = 400;
    tones[1].off_msec = 100;

    status = pjmedia_tonegen_play(tonegen, PJ_ARRAY_SIZE(tones), tones, 
				  PJMEDIA_TONEGEN_LOOP);
    if (status != PJ_SUCCESS)
	return NULL;

    return tonegen;
}

/* Tonegen with single frequency */
static pjmedia_port* create_tonegen1(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return create_tonegen(400, 0, pool, clock_rate, channel_count,
			  samples_per_frame, flags, te);
}

/* Tonegen with dual frequency */
static pjmedia_port* create_tonegen2(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    return create_tonegen(400, 440, pool, clock_rate, channel_count,
			  samples_per_frame, flags, te);
}



/***************************************************************************/
/* Stream */

struct stream_port
{
    pjmedia_port	 base;
    pj_status_t	       (*codec_deinit)();
    pjmedia_endpt	*endpt;
    pjmedia_stream	*stream;
    pjmedia_transport	*transport;
};


static void stream_port_custom_deinit(struct test_entry *te)
{
    struct stream_port *sp = (struct stream_port*) te->pdata[0];

    pjmedia_stream_destroy(sp->stream);
    pjmedia_transport_close(sp->transport);
    sp->codec_deinit();
    pjmedia_endpt_destroy(sp->endpt);

}

static pjmedia_port* create_stream( pj_pool_t *pool,
				    const char *codec,
				    pj_status_t (*codec_init)(pjmedia_endpt*),
				    pj_status_t (*codec_deinit)(),
				    pj_bool_t srtp_enabled,
				    pj_bool_t srtp_80,
				    pj_bool_t srtp_auth,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned flags,
				    struct test_entry *te)
{
    struct stream_port *sp;
    pj_str_t codec_id;
    pjmedia_port *port;
    const pjmedia_codec_info *ci[1];
    unsigned count;
    pjmedia_codec_param codec_param;
    pjmedia_stream_info si;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);

    codec_id = pj_str((char*)codec);
    sp = PJ_POOL_ZALLOC_T(pool, struct stream_port);
    pjmedia_port_info_init(&sp->base.info, &codec_id, 0x123456, clock_rate,
			   channel_count, 16, samples_per_frame);

    te->pdata[0] = sp;
    te->custom_deinit = &stream_port_custom_deinit;
    sp->codec_deinit = codec_deinit;

    status = pjmedia_endpt_create(0, mem, NULL, 0, 0, &sp->endpt);
    if (status != PJ_SUCCESS)
	return NULL;

    status = codec_init(sp->endpt);
    if (status != PJ_SUCCESS)
	return NULL;

    count = 1;
    status = pjmedia_codec_mgr_find_codecs_by_id(pjmedia_endpt_get_codec_mgr(sp->endpt),
						 &codec_id, &count, ci, NULL);
    if (status != PJ_SUCCESS)
	return NULL;



    status = pjmedia_codec_mgr_get_default_param(pjmedia_endpt_get_codec_mgr(sp->endpt),
						 ci[0], &codec_param);
    if (status != PJ_SUCCESS)
	return NULL;

    /* Create stream info */
    pj_bzero(&si, sizeof(si));
    si.type = PJMEDIA_TYPE_AUDIO;
    si.proto = PJMEDIA_TP_PROTO_RTP_AVP;
    si.dir = PJMEDIA_DIR_ENCODING_DECODING;
    pj_sockaddr_in_init(&si.rem_addr.ipv4, NULL, 4000);
    pj_sockaddr_in_init(&si.rem_rtcp.ipv4, NULL, 4001);
    pj_memcpy(&si.fmt, ci[0], sizeof(pjmedia_codec_info));
    si.param = NULL;
    si.tx_pt = ci[0]->pt;
    si.tx_event_pt = 101;
    si.rx_event_pt = 101;
    si.ssrc = pj_rand();
    si.jb_init = si.jb_min_pre = si.jb_max_pre = si.jb_max = -1;

    /* Create loop transport */
    status = pjmedia_transport_loop_create(sp->endpt, &sp->transport);
    if (status != PJ_SUCCESS)
	return NULL;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    if (srtp_enabled) {
	pjmedia_srtp_setting opt;
	pjmedia_srtp_crypto crypto;
	pjmedia_transport *srtp;

	pjmedia_srtp_setting_default(&opt);
	opt.close_member_tp = PJ_TRUE;
	opt.use = PJMEDIA_SRTP_MANDATORY;

	status = pjmedia_transport_srtp_create(sp->endpt, sp->transport, &opt,
					       &srtp);
	if (status != PJ_SUCCESS)
	    return NULL;

	pj_bzero(&crypto, sizeof(crypto));
	if (srtp_80) {
	    crypto.key = pj_str("123456789012345678901234567890");
	    crypto.name = pj_str("AES_CM_128_HMAC_SHA1_80");
	} else {
	    crypto.key = pj_str("123456789012345678901234567890");
	    crypto.name = pj_str("AES_CM_128_HMAC_SHA1_32");
	}

	if (!srtp_auth)
	    crypto.flags = PJMEDIA_SRTP_NO_AUTHENTICATION;

	status = pjmedia_transport_srtp_start(srtp, &crypto, &crypto);
	if (status != PJ_SUCCESS)
	    return NULL;

	sp->transport = srtp;
    }
#endif

    /* Create stream */
    status = pjmedia_stream_create(sp->endpt, pool, &si, sp->transport, NULL, 
				   &sp->stream);
    if (status != PJ_SUCCESS)
	return NULL;

    /* Start stream */
    status = pjmedia_stream_start(sp->stream);
    if (status != PJ_SUCCESS)
	return NULL;

    status = pjmedia_stream_get_port(sp->stream, &port);
    if (status != PJ_SUCCESS)
	return NULL;

    return port;
}

/* G.711 stream, no SRTP */
static pjmedia_port* create_stream_pcmu( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return create_stream(pool, "pcmu", &pjmedia_codec_g711_init, 
			 &pjmedia_codec_g711_deinit,
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G.711 stream, SRTP 32bit key no auth */
static pjmedia_port* create_stream_pcmu_srtp32_no_auth( pj_pool_t *pool,
							unsigned clock_rate,
							unsigned channel_count,
							unsigned samples_per_frame,
							unsigned flags,
							struct test_entry *te)
{
    return create_stream(pool, "pcmu", &pjmedia_codec_g711_init, 
			 &pjmedia_codec_g711_deinit,
			 PJ_TRUE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G.711 stream, SRTP 32bit key with auth */
static pjmedia_port* create_stream_pcmu_srtp32_with_auth(pj_pool_t *pool,
							 unsigned clock_rate,
							 unsigned channel_count,
							 unsigned samples_per_frame,
							 unsigned flags,
							 struct test_entry *te)
{
    return create_stream(pool, "pcmu", &pjmedia_codec_g711_init, 
			 &pjmedia_codec_g711_deinit,
			 PJ_TRUE, PJ_FALSE, PJ_TRUE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G.711 stream, SRTP 80bit key no auth */
static pjmedia_port* create_stream_pcmu_srtp80_no_auth( pj_pool_t *pool,
							unsigned clock_rate,
							unsigned channel_count,
							unsigned samples_per_frame,
							unsigned flags,
							struct test_entry *te)
{
    return create_stream(pool, "pcmu", &pjmedia_codec_g711_init, 
			 &pjmedia_codec_g711_deinit,
			 PJ_TRUE, PJ_TRUE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G.711 stream, SRTP 80bit key with auth */
static pjmedia_port* create_stream_pcmu_srtp80_with_auth(pj_pool_t *pool,
							 unsigned clock_rate,
							 unsigned channel_count,
							 unsigned samples_per_frame,
							 unsigned flags,
							 struct test_entry *te)
{
    return create_stream(pool, "pcmu", &pjmedia_codec_g711_init, 
			 &pjmedia_codec_g711_deinit,
			 PJ_TRUE, PJ_TRUE, PJ_TRUE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* GSM stream */
static pjmedia_port* create_stream_gsm(  pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return create_stream(pool, "gsm", &pjmedia_codec_gsm_init, 
			 &pjmedia_codec_gsm_deinit, 
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* GSM stream, SRTP 32bit, no auth */
static pjmedia_port* create_stream_gsm_srtp32_no_auth(pj_pool_t *pool,
						      unsigned clock_rate,
						      unsigned channel_count,
						      unsigned samples_per_frame,
						      unsigned flags,
						      struct test_entry *te)
{
    return create_stream(pool, "gsm", &pjmedia_codec_gsm_init, 
			 &pjmedia_codec_gsm_deinit, 
			 PJ_TRUE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* GSM stream, SRTP 32bit, with auth */
static pjmedia_port* create_stream_gsm_srtp32_with_auth(pj_pool_t *pool,
						        unsigned clock_rate,
						        unsigned channel_count,
						        unsigned samples_per_frame,
						        unsigned flags,
						        struct test_entry *te)
{
    return create_stream(pool, "gsm", &pjmedia_codec_gsm_init, 
			 &pjmedia_codec_gsm_deinit, 
			 PJ_TRUE, PJ_FALSE, PJ_TRUE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* GSM stream, SRTP 80bit, no auth */
static pjmedia_port* create_stream_gsm_srtp80_no_auth(pj_pool_t *pool,
						      unsigned clock_rate,
						      unsigned channel_count,
						      unsigned samples_per_frame,
						      unsigned flags,
						      struct test_entry *te)
{
    return create_stream(pool, "gsm", &pjmedia_codec_gsm_init, 
			 &pjmedia_codec_gsm_deinit, 
			 PJ_TRUE, PJ_TRUE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* GSM stream, SRTP 80bit, with auth */
static pjmedia_port* create_stream_gsm_srtp80_with_auth(pj_pool_t *pool,
						        unsigned clock_rate,
						        unsigned channel_count,
						        unsigned samples_per_frame,
						        unsigned flags,
						        struct test_entry *te)
{
    return create_stream(pool, "gsm", &pjmedia_codec_gsm_init, 
			 &pjmedia_codec_gsm_deinit, 
			 PJ_TRUE, PJ_TRUE, PJ_TRUE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G722 stream */
static pjmedia_port* create_stream_g722( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned flags,
					 struct test_entry *te)
{
    return create_stream(pool, "g722", &pjmedia_codec_g722_init, 
			 &pjmedia_codec_g722_deinit, 
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G722.1 stream */
#if PJMEDIA_HAS_G7221_CODEC
static pjmedia_port* create_stream_g7221( pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned flags,
					  struct test_entry *te)
{
    return create_stream(pool, "g7221/16000", &pjmedia_codec_g7221_init, 
			 &pjmedia_codec_g7221_deinit, 
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}

/* G722.1 Annex C stream */
static pjmedia_port* create_stream_g7221c( pj_pool_t *pool,
					   unsigned clock_rate,
					   unsigned channel_count,
					   unsigned samples_per_frame,
					   unsigned flags,
					   struct test_entry *te)
{
    return create_stream(pool, "g7221/32000", &pjmedia_codec_g7221_init,
			 &pjmedia_codec_g7221_deinit, 
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}
#endif	/* PJMEDIA_HAS_G7221_CODEC */ 

/* AMR-NB stream */
#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
static pjmedia_port* create_stream_amr( pj_pool_t *pool,
                                        unsigned clock_rate,
                                        unsigned channel_count,
                                        unsigned samples_per_frame,
                                        unsigned flags,
                                        struct test_entry *te)
{
    return create_stream(pool, "AMR/8000", &pjmedia_codec_opencore_amrnb_init, 
			 &pjmedia_codec_opencore_amrnb_deinit, 
			 PJ_FALSE, PJ_FALSE, PJ_FALSE,
			 clock_rate, channel_count,
			 samples_per_frame, flags, te);
}
#endif	/* PJMEDIA_HAS_OPENCORE_AMRNB_CODEC */ 

/***************************************************************************/
/* Delay buffer */
enum {DELAY_BUF_MAX_DELAY = 80};
struct delaybuf_port
{
    pjmedia_port	 base;
    pjmedia_delay_buf   *delaybuf;
    pjmedia_port	*gen_port;
    int			 drift_pct;
};


static pj_status_t delaybuf_get_frame(struct pjmedia_port *this_port, 
				      pjmedia_frame *frame)
{
    struct delaybuf_port *dp = (struct delaybuf_port*)this_port;
    pj_status_t status;

    status = pjmedia_delay_buf_get(dp->delaybuf, (pj_int16_t*)frame->buf);
    pj_assert(status == PJ_SUCCESS);

    /* Additional GET when drift_pct is negative */
    if (dp->drift_pct < 0) {
	int rnd;
	rnd = pj_rand() % 100;

	if (rnd < -dp->drift_pct) {
	    status = pjmedia_delay_buf_get(dp->delaybuf, (pj_int16_t*)frame->buf);
	    pj_assert(status == PJ_SUCCESS);
	}
    }

    return PJ_SUCCESS;
}

static pj_status_t delaybuf_put_frame(struct pjmedia_port *this_port, 
				      const pjmedia_frame *frame)
{
    struct delaybuf_port *dp = (struct delaybuf_port*)this_port;
    pj_status_t status;
    pjmedia_frame f = *frame;

    status = pjmedia_port_get_frame(dp->gen_port, &f);
    pj_assert(status == PJ_SUCCESS);
    status = pjmedia_delay_buf_put(dp->delaybuf, (pj_int16_t*)f.buf);
    pj_assert(status == PJ_SUCCESS);

    /* Additional PUT when drift_pct is possitive */
    if (dp->drift_pct > 0) {
	int rnd;
	rnd = pj_rand() % 100;

	if (rnd < dp->drift_pct) {
	    status = pjmedia_port_get_frame(dp->gen_port, &f);
	    pj_assert(status == PJ_SUCCESS);
	    status = pjmedia_delay_buf_put(dp->delaybuf, (pj_int16_t*)f.buf);
	    pj_assert(status == PJ_SUCCESS);
	}
    }

    return PJ_SUCCESS;
}

static pj_status_t delaybuf_on_destroy(struct pjmedia_port *this_port)
{
    struct delaybuf_port *dp = (struct delaybuf_port*)this_port;
    pjmedia_port_destroy(dp->gen_port);
    pjmedia_delay_buf_destroy(dp->delaybuf);
    return PJ_SUCCESS;
}

static pjmedia_port* create_delaybuf(int drift_pct,
				     pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned flags,
				     struct test_entry *te)
{
    struct delaybuf_port *dp;
    pj_str_t name = pj_str("delaybuf");
    unsigned opt = 0;
    pj_status_t status;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(te);

    dp = PJ_POOL_ZALLOC_T(pool, struct delaybuf_port);
    dp->drift_pct = drift_pct;
    dp->base.get_frame = &delaybuf_get_frame;
    dp->base.put_frame = &delaybuf_put_frame;
    dp->base.on_destroy = &delaybuf_on_destroy;
    pjmedia_port_info_init(&dp->base.info, &name, 0x5678, clock_rate, 
			   channel_count, 16, samples_per_frame);

    status = pjmedia_delay_buf_create(pool, "mips_test", clock_rate, 
				      samples_per_frame, channel_count, 
				      DELAY_BUF_MAX_DELAY,
				      opt, &dp->delaybuf);
    if (status != PJ_SUCCESS)
	return NULL;

    dp->gen_port = create_gen_port(pool, clock_rate, channel_count, 
				   samples_per_frame, 100);
    if (dp->gen_port == NULL)
	return NULL;

    return &dp->base;
}


/* Delay buffer without drift */
static pjmedia_port* delaybuf_0( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(0, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}


/* Delay buffer with 2% drift */
static pjmedia_port* delaybuf_p2( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(2, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with 5% drift */
static pjmedia_port* delaybuf_p5( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(5, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with 10% drift */
static pjmedia_port* delaybuf_p10(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(10, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with 20% drift */
static pjmedia_port* delaybuf_p20(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(20, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with -2% drift */
static pjmedia_port* delaybuf_n2( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(-2, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with -5% drift */
static pjmedia_port* delaybuf_n5( pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(-5, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with -10% drift */
static pjmedia_port* delaybuf_n10(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(-10, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}

/* Delay buffer with -20% drift */
static pjmedia_port* delaybuf_n20(pj_pool_t *pool,
				  unsigned clock_rate,
				  unsigned channel_count,
				  unsigned samples_per_frame,
				  unsigned flags,
				  struct test_entry *te)
{
    return create_delaybuf(-20, pool, clock_rate, channel_count, 
			   samples_per_frame, flags, te);
}


/***************************************************************************/
/* Run test entry, return elapsed time */
static pj_timestamp run_entry(unsigned clock_rate, struct test_entry *e)
{
    pj_pool_t *pool;
    pjmedia_port *port;
    pj_timestamp t0, t1;
    unsigned j, samples_per_frame;
    pj_int16_t pcm[32000 * PTIME / 1000];
    pjmedia_port *gen_port;
    pj_status_t status;

    samples_per_frame = clock_rate * PTIME / 1000;

    pool = pj_pool_create(mem, "pool", 1024, 1024, NULL);
    port = e->init(pool, clock_rate, 1, samples_per_frame, 0, e);
    if (port == NULL) {
	t0.u64 = 0;
	pj_pool_release(pool);
	PJ_LOG(1,(THIS_FILE, " init error"));
	return t0;
    }

    /* Port may decide to use different ptime (e.g. iLBC) */
    samples_per_frame = port->info.samples_per_frame;

    gen_port = create_gen_port(pool, clock_rate, 1, 
			       samples_per_frame, 100);
    if (gen_port == NULL) {
	t0.u64 = 0;
	pj_pool_release(pool);
	return t0;
    }

    pj_get_timestamp(&t0);
    for (j=0; j<DURATION*clock_rate/samples_per_frame/1000; ++j) {
	pjmedia_frame frm;

	if (e->valid_op==OP_GET_PUT) {
	    frm.buf = (void*)pcm;
	    frm.size = samples_per_frame * 2;
	    frm.type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pjmedia_port_get_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	    status = pjmedia_port_put_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	} else if (e->valid_op == OP_GET) {
	    frm.buf = (void*)pcm;
	    frm.size = samples_per_frame * 2;
	    frm.type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pjmedia_port_get_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	} else if (e->valid_op == OP_PUT) {
	    frm.buf = (void*)pcm;
	    frm.size = samples_per_frame * 2;
	    frm.type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pjmedia_port_get_frame(gen_port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	    status = pjmedia_port_put_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	} else if (e->valid_op == OP_PUT_GET) {
	    frm.buf = (void*)pcm;
	    frm.size = samples_per_frame * 2;
	    frm.type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pjmedia_port_get_frame(gen_port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	    status = pjmedia_port_put_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);

	    status = pjmedia_port_get_frame(port, &frm);
	    pj_assert(status == PJ_SUCCESS);
	}
    }
    pj_get_timestamp(&t1);

    pj_sub_timestamp(&t1, &t0);

    if (e->custom_deinit)
	e->custom_deinit(e);

    pjmedia_port_destroy(port);
    pj_pool_release(pool);

    return t1;
}

/***************************************************************************/
int mips_test(void)
{
    struct test_entry entries[] = {
	{ "get from memplayer", OP_GET, K8|K16, &gen_port_test_init},
	{ "conference bridge with 1 call", OP_GET_PUT, K8|K16, &conf1_test_init},
	{ "conference bridge with 2 calls", OP_GET_PUT, K8|K16, &conf2_test_init},
	{ "conference bridge with 4 calls", OP_GET_PUT, K8|K16, &conf4_test_init},
	{ "conference bridge with 8 calls", OP_GET_PUT, K8|K16, &conf8_test_init},
	{ "conference bridge with 16 calls", OP_GET_PUT, K8|K16, &conf16_test_init},
	{ "upsample+downsample - linear", OP_GET, K8|K16, &linear_resample},
	{ "upsample+downsample - small filter", OP_GET, K8|K16, &small_filt_resample},
	{ "upsample+downsample - large filter", OP_GET, K8|K16, &large_filt_resample},
	{ "WSOLA PLC - 0% loss", OP_GET, K8|K16, &wsola_plc_0},
	{ "WSOLA PLC - 2% loss", OP_GET, K8|K16, &wsola_plc_2},
	{ "WSOLA PLC - 5% loss", OP_GET, K8|K16, &wsola_plc_5},
	{ "WSOLA PLC - 10% loss", OP_GET, K8|K16, &wsola_plc_10},
	{ "WSOLA PLC - 20% loss", OP_GET, K8|K16, &wsola_plc_20},
	{ "WSOLA PLC - 50% loss", OP_GET, K8|K16, &wsola_plc_50},
	{ "WSOLA discard 2% excess", OP_GET, K8|K16, &wsola_discard_2},
	{ "WSOLA discard 5% excess", OP_GET, K8|K16, &wsola_discard_5},
	{ "WSOLA discard 10% excess", OP_GET, K8|K16, &wsola_discard_10},
	{ "WSOLA discard 20% excess", OP_GET, K8|K16, &wsola_discard_20},
	{ "WSOLA discard 50% excess", OP_GET, K8|K16, &wsola_discard_50},
	{ "Delay buffer", OP_GET_PUT, K8|K16, &delaybuf_0},
	{ "Delay buffer - drift -2%", OP_GET_PUT, K8|K16, &delaybuf_n2},
	{ "Delay buffer - drift -5%", OP_GET_PUT, K8|K16, &delaybuf_n5},
	{ "Delay buffer - drift -10%", OP_GET_PUT, K8|K16, &delaybuf_n10},
	{ "Delay buffer - drift -20%", OP_GET_PUT, K8|K16, &delaybuf_n20},
	{ "Delay buffer - drift +2%", OP_GET_PUT, K8|K16, &delaybuf_p2},
	{ "Delay buffer - drift +5%", OP_GET_PUT, K8|K16, &delaybuf_p5},
	{ "Delay buffer - drift +10%", OP_GET_PUT, K8|K16, &delaybuf_p10},
	{ "Delay buffer - drift +20%", OP_GET_PUT, K8|K16, &delaybuf_p20},
	{ "echo canceller 100ms tail len", OP_GET_PUT, K8|K16, &ec_create_100},
	{ "echo canceller 128ms tail len", OP_GET_PUT, K8|K16, &ec_create_128},
	{ "echo canceller 200ms tail len", OP_GET_PUT, K8|K16, &ec_create_200},
	{ "echo canceller 256ms tail len", OP_GET_PUT, K8|K16, &ec_create_256},
	{ "echo canceller 400ms tail len", OP_GET_PUT, K8|K16, &ec_create_400},
	{ "echo canceller 500ms tail len", OP_GET_PUT, K8|K16, &ec_create_500},
	{ "echo canceller 512ms tail len", OP_GET_PUT, K8|K16, &ec_create_512},
	{ "echo canceller 600ms tail len", OP_GET_PUT, K8|K16, &ec_create_600},
	{ "echo canceller 800ms tail len", OP_GET_PUT, K8|K16, &ec_create_800},
	{ "echo suppressor 100ms tail len", OP_GET_PUT, K8|K16, &es_create_100},
	{ "echo suppressor 128ms tail len", OP_GET_PUT, K8|K16, &es_create_128},
	{ "echo suppressor 200ms tail len", OP_GET_PUT, K8|K16, &es_create_200},
	{ "echo suppressor 256ms tail len", OP_GET_PUT, K8|K16, &es_create_256},
	{ "echo suppressor 400ms tail len", OP_GET_PUT, K8|K16, &es_create_400},
	{ "echo suppressor 500ms tail len", OP_GET_PUT, K8|K16, &es_create_500},
	{ "echo suppressor 512ms tail len", OP_GET_PUT, K8|K16, &es_create_512},
	{ "echo suppressor 600ms tail len", OP_GET_PUT, K8|K16, &es_create_600},
	{ "echo suppressor 800ms tail len", OP_GET_PUT, K8|K16, &es_create_800},
	{ "tone generator with single freq", OP_GET, K8|K16, &create_tonegen1},
	{ "tone generator with dual freq", OP_GET, K8|K16, &create_tonegen2},
#if PJMEDIA_HAS_G711_CODEC
	{ "codec encode/decode - G.711", OP_PUT, K8, &g711_encode_decode},
#endif
#if PJMEDIA_HAS_G722_CODEC
	{ "codec encode/decode - G.722", OP_PUT, K16, &g722_encode_decode},
#endif
#if PJMEDIA_HAS_GSM_CODEC
	{ "codec encode/decode - GSM", OP_PUT, K8, &gsm_encode_decode},
#endif
#if PJMEDIA_HAS_ILBC_CODEC
	{ "codec encode/decode - iLBC", OP_PUT, K8, &ilbc_encode_decode},
#endif
#if PJMEDIA_HAS_SPEEX_CODEC
	{ "codec encode/decode - Speex 8Khz", OP_PUT, K8, &speex8_encode_decode},
	{ "codec encode/decode - Speex 16Khz", OP_PUT, K16, &speex16_encode_decode},
#endif
#if PJMEDIA_HAS_G7221_CODEC
	{ "codec encode/decode - G.722.1", OP_PUT, K16, &g7221_encode_decode},
	{ "codec encode/decode - G.722.1c", OP_PUT, K32, &g7221c_encode_decode},
#endif
#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
	{ "codec encode/decode - AMR-NB", OP_PUT, K8, &amr_encode_decode},
#endif
#if PJMEDIA_HAS_L16_CODEC
	{ "codec encode/decode - L16/8000/1", OP_PUT, K8, &l16_8_encode_decode},
	{ "codec encode/decode - L16/16000/1", OP_PUT, K16, &l16_16_encode_decode},
#endif
#if PJMEDIA_HAS_G711_CODEC
	{ "stream TX/RX - G.711", OP_PUT_GET, K8, &create_stream_pcmu},
	{ "stream TX/RX - G.711 SRTP 32bit", OP_PUT_GET, K8, &create_stream_pcmu_srtp32_no_auth},
	{ "stream TX/RX - G.711 SRTP 32bit +auth", OP_PUT_GET, K8, &create_stream_pcmu_srtp32_with_auth},
	{ "stream TX/RX - G.711 SRTP 80bit", OP_PUT_GET, K8, &create_stream_pcmu_srtp80_no_auth},
	{ "stream TX/RX - G.711 SRTP 80bit +auth", OP_PUT_GET, K8, &create_stream_pcmu_srtp80_with_auth},
#endif
#if PJMEDIA_HAS_G722_CODEC
	{ "stream TX/RX - G.722", OP_PUT_GET, K16, &create_stream_g722},
#endif
#if PJMEDIA_HAS_GSM_CODEC
	{ "stream TX/RX - GSM", OP_PUT_GET, K8, &create_stream_gsm},
	{ "stream TX/RX - GSM SRTP 32bit", OP_PUT_GET, K8, &create_stream_gsm_srtp32_no_auth},
	{ "stream TX/RX - GSM SRTP 32bit + auth", OP_PUT_GET, K8, &create_stream_gsm_srtp32_with_auth},
	{ "stream TX/RX - GSM SRTP 80bit", OP_PUT_GET, K8, &create_stream_gsm_srtp80_no_auth},
	{ "stream TX/RX - GSM SRTP 80bit + auth", OP_PUT_GET, K8, &create_stream_gsm_srtp80_with_auth},
#endif
#if PJMEDIA_HAS_G7221_CODEC
	{ "stream TX/RX - G.722.1", OP_PUT_GET, K16, &create_stream_g7221},
	{ "stream TX/RX - G.722.1c", OP_PUT_GET, K32, &create_stream_g7221c},
#endif
#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
	{ "stream TX/RX - AMR-NB", OP_PUT_GET, K8, &create_stream_amr},
#endif
    };

    unsigned i, c, k[3] = {K8, K16, K32}, clock_rates[3] = {8000, 16000, 32000};

    PJ_LOG(3,(THIS_FILE, "MIPS test, with CPU=%dMhz, %6.1f MIPS", CPU_MHZ, CPU_IPS / 1000000));
    PJ_LOG(3,(THIS_FILE, "Clock  Item                                      Time     CPU    MIPS"));
    PJ_LOG(3,(THIS_FILE, " Rate                                           (usec)    (%%)       "));
    PJ_LOG(3,(THIS_FILE, "----------------------------------------------------------------------"));

    for (c=0; c<PJ_ARRAY_SIZE(clock_rates); ++c) {
	for (i=0; i<PJ_ARRAY_SIZE(entries); ++i) {
	    enum 
	    {
		RETRY	= 5,	/* number of test retries */
	    };
	    struct test_entry *e = &entries[i];
	    pj_timestamp times[RETRY], tzero;
	    int usec;
	    float cpu_pct, mips_val;
	    unsigned j, clock_rate = clock_rates[c];

	    if ((e->valid_clock_rate & k[c]) == 0)
		continue;

	    /* Run test */
	    for (j=0; j<RETRY; ++j) {
		pj_thread_sleep(1);
		times[j] = run_entry(clock_rate, e);
	    }

	    /* Sort ascending */
	    for (j=0; j<RETRY; ++j) {
		unsigned k;
		for (k=j+1; k<RETRY; ++k) {
		    if (times[k].u64 < times[j].u64) {
			pj_timestamp tmp = times[j];
			times[j] = times[k];
			times[k] = tmp;
		    }
		}
	    }

	    /* Calculate usec elapsed as average of two best times */
	    tzero.u32.hi = tzero.u32.lo = 0;
	    usec = (pj_elapsed_usec(&tzero, &times[0]) + 
		    pj_elapsed_usec(&tzero, &times[1])) / 2;

	    usec = usec / (DURATION / 1000);

	    mips_val = (float)(CPU_IPS * usec / 1000000.0 / 1000000);
	    cpu_pct = (float)(100.0 * usec / 1000000);
	    PJ_LOG(3,(THIS_FILE, "%2dKHz %-38s % 8d %8.3f %7.2f", 
		      clock_rate/1000, e->title, usec, cpu_pct, mips_val));

	}
    }

    return 0;
}



