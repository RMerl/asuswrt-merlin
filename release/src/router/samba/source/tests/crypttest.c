#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

main()
{
	char passwd[9];
	char salt[9];
	char c_out1[256];
	char c_out2[256];

	char expected_out[14];

	strcpy(expected_out, "12yJ.Of/NQ.Pk");
	strcpy(passwd, "12345678");
	strcpy(salt, "12345678");
	
	strcpy(c_out1, crypt(passwd, salt));
	salt[2] = '\0';
	strcpy(c_out2, crypt(passwd, salt));

	/*
	 * If the non-trucated salt fails but the
	 * truncated salt succeeds then exit 1.
	 */

	if((strcmp(c_out1, expected_out) != 0) && 
		(strcmp(c_out2, expected_out) == 0))
		exit(1);

#ifdef HAVE_BIGCRYPT
	/*
	 * Try the same with bigcrypt...
	 */

	{
		char big_passwd[17];
		char big_salt[17];
		char big_c_out1[256];
		char big_c_out2[256];
		char big_expected_out[27];

		strcpy(big_passwd, "1234567812345678");
		strcpy(big_salt, "1234567812345678");
		strcpy(big_expected_out, "12yJ.Of/NQ.PklfyCuHi/rwM");

		strcpy(big_c_out1, bigcrypt(big_passwd, big_salt));
		big_salt[2] = '\0';
		strcpy(big_c_out2, bigcrypt(big_passwd, big_salt));

		/*
		 * If the non-trucated salt fails but the
		 * truncated salt succeeds then exit 1.
		 */

		if((strcmp(big_c_out1, big_expected_out) != 0) && 
			(strcmp(big_c_out2, big_expected_out) == 0))
			exit(1);

	}
#endif

	exit(0);
}
