/*
 *	Wireless Tools
 *
 *		Jean II - HPL 04
 *
 * Main code for "iwmulticall". This is a wrapper for the multicall version
 * of the wireless tools.
 * You need to link this code against "-lm".
 * Thanks to Ned Ludd <solar@gentoo.org> for the inspiration...
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2004 Jean Tourrilhes <jt@hpl.hp.com>
 */

/***************************** INCLUDES *****************************/

#include <libgen.h>	/* Basename */

/**************************** PROTOTYPES ****************************/

/* Prototypes of the main of each tool */
extern int
	main_iwconfig(int	argc,
		      char **	argv);
extern int
	main_iwlist(int	argc,
		    char **	argv);
extern int
	main_iwspy(int	argc,
		   char **	argv);
extern int
	main_iwpriv(int	argc,
		    char **	argv);
extern int
	main_iwgetid(int	argc,
		     char **	argv);

/************************** MULTICALL HACK **************************/
/*
 * The idea for multicall is to put all the tools and the library in
 * the same binary. This way, you can save the overhead of the library,
 * of each tool, can better optimise the code and throw away the stuff
 * you don't need from the library.
 * This almost divide the size of the tools by two (without stripping).
 * On the down side, you no longer have the libiw for other tools to
 * use, but for the target systems (embedded), this doesn't matter
 * much, as they just need to configure the card...
 * Note that splitting the lib and the multicall tools would not
 * make sense, as most gains are found in the inclusion of the lib...
 *
 * Our strategy is to include directly the *.c, rather than compile
 * them separatly. This allow to simplify compilation and hide the
 * multicall tweaks from the other tools.
 * Yeah, this leads to a bit a preprocessor abuse...
 * Jean II
 */

/* We need the library */
#include "iwlib.c"

/* Get iwconfig in there. Mandatory. */
#define main(args...) main_iwconfig(args)
#define iw_usage(args...) iwconfig_usage(args)
#define find_command(args...) iwconfig_find_command(args)
#include "iwconfig.c"
#undef find_command
#undef iw_usage
#undef main

/* Get iwlist in there. Scanning support is pretty sweet. */
#define main(args...) main_iwlist(args)
#define iw_usage(args...) iwlist_usage(args)
#define find_command(args...) iwlist_find_command(args)
#include "iwlist.c"
#undef find_command
#undef iw_usage
#undef main

#ifndef WE_ESSENTIAL
/* Get iwspy in there, it's not that big. */
#define main(args...) main_iwspy(args)
#include "iwspy.c"
#undef main
#endif	/* WE_ESSENTIAL */

/* Get iwpriv in there. Mandatory for HostAP and some other drivers. */
#define main(args...) main_iwpriv(args)
#define iw_usage(args...) iwpriv_usage(args)
#include "iwpriv.c"
#undef iw_usage
#undef main

/* Do we really need iwgetid ? Well, it's not like it's a big one */
#define main(args...) main_iwgetid(args)
#define iw_usage(args...) iwgetid_usage(args)
#include "iwgetid.c"
#undef iw_usage
#undef main

/* iwevent is useless for most people, don't grab it ? */

/* ifrename is big and useless for those systems */


/******************************* MAIN ********************************/

/*------------------------------------------------------------------*/
/*
 * The main !
 */
int
main(int	argc,
     char **	argv)
{
  char *	call_name = basename(argv[0]);	/* Strip path */

  /* This is a testing hack */
  if(!strcmp(call_name, "iwmulticall") && (argc > 0))
    {
      argv++;
      argc--;
      call_name = basename(argv[0]);
    }

  /* Just check the name under which we were called... */

  if(!strcmp(call_name, "iwconfig"))
    return(main_iwconfig(argc, argv));
  if(!strcmp(call_name, "iwlist"))
    return(main_iwlist(argc, argv));
#ifndef WE_ESSENTIAL
  if(!strcmp(call_name, "iwspy"))
    return(main_iwspy(argc, argv));
#endif	/* WE_ESSENTIAL */
  if(!strcmp(call_name, "iwpriv"))
    return(main_iwpriv(argc, argv));
  if(!strcmp(call_name, "iwgetid"))
    return(main_iwgetid(argc, argv));

  /* Uh oh... Not supposed to come here. */
  printf("iwmulticall : you are not supposed to call me this way...\n");
  return(0);
}
