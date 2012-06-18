/* ebtable_nat
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2002
 */

#include <stdio.h>
#include "../include/ebtables_u.h"

#define NAT_VALID_HOOKS ((1 << NF_BR_PRE_ROUTING) | (1 << NF_BR_LOCAL_OUT) | \
   (1 << NF_BR_POST_ROUTING))

static void print_help(const char **hn)
{
	int i;

	printf("Supported chains for the nat table:\n");
	for (i = 0; i < NF_BR_NUMHOOKS; i++)
		if (NAT_VALID_HOOKS & (1 << i))
			printf("%s ", hn[i]);
	printf("\n");
}

static struct
ebt_u_table table =
{
	.name		= "nat",
	.help		= print_help,
};

void _init(void)
{
	ebt_register_table(&table);
}
