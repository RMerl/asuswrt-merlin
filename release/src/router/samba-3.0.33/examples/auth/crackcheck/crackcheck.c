#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <crack.h>

void usage(char *command) {
	char *c, *comm;

	comm = command;
	while ((c = strrchr(comm, '/')) != NULL) {
		comm = c + 1;
	}

	fprintf(stderr, "Usage: %s [-c] [-s] [-d <dictionary>]\n\n", comm);
	fprintf(stderr, "     -c    enables NT like complexity checks\n");
	fprintf(stderr, "     -d <dictionary file>    for cracklib\n");
	fprintf(stderr, "     -s    simple check use NT like checks ONLY\n\n");
	fprintf(stderr, "The password is read via stdin.\n\n");
	exit(-1);
}

int complexity(char* passwd)
{
	/* TG 26.10.2005
	 * check password for complexity like MS Windows NT 
	 */

	int c_upper = 0;
	int c_lower = 0;
	int c_digit = 0;
	int c_punct = 0;
	int c_tot = 0;
	int i, len;

	if (!passwd) goto fail;
	len = strlen(passwd);

	for (i = 0; i < len; i++) {

		if (c_tot >= 3) break;

		if (isupper(passwd[i])) {
			if (!c_upper) {
				c_upper = 1;
				c_tot += 1;
			}
			continue;
		}
		if (islower(passwd[i])) {
			if (!c_lower) {
				c_lower = 1;
				c_tot += 1;
			}
			continue;
		}
		if (isdigit(passwd[i])) {
			if (!c_digit) {
				c_digit = 1;
				c_tot += 1;
			}
			continue;
		}
		if (ispunct(passwd[i])) {
			if (!c_punct) {
				c_punct = 1;
				c_tot += 1;
			}
			continue;
		}
	}

	if ((c_tot) < 3) goto fail;
	return 0;

fail:
	fprintf(stderr, "ERR Complexity check failed\n\n");
	return -4;
}

int main(int argc, char **argv) {
	extern char *optarg;
	int c, ret, complex_check = 0, simplex_check = 0;

	char f[256];
	char *dictionary = NULL;
	char *password;
	char *reply;

	while ( (c = getopt(argc, argv, "d:cs")) != EOF){
		switch(c) {
		case 'd':
			dictionary = strdup(optarg);
			break;
		case 'c':
			complex_check = 1;
			break;
		case 's':
			complex_check = 1;
			simplex_check = 1;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (!simplex_check && dictionary == NULL) {
		fprintf(stderr, "ERR - Missing cracklib dictionary\n\n");
		usage(argv[0]);
	} 

	fflush(stdin);
	password = fgets(f, sizeof(f), stdin);

	if (password == NULL) {
		fprintf(stderr, "ERR - Failed to read password\n\n");
		exit(-2);
	}

	if (complex_check) {
		ret = complexity(password);
		if (ret) {
			exit(ret);
		}
	}

	if (simplex_check) {
		exit(0);
	}

	reply = FascistCheck(password, dictionary);
	if (reply != NULL) {
		fprintf(stderr, "ERR - %s\n\n", reply);
		exit(-3);
	}

	exit(0);
}

