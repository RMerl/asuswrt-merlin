/* mnote-fuji-tag.h
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

#ifndef __MNOTE_FUJI_TAG_H__
#define __MNOTE_FUJI_TAG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <libexif/exif-data.h>

enum _MnoteFujiTag {
	MNOTE_FUJI_TAG_VERSION                  = 0x0000,
	MNOTE_FUJI_TAG_SERIAL_NUMBER            = 0x0010,
	MNOTE_FUJI_TAG_QUALITY                  = 0x1000,
	MNOTE_FUJI_TAG_SHARPNESS                = 0x1001,
	MNOTE_FUJI_TAG_WHITE_BALANCE            = 0x1002,
	MNOTE_FUJI_TAG_COLOR                    = 0x1003,
	MNOTE_FUJI_TAG_TONE                     = 0x1004,
	MNOTE_FUJI_TAG_UNKNOWN_1006             = 0x1006,
	MNOTE_FUJI_TAG_UNKNOWN_1007             = 0x1007,
	MNOTE_FUJI_TAG_UNKNOWN_1008             = 0x1008,
	MNOTE_FUJI_TAG_UNKNOWN_1009             = 0x1009,
	MNOTE_FUJI_TAG_UNKNOWN_100A             = 0x100A,
	MNOTE_FUJI_TAG_UNKNOWN_100B             = 0x100B,
	MNOTE_FUJI_TAG_FLASH_MODE               = 0x1010,
	MNOTE_FUJI_TAG_FLASH_STRENGTH           = 0x1011,
	MNOTE_FUJI_TAG_MACRO                    = 0x1020,
	MNOTE_FUJI_TAG_FOCUS_MODE               = 0x1021,
	MNOTE_FUJI_TAG_UNKNOWN_1022             = 0x1022,
	MNOTE_FUJI_TAG_FOCUS_POINT              = 0x1023,
	MNOTE_FUJI_TAG_UNKNOWN_1024             = 0x1024,
	MNOTE_FUJI_TAG_UNKNOWN_1025             = 0x1025,
	MNOTE_FUJI_TAG_SLOW_SYNC                = 0x1030,
	MNOTE_FUJI_TAG_PICTURE_MODE             = 0x1031,
	MNOTE_FUJI_TAG_UNKNOWN_1032             = 0x1032,
	MNOTE_FUJI_TAG_CONT_TAKING              = 0x1100,
	MNOTE_FUJI_TAG_SEQUENCE_NUMBER          = 0x1101,
	MNOTE_FUJI_TAG_UNKNOWN_1200             = 0x1200,
	MNOTE_FUJI_TAG_FINEPIX_COLOR            = 0x1210,
	MNOTE_FUJI_TAG_BLUR_CHECK               = 0x1300,
	MNOTE_FUJI_TAG_FOCUS_CHECK              = 0x1301,
	MNOTE_FUJI_TAG_AUTO_EXPOSURE_CHECK      = 0x1302,
	MNOTE_FUJI_TAG_UNKNOWN_1303             = 0x1303,
	MNOTE_FUJI_TAG_DYNAMIC_RANGE            = 0x1400,
	MNOTE_FUJI_TAG_FILM_MODE                = 0x1401,
	MNOTE_FUJI_TAG_DYNAMIC_RANGE_SETTING    = 0x1402,
	MNOTE_FUJI_TAG_DEV_DYNAMIC_RANGE_SETTING= 0x1403,
	MNOTE_FUJI_TAG_MIN_FOCAL_LENGTH         = 0x1404,
	MNOTE_FUJI_TAG_MAX_FOCAL_LENGTH         = 0x1405,
	MNOTE_FUJI_TAG_MAX_APERT_AT_MIN_FOC     = 0x1406,
	MNOTE_FUJI_TAG_MAX_APERT_AT_MAX_FOC     = 0x1407,
	MNOTE_FUJI_TAG_UNKNOWN_1408             = 0x1408,
	MNOTE_FUJI_TAG_UNKNOWN_1409             = 0x1409,
	MNOTE_FUJI_TAG_UNKNOWN_140A             = 0x140A,
	MNOTE_FUJI_TAG_UNKNOWN_1410             = 0x1410,
	MNOTE_FUJI_TAG_UNKNOWN_1421             = 0x1421,
	MNOTE_FUJI_TAG_UNKNOWN_4100             = 0x4100,
	MNOTE_FUJI_TAG_UNKNOWN_4800             = 0x4800,
	MNOTE_FUJI_TAG_FILE_SOURCE              = 0x8000,
	MNOTE_FUJI_TAG_ORDER_NUMBER             = 0x8002,
	MNOTE_FUJI_TAG_FRAME_NUMBER             = 0x8003,
};
typedef enum _MnoteFujiTag MnoteFujiTag;

const char *mnote_fuji_tag_get_name        (MnoteFujiTag tag);
const char *mnote_fuji_tag_get_title       (MnoteFujiTag tag);
const char *mnote_fuji_tag_get_description (MnoteFujiTag tag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MNOTE_FUJI_TAG_H__ */
