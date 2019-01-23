/**************************************************************************
 *   proto.h  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2017 Free Software Foundation, Inc.    *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#ifndef PROTO_H
#define PROTO_H 1

#include "nano.h"

/* All external variables.  See global.c for their descriptions. */

#ifndef NANO_TINY
extern volatile sig_atomic_t the_window_resized;
#endif

#ifdef __linux__
extern bool console;
#endif

extern bool meta_key;
extern bool shift_held;

extern bool focusing;

extern bool as_an_at;

extern int margin;
extern int editwincols;

#ifndef DISABLE_COLOR
extern bool have_palette;
#endif

extern bool suppress_cursorpos;

extern message_type lastmessage;

extern filestruct *pletion_line;

extern bool inhelp;
extern char *title;

extern int didfind;

extern int controlleft;
extern int controlright;
extern int controlup;
extern int controldown;
extern int controlhome;
extern int controlend;
#ifndef NANO_TINY
extern int shiftcontrolleft;
extern int shiftcontrolright;
extern int shiftcontrolup;
extern int shiftcontroldown;
extern int shiftcontrolhome;
extern int shiftcontrolend;
extern int shiftaltleft;
extern int shiftaltright;
extern int shiftaltup;
extern int shiftaltdown;
#endif

#ifndef DISABLE_WRAPJUSTIFY
extern ssize_t fill;
extern ssize_t wrap_at;
#endif

extern char *last_search;

extern char *present_path;

extern unsigned flags[4];
extern WINDOW *topwin;
extern WINDOW *edit;
extern WINDOW *bottomwin;
extern int editwinrows;

extern filestruct *cutbuffer;
extern filestruct *cutbottom;
extern partition *filepart;
extern openfilestruct *openfile;

#ifndef NANO_TINY
extern char *matchbrackets;
#endif

#ifndef NANO_TINY
extern char *whitespace;
extern int whitespace_len[2];
#endif

extern const char *exit_tag;
extern const char *close_tag;
extern const char *uncut_tag;
#ifndef DISABLE_JUSTIFY
extern const char *unjust_tag;
extern char *punct;
extern char *brackets;
extern char *quotestr;
extern regex_t quotereg;
extern int quoterc;
extern char *quoteerr;
#endif /* !DISABLE_JUSTIFY */

extern char *word_chars;

extern char *answer;

extern ssize_t tabsize;

#ifndef NANO_TINY
extern char *backup_dir;
extern const char *locking_prefix;
extern const char *locking_suffix;
#endif
#ifndef DISABLE_OPERATINGDIR
extern char *operating_dir;
extern char *full_operating_dir;
#endif

#ifndef DISABLE_SPELLER
extern char *alt_speller;
#endif

#ifndef DISABLE_COLOR
extern syntaxtype *syntaxes;
extern char *syntaxstr;
#endif

extern bool refresh_needed;

extern int currmenu;
extern sc *sclist;
extern subnfunc *allfuncs;
extern subnfunc *exitfunc;
extern subnfunc *uncutfunc;

#ifndef DISABLE_HISTORIES
extern filestruct *search_history;
extern filestruct *searchage;
extern filestruct *searchbot;
extern filestruct *replace_history;
extern filestruct *replaceage;
extern filestruct *replacebot;
extern poshiststruct *position_history;
#endif

extern regex_t search_regexp;
extern regmatch_t regmatches[10];

extern int hilite_attribute;
#ifndef DISABLE_COLOR
extern char* specified_color_combo[NUMBER_OF_ELEMENTS];
#endif
extern int interface_color_pair[NUMBER_OF_ELEMENTS];

extern char *homedir;

typedef void (*functionptrtype)(void);

/* Most functions in browser.c. */
#ifdef ENABLE_BROWSER
char *do_browse_from(const char *inpath);
void read_the_list(const char *path, DIR *dir);
functionptrtype parse_browser_input(int *kbinput);
void browser_refresh(void);
void browser_select_dirname(const char *needle);
void do_filesearch(void);
void do_fileresearch(void);
void do_first_file(void);
void do_last_file(void);
char *strip_last_component(const char *path);
#endif

/* Most functions in chars.c. */
#ifdef ENABLE_UTF8
void utf8_init(void);
bool using_utf8(void);
#endif
char *addstrings(char* str1, size_t len1, char* str2, size_t len2);
bool is_byte(int c);
bool is_alpha_mbchar(const char *c);
bool is_blank_mbchar(const char *c);
bool is_ascii_cntrl_char(int c);
bool is_cntrl_mbchar(const char *c);
bool is_word_mbchar(const char *c, bool allow_punct);
char control_mbrep(const char *c, bool isdata);
int length_of_char(const char *c, int *width);
int mbwidth(const char *c);
char *make_mbchar(long chr, int *chr_mb_len);
int parse_mbchar(const char *buf, char *chr, size_t *col);
size_t move_mbleft(const char *buf, size_t pos);
size_t move_mbright(const char *buf, size_t pos);
int mbstrcasecmp(const char *s1, const char *s2);
int mbstrncasecmp(const char *s1, const char *s2, size_t n);
char *mbstrcasestr(const char *haystack, const char *needle);
char *revstrstr(const char *haystack, const char *needle,
	const char *index);
char *mbrevstrcasestr(const char *haystack, const char *needle,
	const char *index);
size_t mbstrlen(const char *s);
size_t mbstrnlen(const char *s, size_t maxlen);
#if !defined(NANO_TINY) || !defined(DISABLE_JUSTIFY)
char *mbstrchr(const char *s, const char *c);
#endif
#ifndef NANO_TINY
char *mbstrpbrk(const char *s, const char *accept);
char *revstrpbrk(const char *head, const char *accept, const char *index);
char *mbrevstrpbrk(const char *head, const char *accept, const char *index);
#endif
#if defined(ENABLE_NANORC) && (!defined(NANO_TINY) || !defined(DISABLE_JUSTIFY))
bool has_blank_mbchars(const char *s);
#endif
#ifdef ENABLE_UTF8
bool is_valid_unicode(wchar_t wc);
#endif
#ifdef ENABLE_NANORC
bool is_valid_mbstring(const char *s);
#endif

/* Most functions in color.c. */
#ifndef DISABLE_COLOR
void set_colorpairs(void);
void color_init(void);
void color_update(void);
void check_the_multis(filestruct *line);
void alloc_multidata_if_needed(filestruct *fileptr);
void precalc_multicolorinfo(void);
#endif

/* Most functions in cut.c. */
void cutbuffer_reset(void);
bool keeping_cutbuffer(void);
#ifndef NANO_TINY
void cut_marked(bool *right_side_up);
#endif
void do_cut_text(bool copy_text, bool cut_till_eof);
void do_cut_text_void(void);
#ifndef NANO_TINY
void do_copy_text(void);
void do_cut_till_eof(void);
#endif
void do_uncut_text(void);

/* Most functions in files.c. */
void initialize_buffer_text(void);
bool open_buffer(const char *filename, bool undoable);
#ifndef DISABLE_SPELLER
void replace_buffer(const char *filename);
#ifndef NANO_TINY
void replace_marked_buffer(const char *filename, filestruct *top, size_t top_x,
	filestruct *bot, size_t bot_x);
#endif
#endif
void prepare_for_display(void);
#ifdef ENABLE_MULTIBUFFER
void switch_to_prev_buffer_void(void);
void switch_to_next_buffer_void(void);
bool close_buffer(void);
#endif
void read_file(FILE *f, int fd, const char *filename, bool undoable,
		bool checkwritable);
int open_file(const char *filename, bool newfie, bool quiet, FILE **f);
char *get_next_filename(const char *name, const char *suffix);
void do_insertfile_void(void);
char *get_full_path(const char *origpath);
char *safe_tempfile(FILE **f);
#ifndef DISABLE_OPERATINGDIR
void init_operating_dir(void);
bool check_operating_dir(const char *currpath, bool allow_tabcomp);
#endif
#ifndef NANO_TINY
void init_backup_dir(void);
int delete_lockfile(const char *lockfilename);
int write_lockfile(const char *lockfilename, const char *origfilename, bool modified);
#endif
int copy_file(FILE *inn, FILE *out, bool close_out);
bool write_file(const char *name, FILE *f_open, bool tmp,
	kind_of_writing_type method, bool nonamechange);
#ifndef NANO_TINY
bool write_marked_file(const char *name, FILE *f_open, bool tmp,
	kind_of_writing_type method);
#endif
int do_writeout(bool exiting);
void do_writeout_void(void);
#ifndef NANO_TINY
void do_savefile(void);
#endif
char *real_dir_from_tilde(const char *buf);
#if defined(ENABLE_TABCOMP) || defined(ENABLE_BROWSER)
int diralphasort(const void *va, const void *vb);
void free_chararray(char **array, size_t len);
#endif
#ifdef ENABLE_TABCOMP
char *input_tab(char *buf, bool allow_files, size_t *place,
	bool *lastwastab, void (*refresh_func)(void), bool *listed);
#endif
const char *tail(const char *path);
#ifndef DISABLE_HISTORIES
void load_history(void);
void save_history(void);
int check_dotnano(void);
void load_poshistory(void);
void save_poshistory(void);
void update_poshistory(char *filename, ssize_t lineno, ssize_t xpos);
bool has_old_position(const char *file, ssize_t *line, ssize_t *column);
#endif

/* Some functions in global.c. */
size_t length_of_list(int menu);
const sc *first_sc_for(int menu, void (*func)(void));
int the_code_for(void (*func)(void), int defaultval);
functionptrtype func_from_key(int *kbinput);
void assign_keyinfo(sc *s, const char *keystring, const int keycode);
void print_sclist(void);
void shortcut_init(void);
#ifndef DISABLE_COLOR
void set_lint_or_format_shortcuts(void);
void set_spell_shortcuts(void);
#endif
const subnfunc *sctofunc(const sc *s);
const char *flagtostr(int flag);
sc *strtosc(const char *input);
int strtomenu(const char *input);
#ifdef DEBUG
void thanks_for_all_the_fish(void);
#endif

/* All functions in help.c. */
#ifdef ENABLE_HELP
void wrap_the_help_text(bool redisplaying);
void do_help(void);
void help_init(void);
functionptrtype parse_help_input(int *kbinput);
size_t help_line_len(const char *ptr);
#endif
void do_help_void(void);

/* All functions in move.c. */
void do_first_line(void);
void do_last_line(void);
void do_page_up(void);
void do_page_down(void);
#ifndef DISABLE_JUSTIFY
void do_para_begin(bool update_screen);
void do_para_begin_void(void);
void do_para_end(bool update_screen);
void do_para_end_void(void);
#endif
void do_prev_block(void);
void do_next_block(void);
void do_prev_word(bool allow_punct, bool update_screen);
void do_prev_word_void(void);
bool do_next_word(bool allow_punct, bool update_screen);
void do_next_word_void(void);
void do_home(bool be_clever);
void do_home_void(void);
void do_end(bool be_clever);
void do_end_void(void);
void do_up(bool scroll_only);
void do_up_void(void);
void do_down(bool scroll_only);
void do_down_void(void);
#ifndef NANO_TINY
void do_scroll_up(void);
void do_scroll_down(void);
#endif
void do_left(void);
void do_right(void);

/* Most functions in nano.c. */
filestruct *make_new_node(filestruct *prevnode);
void splice_node(filestruct *afterthis, filestruct *newnode);
void unlink_node(filestruct *fileptr);
void delete_node(filestruct *fileptr);
filestruct *copy_filestruct(const filestruct *src);
void free_filestruct(filestruct *src);
void renumber(filestruct *fileptr);
partition *partition_filestruct(filestruct *top, size_t top_x,
	filestruct *bot, size_t bot_x);
void unpartition_filestruct(partition **p);
void extract_buffer(filestruct **file_top, filestruct **file_bot,
	filestruct *top, size_t top_x, filestruct *bot, size_t bot_x);
void ingraft_buffer(filestruct *somebuffer);
void copy_from_buffer(filestruct *somebuffer);
openfilestruct *make_new_opennode(void);
void unlink_opennode(openfilestruct *fileptr);
void delete_opennode(openfilestruct *fileptr);
void print_view_warning(void);
void show_restricted_warning(void);
#ifndef ENABLE_HELP
void say_there_is_no_help(void);
#endif
void finish(void);
void die(const char *msg, ...);
void die_save_file(const char *die_filename, struct stat *die_stat);
void window_init(void);
void do_exit(void);
void close_and_go(void);
RETSIGTYPE handle_hupterm(int signal);
RETSIGTYPE do_suspend(int signal);
RETSIGTYPE do_continue(int signal);
#ifndef NANO_TINY
RETSIGTYPE handle_sigwinch(int signal);
void regenerate_screen(void);
void allow_sigwinch(bool allow);
void do_toggle(int flag);
void do_toggle_void(void);
void enable_signals(void);
#endif
void disable_flow_control(void);
void enable_flow_control(void);
void terminal_init(void);
void unbound_key(int code);
int do_input(bool allow_funcs);
#ifdef ENABLE_MOUSE
int do_mouse(void);
#endif
void do_output(char *output, size_t output_len, bool allow_cntrls);

/* Most functions in prompt.c. */
#ifdef ENABLE_MOUSE
int do_statusbar_mouse(void);
#endif
void do_statusbar_output(int *the_input, size_t input_len,
	bool filtering);
void do_statusbar_home(void);
void do_statusbar_end(void);
void do_statusbar_left(void);
void do_statusbar_right(void);
void do_statusbar_backspace(void);
void do_statusbar_delete(void);
void do_statusbar_cut_text(void);
#ifndef NANO_TINY
void do_statusbar_prev_word(void);
void do_statusbar_next_word(void);
#endif
void do_statusbar_verbatim_input(void);
size_t statusbar_xplustabs(void);
size_t get_statusbar_page_start(size_t start_col, size_t column);
void reinit_statusbar_x(void);
void update_the_statusbar(void);
int do_prompt(bool allow_tabs, bool allow_files,
	int menu, const char *curranswer,
#ifndef DISABLE_HISTORIES
	filestruct **history_list,
#endif
	void (*refresh_func)(void), const char *msg, ...);
int do_yesno_prompt(bool all, const char *msg);

/* Most functions in rcfile.c. */
#ifdef ENABLE_NANORC
#ifndef DISABLE_COLOR
bool parse_color_names(char *combostr, short *fg, short *bg, bool *bright);
void grab_and_store(const char *kind, char *ptr, regexlisttype **storage);
#endif
void parse_rcfile(FILE *rcstream, bool syntax_only);
void do_rcfiles(void);
#endif /* ENABLE_NANORC */

/* Most functions in search.c. */
void not_found_msg(const char *str);
void search_replace_abort(void);
int findnextstr(const char *needle, bool whole_word_only, bool have_region,
	size_t *match_len, bool skipone, const filestruct *begin, size_t begin_x);
void do_search(void);
#ifndef NANO_TINY
void do_findprevious(void);
void do_findnext(void);
#endif
void do_research(void);
void go_looking(void);
ssize_t do_replace_loop(const char *needle, bool whole_word_only,
	const filestruct *real_current, size_t *real_current_x);
void do_replace(void);
void goto_line_posx(ssize_t line, size_t pos_x);
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer,
	bool interactive);
void do_gotolinecolumn_void(void);
#ifndef NANO_TINY
void do_find_bracket(void);
#ifdef ENABLE_TABCOMP
char *get_history_completion(filestruct **h, char *s, size_t len);
#endif
#endif
#ifndef DISABLE_HISTORIES
bool history_has_changed(void);
void history_init(void);
void history_reset(const filestruct *h);
void update_history(filestruct **h, const char *s);
char *get_history_older(filestruct **h);
char *get_history_newer(filestruct **h);
void get_history_older_void(void);
void get_history_newer_void(void);
#endif

/* Most functions in text.c. */
#ifndef NANO_TINY
void do_mark(void);
#endif
void do_delete(void);
void do_backspace(void);
#ifndef NANO_TINY
void do_cut_prev_word(void);
void do_cut_next_word(void);
#endif
void do_tab(void);
#ifndef NANO_TINY
void do_indent(void);
void do_unindent(void);
#endif
bool white_string(const char *s);
#ifdef ENABLE_COMMENT
void do_comment(void);
bool comment_line(undo_type action, filestruct *f, const char *comment_seq);
#endif
void do_undo(void);
void do_redo(void);
void do_enter(void);
#ifndef NANO_TINY
RETSIGTYPE cancel_command(int signal);
bool execute_command(const char *command);
void discard_until(const undo *thisitem, openfilestruct *thefile);
void add_undo(undo_type action);
#ifdef ENABLE_COMMENT
void update_comment_undo(ssize_t lineno);
#endif
void update_undo(undo_type action);
#endif /* !NANO_TINY */
#ifndef DISABLE_WRAPPING
void wrap_reset(void);
bool do_wrap(filestruct *line);
#endif
#if defined(ENABLE_HELP) || !defined(DISABLE_WRAPJUSTIFY)
ssize_t break_line(const char *line, ssize_t goal, bool snap_at_nl);
#endif
#if !defined(NANO_TINY) || !defined(DISABLE_JUSTIFY)
size_t indent_length(const char *line);
#endif
#ifndef DISABLE_JUSTIFY
void justify_format(filestruct *paragraph, size_t skip);
bool begpar(const filestruct *const foo);
bool inpar(const filestruct *const foo);
void do_justify(bool full_justify);
void do_justify_void(void);
void do_full_justify(void);
#endif
#ifndef DISABLE_SPELLER
void do_spell(void);
#endif
#ifndef DISABLE_COLOR
void do_linter(void);
void do_formatter(void);
#endif
#ifndef NANO_TINY
void do_wordlinechar_count(void);
#endif
void do_verbatim_input(void);
void complete_a_word(void);

/* All functions in utils.c. */
void get_homedir(void);
#ifdef ENABLE_LINENUMBERS
int digits(ssize_t n);
#endif
bool parse_num(const char *str, ssize_t *val);
bool parse_line_column(const char *str, ssize_t *line, ssize_t *column);
void snuggly_fit(char **str);
void null_at(char **data, size_t index);
void unsunder(char *str, size_t true_len);
void sunder(char *str);
const char *fixbounds(const char *r);
#ifndef DISABLE_SPELLER
bool is_separate_word(size_t position, size_t length, const char *buf);
#endif
const char *strstrwrapper(const char *haystack, const char *needle,
	const char *start);
void nperror(const char *s);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
char *mallocstrncpy(char *dest, const char *src, size_t n);
char *mallocstrcpy(char *dest, const char *src);
char *free_and_assign(char *dest, char *src);
size_t get_page_start(size_t column);
size_t xplustabs(void);
size_t actual_x(const char *s, size_t column);
size_t strnlenpt(const char *s, size_t maxlen);
size_t strlenpt(const char *s);
void new_magicline(void);
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
void remove_magicline(void);
#endif
#ifndef NANO_TINY
void mark_order(const filestruct **top, size_t *top_x, const filestruct
	**bot, size_t *bot_x, bool *right_side_up);
#endif
size_t get_totsize(const filestruct *begin, const filestruct *end);
#ifndef NANO_TINY
filestruct *fsfromline(ssize_t lineno);
#endif
#ifdef DEBUG
void dump_filestruct(const filestruct *inptr);
void dump_filestruct_reverse(void);
#endif

/* Most functions in winio.c. */
void get_key_buffer(WINDOW *win);
size_t get_key_buffer_len(void);
void unget_kbinput(int kbinput, bool metakey);
int get_kbinput(WINDOW *win);
int parse_kbinput(WINDOW *win);
int arrow_from_abcd(int kbinput);
int parse_escape_sequence(WINDOW *win, int kbinput);
int get_byte_kbinput(int kbinput);
int get_control_kbinput(int kbinput);
int *get_verbatim_kbinput(WINDOW *win, size_t *kbinput_len);
int *parse_verbatim_kbinput(WINDOW *win, size_t *count);
#ifdef ENABLE_MOUSE
int get_mouseinput(int *mouse_x, int *mouse_y, bool allow_shortcuts);
#endif
const sc *get_shortcut(int *kbinput);
void blank_row(WINDOW *win, int y, int x, int n);
void blank_edit(void);
void blank_statusbar(void);
void blank_bottombars(void);
void check_statusblank(void);
char *display_string(const char *buf, size_t column, size_t span, bool isdata);
void titlebar(const char *path);
extern void set_modified(void);
void statusbar(const char *msg);
void warn_and_shortly_pause(const char *msg);
void statusline(message_type importance, const char *msg, ...);
void bottombars(int menu);
void onekey(const char *keystroke, const char *desc, int length);
void place_the_cursor(bool forreal);
void edit_draw(filestruct *fileptr, const char *converted,
	int line, size_t from_col);
int update_line(filestruct *fileptr, size_t index);
#ifndef NANO_TINY
int update_softwrapped_line(filestruct *fileptr);
#endif
bool line_needs_update(const size_t old_column, const size_t new_column);
int go_back_chunks(int nrows, filestruct **line, size_t *leftedge);
int go_forward_chunks(int nrows, filestruct **line, size_t *leftedge);
bool less_than_a_screenful(size_t was_lineno, size_t was_leftedge);
void edit_scroll(scroll_dir direction, int nrows);
#ifndef NANO_TINY
size_t get_softwrap_breakpoint(const char *text, size_t leftedge,
				bool *end_of_line);
size_t get_chunk_and_edge(size_t column, filestruct *line, size_t *leftedge);
size_t chunk_for(size_t column, filestruct *line);
size_t leftedge_for(size_t column, filestruct *line);
size_t number_of_chunks_in(filestruct *line);
void ensure_firstcolumn_is_aligned(void);
#endif
size_t actual_last_column(size_t leftedge, size_t column);
void edit_redraw(filestruct *old_current);
void edit_refresh(void);
void adjust_viewport(update_type location);
void total_redraw(void);
void total_refresh(void);
void display_main_list(void);
void do_cursorpos(bool force);
void do_cursorpos_void(void);
void spotlight(bool active, size_t from_col, size_t to_col);
#ifndef NANO_TINY
void spotlight_softwrapped(bool active, size_t from_col, size_t to_col);
#endif
void xon_complaint(void);
void xoff_complaint(void);
void do_suspend_void(void);
void disable_waiting(void);
void enable_waiting(void);
#ifndef DISABLE_EXTRA
void do_credits(void);
#endif

/* These are just name definitions. */
void do_cancel(void);
void case_sens_void(void);
void regexp_void(void);
void backwards_void(void);
void flip_replace(void);
void gototext_void(void);
#ifdef ENABLE_BROWSER
void to_files_void(void);
void goto_dir_void(void);
#endif
#ifndef NANO_TINY
void dos_format_void(void);
void mac_format_void(void);
void append_void(void);
void prepend_void(void);
void backup_file_void(void);
void flip_execute(void);
#endif
#ifdef ENABLE_MULTIBUFFER
void flip_newbuffer(void);
#endif
void discard_buffer(void);

#endif /* !PROTO_H */
