/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Create codepage files from codepage_def.XXX files.

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

static void codepage_usage(char *progname)
{
  fprintf(stderr, "Usage is : %s [c|d] <codepage> <inputfile> <outputfile>\n",
         progname);
  exit(1);
}

/*
 * Read a line from a buffer into a line buffer. Ensure null
 * terminated.
 */

static void read_line( char **buf, char *line_buf, int size)
{
  char *p = *buf;
  int num = 0;

  for(; *p && (*p != '\n'); p++)
  {
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

static int clean_data( char **buf, size_t *size)
{
  pstring linebuf;
  char *p = *buf;
  int num_lines = 0;
  char *newbuf = (char *)malloc( *size + 1);
  char *newbuf_p = NULL;

  if(newbuf == NULL)
  {
    fprintf(stderr, "%s: malloc fail for size %d.\n", prog_name, *size + 1);
    exit(1);
  }

  newbuf_p = newbuf;
  *newbuf_p = '\0';

  while( *p )
  {
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
 * Parse a byte from a codepage file.
 */

static BOOL parse_byte(char *buf, unsigned char *bp)
{
  unsigned int b;
  char *endptr = NULL;

  b = (unsigned int)strtol(buf, &endptr, 0);
  if(endptr == buf || b > 255)
    return False;

  *bp = (unsigned char)b;
  return True;
}

/*
 * Parse a bool from a codepage file.
 */

static BOOL parse_bool(char *buf, unsigned char *bp)
{
  if(isdigit((int)*buf))
  {
    char *endptr = NULL;

    *bp = (unsigned char)strtol(buf, &endptr, 0);
    if(endptr == buf )
      return False;
    if(*bp != 0)
      *bp = 1;
  } else {
    if(strcasecmp(buf, "True") && strcasecmp(buf, "False"))
      return False;
    if(strcasecmp(buf, "True")==0)
      *bp = 1;
    else
      *bp = 0;
  }
  return True;
}

/*
 * Print a parse error and exit.
 */

static void parse_error(char *buf, char *msg)
{
  fprintf(stderr, "%s: %s whilst parsing line \n%s\n", prog_name,
          msg, buf);
  exit(1);
}
    
/*
 * Create a compiled codepage file from a codepage definition file.
 */

static int do_compile(int codepage, char *input_file, char *output_file)
{
  FILE *fp = NULL;
  size_t size = 0;
  char *buf = NULL;
  char output_buf[CODEPAGE_HEADER_SIZE + 4 * MAXCODEPAGELINES];
  int num_lines = 0;
  int i = 0;
  SMB_STRUCT_STAT st;

  /* Get the size of the input file. Read the entire thing into memory. */
  if(sys_stat((char *)input_file, &st)!= 0)
  {
    fprintf(stderr, "%s: failed to get the file size for file %s. Error was %s\n",
            prog_name, input_file, strerror(errno));
    exit(1);
  }

  size = (uint32)st.st_size;

  /* I don't believe these things should be bigger than 100k :-) */
  if(size > 100*1024)
  {
    fprintf(stderr, "%s: filesize %d is too large for a codepage definition file. \
The maximum size I will believe is 100k.\n", prog_name, size);
    exit(1);
  }

  if((fp = sys_fopen(input_file, "r")) == NULL)
  {
    fprintf(stderr, "%s: cannot open file %s for input.\n", prog_name, input_file);
    exit(1);
  }

  /* As we will be reading text, allocate one more byte for a '\0' */
  if((buf = (char *)malloc( size + 1 )) == NULL)
  {
    fprintf(stderr, "%s: malloc fail for size %d.\n", prog_name, size + 1);
    fclose(fp);
    exit(1);
  }

  if(fread( buf, 1, size, fp) != size)
  {
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

  /* There can be a maximum of MAXCODEPAGELINES lines. */
  if(num_lines > MAXCODEPAGELINES)
  {
    fprintf(stderr, "%s: There can be a maximum %d lines of data in a codepage \
definition file. File %s has %d.\n", prog_name, MAXCODEPAGELINES, input_file, num_lines);
    exit(1);
  }

  /* Setup the output file header. */
  SSVAL(output_buf,CODEPAGE_VERSION_OFFSET,CODEPAGE_FILE_VERSION_ID);
  SSVAL(output_buf,CODEPAGE_CLIENT_CODEPAGE_OFFSET,(uint16)codepage);
  SIVAL(output_buf,CODEPAGE_LENGTH_OFFSET,(num_lines * 4));

  /* Now convert the lines into the compiled form. */
  for(i = 0; i < num_lines; i++)
  {
    char token_buf[512];
    char *p = buf;
    unsigned char b = 0;

    /* Get the 'lower' value. */
    if(!next_token(&p, token_buf, NULL, sizeof(token_buf)))
      parse_error(buf, "cannot parse first value");
    if(!parse_byte( token_buf, &b))
      parse_error(buf, "first value doesn't resolve to a byte");

    /* Add this to the output buffer. */
    SCVAL(output_buf,CODEPAGE_HEADER_SIZE+(i*4),b);

    /* Get the 'upper' value. */
    if(!next_token(&p, token_buf, NULL, sizeof(token_buf)))
      parse_error(buf, "cannot parse second value");
    if(!parse_byte( token_buf, &b))
      parse_error(buf, "second value doesn't resolve to a byte");

    /* Add this to the output buffer. */
    SCVAL(output_buf,CODEPAGE_HEADER_SIZE+(i*4) + 1,b);

    /* Get the 'upper to lower' value. */
    if(!next_token(&p, token_buf, NULL, sizeof(token_buf)))
      parse_error(buf, "cannot parse third value");
    if(!parse_bool( token_buf, &b))
      parse_error(buf, "third value doesn't resolve to a boolean");

    /* Add this to the output buffer. */
    SCVAL(output_buf,CODEPAGE_HEADER_SIZE+(i*4) + 2,b);

    /* Get the 'lower to upper' value. */
    if(!next_token(&p, token_buf, NULL, sizeof(token_buf)))
      parse_error(buf, "cannot parse fourth value");
    if(!parse_bool( token_buf, &b))
      parse_error(buf, "fourth value doesn't resolve to a boolean");

    /* Add this to the output buffer. */
    SCVAL(output_buf,CODEPAGE_HEADER_SIZE+(i*4) + 3,b);

    buf += (strlen(buf) + 1);
  }

  /* Now write out the output_buf. */
  if((fp = sys_fopen(output_file, "w"))==NULL)
  {
    fprintf(stderr, "%s: Cannot open output file %s. Error was %s.\n",
            prog_name, output_file, strerror(errno));
    exit(1);
  }

  if(fwrite(output_buf, 1, CODEPAGE_HEADER_SIZE + (num_lines*4), fp) !=
         CODEPAGE_HEADER_SIZE + (num_lines*4))
  {
    fprintf(stderr, "%s: Cannot write output file %s. Error was %s.\n",
            prog_name, output_file, strerror(errno));
    exit(1);
  }  

  fclose(fp);

  return 0;
}

/*
 * Placeholder for now.
 */

static int do_decompile( int codepage, char *input_file, char *output_file)
{
  size_t size = 0;
  SMB_STRUCT_STAT st;
  char header_buf[CODEPAGE_HEADER_SIZE];
  char *buf = NULL;
  FILE *fp = NULL;
  int num_lines = 0;
  int i = 0;

  /* Get the size of the input file. Read the entire thing into memory. */
  if(sys_stat((char *)input_file, &st)!= 0)
  {
    fprintf(stderr, "%s: failed to get the file size for file %s. Error was %s\n",
            prog_name, input_file, strerror(errno));
    exit(1);
  }

  size = (size_t)st.st_size;

  if( size < CODEPAGE_HEADER_SIZE || size > (CODEPAGE_HEADER_SIZE + 256))
  { 
    fprintf(stderr, "%s: file %s is an incorrect size for a \
code page file.\n", prog_name, input_file);
    exit(1);
  } 
  
  /* Read the first 8 bytes of the codepage file - check
     the version number and code page number. All the data
     is held in little endian format.
   */
    
  if((fp = sys_fopen( input_file, "r")) == NULL)
  { 
    fprintf(stderr, "%s: cannot open file %s. Error was %s\n",
              prog_name, input_file, strerror(errno));
    exit(1);
  } 
  
  if(fread( header_buf, 1, CODEPAGE_HEADER_SIZE, fp)!=CODEPAGE_HEADER_SIZE)
  { 
    fprintf(stderr, "%s: cannot read header from file %s. Error was %s\n",
              prog_name, input_file, strerror(errno));
    exit(1);
  } 
  
  /* Check the version value */
  if(SVAL(header_buf,CODEPAGE_VERSION_OFFSET) != CODEPAGE_FILE_VERSION_ID)
  { 
    fprintf(stderr, "%s: filename %s has incorrect version id. \
Needed %hu, got %hu.\n",
          prog_name, input_file, (uint16)CODEPAGE_FILE_VERSION_ID, 
          SVAL(header_buf,CODEPAGE_VERSION_OFFSET));
    exit(1);
  } 
  
  /* Check the codepage matches */
  if(SVAL(header_buf,CODEPAGE_CLIENT_CODEPAGE_OFFSET) != (uint16)codepage)
  { 
    fprintf(stderr, "%s: filename %s has incorrect codepage. \
Needed %hu, got %hu.\n",
           prog_name, input_file, (uint16)codepage, 
           SVAL(header_buf,CODEPAGE_CLIENT_CODEPAGE_OFFSET));
    exit(1);
  } 
  
  /* Check the length is correct. */
  if(IVAL(header_buf,CODEPAGE_LENGTH_OFFSET) !=
                 (unsigned int)(size - CODEPAGE_HEADER_SIZE))
  { 
    fprintf(stderr, "%s: filename %s has incorrect size headers. \
Needed %u, got %u.\n", prog_name, input_file, size - CODEPAGE_HEADER_SIZE,
               IVAL(header_buf,CODEPAGE_LENGTH_OFFSET));
    exit(1);
  } 
  
  size -= CODEPAGE_HEADER_SIZE; /* Remove header */
    
  /* Make sure the size is a multiple of 4. */
  if((size % 4 ) != 0)
  { 
    fprintf(stderr, "%s: filename %s has a codepage size not a \
multiple of 4.\n", prog_name, input_file);
    exit(1);
  } 
  
  /* Allocate space for the code page file and read it all in. */
  if((buf = (char *)malloc( size )) == NULL)
  { 
    fprintf (stderr, "%s: malloc fail for size %d.\n",
             prog_name, size );
    exit(1);
  } 
  
  if(fread( buf, 1, size, fp)!=size)
  { 
    fprintf(stderr, "%s: read fail on file %s. Error was %s.\n",
              prog_name, input_file, strerror(errno));
    exit(1);
  } 
  
  fclose(fp);
    
  /* Now dump the codepage into an ascii file. */
  if((fp = sys_fopen(output_file, "w")) == NULL)
  {
    fprintf(stderr, "%s: cannot open file %s. Error was %s\n",
              prog_name, output_file, strerror(errno));
    exit(1);
  } 

  fprintf(fp, "#\n# Codepage definition file for IBM Code Page %d.\n#\n",
          codepage);
  fprintf(fp, "# This file was automatically generated.\n#\n");
  fprintf(fp, "# defines lower->upper mapping.\n");
  fprintf(fp, "#\n#The columns are :\n# lower\tupper\tu-t-l\tl-t-u\n#\n");

  num_lines = size / 4;
  for( i = 0; i < num_lines; i++)
  {
    fprintf(fp, "0x%02X\t0x%02X\t%s\t%s\n", CVAL(buf, (i*4)), CVAL(buf, (i*4)+1),
           CVAL(buf, (i*4)+2) ? "True" : "False",
           CVAL(buf, (i*4)+3) ? "True" : "False");
  }
  fclose(fp);
  return 0;
}

int main(int argc, char **argv)
{
  int codepage = 0;
  char *input_file = NULL;
  char *output_file = NULL;
  BOOL compile = False;

  prog_name = argv[0];

  if(argc != 5)
    codepage_usage(prog_name);

  if(argv[1][0] != 'c' && argv[1][0] != 'C' && argv[1][0] != 'd' &&
     argv[1][0] != 'D')
    codepage_usage(prog_name);

  input_file = argv[3];
  output_file = argv[4];

  /* Are we compiling or decompiling. */
  if(argv[1][0] == 'c' || argv[1][0] == 'C')
    compile = True;
 
  /* Convert the second argument into a client codepage value. */
  if((codepage = atoi(argv[2])) == 0)
  {
    fprintf(stderr, "%s: %s is not a valid codepage.\n", prog_name, argv[2]);
    exit(1);
  }

  if(compile)
    return do_compile( codepage, input_file, output_file);
  else
    return do_decompile( codepage, input_file, output_file);
}
