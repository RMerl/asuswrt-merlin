/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************
*/
#include "../rt_config.h"

struct rt_ch_freq_map CH_HZ_ID_MAP[] = {
	{1, 2412}
	,
	{2, 2417}
	,
	{3, 2422}
	,
	{4, 2427}
	,
	{5, 2432}
	,
	{6, 2437}
	,
	{7, 2442}
	,
	{8, 2447}
	,
	{9, 2452}
	,
	{10, 2457}
	,
	{11, 2462}
	,
	{12, 2467}
	,
	{13, 2472}
	,
	{14, 2484}
	,

	/*  UNII */
	{36, 5180}
	,
	{40, 5200}
	,
	{44, 5220}
	,
	{48, 5240}
	,
	{52, 5260}
	,
	{56, 5280}
	,
	{60, 5300}
	,
	{64, 5320}
	,
	{149, 5745}
	,
	{153, 5765}
	,
	{157, 5785}
	,
	{161, 5805}
	,
	{165, 5825}
	,
	{167, 5835}
	,
	{169, 5845}
	,
	{171, 5855}
	,
	{173, 5865}
	,

	/* HiperLAN2 */
	{100, 5500}
	,
	{104, 5520}
	,
	{108, 5540}
	,
	{112, 5560}
	,
	{116, 5580}
	,
	{120, 5600}
	,
	{124, 5620}
	,
	{128, 5640}
	,
	{132, 5660}
	,
	{136, 5680}
	,
	{140, 5700}
	,

	/* Japan MMAC */
	{34, 5170}
	,
	{38, 5190}
	,
	{42, 5210}
	,
	{46, 5230}
	,

	/*  Japan */
	{184, 4920}
	,
	{188, 4940}
	,
	{192, 4960}
	,
	{196, 4980}
	,

	{208, 5040}
	,			/* Japan, means J08 */
	{212, 5060}
	,			/* Japan, means J12 */
	{216, 5080}
	,			/* Japan, means J16 */
};

int CH_HZ_ID_MAP_NUM = (sizeof(CH_HZ_ID_MAP) / sizeof(struct rt_ch_freq_map));

struct rt_ch_region ChRegion[] = {
	{			/* Antigua and Berbuda */
	 "AG",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Argentina */
	 "AR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Aruba */
	 "AW",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Australia */
	 "AU",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Austria */
	 "AT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Bahamas */
	 "BS",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Barbados */
	 "BB",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Bermuda */
	 "BM",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Brazil */
	 "BR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 24, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Belgium */
	 "BE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 18, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 18, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Bulgaria */
	 "BG",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, ODOR, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Canada */
	 "CA",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Cayman IsLands */
	 "KY",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Chile */
	 "CL",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* China */
	 "CN",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 27, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Colombia */
	 "CO",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 17, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Costa Rica */
	 "CR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 17, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Cyprus */
	 "CY",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Czech_Republic */
	 "CZ",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Denmark */
	 "DK",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Dominican Republic */
	 "DO",
	 CE,
	 {
	  {1, 0, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 0 */
	  {149, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Equador */
	 "EC",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {100, 11, 27, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* El Salvador */
	 "SV",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 30, BOTH, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 36, BOTH, TRUE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Finland */
	 "FI",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* France */
	 "FR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Germany */
	 "DE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Greece */
	 "GR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, ODOR, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Guam */
	 "GU",
	 CE,
	 {
	  {1, 11, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {36, 4, 17, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Guatemala */
	 "GT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 17, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Haiti */
	 "HT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 17, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Honduras */
	 "HN",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 27, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Hong Kong */
	 "HK",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Hungary */
	 "HU",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Iceland */
	 "IS",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* India */
	 "IN",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 24, IDOR, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Indonesia */
	 "ID",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 27, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Ireland */
	 "IE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, ODOR, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Israel */
	 "IL",
	 CE,
	 {
	  {1, 3, 20, IDOR, FALSE}
	  ,			/* 2.4 G, ch 1~3 */
	  {4, 6, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 4~9 */
	  {10, 4, 20, IDOR, FALSE}
	  ,			/* 2.4 G, ch 10~13 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Italy */
	 "IT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, ODOR, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Japan */
	 "JP",
	 JAP,
	 {
	  {1, 14, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~14 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Jordan */
	 "JO",
	 CE,
	 {
	  {1, 13, 20, IDOR, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {149, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Latvia */
	 "LV",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Liechtenstein */
	 "LI",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Lithuania */
	 "LT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Luxemburg */
	 "LU",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Malaysia */
	 "MY",
	 CE,
	 {
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Malta */
	 "MT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Marocco */
	 "MA",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 24, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Mexico */
	 "MX",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 5, 30, IDOR, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Netherlands */
	 "NL",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* New Zealand */
	 "NZ",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Norway */
	 "NO",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 24, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 24, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Peru */
	 "PE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 27, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Portugal */
	 "PT",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Poland */
	 "PL",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Romania */
	 "RO",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Russia */
	 "RU",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {149, 4, 20, IDOR, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Saudi Arabia */
	 "SA",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Serbia_and_Montenegro */
	 "CS",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Singapore */
	 "SG",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {149, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Slovakia */
	 "SK",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Slovenia */
	 "SI",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* South Africa */
	 "ZA",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {149, 4, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* South Korea */
	 "KR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 8, 20, BOTH, FALSE}
	  ,			/* 5G, ch 100~128 */
	  {149, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Spain */
	 "ES",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 17, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Sweden */
	 "SE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Switzerland */
	 "CH",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~13 */
	  {36, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Taiwan */
	 "TW",
	 CE,
	 {
	  {1, 11, 30, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {52, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Turkey */
	 "TR",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {36, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 36~48 */
	  {52, 4, 23, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* UK */
	 "GB",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {36, 4, 23, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {52, 4, 23, IDOR, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Ukraine */
	 "UA",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* United_Arab_Emirates */
	 "AE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* United_States */
	 "US",
	 CE,
	 {
	  {1, 11, 30, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {36, 4, 17, IDOR, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {52, 4, 24, BOTH, TRUE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 30, BOTH, TRUE}
	  ,			/* 5G, ch 100~140 */
	  {149, 5, 30, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Venezuela */
	 "VE",
	 CE,
	 {
	  {1, 13, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {149, 4, 27, BOTH, FALSE}
	  ,			/* 5G, ch 149~161 */
	  {0}
	  ,			/* end */
	  }
	 }
	,

	{			/* Default */
	 "",
	 CE,
	 {
	  {1, 11, 20, BOTH, FALSE}
	  ,			/* 2.4 G, ch 1~11 */
	  {36, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {52, 4, 20, BOTH, FALSE}
	  ,			/* 5G, ch 52~64 */
	  {100, 11, 20, BOTH, FALSE}
	  ,			/* 5G, ch 100~140 */
	  {149, 5, 20, BOTH, FALSE}
	  ,			/* 5G, ch 149~165 */
	  {0}
	  ,			/* end */
	  }
	 }
	,
};

static struct rt_ch_region *GetChRegion(u8 *CntryCode)
{
	int loop = 0;
	struct rt_ch_region *pChRegion = NULL;

	while (strcmp((char *)ChRegion[loop].CountReg, "") != 0) {
		if (strncmp
		    ((char *)ChRegion[loop].CountReg, (char *)CntryCode,
		     2) == 0) {
			pChRegion = &ChRegion[loop];
			break;
		}
		loop++;
	}

	if (pChRegion == NULL)
		pChRegion = &ChRegion[loop];
	return pChRegion;
}

static void ChBandCheck(u8 PhyMode, u8 *pChType)
{
	switch (PhyMode) {
	case PHY_11A:
	case PHY_11AN_MIXED:
		*pChType = BAND_5G;
		break;
	case PHY_11ABG_MIXED:
	case PHY_11AGN_MIXED:
	case PHY_11ABGN_MIXED:
		*pChType = BAND_BOTH;
		break;

	default:
		*pChType = BAND_24G;
		break;
	}
}

static u8 FillChList(struct rt_rtmp_adapter *pAd,
			struct rt_ch_desp *pChDesp,
			u8 Offset, u8 increment)
{
	int i, j, l;
	u8 channel;

	j = Offset;
	for (i = 0; i < pChDesp->NumOfCh; i++) {
		channel = pChDesp->FirstChannel + i * increment;
		for (l = 0; l < MAX_NUM_OF_CHANNELS; l++) {
			if (channel == pAd->TxPower[l].Channel) {
				pAd->ChannelList[j].Power =
				    pAd->TxPower[l].Power;
				pAd->ChannelList[j].Power2 =
				    pAd->TxPower[l].Power2;
				break;
			}
		}
		if (l == MAX_NUM_OF_CHANNELS)
			continue;

		pAd->ChannelList[j].Channel =
		    pChDesp->FirstChannel + i * increment;
		pAd->ChannelList[j].MaxTxPwr = pChDesp->MaxTxPwr;
		pAd->ChannelList[j].DfsReq = pChDesp->DfsReq;
		j++;
	}
	pAd->ChannelListNum = j;

	return j;
}

static inline void CreateChList(struct rt_rtmp_adapter *pAd,
				struct rt_ch_region *pChRegion, u8 Geography)
{
	int i;
	u8 offset = 0;
	struct rt_ch_desp *pChDesp;
	u8 ChType;
	u8 increment;

	if (pChRegion == NULL)
		return;

	ChBandCheck(pAd->CommonCfg.PhyMode, &ChType);

	for (i = 0; i < 10; i++) {
		pChDesp = &pChRegion->ChDesp[i];
		if (pChDesp->FirstChannel == 0)
			break;

		if (ChType == BAND_5G) {
			if (pChDesp->FirstChannel <= 14)
				continue;
		} else if (ChType == BAND_24G) {
			if (pChDesp->FirstChannel > 14)
				continue;
		}

		if ((pChDesp->Geography == BOTH)
		    || (pChDesp->Geography == Geography)) {
			if (pChDesp->FirstChannel > 14)
				increment = 4;
			else
				increment = 1;
			offset = FillChList(pAd, pChDesp, offset, increment);
		}
	}
}

void BuildChannelListEx(struct rt_rtmp_adapter *pAd)
{
	struct rt_ch_region *pChReg;

	pChReg = GetChRegion(pAd->CommonCfg.CountryCode);
	CreateChList(pAd, pChReg, pAd->CommonCfg.Geography);
}

void BuildBeaconChList(struct rt_rtmp_adapter *pAd,
		       u8 *pBuf, unsigned long *pBufLen)
{
	int i;
	unsigned long TmpLen;
	struct rt_ch_region *pChRegion;
	struct rt_ch_desp *pChDesp;
	u8 ChType;

	pChRegion = GetChRegion(pAd->CommonCfg.CountryCode);

	if (pChRegion == NULL)
		return;

	ChBandCheck(pAd->CommonCfg.PhyMode, &ChType);
	*pBufLen = 0;

	for (i = 0; i < 10; i++) {
		pChDesp = &pChRegion->ChDesp[i];
		if (pChDesp->FirstChannel == 0)
			break;

		if (ChType == BAND_5G) {
			if (pChDesp->FirstChannel <= 14)
				continue;
		} else if (ChType == BAND_24G) {
			if (pChDesp->FirstChannel > 14)
				continue;
		}

		if ((pChDesp->Geography == BOTH)
		    || (pChDesp->Geography == pAd->CommonCfg.Geography)) {
			MakeOutgoingFrame(pBuf + *pBufLen, &TmpLen,
					  1, &pChDesp->FirstChannel,
					  1, &pChDesp->NumOfCh,
					  1, &pChDesp->MaxTxPwr, END_OF_ARGS);
			*pBufLen += TmpLen;
		}
	}
}

static BOOLEAN IsValidChannel(struct rt_rtmp_adapter *pAd, u8 channel)
{
	int i;

	for (i = 0; i < pAd->ChannelListNum; i++) {
		if (pAd->ChannelList[i].Channel == channel)
			break;
	}

	if (i == pAd->ChannelListNum)
		return FALSE;
	else
		return TRUE;
}

static u8 GetExtCh(u8 Channel, u8 Direction)
{
	char ExtCh;

	if (Direction == EXTCHA_ABOVE)
		ExtCh = Channel + 4;
	else
		ExtCh = (Channel - 4) > 0 ? (Channel - 4) : 0;

	return ExtCh;
}

void N_ChannelCheck(struct rt_rtmp_adapter *pAd)
{
	/*u8 ChannelNum = pAd->ChannelListNum; */
	u8 Channel = pAd->CommonCfg.Channel;

	if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED)
	    && (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40)) {
		if (Channel > 14) {
			if ((Channel == 36) || (Channel == 44)
			    || (Channel == 52) || (Channel == 60)
			    || (Channel == 100) || (Channel == 108)
			    || (Channel == 116) || (Channel == 124)
			    || (Channel == 132) || (Channel == 149)
			    || (Channel == 157)) {
				pAd->CommonCfg.RegTransmitSetting.field.EXTCHA =
				    EXTCHA_ABOVE;
			} else if ((Channel == 40) || (Channel == 48)
				   || (Channel == 56) || (Channel == 64)
				   || (Channel == 104) || (Channel == 112)
				   || (Channel == 120) || (Channel == 128)
				   || (Channel == 136) || (Channel == 153)
				   || (Channel == 161)) {
				pAd->CommonCfg.RegTransmitSetting.field.EXTCHA =
				    EXTCHA_BELOW;
			} else {
				pAd->CommonCfg.RegTransmitSetting.field.BW =
				    BW_20;
			}
		} else {
			do {
				u8 ExtCh;
				u8 Dir =
				    pAd->CommonCfg.RegTransmitSetting.field.
				    EXTCHA;
				ExtCh = GetExtCh(Channel, Dir);
				if (IsValidChannel(pAd, ExtCh))
					break;

				Dir =
				    (Dir ==
				     EXTCHA_ABOVE) ? EXTCHA_BELOW :
				    EXTCHA_ABOVE;
				ExtCh = GetExtCh(Channel, Dir);
				if (IsValidChannel(pAd, ExtCh)) {
					pAd->CommonCfg.RegTransmitSetting.field.
					    EXTCHA = Dir;
					break;
				}
				pAd->CommonCfg.RegTransmitSetting.field.BW =
				    BW_20;
			} while (FALSE);

			if (Channel == 14) {
				pAd->CommonCfg.RegTransmitSetting.field.BW =
				    BW_20;
				/*pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_NONE; // We didn't set the ExtCh as NONE due to it'll set in RTMPSetHT() */
			}
		}
	}

}

void N_SetCenCh(struct rt_rtmp_adapter *pAd)
{
	if (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40) {
		if (pAd->CommonCfg.RegTransmitSetting.field.EXTCHA ==
		    EXTCHA_ABOVE) {
			pAd->CommonCfg.CentralChannel =
			    pAd->CommonCfg.Channel + 2;
		} else {
			if (pAd->CommonCfg.Channel == 14)
				pAd->CommonCfg.CentralChannel =
				    pAd->CommonCfg.Channel - 1;
			else
				pAd->CommonCfg.CentralChannel =
				    pAd->CommonCfg.Channel - 2;
		}
	} else {
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
	}
}

u8 GetCuntryMaxTxPwr(struct rt_rtmp_adapter *pAd, u8 channel)
{
	int i;
	for (i = 0; i < pAd->ChannelListNum; i++) {
		if (pAd->ChannelList[i].Channel == channel)
			break;
	}

	if (i == pAd->ChannelListNum)
		return 0xff;
	else
		return pAd->ChannelList[i].MaxTxPwr;
}
