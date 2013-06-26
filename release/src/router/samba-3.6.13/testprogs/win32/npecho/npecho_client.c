/*
 * Simple Named Pipe Client
 * (C) 2005 Jelmer Vernooij <jelmer@samba.org>
 * Published to the public domain
 */

#include <windows.h>
#include <stdio.h>

#define ECHODATA "Black Dog"

int main(int argc, char *argv[])
{
	HANDLE h;
	DWORD numread = 0;
	char *outbuffer = malloc(strlen(ECHODATA));

	if (argc == 1) {
		printf("Usage: %s pipename\n", argv[0]);
		printf("  Where pipename is something like \\\\servername\\NPECHO\n");
		return -1;
	}

	h = CreateFile(argv[1], GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		printf("Error opening: %d\n", GetLastError());
		return -1;
	}

	if (!WriteFile(h, ECHODATA, strlen(ECHODATA), &numread, NULL)) {
		printf("Error writing: %d\n", GetLastError());
		return -1;
	}

	if (!ReadFile(h, outbuffer, strlen(ECHODATA), &numread, NULL)) {
		printf("Error reading: %d\n", GetLastError());
		return -1;
	}

	printf("Read: %s\n", outbuffer);
	
	if (!TransactNamedPipe(h, ECHODATA, strlen(ECHODATA), outbuffer, strlen(ECHODATA), &numread, NULL)) {
		printf("TransactNamedPipe failed: %d!\n", GetLastError());
		return -1;
	}

	printf("Result: %s\n", outbuffer);

	return 0;
}
