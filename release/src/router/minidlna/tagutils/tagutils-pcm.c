//=========================================================================
// FILENAME	: tagutils-pcm.c
// DESCRIPTION	: Return default PCM values.
//=========================================================================
// Copyright (c) 2009 NETGEAR, Inc. All Rights Reserved.
// based on software from Ron Pedde's FireFly Media Server project
//=========================================================================

/*
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

static int
_get_pcmfileinfo(char *filename, struct song_metadata *psong)
{
	struct stat file;
	uint32_t sec, ms;

	if( stat(filename, &file) != 0 )
	{
		DPRINTF(E_WARN, L_SCANNER, "Could not stat %s\n", filename);
		return -1;
	}

	psong->file_size = file.st_size;
	psong->bitrate = 1411200;
	psong->samplerate = 44100;
	psong->channels = 2;
	sec = psong->file_size / (psong->bitrate / 8);
	ms = ((psong->file_size % (psong->bitrate / 8)) * 1000) / (psong->bitrate / 8);
	psong->song_length = (sec * 1000) + ms;
	psong->lossless = 1;

	asprintf(&(psong->mime), "audio/L16;rate=%d;channels=%d", psong->samplerate, psong->channels);
	asprintf(&(psong->dlna_pn), "LPCM");

	return 0;
}
