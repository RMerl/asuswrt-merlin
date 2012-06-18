/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>

#include <bcmnvram.h>
#include "shutils.h"
#include "shared.h"


/* Transfer Char to ASCII */
void char_to_ascii(char *output, char *input)
{
	int i;
	char *ptr;

	ptr = output;

	for ( i=0; i<strlen(input); i++ )
	{
		if ( input[i] == '[' || input[i] == ']' ) {
			*ptr = '\\';
			ptr ++;
			*ptr = input[i];
			ptr ++;
		}
		else if ( (input[i] >='0' && input[i] <='9')
			||(input[i] >='A' && input[i] <='Z')
			||(input[i] >='a' && input[i] <='z'))
		{
			*ptr = input[i];
			ptr ++;
		}
		else
		{
			sprintf(ptr, "%%%.02X", input[i]);
			ptr += strlen(ptr);
		}
	}
	*(ptr) = '\0';
}

const char *find_word(const char *buffer, const char *word)
{
	const char *p, *q;
	int n;

	n = strlen(word);
	p = buffer;
	while ((p = strstr(p, word)) != NULL) {
		if ((p == buffer) || (*(p - 1) == ' ') || (*(p - 1) == ',')) {
			q = p + n;
			if ((*q == ' ') || (*q == ',') || (*q == 0)) {
				return p;
			}
		}
		++p;
	}
	return NULL;
}

/*
static void add_word(char *buffer, const char *word, int max)
{
	if ((*buffer != 0) && (buffer[strlen(buffer) - 1] != ' '))
		strlcat(buffer, " ", max);
	strlcat(buffer, word, max);
}
*/

int remove_word(char *buffer, const char *word)
{
	char *p;
	char *q;

	if ((p = strstr(buffer, word)) == NULL) return 0;
	q = p;
	p += strlen(word);
	while (*p == ' ') ++p;
	while ((q > buffer) && (*(q - 1) == ' ')) --q;
	if (*q != 0) *q++ = ' ';
	strcpy(q, p);

	return 1;
}
