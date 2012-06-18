/* ebtable_filter
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2002
 */

#include <stdio.h>
#include "../include/ebtables_u.h"

#define FILTER_VALID_HOOKS ((1 << NF_BR_LOCAL_IN) | (1 << NF_BR_FORWARD) | \
   (1 << NF_BR_LOCAL_OUT))

static void print_help(const char **hn)
{
	int i;

	printf("Supported chains for the filter table:\n");
	for (i = 0; i < NF_BR_NUMHOOKS; i++)
		if (FILTER_VALID_HOOKS & (1 << i))
			printf("%s ", hn[i]);
	printf("\n");
}

static struct ebt_u_table table =
{
	.name		= "filter",
	.help		= print_help,
};

void _init(void)
{
	ebt_register_table(&table);
}
