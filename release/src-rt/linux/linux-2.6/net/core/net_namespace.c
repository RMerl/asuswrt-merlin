#include <linux/workqueue.h>
#include <linux/rtnetlink.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/idr.h>
#include <linux/rculist.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

/*
 *	Our network namespace constructor/destructor lists
 */

static LIST_HEAD(pernet_list);
static struct list_head *first_device = &pernet_list;
static DEFINE_MUTEX(net_mutex);

LIST_HEAD(net_namespace_list);
EXPORT_SYMBOL_GPL(net_namespace_list);

struct net init_net;
EXPORT_SYMBOL(init_net);

#define INITIAL_NET_GEN_PTRS	13 /* +1 for len +2 for rcu_head */

static void net_generic_release(struct rcu_head *rcu)
{
	struct net_generic *ng;

	ng = container_of(rcu, struct net_generic, rcu);
	kfree(ng);
}

static int net_assign_generic(struct net *net, int id, void *data)
{
	struct net_generic *ng, *old_ng;

	BUG_ON(!mutex_is_locked(&net_mutex));
	BUG_ON(id == 0);

	old_ng = rcu_dereference_protected(net->gen,
					   lockdep_is_held(&net_mutex));
	ng = old_ng;
	if (old_ng->len >= id)
		goto assign;

	ng = kzalloc(sizeof(struct net_generic) +
			id * sizeof(void *), GFP_KERNEL);
	if (ng == NULL)
		return -ENOMEM;

	/*
	 * Some synchronisation notes:
	 *
	 * The net_generic explores the net->gen array inside rcu
	 * read section. Besides once set the net->gen->ptr[x]
	 * pointer never changes (see rules in netns/generic.h).
	 *
	 * That said, we simply duplicate this array and schedule
	 * the old copy for kfree after a grace period.
	 */

	ng->len = id;
	memcpy(&ng->ptr, &old_ng->ptr, old_ng->len * sizeof(void*));

	rcu_assign_pointer(net->gen, ng);
	call_rcu(&old_ng->rcu, net_generic_release);
assign:
	ng->ptr[id - 1] = data;
	return 0;
}

static int ops_init(const struct pernet_operations *ops, struct net *net)
{
	int err;
	if (ops->id && ops->size) {
		void *data = kzalloc(ops->size, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		err = net_assign_generic(net, *ops->id, data);
		if (err) {
			kfree(data);
			return err;
		}
	}
	if (ops->init)
		return ops->init(net);
	return 0;
}

static void ops_free(const struct pernet_operations *ops, struct net *net)
{
	if (ops->id && ops->size) {
		int id = *ops->id;
		kfree(net_generic(net, id));
	}
}

static void ops_exit_list(const struct pernet_operations *ops,
			  struct list_head *net_exit_list)
{
	struct net *net;
	if (ops->exit) {
		list_for_each_entry(net, net_exit_list, exit_list)
			ops->exit(net);
	}
	if (ops->exit_batch)
		ops->exit_batch(net_exit_list);
}

static void ops_free_list(const struct pernet_operations *ops,
			  struct list_head *net_exit_list)
{
	struct net *net;
	if (ops->size && ops->id) {
		list_for_each_entry(net, net_exit_list, exit_list)
			ops_free(ops, net);
	}
}

/*
 * setup_net runs the initializers for the network namespace object.
 */
static __net_init int setup_net(struct net *net)
{
	/* Must be called with net_mutex held */
	const struct pernet_operations *ops, *saved_ops;
	int error = 0;
	LIST_HEAD(net_exit_list);

	atomic_set(&net->count, 1);

#ifdef NETNS_REFCNT_DEBUG
	atomic_set(&net->use_count, 0);
#endif

	list_for_each_entry(ops, &pernet_list, list) {
		error = ops_init(ops, net);
		if (error < 0)
			goto out_undo;
	}
out:
	return error;

out_undo:
	/* Walk through the list backwards calling the exit functions
	 * for the pernet modules whose init functions did not fail.
	 */
	list_add(&net->exit_list, &net_exit_list);
	saved_ops = ops;
	list_for_each_entry_continue_reverse(ops, &pernet_list, list)
		ops_exit_list(ops, &net_exit_list);

	ops = saved_ops;
	list_for_each_entry_continue_reverse(ops, &pernet_list, list)
		ops_free_list(ops, &net_exit_list);

	rcu_barrier();
	goto out;
}

static struct net_generic *net_alloc_generic(void)
{
	struct net_generic *ng;
	size_t generic_size = sizeof(struct net_generic) +
		INITIAL_NET_GEN_PTRS * sizeof(void *);

	ng = kzalloc(generic_size, GFP_KERNEL);
	if (ng)
		ng->len = INITIAL_NET_GEN_PTRS;

	return ng;
}

#ifdef CONFIG_NET_NS
static struct kmem_cache *net_cachep;
static struct workqueue_struct *netns_wq;

static struct net *net_alloc(void)
{
	struct net *net = NULL;
	struct net_generic *ng;

	ng = net_alloc_generic();
	if (!ng)
		goto out;

	net = kmem_cache_zalloc(net_cachep, GFP_KERNEL);
	if (!net)
		goto out_free;

	rcu_assign_pointer(net->gen, ng);
out:
	return net;

out_free:
	kfree(ng);
	goto out;
}

static void net_free(struct net *net)
{
#ifdef NETNS_REFCNT_DEBUG
	if (unlikely(atomic_read(&net->use_count) != 0)) {
		printk(KERN_EMERG "network namespace not free! Usage: %d\n",
			atomic_read(&net->use_count));
		return;
	}
#endif
	kfree(net->gen);
	kmem_cache_free(net_cachep, net);
}

static struct net *net_create(void)
{
	struct net *net;
	int rv;

	net = net_alloc();
	if (!net)
		return ERR_PTR(-ENOMEM);
	mutex_lock(&net_mutex);
	rv = setup_net(net);
	if (rv == 0) {
		rtnl_lock();
		list_add_tail_rcu(&net->list, &net_namespace_list);
		rtnl_unlock();
	}
	mutex_unlock(&net_mutex);
	if (rv < 0) {
		net_free(net);
		return ERR_PTR(rv);
	}
	return net;
}

struct net *copy_net_ns(unsigned long flags, struct net *old_net)
{
	if (!(flags & CLONE_NEWNET))
		return get_net(old_net);
	return net_create();
}

static DEFINE_SPINLOCK(cleanup_list_lock);
static LIST_HEAD(cleanup_list);  /* Must hold cleanup_list_lock to touch */

static void cleanup_net(struct work_struct *work)
{
	const struct pernet_operations *ops;
	struct net *net, *tmp;
	LIST_HEAD(net_kill_list);
	LIST_HEAD(net_exit_list);

	/* Atomically snapshot the list of namespaces to cleanup */
	spin_lock_irq(&cleanup_list_lock);
	list_replace_init(&cleanup_list, &net_kill_list);
	spin_unlock_irq(&cleanup_list_lock);

	mutex_lock(&net_mutex);

	/* Don't let anyone else find us. */
	rtnl_lock();
	list_for_each_entry(net, &net_kill_list, cleanup_list) {
		list_del_rcu(&net->list);
		list_add_tail(&net->exit_list, &net_exit_list);
	}
	rtnl_unlock();

	/*
	 * Another CPU might be rcu-iterating the list, wait for it.
	 * This needs to be before calling the exit() notifiers, so
	 * the rcu_barrier() below isn't sufficient alone.
	 */
	synchronize_rcu();

	/* Run all of the network namespace exit methods */
	list_for_each_entry_reverse(ops, &pernet_list, list)
		ops_exit_list(ops, &net_exit_list);

	/* Free the net generic variables */
	list_for_each_entry_reverse(ops, &pernet_list, list)
		ops_free_list(ops, &net_exit_list);

	mutex_unlock(&net_mutex);

	/* Ensure there are no outstanding rcu callbacks using this
	 * network namespace.
	 */
	rcu_barrier();

	/* Finally it is safe to free my network namespace structure */
	list_for_each_entry_safe(net, tmp, &net_exit_list, exit_list) {
		list_del_init(&net->exit_list);
		net_free(net);
	}
}
static DECLARE_WORK(net_cleanup_work, cleanup_net);

void __put_net(struct net *net)
{
	/* Cleanup the network namespace in process context */
	unsigned long flags;

	spin_lock_irqsave(&cleanup_list_lock, flags);
	list_add(&net->cleanup_list, &cleanup_list);
	spin_unlock_irqrestore(&cleanup_list_lock, flags);

	queue_work(netns_wq, &net_cleanup_work);
}
EXPORT_SYMBOL_GPL(__put_net);

#else
struct net *copy_net_ns(unsigned long flags, struct net *old_net)
{
	if (flags & CLONE_NEWNET)
		return ERR_PTR(-EINVAL);
	return old_net;
}
#endif

struct net *get_net_ns_by_pid(pid_t pid)
{
	struct task_struct *tsk;
	struct net *net;

	/* Lookup the network namespace */
	net = ERR_PTR(-ESRCH);
	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		struct nsproxy *nsproxy;
		nsproxy = task_nsproxy(tsk);
		if (nsproxy)
			net = get_net(nsproxy->net_ns);
	}
	rcu_read_unlock();
	return net;
}
EXPORT_SYMBOL_GPL(get_net_ns_by_pid);

static int __init net_ns_init(void)
{
	struct net_generic *ng;

#ifdef CONFIG_NET_NS
	net_cachep = kmem_cache_create("net_namespace", sizeof(struct net),
					SMP_CACHE_BYTES,
					SLAB_PANIC, NULL);

	/* Create workqueue for cleanup */
	netns_wq = create_singlethread_workqueue("netns");
	if (!netns_wq)
		panic("Could not create netns workq");
#endif

	ng = net_alloc_generic();
	if (!ng)
		panic("Could not allocate generic netns");

	rcu_assign_pointer(init_net.gen, ng);

	mutex_lock(&net_mutex);
	if (setup_net(&init_net))
		panic("Could not setup the initial network namespace");

	rtnl_lock();
	list_add_tail_rcu(&init_net.list, &net_namespace_list);
	rtnl_unlock();

	mutex_unlock(&net_mutex);

	return 0;
}

pure_initcall(net_ns_init);

#ifdef CONFIG_NET_NS
static int __register_pernet_operations(struct list_head *list,
					struct pernet_operations *ops)
{
	struct net *net;
	int error;
	LIST_HEAD(net_exit_list);

	list_add_tail(&ops->list, list);
	if (ops->init || (ops->id && ops->size)) {
		for_each_net(net) {
			error = ops_init(ops, net);
			if (error)
				goto out_undo;
			list_add_tail(&net->exit_list, &net_exit_list);
		}
	}
	return 0;

out_undo:
	/* If I have an error cleanup all namespaces I initialized */
	list_del(&ops->list);
	ops_exit_list(ops, &net_exit_list);
	ops_free_list(ops, &net_exit_list);
	return error;
}

static void __unregister_pernet_operations(struct pernet_operations *ops)
{
	struct net *net;
	LIST_HEAD(net_exit_list);

	list_del(&ops->list);
	for_each_net(net)
		list_add_tail(&net->exit_list, &net_exit_list);
	ops_exit_list(ops, &net_exit_list);
	ops_free_list(ops, &net_exit_list);
}

#else

static int __register_pernet_operations(struct list_head *list,
					struct pernet_operations *ops)
{
	int err = 0;
	err = ops_init(ops, &init_net);
	if (err)
		ops_free(ops, &init_net);
	return err;
	
}

static void __unregister_pernet_operations(struct pernet_operations *ops)
{
	LIST_HEAD(net_exit_list);
	list_add(&init_net.exit_list, &net_exit_list);
	ops_exit_list(ops, &net_exit_list);
	ops_free_list(ops, &net_exit_list);
}

#endif /* CONFIG_NET_NS */

static DEFINE_IDA(net_generic_ids);

static int register_pernet_operations(struct list_head *list,
				      struct pernet_operations *ops)
{
	int error;

	if (ops->id) {
again:
		error = ida_get_new_above(&net_generic_ids, 1, ops->id);
		if (error < 0) {
			if (error == -EAGAIN) {
				ida_pre_get(&net_generic_ids, GFP_KERNEL);
				goto again;
			}
			return error;
		}
	}
	error = __register_pernet_operations(list, ops);
	if (error) {
		rcu_barrier();
		if (ops->id)
			ida_remove(&net_generic_ids, *ops->id);
	}

	return error;
}

static void unregister_pernet_operations(struct pernet_operations *ops)
{
	
	__unregister_pernet_operations(ops);
	rcu_barrier();
	if (ops->id)
		ida_remove(&net_generic_ids, *ops->id);
}

/**
 *      register_pernet_subsys - register a network namespace subsystem
 *	@ops:  pernet operations structure for the subsystem
 *
 *	Register a subsystem which has init and exit functions
 *	that are called when network namespaces are created and
 *	destroyed respectively.
 *
 *	When registered all network namespace init functions are
 *	called for every existing network namespace.  Allowing kernel
 *	modules to have a race free view of the set of network namespaces.
 *
 *	When a new network namespace is created all of the init
 *	methods are called in the order in which they were registered.
 *
 *	When a network namespace is destroyed all of the exit methods
 *	are called in the reverse of the order with which they were
 *	registered.
 */
int register_pernet_subsys(struct pernet_operations *ops)
{
	int error;
	mutex_lock(&net_mutex);
	error =  register_pernet_operations(first_device, ops);
	mutex_unlock(&net_mutex);
	return error;
}
EXPORT_SYMBOL_GPL(register_pernet_subsys);

/**
 *      unregister_pernet_subsys - unregister a network namespace subsystem
 *	@ops: pernet operations structure to manipulate
 *
 *	Remove the pernet operations structure from the list to be
 *	used when network namespaces are created or destroyed.  In
 *	addition run the exit method for all existing network
 *	namespaces.
 */
void unregister_pernet_subsys(struct pernet_operations *ops)
{
	mutex_lock(&net_mutex);
	unregister_pernet_operations(ops);
	mutex_unlock(&net_mutex);
}
EXPORT_SYMBOL_GPL(unregister_pernet_subsys);

/**
 *      register_pernet_device - register a network namespace device
 *	@ops:  pernet operations structure for the subsystem
 *
 *	Register a device which has init and exit functions
 *	that are called when network namespaces are created and
 *	destroyed respectively.
 *
 *	When registered all network namespace init functions are
 *	called for every existing network namespace.  Allowing kernel
 *	modules to have a race free view of the set of network namespaces.
 *
 *	When a new network namespace is created all of the init
 *	methods are called in the order in which they were registered.
 *
 *	When a network namespace is destroyed all of the exit methods
 *	are called in the reverse of the order with which they were
 *	registered.
 */
int register_pernet_device(struct pernet_operations *ops)
{
	int error;
	mutex_lock(&net_mutex);
	error = register_pernet_operations(&pernet_list, ops);
	if (!error && (first_device == &pernet_list))
		first_device = &ops->list;
	mutex_unlock(&net_mutex);
	return error;
}
EXPORT_SYMBOL_GPL(register_pernet_device);

/**
 *      unregister_pernet_device - unregister a network namespace netdevice
 *	@ops: pernet operations structure to manipulate
 *
 *	Remove the pernet operations structure from the list to be
 *	used when network namespaces are created or destroyed.  In
 *	addition run the exit method for all existing network
 *	namespaces.
 */
void unregister_pernet_device(struct pernet_operations *ops)
{
	mutex_lock(&net_mutex);
	if (&ops->list == first_device)
		first_device = first_device->next;
	unregister_pernet_operations(ops);
	mutex_unlock(&net_mutex);
}
EXPORT_SYMBOL_GPL(unregister_pernet_device);
