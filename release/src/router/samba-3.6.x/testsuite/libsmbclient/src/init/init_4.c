#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libsmbclient.h>


int main( )
{
	int err = -1;

	err = smbc_init(NULL, 0);

	err = errno;

	return err;

}
