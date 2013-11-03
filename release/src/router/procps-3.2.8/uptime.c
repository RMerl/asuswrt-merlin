#include <stdio.h>
#include <string.h>
#include "proc/whattime.h"
#include "proc/version.h"

int main(int argc, char *argv[]) {
    if(argc == 1) {
        print_uptime();
        return 0;
    }
    if((argc == 2) && (!strcmp(argv[1], "-V"))) {
        display_version();
        return 0;
    }
    fprintf(stderr, "usage: uptime [-V]\n    -V    display version\n");
    return 1;
}
