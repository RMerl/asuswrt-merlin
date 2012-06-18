/*
 * $Id: daap.h,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Build daap structs for replies
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _DAAP_H_
#define _DAAP_H_

#include "daap-proto.h"

DAAP_BLOCK *daap_response_server_info(char *name, char *client_version);
DAAP_BLOCK *daap_response_content_codes(void);
DAAP_BLOCK *daap_response_login(char *hostname);
DAAP_BLOCK *daap_response_update(int fd, int clientver);
DAAP_BLOCK *daap_response_songlist(char* metaInfo, char* query);
DAAP_BLOCK *daap_response_playlists(char *name);
DAAP_BLOCK *daap_response_dbinfo(char *name);
DAAP_BLOCK *daap_response_playlist_items(unsigned int playlist, char* metaStr, char* query);
void daap_handle_index(DAAP_BLOCK* block, const char* index);
DAAP_BLOCK* daap_add_song_entry(DAAP_BLOCK* mlcl, MP3FILE* song, unsigned long long meta);
DAAP_BLOCK* daap_response_browse(const char* name, const char* filter);

#endif /* _DAAP_H_ */
