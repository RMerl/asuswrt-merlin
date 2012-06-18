/* mnote-fuji-entry.c
 *
 * Copyright (c) 2002 Lutz Mueller <lutz@users.sourceforge.net>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <libexif/i18n.h>

#include "mnote-fuji-entry.h"

#define CF(format,target,v,maxlen)                              \
{                                                               \
        if (format != target) {                                 \
                snprintf (v, maxlen,	                        \
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

static const struct {
	ExifTag tag;
	struct {
		int index;
		const char *string;
	} elem[22];
} items[] = {
#ifndef NO_VERBOSE_TAG_DATA
  { MNOTE_FUJI_TAG_SHARPNESS,
    { {1, N_("Softest")},
      {2, N_("Soft")},
      {3, N_("Normal")},
      {4, N_("Hard")},
      {5, N_("Hardest")},
      {0x0082, N_("Medium soft")},
      {0x0084, N_("Medium hard")},
      {0x8000, N_("Film simulation mode")},
      {0xFFFF, N_("Off")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_WHITE_BALANCE,
    { {0, N_("Auto")},
      {0x100, N_("Daylight")},
      {0x200, N_("Cloudy")},
      {0x300, N_("Daylight-color fluorescent")},
      {0x301, N_("DayWhite-color fluorescent")},
      {0x302, N_("White fluorescent")},
      {0x400, N_("Incandescent")},
      {0x500, N_("Flash")},
      {0xF00, N_("Custom")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_COLOR,
    { {0, N_("Standard")},
      {0x0080, N_("Medium high")},
      {0x0100, N_("High")},
      {0x0180, N_("Medium low")},
      {0x0200, N_("Original")},
      {0x0300, N_("Black & white")},
      {0x8000, N_("Film simulation mode")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_TONE,
    { {0, N_("Standard")},
      {0x0080, N_("Medium hard")},
      {0x0100, N_("Hard")},
      {0x0180, N_("Medium soft")},
      {0x0200, N_("Original")},
      {0x8000, N_("Film simulation mode")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_FLASH_MODE,
    { {0, N_("Auto")},
      {1, N_("On")},
      {2, N_("Off")},
      {3, N_("Red-eye reduction")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_MACRO,
    { {0, N_("Off")},
      {1, N_("On")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_FOCUS_MODE,
    { {0, N_("Auto")},
      {1, N_("Manual")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_SLOW_SYNC,
    { {0, N_("Off")},
      {1, N_("On")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_PICTURE_MODE,
    { {0, N_("Auto")},
      {1, N_("Portrait")},
      {2, N_("Landscape")},
      {4, N_("Sports")},
      {5, N_("Night")},
      {6, N_("Program AE")},
      {7, N_("Natural photo")},
      {8, N_("Vibration reduction")},
      {0x000A, N_("Sunset")},
      {0x000B, N_("Museum")},
      {0x000C, N_("Party")},
      {0x000D, N_("Flower")},
      {0x000E, N_("Text")},
      {0x000F, N_("NP & flash")},
      {0x0010, N_("Beach")},
      {0x0011, N_("Snow")},
      {0x0012, N_("Fireworks")},
      {0x0013, N_("Underwater")},
      {0x0100, N_("Aperture priority AE")},
      {0x0200, N_("Shutter priority AE")},
      {0x0300, N_("Manual exposure")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_CONT_TAKING,
    { {0, N_("Off")},
      {1, N_("On")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_FINEPIX_COLOR,
    { {0x00, N_("F-Standard")},
      {0x10, N_("F-Chrome")},
      {0x30, N_("F-B&W")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_BLUR_CHECK,
    { {0, N_("No blur")},
      {1, N_("Blur warning")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_FOCUS_CHECK,
    { {0, N_("Focus good")},
      {1, N_("Out of focus")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_AUTO_EXPOSURE_CHECK,
    { {0, N_("AE good")},
      {1, N_("Over exposed")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_DYNAMIC_RANGE,
    { {1, N_("Standard")},
      {3, N_("Wide")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_FILM_MODE,
    { {0, N_("F0/Standard")},
      {0x0100, N_("F1/Studio portrait")},
      {0x0110, N_("F1a/Professional portrait")},
      {0x0120, N_("F1b/Professional portrait")},
      {0x0130, N_("F1c/Professional portrait")},
      {0x0200, N_("F2/Fujichrome")},
      {0x0300, N_("F3/Studio portrait Ex")},
      {0x0400, N_("F4/Velvia")},
      {0, NULL}}},
  { MNOTE_FUJI_TAG_DYNAMIC_RANGE_SETTING,
    { {0, N_("Auto (100-400%)")},
      {1, N_("RAW")},
      {0x0100, N_("Standard (100%)")},
      {0x0200, N_("Wide1 (230%)")},
      {0x0201, N_("Wide2 (400%)")},
      {0x8000, N_("Film simulation mode")},
      {0, NULL}}},
#endif
  {0, {{0, NULL}}}
};


char *
mnote_fuji_entry_get_value (MnoteFujiEntry *entry,
			      char *val, unsigned int maxlen)
{
	ExifLong  vl;
	ExifSLong vsl;
	ExifShort vs, vs2;
	ExifRational vr;
	ExifSRational vsr;
	int i, j;

	if (!entry) return (NULL);

	memset (val, 0, maxlen);
	maxlen--;

	switch (entry->tag) {
	  case MNOTE_FUJI_TAG_VERSION:
		CF (entry->format, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (entry->components, 4, val, maxlen);
		memcpy (val, entry->data, MIN(maxlen, entry->size));
		break;
	  case MNOTE_FUJI_TAG_SHARPNESS:
	  case MNOTE_FUJI_TAG_WHITE_BALANCE:
	  case MNOTE_FUJI_TAG_COLOR:
  	  case MNOTE_FUJI_TAG_TONE:
	  case MNOTE_FUJI_TAG_FLASH_MODE:
	  case MNOTE_FUJI_TAG_MACRO:
	  case MNOTE_FUJI_TAG_FOCUS_MODE:
	  case MNOTE_FUJI_TAG_SLOW_SYNC:
	  case MNOTE_FUJI_TAG_PICTURE_MODE:
	  case MNOTE_FUJI_TAG_CONT_TAKING:
	  case MNOTE_FUJI_TAG_FINEPIX_COLOR:
	  case MNOTE_FUJI_TAG_BLUR_CHECK:
	  case MNOTE_FUJI_TAG_FOCUS_CHECK:
	  case MNOTE_FUJI_TAG_AUTO_EXPOSURE_CHECK:
	  case MNOTE_FUJI_TAG_DYNAMIC_RANGE:
	  case MNOTE_FUJI_TAG_FILM_MODE:
	  case MNOTE_FUJI_TAG_DYNAMIC_RANGE_SETTING:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		CC (entry->components, 1, val, maxlen);
		vs = exif_get_short (entry->data, entry->order);

		/* search the tag */
		for (i = 0; (items[i].tag && items[i].tag != entry->tag); i++);
		if (!items[i].tag) {
			snprintf (val, maxlen,
				  _("Internal error (unknown value %i)"), vs);
		  	break;
		}

		/* find the value */
		for (j = 0; items[i].elem[j].string &&
		    (items[i].elem[j].index < vs); j++);
		if (items[i].elem[j].index != vs) {
			snprintf (val, maxlen,
				  _("Internal error (unknown value %i)"), vs);
			break;
		}
		strncpy (val, _(items[i].elem[j].string), maxlen);
		break;
	  case MNOTE_FUJI_TAG_FOCUS_POINT:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		CC (entry->components, 2, val, maxlen);
		vs = exif_get_short (entry->data, entry->order);
		vs2 = exif_get_short (entry->data+2, entry->order);
		snprintf (val, maxlen, "%i, %i", vs, vs2);
		break;
	  case MNOTE_FUJI_TAG_MIN_FOCAL_LENGTH:
	  case MNOTE_FUJI_TAG_MAX_FOCAL_LENGTH:
		CF (entry->format, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (entry->components, 1, val, maxlen);
		vr = exif_get_rational (entry->data, entry->order);
		if (!vr.denominator) break;
		snprintf (val, maxlen, _("%2.2f mm"), (double) vr.numerator /
			  vr.denominator);
		break;

	default:
		switch (entry->format) {
		case EXIF_FORMAT_ASCII:
		  strncpy (val, (char *)entry->data, MIN(maxlen, entry->size));
		  break;
		case EXIF_FORMAT_SHORT:
		  vs = exif_get_short (entry->data, entry->order);
		  snprintf (val, maxlen, "%i", vs);
		  break;
		case EXIF_FORMAT_LONG:
		  vl = exif_get_long (entry->data, entry->order);
		  snprintf (val, maxlen, "%lu", (long unsigned) vl);
		  break;
		case EXIF_FORMAT_SLONG:
		  vsl = exif_get_slong (entry->data, entry->order);
		  snprintf (val, maxlen, "%li", (long int) vsl);
		  break;
		case EXIF_FORMAT_RATIONAL:
		  vr = exif_get_rational (entry->data, entry->order);
		  if (!vr.denominator) break;
		  snprintf (val, maxlen, "%2.4f", (double) vr.numerator /
						    vr.denominator);
		  break;
		case EXIF_FORMAT_SRATIONAL:
		  vsr = exif_get_srational (entry->data, entry->order);
		  if (!vsr.denominator) break;
		  snprintf (val, maxlen, "%2.4f", (double) vsr.numerator /
			  vsr.denominator);
		  break;
		case EXIF_FORMAT_UNDEFINED:
		default:
		  snprintf (val, maxlen, _("%i bytes unknown data"),
 			  entry->size);
		  break;
		}
		break;
	}

	return (val);
}
