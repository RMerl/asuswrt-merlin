/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define SNDRV_GF1_SCALE_TABLE_SIZE	128
#define SNDRV_GF1_ATTEN_TABLE_SIZE	128

#ifdef __GUS_TABLES_ALLOC__


unsigned short snd_gf1_atten_table[SNDRV_GF1_ATTEN_TABLE_SIZE] = {
  4095 /* 0   */,1789 /* 1   */,1533 /* 2   */,1383 /* 3   */,1277 /* 4   */,
  1195 /* 5   */,1127 /* 6   */,1070 /* 7   */,1021 /* 8   */,978  /* 9   */,
  939  /* 10  */,903  /* 11  */,871  /* 12  */,842  /* 13  */,814  /* 14  */,
  789  /* 15  */,765  /* 16  */,743  /* 17  */,722  /* 18  */,702  /* 19  */,
  683  /* 20  */,665  /* 21  */,647  /* 22  */,631  /* 23  */,615  /* 24  */,
  600  /* 25  */,586  /* 26  */,572  /* 27  */,558  /* 28  */,545  /* 29  */,
  533  /* 30  */,521  /* 31  */,509  /* 32  */,498  /* 33  */,487  /* 34  */,
  476  /* 35  */,466  /* 36  */,455  /* 37  */,446  /* 38  */,436  /* 39  */,
  427  /* 40  */,418  /* 41  */,409  /* 42  */,400  /* 43  */,391  /* 44  */,
  383  /* 45  */,375  /* 46  */,367  /* 47  */,359  /* 48  */,352  /* 49  */,
  344  /* 50  */,337  /* 51  */,330  /* 52  */,323  /* 53  */,316  /* 54  */,
  309  /* 55  */,302  /* 56  */,296  /* 57  */,289  /* 58  */,283  /* 59  */,
  277  /* 60  */,271  /* 61  */,265  /* 62  */,259  /* 63  */,253  /* 64  */,
  247  /* 65  */,242  /* 66  */,236  /* 67  */,231  /* 68  */,225  /* 69  */,
  220  /* 70  */,215  /* 71  */,210  /* 72  */,205  /* 73  */,199  /* 74  */,
  195  /* 75  */,190  /* 76  */,185  /* 77  */,180  /* 78  */,175  /* 79  */,
  171  /* 80  */,166  /* 81  */,162  /* 82  */,157  /* 83  */,153  /* 84  */,
  148  /* 85  */,144  /* 86  */,140  /* 87  */,135  /* 88  */,131  /* 89  */,
  127  /* 90  */,123  /* 91  */,119  /* 92  */,115  /* 93  */,111  /* 94  */,
  107  /* 95  */,103  /* 96  */,100  /* 97  */,96   /* 98  */,92   /* 99  */,
  88   /* 100 */,85   /* 101 */,81   /* 102 */,77   /* 103 */,74   /* 104 */,
  70   /* 105 */,67   /* 106 */,63   /* 107 */,60   /* 108 */,56   /* 109 */,
  53   /* 110 */,50   /* 111 */,46   /* 112 */,43   /* 113 */,40   /* 114 */,
  37   /* 115 */,33   /* 116 */,30   /* 117 */,27   /* 118 */,24   /* 119 */,
  21   /* 120 */,18   /* 121 */,15   /* 122 */,12   /* 123 */,9    /* 124 */,
  6    /* 125 */,3    /* 126 */,0    /* 127 */,
};

#else

extern unsigned int snd_gf1_scale_table[SNDRV_GF1_SCALE_TABLE_SIZE];
extern unsigned short snd_gf1_atten_table[SNDRV_GF1_ATTEN_TABLE_SIZE];

#endif
