/* 
   Unix SMB/Netbios implementation.
   Version 2.0.x.
   Create unicode map files from unicode_def.XXX files.

   Copyright (C) Jeremy Allison 1997-1999.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

static char *prog_name = NULL;

/*
 * Print program usage and die.
 */

static void unicode_map_usage(char *progname)
{
  fprintf(stderr, "Usage is : %s <codepage> <inputfile> <outputfile>\n",
         progname);
  exit(1);
}

/*
 * Read a line from a buffer into a line buffer. Ensure null
 * terminated.
 */

static void read_line( char **buf, char *line_buf, size_t size)
{
  char *p = *buf;
  size_t num = 0;

  for(; *p && (*p != '\n') && (*p != '\032'); p++) {
    if(num < (size - 1))
      line_buf[num++] = *p;
  }
  if(*p)
    p++; /* Go past the '\n' */
  line_buf[num] = '\0';
  *buf = p;
}

/*
 * Strip comment lines and blank lines from the data.
 * Copies into a new buffer and frees the old.
 * Returns the number of lines copied.
 */

static size_t clean_data( char **buf, size_t *size)
{
  pstring linebuf;
  char *p = *buf;
  size_t num_lines = 0;
  char *newbuf = (char *)malloc( *size + 1);
  char *newbuf_p = NULL;

  if(newbuf == NULL) {
    fprintf(stderr, "%s: malloc fail for size %u.\n", prog_name, (unsigned int)(*size + 1));
    exit(1);
  }

  newbuf_p = newbuf;
  *newbuf_p = '\0';

  while( *p ) {
    char *cp;

    read_line( &p, linebuf, sizeof(linebuf));
    /* Null terminate after comment. */
    if((cp = strchr( linebuf, '#'))!= NULL)
      *cp = '\0';

    for(cp = linebuf;*cp && isspace(*cp); cp++)
      ;

    if(*cp == '\0')
      continue;

    safe_strcpy(newbuf_p, cp, *size - (newbuf_p - newbuf));
    num_lines++;
    newbuf_p += (strlen(newbuf_p) + 1);
  }

  free(*buf);
  *buf = newbuf;
  return num_lines;
}

/*
 * Parse a uint16 from a codepage file.
 */

static BOOL parse_uint16(char *buf, uint16 *uip)
{
  unsigned int ui;
  char *endptr = NULL;

  ui = (unsigned int)strtol(buf, &endptr, 0);
  if(endptr == buf || ui > 65535)
    return False;

  *uip = (uint16)ui;
  return True;
}

/*
 * Print a parse error and exit.
 */

static void parse_error(const char *buf, const char *input_file, const char *msg)
{
  fprintf(stderr, "%s: In file %s : %s whilst parsing line \n%s\n", prog_name,
          input_file, msg, buf);
  exit(1);
}
    
/*
 * Create a compiled unicode map file from a unicode map definition file.
 */

static int do_compile(const char *codepage, const char *input_file, const char *output_file)
{
  FILE *fp = NULL;
  size_t size = 0;
  size_t offset = 0;
  char *buf = NULL;
  char *output_buf = NULL;
  uint16 cp_to_ucs2[65536];
  uint16 ucs2_to_cp[65536];
  BOOL multibyte_code_page = False;
  int num_lines = 0;
  int i = 0;
  SMB_STRUCT_STAT st;

  /* Get the size of the input file. Read the entire thing into memory. */
  if(sys_stat((char *)input_file, &st)!= 0) {
    fprintf(stderr, "%s: failed to get the file size for file %s. Error was %s\n",
            prog_name, input_file, strerror(errno));
    exit(1);
  }

  size = (size_t)st.st_size;

  if((fp = sys_fopen(input_file, "r")) == NULL) {
    fprintf(stderr, "%s: cannot open file %s for input.\n", prog_name, input_file);
    exit(1);
  }

  /* As we will be reading text, allocate one more byte for a '\0' */
  if((buf = (char *)malloc( size + 1 )) == NULL) {
    fprintf(stderr, "%s: malloc fail for size %d.\n", prog_name, size + 1);
    fclose(fp);
    exit(1);
  }

  if(fread( buf, 1, size, fp) != size) {
    fprintf(stderr, "%s: read failed for file %s. Error was %s.\n", prog_name,
            input_file, strerror(errno));
    free((char *)buf);
    fclose(fp);
    exit(1);
  }

  /* Null terminate the text read. */
  buf[size] = '\0';

  /* Go through the data line by line, strip out comments (anything
     after a '#' to end-of-line) and blank lines. The rest should be
     the codepage data.
   */

  num_lines = clean_data( &buf, &size);

  /*
   * Initialize the output data.
   */

  memset(cp_to_ucs2, '\0', sizeof(cp_to_ucs2));
  ucs2_to_cp[0] = 0;
  for (i = 1; i < 65536; i++)
    ucs2_to_cp[i] = (uint16)'_';

  /* Now convert the lines into the compiled form. */

  for(i = 0; i < num_lines; i++) {
    char token_buf[512];
    char *p = buf;
    uint16 cp = 0;
    uint16 ucs2 = 0;

    /* Get the codepage value. */
    if(!next_token(&p, token_buf, NULL, sizeof(token_buf)))
      parse_error(buf, input_file, "cannot parse first value");

    if(!parse_uint16( token_buf, &cp))
      parse_error(buf, input_file, "first value doesn't resolve to an unsigned 16 bit integer");

    if(cp > 255)
      multibyte_code_page = True;

    /* Get the ucs2 value. */

    if(!next_token(&p, token_buf, NULL, sizeof(token_buf))) {

      /*
       * Some of the multibyte codepage to unicode map files
       * list a single byte as a leading multibyte and have no
       * second value.
       */

      buf += (strlen(buf) + 1);
      continue;
    }

    if(!parse_uint16( token_buf, &ucs2))
      parse_error(buf, input_file, "second value doesn't resolve to an unsigned 16 bit integer");

    /*
     * Set up the cross reference in little-endian format.
     */

    SSVAL(((char *)&cp_to_ucs2[cp]),0,ucs2);
    SSVAL(((char *)&ucs2_to_cp[ucs2]),0,cp);

    /*
     * Next line.
     */
    buf += (strlen(buf) + 1);
  }

  size = UNICODE_MAP_HEADER_SIZE + (multibyte_code_page ? (4*65536) : (2*256 + 2*65536));

  if((output_buf = (char *)malloc( size )) == NULL) {
    fprintf(stderr, "%s: output buffer malloc fail for size %d.\n", prog_name, size);
    fclose(fp);
    exit(1);
  }

  /* Setup the output file header. */
  SSVAL(output_buf,UNICODE_MAP_VERSION_OFFSET,UNICODE_MAP_FILE_VERSION_ID);
  memset(&output_buf[UNICODE_MAP_CLIENT_CODEPAGE_OFFSET],'\0',UNICODE_MAP_CODEPAGE_ID_SIZE);
  safe_strcpy(&output_buf[UNICODE_MAP_CLIENT_CODEPAGE_OFFSET], codepage, UNICODE_MAP_CODEPAGE_ID_SIZE - 1);
  output_buf[UNICODE_MAP_CLIENT_CODEPAGE_OFFSET+UNICODE_MAP_CODEPAGE_ID_SIZE-1] = '\0';

  offset = UNICODE_MAP_HEADER_SIZE;

  if (multibyte_code_page) {
    SIVAL(output_buf,UNICODE_MAP_CP_TO_UNICODE_LENGTH_OFFSET,2*65536);
    memcpy(output_buf+offset, (char *)cp_to_ucs2, 2*65536);
    offset += 2*65536;
  } else {
    SIVAL(output_buf,UNICODE_MAP_CP_TO_UNICODE_LENGTH_OFFSET,2*256);
    memcpy(output_buf+offset, (char *)cp_to_ucs2, 2*256);
    offset += 2*256;
  }
  SIVAL(output_buf,UNICODE_MAP_UNICODE_TO_CP_LENGTH_OFFSET,65536*2);
  memcpy(output_buf+offset, (char *)ucs2_to_cp, 2*65536);

  /* Now write out the output_buf. */
  if((fp = sys_fopen(output_file, "w"))==NULL) {
    fprintf(stderr, "%s: Cannot open output file %s. Error was %s.\n",
            prog_name, output_file, strerror(errno));
    exit(1);
  }

  if(fwrite(output_buf, 1, size, fp) != size) {
    fprintf(stderr, "%s: Cannot write output file %s. Error was %s.\n",
            prog_name, output_file, strerror(errno));
    exit(1);
  }  

  fclose(fp);

  return 0;
}

int main(int argc, char **argv)
{
  const char *codepage = NULL;
  char *input_file = NULL;
  char *output_file = NULL;

  prog_name = argv[0];

  if(argc != 4)
    unicode_map_usage(prog_name);

  codepage = argv[1];
  input_file = argv[2];
  output_file = argv[3];

  return do_compile( codepage, input_file, output_file);
}
