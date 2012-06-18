#include <string.h>
#include "include/ebtables_u.h"

static struct ebt_u_replace replace;
void ebt_early_init_once();

int main(int argc, char *argv[])
{
	ebt_silent = 0;
	ebt_early_init_once();
	strcpy(replace.name, "filter");
	do_command(argc, argv, EXEC_STYLE_PRG, &replace);
	return 0;
}
