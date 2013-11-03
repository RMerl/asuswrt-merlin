// free.c - free(1)
// procps utility to display free memory information
//
// All new, Robert Love <rml@tech9.net>             18 Nov 2002
// Original by Brian Edmonds and Rafal Maszkowski   14 Dec 1992
//
// This program is licensed under the GNU Library General Public License, v2
//
// Copyright 2003 Robert Love
// Copyright 2004 Albert Cahalan

#include "proc/sysinfo.h"
#include "proc/version.h"
//#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define S(X) ( ((unsigned long long)(X) << 10) >> shift)

const char help_message[] =
"usage: free [-b|-k|-m|-g] [-l] [-o] [-t] [-s delay] [-c count] [-V]\n"
"  -b,-k,-m,-g show output in bytes, KB, MB, or GB\n"
"  -l show detailed low and high memory statistics\n"
"  -o use old format (no -/+buffers/cache line)\n"
"  -t display total for RAM + swap\n"
"  -s update every [delay] seconds\n"
"  -c update [count] times\n"
"  -V display version information and exit\n"
;

int main(int argc, char *argv[]){
    int i;
    int count = 0;
    int shift = 10;
    int pause_length = 0;
    int show_high = 0;
    int show_total = 0;
    int old_fmt = 0;

    /* check startup flags */
    while( (i = getopt(argc, argv, "bkmglotc:s:V") ) != -1 )
        switch (i) {
        case 'b': shift = 0;  break;
        case 'k': shift = 10; break;
        case 'm': shift = 20; break;
        case 'g': shift = 30; break;
        case 'l': show_high = 1; break;
        case 'o': old_fmt = 1; break;
        case 't': show_total = 1; break;
        case 's': pause_length = 1000000 * atof(optarg); break;
        case 'c': count = strtoul(optarg, NULL, 10); break;
	case 'V': display_version(); exit(0);
        default:
            fwrite(help_message,1,strlen(help_message),stderr);
	    return 1;
    }

    do {
        meminfo();
        printf("             total       used       free     shared    buffers     cached\n");
        printf(
            "%-7s %10Lu %10Lu %10Lu %10Lu %10Lu %10Lu\n", "Mem:",
            S(kb_main_total),
            S(kb_main_used),
            S(kb_main_free),
            S(kb_main_shared),
            S(kb_main_buffers),
            S(kb_main_cached)
        );
        // Print low vs. high information, if the user requested it.
        // Note we check if low_total==0: if so, then this kernel does
        // not export the low and high stats.  Note we still want to
        // print the high info, even if it is zero.
        if (show_high) {
            printf(
                "%-7s %10Lu %10Lu %10Lu\n", "Low:",
                S(kb_low_total),
                S(kb_low_total - kb_low_free),
                S(kb_low_free)
            );
            printf(
                "%-7s %10Lu %10Lu %10Lu\n", "High:",
                S(kb_high_total),
                S(kb_high_total - kb_high_free),
                S(kb_high_free)
            );
        }
        if(!old_fmt){
            unsigned KLONG buffers_plus_cached = kb_main_buffers + kb_main_cached;
            printf(
                "-/+ buffers/cache: %10Lu %10Lu\n", 
                S(kb_main_used - buffers_plus_cached),
                S(kb_main_free + buffers_plus_cached)
            );
        }
        printf(
            "%-7s %10Lu %10Lu %10Lu\n", "Swap:",
            S(kb_swap_total),
            S(kb_swap_used),
            S(kb_swap_free)
        );
        if(show_total){
            printf(
                "%-7s %10Lu %10Lu %10Lu\n", "Total:",
                S(kb_main_total + kb_swap_total),
                S(kb_main_used  + kb_swap_used),
                S(kb_main_free  + kb_swap_free)
            );
        }
        if(pause_length){
	    fputc('\n', stdout);
	    fflush(stdout);
	    if (count != 1) usleep(pause_length);
	}
    } while(pause_length && --count);

    return 0;
}
