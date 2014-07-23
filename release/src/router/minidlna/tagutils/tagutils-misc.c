//=========================================================================
// FILENAME	: tagutils-misc.c
// DESCRIPTION	: Misc routines for supporting tagutils
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/* This program is free software; you can redistribute it and/or modify
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

/**************************************************************************
* Language
**************************************************************************/

#define MAX_ICONV_BUF 1024

typedef enum {
	ICONV_OK,
	ICONV_TRYNEXT,
	ICONV_FATAL
} iconv_result;

#ifdef HAVE_ICONV
static iconv_result
do_iconv(const char* to_ces, const char* from_ces,
	 ICONV_CONST char *inbuf,  size_t inbytesleft,
	 char *outbuf_orig, size_t outbytesleft_orig)
{
	size_t rc;
	iconv_result ret = ICONV_OK;

	size_t outbytesleft = outbytesleft_orig - 1;
	char* outbuf = outbuf_orig;

	iconv_t cd  = iconv_open(to_ces, from_ces);

	if(cd == (iconv_t)-1)
	{
		return ICONV_FATAL;
	}
	rc = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if(rc == (size_t)-1)
	{
		if(errno == E2BIG)
		{
			ret = ICONV_FATAL;
		}
		else
		{
			ret = ICONV_TRYNEXT;
			memset(outbuf_orig, '\0', outbytesleft_orig);
		}
	}
	iconv_close(cd);

	return ret;
}
#else // HAVE_ICONV
static iconv_result
do_iconv(const char* to_ces, const char* from_ces,
	 char *inbuf,  size_t inbytesleft,
	 char *outbuf_orig, size_t outbytesleft_orig)
{
	return ICONV_FATAL;
}
#endif // HAVE_ICONV

#define N_LANG_ALT 8
struct {
	char *lang;
	char *cpnames[N_LANG_ALT];
} iconv_map[] = {
	{ "ja_JP",     { "CP932", "CP950", "CP936", "ISO-8859-1", 0 } },
	{ "zh_CN",  { "CP936", "CP950", "CP932", "ISO-8859-1", 0 } },
	{ "zh_TW",  { "CP950", "CP936", "CP932", "ISO-8859-1", 0 } },
	{ "ko_KR",  { "CP949", "ISO-8859-1", 0 } },
	{ 0,        { 0 } }
};
static int lang_index = -1;

static int
_lang2cp(char *lang)
{
	int cp;

	if(!lang || lang[0] == '\0')
		return -1;
	for(cp = 0; iconv_map[cp].lang; cp++)
	{
		if(!strcasecmp(iconv_map[cp].lang, lang))
			return cp;
	}
	return -2;
}

static unsigned char*
_get_utf8_text(const id3_ucs4_t* native_text)
{
	unsigned char *utf8_text = NULL;
	char *in, *in8, *iconv_buf;
	iconv_result rc;
	int i, n;

	in = (char*)id3_ucs4_latin1duplicate(native_text);
	if(!in)
	{
		goto out;
	}

	in8 = (char*)id3_ucs4_utf8duplicate(native_text);
	if(!in8)
	{
		free(in);
		goto out;
	}

	iconv_buf = (char*)calloc(MAX_ICONV_BUF, sizeof(char));
	if(!iconv_buf)
	{
		free(in); free(in8);
		goto out;
	}

	i = lang_index;
	// (1) try utf8 -> default
	rc = do_iconv(iconv_map[i].cpnames[0], "UTF-8", in8, strlen(in8), iconv_buf, MAX_ICONV_BUF);
	if(rc == ICONV_OK)
	{
		utf8_text = (unsigned char*)in8;
		free(iconv_buf);
	}
	else if(rc == ICONV_TRYNEXT)
	{
		// (2) try default -> utf8
		rc = do_iconv("UTF-8", iconv_map[i].cpnames[0], in, strlen(in), iconv_buf, MAX_ICONV_BUF);
		if(rc == ICONV_OK)
		{
			utf8_text = (unsigned char*)iconv_buf;
		}
		else if(rc == ICONV_TRYNEXT)
		{
			// (3) try other encodes
			for(n = 1; n < N_LANG_ALT && iconv_map[i].cpnames[n]; n++)
			{
				rc = do_iconv("UTF-8", iconv_map[i].cpnames[n], in, strlen(in), iconv_buf, MAX_ICONV_BUF);
				if(rc == ICONV_OK)
				{
					utf8_text = (unsigned char*)iconv_buf;
					break;
				}
			}
			if(!utf8_text)
			{
				// cannot iconv
				utf8_text = (unsigned char*)id3_ucs4_utf8duplicate(native_text);
				free(iconv_buf);
			}
		}
		free(in8);
	}
	free(in);

 out:
	if(!utf8_text)
	{
		utf8_text = (unsigned char*)strdup("UNKNOWN");
	}

	return utf8_text;
}


static void
vc_scan(struct song_metadata *psong, const char *comment, const size_t length)
{
	char strbuf[1024];

	if(length > (sizeof(strbuf) - 1))
	{
		if( strncasecmp(comment, "LYRICS=", 7) != 0 )
		{
			const char *eq = strchr(comment, '=');
			int len = 8;
			if (eq)
				len = eq - comment;
			DPRINTF(E_WARN, L_SCANNER, "Vorbis %.*s too long [%s]\n",
				len, comment, psong->path);
		}
		return;
	}
	strncpy(strbuf, comment, length);
	strbuf[length] = '\0';

	// ALBUM, ARTIST, PUBLISHER, COPYRIGHT, DISCNUMBER, ISRC, EAN/UPN, LABEL, LABELNO,
	// LICENSE, OPUS, SOURCEMEDIA, TITLE, TRACKNUMBER, VERSION, ENCODED-BY, ENCODING,
	// -- following tags are muliples
	// COMPOSER, ARRANGER, LYRICIST, AUTHOR, CONDUCTOR, PERFORMER, ENSEMBLE, PART
	// PARTNUMBER, GENRE, DATE, LOCATION, COMMENT
	if(!strncasecmp(strbuf, "ALBUM=", 6))
	{
		if( *(strbuf+6) )
			psong->album = strdup(strbuf + 6);
	}
	else if(!strncasecmp(strbuf, "ARTIST=", 7))
	{
		if( *(strbuf+7) )
			psong->contributor[ROLE_ARTIST] = strdup(strbuf + 7);
	}
	else if(!strncasecmp(strbuf, "ARTISTSORT=", 11))
	{
		psong->contributor_sort[ROLE_ARTIST] = strdup(strbuf + 11);
	}
	else if(!strncasecmp(strbuf, "ALBUMARTIST=", 12))
	{
		if( *(strbuf+12) )
			psong->contributor[ROLE_BAND] = strdup(strbuf + 12);
	}
	else if(!strncasecmp(strbuf, "ALBUMARTISTSORT=", 16))
	{
		psong->contributor_sort[ROLE_BAND] = strdup(strbuf + 16);
	}
	else if(!strncasecmp(strbuf, "TITLE=", 6))
	{
		if( *(strbuf+6) )
			psong->title = strdup(strbuf + 6);
	}
	else if(!strncasecmp(strbuf, "TRACKNUMBER=", 12))
	{
		psong->track = atoi(strbuf + 12);
	}
	else if(!strncasecmp(strbuf, "DISCNUMBER=", 11))
	{
		psong->disc = atoi(strbuf + 11);
	}
	else if(!strncasecmp(strbuf, "GENRE=", 6))
	{
		if( *(strbuf+6) )
			psong->genre = strdup(strbuf + 6);
	}
	else if(!strncasecmp(strbuf, "DATE=", 5))
	{
		if(length >= (5 + 10) &&
		   isdigit(strbuf[5 + 0]) && isdigit(strbuf[5 + 1]) && ispunct(strbuf[5 + 2]) &&
		   isdigit(strbuf[5 + 3]) && isdigit(strbuf[5 + 4]) && ispunct(strbuf[5 + 5]) &&
		   isdigit(strbuf[5 + 6]) && isdigit(strbuf[5 + 7]) && isdigit(strbuf[5 + 8]) && isdigit(strbuf[5 + 9]))
		{
			// nn-nn-yyyy
			strbuf[5 + 10] = '\0';
			psong->year = atoi(strbuf + 5 + 6);
		}
		else
		{
			// year first. year is at most 4 digit.
			strbuf[5 + 4] = '\0';
			psong->year = atoi(strbuf + 5);
		}
	}
	else if(!strncasecmp(strbuf, "COMMENT=", 8))
	{
		if( *(strbuf+8) )
			psong->comment = strdup(strbuf + 8);
	}
	else if(!strncasecmp(strbuf, "MUSICBRAINZ_ALBUMID=", 20))
	{
		psong->musicbrainz_albumid = strdup(strbuf + 20);
	}
	else if(!strncasecmp(strbuf, "MUSICBRAINZ_TRACKID=", 20))
	{
		psong->musicbrainz_trackid = strdup(strbuf + 20);
	}
	else if(!strncasecmp(strbuf, "MUSICBRAINZ_TRACKID=", 20))
	{
		psong->musicbrainz_trackid = strdup(strbuf + 20);
	}
	else if(!strncasecmp(strbuf, "MUSICBRAINZ_ARTISTID=", 21))
	{
		psong->musicbrainz_artistid = strdup(strbuf + 21);
	}
	else if(!strncasecmp(strbuf, "MUSICBRAINZ_ALBUMARTISTID=", 26))
	{
		psong->musicbrainz_albumartistid = strdup(strbuf + 26);
	}
}
