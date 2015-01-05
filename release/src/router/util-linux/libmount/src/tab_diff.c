/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: tabdiff
 * @title: Monitor mountinfo changes
 * @short_description: monitor changes in the list of the mounted filesystems
 */
#include "mountP.h"

struct tabdiff_entry {
	int	oper;			/* MNT_TABDIFF_* flags; */

	struct libmnt_fs *old_fs;	/* pointer to the old FS */
	struct libmnt_fs *new_fs;	/* pointer to the new FS */

	struct list_head changes;
};

struct libmnt_tabdiff {
	int nchanges;			/* number of changes */

	struct list_head changes;	/* list with modified entries */
	struct list_head unused;	/* list with unuused entries */
};

/**
 * mnt_new_tabdiff:
 *
 * Allocates a new table diff struct.
 *
 * Returns: new diff handler or NULL.
 */
struct libmnt_tabdiff *mnt_new_tabdiff(void)
{
	struct libmnt_tabdiff *df = calloc(1, sizeof(*df));

	if (!df)
		return NULL;

	DBG(DIFF, mnt_debug_h(df, "alloc"));

	INIT_LIST_HEAD(&df->changes);
	INIT_LIST_HEAD(&df->unused);
	return df;
}

static void free_tabdiff_entry(struct tabdiff_entry *de)
{
	if (!de)
		return;
	list_del(&de->changes);
	free(de);
}

/**
 * mnt_free_tabdiff:
 * @df: tab diff
 *
 * Deallocates tab diff struct and all entries.
 */
void mnt_free_tabdiff(struct libmnt_tabdiff *df)
{
	if (!df)
		return;

	DBG(DIFF, mnt_debug_h(df, "free"));

	while (!list_empty(&df->changes)) {
		struct tabdiff_entry *de = list_entry(df->changes.next,
			                  struct tabdiff_entry, changes);
		free_tabdiff_entry(de);
	}

	free(df);
}

/**
 * mnt_tabdiff_next_change:
 * @df: tabdiff pointer
 * @itr: iterator
 * @old_fs: returns the old entry or NULL if new entry added
 * @new_fs: returns the new entry or NULL if old entry removed
 * @oper: MNT_TABDIFF_{MOVE,UMOUNT,REMOUNT,MOUNT} flags
 *
 * The options @old_fs, @new_fs and @oper are optional.
 *
 * Returns: 0 on success, negative number in case of error or 1 at end of list.
 */
int mnt_tabdiff_next_change(struct libmnt_tabdiff *df, struct libmnt_iter *itr,
		struct libmnt_fs **old_fs, struct libmnt_fs **new_fs, int *oper)
{
	int rc = 1;
	struct tabdiff_entry *de = NULL;

	assert(df);
	assert(df);

	if (!df || !itr)
		return -EINVAL;

	if (!itr->head)
		MNT_ITER_INIT(itr, &df->changes);
	if (itr->p != itr->head) {
		MNT_ITER_ITERATE(itr, de, struct tabdiff_entry, changes);
		rc = 0;
	}

	if (old_fs)
		*old_fs = de ? de->old_fs : NULL;
	if (new_fs)
		*new_fs = de ? de->new_fs : NULL;
	if (oper)
		*oper = de ? de->oper : 0;

	return rc;
}

static int tabdiff_reset(struct libmnt_tabdiff *df)
{
	assert(df);

	DBG(DIFF, mnt_debug_h(df, "reseting"));

	/* zeroize all entries and move them to the list of unuused
	 */
	while (!list_empty(&df->changes)) {
		struct tabdiff_entry *de = list_entry(df->changes.next,
			                  struct tabdiff_entry, changes);

		list_del(&de->changes);
		list_add_tail(&de->changes, &df->unused);

		de->new_fs = de->old_fs = NULL;
		de->oper = 0;
	}

	df->nchanges = 0;
	return 0;
}

static int tabdiff_add_entry(struct libmnt_tabdiff *df, struct libmnt_fs *old,
			     struct libmnt_fs *new, int oper)
{
	struct tabdiff_entry *de;

	assert(df);

	DBG(DIFF, mnt_debug_h(df, "add change on %s",
				mnt_fs_get_target(new ? new : old)));

	if (!list_empty(&df->unused)) {
		de = list_entry(df->unused.next, struct tabdiff_entry, changes);
		list_del(&de->changes);
	} else {
		de = calloc(1, sizeof(*de));
		if (!de)
			return -ENOMEM;
	}

	INIT_LIST_HEAD(&de->changes);

	de->old_fs = old;
	de->new_fs = new;
	de->oper = oper;

	list_add_tail(&de->changes, &df->changes);
	df->nchanges++;
	return 0;
}

static struct tabdiff_entry *tabdiff_get_mount(struct libmnt_tabdiff *df,
					       const char *src,
					       int id)
{
	struct list_head *p;

	assert(df);

	list_for_each(p, &df->changes) {
		struct tabdiff_entry *de;

		de = list_entry(p, struct tabdiff_entry, changes);

		if (de->oper == MNT_TABDIFF_MOUNT && de->new_fs &&
		    mnt_fs_get_id(de->new_fs) == id) {

			const char *s = mnt_fs_get_source(de->new_fs);

			if (s == NULL && src == NULL)
				return de;
			if (s && src && strcmp(s, src) == 0)
				return de;
		}
	}
	return NULL;
}

/**
 * mnt_diff_tables:
 * @df: diff handler
 * @old_tab: old table
 * @new_tab: new table
 *
 * Compares @old_tab and @new_tab, the result is stored in @df and accessible by
 * mnt_tabdiff_next_change().
 *
 * Returns: number of changes, negative number in case of error.
 */
int mnt_diff_tables(struct libmnt_tabdiff *df, struct libmnt_table *old_tab,
		    struct libmnt_table *new_tab)
{
	struct libmnt_fs *fs;
	struct libmnt_iter itr;
	int no, nn;

	if (!df || !old_tab || !new_tab)
		return -EINVAL;

	tabdiff_reset(df);

	no = mnt_table_get_nents(old_tab);
	nn = mnt_table_get_nents(new_tab);

	if (!no && !nn)			/* both tables are empty */
		return 0;

	DBG(DIFF, mnt_debug_h(df, "analyze new=%p (%d entries), "
				          "old=%p (%d entries)",
				new_tab, nn, old_tab, no));

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	/* all mounted or umounted */
	if (!no && nn) {
		while(mnt_table_next_fs(new_tab, &itr, &fs) == 0)
			tabdiff_add_entry(df, NULL, fs, MNT_TABDIFF_MOUNT);
		goto done;

	} else if (no && !nn) {
		while(mnt_table_next_fs(old_tab, &itr, &fs) == 0)
			tabdiff_add_entry(df, fs, NULL, MNT_TABDIFF_UMOUNT);
		goto done;
	}

	/* search newly mounted or modified */
	while(mnt_table_next_fs(new_tab, &itr, &fs) == 0) {
		struct libmnt_fs *o_fs;
		const char *src = mnt_fs_get_source(fs),
			   *tgt = mnt_fs_get_target(fs);

		o_fs = mnt_table_find_pair(old_tab, src, tgt, MNT_ITER_FORWARD);
		if (!o_fs)
			/* 'fs' is not in the old table -- so newly mounted */
			tabdiff_add_entry(df, NULL, fs, MNT_TABDIFF_MOUNT);
		else {
			/* is modified? */
			const char *o1 = mnt_fs_get_options(o_fs),
				   *o2 = mnt_fs_get_options(fs);

			if (o1 && o2 && strcmp(o1, o2))
				tabdiff_add_entry(df, o_fs, fs, MNT_TABDIFF_REMOUNT);
		}
	}

	/* search umounted or moved */
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	while(mnt_table_next_fs(old_tab, &itr, &fs) == 0) {
		const char *src = mnt_fs_get_source(fs),
			   *tgt = mnt_fs_get_target(fs);

		if (!mnt_table_find_pair(new_tab, src, tgt, MNT_ITER_FORWARD)) {
			struct tabdiff_entry *de;

			de = tabdiff_get_mount(df, src,	mnt_fs_get_id(fs));
			if (de) {
				de->oper = MNT_TABDIFF_MOVE;
				de->old_fs = fs;
			} else
				tabdiff_add_entry(df, fs, NULL, MNT_TABDIFF_UMOUNT);
		}
	}
done:
	DBG(DIFF, mnt_debug_h(df, "%d changes detected", df->nchanges));
	return df->nchanges;
}

#ifdef TEST_PROGRAM

int test_diff(struct libmnt_test *ts, int argc, char *argv[])
{
	struct libmnt_table *tb_old = NULL, *tb_new = NULL;
	struct libmnt_tabdiff *diff = NULL;
	struct libmnt_iter *itr;
	struct libmnt_fs *old, *new;
	int rc = -1, change;

	tb_old = mnt_new_table_from_file(argv[1]);
	tb_new = mnt_new_table_from_file(argv[2]);
	diff = mnt_new_tabdiff();
	itr = mnt_new_iter(MNT_ITER_FORWARD);

	if (!tb_old || !tb_new || !diff || !itr) {
		warnx("failed to allocate resources");
		goto done;
	}

	rc = mnt_diff_tables(diff, tb_old, tb_new);
	if (rc < 0)
		goto done;

	while(mnt_tabdiff_next_change(diff, itr, &old, &new, &change) == 0) {

		printf("%s on %s: ", mnt_fs_get_source(new ? new : old),
				     mnt_fs_get_target(new ? new : old));

		switch(change) {
		case MNT_TABDIFF_MOVE:
			printf("MOVED to %s\n", mnt_fs_get_target(new));
			break;
		case MNT_TABDIFF_UMOUNT:
			printf("UMOUNTED\n");
			break;
		case MNT_TABDIFF_REMOUNT:
			printf("REMOUNTED from '%s' to '%s'\n",
					mnt_fs_get_options(old),
					mnt_fs_get_options(new));
			break;
		case MNT_TABDIFF_MOUNT:
			printf("MOUNTED\n");
			break;
		default:
			printf("unknown change!\n");
		}
	}

	rc = 0;
done:
	mnt_free_table(tb_old);
	mnt_free_table(tb_new);
	mnt_free_tabdiff(diff);
	mnt_free_iter(itr);
	return rc;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
		{ "--diff", test_diff, "<old> <new> prints change" },
		{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
