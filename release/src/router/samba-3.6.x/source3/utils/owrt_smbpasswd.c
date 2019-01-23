/*
 * Copyright (C) 2012 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2008 John Crispin <blogic@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.  */

#include "includes.h"
#include <endian.h>
#include <stdio.h>
#include <sys/stat.h>

static char buf[256];

static const char smbpasswd[] = "/etc/samba/smbpasswd";

static void md4hash(const char *passwd, uchar p16[16])
{
	int len;
	smb_ucs2_t wpwd[129];
	int i;

	len = strlen(passwd);
	for (i = 0; i < len; i++) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		wpwd[i] = (unsigned char)passwd[i];
#else
		wpwd[i] = (unsigned char)passwd[i] << 8;
#endif
	}
	wpwd[i] = 0;

	len = len * sizeof(int16);
	mdfour(p16, (unsigned char *)wpwd, len);
	ZERO_STRUCT(wpwd);
}


static bool find_passwd_line(FILE *fp, const char *user, char **next)
{
	char *p1;

	while (!feof(fp)) {
		if(!fgets(buf, sizeof(buf) - 1, fp))
			continue;

		p1 = strchr(buf, ':');

		if (p1 - buf != strlen(user))
			continue;

		if (strncmp(buf, user, p1 - buf) != 0)
			continue;

		if (next)
			*next = p1;
		return true;
	}
	return false;
}

/* returns -1 if user is not present in /etc/passwd*/
static int find_uid_for_user(const char *user)
{
	FILE *fp;
	char *p1, *p2, *p3;
	int ret = -1;

	fp = fopen("/etc/passwd", "r");
	if (!fp) {
		printf("failed to open /etc/passwd");
		goto out;
	}

	if (!find_passwd_line(fp, user, &p1)) {
		printf("User %s not found or invalid in /etc/passwd\n", user);
		goto out;
	}

	p2 = strchr(p1 + 1, ':');
	if (!p2)
		goto out;

	p2++;
	p3 = strchr(p2, ':');
	if (!p1)
		goto out;

	*p3 = '\0';
	ret = atoi(p2);

out:
	if(fp)
		fclose(fp);
	return ret;
}

static void smbpasswd_write_user(FILE *fp, const char *user, int uid, const char *password)
{
	static uchar nt_p16[NT_HASH_LEN];
	int len = 0;
	int i;

	md4hash(strdup(password), nt_p16);

	len += snprintf(buf + len, sizeof(buf) - len, "%s:%u:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:", user, uid);
	for(i = 0; i < NT_HASH_LEN; i++)
		len += snprintf(buf + len, sizeof(buf) - len, "%02X", nt_p16[i]);

	snprintf(buf + len, sizeof(buf) - len, ":[U          ]:LCT-00000001:\n");
	fputs(buf, fp);
}

static void smbpasswd_delete_user(FILE *fp)
{
	fpos_t r_pos, w_pos;
	int len = strlen(buf);

	fgetpos(fp, &r_pos);
	fseek(fp, -len, SEEK_CUR);
	fgetpos(fp, &w_pos);
	fsetpos(fp, &r_pos);

	while (fgets(buf, sizeof(buf) - 1, fp)) {
		int cur_len = strlen(buf);

		fsetpos(fp, &w_pos);
		fputs(buf, fp);
		fgetpos(fp, &w_pos);

		fsetpos(fp, &r_pos);
		fseek(fp, cur_len, SEEK_CUR);
		fgetpos(fp, &r_pos);
	}

	fsetpos(fp, &w_pos);
	ftruncate(fileno(fp), ftello(fp));
}

static int usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [options] <username>\n"
		"\n"
		"Options:\n"
		"  -s		read password from stdin\n"
		"  -a		add user\n"
		"  -x		delete user\n",
		progname);
	return 1;
}

int main(int argc, char **argv)
{
	const char *prog = argv[0];
	const char *user;
	char *pswd;
	FILE *fp;
	bool add = false, delete = false, get_stdin = false, found;
	int ch;
	int uid;
	struct stat st;

	TALLOC_CTX *frame = talloc_stackframe();

	if(argc != 3)
	{
		printf("usage for smbpasswd - \n\t%s USERNAME PASSWD\n\t%s -del USERNAME\n", argv[0], argv[0]);
		exit(1);
	}

	if(strcmp(argv[1], "-del") == 0) {
		delete = true;
		user = argv[2];
		uid = find_uid_for_user(user);
		if (uid < 0) {
			fprintf(stderr, "Could not find user '%s' in /etc/passwd\n", user);
			return 2;
		}
	} else {
		add = true;
		user = argv[1];
	}

	fp = fopen(smbpasswd, (stat(smbpasswd, &st) == 0) && (!S_ISDIR(st.st_mode)) ? "r+" : "w+");
	if(!fp) {
		fprintf(stderr, "Failed to open %s\n", smbpasswd);
		return 3;
	}

	found = find_passwd_line(fp, user, NULL);
	if (!add && !found) {
		fprintf(stderr, "Could not find user '%s' in %s\n", smbpasswd, user);
		return 3;
	}

	if (delete) {
		smbpasswd_delete_user(fp);
		goto out;
	}

	if (found)
		fseek(fp, -strlen(buf), SEEK_CUR);

	pswd = strdup(argv[2]);
	smbpasswd_write_user(fp, user, uid, pswd);
	free(pswd);
out:

	fclose(fp);
	TALLOC_FREE(frame);

	return 0;
}
