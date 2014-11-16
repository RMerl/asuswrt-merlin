/* MiniDLNA media server
 * Copyright (C) 2014  NETGEAR
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include "clients.h"
#include "minidlnatypes.h"
#include "scanner.h"
#include "upnpglobalvars.h"
#include "containers.h"
#include "log.h"

#define NINETY_DAYS "7776000"

const char *music_id = MUSIC_ID;
const char *music_all_id = MUSIC_ALL_ID;
const char *music_genre_id = MUSIC_GENRE_ID;
const char *music_artist_id = MUSIC_ARTIST_ID;
const char *music_album_id = MUSIC_ALBUM_ID;
const char *music_plist_id = MUSIC_PLIST_ID;
const char *music_dir_id = MUSIC_DIR_ID;
const char *video_all_id = VIDEO_ALL_ID;
const char *video_dir_id = VIDEO_DIR_ID;
const char *image_all_id = IMAGE_ALL_ID;
const char *image_date_id = IMAGE_DATE_ID;
const char *image_camera_id = IMAGE_CAMERA_ID;
const char *image_dir_id = IMAGE_DIR_ID;

struct magic_container_s magic_containers[] =
{
	/* Alternate root container */
	{ NULL,
	  "0",
	  &runtime_vars.root_container,
	  NULL,
	  "0",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1,
	  0,
	},

	/* Recent 50 audio items */
	{ "Recently Added",
	  "1$FF0",
	  NULL,
	  "\"1$FF0$\" || OBJECT_ID",
	  "\"1$FF0\"",
	  "o.OBJECT_ID",
	  "(select null from DETAILS where MIME glob 'a*' and timestamp > (strftime('%s','now') - "NINETY_DAYS") limit 50)",
	  "MIME glob 'a*' and REF_ID is NULL and timestamp > (strftime('%s','now') - "NINETY_DAYS")",
	  "order by TIMESTAMP DESC",
	  50,
	  0,
	},

	/* Recent 50 video items */
	{ "Recently Added",
	  "2$FF0",
	  NULL,
	  "\"2$FF0$\" || OBJECT_ID",
	  "\"2$FF0\"",
	  "o.OBJECT_ID",
	  "(select null from DETAILS where MIME glob 'v*' and timestamp > (strftime('%s','now') - "NINETY_DAYS") limit 50)",
	  "MIME glob 'v*' and REF_ID is NULL and timestamp > (strftime('%s','now') - "NINETY_DAYS")",
	  "order by TIMESTAMP DESC",
	  50,
	  0,
	},

	/* Recent 50 image items */
	{ "Recently Added",
	  "3$FF0",
	  NULL,
	  "\"3$FF0$\" || OBJECT_ID",
	  "\"3$FF0\"",
	  "o.OBJECT_ID",
	  "(select null from DETAILS where MIME glob 'i*' and timestamp > (strftime('%s','now') - "NINETY_DAYS") limit 50)",
	  "MIME glob 'i*' and REF_ID is NULL and timestamp > (strftime('%s','now') - "NINETY_DAYS")",
	  "order by TIMESTAMP DESC",
	  50,
	  0,
	},

	/* Microsoft PlaysForSure containers */
	{ NULL, "4", &music_all_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "5", &music_genre_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "6", &music_artist_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "7", &music_album_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "8", &video_all_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "B", &image_all_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "C", &image_date_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "F", &music_plist_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "14", &music_dir_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "15", &video_dir_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "16", &image_dir_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },
	{ NULL, "D2", &image_camera_id, NULL, NULL, NULL, NULL, NULL, NULL, -1, FLAG_MS_PFS },

	/* Jump straight to Music on audio-only devices */
	{ NULL, "0", &music_id, NULL, "0", NULL, NULL, NULL, NULL, -1, FLAG_AUDIO_ONLY },

	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 }
};

struct magic_container_s *
in_magic_container(const char *id, int flags, const char **real_id)
{
	size_t len;
	int i;

	for (i = 0; magic_containers[i].objectid_match; i++)
	{
		if (magic_containers[i].required_flags && !(flags & magic_containers[i].required_flags))
			continue;
		if (magic_containers[i].objectid && !(*magic_containers[i].objectid))
			continue;
		DPRINTF(E_MAXDEBUG, L_HTTP, "Checking magic container %d [%s]\n", i, magic_containers[i].objectid_match);
		len = strlen(magic_containers[i].objectid_match);
		if (strncmp(id, magic_containers[i].objectid_match, len) == 0)
		{
 			if (*(id+len) == '$')
				*real_id = id+len+1;
 			else if (*(id+len) == '\0')
				*real_id = id;
			else
				continue;
			DPRINTF(E_DEBUG, L_HTTP, "Found magic container %d [%s]\n", i, magic_containers[i].objectid_match);
			return &magic_containers[i];
		}
	}

	return NULL;
}

struct magic_container_s *
check_magic_container(const char *id, int flags)
{
	int i;

	for (i = 0; magic_containers[i].objectid_match; i++)
	{
		if (magic_containers[i].required_flags && !(flags & magic_containers[i].required_flags))
			continue;
		if (magic_containers[i].objectid && !(*magic_containers[i].objectid))
			continue;
		DPRINTF(E_MAXDEBUG, L_HTTP, "Checking magic container %d [%s]\n", i, magic_containers[i].objectid_match);
		if (strcmp(id, magic_containers[i].objectid_match) == 0)
		{
			DPRINTF(E_DEBUG, L_HTTP, "Found magic container %d [%s]\n", i, magic_containers[i].objectid_match);
			return &magic_containers[i];
		}
	}

	return NULL;
}
