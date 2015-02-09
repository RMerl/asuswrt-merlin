/*
 * AppArmor security module
 *
 * This file contains AppArmor policy definitions.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_POLICY_H
#define __AA_POLICY_H

#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/socket.h>

#include "apparmor.h"
#include "audit.h"
#include "capability.h"
#include "domain.h"
#include "file.h"
#include "resource.h"

extern const char *profile_mode_names[];
#define APPARMOR_NAMES_MAX_INDEX 3

#define COMPLAIN_MODE(_profile)	\
	((aa_g_profile_mode == APPARMOR_COMPLAIN) || \
	 ((_profile)->mode == APPARMOR_COMPLAIN))

#define KILL_MODE(_profile) \
	((aa_g_profile_mode == APPARMOR_KILL) || \
	 ((_profile)->mode == APPARMOR_KILL))

#define PROFILE_IS_HAT(_profile) ((_profile)->flags & PFLAG_HAT)

enum profile_mode {
	APPARMOR_ENFORCE,	/* enforce access rules */
	APPARMOR_COMPLAIN,	/* allow and log access violations */
	APPARMOR_KILL,		/* kill task on access violation */
};

enum profile_flags {
	PFLAG_HAT = 1,			/* profile is a hat */
	PFLAG_UNCONFINED = 2,		/* profile is an unconfined profile */
	PFLAG_NULL = 4,			/* profile is null learning profile */
	PFLAG_IX_ON_NAME_ERROR = 8,	/* fallback to ix on name lookup fail */
	PFLAG_IMMUTABLE = 0x10,		/* don't allow changes/replacement */
	PFLAG_USER_DEFINED = 0x20,	/* user based profile - lower privs */
	PFLAG_NO_LIST_REF = 0x40,	/* list doesn't keep profile ref */
	PFLAG_OLD_NULL_TRANS = 0x100,	/* use // as the null transition */

	/* These flags must correspond with PATH_flags */
	PFLAG_MEDIATE_DELETED = 0x10000, /* mediate instead delegate deleted */
};

struct aa_profile;

/* struct aa_policy - common part of both namespaces and profiles
 * @name: name of the object
 * @hname - The hierarchical name
 * @count: reference count of the obj
 * @list: list policy object is on
 * @profiles: head of the profiles list contained in the object
 */
struct aa_policy {
	char *name;
	char *hname;
	struct kref count;
	struct list_head list;
	struct list_head profiles;
};

/* struct aa_ns_acct - accounting of profiles in namespace
 * @max_size: maximum space allowed for all profiles in namespace
 * @max_count: maximum number of profiles that can be in this namespace
 * @size: current size of profiles
 * @count: current count of profiles (includes null profiles)
 */
struct aa_ns_acct {
	int max_size;
	int max_count;
	int size;
	int count;
};

struct aa_namespace {
	struct aa_policy base;
	struct aa_namespace *parent;
	rwlock_t lock;
	struct aa_ns_acct acct;
	struct aa_profile *unconfined;
	struct list_head sub_ns;
};

/* struct aa_profile - basic confinement data
 * @base - base components of the profile (name, refcount, lists, lock ...)
 * @parent: parent of profile
 * @ns: namespace the profile is in
 * @replacedby: is set to the profile that replaced this profile
 * @rename: optional profile name that this profile renamed
 * @xmatch: optional extended matching for unconfined executables names
 * @xmatch_len: xmatch prefix len, used to determine xmatch priority
 * @sid: the unique security id number of this profile
 * @audit: the auditing mode of the profile
 * @mode: the enforcement mode of the profile
 * @flags: flags controlling profile behavior
 * @path_flags: flags controlling path generation behavior
 * @size: the memory consumed by this profiles rules
 * @file: The set of rules governing basic file access and domain transitions
 * @caps: capabilities for the profile
 * @rlimits: rlimits for the profile
 *
 * The AppArmor profile contains the basic confinement data.  Each profile
 * has a name, and exists in a namespace.  The @name and @exec_match are
 * used to determine profile attachment against unconfined tasks.  All other
 * attachments are determined by profile X transition rules.
 *
 * The @replacedby field is write protected by the profile lock.  Reads
 * are assumed to be atomic, and are done without locking.
 *
 * Profiles have a hierarchy where hats and children profiles keep
 * a reference to their parent.
 *
 * Profile names can not begin with a : and can not contain the \0
 * character.  If a profile name begins with / it will be considered when
 * determining profile attachment on "unconfined" tasks.
 */
struct aa_profile {
	struct aa_policy base;
	struct aa_profile *parent;

	struct aa_namespace *ns;
	struct aa_profile *replacedby;
	const char *rename;

	struct aa_dfa *xmatch;
	int xmatch_len;
	u32 sid;
	enum audit_mode audit;
	enum profile_mode mode;
	u32 flags;
	u32 path_flags;
	int size;

	struct aa_file_rules file;
	struct aa_caps caps;
	struct aa_rlimit rlimits;
};

extern struct aa_namespace *root_ns;
extern enum profile_mode aa_g_profile_mode;

void aa_add_profile(struct aa_policy *common, struct aa_profile *profile);

bool aa_ns_visible(struct aa_namespace *curr, struct aa_namespace *view);
const char *aa_ns_name(struct aa_namespace *parent, struct aa_namespace *child);
int aa_alloc_root_ns(void);
void aa_free_root_ns(void);
void aa_free_namespace_kref(struct kref *kref);

struct aa_namespace *aa_find_namespace(struct aa_namespace *root,
				       const char *name);

static inline struct aa_policy *aa_get_common(struct aa_policy *c)
{
	if (c)
		kref_get(&c->count);

	return c;
}

/**
 * aa_get_namespace - increment references count on @ns
 * @ns: namespace to increment reference count of (MAYBE NULL)
 *
 * Returns: pointer to @ns, if @ns is NULL returns NULL
 * Requires: @ns must be held with valid refcount when called
 */
static inline struct aa_namespace *aa_get_namespace(struct aa_namespace *ns)
{
	if (ns)
		kref_get(&(ns->base.count));

	return ns;
}

/**
 * aa_put_namespace - decrement refcount on @ns
 * @ns: namespace to put reference of
 *
 * Decrement reference count of @ns and if no longer in use free it
 */
static inline void aa_put_namespace(struct aa_namespace *ns)
{
	if (ns)
		kref_put(&ns->base.count, aa_free_namespace_kref);
}

struct aa_profile *aa_alloc_profile(const char *name);
struct aa_profile *aa_new_null_profile(struct aa_profile *parent, int hat);
void aa_free_profile_kref(struct kref *kref);
struct aa_profile *aa_find_child(struct aa_profile *parent, const char *name);
struct aa_profile *aa_lookup_profile(struct aa_namespace *ns, const char *name);
struct aa_profile *aa_match_profile(struct aa_namespace *ns, const char *name);

ssize_t aa_replace_profiles(void *udata, size_t size, bool noreplace);
ssize_t aa_remove_profiles(char *name, size_t size);

#define PROF_ADD 1
#define PROF_REPLACE 0

#define unconfined(X) ((X)->flags & PFLAG_UNCONFINED)

/**
 * aa_newest_version - find the newest version of @profile
 * @profile: the profile to check for newer versions of (NOT NULL)
 *
 * Returns: newest version of @profile, if @profile is the newest version
 *          return @profile.
 *
 * NOTE: the profile returned is not refcounted, The refcount on @profile
 * must be held until the caller decides what to do with the returned newest
 * version.
 */
static inline struct aa_profile *aa_newest_version(struct aa_profile *profile)
{
	while (profile->replacedby)
		profile = profile->replacedby;

	return profile;
}

/**
 * aa_get_profile - increment refcount on profile @p
 * @p: profile  (MAYBE NULL)
 *
 * Returns: pointer to @p if @p is NULL will return NULL
 * Requires: @p must be held with valid refcount when called
 */
static inline struct aa_profile *aa_get_profile(struct aa_profile *p)
{
	if (p)
		kref_get(&(p->base.count));

	return p;
}

/**
 * aa_put_profile - decrement refcount on profile @p
 * @p: profile  (MAYBE NULL)
 */
static inline void aa_put_profile(struct aa_profile *p)
{
	if (p)
		kref_put(&p->base.count, aa_free_profile_kref);
}

static inline int AUDIT_MODE(struct aa_profile *profile)
{
	if (aa_g_audit != AUDIT_NORMAL)
		return aa_g_audit;

	return profile->audit;
}

bool aa_may_manage_policy(int op);

#endif /* __AA_POLICY_H */
