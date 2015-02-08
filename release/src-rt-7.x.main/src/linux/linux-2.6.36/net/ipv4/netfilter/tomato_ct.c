/*

	tomato_ct.c
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2.

*/
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/proc_fs.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_helper.h>

//	#define TEST_HASHDIST


#ifdef TEST_HASHDIST
static int hashdist_read(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	struct nf_conntrack_tuple_hash *h;
	int i;
	int n;
	int count;
	char *buf;
	int max;
	
	// do this the easy way...
	max = nf_conntrack_htable_size * sizeof("12345\t12345\n");
	buf = kmalloc(max + 1, GFP_KERNEL);
	if (buf == NULL) return 0;

	n = 0;
	max -= sizeof("12345\t12345\n");
	
	read_lock_bh(&nf_conntrack_lock);

	for (i = 0; i < nf_conntrack_htable_size; ++i) {
		count = 0;
		list_for_each_entry(h, &nf_conntrack_hash[i], list) {
			++count;
		}
		n += sprintf(buf + n, "%d\t%d\n", i, count);
		if (n > max) {
			printk("hashdist: %d > %d\n", n, max);
			break;
		}
	}
	
	read_unlock_bh(&nf_conntrack_lock);
	
	if (offset < n) {
		n = n - offset;
		if (n > length) {
			n = length;
			*eof = 0;
		}			
		else {
			*eof = 1;
		}
		memcpy(buffer, buf + offset, n);
		*start = buffer;
	}
	else {
		n = 0;
		*eof = 1;
	}
	
	kfree(buf);
	return n;
}
#endif


static void interate_all(void (*func)(struct nf_conn *, unsigned long), unsigned long data)
{
	unsigned int i;
	struct nf_conntrack_tuple_hash *h;
	
	write_lock_bh(&nf_conntrack_lock);
	for (i = 0; i < nf_conntrack_htable_size; ++i) {
		list_for_each_entry(h, &nf_conntrack_hash[i], list) {
			func(nf_ct_tuplehash_to_ctrack(h), data);
		}
	}
	write_unlock_bh(&nf_conntrack_lock);
}

static void expireearly(struct nf_conn *ct, unsigned long data)
{
	if (ct->timeout.expires > data) {
		if (del_timer(&ct->timeout)) {
			ct->timeout.expires = data;
			add_timer(&ct->timeout);
		}
	}
}

static int expireearly_write(struct file *file, const char *buffer, unsigned long length, void *data)
{
	char s[8];
	unsigned long n;
	
	if ((length > 0) && (length < 6)) {
		memcpy(s, buffer, length);
		s[length] = 0;
		n = simple_strtoul(s, NULL, 10);
		if (n < 10) n = 10;
			else if (n > 86400) n = 86400;
		
		interate_all(expireearly, jiffies + (n * HZ));
	}

/*	
	if ((length > 0) && (buffer[0] == '1')) {
		interate_all(expireearly, jiffies + (20 * HZ));
	}
*/
	
	return length;
}


static void clearmarks(struct nf_conn *ct, unsigned long data)
{
	ct->mark = 0;
}

static int clearmarks_write(struct file *file, const char *buffer, unsigned long length, void *data)
{
	if ((length > 0) && (buffer[0] == '1')) {
		interate_all(clearmarks, 0);
	}
	return length;
}

static int __init init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *p;

	printk(__FILE__ " [" __DATE__ " " __TIME__ "]\n");

#ifdef TEST_HASHDIST
	p = create_proc_entry("hash_dist", 0400, proc_net);
	if (p) {
		p->owner = THIS_MODULE;
		p->read_proc = hashdist_read;
	}
#endif

	p = create_proc_entry("expire_early", 0200, proc_net);
	if (p) {
		p->owner = THIS_MODULE;
		p->write_proc = expireearly_write;
	}
	
	p = create_proc_entry("clear_marks", 0200, proc_net);
	if (p) {
		p->owner = THIS_MODULE;
		p->write_proc = clearmarks_write;
	}
#endif /* CONFIG_PROC_FS */
	
	return 0;
}

static void __exit fini(void)
{
#ifdef CONFIG_PROC_FS
#ifdef TEST_HASHDIST
	remove_proc_entry("hash_dist", proc_net);
#endif
	remove_proc_entry("expire_early", proc_net);
	remove_proc_entry("clear_marks", proc_net);
#endif /* CONFIG_PROC_FS */
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL");
