/* mnote-pentax-tag.c:
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

#include <config.h>
#include "mnote-pentax-tag.h"

#include <stdlib.h>

#include <libexif/i18n.h>

static const struct {
	MnotePentaxTag tag;
	const char *name;
	const char *title;
	const char *description;
} table[] = {
#ifndef NO_VERBOSE_TAG_STRINGS
	{MNOTE_PENTAX_TAG_MODE, "Mode", N_("Capture Mode"), ""},
	{MNOTE_PENTAX_TAG_QUALITY, "Quality", N_("Quality Level"), ""},
	{MNOTE_PENTAX_TAG_FOCUS, "Focus", N_("Focus Mode"), ""},
	{MNOTE_PENTAX_TAG_FLASH, "Flash", N_("Flash Mode"), ""},
	{MNOTE_PENTAX_TAG_UNKNOWN_05, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_06, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_WHITE_BALANCE, "WhiteBalance", N_("White Balance"), ""},
	{MNOTE_PENTAX_TAG_UNKNOWN_08, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_09, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_ZOOM, "Zoom", N_("Zoom"), NULL},
	{MNOTE_PENTAX_TAG_SHARPNESS, "Sharpness", N_("Sharpness"), ""},
	{MNOTE_PENTAX_TAG_CONTRAST, "Contrast", N_("Contrast"), ""},
	{MNOTE_PENTAX_TAG_SATURATION, "Saturation", N_("Saturation"), ""},
	{MNOTE_PENTAX_TAG_UNKNOWN_14, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_15, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_16, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_17, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_18, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_19, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_ISO_SPEED, "ISOSpeed", N_("ISOSpeed"), ""},
	{MNOTE_PENTAX_TAG_UNKNOWN_21, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_COLOR, "Color", N_("Colors"), ""},
	{MNOTE_PENTAX_TAG_UNKNOWN_24, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_UNKNOWN_25, NULL, NULL, NULL},
	{MNOTE_PENTAX_TAG_PRINTIM, "PrintIM", N_("PrintIM Settings"), ""},
	{MNOTE_PENTAX_TAG_TZ_CITY, "TimeZone", N_("Time Zone"), ""},
	{MNOTE_PENTAX_TAG_TZ_DST, "DaylightSavings", N_("Daylight Savings"), ""},

	{MNOTE_PENTAX2_TAG_MODE, "Mode", N_("Capture Mode"), ""},
	{MNOTE_PENTAX2_TAG_PREVIEW_SIZE, "PentaxPreviewSize", N_("Preview Size"), ""},
	{MNOTE_PENTAX2_TAG_PREVIEW_LENGTH, "PentaxPreviewLength", N_("Preview Length"), ""},
	{MNOTE_PENTAX2_TAG_PREVIEW_START, "PentaxPreviewStart", N_("Preview Start"), ""},
	{MNOTE_PENTAX2_TAG_MODEL_ID, "ModelID", N_("Model Identification"), ""},
	{MNOTE_PENTAX2_TAG_DATE, "Date", N_("Date"), ""},
	{MNOTE_PENTAX2_TAG_TIME, "Time", N_("Time"), ""},
	{MNOTE_PENTAX2_TAG_QUALITY, "Quality", N_("Quality Level"), ""},
	{MNOTE_PENTAX2_TAG_IMAGE_SIZE, "ImageSize", N_("Image Size"), ""},
	{MNOTE_PENTAX2_TAG_PICTURE_MODE, "PictureMode", N_("PictureMode"), ""},
	{MNOTE_PENTAX2_TAG_FLASH_MODE, "FlashMode", N_("Flash Mode"), ""},
	{MNOTE_PENTAX2_TAG_FOCUS_MODE, "FocusMode", N_("Focus Mode"), ""},
	{MNOTE_PENTAX2_TAG_AFPOINT_SELECTED, "AFPointSelected", N_("AF Point Selected"), ""},
	{MNOTE_PENTAX2_TAG_AUTO_AFPOINT, "AutoAFPoint", N_("Auto AF Point"), ""},
	{MNOTE_PENTAX2_TAG_FOCUS_POSITION, "FocusPosition", N_("Focus Position"), ""},
	{MNOTE_PENTAX2_TAG_EXPOSURE_TIME, "ExposureTime", N_("Exposure Time"), ""},
	{MNOTE_PENTAX2_TAG_FNUMBER, "FNumber", N_("F-Number"), ""},
	{MNOTE_PENTAX2_TAG_ISO, "ISO", N_("ISO Number"), ""},
	{MNOTE_PENTAX2_TAG_EXPOSURE_COMPENSATION, "ExposureCompensation", N_("Exposure Compensation"), ""},
	{MNOTE_PENTAX2_TAG_METERING_MODE, "MeteringMode", N_("Metering Mode"), ""},
	{MNOTE_PENTAX2_TAG_AUTO_BRACKETING, "AutoBracketing", N_("Auto Bracketing"), ""},
	{MNOTE_PENTAX2_TAG_WHITE_BALANCE, "WhiteBalance", N_("White Balance"), ""},
	{MNOTE_PENTAX2_TAG_WHITE_BALANCE_MODE, "WhiteBalanceMode", N_("White Balance Mode"), ""},
	{MNOTE_PENTAX2_TAG_BLUE_BALANCE, "BlueBalance", N_("Blue Balance"), ""},
	{MNOTE_PENTAX2_TAG_RED_BALANCE, "RedBalance", N_("Red Balance"), ""},
	{MNOTE_PENTAX2_TAG_FOCAL_LENGTH, "FocalLength", N_("Focal Length"), ""},
	{MNOTE_PENTAX2_TAG_DIGITAL_ZOOM, "DigitalZoom", N_("Digital Zoom"), ""},
	{MNOTE_PENTAX2_TAG_SATURATION, "Saturation", N_("Saturation"), ""},
	{MNOTE_PENTAX2_TAG_CONTRAST, "Contrast", N_("Contrast"), ""},
	{MNOTE_PENTAX2_TAG_SHARPNESS, "Sharpness", N_("Sharpness"), ""},
	{MNOTE_PENTAX2_TAG_WORLDTIME_LOCATION, "WorldTimeLocation", N_("World Time Location"), ""},
	{MNOTE_PENTAX2_TAG_HOMETOWN_CITY, "HometownCity", N_("Hometown City"), ""},
	{MNOTE_PENTAX2_TAG_DESTINATION_CITY, "DestinationCity", N_("Destination City"), ""},
	{MNOTE_PENTAX2_TAG_HOMETOWN_DST, "HometownDST,", N_("Hometown DST"), N_("Home Daylight Savings Time")},
	{MNOTE_PENTAX2_TAG_DESTINATION_DST, "DestinationDST", N_("Destination DST"), N_("Destination Daylight Savings Time")},
	{MNOTE_PENTAX2_TAG_FRAME_NUMBER, "FrameNumber", N_("Frame Number"), ""},
	{MNOTE_PENTAX2_TAG_IMAGE_PROCESSING, "ImageProcessing", N_("Image Processing"), ""},
	{MNOTE_PENTAX2_TAG_PICTURE_MODE2, "PictureMode2", N_("Picture Mode (2)"), ""},
	{MNOTE_PENTAX2_TAG_DRIVE_MODE, "DriveMode", N_("Drive Mode"), ""},
	{MNOTE_PENTAX2_TAG_COLOR_SPACE, "ColorSpace", N_("Color Space"), ""},
	{MNOTE_PENTAX2_TAG_IMAGE_AREA_OFFSET, "ImageAreaOffset", N_("Image Area Offset"), ""},
	{MNOTE_PENTAX2_TAG_RAW_IMAGE_SIZE, "RawImageSize", N_("Raw Image Size"), ""},
	{MNOTE_PENTAX2_TAG_AFPOINTS_USED, "AfPointsUsed,", N_("Autofocus Points Used"), ""},
	{MNOTE_PENTAX2_TAG_LENS_TYPE, "LensType", N_("Lens Type"), ""},
	{MNOTE_PENTAX2_TAG_CAMERA_TEMPERATURE, "CameraTemperature", N_("Camera Temperature"), ""},
	{MNOTE_PENTAX2_TAG_NOISE_REDUCTION, "NoiseReduction", N_("Noise Reduction"), ""},
	{MNOTE_PENTAX2_TAG_FLASH_EXPOSURE_COMP, "FlashExposureComp", N_("Flash Exposure Compensation"), ""},
	{MNOTE_PENTAX2_TAG_IMAGE_TONE, "ImageTone", N_("Image Tone"), ""},
	{MNOTE_PENTAX2_TAG_SHAKE_REDUCTION_INFO, "ShakeReductionInfo,", N_("Shake Reduction Info"), ""},
	{MNOTE_PENTAX2_TAG_BLACK_POINT, "BlackPoint", N_("Black Point"), ""},
	{MNOTE_PENTAX2_TAG_WHITE_POINT, "WhitePoint", N_("White Point"), ""},
	{MNOTE_PENTAX2_TAG_AE_INFO, "AEInfo", N_("AE Info"), ""},
	{MNOTE_PENTAX2_TAG_LENS_INFO, "LensInfo", N_("Lens Info"), ""},
	{MNOTE_PENTAX2_TAG_FLASH_INFO, "FlashInfo", N_("Flash Info"), ""},
	{MNOTE_PENTAX2_TAG_CAMERA_INFO, "CameraInfo", N_("Camera Info"), ""},
	{MNOTE_PENTAX2_TAG_BATTERY_INFO, "BatteryInfo", N_("Battery Info"), ""},
	{MNOTE_PENTAX2_TAG_HOMETOWN_CITY_CODE, "HometownCityCode", N_("Hometown City Code"), ""},
	{MNOTE_PENTAX2_TAG_DESTINATION_CITY_CODE, "DestinationCityCode", N_("Destination City Code"), ""},

	{MNOTE_CASIO2_TAG_PREVIEW_START, "CasioPreviewStart", N_("Preview Start"), ""},
	{MNOTE_CASIO2_TAG_WHITE_BALANCE_BIAS, "WhiteBalanceBias", N_("White Balance Bias"), ""},
	{MNOTE_CASIO2_TAG_WHITE_BALANCE, "WhiteBalance", N_("White Balance"), ""},
	{MNOTE_CASIO2_TAG_OBJECT_DISTANCE, "ObjectDistance", N_("Object Distance"), N_("Distance of photographed object in millimeters.")},
	{MNOTE_CASIO2_TAG_FLASH_DISTANCE, "FlashDistance", N_("Flash Distance"), ""},
	{MNOTE_CASIO2_TAG_RECORD_MODE, "RecordMode", N_("Record Mode"), ""},
	{MNOTE_CASIO2_TAG_SELF_TIMER, "SelfTimer", N_("Self Timer"), ""},
	{MNOTE_CASIO2_TAG_QUALITY, "CasioQuality", N_("Quality Level"), ""},
	{MNOTE_CASIO2_TAG_FOCUS_MODE, "CasioFocusMode", N_("Focus Mode"), ""},
	{MNOTE_CASIO2_TAG_TIME_ZONE, "TimeZone", N_("Time Zone"), ""},
	{MNOTE_CASIO2_TAG_BESTSHOT_MODE, "BestshotMode", N_("Bestshot Mode"), ""},
	{MNOTE_CASIO2_TAG_CCS_ISO_SENSITIVITY, "CCSISOSensitivity", N_("CCS ISO Sensitivity"), ""},
	{MNOTE_CASIO2_TAG_COLOR_MODE, "ColorMode", N_("Color Mode"), ""},
	{MNOTE_CASIO2_TAG_ENHANCEMENT, "Enhancement", N_("Enhancement"), ""},
	{MNOTE_CASIO2_TAG_FINER, "Finer", N_("Finer"), ""},
#endif
	{0, NULL, NULL, NULL}
};

const char *
mnote_pentax_tag_get_name (MnotePentaxTag t)
{
	unsigned int i;

	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) return (table[i].name);
	return NULL;
}

const char *
mnote_pentax_tag_get_title (MnotePentaxTag t)
{
	unsigned int i;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) return (_(table[i].title));
	return NULL;
}

const char *
mnote_pentax_tag_get_description (MnotePentaxTag t)
{
	unsigned int i;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++)
		if (table[i].tag == t) {
			if (!table[i].description || !*table[i].description)
				return "";
			return (_(table[i].description));
		}
	return NULL;
}
