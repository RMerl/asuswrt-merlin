#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libsmbclient.h>


int main(int argc, char **argv )
{
	int err = -1;

	if ( argc > 1 )
	{
		err = smbc_init(NULL, atoi(argv[1]));

		if ( err < 0 )
			err = 1;

	}

	return err;

}
