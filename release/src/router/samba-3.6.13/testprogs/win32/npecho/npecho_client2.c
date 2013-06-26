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
	char *outbuffer = malloc(sizeof(ECHODATA)*2);
	BOOL small_reads = FALSE;
	DWORD state = 0;
	DWORD flags = 0;

	if (argc == 1) {
		goto usage;
	} else if (argc >= 3) {
		if (strcmp(argv[2], "large") == 0) {
			small_reads = FALSE;
		} else if (strcmp(argv[2], "small") == 0) {
			small_reads = TRUE;
		} else {
			goto usage;
		}
	}

	h = CreateFile(argv[1], GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		printf("Error opening: %d\n", GetLastError());
		return -1;
	}

	GetNamedPipeHandleState(h, &state, NULL, NULL, NULL, NULL, 0);

	if (state & PIPE_READMODE_MESSAGE) {
		printf("message read mode\n");
	} else {
		printf("byte read mode\n");
	}

	Sleep(5000);

	if (small_reads) {
		DWORD ofs, size, nread;
		const char *more = "";
		printf("small reads\n");
		numread = 0;
		ofs = 0;
		size = sizeof(ECHODATA)/2;
		if (ReadFile(h, outbuffer+ofs, size, &nread, NULL)) {
			if (state & PIPE_READMODE_MESSAGE) {
				printf("Error message mode small read succeeded\n");
				return -1;
			}
		} else if (GetLastError() == ERROR_MORE_DATA) {
			if (!(state & PIPE_READMODE_MESSAGE)) {
				printf("Error byte mode small read returned ERROR_MORE_DATA\n");
				return -1;
			}
			more = " more data";
		} else {
			printf("Error reading: %d\n", GetLastError());
			return -1;
		}
		printf("Small Read: %d%s\n", nread, more);
		numread += nread;
		ofs = size;
		size = sizeof(ECHODATA) - ofs;
		if (!ReadFile(h, outbuffer+ofs, size, &nread, NULL)) {
			printf("Error reading: %d\n", GetLastError());
			return -1;
		}
		printf("Small Read: %d\n", nread);
		numread += nread;
	} else {
		printf("large read\n");
		if (!ReadFile(h, outbuffer, sizeof(ECHODATA)*2, &numread, NULL)) {
			printf("Error reading: %d\n", GetLastError());
			return -1;
		}
	}
	printf("Read: %s %d\n", outbuffer, numread);
	if (state & PIPE_READMODE_MESSAGE) {
		if (numread != sizeof(ECHODATA)) {
			printf("message mode returned %d bytes should be %s\n",
			       numread, sizeof(ECHODATA));
			return -1;
		}
	} else {
		if (numread != (sizeof(ECHODATA)*2)) {
			printf("message mode returned %d bytes should be %s\n",
			       numread, (sizeof(ECHODATA)*2));
			return -1;
		}
	}

	if (!ReadFile(h, outbuffer, sizeof(ECHODATA)*2, &numread, NULL)) {
		printf("Error reading: %d\n", GetLastError());
		return -1;
	}

	printf("Read: %s %d\n", outbuffer, numread);

	return 0;
usage:
	printf("Usage: %s pipename [read]\n", argv[0]);
	printf("  Where pipename is something like \\\\servername\\NPECHO\n");
	printf("  Where read is something 'large' or 'small'\n");
	return -1;
}
