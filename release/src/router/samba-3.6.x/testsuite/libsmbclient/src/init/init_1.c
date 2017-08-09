#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libsmbclient.h>


int main()
{
	int err = -1;

	err = smbc_init(NULL, 0);

	if ( err < 0 )
		err = 1;

	return err;

}
