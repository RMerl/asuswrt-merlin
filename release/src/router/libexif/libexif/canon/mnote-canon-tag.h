/* mnote-canon-tag.h
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

#ifndef __MNOTE_CANON_TAG_H__
#define __MNOTE_CANON_TAG_H__

#include <libexif/exif-data.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum _MnoteCanonTag {
	MNOTE_CANON_TAG_UNKNOWN_0	= 0x0,
	MNOTE_CANON_TAG_SETTINGS_1	= 0x1,
	MNOTE_CANON_TAG_FOCAL_LENGTH	= 0x2,
	MNOTE_CANON_TAG_UNKNOWN_3	= 0x3,
	MNOTE_CANON_TAG_SETTINGS_2	= 0x4,
	MNOTE_CANON_TAG_PANORAMA	= 0x5,
	MNOTE_CANON_TAG_IMAGE_TYPE	= 0x6,
	MNOTE_CANON_TAG_FIRMWARE	= 0x7,
	MNOTE_CANON_TAG_IMAGE_NUMBER	= 0x8,
	MNOTE_CANON_TAG_OWNER		= 0x9,
	MNOTE_CANON_TAG_UNKNOWN_10	= 0xa,
	MNOTE_CANON_TAG_SERIAL_NUMBER	= 0xc,
	MNOTE_CANON_TAG_UNKNOWN_13	= 0xd,
	MNOTE_CANON_TAG_CUSTOM_FUNCS	= 0xf,
	MNOTE_CANON_TAG_COLOR_INFORMATION = 0xa0
};
typedef enum _MnoteCanonTag MnoteCanonTag;

const char *mnote_canon_tag_get_name        (MnoteCanonTag);
const char *mnote_canon_tag_get_name_sub    (MnoteCanonTag, unsigned int, ExifDataOption);
const char *mnote_canon_tag_get_title       (MnoteCanonTag);
const char *mnote_canon_tag_get_title_sub   (MnoteCanonTag, unsigned int, ExifDataOption);
const char *mnote_canon_tag_get_description (MnoteCanonTag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MNOTE_CANON_TAG_H__ */
