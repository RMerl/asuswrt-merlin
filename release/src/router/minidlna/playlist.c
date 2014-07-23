/* MiniDLNA media server
 * Copyright (C) 2009-2010  Justin Maggard
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
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tagutils/tagutils.h"

#include "upnpglobalvars.h"
#include "scanner.h"
#include "metadata.h"
#include "utils.h"
#include "sql.h"
#include "log.h"

int
insert_playlist(const char * path, char * name)
{
	struct song_metadata plist;
	struct stat file;
	int items = 0, matches, ret;
	char type[4];

	strncpyt(type, strrchr(name, '.')+1, 4);

	if( start_plist(path, NULL, &file, NULL, type) != 0 )
	{
		DPRINTF(E_WARN, L_SCANNER, "Bad playlist [%s]\n", path);
		return -1;
	}
	while( (ret = next_plist_track(&plist, &file, NULL, type)) == 0 )
	{
		items++;
		freetags(&plist);
	}
	if( ret == 2 ) // Bad playlist -- contains binary characters
	{
		DPRINTF(E_WARN, L_SCANNER, "Bad playlist [%s]\n", path);
		return -1;
	}
	strip_ext(name);

	DPRINTF(E_DEBUG, L_SCANNER, "Playlist %s contains %d items\n", name, items);
	
	matches = sql_get_int_field(db, "SELECT count(*) from PLAYLISTS where NAME = '%q'", name);
	if( matches > 0 )
	{
		sql_exec(db, "INSERT into PLAYLISTS"
		             " (NAME, PATH, ITEMS) "
	        	     "VALUES"
		             " ('%q(%d)', '%q', %d)",
		             name, matches, path, items);
	}
	else
	{
		sql_exec(db, "INSERT into PLAYLISTS"
		             " (NAME, PATH, ITEMS) "
	        	     "VALUES"
		             " ('%q', '%q', %d)",
		             name, path, items);
	}
	return 0;
}

static unsigned int
gen_dir_hash(const char *path)
{
	char dir[PATH_MAX], *base;
	int len;

	strncpy(dir, path, sizeof(dir));
	dir[sizeof(dir)-1] = '\0';
	base = strrchr(dir, '/');
	if( !base )
		base = strrchr(dir, '\\');
	if( base )
	{
		*base = '\0';
		len = base - dir;
	}
	else
		return 0;
	

	return DJBHash((uint8_t *)dir, len);
}

int
fill_playlists(void)
{
	int rows, i, found, len;
	char **result;
	char *plpath, *plname, *fname, *last_dir;
	unsigned int hash, last_hash = 0;
	char class[] = "playlistContainer";
	struct song_metadata plist;
	struct stat file;
	char type[4];
	int64_t plID, detailID;
	char sql_buf[] = "SELECT ID, NAME, PATH from PLAYLISTS where ITEMS > FOUND";

	DPRINTF(E_WARN, L_SCANNER, "Parsing playlists...\n");

	if( sql_get_table(db, sql_buf, &result, &rows, NULL) != SQLITE_OK ) 
		return -1;
	if( !rows )
		goto done;

	rows++;
	for( i=3; i<rows*3; i++ )
	{
		plID = strtoll(result[i], NULL, 10);
		plname = result[++i];
		plpath = result[++i];
		last_dir = NULL;
		last_hash = 0;

		strncpyt(type, strrchr(plpath, '.')+1, 4);

		if( start_plist(plpath, NULL, &file, NULL, type) != 0 )
			continue;

		DPRINTF(E_DEBUG, L_SCANNER, "Scanning playlist \"%s\" [%s]\n", plname, plpath);
		if( sql_get_int_field(db, "SELECT ID from OBJECTS where PARENT_ID = '"MUSIC_PLIST_ID"'"
		                          " and NAME = '%q'", plname) <= 0 )
		{
			detailID = GetFolderMetadata(plname, NULL, NULL, NULL, 0);
			sql_exec(db, "INSERT into OBJECTS"
			             " (OBJECT_ID, PARENT_ID, DETAIL_ID, CLASS, NAME) "
			             "VALUES"
			             " ('%s$%llX', '%s', %lld, 'container.%s', '%q')",
			             MUSIC_PLIST_ID, plID, MUSIC_PLIST_ID, detailID, class, plname);
		}

		plpath = dirname(plpath);
		found = 0;
		while( next_plist_track(&plist, &file, NULL, type) == 0 )
		{
			hash = gen_dir_hash(plist.path);
			if( sql_get_int_field(db, "SELECT 1 from OBJECTS where OBJECT_ID = '%s$%llX$%d'",
			                      MUSIC_PLIST_ID, plID, plist.track) == 1 )
			{
				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "%d: already in database\n", plist.track);
				found++;
       				freetags(&plist);
				continue;
			}
			if( last_dir )
			{
				if( hash == last_hash )
				{
					fname = basename(plist.path);
					detailID = sql_get_int_field(db, "SELECT ID from DETAILS where PATH = '%q/%q'", last_dir, fname);
				}
				else
					detailID = -1;
				if( detailID <= 0 )
				{
					sqlite3_free(last_dir);
					last_dir = NULL;
				}
				else
					goto found;
			}
			
			fname = plist.path;
			DPRINTF(E_DEBUG, L_SCANNER, "%d: checking database for %s\n", plist.track, plist.path);
			if( !strpbrk(fname, "\\/") )
			{
				len = strlen(fname) + strlen(plpath) + 2;
				plist.path = malloc(len);
				snprintf(plist.path, len, "%s/%s", plpath, fname);
				free(fname);
				fname = plist.path;
			}
			else
			{
				while( *fname == '\\' )
				{
					fname++;
				}
			}
retry:
			//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "* Searching for %s in db\n", fname);
			detailID = sql_get_int_field(db, "SELECT ID from DETAILS where PATH like '%%%q'", fname);
			if( detailID > 0 )
			{
found:
				DPRINTF(E_DEBUG, L_SCANNER, "+ %s found in db\n", fname);
				sql_exec(db, "INSERT into OBJECTS"
				             " (OBJECT_ID, PARENT_ID, CLASS, DETAIL_ID, NAME, REF_ID) "
				             "SELECT"
				             " '%s$%llX$%d', '%s$%llX', CLASS, DETAIL_ID, NAME, OBJECT_ID from OBJECTS"
				             " where DETAIL_ID = %lld and OBJECT_ID glob '" BROWSEDIR_ID "$*'",
				             MUSIC_PLIST_ID, plID, plist.track,
				             MUSIC_PLIST_ID, plID,
				             detailID);
				if( !last_dir )
				{
					last_dir = sql_get_text_field(db, "SELECT PATH from DETAILS where ID = %lld", detailID);
					if( last_dir )
					{
						fname = strrchr(last_dir, '/');
						if( fname )
							*fname = '\0';
					}
					last_hash = hash;
				}
				found++;
			}
			else
			{
				DPRINTF(E_DEBUG, L_SCANNER, "- %s not found in db\n", fname);
				if( strchr(fname, '\\') )
				{
					fname = modifyString(fname, "\\", "/", 1);
					goto retry;
				}
				else if( (fname = strchr(fname, '/')) )
				{
					fname++;
					goto retry;
				}
			}
       			freetags(&plist);
		}
		if( last_dir )
		{
			sqlite3_free(last_dir);
			last_dir = NULL;
		}
		sql_exec(db, "UPDATE PLAYLISTS set FOUND = %d where ID = %lld", found, plID);
	}
done:
	sqlite3_free_table(result);
	DPRINTF(E_WARN, L_SCANNER, "Finished parsing playlists.\n");

	return 0;
}

