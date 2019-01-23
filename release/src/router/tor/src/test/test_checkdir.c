/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#endif

#include "config.h"
#include "test.h"
#include "util.h"

#ifdef _WIN32
#define mkdir(a,b) mkdir(a)
#define tt_int_op_nowin(a,op,b) do { (void)(a); (void)(b); } while (0)
#define umask(mask) ((void)0)
#else
#define tt_int_op_nowin(a,op,b) tt_int_op((a),op,(b))
#endif

/** Run unit tests for private dir permission enforcement logic. */
static void
test_checkdir_perms(void *testdata)
{
  (void)testdata;
  or_options_t *options = get_options_mutable();
  const char *subdir = "test_checkdir";
  char *testdir = NULL;
  cpd_check_t  cpd_chkopts;
  cpd_check_t  unix_create_opts;
  cpd_check_t  unix_verify_optsmask;
  struct stat st;

  umask(022);

  /* setup data directory before tests. */
  tor_free(options->DataDirectory);
  options->DataDirectory = tor_strdup(get_fname(subdir));
  tt_int_op(mkdir(options->DataDirectory, 0750), OP_EQ, 0);

  /* test: create new dir, no flags. */
  testdir = get_datadir_fname("checkdir_new_none");
  cpd_chkopts = CPD_CREATE;
  unix_verify_optsmask = 0077;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: create new dir, CPD_GROUP_OK option set. */
  testdir = get_datadir_fname("checkdir_new_groupok");
  cpd_chkopts = CPD_CREATE|CPD_GROUP_OK;
  unix_verify_optsmask = 0077;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: should get an error on existing dir with
           wrong perms */
  testdir = get_datadir_fname("checkdir_new_groupok_err");
  tt_int_op(0, OP_EQ, mkdir(testdir, 027));
  cpd_chkopts = CPD_CHECK_MODE_ONLY|CPD_CREATE|CPD_GROUP_OK;
  tt_int_op_nowin(-1, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tor_free(testdir);

  /* test: create new dir, CPD_GROUP_READ option set. */
  testdir = get_datadir_fname("checkdir_new_groupread");
  cpd_chkopts = CPD_CREATE|CPD_GROUP_READ;
  unix_verify_optsmask = 0027;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: check existing dir created with defaults,
            and verify with CPD_CREATE only. */
  testdir = get_datadir_fname("checkdir_exists_none");
  cpd_chkopts = CPD_CREATE;
  unix_create_opts = 0700;
  (void)unix_create_opts;
  unix_verify_optsmask = 0077;
  tt_int_op(0, OP_EQ, mkdir(testdir, unix_create_opts));
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: check existing dir created with defaults,
            and verify with CPD_GROUP_OK option set. */
  testdir = get_datadir_fname("checkdir_exists_groupok");
  cpd_chkopts = CPD_CREATE;
  unix_verify_optsmask = 0077;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  cpd_chkopts = CPD_GROUP_OK;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: check existing dir created with defaults,
            and verify with CPD_GROUP_READ option set. */
  testdir = get_datadir_fname("checkdir_exists_groupread");
  cpd_chkopts = CPD_CREATE;
  unix_verify_optsmask = 0027;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  cpd_chkopts = CPD_GROUP_READ;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: check existing dir created with CPD_GROUP_READ,
            and verify with CPD_GROUP_OK option set. */
  testdir = get_datadir_fname("checkdir_existsread_groupok");
  cpd_chkopts = CPD_CREATE|CPD_GROUP_READ;
  unix_verify_optsmask = 0027;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  cpd_chkopts = CPD_GROUP_OK;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));
  tor_free(testdir);

  /* test: check existing dir created with CPD_GROUP_READ,
            and verify with CPD_GROUP_READ option set. */
  testdir = get_datadir_fname("checkdir_existsread_groupread");
  cpd_chkopts = CPD_CREATE|CPD_GROUP_READ;
  unix_verify_optsmask = 0027;
  tt_int_op(0, OP_EQ, check_private_dir(testdir, cpd_chkopts, NULL));
  tt_int_op(0, OP_EQ, stat(testdir, &st));
  tt_int_op_nowin(0, OP_EQ, (st.st_mode & unix_verify_optsmask));

 done:
  tor_free(testdir);
}

#define CHECKDIR(name,flags)                              \
  { #name, test_checkdir_##name, (flags), NULL, NULL }

struct testcase_t checkdir_tests[] = {
  CHECKDIR(perms, TT_FORK),
  END_OF_TESTCASES
};

