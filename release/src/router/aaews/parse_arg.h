#ifndef _PARSE_ARG_H
#define _PARSE_ARG_H
#include <stdlib.h>

#define PATH_LEN	128
#define ID_MAX_LEN	64
#define PWD_MAX_LEN	64
#define URL_MAX_LEN	128
#define FLAG_LEN	2
#define PORT_LEN 	8

int 	parse_arg(int argc, char* argv[] );
int 	get_arg_field(char* field, char* ret_val );
void 	dump_arg();
#endif
