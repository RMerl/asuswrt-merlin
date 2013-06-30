/*
 * test_rel.h
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */



void do_brel_ma_create(int argc, char **argv);
void do_brel_free(int argc, char **argv);
void do_brel_put(int argc, char **argv);
void do_brel_get(int argc, char **argv);
void do_brel_start_iter(int argc, char **argv);
void do_brel_next(int argc, char **argv);
void do_brel_dump(int argc, char **argv);
void do_brel_move(int argc, char **argv);
void do_brel_delete(int argc, char **argv);
void do_irel_ma_create(int argc, char **argv);
void do_irel_free(int argc, char **argv);
void do_irel_put(int argc, char **argv);
void do_irel_get(int argc, char **argv);
void do_irel_get_by_orig(int argc, char **argv);
void do_irel_start_iter(int argc, char **argv);
void do_irel_next(int argc, char **argv);
void do_irel_dump(int argc, char **argv);
void do_irel_add_ref(int argc, char **argv);
void do_irel_start_iter_ref(int argc, char **argv);
void do_irel_next_ref(int argc, char **argv);
void do_irel_move(int argc, char **argv);
void do_irel_delete(int argc, char **argv);
