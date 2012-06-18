/* mnote-fuji-tag.c
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

#include <config.h>
#include <libexif/i18n.h>

#include "mnote-fuji-tag.h"


static const struct {
	MnoteFujiTag tag;
	const char *name;
	const char *title;
	const char *description;
} table[] = {
#ifndef NO_VERBOSE_TAG_STRINGS
	{MNOTE_FUJI_TAG_VERSION, "Version", N_("Maker Note Version"), ""},
	{MNOTE_FUJI_TAG_SERIAL_NUMBER, "SerialNumber", N_("Serial Number"), N_("This number is unique, it contains the date of manufacture.")},
	{MNOTE_FUJI_TAG_QUALITY, "Quality", N_("Quality"), ""},
	{MNOTE_FUJI_TAG_SHARPNESS, "Sharpness", N_("Sharpness"), ""},
	{MNOTE_FUJI_TAG_WHITE_BALANCE, "WhiteBalance", N_("White Balance"), ""},
	{MNOTE_FUJI_TAG_COLOR, "ChromaticitySaturation", N_("Chromaticity Saturation"), ""},
	{MNOTE_FUJI_TAG_TONE, "Contrast", N_("Contrast"), ""},
	{MNOTE_FUJI_TAG_FLASH_MODE, "FlashMode", N_("Flash Mode"), ""},
	{MNOTE_FUJI_TAG_FLASH_STRENGTH, "FlashStrength", N_("Flash Firing Strength Compensation"), ""},
	{MNOTE_FUJI_TAG_MACRO, "MacroMode", N_("Macro mode"), ""},
	{MNOTE_FUJI_TAG_FOCUS_MODE, "FocusingMode", N_("Focusing Mode"), ""},
	{MNOTE_FUJI_TAG_FOCUS_POINT, "FocusPoint", N_("Focus Point"), ""},
	{MNOTE_FUJI_TAG_SLOW_SYNC, "SlowSynchro", N_("Slow Synchro Mode"), ""},
	{MNOTE_FUJI_TAG_PICTURE_MODE, "PictureMode", N_("Picture Mode"), ""},
	{MNOTE_FUJI_TAG_CONT_TAKING, "ContinuousTaking", N_("Continuous Taking"), ""},
	{MNOTE_FUJI_TAG_SEQUENCE_NUMBER, "ContinuousSequence", N_("Continuous Sequence Number"), ""},
	{MNOTE_FUJI_TAG_FINEPIX_COLOR, "FinePixColor", N_("FinePix Color"), ""},
	{MNOTE_FUJI_TAG_BLUR_CHECK, "BlurCheck", N_("Blur Check"), ""},
	{MNOTE_FUJI_TAG_FOCUS_CHECK, "AutoFocusCheck", N_("Auto Focus Check"), ""},
	{MNOTE_FUJI_TAG_AUTO_EXPOSURE_CHECK, "AutoExposureCheck", N_("Auto Exposure Check"), ""},
	{MNOTE_FUJI_TAG_DYNAMIC_RANGE, "DynamicRange", N_("Dynamic Range"), ""},
	{MNOTE_FUJI_TAG_FILM_MODE, "FilmMode", N_("Film Simulation Mode"), ""},
	{MNOTE_FUJI_TAG_DYNAMIC_RANGE_SETTING, "DRangeMode", N_("Dynamic Range Wide Mode"), ""},
	{MNOTE_FUJI_TAG_DEV_DYNAMIC_RANGE_SETTING, "DevDRangeMode", N_("Development Dynamic Range Wide Mode"), ""},
	{MNOTE_FUJI_TAG_MIN_FOCAL_LENGTH, "MinFocalLen", N_("Minimum Focal Length"), ""},
	{MNOTE_FUJI_TAG_MAX_FOCAL_LENGTH, "MaxFocalLen", N_("Maximum Focal Length"), ""},
	{MNOTE_FUJI_TAG_MAX_APERT_AT_MIN_FOC, "MaxApertAtMinFoc", N_("Maximum Aperture at Minimum Focal"), ""},
	{MNOTE_FUJI_TAG_MAX_APERT_AT_MAX_FOC, "MaxApertAtMaxFoc", N_("Maximum Aperture at Maximum Focal"), ""},
	{MNOTE_FUJI_TAG_FILE_SOURCE, "FileSource", N_("File Source"), ""},
	{MNOTE_FUJI_TAG_ORDER_NUMBER, "OrderNumber", N_("Order Number"), ""},
	{MNOTE_FUJI_TAG_FRAME_NUMBER, "FrameNumber", N_("Frame Number"), ""},
#endif
	{0, NULL, NULL, NULL}
};

const char *
mnote_fuji_tag_get_name (MnoteFujiTag t)
{
	unsigned int i;

	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) return (table[i].name);
	return NULL;
}

const char *
mnote_fuji_tag_get_title (MnoteFujiTag t)
{
	unsigned int i;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) return (_(table[i].title));
	return NULL;
}

const char *
mnote_fuji_tag_get_description (MnoteFujiTag t)
{
	unsigned int i;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) {
			if (!*table[i].description)
				return "";
			return (_(table[i].description));
		}
	return NULL;
}
