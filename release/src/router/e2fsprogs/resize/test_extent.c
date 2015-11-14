/*
 * test_extent.c --- tester for the extent abstraction
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000 by Theosore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include "resize2fs.h"

void do_test(FILE *in, FILE *out);

void do_test(FILE *in, FILE *out)
{
	char		buf[128];
	char		*cp, *cmd, *arg1, *arg2;
	__u64		num1, num2;
	__u64		size;
	errcode_t	retval;
	ext2_extent	extent = 0;
	const char	*no_table = "# No extent table\n";

	while (!feof(in)) {
		if (!fgets(buf, sizeof(buf), in))
			break;
		/*
		 * Ignore comments
		 */
		if (buf[0] =='#')
			continue;

		/*
		 * Echo command
		 */
		fputs(buf, out);

		cp = strchr(buf, '\n');
		if (cp)
			*cp = '\0';

		/*
		 * Parse command line; simple, at most two arguments
		 */
		cmd = buf;
		num1 = num2 = 0;
		arg1 = arg2 = 0;
		cp = strchr(buf, ' ');
		if (cp) {
			*cp++ = '\0';
			arg1 = cp;
			num1 = strtoul(arg1, 0, 0);

			cp = strchr(cp, ' ');
		}
		if (cp) {
			*cp++ = '\0';
			arg2 = cp;
			num2 = strtoul(arg2, 0, 0);
		}

		if (!strcmp(cmd, "create")) {
			retval = ext2fs_create_extent_table(&extent, num1);
			if (retval) {
			handle_error:
				fprintf(out, "# Error: %s\n",
					error_message(retval));
				continue;
			}
			continue;
		}
		if (!extent) {
			fputs(no_table, out);
			continue;
		}
		if (!strcmp(cmd, "free")) {
			ext2fs_free_extent_table(extent);
			extent = 0;
		} else if (!strcmp(cmd, "add")) {
			retval = ext2fs_add_extent_entry(extent, num1, num2);
			if (retval)
				goto handle_error;
		} else if (!strcmp(cmd, "lookup")) {
			num2 = ext2fs_extent_translate(extent, num1);
			fprintf(out, "# Answer: %llu%s\n", num2,
				num2 ? "" : " (not found)");
		} else if (!strcmp(cmd, "dump")) {
			ext2fs_extent_dump(extent, out);
		} else if (!strcmp(cmd, "iter_test")) {
			retval = ext2fs_iterate_extent(extent, 0, 0, 0);
			if (retval)
				goto handle_error;
			while (1) {
				retval = ext2fs_iterate_extent(extent,
					       &num1, &num2, &size);
				if (retval)
					goto handle_error;
				if (!size)
					break;
				fprintf(out, "# %llu -> %llu (%llu)\n",
					num1, num2, size);
			}
		} else
			fputs("# Syntax error\n", out);
	}
	if (extent)
		ext2fs_free_extent_table(extent);
}

#ifdef __GNUC__
#define ATTR(x) __attribute__(x)
#else
#define ATTR(x)
#endif

int main(int argc ATTR((unused)), char **argv ATTR((unused)))
{
	do_test(stdin, stdout);
	exit(0);
}
