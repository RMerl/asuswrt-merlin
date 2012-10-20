/* MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <locale.h>
#include <libgen.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "config.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#include <sqlite3.h>

#include "upnpglobalvars.h"
#include "metadata.h"
#include "playlist.h"
#include "utils.h"
#include "sql.h"
#include "scanner.h"
#include "albumart.h"
#include "log.h"

#ifndef AV_LOG_PANIC
#define AV_LOG_PANIC AV_LOG_FATAL
#endif

int valid_cache = 0;

struct virtual_item
{
	sqlite3_int64 objectID;
	char parentID[64];
	char name[256];
};

sqlite_int64
get_next_available_id(const char * table, const char * parentID)
{
		char *ret, *base;
		sqlite_int64 objectID = 0;

		ret = sql_get_text_field(db, "SELECT OBJECT_ID from %s where ID = "
		                             "(SELECT max(ID) from %s where PARENT_ID = '%s')",
		                             table, table, parentID);
		if( ret )
		{
			base = strrchr(ret, '$');
			if( base )
				objectID = strtoll(base+1, NULL, 16) + 1;
			sqlite3_free(ret);
		}

		return objectID;
}

int
insert_container(const char * item, const char * rootParent, const char * refID, const char *class,
                 const char *artist, const char *genre, const char *album_art, sqlite3_int64 *objectID, sqlite3_int64 *parentID)
{
	char *result;
	char *base;
	int ret = 0;
	sqlite_int64 detailID = 0;

	result = sql_get_text_field(db, "SELECT OBJECT_ID from OBJECTS o "
	                                "left join DETAILS d on (o.DETAIL_ID = d.ID)"
	                                " where o.PARENT_ID = '%s'"
			                " and o.NAME like '%q'"
			                " and d.ARTIST %s %Q"
	                                " and o.CLASS = 'container.%s' limit 1",
	                                rootParent, item, artist?"like":"is", artist, class);
	if( result )
	{
		base = strrchr(result, '$');
		if( base )
			*parentID = strtoll(base+1, NULL, 16);
		else
			*parentID = 0;
		*objectID = get_next_available_id("OBJECTS", result);
	}
	else
	{
		*objectID = 0;
		*parentID = get_next_available_id("OBJECTS", rootParent);
		if( refID )
		{
			result = sql_get_text_field(db, "SELECT DETAIL_ID from OBJECTS where OBJECT_ID = %Q", refID);
			if( result )
				detailID = strtoll(result, NULL, 10);
		}
		if( !detailID )
		{
			detailID = GetFolderMetadata(item, NULL, artist, genre, (album_art ? strtoll(album_art, NULL, 10) : 0));
		}
		ret = sql_exec(db, "INSERT into OBJECTS"
		                   " (OBJECT_ID, PARENT_ID, REF_ID, DETAIL_ID, CLASS, NAME) "
		                   "VALUES"
		                   " ('%s$%"PRIX64"', '%s', %Q, %"PRId64", 'container.%s', '%q')",
		                   rootParent, *parentID, rootParent, refID, detailID, class, item);
	}
	sqlite3_free(result);

	return ret;
}

static void
insert_containers(const char * name, const char *path, const char * refID, const char * class, sqlite3_int64 detailID)
{
	char sql[128];
	char **result;
	int ret;
	int cols, row;
	sqlite_int64 objectID, parentID;

	if( strstr(class, "imageItem") )
	{
		char *date = NULL, *cam = NULL;
		char date_taken[13], camera[64];
		static struct virtual_item last_date;
		static struct virtual_item last_cam;
		static struct virtual_item last_camdate;
		static sqlite_int64 last_all_objectID = 0;

		snprintf(sql, sizeof(sql), "SELECT DATE, CREATOR from DETAILS where ID = %lld", detailID);
		ret = sql_get_table(db, sql, &result, &row, &cols);
		if( ret == SQLITE_OK )
		{
			date = result[2];
			cam = result[3];
		}

		if( date )
		{
			strncpy(date_taken, date, 10);
			date_taken[10] = '\0';
		}
		else
		{
			strcpy(date_taken, _("Unknown Date"));
		}
		if( valid_cache && strcmp(last_date.name, date_taken) == 0 )
		{
			last_date.objectID++;
			//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Using last date item: %s/%s/%X\n", last_date.name, last_date.parentID, last_date.objectID);
		}
		else
		{
			insert_container(date_taken, IMAGE_DATE_ID, NULL, "album.photoAlbum", NULL, NULL, NULL, &objectID, &parentID);
			sprintf(last_date.parentID, IMAGE_DATE_ID"$%"PRIX64, parentID);
			last_date.objectID = objectID;
			strcpy(last_date.name, date_taken);
			//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Creating cached date item: %s/%s/%X\n", last_date.name, last_date.parentID, last_date.objectID);
		}
		sql_exec(db, "INSERT into OBJECTS"
		             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
		             "VALUES"
		             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
		             last_date.parentID, last_date.objectID, last_date.parentID, refID, class, detailID, name);

		if( cam )
		{
			strncpy(camera, cam, 63);
			camera[63] = '\0';
		}
		else
		{
			strcpy(camera, _("Unknown Camera"));
		}
		if( !valid_cache || strcmp(camera, last_cam.name) != 0 )
		{
			insert_container(camera, IMAGE_CAMERA_ID, NULL, "storageFolder", NULL, NULL, NULL, &objectID, &parentID);
			sprintf(last_cam.parentID, IMAGE_CAMERA_ID"$%"PRIX64, parentID);
			strncpy(last_cam.name, camera, 255);
			last_camdate.name[0] = '\0';
		}
		if( valid_cache && strcmp(last_camdate.name, date_taken) == 0 )
		{
			last_camdate.objectID++;
			//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Using last camdate item: %s/%s/%s/%X\n", camera, last_camdate.name, last_camdate.parentID, last_camdate.objectID);
		}
		else
		{
			insert_container(date_taken, last_cam.parentID, NULL, "album.photoAlbum", NULL, NULL, NULL, &objectID, &parentID);
			sprintf(last_camdate.parentID, "%s$%"PRIX64, last_cam.parentID, parentID);
			last_camdate.objectID = objectID;
			strcpy(last_camdate.name, date_taken);
			//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Creating cached camdate item: %s/%s/%s/%X\n", camera, last_camdate.name, last_camdate.parentID, last_camdate.objectID);
		}
		sql_exec(db, "INSERT into OBJECTS"
		             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
		             "VALUES"
		             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
		             last_camdate.parentID, last_camdate.objectID, last_camdate.parentID, refID, class, detailID, name);
		/* All Images */
		if( !last_all_objectID )
		{
			last_all_objectID = get_next_available_id("OBJECTS", IMAGE_ALL_ID);
		}
		sql_exec(db, "INSERT into OBJECTS"
		             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
		             "VALUES"
		             " ('"IMAGE_ALL_ID"$%"PRIX64"', '"IMAGE_ALL_ID"', '%s', '%s', %"PRId64", %Q)",
		             last_all_objectID++, refID, class, detailID, name);
	}
	else if( strstr(class, "audioItem") )
	{
		snprintf(sql, sizeof(sql), "SELECT ALBUM, ARTIST, GENRE, ALBUM_ART from DETAILS where ID = %lld", detailID);
		ret = sql_get_table(db, sql, &result, &row, &cols);
		if( ret != SQLITE_OK )
			return;
		if( !row )
		{
			sqlite3_free_table(result);
			return;
		}
		char *album = result[4], *artist = result[5], *genre = result[6];
		char *album_art = result[7];
		static struct virtual_item last_album;
		static struct virtual_item last_artist;
		static struct virtual_item last_artistAlbum;
		static struct virtual_item last_artistAlbumAll;
		static struct virtual_item last_genre;
		static struct virtual_item last_genreArtist;
		static struct virtual_item last_genreArtistAll;
		static sqlite_int64 last_all_objectID = 0;

		if( album )
		{
			if( valid_cache && strcmp(album, last_album.name) == 0 )
			{
				last_album.objectID++;
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Using last album item: %s/%s/%X\n", last_album.name, last_album.parentID, last_album.objectID);
			}
			else
			{
				strcpy(last_album.name, album);
				insert_container(album, MUSIC_ALBUM_ID, NULL, "album.musicAlbum", artist, genre, album_art, &objectID, &parentID);
				sprintf(last_album.parentID, MUSIC_ALBUM_ID"$%llX", parentID);
				last_album.objectID = objectID;
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Creating cached album item: %s/%s/%X\n", last_album.name, last_album.parentID, last_album.objectID);
			}
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
			             "VALUES"
			             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
			             last_album.parentID, last_album.objectID, last_album.parentID, refID, class, detailID, name);
		}
		if( artist )
		{
			if( !valid_cache || strcmp(artist, last_artist.name) != 0 )
			{
				insert_container(artist, MUSIC_ARTIST_ID, NULL, "person.musicArtist", NULL, genre, NULL, &objectID, &parentID);
				sprintf(last_artist.parentID, MUSIC_ARTIST_ID"$%"PRIX64, parentID);
				strcpy(last_artist.name, artist);
				last_artistAlbum.name[0] = '\0';
				/* Add this file to the "- All Albums -" container as well */
				insert_container(_("- All Albums -"), last_artist.parentID, NULL, "album", artist, genre, NULL, &objectID, &parentID);
				sprintf(last_artistAlbumAll.parentID, "%s$%"PRIX64, last_artist.parentID, parentID);
				last_artistAlbumAll.objectID = objectID;
			}
			else
			{
				last_artistAlbumAll.objectID++;
			}
			if( valid_cache && strcmp(album?album:_("Unknown Album"), last_artistAlbum.name) == 0 )
			{
				last_artistAlbum.objectID++;
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Using last artist/album item: %s/%s/%X\n", last_artist.name, last_artist.parentID, last_artist.objectID);
			}
			else
			{
				insert_container(album?album:_("Unknown Album"), last_artist.parentID, album?last_album.parentID:NULL,
				                 "album.musicAlbum", artist, genre, album_art, &objectID, &parentID);
				sprintf(last_artistAlbum.parentID, "%s$%"PRIX64, last_artist.parentID, parentID);
				last_artistAlbum.objectID = objectID;
				strcpy(last_artistAlbum.name, album?album:_("Unknown Album"));
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Creating cached artist/album item: %s/%s/%X\n", last_artist.name, last_artist.parentID, last_artist.objectID);
			}
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
			             "VALUES"
			             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
			             last_artistAlbum.parentID, last_artistAlbum.objectID, last_artistAlbum.parentID, refID, class, detailID, name);
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
			             "VALUES"
			             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
			             last_artistAlbumAll.parentID, last_artistAlbumAll.objectID, last_artistAlbumAll.parentID, refID, class, detailID, name);
		}
		if( genre )
		{
			if( !valid_cache || strcmp(genre, last_genre.name) != 0 )
			{
				insert_container(genre, MUSIC_GENRE_ID, NULL, "genre.musicGenre", NULL, NULL, NULL, &objectID, &parentID);
				sprintf(last_genre.parentID, MUSIC_GENRE_ID"$%"PRIX64, parentID);
				strcpy(last_genre.name, genre);
				last_genreArtist.name[0] = '\0';
				/* Add this file to the "- All Artists -" container as well */
				insert_container(_("- All Artists -"), last_genre.parentID, NULL, "person", NULL, genre, NULL, &objectID, &parentID);
				sprintf(last_genreArtistAll.parentID, "%s$%"PRIX64, last_genre.parentID, parentID);
				last_genreArtistAll.objectID = objectID;
			}
			else
			{
				last_genreArtistAll.objectID++;
			}
			if( valid_cache && strcmp(artist?artist:_("Unknown Artist"), last_genreArtist.name) == 0 )
			{
				last_genreArtist.objectID++;
			}
			else
			{
				insert_container(artist?artist:_("Unknown Artist"), last_genre.parentID, artist?last_artist.parentID:NULL,
				                 "person.musicArtist", NULL, genre, NULL, &objectID, &parentID);
				sprintf(last_genreArtist.parentID, "%s$%"PRIX64, last_genre.parentID, parentID);
				last_genreArtist.objectID = objectID;
				strcpy(last_genreArtist.name, artist?artist:_("Unknown Artist"));
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Creating cached genre/artist item: %s/%s/%X\n", last_genreArtist.name, last_genreArtist.parentID, last_genreArtist.objectID);
			}
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
			             "VALUES"
			             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
			             last_genreArtist.parentID, last_genreArtist.objectID, last_genreArtist.parentID, refID, class, detailID, name);
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
			             "VALUES"
			             " ('%s$%"PRIX64"', '%s', '%s', '%s', %"PRId64", %Q)",
			             last_genreArtistAll.parentID, last_genreArtistAll.objectID, last_genreArtistAll.parentID, refID, class, detailID, name);
		}
		/* All Music */
		if( !last_all_objectID )
		{
			last_all_objectID = get_next_available_id("OBJECTS", MUSIC_ALL_ID);
		}
		sql_exec(db, "INSERT into OBJECTS"
		             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
		             "VALUES"
		             " ('"MUSIC_ALL_ID"$%"PRIX64"', '"MUSIC_ALL_ID"', '%s', '%s', %"PRId64", %Q)",
		             last_all_objectID++, refID, class, detailID, name);
	}
	else if( strstr(class, "videoItem") )
	{
		static sqlite_int64 last_all_objectID = 0;

		/* All Videos */
		if( !last_all_objectID )
		{
			last_all_objectID = get_next_available_id("OBJECTS", VIDEO_ALL_ID);
		}
		sql_exec(db, "INSERT into OBJECTS"
		             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
		             "VALUES"
		             " ('"VIDEO_ALL_ID"$%"PRIX64"', '"VIDEO_ALL_ID"', '%s', '%s', %"PRId64", %Q)",
		             last_all_objectID++, refID, class, detailID, name);
		return;
	}
	else
	{
		return;
	}
	sqlite3_free_table(result);
	valid_cache = 1;
}

int
insert_directory(const char * name, const char * path, const char * base, const char * parentID, int objectID)
{
	int found = 0;
	sqlite_int64 detailID = 0;
	char * refID = NULL;
	char class[] = "container.storageFolder";
	char *result, *p;
	static char last_found[256] = "-1";

	if( strcmp(base, BROWSEDIR_ID) != 0 )
	{
		if( asprintf(&refID, "%s%s$%X", BROWSEDIR_ID, parentID, objectID) == -1 )
			return 1;
	}

	if( refID )
	{
		char id_buf[64], parent_buf[64];
		char *dir_buf, *dir;
 		dir_buf = strdup(path);
		dir = dirname(dir_buf);
		snprintf(id_buf, sizeof(id_buf), "%s%s$%X", base, parentID, objectID);
		snprintf(parent_buf, sizeof(parent_buf), "%s%s", base, parentID);
		while( !found )
		{
			if( valid_cache && strcmp(id_buf, last_found) == 0 )
				break;
			if( sql_get_int_field(db, "SELECT count(*) from OBJECTS where OBJECT_ID = '%s'", id_buf) > 0 )
			{
				strcpy(last_found, id_buf);
				break;
			}
			/* Does not exist.  Need to create, and may need to create parents also */
			result = sql_get_text_field(db, "SELECT DETAIL_ID from OBJECTS where OBJECT_ID = '%s'", refID);
			if( result )
			{
				detailID = strtoll(result, NULL, 10);
				sqlite3_free(result);
			}
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, REF_ID, DETAIL_ID, CLASS, NAME) "
			             "VALUES"
			             " ('%s', '%s', %Q, %"PRId64", '%s', '%q')",
			             id_buf, parent_buf, refID, detailID, class, strrchr(dir, '/')+1);
			if( (p = strrchr(id_buf, '$')) )
				*p = '\0';
			if( (p = strrchr(parent_buf, '$')) )
				*p = '\0';
			if( (p = strrchr(refID, '$')) )
				*p = '\0';
			dir = dirname(dir);
		}
		free(refID);
		free(dir_buf);
		return 0;
	}

	detailID = GetFolderMetadata(name, path, NULL, NULL, find_album_art(path, NULL, 0));
	sql_exec(db, "INSERT into OBJECTS"
	             " (OBJECT_ID, PARENT_ID, REF_ID, DETAIL_ID, CLASS, NAME) "
	             "VALUES"
	             " ('%s%s$%X', '%s%s', %Q, %"PRId64", '%s', '%q')",
	             base, parentID, objectID, base, parentID, refID, detailID, class, name);
	if( refID )
		free(refID);

	return 0;
}

int
insert_file(char * name, const char * path, const char * parentID, int object)
{
	char class[32];
	char objectID[64];
	sqlite3_int64 detailID = 0;
	char base[8];
	char * typedir_parentID;
	int typedir_objectID;
	char * baseid;
	char * orig_name = NULL;

	if( is_image(name) )
	{
		if( is_album_art(name) )
			return -1;
		strcpy(base, IMAGE_DIR_ID);
		strcpy(class, "item.imageItem.photo");
		detailID = GetImageMetadata(path, name);
	}
	else if( is_video(name) )
	{
 		orig_name = strdup(name);
		strcpy(base, VIDEO_DIR_ID);
		strcpy(class, "item.videoItem");
		detailID = GetVideoMetadata(path, name);
		if( !detailID )
			strcpy(name, orig_name);
	}
	else if( is_playlist(name) )
	{
		if( insert_playlist(path, name) == 0 )
			return 1;
	}
	if( !detailID && is_audio(name) )
	{
		strcpy(base, MUSIC_DIR_ID);
		strcpy(class, "item.audioItem.musicTrack");
		detailID = GetAudioMetadata(path, name);
	}
	if( orig_name )
		free(orig_name);
	if( !detailID )
	{
		DPRINTF(E_WARN, L_SCANNER, "Unsuccessful getting details for %s!\n", path);
		return -1;
	}

	sprintf(objectID, "%s%s$%X", BROWSEDIR_ID, parentID, object);

	sql_exec(db, "INSERT into OBJECTS"
	             " (OBJECT_ID, PARENT_ID, CLASS, DETAIL_ID, NAME) "
	             "VALUES"
	             " ('%s', '%s%s', '%s', %"PRId64", '%q')",
	             objectID, BROWSEDIR_ID, parentID, class, detailID, name);

	if( *parentID )
	{
		typedir_objectID = 0;
		typedir_parentID = strdup(parentID);
		baseid = strrchr(typedir_parentID, '$');
		if( baseid )
		{
			typedir_objectID = strtol(baseid+1, NULL, 16);
			*baseid = '\0';
		}
		insert_directory(name, path, base, typedir_parentID, typedir_objectID);
		free(typedir_parentID);
	}
	sql_exec(db, "INSERT into OBJECTS"
	             " (OBJECT_ID, PARENT_ID, REF_ID, CLASS, DETAIL_ID, NAME) "
	             "VALUES"
	             " ('%s%s$%X', '%s%s', '%s', '%s', %"PRId64", '%q')",
	             base, parentID, object, base, parentID, objectID, class, detailID, name);

	insert_containers(name, path, objectID, class, detailID);
	return 0;
}

int
CreateDatabase(void)
{
	int ret, i;
	const char * containers[] = { "0","-1",   "root",
	                         MUSIC_ID, "0", _("Music"),
	                     MUSIC_ALL_ID, MUSIC_ID, _("All Music"),
	                   MUSIC_GENRE_ID, MUSIC_ID, _("Genre"),
	                  MUSIC_ARTIST_ID, MUSIC_ID, _("Artist"),
	                   MUSIC_ALBUM_ID, MUSIC_ID, _("Album"),
	                     MUSIC_DIR_ID, MUSIC_ID, _("Folders"),
	                   MUSIC_PLIST_ID, MUSIC_ID, _("Playlists"),

	                         VIDEO_ID, "0", _("Video"),
	                     VIDEO_ALL_ID, VIDEO_ID, _("All Video"),
	                     VIDEO_DIR_ID, VIDEO_ID, _("Folders"),

	                         IMAGE_ID, "0", _("Pictures"),
	                     IMAGE_ALL_ID, IMAGE_ID, _("All Pictures"),
	                    IMAGE_DATE_ID, IMAGE_ID, _("Date Taken"),
	                  IMAGE_CAMERA_ID, IMAGE_ID, _("Camera"),
	                     IMAGE_DIR_ID, IMAGE_ID, _("Folders"),

	                     BROWSEDIR_ID, "0", _("Browse Folders"),
			0 };

	ret = sql_exec(db, "CREATE TABLE OBJECTS ( "
					"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
					"OBJECT_ID TEXT UNIQUE NOT NULL, "
					"PARENT_ID TEXT NOT NULL, "
					"REF_ID TEXT DEFAULT NULL, "
					"CLASS TEXT NOT NULL, "
					"DETAIL_ID INTEGER DEFAULT NULL, "
					"NAME TEXT DEFAULT NULL"
					");");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE DETAILS ( "
					"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
					"PATH TEXT DEFAULT NULL, "
					"SIZE INTEGER, "
					"TIMESTAMP INTEGER, "
					"TITLE TEXT COLLATE NOCASE, "
					"DURATION TEXT, "
					"BITRATE INTEGER, "
					"SAMPLERATE INTEGER, "
					"CREATOR TEXT COLLATE NOCASE, "
					"ARTIST TEXT COLLATE NOCASE, "
					"ALBUM TEXT COLLATE NOCASE, "
					"GENRE TEXT COLLATE NOCASE, "
					"COMMENT TEXT, "
					"CHANNELS INTEGER, "
					"DISC INTEGER, "
					"TRACK INTEGER, "
					"DATE DATE, "
					"RESOLUTION TEXT, "
					"THUMBNAIL BOOL DEFAULT 0, "
					"ALBUM_ART INTEGER DEFAULT 0, "
					"ROTATION INTEGER, "
					"DLNA_PN TEXT, "
					"MIME TEXT"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE ALBUM_ART ( "
					"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
					"PATH TEXT NOT NULL"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE CAPTIONS ("
					"ID INTEGER PRIMARY KEY, "
					"PATH TEXT NOT NULL"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE BOOKMARKS ("
					"ID INTEGER PRIMARY KEY, "
					"SEC INTEGER"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE PLAYLISTS ("
					"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
					"NAME TEXT NOT NULL, "
					"PATH TEXT NOT NULL, "
					"ITEMS INTEGER DEFAULT 0, "
					"FOUND INTEGER DEFAULT 0"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "CREATE TABLE SETTINGS ("
					"UPDATE_ID INTEGER PRIMARY KEY DEFAULT 0, "
					"FLAGS INTEGER DEFAULT 0"
					")");
	if( ret != SQLITE_OK )
		goto sql_failed;
	ret = sql_exec(db, "INSERT into SETTINGS values (0, 0)");
	if( ret != SQLITE_OK )
		goto sql_failed;
	for( i=0; containers[i]; i=i+3 )
	{
		ret = sql_exec(db, "INSERT into OBJECTS (OBJECT_ID, PARENT_ID, DETAIL_ID, CLASS, NAME)"
		                   " values "
		                   "('%s', '%s', %lld, 'container.storageFolder', '%q')",
		                   containers[i], containers[i+1], GetFolderMetadata(containers[i+2], NULL, NULL, NULL, 0), containers[i+2]);
		if( ret != SQLITE_OK )
			goto sql_failed;
	}
	sql_exec(db, "create INDEX IDX_OBJECTS_OBJECT_ID ON OBJECTS(OBJECT_ID);");
	sql_exec(db, "create INDEX IDX_OBJECTS_PARENT_ID ON OBJECTS(PARENT_ID);");
	sql_exec(db, "create INDEX IDX_OBJECTS_DETAIL_ID ON OBJECTS(DETAIL_ID);");
	sql_exec(db, "create INDEX IDX_OBJECTS_CLASS ON OBJECTS(CLASS);");
	sql_exec(db, "create INDEX IDX_DETAILS_PATH ON DETAILS(PATH);");
	sql_exec(db, "create INDEX IDX_DETAILS_ID ON DETAILS(ID);");
	sql_exec(db, "create INDEX IDX_ALBUM_ART ON ALBUM_ART(ID);");
	sql_exec(db, "create INDEX IDX_SCANNER_OPT ON OBJECTS(PARENT_ID, NAME, OBJECT_ID);");

sql_failed:
	if( ret != SQLITE_OK )
		fprintf(stderr, "Error creating SQLite3 database!\n");
	return (ret != SQLITE_OK);
}

int
filter_audio(const struct dirent *d)
{
	return ( (*d->d_name != '.') &&
	         ((d->d_type == DT_DIR) ||
	          (d->d_type == DT_LNK) ||
	          (d->d_type == DT_UNKNOWN) ||
		  ((d->d_type == DT_REG) &&
		   (is_audio(d->d_name) ||
	            is_playlist(d->d_name)
		   )
	       ) ));
}

int
filter_video(const struct dirent *d)
{
	return ( (*d->d_name != '.') &&
	         ((d->d_type == DT_DIR) ||
	          (d->d_type == DT_LNK) ||
	          (d->d_type == DT_UNKNOWN) ||
		  ((d->d_type == DT_REG) &&
		   is_video(d->d_name) )
	       ) );
}

int
filter_images(const struct dirent *d)
{
	return ( (*d->d_name != '.') &&
	         ((d->d_type == DT_DIR) ||
	          (d->d_type == DT_LNK) ||
	          (d->d_type == DT_UNKNOWN) ||
		  ((d->d_type == DT_REG) &&
		   is_image(d->d_name) )
	       ) );
}

int
filter_media(const struct dirent *d)
{
	return ( (*d->d_name != '.') &&
	         ((d->d_type == DT_DIR) ||
	          (d->d_type == DT_LNK) ||
	          (d->d_type == DT_UNKNOWN) ||
	          ((d->d_type == DT_REG) &&
	           (is_image(d->d_name) ||
	            is_audio(d->d_name) ||
	            is_video(d->d_name) ||
	            is_playlist(d->d_name)
	           )
	       ) ));
}

void
ScanDirectory(const char * dir, const char * parent, enum media_types dir_type)
{
	struct dirent **namelist;
	int i, n, startID=0;
	char parent_id[PATH_MAX];
	char full_path[PATH_MAX];
	char * name = NULL;
	static long long unsigned int fileno = 0;
	enum file_types type;

	setlocale(LC_COLLATE, "");
	if( chdir(dir) != 0 )
		return;

	DPRINTF(parent?E_INFO:E_WARN, L_SCANNER, _("Scanning %s\n"), dir);
	switch( dir_type )
	{
		case ALL_MEDIA:
			n = scandir(".", &namelist, filter_media, alphasort);
			break;
		case AUDIO_ONLY:
			n = scandir(".", &namelist, filter_audio, alphasort);
			break;
		case VIDEO_ONLY:
			n = scandir(".", &namelist, filter_video, alphasort);
			break;
		case IMAGES_ONLY:
			n = scandir(".", &namelist, filter_images, alphasort);
			break;
		default:
			n = -1;
			break;
	}
	if (n < 0) {
		fprintf(stderr, "Error scanning %s [scandir]\n", dir);
		return;
	}

	if( !parent )
	{
		startID = get_next_available_id("OBJECTS", BROWSEDIR_ID);
	}

	for (i=0; i < n; i++)
	{
#if !USE_FORK
		if( quitting )
			break;
#endif
		type = TYPE_UNKNOWN;
		sprintf(full_path, "%s/%s", dir, namelist[i]->d_name);
		name = escape_tag(namelist[i]->d_name, 1);
		if(strstr(full_path,"/Download2/InComplete") || strstr(full_path,"/Download2/Seeds") || strstr(full_path,"/Download2/config") || strstr(full_path,"/Download2/action"))
			 continue;
		if((strncmp(name,"asusware",8) == 0))//eric added for have no need to scan asusware folder
			continue;
		if( namelist[i]->d_type == DT_DIR )
		{
			type = TYPE_DIR;
		}
		else if( namelist[i]->d_type == DT_REG )
		{
			type = TYPE_FILE;
		}
		else
		{
			type = resolve_unknown_type(full_path, dir_type);
		}
		if( (type == TYPE_DIR) && (access(full_path, R_OK|X_OK) == 0) )
		{
			insert_directory(name, full_path, BROWSEDIR_ID, (parent ? parent:""), i+startID);
			sprintf(parent_id, "%s$%X", (parent ? parent:""), i+startID);
			ScanDirectory(full_path, parent_id, dir_type);
		}
		else if( type == TYPE_FILE && (access(full_path, R_OK) == 0) )
		{
			if( insert_file(name, full_path, (parent ? parent:""), i+startID) == 0 )
				fileno++;
		}
		free(name);
		free(namelist[i]);
	}
	free(namelist);
	if( parent )
	{
		chdir(dirname((char*)dir));
	}
	else
	{
		DPRINTF(E_WARN, L_SCANNER, _("Scanning %s finished (%llu files)!\n"), dir, fileno);
	}
}

void
start_scanner()
{
	struct media_dir_s * media_path = media_dirs;
	char name[PATH_MAX];

	if (setpriority(PRIO_PROCESS, 0, 15) == -1)
		DPRINTF(E_WARN, L_INOTIFY,  "Failed to reduce scanner thread priority\n");

#ifdef READYNAS
	FILE * flag = fopen("/ramfs/.upnp-av_scan", "w");
	if( flag )
		fclose(flag);
#else
	create_scantag();
#endif
	freopen("/dev/null", "a", stderr);
	while( media_path )
	{
		strncpy(name, media_path->path, sizeof(name));
		GetFolderMetadata(basename(name), media_path->path, NULL, NULL, 0);
		ScanDirectory(media_path->path, NULL, media_path->type);
		media_path = media_path->next;
	}
	freopen("/proc/self/fd/2", "a", stderr);
#ifdef READYNAS
	if( access("/ramfs/.rescan_done", F_OK) == 0 )
		system("/bin/sh /ramfs/.rescan_done");
	unlink("/ramfs/.upnp-av_scan");
#else
	remove_scantag();
#endif
	/* Create this index after scanning, so it doesn't slow down the scanning process.
	 * This index is very useful for large libraries used with an XBox360 (or any
	 * client that uses UPnPSearch on large containers). */
	sql_exec(db, "create INDEX IDX_SEARCH_OPT ON OBJECTS(OBJECT_ID, CLASS, DETAIL_ID);");

	if( GETFLAG(NO_PLAYLIST_MASK) )
	{
		DPRINTF(E_WARN, L_SCANNER, "Playlist creation disabled\n");	  
	}
	else
	{
		fill_playlists();
	}

	DPRINTF(E_DEBUG, L_SCANNER, "Initial file scan completed\n", DB_VERSION);
	//JM: Set up a db version number, so we know if we need to rebuild due to a new structure.
	sql_exec(db, "pragma user_version = %d;", DB_VERSION);
}
