/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Character set handling
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#define CHARSET_C
#include "includes.h"

extern int DEBUGLEVEL;

/*
 * Codepage definitions.
 */

#if !defined(KANJI)
/* lower->upper mapping for IBM Code Page 850 - MS-DOS Latin 1 */
unsigned char cp_850[][4] = {
/* dec col/row oct hex  description */
/* 133  08/05  205  85  a grave */
/* 183  11/07  267  B7  A grave */ 	{0x85,0xB7,1,1},
/* 160  10/00  240  A0  a acute */
/* 181  11/05  265  B5  A acute */	{0xA0,0xB5,1,1},
/* 131  08/03  203  83  a circumflex */
/* 182  11/06  266  B6  A circumflex */	{0x83,0xB6,1,1},
/* 198  12/06  306  C6  a tilde */
/* 199  12/07  307  C7  A tilde */	{0xC6,0xC7,1,1},
/* 132  08/04  204  84  a diaeresis */
/* 142  08/14  216  8E  A diaeresis */	{0x84,0x8E,1,1},
/* 134  08/06  206  86  a ring */
/* 143  08/15  217  8F  A ring */	{0x86,0x8F,1,1},
/* 145  09/01  221  91  ae diphthong */
/* 146  09/02  222  92  AE diphthong */	{0x91,0x92,1,1},
/* 135  08/07  207  87  c cedilla */
/* 128  08/00  200  80  C cedilla */	{0x87,0x80,1,1},
/* 138  08/10  212  8A  e grave */
/* 212  13/04  324  D4  E grave */	{0x8A,0xD4,1,1},
/* 130  08/02  202  82  e acute */
/* 144  09/00  220  90  E acute */	{0x82,0x90,1,1},
/* 136  08/08  210  88  e circumflex */
/* 210  13/02  322  D2  E circumflex */	{0x88,0xD2,1,1},
/* 137  08/09  211  89  e diaeresis */
/* 211  13/03  323  D3  E diaeresis */	{0x89,0xD3,1,1},
/* 141  08/13  215  8D  i grave */
/* 222  13/14  336  DE  I grave */	{0x8D,0xDE,1,1},
/* 161  10/01  241  A1  i acute */
/* 214  13/06  326  D6  I acute */	{0xA1,0xD6,1,1},
/* 140  08/12  214  8C  i circumflex */
/* 215  13/07  327  D7  I circumflex */	{0x8C,0xD7,1,1},
/* 139  08/11  213  8B  i diaeresis */
/* 216  13/08  330  D8  I diaeresis */	{0x8B,0xD8,1,1},
/* 208  13/00  320  D0  Icelandic eth */
/* 209  13/01  321  D1  Icelandic Eth */ {0xD0,0xD1,1,1},
/* 164  10/04  244  A4  n tilde */
/* 165  10/05  245  A5  N tilde */	{0xA4,0xA5,1,1},
/* 149  09/05  225  95  o grave */
/* 227  14/03  343  E3  O grave */	{0x95,0xE3,1,1},
/* 162  10/02  242  A2  o acute */
/* 224  14/00  340  E0  O acute */	{0xA2,0xE0,1,1},
/* 147  09/03  223  93  o circumflex */
/* 226  14/02  342  E2  O circumflex */	{0x93,0xE2,1,1},
/* 228  14/04  344  E4  o tilde */
/* 229  14/05  345  E5  O tilde */	{0xE4,0xE5,1,1},
/* 148  09/04  224  94  o diaeresis */
/* 153  09/09  231  99  O diaeresis */	{0x94,0x99,1,1},
/* 155  09/11  233  9B  o slash */
/* 157  09/13  235  9D  O slash */	{0x9B,0x9D,1,1},
/* 151  09/07  227  97  u grave */
/* 235  14/11  353  EB  U grave */ 	{0x97,0xEB,1,1},
/* 163  10/03  243  A3  u acute */
/* 233  14/09  351  E9  U acute */	{0xA3,0xE9,1,1},
/* 150  09/06  226  96  u circumflex */
/* 234  14/10  352  EA  U circumflex */ {0x96,0xEA,1,1},
/* 129  08/01  201  81  u diaeresis */
/* 154  09/10  232  9A  U diaeresis */	{0x81,0x9A,1,1},
/* 236  14/12  354  EC  y acute */
/* 237  14/13  355  ED  Y acute */	{0xEC,0xED,1,1},
/* 231  14/07  347  E7  Icelandic thorn */
/* 232  14/08  350  E8  Icelandic Thorn */ {0xE7,0xE8,1,1},
   
  {0x9C,0,0,0},     /* Pound        */
  {0,0,0,0}
};
#else /* KANJI */ 
/* lower->upper mapping for IBM Code Page 932 - MS-DOS Japanese SJIS */
unsigned char cp_932[][4] = {
  {0,0,0,0}
};
#endif /* KANJI */

char xx_dos_char_map[256];
char xx_upper_char_map[256];
char xx_lower_char_map[256];

char *dos_char_map = xx_dos_char_map;
char *upper_char_map = xx_upper_char_map;
char *lower_char_map = xx_lower_char_map;

/*
 * This code has been extended to deal with ascynchronous mappings
 * like MS-DOS Latin US (Code page 437) where things like :
 * a acute are capitalized to 'A', but the reverse mapping
 * must not hold true. This allows the filename case insensitive
 * matching in do_match() to work, as the DOS/Win95/NT client 
 * uses 'A' as a mask to match against characters like a acute.
 * This is the meaning behind the parameters that allow a
 * mapping from lower to upper, but not upper to lower.
 */

static void add_dos_char(int lower, BOOL map_lower_to_upper, 
                         int upper, BOOL map_upper_to_lower)
{
  lower &= 0xff;
  upper &= 0xff;
  DEBUGADD( 6, ( "Adding chars 0x%x 0x%x (l->u = %s) (u->l = %s)\n",
                 lower, upper,
                 map_lower_to_upper ? "True" : "False",
                 map_upper_to_lower ? "True" : "False" ) );
  if (lower) dos_char_map[lower] = 1;
  if (upper) dos_char_map[upper] = 1;
  lower_char_map[lower] = (char)lower; /* Define tolower(lower) */
  upper_char_map[upper] = (char)upper; /* Define toupper(upper) */
  if (lower && upper) {
    if(map_upper_to_lower)
    lower_char_map[upper] = (char)lower;
    if(map_lower_to_upper)
    upper_char_map[lower] = (char)upper;
  }
}

/****************************************************************************
initialise the charset arrays
****************************************************************************/
void charset_initialise(void)
{
  int i;

#ifdef LC_ALL
  /* include <locale.h> in includes.h if available for OS                  */
  /* we take only standard 7-bit ASCII definitions from ctype              */
  setlocale(LC_ALL,"C");
#endif

  for (i= 0;i<=255;i++) {
    dos_char_map[i] = 0;
  }

  for (i=0;i<=127;i++) {
    if (isalnum(i) || strchr("._^$~!#%&-{}()@'`",(char)i))
      add_dos_char(i,False,0,False);
  }

  for (i=0; i<=255; i++) {
    char c = (char)i;
    upper_char_map[i] = lower_char_map[i] = c;

    /* Some systems have buggy isupper/islower for characters
       above 127. Best not to rely on them. */
    if(i < 128) {
      if (isupper((int)c)) lower_char_map[i] = tolower(c);
      if (islower((int)c)) upper_char_map[i] = toupper(c);
    }
  }
}

/****************************************************************************
load the client codepage.
****************************************************************************/

typedef unsigned char (*codepage_p)[4];

static codepage_p load_client_codepage( int client_codepage )
{
  pstring codepage_file_name;
  unsigned char buf[8];
  FILE *fp = NULL;
  SMB_OFF_T size;
  codepage_p cp_p = NULL;
  SMB_STRUCT_STAT st;

  DEBUG(5, ("load_client_codepage: loading codepage %d.\n", client_codepage));

  if(strlen(CODEPAGEDIR) + 14 > sizeof(codepage_file_name))
  {
    DEBUG(0,("load_client_codepage: filename too long to load\n"));
    return NULL;
  }

  pstrcpy(codepage_file_name, CODEPAGEDIR);
  pstrcat(codepage_file_name, "/");
  pstrcat(codepage_file_name, "codepage.");
  slprintf(&codepage_file_name[strlen(codepage_file_name)], 
	   sizeof(pstring)-(strlen(codepage_file_name)+1),
	   "%03d",
           client_codepage);

  if(sys_stat(codepage_file_name,&st)!=0)
  {
    DEBUG(0,("load_client_codepage: filename %s does not exist.\n",
              codepage_file_name));
    return NULL;
  }

  /* Check if it is at least big enough to hold the required
     data. Should be 2 byte version, 2 byte codepage, 4 byte length, 
     plus zero or more bytes of data. Note that the data cannot be more
     than 4 * MAXCODEPAGELINES bytes.
   */
  size = st.st_size;

  if( size < CODEPAGE_HEADER_SIZE || size > (CODEPAGE_HEADER_SIZE + 4 * MAXCODEPAGELINES))
  {
    DEBUG(0,("load_client_codepage: file %s is an incorrect size for a \
code page file (size=%d).\n", codepage_file_name, (int)size));
    return NULL;
  }

  /* Read the first 8 bytes of the codepage file - check
     the version number and code page number. All the data
     is held in little endian format.
   */

  if((fp = sys_fopen( codepage_file_name, "r")) == NULL)
  {
    DEBUG(0,("load_client_codepage: cannot open file %s. Error was %s\n",
              codepage_file_name, strerror(errno)));
    return NULL;
  }

  if(fread( buf, 1, CODEPAGE_HEADER_SIZE, fp)!=CODEPAGE_HEADER_SIZE)
  {
    DEBUG(0,("load_client_codepage: cannot read header from file %s. Error was %s\n",
              codepage_file_name, strerror(errno)));
    goto clean_and_exit;
  }

  /* Check the version value */
  if(SVAL(buf,CODEPAGE_VERSION_OFFSET) != CODEPAGE_FILE_VERSION_ID)
  {
    DEBUG(0,("load_client_codepage: filename %s has incorrect version id. \
Needed %hu, got %hu.\n", 
          codepage_file_name, (uint16)CODEPAGE_FILE_VERSION_ID, 
          SVAL(buf,CODEPAGE_VERSION_OFFSET)));
    goto clean_and_exit;
  }

  /* Check the codepage matches */
  if(SVAL(buf,CODEPAGE_CLIENT_CODEPAGE_OFFSET) != (uint16)client_codepage)
  {
    DEBUG(0,("load_client_codepage: filename %s has incorrect codepage. \
Needed %hu, got %hu.\n", 
           codepage_file_name, (uint16)client_codepage, 
           SVAL(buf,CODEPAGE_CLIENT_CODEPAGE_OFFSET)));
    goto clean_and_exit;
  }

  /* Check the length is correct. */
  if(IVAL(buf,CODEPAGE_LENGTH_OFFSET) != (size - CODEPAGE_HEADER_SIZE))
  {
    DEBUG(0,("load_client_codepage: filename %s has incorrect size headers. \
Needed %u, got %u.\n", codepage_file_name, (uint32)(size - CODEPAGE_HEADER_SIZE), 
               IVAL(buf,CODEPAGE_LENGTH_OFFSET)));
    goto clean_and_exit;
  }

  size -= CODEPAGE_HEADER_SIZE; /* Remove header */

  /* Make sure the size is a multiple of 4. */
  if((size % 4 ) != 0)
  {
    DEBUG(0,("load_client_codepage: filename %s has a codepage size not a \
multiple of 4.\n", codepage_file_name));
    goto clean_and_exit;
  }

  /* Allocate space for the code page file and read it all in. */
  if((cp_p = (codepage_p)malloc( size  + 4 )) == NULL)
  {
    DEBUG(0,("load_client_codepage: malloc fail.\n"));
    goto clean_and_exit;
  }

  if(fread( (char *)cp_p, 1, size, fp)!=size)
  {
    DEBUG(0,("load_client_codepage: read fail on file %s. Error was %s.\n",
              codepage_file_name, strerror(errno)));
    goto clean_and_exit;
  }

  /* Ensure array is correctly terminated. */
  memset(((char *)cp_p) + size, '\0', 4);

  fclose(fp);
  return cp_p;

clean_and_exit:

  /* pseudo destructor :-) */

  if(fp != NULL)
    fclose(fp);
  if(cp_p)
    free((char *)cp_p);
  return NULL;
}

/****************************************************************************
 Initialise the client codepage.
****************************************************************************/

void codepage_initialise(int client_codepage)
{
  int i;
  static codepage_p cp = NULL;

  if(cp != NULL)
  {
    DEBUG(6,
      ("codepage_initialise: called twice - ignoring second client code page = %d\n",
      client_codepage));
    return;
  }

  DEBUG(6,("codepage_initialise: client code page = %d\n", client_codepage));

  /*
   * Known client codepages - these can be added to.
   */
  cp = load_client_codepage( client_codepage );

  if(cp == NULL)
  {
#ifdef KANJI
    DEBUG(6,("codepage_initialise: loading dynamic codepage file %s/codepage.%d \
for code page %d failed. Using default client codepage 932\n", 
             CODEPAGEDIR, client_codepage, client_codepage));
    cp = cp_932;
    client_codepage = KANJI_CODEPAGE;
#else /* KANJI */
    DEBUG(6,("codepage_initialise: loading dynamic codepage file %s/codepage.%d \
for code page %d failed. Using default client codepage 850\n", 
             CODEPAGEDIR, client_codepage, client_codepage));
    cp = cp_850;
    client_codepage = MSDOS_LATIN_1_CODEPAGE;
#endif /* KANJI */
  }

  /*
   * Setup the function pointers for the loaded codepage.
   */
  initialize_multibyte_vectors( client_codepage );

  if(cp)
  {
    for(i = 0; !((cp[i][0] == '\0') && (cp[i][1] == '\0')); i++)
      add_dos_char(cp[i][0], (BOOL)cp[i][2], cp[i][1], (BOOL)cp[i][3]);
  }

  /* Try and load the unicode map. */
  load_dos_unicode_map(client_codepage);
}

/*******************************************************************
add characters depending on a string passed by the user
********************************************************************/
void add_char_string(char *s)
{
  char *extra_chars = (char *)strdup(s);
  char *t;
  if (!extra_chars) return;

  for (t=strtok(extra_chars," \t\r\n"); t; t=strtok(NULL," \t\r\n")) {
    char c1=0,c2=0;
    int i1=0,i2=0;
    if (isdigit((unsigned char)*t) || (*t)=='-') {
      sscanf(t,"%i:%i",&i1,&i2);
      add_dos_char(i1,True,i2,True);
    } else {
      sscanf(t,"%c:%c",&c1,&c2);
      add_dos_char((unsigned char)c1,True,(unsigned char)c2, True);
    }
  }

  free(extra_chars);
}
