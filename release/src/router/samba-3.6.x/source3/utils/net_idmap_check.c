/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief  Check the idmap database.
 * @author Gregor Beck <gb@sernet.de>
 * @date   Mar 2011
 */

#include "net_idmap_check.h"
#include "includes.h"
#include "system/filesys.h"
#include "dbwrap.h"
#include "net.h"
#include "../libcli/security/dom_sid.h"
#include "cbuf.h"
#include "srprs.h"
#include <termios.h>
#include "util_tdb.h"

static int traverse_commit(struct db_record *diff_rec, void* data);
static int traverse_check(struct db_record *rec, void* data);

static char* interact_edit(TALLOC_CTX* mem_ctx, const char* str);
static int interact_prompt(const char* msg, const char* accept, char def);

/* TDB_DATA *******************************************************************/
static char*    print_data(TALLOC_CTX* mem_ctx, TDB_DATA d);
static TDB_DATA parse_data(TALLOC_CTX* mem_ctx, const char** ptr);
static TDB_DATA talloc_copy(TALLOC_CTX* mem_ctx, TDB_DATA data);
static bool is_empty(TDB_DATA data) {
	return (data.dsize == 0) || (data.dptr == NULL);
}

/* record *********************************************************************/

enum DT {
	DT_INV = 0,
	DT_SID,	DT_UID,	DT_GID,
	DT_HWM,	DT_VER, DT_SEQ,
};

struct record {
	enum DT key_type, val_type;
	TDB_DATA key, val;
	struct dom_sid sid;
	long unsigned  id;
};

static struct record* parse_record(TALLOC_CTX* ctx, TDB_DATA key, TDB_DATA val);
static struct record* reverse_record(struct record* rec);

static bool is_invalid(const struct record* r) {
	return (r->key_type == DT_INV) || (r->val_type == DT_INV);
}

static bool is_map(const struct record* r) {
	return (r->key_type == DT_SID)
		|| (r->key_type == DT_UID) || (r->key_type == DT_GID);
}

/* action *********************************************************************/

typedef struct check_action {
	const char* fmt;
	const char* name;
	const char* prompt;
	const char* answers;
	char auto_action;
	char default_action;
	bool verbose;
} check_action;

struct check_actions {
	check_action invalid_record;
	check_action missing_reverse;
	check_action invalid_mapping;
	check_action invalid_edit;
	check_action record_exists;
	check_action no_version;
	check_action wrong_version;
	check_action invalid_hwm;
	check_action commit;
	check_action valid_mapping;
	check_action valid_other;
	check_action invalid_diff;
};

static struct check_actions
check_actions_init(const struct check_options* opts) {
	struct check_actions ret = {
		.invalid_record = (check_action) {
			.name = "Invalid record",
			.prompt = "[e]dit/[d]elete/[D]elete all"
			"/[s]kip/[S]kip all",
			.answers = "eds",
			.default_action = 'e',
			.verbose = true,
		},
		.missing_reverse = (check_action) {
			.name = "Missing reverse mapping for",
			.prompt = "[f]ix/[F]ix all/[e]dit/[d]elete/[D]elete all"
			"/[s]kip/[S]kip all",
			.answers = "feds",
			.default_action = 'f',
			.verbose = true,
		},
		.invalid_mapping = (check_action) {
			.fmt = "%1$s: %2$s -> %3$s\n(%4$s <- %3$s)\n",
			.name = "Invalid mapping",
			.prompt = "[e]dit/[d]elete/[D]elete all"
			"/[s]kip/[S]kip all",
			.answers = "eds",
			.default_action = 'd',
			.verbose = true,
		},
		.invalid_edit = (check_action) {
			.name = "Invalid record",
			.prompt = "[e]dit/[d]elete/[D]elete all"
			"/[s]kip/[S]kip all",
			.answers = "eds",
			.default_action = 'e',
			.verbose = true,
		},
		.record_exists = (check_action) {
			.fmt = "%1$s: %2$s\n-%4$s\n+%3$s\n",
			.name = "Record exists",
			.prompt = "[o]verwrite/[O]verwrite all/[e]dit"
			"/[d]elete/[D]elete all/[s]kip/[S]kip all",
			.answers = "oeds",
			.default_action = 'o',
			.verbose = true,
		},
		.no_version = (check_action) {
			.prompt = "[f]ix/[s]kip/[a]bort",
			.answers = "fsa",
			.default_action = 'f',
		},
		.wrong_version = (check_action) {
			.prompt = "[f]ix/[s]kip/[a]bort",
			.answers = "fsa",
			.default_action = 'a',
		},
		.invalid_hwm = (check_action) {
			.prompt = "[f]ix/[s]kip",
			.answers = "fs",
			.default_action = 'f',
		},
		.commit = (check_action) {
			.prompt = "[c]ommit/[l]ist/[s]kip",
			.answers = "cls",
			.default_action = 'l',
			.verbose = true,
		},
		.valid_mapping = (check_action) {
			.fmt = "%1$s: %2$s <-> %3$s\n",
			.name = "Mapping",
			.auto_action = 's',
			.verbose = opts->verbose,
		},
		.valid_other = (check_action) {
			.name = "Other",
			.auto_action = 's',
			.verbose = opts->verbose,
		},
		.invalid_diff = (check_action) {
			.prompt = "[s]kip/[S]kip all/[c]ommit/[C]ommit all"
			"/[a]bort",
			.answers = "sca",
			.default_action = 's',
		},
	};

	if (!opts->repair) {
		ret.invalid_record.auto_action  = 's';
		ret.missing_reverse.auto_action = 's';
		ret.invalid_mapping.auto_action = 's';
		ret.no_version.auto_action      = 's';
		ret.wrong_version.auto_action   = 's';
		ret.invalid_hwm.auto_action     = 's';
		ret.commit.auto_action          = 's';
	}

	if (opts->automatic) {
		ret.invalid_record.auto_action  = 'd'; /* delete */
		ret.missing_reverse.auto_action = 'f'; /* fix */
		ret.invalid_mapping.auto_action = 'd'; /* delete */
		ret.no_version.auto_action      = 'f'; /* fix */
		ret.wrong_version.auto_action   = 'a'; /* abort */
		ret.invalid_hwm.auto_action     = 'f'; /* fix */
		ret.commit.auto_action          = 'c'; /* commit */
		ret.invalid_diff.auto_action    = 'a'; /* abort */
		if (opts->force) {
			ret.wrong_version.auto_action   = 'f'; /* fix */
			ret.invalid_diff.auto_action    = 'c'; /* commit */
		}
	}
	if (opts->test) {
		ret.invalid_diff.auto_action    = 'c'; /* commit */
/*		ret.commit.auto_action          = 'c';*/ /* commit */
	}

	return ret;
}

static char get_action(struct check_action* a, struct record* r, TDB_DATA* v) {
	char ret;
	if (a->verbose && (r != NULL)) {
		if (!a->fmt) {
			d_printf("%s: %s ", a->name, print_data(r, r->key));
			if (is_map(r)) {
				d_printf("-> %s\n", print_data(r, r->val));
			} else if (r->key_type == DT_HWM ||
				   r->key_type == DT_VER ||
				   r->key_type == DT_SEQ)
			{
				d_printf(": %ld\n", r->id);
			} else {
				d_printf("\n");
			}
		} else {
			d_printf(a->fmt, a->name,
				 print_data(r, r->key),
				 print_data(r, r->val),
				 (v ? print_data(r, *v) : ""));
		}
	}

	if (a->auto_action != '\0') {
		return a->auto_action;
	}

	ret = interact_prompt(a->prompt, a->answers, a->default_action);

	if (isupper(ret)) {
		ret = tolower(ret);
		a->auto_action = ret;
	}
	a->default_action = ret;
	return ret;
}

/*  *************************************************************************/

typedef struct {
	TDB_DATA oval, nval;
} TDB_DATA_diff;

static TDB_DATA pack_diff(TDB_DATA_diff* diff) {
	return (TDB_DATA) {
		.dptr = (uint8_t *)diff,
		.dsize = sizeof(TDB_DATA_diff),
	};
}

static TDB_DATA_diff unpack_diff(TDB_DATA data) {
	assert(data.dsize == sizeof(TDB_DATA_diff));
	return *(TDB_DATA_diff*)data.dptr;
}

#define DEBUG_DIFF(LEV,MEM,MSG,KEY,OLD,NEW)			\
	DEBUG(LEV, ("%s: %s\n", MSG, print_data(MEM, KEY)));	\
	if (!is_empty(OLD)) {					\
		DEBUGADD(LEV, ("-%s\n", print_data(MEM, OLD)));	\
	}							\
	if (!is_empty(NEW)) {					\
		DEBUGADD(LEV, ("+%s\n", print_data(MEM, NEW)));	\
	}

struct check_ctx {
	int oflags;
	char* name;
	bool transaction;
	struct db_context *db;
	struct db_context *diff;
	struct check_actions action;

	uint32_t uid_hwm;
	uint32_t gid_hwm;

	unsigned n_invalid_record;
	unsigned n_missing_reverse;
	unsigned n_invalid_mappping;
	unsigned n_map;
	unsigned n_other;
	unsigned n_diff;
	struct check_options opts;
};


static void adjust_hwm(struct check_ctx* ctx, const struct record* r);

static int add_record(struct check_ctx* ctx, TDB_DATA key, TDB_DATA value)
{
	NTSTATUS status;
	TDB_DATA_diff diff;
	TALLOC_CTX* mem = talloc_new(ctx->diff);
	struct db_record* rec = ctx->diff->fetch_locked(ctx->diff, mem, key);
	if (rec == NULL) {
		return -1;
	};
	if (rec->value.dptr == 0) { /* first entry */
		diff.oval = dbwrap_fetch(ctx->db, ctx->diff, key);
	} else {
		diff = unpack_diff(rec->value);
		talloc_free(diff.nval.dptr);
	}
	diff.nval = talloc_copy(ctx->diff, value);

	DEBUG_DIFF(2, mem, "TDB DIFF", key, diff.oval, diff.nval);

	status = rec->store(rec, pack_diff(&diff), 0);

	talloc_free(mem);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("could not store record %s\n", nt_errstr(status)));
		return -1;
	}
	ctx->n_diff ++;
	return 0;
}

static int del_record(struct check_ctx* ctx, TDB_DATA key) {
	return add_record(ctx, key, tdb_null);
}

static TDB_DATA
fetch_record(struct check_ctx* ctx, TALLOC_CTX* mem_ctx, TDB_DATA key)
{
	TDB_DATA tmp;

	if (ctx->diff->fetch(ctx->diff, mem_ctx, key, &tmp) == -1) {
		DEBUG(0, ("Out of memory!\n"));
		return tdb_null;
	}
	if (tmp.dptr != NULL) {
		TDB_DATA_diff diff = unpack_diff(tmp);
		TDB_DATA ret = talloc_copy(mem_ctx, diff.nval);
		talloc_free(tmp.dptr);
		return ret;
	}

	if (ctx->db->fetch(ctx->db, mem_ctx, key, &tmp) == -1) {
		DEBUG(0, ("Out of memory!\n"));
		return tdb_null;
	}

	return tmp;
}

static void edit_record(struct record* r) {
	TALLOC_CTX* mem = talloc_new(r);
	cbuf* ost = cbuf_new(mem);
	const char* str;
	struct record* nr;
	TDB_DATA key;
	TDB_DATA val;
	cbuf_printf(ost, "%s %s\n",
		    print_data(mem, r->key), print_data(mem, r->val));
	str = interact_edit(mem, cbuf_gets(ost, 0));
	key = parse_data(mem, &str);
	val = parse_data(mem, &str);
	nr = parse_record(talloc_parent(r), key, val);
	if (nr != NULL) {
		*r = *nr;
	}
	talloc_free(mem);
}

static bool check_version(struct check_ctx* ctx) {
	static const char* key = "IDMAP_VERSION";
	uint32_t version;
	bool no_version = !dbwrap_fetch_uint32(ctx->db, key, &version);
	char action = 's';
	struct check_actions* act = &ctx->action;
	if (no_version) {
		d_printf("No version number, assume 2\n");
		action = get_action(&act->no_version, NULL, NULL);
	} else if (version != 2) {
		d_printf("Wrong version number %d, should be 2\n", version);
		action = get_action(&act->wrong_version, NULL, NULL);
	}
	switch (action) {
	case 's':
		break;
	case 'f':
		SIVAL(&version, 0, 2);
		add_record(ctx, string_term_tdb_data(key),
			   make_tdb_data((uint8_t *)&version, sizeof(uint32_t)));
		break;
	case 'a':
		return false;
	default:
		assert(false);
	}
	return true;
}

static void check_hwm(struct check_ctx* ctx, const char* key, uint32_t target) {
	uint32_t hwm;
	char action = 's';
	bool found = dbwrap_fetch_uint32(ctx->db, key, &hwm);
	struct check_actions* act = &ctx->action;
	if (!found) {
		d_printf("No %s should be %d\n", key, target);
		action = get_action(&act->invalid_hwm, NULL, NULL);
	} else if (target < hwm) {
		d_printf("Invalid %s %d: should be %d\n", key, hwm, target);
		action = get_action(&act->invalid_hwm, NULL, NULL);
	}
	if (action == 'f') {
		SIVAL(&hwm, 0, target);
		add_record(ctx, string_term_tdb_data(key),
			   make_tdb_data((uint8_t *)&hwm, sizeof(uint32_t)));
	}
}

int traverse_check(struct db_record *rec, void* data) {
	struct check_ctx* ctx = (struct check_ctx*)data;
	struct check_actions* act = &ctx->action;
	TALLOC_CTX* mem = talloc_new(ctx->diff);
	struct record* r = parse_record(mem, rec->key, rec->value);
	char action = 's';

	if (is_invalid(r)) {
		action = get_action(&act->invalid_record, r, NULL);
		ctx->n_invalid_record++;
	} else if (is_map(r)) {
		TDB_DATA back = fetch_record(ctx, mem, r->val);
		if (back.dptr == NULL) {
			action = get_action(&act->missing_reverse, r, NULL);
			ctx->n_missing_reverse++;
		} else if (!tdb_data_equal(r->key, back)) {
			action = get_action(&act->invalid_mapping, r, &back);
			ctx->n_invalid_mappping++;
		} else {
			if (r->key_type == DT_SID) {
				action = get_action(&act->valid_mapping, r, NULL);
				ctx->n_map++;
			} else {
				action = get_action(&act->valid_mapping, NULL,
						    NULL);
			}
		}
		adjust_hwm(ctx, r);
	} else {
		action = get_action(&act->valid_other, r, NULL);
		ctx->n_other++;
	}

	while (action) {
		switch (action) {
		case 's': /* skip */
			break;
		case 'd': /* delete */
			del_record(ctx, rec->key);
			break;
		case 'f': /* add reverse mapping */
			add_record(ctx, rec->value, rec->key);
			break;
		case 'e': /* edit */
			edit_record(r);
			action = 'o';
			if (is_invalid(r)) {
				action = get_action(&act->invalid_edit, r,NULL);
				continue;
			}
			if (!tdb_data_equal(rec->key, r->key)) {
				TDB_DATA oval = fetch_record(ctx, mem, r->key);
				if (!is_empty(oval) &&
				    !tdb_data_equal(oval, r->val))
				{
					action = get_action(&act->record_exists,
							    r, &oval);
					if (action != 'o') {
						continue;
					}
				}
			}
			if (is_map(r)) {
				TDB_DATA okey = fetch_record(ctx, mem, r->val);
				if (!is_empty(okey) &&
				    !tdb_data_equal(okey, r->key))
				{
					action = get_action(&act->record_exists,
							    reverse_record(r),
							    &okey);
				}
			}
			continue;
		case 'o': /* overwrite */
			adjust_hwm(ctx, r);
			if (!tdb_data_equal(rec->key, r->key)) {
				del_record(ctx, rec->key);
			}
			add_record(ctx, r->key, r->val);
			if (is_map(r)) {
				add_record(ctx, r->val, r->key);
			}
		}
		action = '\0';
	};

	talloc_free(mem);

	return 0;
}

/******************************************************************************/

void adjust_hwm(struct check_ctx* ctx, const struct record* r) {
	enum DT type = (r->key_type == DT_SID) ? r->val_type : r->key_type;
	if (type == DT_UID) {
		ctx->uid_hwm = MAX(ctx->uid_hwm, r->id);
	} else if (type == DT_GID) {
		ctx->gid_hwm = MAX(ctx->gid_hwm, r->id);
	}
}

TDB_DATA talloc_copy(TALLOC_CTX* mem_ctx, TDB_DATA data) {
	TDB_DATA ret = {
		.dptr  = (uint8_t *)talloc_size(mem_ctx, data.dsize+1),
		.dsize = data.dsize
	};
	if (ret.dptr == NULL) {
		ret.dsize = 0;
	} else {
		memcpy(ret.dptr, data.dptr, data.dsize);
		ret.dptr[ret.dsize] = '\0';
	}
	return ret;
}

static bool is_cstr(TDB_DATA str) {
	return !is_empty(str) && str.dptr[str.dsize-1] == '\0';
}

static bool parse_sid (TDB_DATA str, enum DT* type, struct dom_sid* sid) {
	struct dom_sid tmp;
	const char* s = (const char*)str.dptr;
	if ((s[0] == 'S') && string_to_sid(&tmp, s)) {
		*sid = tmp;
		*type = DT_SID;
		return true;
	}
	return false;
}

static bool parse_xid(TDB_DATA str, enum DT* type, unsigned long* id) {
	char c, t;
	unsigned long tmp;
	if (sscanf((const char*)str.dptr, "%cID %lu%c", &c, &tmp, &t) == 2) {
		if (c == 'U') {
			*id = tmp;
			*type = DT_UID;
			return true;
		} else if (c == 'G') {
			*id = tmp;
			*type = DT_GID;
			return true;
		}
	}
	return false;
}


struct record*
parse_record(TALLOC_CTX* mem_ctx, TDB_DATA key, TDB_DATA val)
{
	struct record* ret = talloc_zero(mem_ctx, struct record);
	if (ret == NULL) {
		DEBUG(0, ("Out of memory.\n"));
		return NULL;
	}
	ret->key = talloc_copy(ret, key);
	ret->val = talloc_copy(ret, val);
	if ((ret->key.dptr == NULL && key.dptr != NULL) ||
	    (ret->val.dptr == NULL && val.dptr != NULL))
	{
		talloc_free(ret);
		DEBUG(0, ("Out of memory.\n"));
		return NULL;
	}
	assert((ret->key_type == DT_INV) && (ret->val_type == DT_INV));

	if (!is_cstr(key)) {
		return ret;
	}
	if (parse_sid(key, &ret->key_type, &ret->sid)) {
		parse_xid(val, &ret->val_type, &ret->id);
	} else if (parse_xid(key, &ret->key_type, &ret->id)) {
		if (is_cstr(val)) {
			parse_sid(val, &ret->val_type, &ret->sid);
		}
	} else if (strcmp((const char*)key.dptr, "USER HWM") == 0) {
		ret->key_type = DT_HWM;
		if (val.dsize == 4) {
			ret->id = IVAL(val.dptr,0);
			ret->val_type = DT_UID;
		}
	} else if (strcmp((const char*)key.dptr, "GROUP HWM") == 0) {
		ret->key_type = DT_HWM;
		if (val.dsize == 4) {
			ret->id = IVAL(val.dptr,0);
			ret->val_type = DT_GID;
		}
	} else if (strcmp((const char*)key.dptr, "IDMAP_VERSION") == 0) {
		ret->key_type = DT_VER;
		if (val.dsize == 4) {
			ret->id = IVAL(val.dptr,0);
			ret->val_type = DT_VER;
		}
	} else if (strcmp((const char*)key.dptr, "__db_sequence_number__") == 0) {
		ret->key_type = DT_SEQ;
		if (val.dsize == 8) {
			ret->id = *(uint64_t*)val.dptr;
			ret->val_type = DT_SEQ;
		}
	}

	return ret;
}

struct record* reverse_record(struct record* in)
{
	return parse_record(talloc_parent(in), in->val, in->key);
}


/******************************************************************************/

int interact_prompt(const char* msg, const char* acc, char def) {
	struct termios old_tio, new_tio;
	int c;

	tcgetattr(STDIN_FILENO, &old_tio);
	new_tio=old_tio;
	new_tio.c_lflag &=(~ICANON & ~ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

	do {
		d_printf("%s? [%c]\n", msg, def);
		fflush(stdout);
		c = getchar();
		if (c == '\n') {
			c = def;
			break;
		}
		else if (strchr(acc, tolower(c)) != NULL) {
			break;
		}
		d_printf("Invalid input '%c'\n", c);
	} while(c != EOF);
	tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
	return c;
}

char* print_data(TALLOC_CTX* mem_ctx, TDB_DATA d)
{
	if (!is_empty(d)) {
		char* ret = NULL;
		cbuf* ost = cbuf_new(mem_ctx);
		int len = cbuf_print_quoted(ost, (const char*)d.dptr, d.dsize);
		if (len != -1) {
			cbuf_swapptr(ost, &ret, 0);
			talloc_steal(mem_ctx, ret);
		}
		talloc_free(ost);
		return ret;
	}
	return talloc_strdup(mem_ctx, "<NULL>");
}


TDB_DATA parse_data(TALLOC_CTX* mem_ctx, const char** ptr) {
	cbuf* ost = cbuf_new(mem_ctx);
	TDB_DATA ret = tdb_null;
	srprs_skipws(ptr);
	if (srprs_quoted(ptr, ost)) {
		ret.dsize = cbuf_getpos(ost);
		ret.dptr = (uint8_t *)talloc_steal(mem_ctx, cbuf_gets(ost,0));
	}
	talloc_free(ost);
	return ret;
}

static const char* get_editor(void) {
	static const char* editor = NULL;
	if (editor == NULL) {
		editor = getenv("VISUAL");
		if (editor == NULL) {
			editor = getenv("EDITOR");
		}
		if (editor == NULL) {
			editor = "vi";
		}
	}
	return editor;
}

char* interact_edit(TALLOC_CTX* mem_ctx, const char* str) {
	char fname[] = "/tmp/net_idmap_check.XXXXXX";
	char buf[128];
	char* ret = NULL;
	FILE* file;

	int fd = mkstemp(fname);
	if (fd == -1) {
		DEBUG(0, ("failed to mkstemp %s: %s\n", fname,
			  strerror(errno)));
		return NULL;
	}

	file  = fdopen(fd, "w");
	if (!file) {
		DEBUG(0, ("failed to open %s for writing: %s\n", fname,
			  strerror(errno)));
		close(fd);
		unlink(fname);
		return NULL;
	}

	fprintf(file, "%s", str);
	fclose(file);

	snprintf(buf, sizeof(buf), "%s %s\n", get_editor(), fname);
	if (system(buf) != 0) {
		DEBUG(0, ("failed to start editor %s: %s\n", buf,
			  strerror(errno)));
		unlink(fname);
		return NULL;
	}

	file = fopen(fname, "r");
	if (!file) {
		DEBUG(0, ("failed to open %s for reading: %s\n", fname,
			  strerror(errno)));
		unlink(fname);
		return NULL;
	}
	while ( fgets(buf, sizeof(buf), file) ) {
		ret = talloc_strdup_append(ret, buf);
	}
	fclose(file);
	unlink(fname);

	return talloc_steal(mem_ctx, ret);
}


static int traverse_print_diff(struct db_record *rec, void* data) {
	struct check_ctx* ctx = (struct check_ctx*)data;
	TDB_DATA key = rec->key;
	TDB_DATA_diff diff = unpack_diff(rec->value);
	TALLOC_CTX* mem = talloc_new(ctx->diff);

	DEBUG_DIFF(0, mem, "DIFF", key, diff.oval, diff.nval);

	talloc_free(mem);
	return 0;
}


static int traverse_commit(struct db_record *diff_rec, void* data) {
	struct check_ctx* ctx = (struct check_ctx*)data;
	TDB_DATA_diff diff = unpack_diff(diff_rec->value);
	TDB_DATA key = diff_rec->key;
	TALLOC_CTX* mem = talloc_new(ctx->diff);
	int ret = -1;
	NTSTATUS status;
	struct check_actions* act = &ctx->action;

	struct db_record* rec = ctx->db->fetch_locked(ctx->db, mem, key);
	if (rec == NULL) {
		goto done;
	};

	if (!tdb_data_equal(rec->value, diff.oval)) {
		char action;

		d_printf("Warning: record has changed: %s\n"
			 "expected: %s got %s\n", print_data(mem, key),
			 print_data(mem, diff.oval),
			 print_data(mem, rec->value));

		action = get_action(&act->invalid_diff, NULL, NULL);
		if (action == 's') {
			ret = 0;
			goto done;
		} else if (action == 'a') {
			goto done;
		}
	}

	DEBUG_DIFF(0, mem, "Commit", key, diff.oval, diff.nval);

	if (is_empty(diff.nval)) {
		status = rec->delete_rec(rec);
	} else {
		status = rec->store(rec, diff.nval, 0);
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("could not store record %s\n", nt_errstr(status)));
		if (!ctx->opts.force) {
			goto done;
		}
	}
	ret = 0;
done:
	talloc_free(mem);
	return  ret;
}

static struct check_ctx*
check_init(TALLOC_CTX* mem_ctx, const struct check_options* o)
{
	struct check_ctx* ctx = talloc_zero(mem_ctx, struct check_ctx);
	if (ctx == NULL) {
		DEBUG(0, (_("No memory\n")));
		return NULL;
	}

	ctx->diff = db_open_rbt(ctx);
	if (ctx->diff == NULL) {
		talloc_free(ctx);
		DEBUG(0, (_("No memory\n")));
		return NULL;
	}

	ctx->action = check_actions_init(o);
	ctx->opts = *o;
	return ctx;
}

static bool check_open_db(struct check_ctx* ctx, const char* name, int oflags)
{
	if (name == NULL) {
		d_fprintf(stderr, _("Error: name == NULL in check_open_db().\n"));
		return false;
	}

	if (ctx->db != NULL) {
		if ((ctx->oflags == oflags) && (strcmp(ctx->name, name))) {
			return true;
		} else {
			TALLOC_FREE(ctx->db);
		}
	}

	ctx->db = db_open(ctx, name, 0, TDB_DEFAULT, oflags, 0);
	if (ctx->db == NULL) {
		d_fprintf(stderr,
			  _("Could not open idmap db (%s) for writing: %s\n"),
			  name, strerror(errno));
		return false;
	}

	if (ctx->name != name) {
		TALLOC_FREE(ctx->name);
		ctx->name = talloc_strdup(ctx, name);
	}

	ctx->oflags = oflags;
	return true;
}

static bool check_do_checks(struct check_ctx* ctx)
{
	NTSTATUS status;

	if (!check_version(ctx)) {
		return false;
	}

	status = dbwrap_traverse(ctx->db, traverse_check, ctx, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("failed to traverse %s\n", ctx->name));
		return false;
	}

	check_hwm(ctx, "USER HWM", ctx->uid_hwm + 1);
	check_hwm(ctx, "GROUP HWM", ctx->gid_hwm + 1);

	return true;
}

static void check_summary(const struct check_ctx* ctx)
{
	d_printf("uid hwm: %d\ngid hwm: %d\n", ctx->uid_hwm, ctx->gid_hwm);
	d_printf("mappings: %d\nother: %d\n", ctx->n_map, ctx->n_other);
	d_printf("invalid records: %d\nmissing links: %d\ninvalid links: %d\n",
		 ctx->n_invalid_record, ctx->n_missing_reverse,
		 ctx->n_invalid_mappping);
	d_printf("%u changes:\n", ctx->n_diff);
}

static bool check_transaction_start(struct check_ctx* ctx) {
	return (ctx->db->transaction_start(ctx->db) == 0);
}

static bool check_transaction_commit(struct check_ctx* ctx) {
	return (ctx->db->transaction_commit(ctx->db) == 0);
}

static bool check_transaction_cancel(struct check_ctx* ctx) {
	return (ctx->db->transaction_cancel(ctx->db) == 0);
}


static void check_diff_list(struct check_ctx* ctx) {
	NTSTATUS status = dbwrap_traverse(ctx->diff, traverse_print_diff, ctx, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("failed to traverse diff\n"));
	}

}

static bool check_commit(struct check_ctx* ctx)
{
	struct check_actions* act = &ctx->action;
	char action;
	NTSTATUS status = NT_STATUS_OK;

	check_summary(ctx);

	if (ctx->n_diff == 0) {
		return true;
	}

	while ((action = get_action(&act->commit, NULL, NULL)) == 'l') {
		check_diff_list(ctx);
	}
	if (action == 's') {
		return true;
	}
	assert(action == 'c');

	if (!check_open_db(ctx, ctx->name, O_RDWR)) {
		return false;
	}

	if (!check_transaction_start(ctx)) {
		return false;
	}

	status = dbwrap_traverse(ctx->diff, traverse_commit, ctx, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		check_transaction_cancel(ctx);
		return false;
	}
	if (ctx->opts.test) { //get_action?
		return check_transaction_cancel(ctx);
	} else {
		return check_transaction_commit(ctx);
	}
}

int net_idmap_check_db(const char* db, const struct check_options* o)
{
	int ret = -1;
	TALLOC_CTX* mem_ctx = talloc_stackframe();
	struct check_ctx* ctx = check_init(mem_ctx, o);

	if (!o->automatic && !isatty(STDIN_FILENO)) {
		DEBUG(0, ("Interactive use needs tty, use --auto\n"));
		goto done;
	}
	if (o->lock) {
		if (check_open_db(ctx, db, O_RDWR)
		    && check_transaction_start(ctx))
		{
			if ( check_do_checks(ctx)
			     && check_commit(ctx)
			     && check_transaction_commit(ctx))
			{
				ret = 0;
			} else {
				check_transaction_cancel(ctx);
			}
		}
	} else {
		if (check_open_db(ctx, db, O_RDONLY)
		    && check_do_checks(ctx)
		    && check_commit(ctx))
		{
			ret = 0;
		}
	}
done:
	talloc_free(mem_ctx);
	return ret;
}


/*Local Variables:*/
/*mode: c*/
/*End:*/
