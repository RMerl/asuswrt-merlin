#include "symbol.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "map.h"

const char *map_type__name[MAP__NR_TYPES] = {
	[MAP__FUNCTION] = "Functions",
	[MAP__VARIABLE] = "Variables",
};

static inline int is_anon_memory(const char *filename)
{
	return strcmp(filename, "//anon") == 0;
}

void map__init(struct map *self, enum map_type type,
	       u64 start, u64 end, u64 pgoff, struct dso *dso)
{
	self->type     = type;
	self->start    = start;
	self->end      = end;
	self->pgoff    = pgoff;
	self->dso      = dso;
	self->map_ip   = map__map_ip;
	self->unmap_ip = map__unmap_ip;
	RB_CLEAR_NODE(&self->rb_node);
	self->groups   = NULL;
	self->referenced = false;
}

struct map *map__new(struct list_head *dsos__list, u64 start, u64 len,
		     u64 pgoff, u32 pid, char *filename,
		     enum map_type type)
{
	struct map *self = malloc(sizeof(*self));

	if (self != NULL) {
		char newfilename[PATH_MAX];
		struct dso *dso;
		int anon;

		anon = is_anon_memory(filename);

		if (anon) {
			snprintf(newfilename, sizeof(newfilename), "/tmp/perf-%d.map", pid);
			filename = newfilename;
		}

		dso = __dsos__findnew(dsos__list, filename);
		if (dso == NULL)
			goto out_delete;

		map__init(self, type, start, start + len, pgoff, dso);

		if (anon) {
set_identity:
			self->map_ip = self->unmap_ip = identity__map_ip;
		} else if (strcmp(filename, "[vdso]") == 0) {
			dso__set_loaded(dso, self->type);
			goto set_identity;
		}
	}
	return self;
out_delete:
	free(self);
	return NULL;
}

void map__delete(struct map *self)
{
	free(self);
}

void map__fixup_start(struct map *self)
{
	struct rb_root *symbols = &self->dso->symbols[self->type];
	struct rb_node *nd = rb_first(symbols);
	if (nd != NULL) {
		struct symbol *sym = rb_entry(nd, struct symbol, rb_node);
		self->start = sym->start;
	}
}

void map__fixup_end(struct map *self)
{
	struct rb_root *symbols = &self->dso->symbols[self->type];
	struct rb_node *nd = rb_last(symbols);
	if (nd != NULL) {
		struct symbol *sym = rb_entry(nd, struct symbol, rb_node);
		self->end = sym->end;
	}
}

#define DSO__DELETED "(deleted)"

int map__load(struct map *self, symbol_filter_t filter)
{
	const char *name = self->dso->long_name;
	int nr;

	if (dso__loaded(self->dso, self->type))
		return 0;

	nr = dso__load(self->dso, self, filter);
	if (nr < 0) {
		if (self->dso->has_build_id) {
			char sbuild_id[BUILD_ID_SIZE * 2 + 1];

			build_id__sprintf(self->dso->build_id,
					  sizeof(self->dso->build_id),
					  sbuild_id);
			pr_warning("%s with build id %s not found",
				   name, sbuild_id);
		} else
			pr_warning("Failed to open %s", name);

		pr_warning(", continuing without symbols\n");
		return -1;
	} else if (nr == 0) {
		const size_t len = strlen(name);
		const size_t real_len = len - sizeof(DSO__DELETED);

		if (len > sizeof(DSO__DELETED) &&
		    strcmp(name + real_len + 1, DSO__DELETED) == 0) {
			pr_warning("%.*s was updated, restart the long "
				   "running apps that use it!\n",
				   (int)real_len, name);
		} else {
			pr_warning("no symbols found in %s, maybe install "
				   "a debug package?\n", name);
		}

		return -1;
	}
	/*
	 * Only applies to the kernel, as its symtabs aren't relative like the
	 * module ones.
	 */
	if (self->dso->kernel)
		map__reloc_vmlinux(self);

	return 0;
}

struct symbol *map__find_symbol(struct map *self, u64 addr,
				symbol_filter_t filter)
{
	if (map__load(self, filter) < 0)
		return NULL;

	return dso__find_symbol(self->dso, self->type, addr);
}

struct symbol *map__find_symbol_by_name(struct map *self, const char *name,
					symbol_filter_t filter)
{
	if (map__load(self, filter) < 0)
		return NULL;

	if (!dso__sorted_by_name(self->dso, self->type))
		dso__sort_by_name(self->dso, self->type);

	return dso__find_symbol_by_name(self->dso, self->type, name);
}

struct map *map__clone(struct map *self)
{
	struct map *map = malloc(sizeof(*self));

	if (!map)
		return NULL;

	memcpy(map, self, sizeof(*self));

	return map;
}

int map__overlap(struct map *l, struct map *r)
{
	if (l->start > r->start) {
		struct map *t = l;
		l = r;
		r = t;
	}

	if (l->end > r->start)
		return 1;

	return 0;
}

size_t map__fprintf(struct map *self, FILE *fp)
{
	return fprintf(fp, " %Lx-%Lx %Lx %s\n",
		       self->start, self->end, self->pgoff, self->dso->name);
}

/*
 * objdump wants/reports absolute IPs for ET_EXEC, and RIPs for ET_DYN.
 * map->dso->adjust_symbols==1 for ET_EXEC-like cases.
 */
u64 map__rip_2objdump(struct map *map, u64 rip)
{
	u64 addr = map->dso->adjust_symbols ?
			map->unmap_ip(map, rip) :	/* RIP -> IP */
			rip;
	return addr;
}

u64 map__objdump_2ip(struct map *map, u64 addr)
{
	u64 ip = map->dso->adjust_symbols ?
			addr :
			map->unmap_ip(map, addr);	/* RIP -> IP */
	return ip;
}

void map_groups__init(struct map_groups *self)
{
	int i;
	for (i = 0; i < MAP__NR_TYPES; ++i) {
		self->maps[i] = RB_ROOT;
		INIT_LIST_HEAD(&self->removed_maps[i]);
	}
	self->machine = NULL;
}

static void maps__delete(struct rb_root *self)
{
	struct rb_node *next = rb_first(self);

	while (next) {
		struct map *pos = rb_entry(next, struct map, rb_node);

		next = rb_next(&pos->rb_node);
		rb_erase(&pos->rb_node, self);
		map__delete(pos);
	}
}

static void maps__delete_removed(struct list_head *self)
{
	struct map *pos, *n;

	list_for_each_entry_safe(pos, n, self, node) {
		list_del(&pos->node);
		map__delete(pos);
	}
}

void map_groups__exit(struct map_groups *self)
{
	int i;

	for (i = 0; i < MAP__NR_TYPES; ++i) {
		maps__delete(&self->maps[i]);
		maps__delete_removed(&self->removed_maps[i]);
	}
}

void map_groups__flush(struct map_groups *self)
{
	int type;

	for (type = 0; type < MAP__NR_TYPES; type++) {
		struct rb_root *root = &self->maps[type];
		struct rb_node *next = rb_first(root);

		while (next) {
			struct map *pos = rb_entry(next, struct map, rb_node);
			next = rb_next(&pos->rb_node);
			rb_erase(&pos->rb_node, root);
			/*
			 * We may have references to this map, for
			 * instance in some hist_entry instances, so
			 * just move them to a separate list.
			 */
			list_add_tail(&pos->node, &self->removed_maps[pos->type]);
		}
	}
}

struct symbol *map_groups__find_symbol(struct map_groups *self,
				       enum map_type type, u64 addr,
				       struct map **mapp,
				       symbol_filter_t filter)
{
	struct map *map = map_groups__find(self, type, addr);

	if (map != NULL) {
		if (mapp != NULL)
			*mapp = map;
		return map__find_symbol(map, map->map_ip(map, addr), filter);
	}

	return NULL;
}

struct symbol *map_groups__find_symbol_by_name(struct map_groups *self,
					       enum map_type type,
					       const char *name,
					       struct map **mapp,
					       symbol_filter_t filter)
{
	struct rb_node *nd;

	for (nd = rb_first(&self->maps[type]); nd; nd = rb_next(nd)) {
		struct map *pos = rb_entry(nd, struct map, rb_node);
		struct symbol *sym = map__find_symbol_by_name(pos, name, filter);

		if (sym == NULL)
			continue;
		if (mapp != NULL)
			*mapp = pos;
		return sym;
	}

	return NULL;
}

size_t __map_groups__fprintf_maps(struct map_groups *self,
				  enum map_type type, int verbose, FILE *fp)
{
	size_t printed = fprintf(fp, "%s:\n", map_type__name[type]);
	struct rb_node *nd;

	for (nd = rb_first(&self->maps[type]); nd; nd = rb_next(nd)) {
		struct map *pos = rb_entry(nd, struct map, rb_node);
		printed += fprintf(fp, "Map:");
		printed += map__fprintf(pos, fp);
		if (verbose > 2) {
			printed += dso__fprintf(pos->dso, type, fp);
			printed += fprintf(fp, "--\n");
		}
	}

	return printed;
}

size_t map_groups__fprintf_maps(struct map_groups *self, int verbose, FILE *fp)
{
	size_t printed = 0, i;
	for (i = 0; i < MAP__NR_TYPES; ++i)
		printed += __map_groups__fprintf_maps(self, i, verbose, fp);
	return printed;
}

static size_t __map_groups__fprintf_removed_maps(struct map_groups *self,
						 enum map_type type,
						 int verbose, FILE *fp)
{
	struct map *pos;
	size_t printed = 0;

	list_for_each_entry(pos, &self->removed_maps[type], node) {
		printed += fprintf(fp, "Map:");
		printed += map__fprintf(pos, fp);
		if (verbose > 1) {
			printed += dso__fprintf(pos->dso, type, fp);
			printed += fprintf(fp, "--\n");
		}
	}
	return printed;
}

static size_t map_groups__fprintf_removed_maps(struct map_groups *self,
					       int verbose, FILE *fp)
{
	size_t printed = 0, i;
	for (i = 0; i < MAP__NR_TYPES; ++i)
		printed += __map_groups__fprintf_removed_maps(self, i, verbose, fp);
	return printed;
}

size_t map_groups__fprintf(struct map_groups *self, int verbose, FILE *fp)
{
	size_t printed = map_groups__fprintf_maps(self, verbose, fp);
	printed += fprintf(fp, "Removed maps:\n");
	return printed + map_groups__fprintf_removed_maps(self, verbose, fp);
}

int map_groups__fixup_overlappings(struct map_groups *self, struct map *map,
				   int verbose, FILE *fp)
{
	struct rb_root *root = &self->maps[map->type];
	struct rb_node *next = rb_first(root);
	int err = 0;

	while (next) {
		struct map *pos = rb_entry(next, struct map, rb_node);
		next = rb_next(&pos->rb_node);

		if (!map__overlap(pos, map))
			continue;

		if (verbose >= 2) {
			fputs("overlapping maps:\n", fp);
			map__fprintf(map, fp);
			map__fprintf(pos, fp);
		}

		rb_erase(&pos->rb_node, root);
		/*
		 * Now check if we need to create new maps for areas not
		 * overlapped by the new map:
		 */
		if (map->start > pos->start) {
			struct map *before = map__clone(pos);

			if (before == NULL) {
				err = -ENOMEM;
				goto move_map;
			}

			before->end = map->start - 1;
			map_groups__insert(self, before);
			if (verbose >= 2)
				map__fprintf(before, fp);
		}

		if (map->end < pos->end) {
			struct map *after = map__clone(pos);

			if (after == NULL) {
				err = -ENOMEM;
				goto move_map;
			}

			after->start = map->end + 1;
			map_groups__insert(self, after);
			if (verbose >= 2)
				map__fprintf(after, fp);
		}
move_map:
		/*
		 * If we have references, just move them to a separate list.
		 */
		if (pos->referenced)
			list_add_tail(&pos->node, &self->removed_maps[map->type]);
		else
			map__delete(pos);

		if (err)
			return err;
	}

	return 0;
}

int map_groups__clone(struct map_groups *self,
		      struct map_groups *parent, enum map_type type)
{
	struct rb_node *nd;
	for (nd = rb_first(&parent->maps[type]); nd; nd = rb_next(nd)) {
		struct map *map = rb_entry(nd, struct map, rb_node);
		struct map *new = map__clone(map);
		if (new == NULL)
			return -ENOMEM;
		map_groups__insert(self, new);
	}
	return 0;
}

static u64 map__reloc_map_ip(struct map *map, u64 ip)
{
	return ip + (s64)map->pgoff;
}

static u64 map__reloc_unmap_ip(struct map *map, u64 ip)
{
	return ip - (s64)map->pgoff;
}

void map__reloc_vmlinux(struct map *self)
{
	struct kmap *kmap = map__kmap(self);
	s64 reloc;

	if (!kmap->ref_reloc_sym || !kmap->ref_reloc_sym->unrelocated_addr)
		return;

	reloc = (kmap->ref_reloc_sym->unrelocated_addr -
		 kmap->ref_reloc_sym->addr);

	if (!reloc)
		return;

	self->map_ip   = map__reloc_map_ip;
	self->unmap_ip = map__reloc_unmap_ip;
	self->pgoff    = reloc;
}

void maps__insert(struct rb_root *maps, struct map *map)
{
	struct rb_node **p = &maps->rb_node;
	struct rb_node *parent = NULL;
	const u64 ip = map->start;
	struct map *m;

	while (*p != NULL) {
		parent = *p;
		m = rb_entry(parent, struct map, rb_node);
		if (ip < m->start)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&map->rb_node, parent, p);
	rb_insert_color(&map->rb_node, maps);
}

void maps__remove(struct rb_root *self, struct map *map)
{
	rb_erase(&map->rb_node, self);
}

struct map *maps__find(struct rb_root *maps, u64 ip)
{
	struct rb_node **p = &maps->rb_node;
	struct rb_node *parent = NULL;
	struct map *m;

	while (*p != NULL) {
		parent = *p;
		m = rb_entry(parent, struct map, rb_node);
		if (ip < m->start)
			p = &(*p)->rb_left;
		else if (ip > m->end)
			p = &(*p)->rb_right;
		else
			return m;
	}

	return NULL;
}

int machine__init(struct machine *self, const char *root_dir, pid_t pid)
{
	map_groups__init(&self->kmaps);
	RB_CLEAR_NODE(&self->rb_node);
	INIT_LIST_HEAD(&self->user_dsos);
	INIT_LIST_HEAD(&self->kernel_dsos);

	self->kmaps.machine = self;
	self->pid	    = pid;
	self->root_dir      = strdup(root_dir);
	return self->root_dir == NULL ? -ENOMEM : 0;
}

static void dsos__delete(struct list_head *self)
{
	struct dso *pos, *n;

	list_for_each_entry_safe(pos, n, self, node) {
		list_del(&pos->node);
		dso__delete(pos);
	}
}

void machine__exit(struct machine *self)
{
	map_groups__exit(&self->kmaps);
	dsos__delete(&self->user_dsos);
	dsos__delete(&self->kernel_dsos);
	free(self->root_dir);
	self->root_dir = NULL;
}

void machine__delete(struct machine *self)
{
	machine__exit(self);
	free(self);
}

struct machine *machines__add(struct rb_root *self, pid_t pid,
			      const char *root_dir)
{
	struct rb_node **p = &self->rb_node;
	struct rb_node *parent = NULL;
	struct machine *pos, *machine = malloc(sizeof(*machine));

	if (!machine)
		return NULL;

	if (machine__init(machine, root_dir, pid) != 0) {
		free(machine);
		return NULL;
	}

	while (*p != NULL) {
		parent = *p;
		pos = rb_entry(parent, struct machine, rb_node);
		if (pid < pos->pid)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&machine->rb_node, parent, p);
	rb_insert_color(&machine->rb_node, self);

	return machine;
}

struct machine *machines__find(struct rb_root *self, pid_t pid)
{
	struct rb_node **p = &self->rb_node;
	struct rb_node *parent = NULL;
	struct machine *machine;
	struct machine *default_machine = NULL;

	while (*p != NULL) {
		parent = *p;
		machine = rb_entry(parent, struct machine, rb_node);
		if (pid < machine->pid)
			p = &(*p)->rb_left;
		else if (pid > machine->pid)
			p = &(*p)->rb_right;
		else
			return machine;
		if (!machine->pid)
			default_machine = machine;
	}

	return default_machine;
}

struct machine *machines__findnew(struct rb_root *self, pid_t pid)
{
	char path[PATH_MAX];
	const char *root_dir;
	struct machine *machine = machines__find(self, pid);

	if (!machine || machine->pid != pid) {
		if (pid == HOST_KERNEL_ID || pid == DEFAULT_GUEST_KERNEL_ID)
			root_dir = "";
		else {
			if (!symbol_conf.guestmount)
				goto out;
			sprintf(path, "%s/%d", symbol_conf.guestmount, pid);
			if (access(path, R_OK)) {
				pr_err("Can't access file %s\n", path);
				goto out;
			}
			root_dir = path;
		}
		machine = machines__add(self, pid, root_dir);
	}

out:
	return machine;
}

void machines__process(struct rb_root *self, machine__process_t process, void *data)
{
	struct rb_node *nd;

	for (nd = rb_first(self); nd; nd = rb_next(nd)) {
		struct machine *pos = rb_entry(nd, struct machine, rb_node);
		process(pos, data);
	}
}

char *machine__mmap_name(struct machine *self, char *bf, size_t size)
{
	if (machine__is_host(self))
		snprintf(bf, size, "[%s]", "kernel.kallsyms");
	else if (machine__is_default_guest(self))
		snprintf(bf, size, "[%s]", "guest.kernel.kallsyms");
	else
		snprintf(bf, size, "[%s.%d]", "guest.kernel.kallsyms", self->pid);

	return bf;
}
