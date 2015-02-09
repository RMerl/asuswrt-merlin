#include "../libslang.h"
#include <elf.h>
#include <newt.h>
#include <sys/ttydefaults.h>
#include <ctype.h>
#include <string.h>
#include <linux/bitops.h>
#include "../../debug.h"
#include "../../symbol.h"
#include "../browser.h"
#include "../helpline.h"
#include "map.h"

static int ui_entry__read(const char *title, char *bf, size_t size, int width)
{
	struct newtExitStruct es;
	newtComponent form, entry;
	const char *result;
	int err = -1;

	newtCenteredWindow(width, 1, title);
	form = newtForm(NULL, NULL, 0);
	if (form == NULL)
		return -1;

	entry = newtEntry(0, 0, "0x", width, &result, NEWT_FLAG_SCROLL);
	if (entry == NULL)
		goto out_free_form;

	newtFormAddComponent(form, entry);
	newtFormAddHotKey(form, NEWT_KEY_ENTER);
	newtFormAddHotKey(form, NEWT_KEY_ESCAPE);
	newtFormAddHotKey(form, NEWT_KEY_LEFT);
	newtFormAddHotKey(form, CTRL('c'));
	newtFormRun(form, &es);

	if (result != NULL) {
		strncpy(bf, result, size);
		err = 0;
	}
out_free_form:
	newtPopWindow();
	newtFormDestroy(form);
	return 0;
}

struct map_browser {
	struct ui_browser b;
	struct map	  *map;
	u16		  namelen;
	u8		  addrlen;
};

static void map_browser__write(struct ui_browser *self, void *nd, int row)
{
	struct symbol *sym = rb_entry(nd, struct symbol, rb_node);
	struct map_browser *mb = container_of(self, struct map_browser, b);
	bool current_entry = ui_browser__is_current_entry(self, row);
	int color = ui_browser__percent_color(0, current_entry);

	SLsmg_set_color(color);
	slsmg_printf("%*llx %*llx %c ",
		     mb->addrlen, sym->start, mb->addrlen, sym->end,
		     sym->binding == STB_GLOBAL ? 'g' :
		     sym->binding == STB_LOCAL  ? 'l' : 'w');
	slsmg_write_nstring(sym->name, mb->namelen);
}

static u32 *symbol__browser_index(struct symbol *self)
{
	return ((void *)self) - sizeof(struct rb_node) - sizeof(u32);
}

static int map_browser__search(struct map_browser *self)
{
	char target[512];
	struct symbol *sym;
	int err = ui_entry__read("Search by name/addr", target, sizeof(target), 40);

	if (err)
		return err;

	if (target[0] == '0' && tolower(target[1]) == 'x') {
		u64 addr = strtoull(target, NULL, 16);
		sym = map__find_symbol(self->map, addr, NULL);
	} else
		sym = map__find_symbol_by_name(self->map, target, NULL);

	if (sym != NULL) {
		u32 *idx = symbol__browser_index(sym);

		self->b.top = &sym->rb_node;
		self->b.index = self->b.top_idx = *idx;
	} else
		ui_helpline__fpush("%s not found!", target);

	return 0;
}

static int map_browser__run(struct map_browser *self, struct newtExitStruct *es)
{
	if (ui_browser__show(&self->b, self->map->dso->long_name,
			     "Press <- or ESC to exit, %s / to search",
			     verbose ? "" : "restart with -v to use") < 0)
		return -1;

	newtFormAddHotKey(self->b.form, NEWT_KEY_LEFT);
	newtFormAddHotKey(self->b.form, NEWT_KEY_ENTER);
	if (verbose)
		newtFormAddHotKey(self->b.form, '/');

	while (1) {
		ui_browser__run(&self->b, es);

		if (es->reason != NEWT_EXIT_HOTKEY)
			break;
		if (verbose && es->u.key == '/')
			map_browser__search(self);
		else
			break;
	}

	ui_browser__hide(&self->b);
	return 0;
}

int map__browse(struct map *self)
{
	struct map_browser mb = {
		.b = {
			.entries = &self->dso->symbols[self->type],
			.refresh = ui_browser__rb_tree_refresh,
			.seek	 = ui_browser__rb_tree_seek,
			.write	 = map_browser__write,
		},
		.map = self,
	};
	struct newtExitStruct es;
	struct rb_node *nd;
	char tmp[BITS_PER_LONG / 4];
	u64 maxaddr = 0;

	for (nd = rb_first(mb.b.entries); nd; nd = rb_next(nd)) {
		struct symbol *pos = rb_entry(nd, struct symbol, rb_node);

		if (mb.namelen < pos->namelen)
			mb.namelen = pos->namelen;
		if (maxaddr < pos->end)
			maxaddr = pos->end;
		if (verbose) {
			u32 *idx = symbol__browser_index(pos);
			*idx = mb.b.nr_entries;
		}
		++mb.b.nr_entries;
	}

	mb.addrlen = snprintf(tmp, sizeof(tmp), "%llx", maxaddr);
	mb.b.width += mb.addrlen * 2 + 4 + mb.namelen;
	return map_browser__run(&mb, &es);
}
