#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

int ip6tables_main(int argc, char **argv);
int ip6tables_save_main(int argc, char **argv);
int ip6tables_restore_main(int argc, char **argv);

int main(int argc, char **argv) {
  char *progname;

  if (argc == 0) {
    fprintf(stderr, "no argv[0]?");
    exit(1);
  } else {
    progname = basename(argv[0]);

    if (!strcmp(progname, "ip6tables"))
      return ip6tables_main(argc, argv);
    
#ifdef IPTABLES_SAVE
    if (!strcmp(progname, "ip6tables-save"))
      return ip6tables_save_main(argc, argv);
#endif
    
    if (!strcmp(progname, "ip6tables-restore"))
      return ip6tables_restore_main(argc, argv);
    
    fprintf(stderr, "ip6tables multi-purpose version: unknown applet name %s\n", progname);
    exit(1);
  }
}
