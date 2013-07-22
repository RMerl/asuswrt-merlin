//=========================================================================
// FILENAME	: tagutils-wav.c
// DESCRIPTION	: WAV metadata reader
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

#define GET_WAV_INT32(p) ((((uint8_t)((p)[3])) << 24) |   \
			  (((uint8_t)((p)[2])) << 16) |   \
			  (((uint8_t)((p)[1])) << 8) |	   \
			  (((uint8_t)((p)[0]))))

#define GET_WAV_INT16(p) ((((uint8_t)((p)[1])) << 8) |	   \
			  (((uint8_t)((p)[0]))))

static int
_get_wavtags(char *filename, struct song_metadata *psong)
{
	int fd;
	uint32_t len;
	unsigned char hdr[12];
	unsigned char fmt[16];
	//uint32_t chunk_data_length;
	uint32_t format_data_length = 0;
	uint32_t compression_code = 0;
	uint32_t channel_count = 0;
	uint32_t sample_rate = 0;
	uint32_t sample_bit_length = 0;
	uint32_t bit_rate;
	uint32_t data_length = 0;
	uint32_t sec, ms;

	uint32_t current_offset;
	uint32_t block_len;

	//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Getting WAV file info\n");

	if(!(fd = open(filename, O_RDONLY)))
	{
		DPRINTF(E_WARN, L_SCANNER, "Could not create file handle\n");
		return -1;
	}

	len = 12;
	if(!(len = read(fd, hdr, len)) || (len != 12))
	{
		DPRINTF(E_WARN, L_SCANNER, "Could not read wav header from %s\n", filename);
		close(fd);
		return -1;
	}

	/* I've found some wav files that have INFO tags
	 * in them... */
	if(strncmp((char*)hdr + 0, "RIFF", 4) ||
	   strncmp((char*)hdr + 8, "WAVE", 4))
	{
		DPRINTF(E_WARN, L_SCANNER, "Invalid wav header in %s\n", filename);
		close(fd);
		return -1;
	}

	//chunk_data_length = GET_WAV_INT32(hdr + 4);

	/* now, walk through the chunks */
	current_offset = 12;
	while(current_offset + 8 < psong->file_size)
	{
		len = 8;
		if(!(len = read(fd, hdr, len)) || (len != 8))
		{
			close(fd);
			DPRINTF(E_WARN, L_SCANNER, "Error reading block: %s\n", filename);
			return -1;
		}

		current_offset += 8;
		block_len = GET_WAV_INT32(hdr + 4);

		//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "Read block %02x%02x%02x%02x (%c%c%c%c) of "
		//        "size 0x%08x\n",hdr[0],hdr[1],hdr[2],hdr[3],
		//        isprint(hdr[0]) ? hdr[0] : '?',
		//        isprint(hdr[1]) ? hdr[1] : '?',
		//        isprint(hdr[2]) ? hdr[2] : '?',
		//        isprint(hdr[3]) ? hdr[3] : '?',
		//        block_len);

		if(block_len < 0)
		{
			close(fd);
			DPRINTF(E_WARN, L_SCANNER, "Bad block len: %s\n", filename);
			return -1;
		}

		if(strncmp((char*)&hdr, "fmt ", 4) == 0)
		{
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Found 'fmt ' header\n");
			len = 16;
			if(!read(fd, fmt, len) || (len != 16))
			{
				close(fd);
				DPRINTF(E_WARN, L_SCANNER, "Bad .wav file: can't read fmt: %s\n",
					filename);
				return -1;
			}

			format_data_length = block_len;
			compression_code = GET_WAV_INT16(fmt);
			channel_count = GET_WAV_INT16(fmt + 2);
			sample_rate = GET_WAV_INT32(fmt + 4);
			sample_bit_length = GET_WAV_INT16(fmt + 14);
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Compression code: %d\n",compression_code);
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Channel count:    %d\n",channel_count);
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Sample Rate:      %d\n",sample_rate);
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Sample bit length %d\n",sample_bit_length);

		}
		else if(strncmp((char*)&hdr, "data", 4) == 0)
		{
			//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Found 'data' header\n");
			data_length = block_len;
			goto next_block;
		}
		else if(strncmp((char*)&hdr, "LIST", 4) == 0)
		{
			char *tags;
			char *p;
			int off;
			uint32_t taglen;
			char **m;

			len = GET_WAV_INT32(hdr + 4);
			if(len > 65536 || len < 9)
				goto next_block;

			tags = malloc(len+1);
			if(!tags)
				goto next_block;

			if(read(fd, tags, len) < len ||
			   strncmp(tags, "INFO", 4) != 0)
			{
				free(tags);
				goto next_block;
			}
			tags[len] = '\0';

			off = 4;
			p = tags + off;
			while(off < len - 8)
			{
				taglen = GET_WAV_INT32(p + 4);

				//DEBUG DPRINTF(E_DEBUG, L_SCANNER, "%.*s: %.*s (%d)\n", 4, p, taglen, p + 8, taglen);
				m = NULL;
				if (taglen > 2048) {
					DPRINTF(E_WARN, L_SCANNER, "Ignoring long tag [%.*s] in %s\n",
				                4, p+8, filename);
				}
				else if(strncmp(p, "INAM", 4) == 0)
					m = &(psong->title);
				else if(strncmp(p, "IALB", 4) == 0 ||
				        strncmp(p, "IPRD", 4) == 0)
					m = &(psong->album);
				else if(strncmp(p, "IGRE", 4) == 0 ||
				        strncmp(p, "IGNR", 4) == 0)
					m = &(psong->genre);
				else if(strncmp(p, "ICMT", 4) == 0)
					m = &(psong->comment);
				else if(strncmp(p, "IART", 4) == 0)
					m = &(psong->contributor[ROLE_TRACKARTIST]);
				else if(strncmp(p, "IAAR", 4) == 0)
					m = &(psong->contributor[ROLE_ALBUMARTIST]);
				else if(strncmp(p, "ICOM", 4) == 0 ||
				        strncmp(p, "IMUS", 4) == 0)
					m = &(psong->contributor[ROLE_COMPOSER]);
				else if(strncasecmp(p, "ITRK", 4) == 0)
					psong->track = atoi(p + 8);
				else if(strncmp(p, "ICRD", 4) == 0 ||
				        strncmp(p, "IYER", 4) == 0)
					psong->year = atoi(p + 8);
				if(m)
				{
					*m = malloc(taglen + 1);
					strncpy(*m, p + 8, taglen);
					(*m)[taglen] = '\0';
				}

				p += taglen + 8;
				off += taglen + 8;
				/* Handle some common WAV file malformations */
				while (*p == '\0' && off < len) {
					p++;
					off++;
				}
			}
			free(tags);
		}
next_block:
		lseek(fd, current_offset + block_len, SEEK_SET);
		current_offset += block_len;
	}
	close(fd);

	if(((format_data_length != 16) && (format_data_length != 18)) ||
	   (compression_code != 1) ||
	   (channel_count < 1))
	{
		DPRINTF(E_WARN, L_SCANNER, "Invalid wav header in %s\n", filename);
		return -1;
	}

	if( !data_length )
		data_length = psong->file_size - 44;

	bit_rate = sample_rate * channel_count * ((sample_bit_length + 7) / 8) * 8;
	psong->bitrate = bit_rate;
	psong->samplerate = sample_rate;
	psong->channels = channel_count;
	//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Data length: %d\n", data_length);
	sec = data_length / (bit_rate / 8);
	ms = ((data_length % (bit_rate / 8)) * 1000) / (bit_rate / 8);
	psong->song_length = (sec * 1000) + ms;
	//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Song length: %d\n", psong->song_length);
	//DEBUG DPRINTF(E_DEBUG,L_SCANNER,"Bit rate: %d\n", psong->bitrate);

	/* Upon further review, WAV files should be little-endian, and DLNA requires the LPCM profile to be big-endian.
	asprintf(&(psong->mime), "audio/L16;rate=%d;channels=%d", psong->samplerate, psong->channels);
	*/

	return 0;
}

static int
_get_wavfileinfo(char *filename, struct song_metadata *psong)
{
	psong->lossless = 1;
	/* Upon further review, WAV files should be little-endian, and DLNA requires the LPCM profile to be big-endian.
	asprintf(&(psong->dlna_pn), "LPCM");
	*/

	return 0;
}
