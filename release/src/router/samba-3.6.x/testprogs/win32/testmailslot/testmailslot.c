/*
 * Very simple test application for mailslots
 * (C) 2005 Jelmer Vernooij <jelmer@samba.org>
 * Published to the public domain
 */

#include <windows.h>
#include <stdio.h>

int read_slot(const char *mailslotname)
{
	HANDLE h;
	DWORD nr;
	char data[30000];
	DWORD nextsize, nummsg = 0;

	if (strncmp(mailslotname, "\\\\.\\mailslot\\", 13) && strncmp(mailslotname, "\\\\*\\mailslot\\", 13)) {
		printf("Must specify local mailslot name (starting with \\\\.\\mailslot\\)\n");
		return 1;
	}

	h = CreateMailslot(mailslotname, 0, MAILSLOT_WAIT_FOREVER, NULL);

	if (h == INVALID_HANDLE_VALUE) {
		printf("Unable to create mailslot %s: %d\n", mailslotname, GetLastError());
		return 1;
	}

	if (!ReadFile(h, data, sizeof(data)-1, &nr, NULL)) {
		printf("Error reading: %d\n", GetLastError());
		return 1;
	}

	data[nr] = '\0';

	printf("%s\n", data);

	CloseHandle(h);
}

int write_slot(const char *mailslotname)
{
	HANDLE h;
	DWORD nw;
	char data[30000];
	h = CreateFile(mailslotname, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL);

	if (h == INVALID_HANDLE_VALUE) {
		printf("Unable to open file: %d\n", GetLastError());
		return 1;
	}

	gets(data);

	if (!WriteFile(h, data, strlen(data), &nw, NULL)) {
		printf("Error writing file: %d\n", GetLastError());
		return 1;
	}

	CloseHandle(h);	
}

int main(int argc, char **argv)
{
	if (argc < 3 || 
			(strcmp(argv[1], "read") && strcmp(argv[1], "write"))) {
		printf("Usage: %s read|write mailslot\n", argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "read")) {
		return read_slot(argv[2]);
	}

	if (!strcmp(argv[1], "write")) {
		return write_slot(argv[2]);
	}

	return 0;
}
