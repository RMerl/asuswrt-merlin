/*
 *
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/param.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>

#include <atalk/globals.h>

static char	*l_curr;
static char	*l_end;

void initline( int len, char *line)
{
    l_curr = line;
    l_end = line + len;
}

#define ST_QUOTE	0
#define ST_WORD		1
#define ST_BEGIN	2

int
parseline(int len, char *token)
{
    char	*p, *e;
    int		state;

    state = ST_BEGIN;
    p = token;
    e = token + len;

    for (;;) {
        if ( l_curr > l_end ) {			/* end of line */
            *token = '\0';
            return( -1 );
        }

        switch ( *l_curr ) {
        case '"' :
            if ( state == ST_QUOTE ) {
                state = ST_WORD;
            } else {
                state = ST_QUOTE;
            }
            break;

        case '\0' :
        case '\t' :
        case '\n' :
        case ' ' :
            if ( state == ST_WORD ) {
                *p = '\0';
                return( p - token );
            }
            if ( state != ST_QUOTE ) {
                break;
            }
            /* FALL THROUGH */

        default :
            if ( state == ST_BEGIN ) {
                state = ST_WORD;
            }
            if ( p > e ) {			/* end of token */
                *token = '\0';
                return( -1 );
            }
            *p++ = *l_curr;
            break;
        }

        l_curr++;
    }
}

#ifdef notdef
void parseline(char *token, char *user)
{
    char		*p = pos, *t = token, *u, *q, buf[ MAXPATHLEN ];
    struct passwd	*pwent;
    int			quoted = 0;

    while ( isspace( *p )) {
        p++;
    }

    /*
     * If we've reached the end of the line, or a comment,
     * don't return any more tokens.
     */
    if ( *p == '\0' || *p == '#' ) {
        *token = '\0';
        return;
    }

    if ( *p == '"' ) {
        p++;
        quoted = 1;
    }
    while ( *p != '\0' && ( quoted || !isspace( *p ))) {
        if ( *p == '"' ) {
            if ( quoted ) {
                *t = '\0';
                break;
            }
            quoted = 1;
            p++;
        } else {
            *t++ = *p++;
        }
    }
    pos = p;
    *t = '\0';

    /*
     * We got to the end of the line without closing an open quote
     */
    if ( *p == '\0' && quoted ) {
        *token = '\0';
        return;
    }

    t = token;
    if ( *t == '~' ) {
        t++;
        if ( *t == '\0' || *t == '/' ) {
            u = user;
            if ( *t == '/' ) {
                t++;
            }
        } else {
            u = t;
            if (( q = strchr( t, '/' )) == NULL ) {
                t = "";
            } else {
                *q = '\0';
                t = q + 1;
            }
        }
        if ( u == NULL || ( pwent = getpwnam( u )) == NULL ) {
            *token = '\0';
            return;
        }
        strcpy( buf, pwent->pw_dir );
        if ( *t != '\0' ) {
            strcat( buf, "/" );
            strcat( buf, t );
        }
        strcpy( token, buf );
    }
    return;
}
#endif /* notdef */
