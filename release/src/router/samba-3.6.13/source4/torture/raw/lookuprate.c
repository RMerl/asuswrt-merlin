/*
   File lookup rate test.

   Copyright (C) James Peach 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "torture/smbtorture.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\lookuprate"
#define MISSINGNAME BASEDIR "\\foo"

#define FUZZ_PERCENT 10

#define usec_to_sec(s) ((s) / 1000000)
#define sec_to_usec(s) ((s) * 1000000)

struct rate_record
{
    unsigned	dirent_count;
    unsigned	querypath_persec;
    unsigned	findfirst_persec;
};

static struct rate_record records[] =
{
    { 0, 0, 0 },	/* Base (optimal) lookup rate. */
    { 100, 0, 0},
    { 1000, 0, 0},
    { 10000, 0, 0},
    { 100000, 0, 0}
};

typedef NTSTATUS lookup_function(struct smbcli_tree *tree, const char * path);

/* Test whether rhs is within fuzz% of lhs. */
static bool fuzzily_equal(unsigned lhs, unsigned rhs, int percent)
{
	double fuzz = (double)lhs * (double)percent/100.0;

	if (((double)rhs >= ((double)lhs - fuzz)) &&
	    ((double)rhs <= ((double)lhs + fuzz))) {
		return true;
	}

	return false;

}

static NTSTATUS fill_directory(struct smbcli_tree *tree,
	    const char * path, unsigned count)
{
	NTSTATUS	status;
	char		*fname = NULL;
	unsigned	i;
	unsigned	current;

	struct timeval start;
	struct timeval now;

	status = smbcli_mkdir(tree, path);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	printf("filling directory %s with %u files... ", path, count);
	fflush(stdout);

	current = random();
	start = timeval_current();

	for (i = 0; i < count; ++i) {
		int fnum;

		++current;
		fname = talloc_asprintf(NULL, "%s\\fill%u",
				    path, current);

		fnum = smbcli_open(tree, fname, O_RDONLY|O_CREAT,
				DENY_NONE);
		if (fnum < 0) {
			talloc_free(fname);
			return smbcli_nt_error(tree);
		}

		smbcli_close(tree, fnum);
		talloc_free(fname);
	}

	if (count) {
		double rate;
		now = timeval_current();
		rate = (double)count / usec_to_sec((double)usec_time_diff(&now, &start));
		printf("%u/sec\n", (unsigned)rate);
	} else {
		printf("done\n");
	}

	return NT_STATUS_OK;
}

static NTSTATUS squash_lookup_error(NTSTATUS status)
{
	if (NT_STATUS_IS_OK(status)) {
		return NT_STATUS_OK;
	}

	/* We don't care if the file isn't there. */
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_PATH_NOT_FOUND)) {
		return NT_STATUS_OK;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		return NT_STATUS_OK;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_FILE)) {
		return NT_STATUS_OK;
	}

	return status;
}

/* Look up a pathname using TRANS2_QUERY_PATH_INFORMATION. */
static NTSTATUS querypath_lookup(struct smbcli_tree *tree, const char * path)
{
	NTSTATUS	status;
	time_t		ftimes[3];
	size_t		fsize;
	uint16_t	fmode;

	status = smbcli_qpathinfo(tree, path, &ftimes[0], &ftimes[1], &ftimes[2],
			&fsize, &fmode);

	return squash_lookup_error(status);
}

/* Look up a pathname using TRANS2_FIND_FIRST2. */
static NTSTATUS findfirst_lookup(struct smbcli_tree *tree, const char * path)
{
	NTSTATUS status = NT_STATUS_OK;

	if (smbcli_list(tree, path, 0, NULL, NULL) < 0) {
		status = smbcli_nt_error(tree);
	}

	return squash_lookup_error(status);
}

static NTSTATUS lookup_rate_convert(struct smbcli_tree *tree,
	lookup_function lookup, const char * path, unsigned * rate)
{
	NTSTATUS	status;

	struct timeval	start;
	struct timeval	now;
	unsigned	count = 0;
	int64_t		elapsed = 0;

#define LOOKUP_PERIOD_SEC (2)

	start = timeval_current();
	while (elapsed < sec_to_usec(LOOKUP_PERIOD_SEC)) {

		status = lookup(tree, path);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		++count;
		now = timeval_current();
		elapsed = usec_time_diff(&now, &start);
	}

#undef LOOKUP_PERIOD_SEC

	*rate = (unsigned)((double)count / (double)usec_to_sec(elapsed));
	return NT_STATUS_OK;
}

static bool remove_working_directory(struct smbcli_tree *tree,
		const char * path)
{
	int tries;

	/* Using smbcli_deltree to delete a very large number of files
	 * doesn't work against all servers. Work around this by
	 * retrying.
	 */
	for (tries = 0; tries < 5; ) {
		int ret;

		ret = smbcli_deltree(tree, BASEDIR);
		if (ret == -1) {
			tries++;
			printf("(%s) failed to deltree %s: %s\n",
				__location__, BASEDIR,
				smbcli_errstr(tree));
			continue;
		}

		return true;
	}

	return false;

}

/* Verify that looking up a file name takes constant time.
 *
 * This test samples the lookup rate for a non-existant filename in a
 * directory, while varying the number of files in the directory. The
 * lookup rate should continue to approximate the lookup rate for the
 * empty directory case.
 */
bool torture_bench_lookup(struct torture_context *torture)
{
	NTSTATUS	status;
	bool		result = false;

	int i;
	struct smbcli_state *cli = NULL;

	if (!torture_open_connection(&cli, torture, 0)) {
		goto done;
	}

	remove_working_directory(cli->tree, BASEDIR);

	for (i = 0; i < ARRAY_SIZE(records); ++i) {
		printf("Testing lookup rate with %u directory entries\n",
				records[i].dirent_count);

		status = fill_directory(cli->tree, BASEDIR,
				records[i].dirent_count);
		if (!NT_STATUS_IS_OK(status)) {
			printf("failed to fill directory: %s\n", nt_errstr(status));
			goto done;
		}

		status = lookup_rate_convert(cli->tree, querypath_lookup,
			MISSINGNAME, &records[i].querypath_persec);
		if (!NT_STATUS_IS_OK(status)) {
			printf("querypathinfo of %s failed: %s\n",
				MISSINGNAME, nt_errstr(status));
			goto done;
		}

		status = lookup_rate_convert(cli->tree, findfirst_lookup,
			MISSINGNAME, &records[i].findfirst_persec);
		if (!NT_STATUS_IS_OK(status)) {
			printf("findfirst of %s failed: %s\n",
				MISSINGNAME, nt_errstr(status));
			goto done;
		}

		printf("entries = %u, querypath = %u/sec, findfirst = %u/sec\n",
				records[i].dirent_count,
				records[i].querypath_persec,
				records[i].findfirst_persec);

		if (!remove_working_directory(cli->tree, BASEDIR)) {
			goto done;
		}
	}

	/* Ok. We have run all our tests. Walk through the records we
	 * accumulated and figure out whether the lookups took constant
	 * time of not.
	 */
	for (i = 0; i < ARRAY_SIZE(records); ++i) {
		if (!fuzzily_equal(records[0].querypath_persec,
				    records[i].querypath_persec,
				    FUZZ_PERCENT)) {
			printf("querypath rate for %d entries differed by "
				"more than %d%% from base rate\n",
				records[i].dirent_count, FUZZ_PERCENT);
			result = false;
		}

		if (!fuzzily_equal(records[0].findfirst_persec,
				    records[i].findfirst_persec,
				    FUZZ_PERCENT)) {
			printf("findfirst rate for %d entries differed by "
				"more than %d%% from base rate\n",
				records[i].dirent_count, FUZZ_PERCENT);
			result = false;
		}
	}

done:
	if (cli) {
		remove_working_directory(cli->tree, BASEDIR);
		talloc_free(cli);
	}

	return result;
}

/* vim: set sts=8 sw=8 : */
