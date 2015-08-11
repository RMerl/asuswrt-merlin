/*
 * $Id: smart-parser.c 1622 2007-07-31 04:34:33Z rpedde $
 *
 * This is really two parts -- the lexer and the parser.  Converting
 * a parse tree back to a format that works with the database backend
 * is left to the db backend.
 *
 * Oh, and this is called "smart-parser" because it parses terms for
 * specifying smart playlists, not because it is particularly smart.  :)
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "daapd.h"
#include "err.h"

#ifdef HAVE_SQL
extern int db_sql_escape(char *buffer, int *size, char *fmt, ...);
#endif


typedef struct tag_token {
    int token_id;
    union {
        char *cvalue;
        int ivalue;
        time_t tvalue;
    } data;
} SP_TOKEN;

typedef struct tag_sp_node {
    union {
        struct tag_sp_node *node;
        char *field;
    } left;

    int op;
    int op_type;
    int not_flag;

    union {
        struct tag_sp_node *node;
        int ivalue;
        char *cvalue;
        time_t tvalue;
    } right;
} SP_NODE;

#define SP_OPTYPE_INVALID 0
#define SP_OPTYPE_ANDOR   1
#define SP_OPTYPE_STRING  2
#define SP_OPTYPE_INT     3
#define SP_OPTYPE_DATE    4

#define SP_HINT_NONE      0
#define SP_HINT_STRING    1
#define SP_HINT_INT       2
#define SP_HINT_DATE      3

/*
#define T_ID            0x00
#define T_PATH          0x01
#define T_TITLE         0x02
#define T_ARTIST        0x03
#define T_ALBUM         0x04
#define T_GENRE         0x05
#define T_COMMENT       0x06
#define T_TYPE          0x07
#define T_COMPOSER      0x08
#define T_ORCHESTRA     0x09
#define T_GROUPING      0x0a
#define T_URL           0x0b
#define T_BITRATE       0x0c
#define T_SAMPLERATE    0x0d
#define T_SONG_LENGTH   0x0e
#define T_FILE_SIZE     0x0f
#define T_YEAR          0x10
#define T_TRACK         0x11
#define T_TOTAL_TRACKS  0x12
#define T_DISC          0x13
#define T_TOTAL_DISCS   0x14
#define T_BPM           0x15
#define T_COMPILATION   0x16
#define T_RATING        0x17
#define T_PLAYCOUNT     0x18
#define T_DATA_KIND     0x19
#define T_ITEM_KIND     0x1a
#define T_DESCRIPTION   0x1b
#define T_TIME_ADDED    0x1c
#define T_TIME_MODIFIED 0x0d
#define T_TIME_PLAYED   0x1d
#define T_TIME_STAMP    0x1e
#define T_DISABLED      0x1f
#define T_SAMPLE_COUNT  0x1e
#define T_FORCE_UPDATE  0x1f
#define T_CODECTYPE     0x20
#define T_IDX           0x21
*/

/**
 * high 4 bits:
 *
 * 0x8000 -
 * 0x4000 -
 * 0x2000 - data is string
 * 0x1000 - data is int
 *
 * 0x0800 -
 * 0x0400 -
 * 0x0200 -
 * 0x0100 -
 */

#define T_STRING        0x2001
#define T_INT_FIELD     0x2002
#define T_STRING_FIELD  0x2003
#define T_DATE_FIELD    0x2004

#define T_OPENPAREN     0x0005
#define T_CLOSEPAREN    0x0006
#define T_LESS          0x0007
#define T_LESSEQUAL     0x0008
#define T_GREATER       0x0009
#define T_GREATEREQUAL  0x000a
#define T_EQUAL         0x000b
#define T_OR            0x000c
#define T_AND           0x000d
#define T_QUOTE         0x000e
#define T_NUMBER        0x000f
#define T_INCLUDES      0x0010
#define T_BEFORE        0x0011
#define T_AFTER         0x0012
#define T_AGO           0x0013
#define T_TODAY         0x0014
#define T_THE           0x0015
#define T_DAY           0x0016
#define T_WEEK          0x0017
#define T_MONTH         0x0018
#define T_YEAR          0x0019
#define T_DATE          0x001a
#define T_NOT           0x001b
#define T_STARTSWITH    0x001c
#define T_ENDSWITH      0x001d
#define T_LAST          0x001e

/* specific to query extensions */
#define T_GREATERAND    0x001f
#define T_EXPRQUOTE     0x0020

#define T_EOF           0x00fd
#define T_BOF           0x00fe
#define T_ERROR         0x00ff

char *sp_token_descr[] = {
    "unknown",
    "literal string",
    "integer field",
    "string field",
    "date field",
    "(",
    ")",
    "<",
    "<=",
    ">",
    ">=",
    "=",
    "or",
    "and",
    "quote",
    "number",
    "like",
    "before",
    "after",
    "ago",
    "today",
    "the",
    "day(s)",
    "week(s)",
    "month(s)",
    "year(s)",
    "date",
    "not",
    "like",
    "like"
};

typedef struct tag_fieldlookup {
    int type;
    char *name;
    char *xlat;
} FIELDLOOKUP;

/* normal terminators, in-string terminators, escapes, quotes */
char *sp_terminators[2][4] = {
    { " \t\n\r\"<>=()|&!", "\"","\"","\"" },
    { "()'+: -,", "'", "\\*'","" }
};

FIELDLOOKUP sp_symbols_0[] = {
    { T_OR, "||", NULL },
    { T_AND, "&&", NULL },
    { T_EQUAL, "=", NULL },
    { T_LESSEQUAL, "<=", NULL },
    { T_LESS, "<", NULL },
    { T_GREATEREQUAL, ">=", NULL },
    { T_GREATER, ">", NULL },
    { T_OPENPAREN, "(", NULL },
    { T_CLOSEPAREN, ")", NULL },
    { T_NOT, "!", NULL },
    { 0, NULL, NULL }
};

FIELDLOOKUP sp_symbols_1[] = {
    { T_OPENPAREN, "(", NULL },
    { T_CLOSEPAREN, ")", NULL },
    { T_EXPRQUOTE, "'", NULL },
    { T_GREATERAND, "+", NULL },
    { T_GREATERAND, " ", NULL },
    { T_LESS,"-", NULL },
    { T_OR, ",", NULL },
    { T_EQUAL, ":", NULL },
    { T_NOT, "!", NULL },
    { 0, NULL, NULL }
};

FIELDLOOKUP *sp_symbols[2] = {
    sp_symbols_0, sp_symbols_1
};

FIELDLOOKUP sp_fields_0[] = {
    { T_INT_FIELD, "id", NULL },
    { T_STRING_FIELD, "path", NULL },
    { T_STRING_FIELD, "fname", NULL },
    { T_STRING_FIELD, "title", NULL },
    { T_STRING_FIELD, "artist", NULL },
    { T_STRING_FIELD, "album", NULL },
    { T_STRING_FIELD, "genre", NULL },
    { T_STRING_FIELD, "comment", NULL },
    { T_STRING_FIELD, "type", NULL },
    { T_STRING_FIELD, "composer", NULL },
    { T_STRING_FIELD, "orchestra", NULL },
    { T_STRING_FIELD, "grouping", NULL },
    { T_STRING_FIELD, "url", NULL },
    { T_INT_FIELD, "bitrate", NULL },
    { T_INT_FIELD, "samplerate", NULL },
    { T_INT_FIELD, "song_length", NULL },
    { T_INT_FIELD, "file_size", NULL },
    { T_INT_FIELD, "year", NULL },
    { T_INT_FIELD, "track", NULL },
    { T_INT_FIELD, "total_tracks", NULL },
    { T_INT_FIELD, "disc", NULL },
    { T_INT_FIELD, "total_discs", NULL },
    { T_INT_FIELD, "bpm", NULL },
    { T_INT_FIELD, "compilation", NULL },
    { T_INT_FIELD, "rating", NULL },
    { T_INT_FIELD, "play_count", NULL },
    { T_INT_FIELD, "data_kind", NULL },
    { T_INT_FIELD, "item_kind", NULL },
    { T_STRING_FIELD, "description", NULL },
    { T_DATE_FIELD, "time_added", NULL },
    { T_DATE_FIELD, "time_modified", NULL },
    { T_DATE_FIELD, "time_played", NULL },
    { T_DATE_FIELD, "db_timestamp", NULL },
    { T_INT_FIELD, "sample_count", NULL },
    { T_INT_FIELD, "force_update", NULL },
    { T_STRING_FIELD, "codectype", NULL },
    { T_INT_FIELD, "idx", NULL },
    { T_INT_FIELD, "has_video", NULL },
    { T_INT_FIELD, "contentrating", NULL },
    { T_INT_FIELD, "bits_per_sample", NULL },
    { T_STRING_FIELD, "album_artist", NULL },

    /* end of db fields */
    { T_OR, "or", NULL },
    { T_AND, "and", NULL },
    { T_INCLUDES, "includes", NULL },
    { T_BEFORE, "before", NULL },
    { T_AFTER, "after", NULL },
    { T_AGO, "ago", NULL },
    { T_TODAY, "today", NULL },
    { T_THE, "the", NULL },
    { T_DAY, "days", NULL },
    { T_DAY, "day", NULL },
    { T_WEEK, "weeks", NULL },
    { T_WEEK, "week", NULL },
    { T_MONTH, "months", NULL },
    { T_MONTH, "month", NULL },
    { T_YEAR, "years", NULL },
    { T_YEAR, "year", NULL },
    { T_NOT, "not", NULL },
    { T_STARTSWITH, "startswith", NULL },
    { T_ENDSWITH, "endswith", NULL },

    /* end */
    { 0, NULL, NULL }
};

FIELDLOOKUP sp_fields_1[] = {
    { T_STRING_FIELD, "dmap.itemname", "title" },
    { T_INT_FIELD, "dmap.itemid", "id" },
    { T_STRING_FIELD, "daap.songalbum", "album" },
    { T_STRING_FIELD, "daap.songartist", "artist" },
    { T_INT_FIELD, "daap.songbitrate", "bitrate" },
    { T_STRING_FIELD, "daap.songcomment", "comment" },
    { T_INT_FIELD, "daap.songcompilation", "compilation" },
    { T_STRING_FIELD, "daap.songcomposer", "composer" },
    { T_INT_FIELD, "daap.songdatakind", "data_kind" },
    { T_STRING_FIELD, "daap.songdataurl", "url" },
    { T_INT_FIELD, "daap.songdateadded", "time_added" },
    { T_INT_FIELD, "daap.songdatemodified", "time_modified" },
    { T_STRING_FIELD, "daap.songdescription", "description" },
    { T_INT_FIELD, "daap.songdisccount", "total_discs" },
    { T_INT_FIELD, "daap.songdiscnumber", "disc" },
    { T_STRING_FIELD, "daap.songformat", "type" },
    { T_STRING_FIELD, "daap.songgenre", "genre" },
    { T_INT_FIELD, "daap.songsamplerate", "samplerate" },
    { T_INT_FIELD, "daap.songsize", "file_size" },
    //    { T_INT_FIELD,    "daap.songstarttime",   0 },
    { T_INT_FIELD, "daap.songstoptime", "song_length" },
    { T_INT_FIELD, "daap.songtime", "song_length" },
    { T_INT_FIELD, "daap.songtrackcount", "total_tracks" },
    { T_INT_FIELD, "daap.songtracknumber", "track" },
    { T_INT_FIELD, "daap.songyear", "year" },

    { 0, NULL, NULL }
};

FIELDLOOKUP *sp_fields[2] = {
    sp_fields_0, sp_fields_1
};

typedef struct tag_parsetree {
    int in_string;
    int token_list;
    char *term;
    char *current;
    SP_TOKEN token;
    int token_pos;
    SP_NODE *tree;
    char *error;
    char level;
} PARSESTRUCT, *PARSETREE;

#define SP_E_SUCCESS       0x00
#define SP_E_CLOSE         0x01
#define SP_E_FIELD         0x02
#define SP_E_STRCMP        0x03
#define SP_E_CLOSEQUOTE    0x04
#define SP_E_STRING        0x05
#define SP_E_OPENQUOTE     0x06
#define SP_E_INTCMP        0x07
#define SP_E_NUMBER        0x08
#define SP_E_DATECMP       0x09
#define SP_E_BEFOREAFTER   0x0a
#define SP_E_TIMEINTERVAL  0x0b
#define SP_E_DATE          0x0c
#define SP_E_EXPRQUOTE     0x0d
#define SP_E_EOS           0x0e
#define SP_E_UNKNOWN       0x0f

char *sp_errorstrings[] = {
    "Success",
    "Expecting ')'",
    "Expecting field name",
    "Expecting string comparison operator (=, includes)",
    "Expecting '\"' (closing quote)",
    "Expecting literal string",
    "Expecting '\"' (opening quote)",
    "Expecting integer comparison operator (=,<,>, etc)",
    "Expecting integer",
    "Expecting date comparison operator (<,<=,>,>=)",
    "Expecting interval comparison (before, after)",
    "Expecting time interval (days, weeks, months, years)",
    "Expecting date",
    "Expecting ' (single quote)\n",
    "Expecting end of statement\n",
    "Unknown Error.  Help?\n"
};

/* Forwards */
SP_NODE *sp_parse_phrase(PARSETREE tree);
SP_NODE *sp_parse_oexpr(PARSETREE tree);
SP_NODE *sp_parse_aexpr(PARSETREE tree);
SP_NODE *sp_parse_expr(PARSETREE tree);
SP_NODE *sp_parse_criterion(PARSETREE tree);
SP_NODE *sp_parse_string_criterion(PARSETREE tree);
SP_NODE *sp_parse_int_criterion(PARSETREE tree);
SP_NODE *sp_parse_date_criterion(PARSETREE tree);
time_t sp_parse_date(PARSETREE tree);
time_t sp_parse_date_interval(PARSETREE tree);
void sp_free_node(SP_NODE *node);
int sp_node_size(SP_NODE *node);
void sp_set_error(PARSETREE tree,int error);


/**
 * simple logging funcitons
 *
 * @param tree tree ew are parsing
 * @param function funtion entering/exiting
 * @param result result of param (if exiting)
 */
void sp_enter_exit(PARSETREE tree, char *function, int enter, void *result) {
    char *str_result = result ? "success" : "failure";

    if(enter) {
        tree->level++;
        DPRINTF(E_DBG,L_PARSE,"%*s Entering %s\n",tree->level," ",function);
    } else {
        DPRINTF(E_DBG,L_PARSE,"%*s Exiting %s (%s)\n",tree->level," ",
            function, str_result);
        tree->level--;
    }
}


/**
 * see if a string is actually a number
 *
 * @param string string to check
 * @returns 1 if the string is numeric, 0 otherwise
 */
int sp_isnumber(char *string) {
    char *current=string;

    while(*current && (*current >= '0') && (*current <= '9')) {
        current++;
    }

    return *current ? 0 : 1;
}

/**
 * see if a string is actually a date in the date format
 * YYYY-MM-DD
 *
 * @param string string to check
 * @returns time_t of date, or 0 if not a date
 */
time_t sp_isdate(char *string) {
    struct tm date_time;
    time_t seconds=0;

    memset((void*)&date_time,0,sizeof(date_time));
    if(strptime(string,"%Y-%m-%d",&date_time)) {
        seconds=timegm(&date_time);
    }

    return seconds;
}

/**
 * scan the input, returning the next available token.  This is
 * kind of a mess, and looking at it with new eyes would probably
 * yield a better way of tokenizing the stream, but this seems to
 * work.
 *
 * @param tree current working parse tree.
 * @returns next token (token, not the value)
 */
int sp_scan(PARSETREE tree, int hint) {
    char *terminator=NULL;
    char *tail;
    FIELDLOOKUP *pfield=sp_fields[tree->token_list];
    int len;
    int found;
    int numval;
    int is_qstr;
    time_t tval;
    char *qstr;
    char *token_string;
    char *dst, *src;

    if(tree->token.token_id & 0x2000) {
        if(tree->token.data.cvalue)
            free(tree->token.data.cvalue);
    }

    if(tree->token.token_id == T_EOF) {
        DPRINTF(E_DBG,L_PARSE,"%*s Returning token T_EOF\n",tree->level," ");
        return T_EOF;
    }

    /* keep advancing until we have a token */
    while(*(tree->current) && strchr(" \t\n\r",*(tree->current)))
        tree->current++;

    tree->token_pos = (int) (tree->current - tree->term);

    if(!*(tree->current)) {
        tree->token.token_id = T_EOF;
        DPRINTF(E_DBG,L_PARSE,"%*s Returning token %04x\n",tree->level," ",
            tree->token.token_id);
        return tree->token.token_id;
    }

    DPRINTF(E_SPAM,L_PARSE,"Current offset: %d, char: %c\n",
        tree->token_pos, *(tree->current));

    if(hint == SP_HINT_STRING) {
        terminator=sp_terminators[tree->token_list][1];
        tree->in_string = 1;
    } else {
        terminator = sp_terminators[tree->token_list][0];
        tree->in_string = 0;
    }


    DPRINTF(E_SPAM,L_PARSE,"Starting scan - in_string: %d, hint: %d\n",
            tree->in_string, hint);

    /* check symbols */
    if(!tree->in_string) {
        pfield=sp_symbols[tree->token_list];
        while(pfield->name) {
            if(!strncmp(pfield->name,tree->current,strlen(pfield->name))) {
                /* that's a match */
                tree->current += strlen(pfield->name);
                tree->token.token_id = pfield->type;
                return pfield->type;
            }
            pfield++;
        }
    }

    qstr = sp_terminators[tree->token_list][3];
    is_qstr = (strchr(qstr,*(tree->current)) != NULL);

    DPRINTF(E_SPAM,L_PARSE,"qstr: %s -- is_quoted: %d\n",qstr,is_qstr);

    if(strlen(qstr)) { /* strings ARE quoted */
        if(hint == SP_HINT_STRING) { /* MUST be a quote */
            if(!is_qstr) {
                tree->token.token_id = T_ERROR;
                return T_ERROR;
            }
            tree->current++; /* absorb it*/
        } else {
            if(is_qstr) {
                tree->in_string = 1; /* guess we're in a string */
                terminator=sp_terminators[tree->token_list][1];
                tree->current++;
            }
        }
    }

    DPRINTF(E_SPAM,L_PARSE,"keyword or string!\n");

    /* walk to a terminator */
    tail = tree->current;

    while((*tail) && (!strchr(terminator,*tail))) {
        /* skip escaped characters -- will be unescaped later */
        if((*tail == '\\')&&(*(tail+1) != '\0'))
            tail++;
        tail++;
    }

    found=0;
    len = (int) (tail - tree->current);

    DPRINTF(E_SPAM,L_PARSE,"Len: %d, in_string: %d\n",len,tree->in_string);

    if(!tree->in_string) {
        /* find it in the token list */
        pfield=sp_fields[tree->token_list];
        while(pfield->name) {
            DPRINTF(E_SPAM,L_PARSE,"Comparing to %s\n",pfield->name);
            if(strlen(pfield->name) == len) {
                if(strncasecmp(pfield->name,tree->current,len) == 0) {
                    found=1;
                    break;
                }
            }
            pfield++;
        }
    }

    if(found) {
        tree->token.token_id = pfield->type;
    } else {
        tree->token.token_id = T_STRING;
    }

    if(tree->token.token_id & 0x2000) {
        token_string=tree->current;
        if(found) {
            if(pfield->xlat) {
                len = (int)strlen(pfield->xlat);
                token_string = pfield->xlat;
            }
        }

        tree->token.data.cvalue = malloc(len + 1);
        if(!tree->token.data.cvalue) {
            /* fail on malloc error */
            DPRINTF(E_FATAL,L_PARSE,"Malloc error.\n");
        }
        strncpy(tree->token.data.cvalue,token_string,len);
        tree->token.data.cvalue[len] = '\x0';
    }

    if((hint == SP_HINT_NONE) || (hint == SP_HINT_INT)) {
        /* check for numeric? */
        if(tree->token.token_id == T_STRING &&
           (!tree->in_string) &&
           sp_isnumber(tree->token.data.cvalue)) {
            /* woops! */
            numval = atoi(tree->token.data.cvalue);
            free(tree->token.data.cvalue);
            tree->token.data.ivalue = numval;
            tree->token.token_id = T_NUMBER;
        }
    }

    if((hint == SP_HINT_NONE) || (hint == SP_HINT_DATE)) {
        if(tree->token.token_id == T_STRING &&
           (!tree->in_string) &&
           (tval=sp_isdate(tree->token.data.cvalue))) {
            free(tree->token.data.cvalue);
            tree->token.data.tvalue = tval;
            tree->token.token_id = T_DATE;
        }
    }

    tree->current=tail;

    /* if we are in_string, and we have quoted strings, ensure we
     * have a quote */

    is_qstr = (strchr(qstr,*tree->current) != NULL);
    if((!found) && strlen(qstr) && (tree->in_string)) {
        if(is_qstr) {
            tree->current++; /* absorb it */
        } else {
            DPRINTF(E_INF,L_PARSE,"Missing closing quotes\n");
            if(tree->token.token_id & 0x2000) {
                free(tree->token.data.cvalue);
            }
            tree->token.token_id = T_ERROR;
        }
    }

    /* escape string */
    if(tree->token.token_id == T_STRING) {
        src = dst = tree->token.data.cvalue;
        while(*src) {
            if(*src != '\\') {
                *dst++ = *src++;
            } else {
                src++;
                if(*src) {
                    *dst++ = *src++;
                }
            }
        }
        *dst = '\0';
    }

    DPRINTF(E_DBG,L_PARSE,"%*s Returning token %04x\n",tree->level," ",
        tree->token.token_id);
    if(tree->token.token_id & 0x2000)
        DPRINTF(E_SPAM,L_PARSE,"String val: %s\n",tree->token.data.cvalue);
    if(tree->token.token_id & 0x1000)
        DPRINTF(E_SPAM,L_PARSE,"Int val: %d\n",tree->token.data.ivalue);

    return tree->token.token_id;
}


/**
 * set up the initial parse tree
 *
 * @returns opaque parsetree struct
 */
PARSETREE sp_init(void) {
    PARSETREE ptree;

    ptree = (PARSETREE)malloc(sizeof(PARSESTRUCT));
    if(!ptree)
        DPRINTF(E_FATAL,L_PARSE,"Alloc error\n");

    memset(ptree,0,sizeof(PARSESTRUCT));

    /* this sets the default token list as well (0) */

    return ptree;
}

/**
 * parse a term or phrase into a tree.
 *
 * I'm not a language expert, so I'd welcome suggestions on the
 * following production rules:
 *
 * phrase -> oexpr T_EOF
 * oexpr -> aexpr { T_AND aexpr }
 * aexpr -> expr { T_OR expr }
 * expr -> T_OPENPAREN oexpr T_CLOSEPAREN | criterion
 * criterion -> field op value
 *
 * field -> T_STRINGFIELD, T_INTFIELD, T_DATEFIELD
 * op -> T_EQUAL, T_GREATEREQUAL, etc
 * value -> T_NUMBER, T_STRING, or T_DATE, as appropriate
 *
 * @param tree parsetree previously created with sp_init
 * @param term term or phrase to parse
 * @returns 1 if successful, 0 if not
 */
int sp_parse(PARSETREE tree, char *term, int type) {
    tree->term = strdup(term); /* will be destroyed by parsing */
    tree->current=tree->term;
    tree->token.token_id=T_BOF;
    tree->token_list = type;

    if(tree->tree)
        sp_free_node(tree->tree);

    sp_scan(tree,SP_HINT_NONE);
    tree->tree = sp_parse_phrase(tree);

    if(tree->tree) {
        DPRINTF(E_SPAM,L_PARSE,"Parsed successfully (type %d)\n",type);
    } else {
        DPRINTF(E_SPAM,L_PARSE,"Parsing error (type %d)\n",type);
    }

    return tree->tree ? 1 : 0;
}


/**
 * parse for a phrase
 *
 * phrase -> aexpr T_EOF
 *
 * @param tree tree we are parsing (and building)
 * @returns new SP_NODE * if successful, NULL otherwise
 */

SP_NODE *sp_parse_phrase(PARSETREE tree) {
    SP_NODE *expr;

    sp_enter_exit(tree,"sp_parse_phrase",1,NULL);

    DPRINTF(E_SPAM,L_PARSE,"%*s Entering sp_parse_phrase\n",tree->level," ");
    tree->level++;

    expr = sp_parse_oexpr(tree);
    if((!expr) || (tree->token.token_id != T_EOF)) {
        if(!tree->error) {
            sp_set_error(tree,SP_E_EOS);
        }
        sp_free_node(expr);
        expr = NULL;
    }

    sp_enter_exit(tree,"sp_parse_phrase",0,expr);
    return expr;
}

/**
 * parse for an ANDed expression
 *
 * aexpr -> expr { T_AND expr }
 *
 * @param tree tree we are building
 * @returns new SP_NODE pointer if successful, NULL otherwise
 */
SP_NODE *sp_parse_aexpr(PARSETREE tree) {
    SP_NODE *expr;
    SP_NODE *pnew;

    sp_enter_exit(tree,"sp_parse_aexpr",1,NULL);

    expr = sp_parse_expr(tree);

    while(expr && ((tree->token.token_id == T_AND) ||
                   (tree->token.token_id == T_GREATERAND))) {
        pnew = (SP_NODE*)malloc(sizeof(SP_NODE));
        if(!pnew) {
            DPRINTF(E_FATAL,L_PARSE,"Malloc error\n");
        }

        memset(pnew,0x00,sizeof(SP_NODE));
        pnew->op=T_AND;
        pnew->op_type = SP_OPTYPE_ANDOR;

        pnew->left.node = expr;
        sp_scan(tree,SP_HINT_NONE);
        pnew->right.node = sp_parse_expr(tree);

        if(!pnew->right.node) {
            sp_free_node(pnew);
            pnew=NULL;
        }

        expr=pnew;
    }

    sp_enter_exit(tree,"sp_parse_aexpr",0,expr);
    return expr;
}

/**
 * parse for an ORed expression
 *
 * oexpr -> aexpr { T_OR aexpr }
 *
 * @param tree tree we are building
 * @returns new SP_NODE pointer if successful, NULL otherwise
 */
SP_NODE *sp_parse_oexpr(PARSETREE tree) {
    SP_NODE *expr;
    SP_NODE *pnew;

    sp_enter_exit(tree,"sp_parse_oexpr",1,NULL);

    expr = sp_parse_aexpr(tree);

    while(expr && (tree->token.token_id == T_OR)) {
        pnew = (SP_NODE*)malloc(sizeof(SP_NODE));
        if(!pnew) {
            DPRINTF(E_FATAL,L_PARSE,"Malloc error\n");
        }

        memset(pnew,0x00,sizeof(SP_NODE));
        pnew->op=T_OR;
        pnew->op_type = SP_OPTYPE_ANDOR;

        pnew->left.node = expr;
        sp_scan(tree,SP_HINT_NONE);
        pnew->right.node = sp_parse_aexpr(tree);

        if(!pnew->right.node) {
            sp_free_node(pnew);
            pnew=NULL;
        }

        expr=pnew;
    }

    sp_enter_exit(tree,"sp_parse_oexpr",0,expr);
    return expr;
}

/**
 * parse for an expression
 *
 * expr -> T_OPENPAREN phrase T_CLOSEPAREN | criteria
 *
 * @param tree tree we are building
 * @returns pointer to new SP_NODE if successful, NULL otherwise
 */
SP_NODE *sp_parse_expr(PARSETREE tree) {
    SP_NODE *expr;

    sp_enter_exit(tree,"sp_parse_expr",1,NULL);

    if(tree->token.token_id == T_OPENPAREN) {
        sp_scan(tree,SP_HINT_NONE);
        expr = sp_parse_oexpr(tree);
        if((expr) && (tree->token.token_id == T_CLOSEPAREN)) {
            sp_scan(tree,SP_HINT_NONE);
        } else {
            /* Error: expecting close paren */
            sp_set_error(tree,SP_E_CLOSE);
            sp_free_node(expr);
            expr=NULL;
        }
    } else {
        expr = sp_parse_criterion(tree);
    }

    sp_enter_exit(tree,"sp_parse_expr",0,expr);
    return expr;
}

/**
 * parse for a criterion
 *
 * criterion -> field op value
 *
 * @param tree tree we are building
 * @returns pointer to new SP_NODE if successful, NULL otherwise.
 */
SP_NODE *sp_parse_criterion(PARSETREE tree) {
    SP_NODE *expr=NULL;

    sp_enter_exit(tree,"sp_parse_criterion",1,expr);

    if(tree->token_list == 1) {
        if(tree->token.token_id != T_EXPRQUOTE) {
            sp_set_error(tree,SP_E_EXPRQUOTE);
            return NULL;
        } else {
            sp_scan(tree,SP_HINT_NONE);
        }
    }

    switch(tree->token.token_id) {
    case T_STRING_FIELD:
        expr = sp_parse_string_criterion(tree);
        break;

    case T_INT_FIELD:
        expr = sp_parse_int_criterion(tree);
        break;

    case T_DATE_FIELD:
        expr = sp_parse_date_criterion(tree);
        break;

    default:
        /* Error: expecting field */
        sp_set_error(tree,SP_E_FIELD);
        expr = NULL;
        break;
    }

    if(tree->token_list == 1) {
        if(tree->token.token_id != T_EXPRQUOTE) {
            sp_set_error(tree,SP_E_EXPRQUOTE);
            sp_free_node(expr);
            return NULL;
        } else {
            sp_scan(tree,SP_HINT_NONE);
        }
    }

    sp_enter_exit(tree,"sp_parse_criterion",0,expr);
    return expr;
}

/**
 * parse for a string criterion
 *
 * @param tree tree we are building
 * @returns pointer to new SP_NODE if successful, NULL otherwise
 */
 SP_NODE *sp_parse_string_criterion(PARSETREE tree) {
    int result=0;
    SP_NODE *pnew = NULL;

    sp_enter_exit(tree,"sp_parse_string_criterion",1,NULL);

    pnew = malloc(sizeof(SP_NODE));
    if(!pnew) {
        DPRINTF(E_FATAL,L_PARSE,"Malloc Error\n");
    }
    memset(pnew,0x00,sizeof(SP_NODE));
    pnew->left.field = strdup(tree->token.data.cvalue);

    sp_scan(tree,SP_HINT_NONE); /* scan past the string field we know is there */

    if(tree->token.token_id == T_NOT) {
        pnew->not_flag=1;
        sp_scan(tree,SP_HINT_NONE);
    }

    switch(tree->token.token_id) {
    case T_EQUAL:
    case T_INCLUDES:
    case T_STARTSWITH:
    case T_ENDSWITH:
        pnew->op=tree->token.token_id;
        pnew->op_type = SP_OPTYPE_STRING;
        result = 1;
        break;
    default:
        /* Error: expecting legal string comparison operator */
        sp_set_error(tree,SP_E_STRCMP);
        break;
    }

    if(result) {
        sp_scan(tree,SP_HINT_STRING);
        /* should be sitting on string literal */
        if(tree->token.token_id == T_STRING) {
            result = 1;
            pnew->right.cvalue=strdup(tree->token.data.cvalue);
            if(tree->token_list == 1) {
                if(pnew->right.cvalue[0]=='*') {
                    pnew->op = T_ENDSWITH;
                    memcpy(pnew->right.cvalue,&pnew->right.cvalue[1],
                           (int)strlen(pnew->right.cvalue)); /* with zt */
                }
                if(pnew->right.cvalue[strlen(pnew->right.cvalue)-1] == '*') {
                    pnew->op = (pnew->op==T_ENDSWITH)?T_INCLUDES:T_STARTSWITH;
                    pnew->right.cvalue[strlen(pnew->right.cvalue)-1] = '\0';
                }
            }
            sp_scan(tree,SP_HINT_NONE);
        } else {
            sp_set_error(tree,SP_E_OPENQUOTE);
            DPRINTF(E_SPAM,L_PARSE,"Expecting string, got %04X\n",
                    tree->token.token_id);
            result = 0;
        }
    }


    if(!result) {
        sp_free_node(pnew);
        pnew=NULL;
    }

    sp_enter_exit(tree,"sp_parse_string_criterion",0,pnew);
    return pnew;
 }

/**
 * parse for an int criterion
 *
 * @param tree tree we are building
 * @returns address of new SP_NODE if successful, NULL otherwise
 */
 SP_NODE *sp_parse_int_criterion(PARSETREE tree) {
    int result=0;
    SP_NODE *pnew = NULL;

    sp_enter_exit(tree,"sp_parse_int_criterion",1,pnew);
    pnew = malloc(sizeof(SP_NODE));
    if(!pnew) {
        DPRINTF(E_FATAL,L_PARSE,"Malloc Error\n");
    }
    memset(pnew,0x00,sizeof(SP_NODE));
    pnew->left.field = strdup(tree->token.data.cvalue);

    sp_scan(tree,SP_HINT_NONE); /* scan past the int field we know is there */

    if(tree->token.token_id == T_NOT) {
        pnew->not_flag=1;
        sp_scan(tree,SP_HINT_NONE);
    }

    switch(tree->token.token_id) {
    case T_LESSEQUAL:
    case T_LESS:
    case T_GREATEREQUAL:
    case T_GREATER:
    case T_EQUAL:
        result = 1;
        pnew->op=tree->token.token_id;
        pnew->op_type = SP_OPTYPE_INT;
        break;
    case T_GREATERAND:
        result = 1;
        pnew->op = T_GREATER;
        pnew->op_type = SP_OPTYPE_INT;
        break;
    default:
        /* Error: expecting legal int comparison operator */
        sp_set_error(tree,SP_E_INTCMP);
        DPRINTF(E_LOG,L_PARSE,"Expecting int comparison op, got %04X\n",
            tree->token.token_id);
        break;
    }

    if(result) {
        sp_scan(tree,SP_HINT_INT);
        /* should be sitting on a literal string */
        if(tree->token.token_id == T_NUMBER) {
            result = 1;
            pnew->right.ivalue=tree->token.data.ivalue;
            sp_scan(tree,SP_HINT_NONE);
        } else {
            /* Error: Expecting number */
            sp_set_error(tree,SP_E_NUMBER);
            DPRINTF(E_LOG,L_PARSE,"Expecting number, got %04X\n",
                tree->token.token_id);
            result = 0;
        }
    }

    if(!result) {
        sp_free_node(pnew);
        pnew=NULL;
    }

    sp_enter_exit(tree,"sp_parse_int_criterion",0,pnew);

    return pnew;
 }


/**
 * parse for a date criterion
 *
 * @param tree tree we are building
 * @returns pointer to new SP_NODE if successful, NULL otherwise
 */
SP_NODE *sp_parse_date_criterion(PARSETREE tree) {
    SP_NODE *pnew=NULL;
    int result=0;

    sp_enter_exit(tree,"sp_parse_date_criterion",1,pnew);
    pnew = malloc(sizeof(SP_NODE));
    if(!pnew) {
        DPRINTF(E_FATAL,L_PARSE,"Malloc Error\n");
    }
    memset(pnew,0x00,sizeof(SP_NODE));
    pnew->left.field = strdup(tree->token.data.cvalue);

    sp_scan(tree,SP_HINT_NONE); /* scan past the date field we know is there */

    if(tree->token.token_id == T_NOT) {
        pnew->not_flag=1;
        sp_scan(tree,SP_HINT_NONE);
    }

    switch(tree->token.token_id) {
    case T_LESSEQUAL:
    case T_LESS:
    case T_GREATEREQUAL:
    case T_GREATER:
        result = 1;
        pnew->op=tree->token.token_id;
        pnew->op_type = SP_OPTYPE_DATE;
        break;
    case T_GREATERAND:
        result = 1;
        pnew->op=T_GREATER;
        pnew->op_type = SP_OPTYPE_DATE;
        break;
    case T_BEFORE:
        result = 1;
        pnew->op_type = SP_OPTYPE_DATE;
        pnew->op=T_LESS;
        break;
    case T_AFTER:
        result = 1;
        pnew->op_type = SP_OPTYPE_DATE;
        pnew->op=T_GREATER;
        break;
    default:
        /* Error: expecting legal int comparison operator */
        sp_set_error(tree,SP_E_DATECMP);
        DPRINTF(E_LOG,L_PARSE,"Expecting int comparison op, got %04X\n",
            tree->token.token_id);
        break;
    }

    if(result) {
        sp_scan(tree,SP_HINT_NONE);
        /* should be sitting on a date */
        if((pnew->right.tvalue = sp_parse_date(tree))) {
            result=1;
        } else {
            sp_set_error(tree,SP_E_DATE);
            result=0;
        }
    }

    if(!result) {
        sp_free_node(pnew);
        pnew=NULL;
    }


    sp_enter_exit(tree,"sp_parse_date_criterion",0,pnew);

    return pnew;
 }

/**
 * parse a date value for a date field comparison
 *
 * date -> T_TODAY | T_DATE |
 *         date_interval T_BEFORE date | date_interval T_AFTER date
 * date_interval -> T_NUMBER ( T_DAY | T_WEEK | T_MONTH | T_YEAR)
 *                  (T_BEFORE | T_AFTER) date
 *
 * @param tree tree we are building/parsing
 * @returns time_t of date, or 0 if failure
 */
time_t sp_parse_date(PARSETREE tree) {
    time_t result=0;
    time_t interval;
    int before;

    sp_enter_exit(tree,"sp_parse_date",1,(void*)result);

    switch(tree->token.token_id) {
    case T_TODAY:
        result = time(NULL);
        sp_scan(tree,SP_HINT_NONE);
        break;
    case T_DATE:
        result = tree->token.data.tvalue;
        sp_scan(tree,SP_HINT_NONE);
        break;
    }

    if(!result) {
        /* must be an interval */
        interval = sp_parse_date_interval(tree);
        if(!interval) {
            result = 0;
        } else if((tree->token.token_id != T_BEFORE) &&
                (tree->token.token_id != T_AFTER)) {
            sp_set_error(tree,SP_E_BEFOREAFTER);
            result=0;
        } else {
            before = 1;
            if(tree->token.token_id == T_AFTER)
                before = 0;

            sp_scan(tree,SP_HINT_NONE);
            result = sp_parse_date(tree);
            if(result) {
                if(before) {
                    result -= interval;
                } else {
                    result += interval;
                }
            }
        }
    }

    sp_enter_exit(tree,"sp_parse_date_criterion",0,(void*)result);

    return result;
}

/**
 * parse a date inteval
 *
 * date_interval -> T_NUMBER (T_DAY | T_WEEK | T_MONTH | T_YEAR)
 * @param tree tree we are parsing/building
 * @returns time_t seconds in interval, or 0 if error
 */
time_t sp_parse_date_interval(PARSETREE tree) {
    time_t result=0;
    int count;

    sp_enter_exit(tree,"sp_parse_date_interval",1,(void*)result);

    if(tree->token.token_id != T_NUMBER) {
        result=0;
        sp_set_error(tree,SP_E_NUMBER);
    } else {
        count = tree->token.data.ivalue;
        sp_scan(tree,SP_HINT_NONE);
        switch(tree->token.token_id) {
        case T_DAY:
            result = count * 3600 * 24;
            sp_scan(tree,SP_HINT_NONE);
            break;
        case T_WEEK:
            result = count * 3600 * 24 * 7;
            sp_scan(tree,SP_HINT_NONE);
            break;
        case T_MONTH:
            result = count * 3600 * 24 * 30;
            sp_scan(tree,SP_HINT_NONE);
            break;
        case T_YEAR:
            result = count * 3600 * 24 * 365;
            sp_scan(tree,SP_HINT_NONE);
            break;
        default:
            result=0;
            sp_set_error(tree, SP_E_TIMEINTERVAL);
            break;
        }
    }

    sp_enter_exit(tree,"sp_parse_date_interval",0,(void*)result);
    return result;
}



/**
 * free a node, and all left/right subnodes
 *
 * @param node node to free
 */
void sp_free_node(SP_NODE *node) {
    if(!node)
        return;

    if(node->op_type == SP_OPTYPE_ANDOR) {
        if(node->left.node) {
            sp_free_node(node->left.node);
            node->left.node = NULL;
        }

        if(node->right.node) {
            sp_free_node(node->right.node);
            node->right.node = NULL;
        }
    } else {
        if(node->left.field) {
            free(node->left.field);
            node->left.field = NULL;
        }

        if(node->op_type == SP_OPTYPE_STRING) {
            if(node->right.cvalue) {
                free(node->right.cvalue);
                node->right.cvalue = NULL;
            }
        }
    }


    free(node);
}


/**
 * dispose of an initialized tree
 *
 * @param tree tree to dispose
 * @returns 1
 */
int sp_dispose(PARSETREE tree) {
    if(tree->term)
        free(tree->term);

    if(tree->token.token_id & 0x2000)
        free(tree->token.data.cvalue);

    if(tree->error)
        free(tree->error);

    free(tree);
    return 1;
}

#ifdef HAVE_SQL
/**
 * calculate the size required to render the tree as a
 * sql query.
 *
 * @parameter node node/tree to calculate
 * @returns size in bytes of sql "where" clause
 */
int sp_node_size(SP_NODE *node) {
    int size;
    int string_size;

    if(node->op_type == SP_OPTYPE_ANDOR) {
        size = sp_node_size(node->left.node);
        size += sp_node_size(node->right.node);
        size += 7; /* (xxx AND xxx) */
    } else {
        size = 4; /* parens, plus spaces around op */
        size += (int) strlen(node->left.field);
        if((node->op & 0x0FFF) > T_LAST) {
            DPRINTF(E_FATAL,L_PARSE,"Can't determine node size:  op %04x\n",
                node->op);
        } else {
            size += (int) strlen(sp_token_descr[node->op & 0x0FFF]);
        }

        if(node->op_type == SP_OPTYPE_STRING) {
            string_size = 0;
            if((node->right.cvalue) && (strlen(node->right.cvalue)))
                db_sql_escape(NULL,&string_size,"%q",node->right.cvalue);

            size += (2 + string_size);
            if(node->op == T_INCLUDES) {
                size += 2; /* extra %'s */
            }
            if((node->op == T_STARTSWITH)||(node->op == T_ENDSWITH)) {
                size += 1;
            }
        }

        if((node->op_type == SP_OPTYPE_INT) || (node->op_type == SP_OPTYPE_DATE)) {
            size += 40; /* what *is* the max size of int64? */
        }

        if(node->not_flag) {
            size += 5; /* " not " */
        }
    }

    return size;
}

/**
 * serialize node into pre-allocated string
 *
 * @param node node to serialize
 * @param string string to generate
 */
void sp_serialize_sql(SP_NODE *node, char *string) {
    char buffer[40];
    int size;

    if(node->op_type == SP_OPTYPE_ANDOR) {
        strcat(string,"(");
        sp_serialize_sql(node->left.node,string);
        if(node->op == T_AND) strcat(string," and ");
        if(node->op == T_OR) strcat(string," or ");
        sp_serialize_sql(node->right.node,string);
        strcat(string,")");
    } else {
        strcat(string,"(");
        if(node->not_flag) {
            strcat(string,"not ");
        }
        strcat(string,node->left.field);
        strcat(string," ");
        strcat(string,sp_token_descr[node->op & 0x0FFF]);
        strcat(string," ");
        if(node->op_type == SP_OPTYPE_STRING) {
            strcat(string,"'");
            if((node->op == T_INCLUDES) || (node->op == T_ENDSWITH))
                strcat(string,"%");
            size = 0;
            if((node->right.cvalue) && (strlen(node->right.cvalue)))
                db_sql_escape(NULL,&size,"%q",node->right.cvalue);

            /* we don't have a way to verify we have that much
             * room, but we must... we allocated it earlier.
             */
            if((node->right.cvalue) && (strlen(node->right.cvalue)))
                db_sql_escape(&string[strlen(string)],&size,"%q",
                              node->right.cvalue);

//            strcat(string,node->right.cvalue);
            if((node->op == T_INCLUDES) || (node->op == T_STARTSWITH))
                strcat(string,"%");
            strcat(string,"'");
        }

        if(node->op_type == SP_OPTYPE_INT) {
            sprintf(buffer,"%d",node->right.ivalue);
            strcat(string,buffer);
        }

        if(node->op_type == SP_OPTYPE_DATE) {
            sprintf(buffer,"%d",(int)node->right.tvalue);
            strcat(string,buffer);
        }
        strcat(string,")");

    }
}



/**
 * generate sql "where" clause
 *
 * @param node node/tree to calculate
 * @returns sql string.  Must be freed by caller
 */
char *sp_sql_clause(PARSETREE tree) {
    int size;
    char *sql;

    DPRINTF(E_DBG,L_PARSE,"Fetching sql statement size\n");
    size = sp_node_size(tree->tree);
    DPRINTF(E_DBG,L_PARSE,"Size: %d\n",size);

    sql = (char*)malloc(size+1);

    memset(sql,0x00,size+1);
    sp_serialize_sql(tree->tree,sql);

    DPRINTF(E_DBG,L_PARSE,"Serialized to : %s\n",sql);

    return sql;
}
#endif /* HAVE_SQL */


/**
 * if there was an error in a previous action (parsing?)
 * then return that error to the client.  This does not
 * clear the error condition -- multiple calls to sp_geterror
 * will return the same value.  Also, if you want to keep an error,
 * you must strdup it before it disposing the parse tree...
 *
 * memory handling is done on the smart-parser side.
 *
 * @param tree tree that generated the last error
 * @returns text of the last error
 */
char *sp_get_error(PARSETREE tree) {
    if(tree->error == NULL) {
        sp_set_error(tree,SP_E_UNKNOWN);
    }
    return tree->error;
}

/**
 * set the parse tree error for retrieval above
 *
 * @param tree tree we are setting error for
 * @param error error code
 */
void sp_set_error(PARSETREE tree, int error) {
    int len;

    if(tree->error)
        free(tree->error);

    len = 10 + (tree->token_pos / 10) + 1 + (int) strlen(sp_errorstrings[error]) + 1;
    tree->error = (char*)malloc(len);
    if(!tree->error) {
        DPRINTF(E_FATAL,L_PARSE,"Malloc error");
        return;
    }

    sprintf(tree->error,"Offset %d:  %s",tree->token_pos + 1,sp_errorstrings[error]);
    return;
}

