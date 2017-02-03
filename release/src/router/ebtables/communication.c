/*
 * communication.c, v2.0 July 2002
 *
 * Author: Bart De Schuymer
 *
 */

/*
 * All the userspace/kernel communication is in this file.
 * The other code should not have to know anything about the way the
 * kernel likes the structure of the table data.
 * The other code works with linked lists. So, the translation is done here.
 */

#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "include/ebtables_u.h"

extern char* hooknames[NF_BR_NUMHOOKS];

#ifdef KERNEL_64_USERSPACE_32
#define sparc_cast (uint64_t)
#else
#define sparc_cast
#endif

int sockfd = -1;

static int get_sockfd()
{
	int ret = 0;
	if (sockfd == -1) {
		sockfd = socket(AF_INET, SOCK_RAW, PF_INET);
		if (sockfd < 0) {
			ebt_print_error("Problem getting a socket, "
					"you probably don't have the right "
					"permissions");
			ret = -1;
		}
	}
	return ret;
}

static struct ebt_replace *translate_user2kernel(struct ebt_u_replace *u_repl)
{
	struct ebt_replace *new;
	struct ebt_u_entry *e;
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;
	struct ebt_u_entries *entries;
	char *p, *base;
	int i, j;
	unsigned int entries_size = 0, *chain_offsets;

	new = (struct ebt_replace *)malloc(sizeof(struct ebt_replace));
	if (!new)
		ebt_print_memory();
	new->valid_hooks = u_repl->valid_hooks;
	strcpy(new->name, u_repl->name);
	new->nentries = u_repl->nentries;
	new->num_counters = u_repl->num_counters;
	new->counters = sparc_cast u_repl->counters;
	chain_offsets = (unsigned int *)calloc(u_repl->num_chains, sizeof(unsigned int));
	if (!chain_offsets)
		ebt_print_memory();
	/* Determine size */
	for (i = 0; i < u_repl->num_chains; i++) {
		if (!(entries = u_repl->chains[i]))
			continue;
		chain_offsets[i] = entries_size;
		entries_size += sizeof(struct ebt_entries);
		j = 0;
		e = entries->entries->next;
		while (e != entries->entries) {
			j++;
			entries_size += sizeof(struct ebt_entry);
			m_l = e->m_list;
			while (m_l) {
				entries_size += m_l->m->match_size +
				   sizeof(struct ebt_entry_match);
				m_l = m_l->next;
			}
			w_l = e->w_list;
			while (w_l) {
				entries_size += w_l->w->watcher_size +
				   sizeof(struct ebt_entry_watcher);
				w_l = w_l->next;
			}
			entries_size += e->t->target_size +
			   sizeof(struct ebt_entry_target);
			e = e->next;
		}
		/* A little sanity check */
		if (j != entries->nentries)
			ebt_print_bug("Wrong nentries: %d != %d, hook = %s", j,
			   entries->nentries, entries->name);
	}

	new->entries_size = entries_size;
	p = (char *)malloc(entries_size);
	if (!p)
		ebt_print_memory();

	/* Put everything in one block */
	new->entries = sparc_cast p;
	for (i = 0; i < u_repl->num_chains; i++) {
		struct ebt_entries *hlp;

		hlp = (struct ebt_entries *)p;
		if (!(entries = u_repl->chains[i]))
			continue;
		if (i < NF_BR_NUMHOOKS)
			new->hook_entry[i] = sparc_cast hlp;
		hlp->nentries = entries->nentries;
		hlp->policy = entries->policy;
		strcpy(hlp->name, entries->name);
		hlp->counter_offset = entries->counter_offset;
		hlp->distinguisher = 0; /* Make the kernel see the light */
		p += sizeof(struct ebt_entries);
		e = entries->entries->next;
		while (e != entries->entries) {
			struct ebt_entry *tmp = (struct ebt_entry *)p;

			tmp->bitmask = e->bitmask | EBT_ENTRY_OR_ENTRIES;
			tmp->invflags = e->invflags;
			tmp->ethproto = e->ethproto;
			strcpy(tmp->in, e->in);
			strcpy(tmp->out, e->out);
			strcpy(tmp->logical_in, e->logical_in);
			strcpy(tmp->logical_out, e->logical_out);
			memcpy(tmp->sourcemac, e->sourcemac,
			   sizeof(tmp->sourcemac));
			memcpy(tmp->sourcemsk, e->sourcemsk,
			   sizeof(tmp->sourcemsk));
			memcpy(tmp->destmac, e->destmac, sizeof(tmp->destmac));
			memcpy(tmp->destmsk, e->destmsk, sizeof(tmp->destmsk));

			base = p;
			p += sizeof(struct ebt_entry);
			m_l = e->m_list;
			while (m_l) {
				memcpy(p, m_l->m, m_l->m->match_size +
				   sizeof(struct ebt_entry_match));
				p += m_l->m->match_size +
				   sizeof(struct ebt_entry_match);
				m_l = m_l->next;
			}
			tmp->watchers_offset = p - base;
			w_l = e->w_list;
			while (w_l) {
				memcpy(p, w_l->w, w_l->w->watcher_size +
				   sizeof(struct ebt_entry_watcher));
				p += w_l->w->watcher_size +
				   sizeof(struct ebt_entry_watcher);
				w_l = w_l->next;
			}
			tmp->target_offset = p - base;
			memcpy(p, e->t, e->t->target_size +
			   sizeof(struct ebt_entry_target));
			if (!strcmp(e->t->u.name, EBT_STANDARD_TARGET)) {
				struct ebt_standard_target *st =
				   (struct ebt_standard_target *)p;
				/* Translate the jump to a udc */
				if (st->verdict >= 0)
					st->verdict = chain_offsets
					   [st->verdict + NF_BR_NUMHOOKS];
			}
			p += e->t->target_size +
			   sizeof(struct ebt_entry_target);
			tmp->next_offset = p - base;
			e = e->next;
		}
	}

	/* Sanity check */
	if (p - (char *)new->entries != new->entries_size)
		ebt_print_bug("Entries_size bug");
	free(chain_offsets);
	return new;
}

static void store_table_in_file(char *filename, struct ebt_replace *repl)
{
	char *data;
	int size;
	int fd;

	/* Start from an empty file with the correct priviliges */
	if ((fd = creat(filename, 0600)) == -1) {
		ebt_print_error("Couldn't create file %s", filename);
		return;
	}

	size = sizeof(struct ebt_replace) + repl->entries_size +
	   repl->nentries * sizeof(struct ebt_counter);
	data = (char *)malloc(size);
	if (!data)
		ebt_print_memory();
	memcpy(data, repl, sizeof(struct ebt_replace));
	memcpy(data + sizeof(struct ebt_replace), (char *)repl->entries,
	   repl->entries_size);
	/* Initialize counters to zero, deliver_counters() can update them */
	memset(data + sizeof(struct ebt_replace) + repl->entries_size,
	   0, repl->nentries * sizeof(struct ebt_counter));
	if (write(fd, data, size) != size)
		ebt_print_error("Couldn't write everything to file %s",
				filename);
	close(fd);
	free(data);
}

void ebt_deliver_table(struct ebt_u_replace *u_repl)
{
	socklen_t optlen;
	struct ebt_replace *repl;

	/* Translate the struct ebt_u_replace to a struct ebt_replace */
	repl = translate_user2kernel(u_repl);
	if (u_repl->filename != NULL) {
		store_table_in_file(u_repl->filename, repl);
		goto free_repl;
	}
	/* Give the data to the kernel */
	optlen = sizeof(struct ebt_replace) + repl->entries_size;
	if (get_sockfd())
		goto free_repl;
	if (!setsockopt(sockfd, IPPROTO_IP, EBT_SO_SET_ENTRIES, repl, optlen))
		goto free_repl;
	if (u_repl->command == 8) { /* The ebtables module may not
	                             * yet be loaded with --atomic-commit */
		ebtables_insmod("ebtables");
		if (!setsockopt(sockfd, IPPROTO_IP, EBT_SO_SET_ENTRIES,
		    repl, optlen))
			goto free_repl;
	}

	ebt_print_error("Unable to update the kernel. Two possible causes:\n"
			"1. Multiple ebtables programs were executing simultaneously. The ebtables\n"
			"   userspace tool doesn't by default support multiple ebtables programs running\n"
			"   concurrently. The ebtables option --concurrent or a tool like flock can be\n"
			"   used to support concurrent scripts that update the ebtables kernel tables.\n"
			"2. The kernel doesn't support a certain ebtables extension, consider\n"
			"   recompiling your kernel or insmod the extension.\n");
free_repl:
	if (repl) {
		free(repl->entries);
		free(repl);
	}
}

static int store_counters_in_file(char *filename, struct ebt_u_replace *repl)
{
	int size = repl->nentries * sizeof(struct ebt_counter), ret = 0;
	unsigned int entries_size;
	struct ebt_replace hlp;
	FILE *file;

	if (!(file = fopen(filename, "r+b"))) {
		ebt_print_error("Could not open file %s", filename);
		return -1;
	}
	/* Find out entries_size and then set the file pointer to the
	 * counters */
	if (fseek(file, (char *)(&hlp.entries_size) - (char *)(&hlp), SEEK_SET)
	   || fread(&entries_size, sizeof(char), sizeof(unsigned int), file) !=
	   sizeof(unsigned int) ||
	   fseek(file, entries_size + sizeof(struct ebt_replace), SEEK_SET)) {
		ebt_print_error("File %s is corrupt", filename);
		ret = -1;
		goto close_file;
	}
	if (fwrite(repl->counters, sizeof(char), size, file) != size) {
		ebt_print_error("Could not write everything to file %s",
				filename);
		ret = -1;
	}
close_file:
	fclose(file);
	return 0;
}

/* Gets executed after ebt_deliver_table. Delivers the counters to the kernel
 * and resets the counterchanges to CNT_NORM */
void ebt_deliver_counters(struct ebt_u_replace *u_repl)
{
	struct ebt_counter *old, *new, *newcounters;
	socklen_t optlen;
	struct ebt_replace repl;
	struct ebt_cntchanges *cc = u_repl->cc->next, *cc2;
	struct ebt_u_entries *entries = NULL;
	struct ebt_u_entry *next = NULL;
	int i, chainnr = -1;

	if (u_repl->nentries == 0)
		return;

	newcounters = (struct ebt_counter *)
	   malloc(u_repl->nentries * sizeof(struct ebt_counter));
	if (!newcounters)
		ebt_print_memory();
	memset(newcounters, 0, u_repl->nentries * sizeof(struct ebt_counter));
	old = u_repl->counters;
	new = newcounters;
	while (cc != u_repl->cc) {
		if (!next || next == entries->entries) {
			chainnr++;
			while (chainnr < u_repl->num_chains && (!(entries = u_repl->chains[chainnr]) ||
			       (next = entries->entries->next) == entries->entries))
				chainnr++;
			if (chainnr == u_repl->num_chains)
				break;
		}
		if (next == NULL)
			ebt_print_bug("next == NULL");
		if (cc->type == CNT_NORM) {
			/* 'Normal' rule, meaning we didn't do anything to it
			 * So, we just copy */
			*new = *old;
			next->cnt = *new;
			next->cnt_surplus.pcnt = next->cnt_surplus.bcnt = 0;
			old++; /* We've used an old counter */
			new++; /* We've set a new counter */
			next = next->next;
		} else if (cc->type == CNT_DEL) {
			old++; /* Don't use this old counter */
		} else {
			if (cc->type == CNT_CHANGE) {
				if (cc->change % 3 == 1)
					new->pcnt = old->pcnt + next->cnt_surplus.pcnt;
				else if (cc->change % 3 == 2)
					new->pcnt = old->pcnt - next->cnt_surplus.pcnt;
				else
					new->pcnt = next->cnt.pcnt;
				if (cc->change / 3 == 1)
					new->bcnt = old->bcnt + next->cnt_surplus.bcnt;
				else if (cc->change / 3 == 2)
					new->bcnt = old->bcnt - next->cnt_surplus.bcnt;
				else
					new->bcnt = next->cnt.bcnt;
			} else
				*new = next->cnt;
			next->cnt = *new;
			next->cnt_surplus.pcnt = next->cnt_surplus.bcnt = 0;
			if (cc->type == CNT_ADD)
				new++;
			else {
				old++;
				new++;
			}
			next = next->next;
		}
		cc = cc->next;
	}

	free(u_repl->counters);
	u_repl->counters = newcounters;
	u_repl->num_counters = u_repl->nentries;
	/* Reset the counterchanges to CNT_NORM and delete the unused cc */
	i = 0;
	cc = u_repl->cc->next;
	while (cc != u_repl->cc) {
		if (cc->type == CNT_DEL) {
			cc->prev->next = cc->next;
			cc->next->prev = cc->prev;
			cc2 = cc->next;
			free(cc);
			cc = cc2;
		} else {
			cc->type = CNT_NORM;
			cc->change = 0;
			i++;
			cc = cc->next;
		}
	}
	if (i != u_repl->nentries)
		ebt_print_bug("i != u_repl->nentries");
	if (u_repl->filename != NULL) {
		store_counters_in_file(u_repl->filename, u_repl);
		return;
	}
	optlen = u_repl->nentries * sizeof(struct ebt_counter) +
	   sizeof(struct ebt_replace);
	/* Now put the stuff in the kernel's struct ebt_replace */
	repl.counters = sparc_cast u_repl->counters;
	repl.num_counters = u_repl->num_counters;
	memcpy(repl.name, u_repl->name, sizeof(repl.name));

	if (get_sockfd())
		return;
	if (setsockopt(sockfd, IPPROTO_IP, EBT_SO_SET_COUNTERS, &repl, optlen))
		ebt_print_bug("Couldn't update kernel counters");
}

static int
ebt_translate_match(struct ebt_entry_match *m, struct ebt_u_match_list ***l)
{
	struct ebt_u_match_list *new;
	int ret = 0;

	new = (struct ebt_u_match_list *)
	   malloc(sizeof(struct ebt_u_match_list));
	if (!new)
		ebt_print_memory();
	new->m = (struct ebt_entry_match *)
	   malloc(m->match_size + sizeof(struct ebt_entry_match));
	if (!new->m)
		ebt_print_memory();
	memcpy(new->m, m, m->match_size + sizeof(struct ebt_entry_match));
	new->next = NULL;
	**l = new;
	*l = &new->next;
	if (ebt_find_match(new->m->u.name) == NULL) {
		ebt_print_error("Kernel match %s unsupported by userspace tool",
				new->m->u.name);
		ret = -1;
	}
	return ret;
}

static int
ebt_translate_watcher(struct ebt_entry_watcher *w,
   struct ebt_u_watcher_list ***l)
{
	struct ebt_u_watcher_list *new;
	int ret = 0;

	new = (struct ebt_u_watcher_list *)
	   malloc(sizeof(struct ebt_u_watcher_list));
	if (!new)
		ebt_print_memory();
	new->w = (struct ebt_entry_watcher *)
	   malloc(w->watcher_size + sizeof(struct ebt_entry_watcher));
	if (!new->w)
		ebt_print_memory();
	memcpy(new->w, w, w->watcher_size + sizeof(struct ebt_entry_watcher));
	new->next = NULL;
	**l = new;
	*l = &new->next;
	if (ebt_find_watcher(new->w->u.name) == NULL) {
		ebt_print_error("Kernel watcher %s unsupported by userspace "
				"tool", new->w->u.name);
		ret = -1;
	}
	return ret;
}

static int
ebt_translate_entry(struct ebt_entry *e, int *hook, int *n, int *cnt,
   int *totalcnt, struct ebt_u_entry **u_e, struct ebt_u_replace *u_repl,
   unsigned int valid_hooks, char *base, struct ebt_cntchanges **cc)
{
	/* An entry */
	if (e->bitmask & EBT_ENTRY_OR_ENTRIES) {
		struct ebt_u_entry *new;
		struct ebt_u_match_list **m_l;
		struct ebt_u_watcher_list **w_l;
		struct ebt_entry_target *t;

		new = (struct ebt_u_entry *)malloc(sizeof(struct ebt_u_entry));
		if (!new)
			ebt_print_memory();
		new->bitmask = e->bitmask;
		/*
		 * Plain userspace code doesn't know about
		 * EBT_ENTRY_OR_ENTRIES
		 */
		new->bitmask &= ~EBT_ENTRY_OR_ENTRIES;
		new->invflags = e->invflags;
		new->ethproto = e->ethproto;
		strcpy(new->in, e->in);
		strcpy(new->out, e->out);
		strcpy(new->logical_in, e->logical_in);
		strcpy(new->logical_out, e->logical_out);
		memcpy(new->sourcemac, e->sourcemac, sizeof(new->sourcemac));
		memcpy(new->sourcemsk, e->sourcemsk, sizeof(new->sourcemsk));
		memcpy(new->destmac, e->destmac, sizeof(new->destmac));
		memcpy(new->destmsk, e->destmsk, sizeof(new->destmsk));
		if (*totalcnt >= u_repl->nentries)
			ebt_print_bug("*totalcnt >= u_repl->nentries");
		new->cnt = u_repl->counters[*totalcnt];
		new->cnt_surplus.pcnt = new->cnt_surplus.bcnt = 0;
		new->cc = *cc;
		*cc = (*cc)->next;
		new->m_list = NULL;
		new->w_list = NULL;
		new->next = (*u_e)->next;
		new->next->prev = new;
		(*u_e)->next = new;
		new->prev = *u_e;
		*u_e = new;
		m_l = &new->m_list;
		EBT_MATCH_ITERATE(e, ebt_translate_match, &m_l);
		w_l = &new->w_list;
		EBT_WATCHER_ITERATE(e, ebt_translate_watcher, &w_l);

		t = (struct ebt_entry_target *)(((char *)e) + e->target_offset);
		new->t = (struct ebt_entry_target *)
		   malloc(t->target_size + sizeof(struct ebt_entry_target));
		if (!new->t)
			ebt_print_memory();
		if (ebt_find_target(t->u.name) == NULL) {
			ebt_print_error("Kernel target %s unsupported by "
					"userspace tool", t->u.name);
			return -1;
		}
		memcpy(new->t, t, t->target_size +
		   sizeof(struct ebt_entry_target));
		/* Deal with jumps to udc */
		if (!strcmp(t->u.name, EBT_STANDARD_TARGET)) {
			char *tmp = base;
			int verdict = ((struct ebt_standard_target *)t)->verdict;
			int i;

			if (verdict >= 0) {
				tmp += verdict;
				for (i = NF_BR_NUMHOOKS; i < u_repl->num_chains; i++)
					if (u_repl->chains[i]->kernel_start == tmp)
						break;
				if (i == u_repl->num_chains)
					ebt_print_bug("Can't find udc for jump");
				((struct ebt_standard_target *)new->t)->verdict = i-NF_BR_NUMHOOKS;
			}
		}

		(*cnt)++;
		(*totalcnt)++;
		return 0;
	} else { /* A new chain */
		int i;
		struct ebt_entries *entries = (struct ebt_entries *)e;

		if (*n != *cnt)
			ebt_print_bug("Nr of entries in the chain is wrong");
		*n = entries->nentries;
		*cnt = 0;
		for (i = *hook + 1; i < NF_BR_NUMHOOKS; i++)
			if (valid_hooks & (1 << i))
				break;
		*hook = i;
		*u_e = u_repl->chains[*hook]->entries;
		return 0;
	}
}

/* Initialize all chain headers */
static int
ebt_translate_chains(struct ebt_entry *e, int *hook,
   struct ebt_u_replace *u_repl, unsigned int valid_hooks)
{
	int i;
	struct ebt_entries *entries = (struct ebt_entries *)e;
	struct ebt_u_entries *new;

	if (!(e->bitmask & EBT_ENTRY_OR_ENTRIES)) {
		for (i = *hook + 1; i < NF_BR_NUMHOOKS; i++)
			if (valid_hooks & (1 << i))
				break;
		new = (struct ebt_u_entries *)malloc(sizeof(struct ebt_u_entries));
		if (!new)
			ebt_print_memory();
		if (i == u_repl->max_chains)
			ebt_double_chains(u_repl);
		u_repl->chains[i] = new;
		if (i >= NF_BR_NUMHOOKS)
			new->kernel_start = (char *)e;
		*hook = i;
		new->nentries = entries->nentries;
		new->policy = entries->policy;
		new->entries = (struct ebt_u_entry *)malloc(sizeof(struct ebt_u_entry));
		if (!new->entries)
			ebt_print_memory();
		new->entries->next = new->entries->prev = new->entries;
		new->counter_offset = entries->counter_offset;
		strcpy(new->name, entries->name);
	}
	return 0;
}

static int retrieve_from_file(char *filename, struct ebt_replace *repl,
   char command)
{
	FILE *file;
	char *hlp = NULL, *entries;
	struct ebt_counter *counters;
	int size, ret = 0;

	if (!(file = fopen(filename, "r+b"))) {
		ebt_print_error("Could not open file %s", filename);
		return -1;
	}
	/* Make sure table name is right if command isn't -L or --atomic-commit */
	if (command != 'L' && command != 8) {
		hlp = (char *)malloc(strlen(repl->name) + 1);
		if (!hlp)
			ebt_print_memory();
		strcpy(hlp, repl->name);
	}
	if (fread(repl, sizeof(char), sizeof(struct ebt_replace), file)
	   != sizeof(struct ebt_replace)) {
		ebt_print_error("File %s is corrupt", filename);
		ret = -1;
		goto close_file;
	}
	if (command != 'L' && command != 8 && strcmp(hlp, repl->name)) {
		ebt_print_error("File %s contains wrong table name or is "
				"corrupt", filename);
		ret = -1;
		goto close_file;
	} else if (!ebt_find_table(repl->name)) {
		ebt_print_error("File %s contains invalid table name",
				filename);
		ret = -1;
		goto close_file;
	}

	size = sizeof(struct ebt_replace) +
	   repl->nentries * sizeof(struct ebt_counter) + repl->entries_size;
	fseek(file, 0, SEEK_END);
	if (size != ftell(file)) {
		ebt_print_error("File %s has wrong size", filename);
		ret = -1;
		goto close_file;
	}
	entries = (char *)malloc(repl->entries_size);
	if (!entries)
		ebt_print_memory();
	repl->entries = sparc_cast entries;
	if (repl->nentries) {
		counters = (struct ebt_counter *)
		   malloc(repl->nentries * sizeof(struct ebt_counter));
		repl->counters = sparc_cast counters;
		if (!repl->counters)
			ebt_print_memory();
	} else
		repl->counters = sparc_cast NULL;
	/* Copy entries and counters */
	if (fseek(file, sizeof(struct ebt_replace), SEEK_SET) ||
	   fread((char *)repl->entries, sizeof(char), repl->entries_size, file)
	   != repl->entries_size ||
	   fseek(file, sizeof(struct ebt_replace) + repl->entries_size,
		 SEEK_SET)
	   || (repl->counters && fread((char *)repl->counters, sizeof(char),
	   repl->nentries * sizeof(struct ebt_counter), file)
	   != repl->nentries * sizeof(struct ebt_counter))) {
		ebt_print_error("File %s is corrupt", filename);
		free(entries);
		repl->entries = NULL;
		ret = -1;
	}
close_file:
	fclose(file);
	free(hlp);
	return ret;
}

static int retrieve_from_kernel(struct ebt_replace *repl, char command,
				int init)
{
	socklen_t optlen;
	int optname;
	char *entries;

	optlen = sizeof(struct ebt_replace);
	if (get_sockfd())
		return -1;
	/* --atomic-init || --init-table */
	if (init)
		optname = EBT_SO_GET_INIT_INFO;
	else
		optname = EBT_SO_GET_INFO;
	if (getsockopt(sockfd, IPPROTO_IP, optname, repl, &optlen))
		return -1;

	if ( !(entries = (char *)malloc(repl->entries_size)) )
		ebt_print_memory();
	repl->entries = sparc_cast entries;
	if (repl->nentries) {
		struct ebt_counter *counters;

		if (!(counters = (struct ebt_counter *)
		   malloc(repl->nentries * sizeof(struct ebt_counter))) )
			ebt_print_memory();
		repl->counters = sparc_cast counters;
	}
	else
		repl->counters = sparc_cast NULL;

	/* We want to receive the counters */
	repl->num_counters = repl->nentries;
	optlen += repl->entries_size + repl->num_counters *
	   sizeof(struct ebt_counter);
	if (init)
		optname = EBT_SO_GET_INIT_ENTRIES;
	else
		optname = EBT_SO_GET_ENTRIES;
	if (getsockopt(sockfd, IPPROTO_IP, optname, repl, &optlen))
		ebt_print_bug("Hmm, what is wrong??? bug#1");

	return 0;
}

int ebt_get_table(struct ebt_u_replace *u_repl, int init)
{
	int i, j, k, hook;
	struct ebt_replace repl;
	struct ebt_u_entry *u_e = NULL;
	struct ebt_cntchanges *new_cc = NULL, *cc;

	strcpy(repl.name, u_repl->name);
	if (u_repl->filename != NULL) {
		if (init)
			ebt_print_bug("Getting initial table data from a file is impossible");
		if (retrieve_from_file(u_repl->filename, &repl, u_repl->command))
			return -1;
		/* -L with a wrong table name should be dealt with silently */
		strcpy(u_repl->name, repl.name);
	} else if (retrieve_from_kernel(&repl, u_repl->command, init))
		return -1;

	/* Translate the struct ebt_replace to a struct ebt_u_replace */
	u_repl->valid_hooks = repl.valid_hooks;
	u_repl->nentries = repl.nentries;
	u_repl->num_counters = repl.num_counters;
	u_repl->counters = repl.counters;
	u_repl->cc = (struct ebt_cntchanges *)malloc(sizeof(struct ebt_cntchanges));
	if (!u_repl->cc)
		ebt_print_memory();
	u_repl->cc->next = u_repl->cc->prev = u_repl->cc;
	cc = u_repl->cc;
	for (i = 0; i < repl.nentries; i++) {
		new_cc = (struct ebt_cntchanges *)malloc(sizeof(struct ebt_cntchanges));
		if (!new_cc)
			ebt_print_memory();
		new_cc->type = CNT_NORM;
		new_cc->change = 0;
		new_cc->prev = cc;
		cc->next = new_cc;
		cc = new_cc;
	}
	if (repl.nentries) {
		new_cc->next = u_repl->cc;
		u_repl->cc->prev = new_cc;
	}
	u_repl->chains = (struct ebt_u_entries **)calloc(EBT_ORI_MAX_CHAINS, sizeof(void *));
	u_repl->max_chains = EBT_ORI_MAX_CHAINS;
	hook = -1;
	/* FIXME: Clean up when an error is encountered */
	EBT_ENTRY_ITERATE(repl.entries, repl.entries_size, ebt_translate_chains,
	   &hook, u_repl, u_repl->valid_hooks);
	if (hook >= NF_BR_NUMHOOKS)
		u_repl->num_chains = hook + 1;
	else
		u_repl->num_chains = NF_BR_NUMHOOKS;
	i = 0; /* Holds the expected nr. of entries for the chain */
	j = 0; /* Holds the up to now counted entries for the chain */
	k = 0; /* Holds the total nr. of entries, should equal u_repl->nentries afterwards */
	cc = u_repl->cc->next;
	hook = -1;
	EBT_ENTRY_ITERATE((char *)repl.entries, repl.entries_size,
	   ebt_translate_entry, &hook, &i, &j, &k, &u_e, u_repl,
	   u_repl->valid_hooks, (char *)repl.entries, &cc);
	if (k != u_repl->nentries)
		ebt_print_bug("Wrong total nentries");
	free(repl.entries);
	return 0;
}
