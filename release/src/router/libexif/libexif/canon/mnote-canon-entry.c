/* mnote-canon-entry.c
 *
 * Copyright (c) 2002 Lutz Mueller <lutz@users.sourceforge.net>
 * Copyright (c) 2003 Matthieu Castet <mat-c@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

#include "config.h"
#include "mnote-canon-entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libexif/exif-format.h>
#include <libexif/exif-utils.h>
#include <libexif/i18n.h>

/* #define DEBUG */

#define CF(format,target,v,maxlen)                              \
{                                                               \
        if (format != target) {                                 \
                snprintf (v, maxlen,                            \
                        _("Invalid format '%s', "               \
                        "expected '%s'."),                      \
                        exif_format_get_name (format),          \
                        exif_format_get_name (target));         \
                break;                                          \
        }                                                       \
}

#define CC(number,target,v,maxlen)                                      \
{                                                                       \
        if (number != target) {                                         \
                snprintf (v, maxlen,                                    \
                        _("Invalid number of components (%i, "          \
                        "expected %i)."), (int) number, (int) target);  \
                break;                                                  \
        }                                                               \
}
#define CC2(number,t1,t2,v,maxlen)                                      \
{                                                                       \
	if ((number != t1) && (number != t2)) {                         \
		snprintf (v, maxlen,                                    \
			_("Invalid number of components (%i, "          \
			"expected %i or %i)."), (int) number,		\
			(int) t1, (int) t2);  				\
		break;                                                  \
	}                                                               \
}

#define UNDEFINED 0xFF
    
static const struct canon_entry_table_t {
  unsigned int subtag;
  ExifShort value;
  const char *name;
} entries_settings_1 [] = {
#ifndef NO_VERBOSE_TAG_DATA
  { 0,  1, N_("Macro")},
  { 0,  2, N_("Normal")},
  { 2,  1, N_("Economy")},
  { 2,  2, N_("Normal")},  
  { 2,  3, N_("Fine")},
  { 2,  4, N_("RAW")},
  { 2,  5, N_("Superfine")},
  { 3,  0, N_("Off")},
  { 3,  1, N_("Auto")},
  { 3,  2, N_("On")},
  { 3,  3, N_("Red-eye reduction")},
  { 3,  4, N_("Slow synchro")},
  { 3,  5, N_("Auto + Red-eye reduction")},
  { 3,  6, N_("On + Red-eye reduction")},
  { 3, 16, N_("External flash")},
  { 4,  0, N_("Single")},
  { 4,  1, N_("Continuous")},
  { 4,  2, N_("Movie")},
  { 4,  3, N_("Continuous, speed priority")},
  { 4,  4, N_("Continuous, low")},
  { 4,  5, N_("Continuous, high")},
  { 6,  0, N_("One-shot AF")},
  { 6,  1, N_("AI servo AF")},
  { 6,  2, N_("AI focus AF")},
  { 6,  3, N_("Manual focus")},
  { 6,  4, N_("Single")},
  { 6,  5, N_("Continuous")},
  { 6,  6, N_("Manual focus")},
  { 6,  16, N_("Pan focus")},
  { 8,  1, N_("JPEG")},
  { 8,  2, N_("CRW+THM")},
  { 8,  3, N_("AVI+THM")},
  { 8,  4, N_("TIF")},
  { 8,  5, N_("TIF+JPEG")},
  { 8,  6, N_("CR2")},
  { 8,  7, N_("CR2+JPEG")},
  { 9,  0, N_("Large")},
  { 9,  1, N_("Medium")},
  { 9,  2, N_("Small")},
  { 9,  5, N_("Medium 1")},
  { 9,  6, N_("Medium 2")},
  { 9,  7, N_("Medium 3")},
  { 9,  8, N_("Postcard")},
  { 9,  9, N_("Widescreen")},
  {10,  0, N_("Full auto")},
  {10,  1, N_("Manual")},
  {10,  2, N_("Landscape")},
  {10,  3, N_("Fast shutter")},
  {10,  4, N_("Slow shutter")},
  {10,  5, N_("Night")},
  {10,  6, N_("Grayscale")},
  {10,  7, N_("Sepia")},
  {10,  8, N_("Portrait")},
  {10,  9, N_("Sports")},
  {10, 10, N_("Macro")},
  {10, 11, N_("Black & white")},
  {10, 12, N_("Pan focus")},
  {10, 13, N_("Vivid")},
  {10, 14, N_("Neutral")},
  {10, 15, N_("Flash off")},
  {10, 16, N_("Long shutter")},
  {10, 17, N_("Super macro")},
  {10, 18, N_("Foliage")},
  {10, 19, N_("Indoor")},
  {10, 20, N_("Fireworks")},
  {10, 21, N_("Beach")},
  {10, 22, N_("Underwater")},
  {10, 23, N_("Snow")},
  {10, 24, N_("Kids & pets")},
  {10, 25, N_("Night snapshot")},
  {10, 26, N_("Digital macro")},
  {10, 27, N_("My colors")},
  {10, 28, N_("Still image")},
  {10, 30, N_("Color accent")},
  {10, 31, N_("Color swap")},
  {10, 32, N_("Aquarium")},
  {10, 33, N_("ISO 3200")},
  {11, 0, N_("None")},
  {11, 1, N_("2x")},
  {11, 2, N_("4x")},
  {11, 3, N_("Other")},
  {12, 0x0000, N_("Normal")},
  {12, 0x0001, N_("High")},
  {12, 0xffff, N_("Low")},
  {13, 0x0000, N_("Normal")},
  {13, 0x0001, N_("High")},
  {13, 0xffff, N_("Low")},
  {14, 0x0000, N_("Normal")},
  {14, 0x0001, N_("High")},
  {14, 0xffff, N_("Low")},
  {15, 14, N_("Auto high")},
  {15, 15, N_("Auto")},
  {15, 16, N_("50")},
  {15, 17, N_("100")},
  {15, 18, N_("200")},
  {15, 19, N_("400")},
  {15, 20, N_("800")},
  {16,  0, N_("Default")},
  {16,  1, N_("Spot")},
  {16,  2, N_("Average")},	
  {16,  3, N_("Evaluative")},
  {16,  4, N_("Partial")},
  {16,  5, N_("Center-weighted average")},
  {17,  0, N_("Manual")},
  {17,  1, N_("Auto")},
  {17,  2, N_("Not known")},
  {17,  3, N_("Macro")},
  {17,  4, N_("Very close")},
  {17,  5, N_("Close")},
  {17,  6, N_("Middle range")},
  {17,  7, N_("Far range")},
  {17,  8, N_("Pan focus")},
  {17,  9, N_("Super macro")},
  {17,  10, N_("Infinity")},
  {18, 0x2005, N_("Manual AF point selection")},
  {18, 0x3000, N_("None (MF)")},
  {18, 0x3001, N_("Auto-selected")},
  {18, 0x3002, N_("Right")},
  {18, 0x3003, N_("Center")},
  {18, 0x3004, N_("Left")},
  {18, 0x4001, N_("Auto AF point selection")},
  {19,  0, N_("Easy shooting")},
  {19,  1, N_("Program")},
  {19,  2, N_("Tv-priority")},
  {19,  3, N_("Av-priority")},
  {19,  4, N_("Manual")},
  {19,  5, N_("A-DEP")},
  {19,  6, N_("M-DEP")},
  {21,   1, N_("Canon EF 50mm f/1.8")},
  {21,   2, N_("Canon EF 28mm f/2.8")},
  {21,   4, N_("Sigma UC Zoom 35-135mm f/4-5.6")},
  {21,   6, N_("Tokina AF193-2 19-35mm f/3.5-4.5")},
  {21,   7, N_("Canon EF 100-300mm F5.6L")},
  {21,  10, N_("Sigma 50mm f/2.8 EX or 28mm f/1.8")},
  {21,  11, N_("Canon EF 35mm f/2")},
  {21,  13, N_("Canon EF 15mm f/2.8")},
  {21,  21, N_("Canon EF 80-200mm f/2.8L")},
  {21,  22, N_("Tokina AT-X280AF PRO 28-80mm F2.8 ASPHERICAL")},
  {21,  26, N_("Cosina 100mm f/3.5 Macro AF")},
  {21,  28, N_("Tamron AF Aspherical 28-200mm f/3.8-5.6")},
  {21,  29, N_("Canon EF 50mm f/1.8 MkII")},
  {21,  31, N_("Tamron SP AF 300mm f/2.8 LD IF")},
  {21,  32, N_("Canon EF 24mm f/2.8 or Sigma 15mm f/2.8 EX Fisheye")},
  {21,  39, N_("Canon EF 75-300mm f/4-5.6")},
  {21,  40, N_("Canon EF 28-80mm f/3.5-5.6")},
  {21,  43, N_("Canon EF 28-105mm f/4-5.6")},
  {21,  45, N_("Canon EF-S 18-55mm f/3.5-5.6")},
  {21, 124, N_("Canon MP-E 65mm f/2.8 1-5x Macro Photo")},
  {21, 125, N_("Canon TS-E 24mm f/3.5L")},
  {21, 126, N_("Canon TS-E 45mm f/2.8")},
  {21, 127, N_("Canon TS-E 90mm f/2.8")},
  {21, 130, N_("Canon EF 50mm f/1.0L")},
  {21, 131, N_("Sigma 17-35mm f2.8-4 EX Aspherical HSM")},
  {21, 134, N_("Canon EF 600mm f/4L IS")},
  {21, 135, N_("Canon EF 200mm f/1.8L")},
  {21, 136, N_("Canon EF 300mm f/2.8L")},
  {21, 137, N_("Canon EF 85mm f/1.2L")},
  {21, 139, N_("Canon EF 400mm f/2.8L")},
  {21, 141, N_("Canon EF 500mm f/4.5L")},
  {21, 142, N_("Canon EF 300mm f/2.8L IS")},
  {21, 143, N_("Canon EF 500mm f/4L IS")},
  {21, 149, N_("Canon EF 100mm f/2")},
  {21, 150, N_("Sigma 20mm EX f/1.8")},
  {21, 151, N_("Canon EF 200mm f/2.8L")},
  {21, 152, N_("Sigma 10-20mm F4-5.6 or 12-24mm f/4.5-5.6 or 14mm f/2.8")},
  {21, 153, N_("Canon EF 35-350mm f/3.5-5.6L")},
  {21, 155, N_("Canon EF 85mm f/1.8 USM")},
  {21, 156, N_("Canon EF 28-105mm f/3.5-4.5 USM")},
  {21, 160, N_("Canon EF 20-35mm f/3.5-4.5 USM")},
  {21, 161, N_("Canon EF 28-70mm f/2.8L or Sigma 24-70mm EX f/2.8")},
  {21, 165, N_("Canon EF 70-200mm f/2.8 L")},
  {21, 166, N_("Canon EF 70-200mm f/2.8 L + x1.4")},
  {21, 167, N_("Canon EF 70-200mm f/2.8 L + x2")},
  {21, 168, N_("Canon EF 28mm f/1.8 USM")},
  {21, 169, N_("Sigma 15-30mm f/3.5-4.5 EX DG Aspherical")},
  {21, 170, N_("Canon EF 200mm f/2.8L II")},
  {21, 173, N_("Canon EF 180mm Macro f/3.5L or Sigma 180mm EX HSM Macro f/3.5")},
  {21, 174, N_("Canon EF 135mm f/2L")},
  {21, 176, N_("Canon EF 24-85mm f/3.5-4.5 USM")},
  {21, 177, N_("Canon EF 300mm f/4L IS")},
  {21, 178, N_("Canon EF 28-135mm f/3.5-5.6 IS")},
  {21, 180, N_("Canon EF 35mm f/1.4L")},
  {21, 181, N_("Canon EF 100-400mm f/4.5-5.6L IS + x1.4")},
  {21, 182, N_("Canon EF 100-400mm f/4.5-5.6L IS + x2")},
  {21, 183, N_("Canon EF 100-400mm f/4.5-5.6L IS")},
  {21, 184, N_("Canon EF 400mm f/2.8L + x2")},
  {21, 186, N_("Canon EF 70-200mm f/4L")},
  {21, 190, N_("Canon EF 100mm f/2.8 Macro")},
  {21, 191, N_("Canon EF 400mm f/4 DO IS")},
  {21, 197, N_("Canon EF 75-300mm f/4-5.6 IS")},
  {21, 198, N_("Canon EF 50mm f/1.4")},
  {21, 202, N_("Canon EF 28-80 f/3.5-5.6 USM IV")},
  {21, 211, N_("Canon EF 28-200mm f/3.5-5.6")},
  {21, 213, N_("Canon EF 90-300mm f/4.5-5.6")},
  {21, 214, N_("Canon EF-S 18-55mm f/3.5-4.5 USM")},
  {21, 224, N_("Canon EF 70-200mm f/2.8L IS USM")},
  {21, 225, N_("Canon EF 70-200mm f/2.8L IS USM + x1.4")},
  {21, 226, N_("Canon EF 70-200mm f/2.8L IS USM + x2")},
  {21, 229, N_("Canon EF 16-35mm f/2.8L")},
  {21, 230, N_("Canon EF 24-70mm f/2.8L")},
  {21, 231, N_("Canon EF 17-40mm f/4L")},
  {21, 232, N_("Canon EF 70-300mm f/4.5-5.6 DO IS USM")},
  {21, 234, N_("Canon EF-S 17-85mm f4-5.6 IS USM")},
  {21, 235, N_("Canon EF-S10-22mm F3.5-4.5 USM")},
  {21, 236, N_("Canon EF-S60mm F2.8 Macro USM")},
  {21, 237, N_("Canon EF 24-105mm f/4L IS")},
  {21, 238, N_("Canon EF 70-300mm F4-5.6 IS USM")},
  {21, 241, N_("Canon EF 50mm F1.2L USM")},
  {21, 242, N_("Canon EF 70-200mm f/4L IS USM")},
  {28, 0, N_("Manual")},
  {28, 1, N_("TTL")},
  {28, 2, N_("A-TTL")},
  {28, 3, N_("E-TTL")},
  {28, 4, N_("FP sync enabled")},
  {28, 7, N_("2nd-curtain sync used")},
  {28, 11, N_("FP sync used")},
  {28, 13, N_("Internal")},
  {28, 14, N_("External")},
  {31,  0, N_("Single")},
  {31,  1, N_("Continuous")},
  {32, 0, N_("Normal AE")},
  {32, 1, N_("Exposure compensation")},
  {32, 2, N_("AE lock")},
  {32, 3, N_("AE lock + Exposure compensation")},
  {32, 4, N_("No AE")},
  {33, 0, N_("Off")},
  {33, 1, N_("On")},
  {33, 2, N_("On, shot only")},
  {39, 0, N_("Off")},
  {39, 1, N_("Vivid")},
  {39, 2, N_("Neutral")},
  {39, 3, N_("Smooth")},
  {39, 4, N_("Sepia")},
  {39, 5, N_("Black & white")},
  {39, 6, N_("Custom")},
  {39, 100, N_("My color data")},
  {40, 0, N_("Off")},
  {40, 0x0500, N_("Full")},
  {40, 0x0502, N_("2/3")},
  {40, 0x0504, N_("1/3")},
#endif
  { 0,  0, NULL}
},
entries_focal_length [] = {
#ifndef NO_VERBOSE_TAG_DATA
	{0, 1, N_("Fixed")},
	{0, 2, N_("Zoom")},
#endif
	{0, 0, NULL}
},
entries_settings_2 [] = {
#ifndef NO_VERBOSE_TAG_DATA
  { 6,  0, N_("Auto")},
  { 6,  1, N_("Sunny")},
  { 6,  2, N_("Cloudy")},
  { 6,  3, N_("Tungsten")},
  { 6,  4, N_("Fluorescent")},
  { 6,  5, N_("Flash")},
  { 6,  6, N_("Custom")},
  { 6,  7, N_("Black & white")},
  { 6,  8, N_("Shade")},
  { 6,  9, N_("Manual temperature (Kelvin)")},
  { 6,  10, N_("PC set 1")},
  { 6,  11, N_("PC set 2")},
  { 6,  12, N_("PC set 3")},
  { 6,  14, N_("Daylight fluorescent")},
  { 6,  15, N_("Custom 1")},
  { 6,  16, N_("Custom 2")},
  { 6,  17, N_("Underwater")},
  { 7,  0, N_("Off")},
  { 7,  1, N_("Night scene")},
  { 7,  2, N_("On")},
  { 7,  3, N_("None")},
  { 13,  0x3000, N_("None (MF)")},
  { 13,  0x3001, N_("Right")},
  { 13,  0x3002, N_("Center")},
  { 13,  0x3003, N_("Center + Right")},
  { 13,  0x3004, N_("Left")},
  { 13,  0x3005, N_("Left + Right")},
  { 13,  0x3006, N_("Left + Center")},
  { 13,  0x3007, N_("All")},
  { 15,  0, N_("Off")},
  { 15,  1, N_("On (shot 1)")},
  { 15,  2, N_("On (shot 2)")},
  { 15,  3, N_("On (shot 3)")},
  { 15,  0xffff, N_("On")},
  { 25,  248, N_("EOS high-end")},
  { 25,  250, N_("Compact")},
  { 25,  252, N_("EOS mid-range")},
  { 26,  0, N_("None")},
  { 26,  1, N_("Rotate 90 CW")},
  { 26,  2, N_("Rotate 180")},
  { 26,  3, N_("Rotate 270 CW")},
  { 26,  0xffff, N_("Rotated by software")},
  { 27,  0, N_("Off")},
  { 27,  1, N_("On")}, 
  { 32,  0, N_("Off")},
  { 32,  0x0014, N_("1/3")},
  { 32,  0x008c, N_("2/3")},
  { 32,  0x07d0, N_("Full")},
#endif
  {0, 0, NULL}
},
entries_panorama [] = {
#ifndef NO_VERBOSE_TAG_DATA
	{0, 0, N_("Left to right")},
	{0, 1, N_("Right to left")},
	{0, 2, N_("Bottom to top")},
	{0, 3, N_("Top to bottom")},
	{0, 4, N_("2x2 matrix (clockwise)")},
#endif
	{0, 0, NULL}
},
color_information [] = {
#ifndef NO_VERBOSE_TAG_DATA
  {0, 0, N_("Standard")},
  {0, 1, N_("Manual")},
  {0, 2, N_("Custom")},
  {2, 0, N_("N/A")},
  {2, 1, N_("Lowest")},
  {2, 2, N_("Low")},
  {2, 3, N_("Standard")},
  {2, 4, N_("High")},
  {2, 5, N_("Highest")},
  {7,  0, N_("Auto")},
  {7,  1, N_("Daylight")},
  {7,  2, N_("Cloudy")},
  {7,  3, N_("Tungsten")},
  {7,  4, N_("Fluorescent")},
  {7,  5, N_("Flash")},
  {7,  6, N_("Custom")},
  {7,  7, N_("Black & White")},
  {7,  8, N_("Shade")},
  {7,  9, N_("Manual Temperature (Kelvin)")},
  {7, 10, N_("PC Set1")},
  {7, 11, N_("PC Set2")},
  {7, 12, N_("PC Set3")},
  {7, 14, N_("Daylight Fluorescent")},
  {7, 15, N_("Custom 1")},
  {7, 16, N_("Custom 2")},
  {7, 17, N_("Underwater")},
  {9, 0x00, N_("None")},
  {9, 0x01, N_("Standard")},
  {9, 0x02, N_("Set 1")},
  {9, 0x03, N_("Set 2")},
  {9, 0x04, N_("Set 3")},
  {9, 0x21, N_("User Def. 1")},
  {9, 0x22, N_("User Def. 2")},
  {9, 0x23, N_("User Def. 3")},
  {9, 0x41, N_("External 1")},
  {9, 0x42, N_("External 2")},
  {9, 0x43, N_("External 3")},
  {9, 0x81, N_("Standard")},
  {9, 0x82, N_("Portrait")},
  {9, 0x83, N_("Landscape")},
  {9, 0x84, N_("Neutral")},
  {9, 0x85, N_("Faithful")},
  {9, 0x86, N_("Monochrome")},
#endif
  {0, 0, NULL}
};

static void
canon_search_table_value (const struct canon_entry_table_t table[],
    unsigned int t, ExifShort vs, char *val, unsigned int maxlen)
{
	unsigned int j;

	/* Search the table for the first matching subtag and value. */
	for (j = 0; table[j].name && ((table[j].subtag < t) ||
			((table[j].subtag == t) && table[j].value <= vs)); j++) {
		if ((table[j].subtag == t) && (table[j].value == vs)) {
			break;
		}
	}
	if ((table[j].subtag == t) && (table[j].value == vs) && table[j].name) {
		/* Matching subtag and value found. */
		strncpy (val, _(table[j].name), maxlen);
	} else {
		/* No matching subtag and/or value found. */
		snprintf (val, maxlen, "0x%04x", vs);
	}
}

static void
canon_search_table_bitfield (const struct canon_entry_table_t table[],
    unsigned int t, ExifShort vs, char *val, unsigned int maxlen)
{
	unsigned int j;

	/* Search the table for the first matching subtag. */
	for (j = 0; table[j].name && (table[j].subtag <= t); j++) {
		if (table[j].subtag == t) {
			break;
		}
	}
	if ((table[j].subtag == t) && table[j].name) {
		unsigned int i, bit, lastbit = 0;

		/*
		 * Search the table for the last matching bit, because
		 * that one needs no additional comma appended.
		 */
		for (i = j; table[i].name && (table[i].subtag == t); i++) {
			bit = table[i].value;
			if ((vs >> bit) & 1) {
				lastbit = bit;
			}
		}
		/* Search the table for all matching bits. */
		for (i = j; table[i].name && (table[i].subtag == t); i++) {
			bit = table[i].value;
			if ((vs >> bit) & 1) {
				strncat(val, _(table[i].name), maxlen - strlen (val));
				if (bit != lastbit) 
					strncat (val, _(", "), maxlen - strlen (val));
			}
		}
	} else {
		/* No matching subtag found. */
		snprintf (val, maxlen, "0x%04x", vs);
	}
}

unsigned int
mnote_canon_entry_count_values (const MnoteCanonEntry *entry)
{
	unsigned int  val;

	if (!entry) return 0;

	switch (entry->tag) {
	case MNOTE_CANON_TAG_FOCAL_LENGTH:
	case MNOTE_CANON_TAG_PANORAMA:
		return entry->components;
	case MNOTE_CANON_TAG_SETTINGS_1:
	case MNOTE_CANON_TAG_SETTINGS_2:
	case MNOTE_CANON_TAG_CUSTOM_FUNCS:
	case MNOTE_CANON_TAG_COLOR_INFORMATION:
		if (entry->format != EXIF_FORMAT_SHORT) return 0;

		val = exif_get_short (entry->data, entry->order);
		/* val is buffer size, i.e. # of values plus 1 */
		return MIN (entry->size - 2, val) / 2;
	default:
		return 1;
	}
}

/*
 * For reference, see Exif 2.1 specification (Appendix C), 
 * or http://en.wikipedia.org/wiki/APEX_system
 */
static double
apex_value_to_aperture (double x)
{
	return pow (2, x / 2.);
}

static double
apex_value_to_shutter_speed(double x)
{
	return 1.0 / pow (2, x);
}

static double
apex_value_to_iso_speed (double x)
{
	return 3.125 * pow (2, x);
}

char *
mnote_canon_entry_get_value (const MnoteCanonEntry *entry, unsigned int t, char *val, unsigned int maxlen)
{
	char buf[128];
	ExifLong vl;
	ExifShort vs, n;
	unsigned char *data;
	double d;

	if (!entry) 
		return NULL;

	data = entry->data;

	memset (val, 0, maxlen);
	maxlen--;

	switch (entry->tag) {
	case MNOTE_CANON_TAG_SETTINGS_1:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		n = exif_get_short (data, entry->order) / 2;
		if (t >= n) return NULL;
		CC (entry->components, n, val, maxlen);
		vs = exif_get_short (data + 2 + t * 2, entry->order);
		switch (t) {
		case 1:
			if (!vs) {
				strncpy(val, _("Off"), maxlen);
				break;
			}
			snprintf (val, maxlen, _("%i (ms)"), vs * 100);
			break;
		case 15:
			if (((vs & 0xC000) == 0x4000) && (vs != 0x7FFF)) {
				/* Canon S3 IS - directly specified value */
				snprintf (val, maxlen, "%i", vs & ~0x4000);
			} else {
				/* Standard Canon - index into lookup table */
				canon_search_table_value (entries_settings_1, t, vs, val, maxlen);
			}
			break;
		case 22:
		case 23:
		case 24:
			snprintf (val, maxlen, "%u", vs);
			break;
		case 25:
		case 26:
			snprintf (val, maxlen, "%.2f", apex_value_to_aperture (vs / 32.0));
			break;
		case 28:
			canon_search_table_bitfield(entries_settings_1, t, vs, val, maxlen);
			break;
		case 34:
			snprintf (val, maxlen, "%.2f", vs / 10.0);
			break;
		case 35:
		case 36:
			snprintf (val, maxlen, "%u", vs);
			break;
		default:
			canon_search_table_value (entries_settings_1, t, vs, val, maxlen);
		}
		break;

	case MNOTE_CANON_TAG_FOCAL_LENGTH:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		vs = exif_get_short (data + t * 2, entry->order);
		switch (t) {
		case 1:
			snprintf (val, maxlen, "%u", vs);
			break;
		case 2:
		case 3:
			snprintf (val, maxlen, _("%.2f mm"), vs * 25.4 / 1000);
			break;
		default:
			canon_search_table_value (entries_focal_length, t, vs, val, maxlen);
		}
		break;

	case MNOTE_CANON_TAG_SETTINGS_2:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		n = exif_get_short (data, entry->order) / 2;
		if (t >= n) return NULL;
		CC (entry->components, n, val, maxlen);
		vs = exif_get_short (data + 2 + t * 2, entry->order);
		switch (t) {
		case 0:
			snprintf (val, maxlen, "%.3f", pow (2, (ExifSShort)vs / 32.0));
			break;
		case 1:
			snprintf (val, maxlen, "%.0f", apex_value_to_iso_speed ((ExifSShort)vs / 32.0));
			break;
		case 2:
		case 5:
		case 14:
		case 16:
			snprintf (val, maxlen, _("%.2f EV"), (ExifSShort)vs / 32.0);
			break;
		case 3:
		case 20:
			snprintf (val, maxlen, "%.2f", apex_value_to_aperture (vs / 32.0));
			break;
		case 4:
		case 21:
			d = apex_value_to_shutter_speed ((ExifSShort)vs / 32.0);
			if (d < 1)
				snprintf (val, maxlen, _("1/%i"),(int)(1.0 / d));
			else
				snprintf (val, maxlen, "%i", (int) d);
			break;
		case 8:
			snprintf (val, maxlen, "%u", vs);
			break;
		case 12:
			snprintf (val, maxlen, "%.2f", vs / 32.0);
			break;
		case 18:
		case 19:
			snprintf (val, maxlen, _("%u mm"), vs);
			break;
		case 28:
			if ((ExifSShort)vs <= 0) {
				strncpy(val, _("Off"), maxlen);
				break;
			}
			snprintf (val, maxlen, _("%i (ms)"), vs * 100);
			break;
		default:
			canon_search_table_value (entries_settings_2, t, vs, val, maxlen);
		}
		break;

	case MNOTE_CANON_TAG_PANORAMA:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		vs = exif_get_short (data + t * 2, entry->order);
		canon_search_table_value (entries_panorama, t, vs, val, maxlen);
		break;

	case MNOTE_CANON_TAG_OWNER:
		CC (entry->components, 32, val, maxlen);
		/* Fall through; ImageType can have many sizes */
	case MNOTE_CANON_TAG_IMAGE_TYPE:
		CF (entry->format, EXIF_FORMAT_ASCII, val, maxlen);
		strncpy (val, (char *)data, MIN (entry->size, maxlen));
		break;

	case MNOTE_CANON_TAG_FIRMWARE:
		CF (entry->format, EXIF_FORMAT_ASCII, val, maxlen);
/*		CC2 (entry->components, 24, 32, val, maxlen); Can also be 22 */
		strncpy (val, (char *)data, MIN (entry->size, maxlen));
		break;

	case MNOTE_CANON_TAG_IMAGE_NUMBER:
		CF (entry->format, EXIF_FORMAT_LONG, val, maxlen);
		CC (entry->components, 1, val, maxlen);
		vl = exif_get_long (data, entry->order);
		snprintf (val, maxlen, "%03lu-%04lu",
				(unsigned long) vl/10000,
				(unsigned long) vl%10000);
		break;

	case MNOTE_CANON_TAG_SERIAL_NUMBER:
		CF (entry->format, EXIF_FORMAT_LONG, val, maxlen);
		CC (entry->components, 1, val, maxlen);
		vl = exif_get_long (data, entry->order);
		snprintf (val, maxlen, "%04X-%05d", (int)vl>>16,(int)vl&0xffff);
		break;

	case MNOTE_CANON_TAG_CUSTOM_FUNCS:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		n = exif_get_short (data, entry->order) / 2;
		if (t >= n) return NULL;
		CC (entry->components, n, val, maxlen);
		vs = exif_get_short (data + 2 + t * 2, entry->order);
		snprintf (buf, sizeof (buf), "%u", vs);
		strncat (val, buf, maxlen - strlen (val));
		break;

	case MNOTE_CANON_TAG_COLOR_INFORMATION:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		n = exif_get_short (data, entry->order) / 2;
		if (t >= n) return NULL;
		CC (entry->components, n, val, maxlen);
		vs = exif_get_short (data + 2 + t * 2, entry->order);
		canon_search_table_value (color_information, t, vs, val, maxlen);
		break;

	default:
#ifdef DEBUG
	  {
		int i;
		if (entry->format == EXIF_FORMAT_SHORT)
		for(i=0;i<entry->components;i++) {
			vs = exif_get_short (data, entry->order);
			data+=2;
			printf ("Value%d=%d\n", i, vs);
		}
		else if (entry->format == EXIF_FORMAT_LONG)
		for(i=0;i<entry->components;i++) {
			vl = exif_get_long (data, entry->order);
			data+=4;
			printf ("Value%d=%d\n", i, vs);
		}
		else if (entry->format == EXIF_FORMAT_ASCII)
		    strncpy (val, data, MIN (entry->size, maxlen));
	  }
#endif
		break;
	}
	return val;
}
