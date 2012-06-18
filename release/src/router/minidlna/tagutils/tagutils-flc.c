//=========================================================================
// FILENAME	: tagutils-flc.c
// DESCRIPTION	: FLAC metadata reader
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

static int
_get_flctags(char *filename, struct song_metadata *psong)
{
	FLAC__Metadata_SimpleIterator *iterator = 0;
	FLAC__StreamMetadata *block;
	unsigned int sec, ms;
	int i;
	int err = 0;

	if(!(iterator = FLAC__metadata_simple_iterator_new()))
	{
		DPRINTF(E_FATAL, L_SCANNER, "Out of memory while FLAC__metadata_simple_iterator_new()\n");
		return -1;
	}

	if(!FLAC__metadata_simple_iterator_init(iterator, filename, true, true))
	{
		DPRINTF(E_ERROR, L_SCANNER, "Cannot extract tag from %s\n", filename);
		return -1;
	}

	do {
		if(!(block = FLAC__metadata_simple_iterator_get_block(iterator)))
		{
			DPRINTF(E_ERROR, L_SCANNER, "Cannot extract tag from %s\n", filename);
			err = -1;
			goto _exit;
		}

		switch(block->type)
		{
		case FLAC__METADATA_TYPE_STREAMINFO:
			sec = (unsigned int)(block->data.stream_info.total_samples /
			                     block->data.stream_info.sample_rate);
			ms = (unsigned int)(((block->data.stream_info.total_samples %
			                      block->data.stream_info.sample_rate) * 1000) /
			                      block->data.stream_info.sample_rate);
			if ((sec == 0) && (ms == 0))
				break; /* Info is crap, escape div-by-zero. */
			psong->song_length = (sec * 1000) + ms;
			psong->bitrate = (((uint64_t)(psong->file_size) * 1000) / (psong->song_length / 8));
			psong->samplerate = block->data.stream_info.sample_rate;
			psong->channels = block->data.stream_info.channels;
			break;

		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			for(i = 0; i < block->data.vorbis_comment.num_comments; i++)
			{
				vc_scan(psong,
					(char*)block->data.vorbis_comment.comments[i].entry,
					block->data.vorbis_comment.comments[i].length);
			}
			break;
#if FLAC_API_VERSION_CURRENT >= 10
		case FLAC__METADATA_TYPE_PICTURE:
			psong->image_size = block->data.picture.data_length;
			if((psong->image = malloc(psong->image_size)))
				memcpy(psong->image, block->data.picture.data, psong->image_size);
			else
				DPRINTF(E_ERROR, L_SCANNER, "Out of memory [%s]\n", filename);
			break;
#endif
		default:
			break;
		}
		FLAC__metadata_object_delete(block);
	}
	while(FLAC__metadata_simple_iterator_next(iterator));

 _exit:
	if(iterator)
		FLAC__metadata_simple_iterator_delete(iterator);

	return err;
}

static int
_get_flcfileinfo(char *filename, struct song_metadata *psong)
{
	psong->lossless = 1;
	psong->vbr_scale = 1;

	return 0;
}
