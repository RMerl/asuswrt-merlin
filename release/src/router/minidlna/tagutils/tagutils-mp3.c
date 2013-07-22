//=========================================================================
// FILENAME	: tagutils-mp3.c
// DESCRIPTION	: MP3 metadata reader
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
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

/*
 * This file is derived from mt-daap project.
 */

static int
_get_mp3tags(char *file, struct song_metadata *psong)
{
	struct id3_file *pid3file;
	struct id3_tag *pid3tag;
	struct id3_frame *pid3frame;
	int err;
	int index;
	int used;
	unsigned char *utf8_text;
	int genre = WINAMP_GENRE_UNKNOWN;
	int have_utf8;
	int have_text;
	id3_ucs4_t const *native_text;
	char *tmp;
	int got_numeric_genre;
	id3_byte_t const *image;
	id3_length_t image_size = 0;

	pid3file = id3_file_open(file, ID3_FILE_MODE_READONLY);
	if(!pid3file)
	{
		DPRINTF(E_ERROR, L_SCANNER, "Cannot open %s\n", file);
		return -1;
	}

	pid3tag = id3_file_tag(pid3file);

	if(!pid3tag)
	{
		err = errno;
		id3_file_close(pid3file);
		errno = err;
		DPRINTF(E_WARN, L_SCANNER, "Cannot get ID3 tag for %s\n", file);
		return -1;
	}

	index = 0;
	while((pid3frame = id3_tag_findframe(pid3tag, "", index)))
	{
		used = 0;
		utf8_text = NULL;
		native_text = NULL;
		have_utf8 = 0;
		have_text = 0;

		if(!strcmp(pid3frame->id, "YTCP"))   /* for id3v2.2 */
		{
			psong->compilation = 1;
			DPRINTF(E_DEBUG, L_SCANNER, "Compilation: %d [%s]\n", psong->compilation, basename(file));
		}
		else if(!strcmp(pid3frame->id, "APIC") && !image_size)
		{
			if( (strcmp((char*)id3_field_getlatin1(&pid3frame->fields[1]), "image/jpeg") == 0) ||
			    (strcmp((char*)id3_field_getlatin1(&pid3frame->fields[1]), "image/jpg") == 0) ||
			    (strcmp((char*)id3_field_getlatin1(&pid3frame->fields[1]), "jpeg") == 0) )
			{
				image = id3_field_getbinarydata(&pid3frame->fields[4], &image_size);
				if( image_size )
				{
					psong->image = malloc(image_size);
					memcpy(psong->image, image, image_size);
					psong->image_size = image_size;
					//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Found thumbnail: %d\n", psong->image_size);
				}
			}
		}

		if(((pid3frame->id[0] == 'T') || (strcmp(pid3frame->id, "COMM") == 0)) &&
		   (id3_field_getnstrings(&pid3frame->fields[1])))
			have_text = 1;

		if(have_text)
		{
			native_text = id3_field_getstrings(&pid3frame->fields[1], 0);

			if(native_text)
			{
				have_utf8 = 1;
				if(lang_index >= 0)
					utf8_text = _get_utf8_text(native_text); // through iconv
				else
					utf8_text = (unsigned char*)id3_ucs4_utf8duplicate(native_text);

				if(!strcmp(pid3frame->id, "TIT2"))
				{
					used = 1;
					psong->title = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TPE1"))
				{
					used = 1;
					psong->contributor[ROLE_ARTIST] = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TALB"))
				{
					used = 1;
					psong->album = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TCOM"))
				{
					used = 1;
					psong->contributor[ROLE_COMPOSER] = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TIT1"))
				{
					used = 1;
					psong->grouping = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TPE2"))
				{
					used = 1;
					psong->contributor[ROLE_BAND] = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TPE3"))
				{
					used = 1;
					psong->contributor[ROLE_CONDUCTOR] = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TCON"))
				{
					used = 1;
					psong->genre = (char*)utf8_text;
					got_numeric_genre = 0;
					if(psong->genre)
					{
						if(!strlen(psong->genre))
						{
							genre = WINAMP_GENRE_UNKNOWN;
							got_numeric_genre = 1;
						}
						else if(isdigit(psong->genre[0]))
						{
							genre = atoi(psong->genre);
							got_numeric_genre = 1;
						}
						else if((psong->genre[0] == '(') && (isdigit(psong->genre[1])))
						{
							genre = atoi((char*)&psong->genre[1]);
							got_numeric_genre = 1;
						}

						if(got_numeric_genre)
						{
							if((genre < 0) || (genre > WINAMP_GENRE_UNKNOWN))
								genre = WINAMP_GENRE_UNKNOWN;
							free(psong->genre);
							psong->genre = strdup(winamp_genre[genre]);
						}
					}
				}
				else if(!strcmp(pid3frame->id, "COMM"))
				{
					used = 1;
					psong->comment = (char*)utf8_text;
				}
				else if(!strcmp(pid3frame->id, "TPOS"))
				{
					tmp = (char*)utf8_text;
					strsep(&tmp, "/");
					if(tmp)
					{
						psong->total_discs = atoi(tmp);
					}
					psong->disc = atoi((char*)utf8_text);
				}
				else if(!strcmp(pid3frame->id, "TRCK"))
				{
					tmp = (char*)utf8_text;
					strsep(&tmp, "/");
					if(tmp)
					{
						psong->total_tracks = atoi(tmp);
					}
					psong->track = atoi((char*)utf8_text);
				}
				else if(!strcmp(pid3frame->id, "TDRC"))
				{
					psong->year = atoi((char*)utf8_text);
				}
				else if(!strcmp(pid3frame->id, "TLEN"))
				{
					psong->song_length = atoi((char*)utf8_text);
				}
				else if(!strcmp(pid3frame->id, "TBPM"))
				{
					psong->bpm = atoi((char*)utf8_text);
				}
				else if(!strcmp(pid3frame->id, "TCMP"))
				{
					psong->compilation = (char)atoi((char*)utf8_text);
				}
			}
		}

		// check if text tag
		if((!used) && (have_utf8) && (utf8_text))
			free(utf8_text);

		// v2 COMM
		if((!strcmp(pid3frame->id, "COMM")) && (pid3frame->nfields == 4))
		{
			native_text = id3_field_getstring(&pid3frame->fields[2]);
			if(native_text)
			{
				utf8_text = (unsigned char*)id3_ucs4_utf8duplicate(native_text);
				if((utf8_text) && (strncasecmp((char*)utf8_text, "iTun", 4) != 0))
				{
					// read comment
					free(utf8_text);

					native_text = id3_field_getfullstring(&pid3frame->fields[3]);
					if(native_text)
					{
						utf8_text = (unsigned char*)id3_ucs4_utf8duplicate(native_text);
						if(utf8_text)
						{
							free(psong->comment);
							psong->comment = (char*)utf8_text;
						}
					}
				}
				else
				{
					free(utf8_text);
				}
			}
		}

		index++;
	}

	id3_file_close(pid3file);
	//DEBUG DPRINTF(E_INFO, L_SCANNER, "Got id3 tag successfully for file=%s\n", file);
	return 0;
}

// _decode_mp3_frame
static int
_decode_mp3_frame(unsigned char *frame, struct mp3_frameinfo *pfi)
{
	int ver;
	int layer_index;
	int sample_index;
	int bitrate_index;
	int samplerate_index;

	if((frame[0] != 0xFF) || (frame[1] < 224))
	{
		pfi->is_valid = 0;
		return -1;
	}

	ver = (frame[1] & 0x18) >> 3;
	pfi->layer = 4 - ((frame[1] & 0x6) >> 1);

	layer_index = sample_index = -1;

	switch(ver)
	{
	case 0:
		pfi->mpeg_version = 0x25;       // 2.5
		sample_index = 2;
		if(pfi->layer == 1)
			layer_index = 3;
		if((pfi->layer == 2) || (pfi->layer == 3))
			layer_index = 4;
		break;
	case 2:
		pfi->mpeg_version = 0x20;       // 2.0
		sample_index = 1;
		if(pfi->layer == 1)
			layer_index = 3;
		if((pfi->layer == 2) || (pfi->layer == 3))
			layer_index = 4;
		break;
	case 3:
		pfi->mpeg_version = 0x10;       // 1.0
		sample_index = 0;
		if(pfi->layer == 1)
			layer_index = 0;
		if(pfi->layer == 2)
			layer_index = 1;
		if(pfi->layer == 3)
			layer_index = 2;
		break;
	}

	if((layer_index < 0) || (layer_index > 4))
	{
		pfi->is_valid = 0;
		return -1;
	}

	if((sample_index < 0) || (sample_index >= 2))
	{
		pfi->is_valid = 0;
		return -1;
	}

	if(pfi->layer == 1) pfi->samples_per_frame = 384;
	if(pfi->layer == 2) pfi->samples_per_frame = 1152;
	if(pfi->layer == 3)
	{
		if(pfi->mpeg_version == 0x10)
			pfi->samples_per_frame = 1152;
		else
			pfi->samples_per_frame = 576;
	}

	bitrate_index = (frame[2] & 0xF0) >> 4;
	samplerate_index = (frame[2] & 0x0C) >> 2;

	if((bitrate_index == 0xF) || (bitrate_index == 0x0))
	{
		pfi->is_valid = 0;
		return -1;
	}

	if(samplerate_index == 3)
	{
		pfi->is_valid = 0;
		return -1;
	}


	pfi->bitrate = bitrate_tbl[layer_index][bitrate_index];
	pfi->samplerate = sample_rate_tbl[sample_index][samplerate_index];

	if((frame[3] & 0xC0 >> 6) == 3)
		pfi->stereo = 0;
	else
		pfi->stereo = 1;

	if(frame[2] & 0x02)
		pfi->padding = 1;
	else
		pfi->padding = 0;

	if(pfi->mpeg_version == 0x10)
	{
		if(pfi->stereo)
			pfi->xing_offset = 32;
		else
			pfi->xing_offset = 17;
	}
	else
	{
		if(pfi->stereo)
			pfi->xing_offset = 17;
		else
			pfi->xing_offset = 9;
	}

	pfi->crc_protected = frame[1] & 0xFE;

	if(pfi->layer == 1)
		pfi->frame_length = (12 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding) * 4;
	else
		pfi->frame_length = 144 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding;

	if((pfi->frame_length > 2880) || (pfi->frame_length <= 0))
	{
		pfi->is_valid = 0;
		return -1;
	}

	pfi->is_valid = 1;
	return 0;
}

// _mp3_get_average_bitrate
//    read from midle of file, and estimate
static void _mp3_get_average_bitrate(FILE *infile, struct mp3_frameinfo *pfi, const char *fname)
{
	off_t file_size;
	unsigned char frame_buffer[2900];
	unsigned char header[4];
	int index = 0;
	int found = 0;
	off_t pos;
	struct mp3_frameinfo fi;
	int frame_count = 0;
	int bitrate_total = 0;

	fseek(infile, 0, SEEK_END);
	file_size = ftell(infile);

	pos = file_size >> 1;

	/* now, find the first frame */
	fseek(infile, pos, SEEK_SET);
	if(fread(frame_buffer, 1, sizeof(frame_buffer), infile) != sizeof(frame_buffer))
		return;

	while(!found)
	{
		while((frame_buffer[index] != 0xFF) && (index < (sizeof(frame_buffer) - 4)))
			index++;

		if(index >= (sizeof(frame_buffer) - 4))   // max mp3 framesize = 2880
		{
			DPRINTF(E_DEBUG, L_SCANNER, "Could not find frame for %s\n", basename((char *)fname));
			return;
		}

		if(!_decode_mp3_frame(&frame_buffer[index], &fi))
		{
			/* see if next frame is valid */
			fseek(infile, pos + index + fi.frame_length, SEEK_SET);
			if(fread(header, 1, sizeof(header), infile) != sizeof(header))
			{
				DPRINTF(E_DEBUG, L_SCANNER, "Could not read frame header for %s\n", basename((char *)fname));
				return;
			}

			if(!_decode_mp3_frame(header, &fi))
				found = 1;
		}

		if(!found)
			index++;
	}

	pos += index;

	// got first frame
	while(frame_count < 10)
	{
		fseek(infile, pos, SEEK_SET);
		if(fread(header, 1, sizeof(header), infile) != sizeof(header))
		{
			DPRINTF(E_DEBUG, L_SCANNER, "Could not read frame header for %s\n", basename((char *)fname));
			return;
		}
		if(_decode_mp3_frame(header, &fi))
		{
			DPRINTF(E_DEBUG, L_SCANNER, "Invalid frame header while averaging %s\n", basename((char *)fname));
			return;
		}

		bitrate_total += fi.bitrate;
		frame_count++;
		pos += fi.frame_length;
	}

	pfi->bitrate = bitrate_total / frame_count;

	return;
}

// _mp3_get_frame_count
//   do brute scan
static void __attribute__((unused))
_mp3_get_frame_count(FILE *infile, struct mp3_frameinfo *pfi)
{
	int pos;
	int frames = 0;
	unsigned char frame_buffer[4];
	struct mp3_frameinfo fi;
	off_t file_size;
	int err = 0;
	int cbr = 1;
	int last_bitrate = 0;

	fseek(infile, 0, SEEK_END);
	file_size = ftell(infile);

	pos = pfi->frame_offset;

	while(1)
	{
		err = 1;

		fseek(infile, pos, SEEK_SET);
		if(fread(frame_buffer, 1, sizeof(frame_buffer), infile) == sizeof(frame_buffer))
		{
			// valid frame?
			if(!_decode_mp3_frame(frame_buffer, &fi))
			{
				frames++;
				pos += fi.frame_length;
				err = 0;

				if((last_bitrate) && (fi.bitrate != last_bitrate))
					cbr = 0;
				last_bitrate = fi.bitrate;

				// no sense to scan cbr
				if(cbr && (frames > 100))
				{
					DPRINTF(E_DEBUG, L_SCANNER, "File appears to be CBR... quitting frame _mp3_get_frame_count()\n");
					return;
				}
			}
		}

		if(err)
		{
			if(pos > (file_size - 4096))
			{
				pfi->number_of_frames = frames;
				return;
			}
			else
			{
				DPRINTF(E_ERROR, L_SCANNER, "Frame count aborted on error.  Pos=%d, Count=%d\n",
					pos, frames);
				return;
			}
		}
	}
}

// _get_mp3fileinfo
static int
_get_mp3fileinfo(char *file, struct song_metadata *psong)
{
	FILE *infile;
	struct id3header *pid3;
	struct mp3_frameinfo fi;
	unsigned int size = 0;
	unsigned int n_read;
	off_t fp_size = 0;
	off_t file_size;
	unsigned char buffer[1024];
	int index;

	int xing_flags;
	int found;

	int first_check = 0;
	char frame_buffer[4];

	char id3v1taghdr[4];

	if(!(infile = fopen(file, "rb")))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Could not open %s for reading\n", file);
		return -1;
	}

	memset((void*)&fi, 0, sizeof(fi));

	fseek(infile, 0, SEEK_END);
	file_size = ftell(infile);
	fseek(infile, 0, SEEK_SET);

	if(fread(buffer, 1, sizeof(buffer), infile) != sizeof(buffer))
	{
		if(ferror(infile))
		{
			DPRINTF(E_ERROR, L_SCANNER, "Error reading: %s [%s]\n", strerror(errno), file);
		}
		else
		{
			DPRINTF(E_WARN, L_SCANNER, "File too small. Probably corrupted. [%s]\n", file);
		}
		fclose(infile);
		return -1;
	}

	pid3 = (struct id3header*)buffer;

	found = 0;
	fp_size = 0;

	if(strncmp((char*)pid3->id, "ID3", 3) == 0)
	{
		char tagversion[16];

		/* found an ID3 header... */
		size = (pid3->size[0] << 21 | pid3->size[1] << 14 |
			pid3->size[2] << 7 | pid3->size[3]);
		fp_size = size + sizeof(struct id3header);
		first_check = 1;

		snprintf(tagversion, sizeof(tagversion), "ID3v2.%d.%d",
			 pid3->version[0], pid3->version[1]);
		psong->tagversion = strdup(tagversion);
	}

	index = 0;

	/* Here we start the brute-force header seeking.  Sure wish there
	 * weren't so many crappy mp3 files out there
	 */

	while(!found)
	{
		fseek(infile, fp_size, SEEK_SET);
		if((n_read = fread(buffer, 1, sizeof(buffer), infile)) < 4)   // at least mp3 frame header size (i.e. 4 bytes)
		{
			fclose(infile);
			return 0;
		}

		index = 0;
		while(!found)
		{
			while((buffer[index] != 0xFF) && (index < (n_read - 50)))
				index++;

			if((first_check) && (index))
			{
				fp_size = 0;
				first_check = 0;
				if(n_read < sizeof(buffer))
				{
					fclose(infile);
					return 0;
				}
				break;
			}

			if(index > (n_read - 50))
			{
				fp_size += index;
				if(n_read < sizeof(buffer))
				{
					fclose(infile);
					return 0;
				}
				break;
			}

			if(!_decode_mp3_frame(&buffer[index], &fi))
			{
				if(!strncasecmp((char*)&buffer[index + fi.xing_offset + 4], "XING", 4))
				{
					/* no need to check further... if there is a xing header there,
					 * this is definately a valid frame */
					found = 1;
					fp_size += index;
				}
				else
				{
					/* No Xing... check for next frame to validate current fram is correct */
					fseek(infile, fp_size + index + fi.frame_length, SEEK_SET);
					if(fread(frame_buffer, 1, sizeof(frame_buffer), infile) == sizeof(frame_buffer))
					{
						if(!_decode_mp3_frame((unsigned char*)frame_buffer, &fi))
						{
							found = 1;
							fp_size += index;
						}
					}
					else
					{
						DPRINTF(E_ERROR, L_SCANNER, "Could not read frame header: %s\n", file);
						fclose(infile);
						return 0;
					}

					if(!found)
					{
						// cannot find second frame. Song may be too short. So assume first frame is valid.
						found = 1;
						fp_size += index;
					}
				}
			}

			if(!found)
			{
				index++;
				if(first_check)
				{
					DPRINTF(E_INFO, L_SCANNER, "Bad header... dropping back for full frame search [%s]\n", psong->path);
					first_check = 0;
					fp_size = 0;
					break;
				}
			}
		}
	}

	fi.frame_offset = fp_size;

	psong->audio_offset = fp_size;
	psong->audio_size = file_size - fp_size;
	// check if last 128 bytes is ID3v1.0 ID3v1.1 tag
	fseek(infile, file_size - 128, SEEK_SET);
	if(fread(id3v1taghdr, 1, 4, infile) == 4)
	{
		if(id3v1taghdr[0] == 'T' && id3v1taghdr[1] == 'A' && id3v1taghdr[2] == 'G')
		{
			psong->audio_size -= 128;
		}
	}

	if(_decode_mp3_frame(&buffer[index], &fi))
	{
		fclose(infile);
		DPRINTF(E_ERROR, L_SCANNER, "Could not find sync frame: %s\n", file);
		return 0;
	}

	/* now check for an XING header */
	psong->vbr_scale = -1;
	if(!strncasecmp((char*)&buffer[index + fi.xing_offset + 4], "XING", 4))
	{
		xing_flags = buffer[index+fi.xing_offset+4+4] << 24 |
		             buffer[index+fi.xing_offset+4+5] << 16 |
		             buffer[index+fi.xing_offset+4+6] << 8 |
		             buffer[index+fi.xing_offset+4+7];
		psong->vbr_scale = 78;

		if(xing_flags & 0x1)
		{
			/* Frames field is valid... */
			fi.number_of_frames = buffer[index+fi.xing_offset+4+8] << 24 |
			                      buffer[index+fi.xing_offset+4+9] << 16 |
			                      buffer[index+fi.xing_offset+4+10] << 8 |
			                      buffer[index+fi.xing_offset+4+11];
		}
	}

	if((fi.number_of_frames == 0) && (!psong->song_length))
	{
		_mp3_get_average_bitrate(infile, &fi, file);
	}

	psong->bitrate = fi.bitrate * 1000;
	psong->samplerate = fi.samplerate;

	if(!psong->song_length)
	{
		if(fi.number_of_frames)
		{
			psong->song_length = (int)((double)(fi.number_of_frames * fi.samples_per_frame * 1000.) /
						   (double)fi.samplerate);
			psong->vbr_scale = 78;
		}
		else
		{
			psong->song_length = (int)((double)(file_size - fp_size) * 8. /
						   (double)fi.bitrate);
		}
	}
	psong->channels = fi.stereo ? 2 : 1;

	fclose(infile);
	//DEBUG DPRINTF(E_INFO, L_SCANNER, "Got fileinfo successfully for file=%s song_length=%d\n", file, psong->song_length);

	psong->blockalignment = 1;
	xasprintf(&(psong->dlna_pn), "MP3");

	return 0;
}
