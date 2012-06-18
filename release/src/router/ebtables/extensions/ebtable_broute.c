/* ebtable_broute
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2002
 */

#include <stdio.h>
#include "../include/ebtables_u.h"


static void print_help(const char **hn)
{
	printf("Supported chain for the broute table:\n");
	printf("%s\n",hn[NF_BR_BROUTING]);
}

static struct
ebt_u_table table =
{
	.name		= "broute",
	.help		= print_help,
};

void _init(void)
{
	ebt_register_table(&table);
}
