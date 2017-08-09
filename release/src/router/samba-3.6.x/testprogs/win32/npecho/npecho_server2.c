/*
 * Simple Named Pipe Client
 * (C) 2005 Jelmer Vernooij <jelmer@samba.org>
 * (C) 2009 Stefan Metzmacher <metze@samba.org>
 * Published to the public domain
 */

#include <windows.h>
#include <stdio.h>

#define ECHODATA "Black Dog"

int main(int argc, char *argv[])
{
	HANDLE h;
	DWORD numread = 0;
	char *outbuffer = malloc(sizeof(ECHODATA));
	BOOL msgmode = FALSE;
	DWORD type = 0;

	if (argc == 1) {
		goto usage;
	} else if (argc >= 3) {
		if (strcmp(argv[2], "byte") == 0) {
			msgmode = FALSE;
		} else if (strcmp(argv[2], "message") == 0) {
			msgmode = TRUE;
		} else {
			goto usage;
		}
	}

	if (msgmode == TRUE) {
		printf("using message mode\n");
		type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
	} else {
		printf("using byte mode\n");
		type = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
	}

	h = CreateNamedPipe(argv[1],
			    PIPE_ACCESS_DUPLEX,
			    type,
			    PIPE_UNLIMITED_INSTANCES,
			    1024,
			    1024,
			    0,
			    NULL);
	if (h == INVALID_HANDLE_VALUE) {
		printf("Error opening: %d\n", GetLastError());
		return -1;
	}

	ConnectNamedPipe(h, NULL);

	if (!WriteFile(h, ECHODATA, sizeof(ECHODATA), &numread, NULL)) {
		printf("Error writing: %d\n", GetLastError());
		return -1;
	}

	if (!WriteFile(h, ECHODATA, sizeof(ECHODATA), &numread, NULL)) {
		printf("Error writing: %d\n", GetLastError());
		return -1;
	}

	FlushFileBuffers(h);
	DisconnectNamedPipe(h);
	CloseHandle(h);

	return 0;
usage:
	printf("Usage: %s pipename [mode]\n", argv[0]);
	printf("  Where pipename is something like \\\\servername\\PIPE\\NPECHO\n");
	printf("  Where mode is 'byte' or 'message'\n");
	return -1;
}
