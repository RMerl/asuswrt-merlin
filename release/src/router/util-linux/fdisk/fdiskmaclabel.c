/*
  Changes:
  Sat Mar 20 09:51:38 EST 1999 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
	Internationalization
*/
#include <stdio.h>              /* stderr */
#include <string.h>             /* strstr */
#include <unistd.h>             /* write */

#include <endian.h>

#include "common.h"
#include "fdisk.h"
#include "fdiskmaclabel.h"
#include "nls.h"

#define MAC_BITMASK 0xffff0000


static	int     other_endian = 0;
static  short	volumes=1;

/*
 * only dealing with free blocks here
 */

static void
mac_info( void ) {
    puts(
	_("\n\tThere is a valid Mac label on this disk.\n"
	"\tUnfortunately fdisk(1) cannot handle these disks.\n"
	"\tUse either pdisk or parted to modify the partition table.\n"
	"\tNevertheless some advice:\n"
	"\t1. fdisk will destroy its contents on write.\n"
	"\t2. Be sure that this disk is NOT a still vital\n"
	"\t   part of a volume group. (Otherwise you may\n"
	"\t   erase the other disks as well, if unmirrored.)\n")
    );
}

void
mac_nolabel( void )
{
    maclabel->magic = 0;
    partitions = 4;
    zeroize_mbr_buffer();
    return;
}

int
check_mac_label( void )
{
	/*
	Conversion: only 16 bit should compared
	e.g.: HFS Label is only 16bit long
	*/
	int magic_masked = 0 ;
	magic_masked =  maclabel->magic & MAC_BITMASK ;

	switch (magic_masked) {
		case MAC_LABEL_MAGIC :
		case MAC_LABEL_MAGIC_2:
		case MAC_LABEL_MAGIC_3:
			goto IS_MAC;
			break;
		default:
			other_endian = 0;
			return 0;


	}

IS_MAC:
    other_endian = (maclabel->magic == MAC_LABEL_MAGIC_SWAPPED); // =?
    update_units();
    disklabel = MAC_LABEL;
    partitions= 1016; // =?
    volumes = 15;	// =?
    mac_info();
    mac_nolabel();		/* %% */
    return 1;
}

