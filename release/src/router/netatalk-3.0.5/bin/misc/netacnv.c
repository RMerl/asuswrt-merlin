#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <atalk/unicode.h>

#define MACCHARSET "MAC_ROMAN"

#define flag(x) {x, #x}

struct flag_map {
    int flag;
    const char *flagname;
};

struct flag_map flag_map[] = {
    flag(CONV_ESCAPEHEX),
    flag(CONV_UNESCAPEHEX),    
    flag(CONV_ESCAPEDOTS),
    flag(CONV_IGNORE),
    flag(CONV_FORCE),
    flag(CONV__EILSEQ),
    flag(CONV_TOUPPER),
    flag(CONV_TOLOWER),
    flag(CONV_PRECOMPOSE),
    flag(CONV_DECOMPOSE)
};

char buffer[MAXPATHLEN +2];

int main(int argc, char **argv)
{
    int opt;
    uint16_t flags = 0;
    char *string, *macName = MACCHARSET;
    char *f = NULL, *t = NULL;
    charset_t from, to, mac;

    while ((opt = getopt(argc, argv, "m:o:f:t:")) != -1) {
        switch(opt) {
        case 'm':
            macName = strdup(optarg);
            break;
        case 'o':
            for (int i = 0; i < sizeof(flag_map)/sizeof(struct flag_map) - 1; i++)
                if ((strcmp(flag_map[i].flagname, optarg)) == 0)
                    flags |= flag_map[i].flag;
            break;
        case 'f':
            f = strdup(optarg);
            break;
        case 't':
            t = strdup(optarg);
            break;
        }
    }

    if ((optind + 1) != argc) {
        printf("Usage: test [-o <conversion option> [...]] [-f <from charset>] [-t <to charset>] [-m legacy Mac charset] <string>\n");
        printf("Defaults: -f: UTF8-MAC, -t: UTF8, -m MAC_ROMAN\n");
        printf("Available conversion options:\n");
        for (int i = 0; i < (sizeof(flag_map)/sizeof(struct flag_map) - 1); i++) {
            printf("%s\n", flag_map[i].flagname);
        }
        return 1;
    }
    string = argv[optind];

    set_charset_name(CH_UNIX, "UTF8");
    set_charset_name(CH_MAC, macName);

    if ( (charset_t) -1 == (from = add_charset(f ? f : "UTF8-MAC")) ) {
        fprintf( stderr, "Setting codepage %s as from codepage failed\n", f ? f : "UTF8-MAC");
        return (-1);
    }

    if ( (charset_t) -1 == (to = add_charset(t ? t : "UTF8")) ) {
        fprintf( stderr, "Setting codepage %s as to codepage failed\n", t ? t : "UTF8");
        return (-1);
    }

    if ( (charset_t) -1 == (mac = add_charset(macName)) ) {
        fprintf( stderr, "Setting codepage %s as Mac codepage failed\n", MACCHARSET);
        return (-1);
    }


    if ((size_t)-1 == (convert_charset(from, to, mac,
                                       string, strlen(string),
                                       buffer, MAXPATHLEN,
                                       &flags)) ) {
        perror("Conversion error");
        return 1;
    }

    printf("from: %s\nto: %s\n", string, buffer);

    return 0;
}
