#include <net-snmp/net-snmp-config.h>

#include <stdlib.h>
#include <string.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/snmp_transport.h>

static char**
create_word_array_helper(const char* cptr, size_t idx, char* tmp, size_t tmplen)
{
    char* item;
    char** res;
    cptr = copy_nword_const(cptr, tmp, tmplen);
    item = strdup(tmp);
    if (cptr)
        res = create_word_array_helper(cptr, idx + 1, tmp, tmplen);
    else {
        res = (char**)malloc(sizeof(char*) * (idx + 2));
        res[idx + 1] = NULL;
    }
    res[idx] = item;
    return res;
}

static char**
create_word_array(const char* cptr)
{
    size_t tmplen = strlen(cptr);
    char* tmp = (char*)malloc(tmplen + 1);
    char** res = create_word_array_helper(cptr, 0, tmp, tmplen + 1);
    free(tmp);
    return res;
}

static void
destroy_word_array(char** arr)
{
    if (arr) {
        char** run = arr;
        while(*run) {
            free(*run);
            ++run;
        }
        free(arr);
    }
}

struct netsnmp_lookup_domain {
    char* application;
    char** userDomain;
    char** domain;
    struct netsnmp_lookup_domain* next;
};

static struct netsnmp_lookup_domain* domains = NULL;

int
netsnmp_register_default_domain(const char* application, const char* domain)
{
    struct netsnmp_lookup_domain *run = domains, *prev = NULL;
    int res = 0;

    while (run != NULL && strcmp(run->application, application) < 0) {
	prev = run;
	run = run->next;
    }
    if (run && strcmp(run->application, application) == 0) {
      if (run->domain != NULL) {
          destroy_word_array(run->domain);
	  run->domain = NULL;
	  res = 1;
      }
    } else {
	run = SNMP_MALLOC_STRUCT(netsnmp_lookup_domain);
	run->application = strdup(application);
	run->userDomain = NULL;
	if (prev) {
	    run->next = prev->next;
	    prev->next = run;
	} else {
	    run->next = domains;
	    domains = run;
	}
    }
    if (domain) {
        run->domain = create_word_array(domain);
    } else if (run->userDomain == NULL) {
	if (prev)
	    prev->next = run->next;
	else
	    domains = run->next;
	free(run->application);
	free(run);
    }
    return res;
}

void
netsnmp_clear_default_domain(void)
{
    while (domains) {
	struct netsnmp_lookup_domain *tmp = domains;
	domains = domains->next;
	free(tmp->application);
        destroy_word_array(tmp->userDomain);
        destroy_word_array(tmp->domain);
	free(tmp);
    }
}

static void
netsnmp_register_user_domain(const char* token, char* cptr)
{
    struct netsnmp_lookup_domain *run = domains, *prev = NULL;
    size_t len = strlen(cptr) + 1;
    char* application = (char*)malloc(len);
    char** domain;

    {
        char* cp = copy_nword(cptr, application, len);
        if (cp == NULL) {
            netsnmp_config_error("No domain(s) in registration of "
                                 "defDomain \"%s\"", application);
            free(application);
            return;
        }
        domain = create_word_array(cp);
    }

    while (run != NULL && strcmp(run->application, application) < 0) {
	prev = run;
	run = run->next;
    }
    if (run && strcmp(run->application, application) == 0) {
	if (run->userDomain != NULL) {
	    config_perror("Default transport already registered for this "
			  "application");
            destroy_word_array(domain);
            free(application);
	    return;
	}
    } else {
	run = SNMP_MALLOC_STRUCT(netsnmp_lookup_domain);
	run->application = strdup(application);
	run->domain = NULL;
	if (prev) {
	    run->next = prev->next;
	    prev->next = run;
	} else {
	    run->next = domains;
	    domains = run;
	}
    }
    run->userDomain = domain;
    free(application);
}

static void
netsnmp_clear_user_domain(void)
{
    struct netsnmp_lookup_domain *run = domains, *prev = NULL;

    while (run) {
	if (run->userDomain != NULL) {
            destroy_word_array(run->userDomain);
	    run->userDomain = NULL;
	}
	if (run->domain == NULL) {
	    struct netsnmp_lookup_domain *tmp = run;
	    if (prev)
		run = prev->next = run->next;
	    else
		run = domains = run->next;
	    free(tmp->application);
	    free(tmp);
	} else {
	    prev = run;
	    run = run->next;
	}
    }
}

const char* const *
netsnmp_lookup_default_domains(const char* application)
{
    const char * const * res;

    if (application == NULL)
	res = NULL;
    else {
        struct netsnmp_lookup_domain *run = domains;

	while (run && strcmp(run->application, application) < 0)
	    run = run->next;
	if (run && strcmp(run->application, application) == 0)
	    if (run->userDomain)
                res = (const char * const *)run->userDomain;
	    else
                res = (const char * const *)run->domain;
	else
	    res = NULL;
    }
    DEBUGMSGTL(("defaults",
                "netsnmp_lookup_default_domain(\"%s\") ->",
                application ? application : "[NIL]"));
    if (res) {
        const char * const * r = res;
        while(*r) {
            DEBUGMSG(("defaults", " \"%s\"", *r));
            ++r;
        }
        DEBUGMSG(("defaults", "\n"));
    } else
        DEBUGMSG(("defaults", " \"[NIL]\"\n"));
    return res;
}

const char*
netsnmp_lookup_default_domain(const char* application)
{
    const char * const * res = netsnmp_lookup_default_domains(application);
    return (res ? *res : NULL);
}

struct netsnmp_lookup_target {
    char* application;
    char* domain;
    char* userTarget;
    char* target;
    struct netsnmp_lookup_target* next;
};

static struct netsnmp_lookup_target* targets = NULL;

/**
 * Add an (application, domain, target) triplet to the targets list if target
 * != NULL. Remove an entry if target == NULL and the userTarget pointer for
 * the entry found is also NULL. Keep at most one target per (application,
 * domain) pair.
 *
 * @return 1 if an entry for (application, domain) was already present in the
 *   targets list or 0 if such an entry was not yet present in the targets list.
 */
int
netsnmp_register_default_target(const char* application, const char* domain,
				const char* target)
{
    struct netsnmp_lookup_target *run = targets, *prev = NULL;
    int i = 0, res = 0;
    while (run && ((i = strcmp(run->application, application)) < 0 ||
		   (i == 0 && strcmp(run->domain, domain) < 0))) {
	prev = run;
	run = run->next;
    }
    if (run && i == 0 && strcmp(run->domain, domain) == 0) {
      if (run->target != NULL) {
	    free(run->target);
	    run->target = NULL;
	    res = 1;
      }
    } else {
	run = SNMP_MALLOC_STRUCT(netsnmp_lookup_target);
	run->application = strdup(application);
	run->domain = strdup(domain);
	run->userTarget = NULL;
	if (prev) {
	    run->next = prev->next;
	    prev->next = run;
	} else {
	    run->next = targets;
	    targets = run;
	}
    }
    if (target) {
	run->target = strdup(target);
    } else if (run->userTarget == NULL) {
	if (prev)
	    prev->next = run->next;
	else
	    targets = run->next;
	free(run->domain);
	free(run->application);
	free(run);
    }
    return res;
}

/**
 * Clear the targets list.
 */
void
netsnmp_clear_default_target(void)
{
    while (targets) {
	struct netsnmp_lookup_target *tmp = targets;
	targets = targets->next;
	free(tmp->application);
	free(tmp->domain);
	free(tmp->userTarget);
	free(tmp->target);
	free(tmp);
    }
}

static void
netsnmp_register_user_target(const char* token, char* cptr)
{
    struct netsnmp_lookup_target *run = targets, *prev = NULL;
    size_t len = strlen(cptr) + 1;
    char* application = (char*)malloc(len);
    char* domain = (char*)malloc(len);
    char* target = (char*)malloc(len);
    int i = 0;

    {
	char* cp = copy_nword(cptr, application, len);
        if (cp == NULL) {
            netsnmp_config_error("No domain and target in registration of "
                                 "defTarget \"%s\"", application);
            goto done;
        }
	cp = copy_nword(cp, domain, len);
        if (cp == NULL) {
            netsnmp_config_error("No target in registration of "
                                 "defTarget \"%s\" \"%s\"",
                                 application, domain);
            goto done;
        }
	cp = copy_nword(cp, target, len);
	if (cp)
	    config_pwarn("Trailing junk found");
    }

    while (run && ((i = strcmp(run->application, application)) < 0 ||
		   (i == 0 && strcmp(run->domain, domain) < 0))) {
	prev = run;
	run = run->next;
    }
    if (run && i == 0 && strcmp(run->domain, domain) == 0) {
	if (run->userTarget != NULL) {
	    config_perror("Default target already registered for this "
			  "application-domain combination");
	    goto done;
	}
    } else {
	run = SNMP_MALLOC_STRUCT(netsnmp_lookup_target);
	run->application = strdup(application);
	run->domain = strdup(domain);
	run->target = NULL;
	if (prev) {
	    run->next = prev->next;
	    prev->next = run;
	} else {
	    run->next = targets;
	    targets = run;
	}
    }
    run->userTarget = strdup(target);
 done:
    free(target);
    free(domain);
    free(application);
}

static void
netsnmp_clear_user_target(void)
{
    struct netsnmp_lookup_target *run = targets, *prev = NULL;

    while (run) {
	if (run->userTarget != NULL) {
	    free(run->userTarget);
	    run->userTarget = NULL;
	}
	if (run->target == NULL) {
	    struct netsnmp_lookup_target *tmp = run;
	    if (prev)
		run = prev->next = run->next;
	    else
		run = targets = run->next;
	    free(tmp->application);
	    free(tmp->domain);
	    free(tmp);
	} else {
	    prev = run;
	    run = run->next;
	}
    }
}

const char*
netsnmp_lookup_default_target(const char* application, const char* domain)
{
    int i = 0;
    struct netsnmp_lookup_target *run = targets;
    const char *res;

    if (application == NULL || domain == NULL)
	res = NULL;
    else {
	while (run && ((i = strcmp(run->application, application)) < 0 ||
		       (i == 0 && strcmp(run->domain, domain) < 0)))
	    run = run->next;
	if (run && i == 0 && strcmp(run->domain, domain) == 0)
	    if (run->userTarget != NULL)
		res = run->userTarget;
	    else
		res = run->target;
	else
	    res = NULL;
    }
    DEBUGMSGTL(("defaults",
		"netsnmp_lookup_default_target(\"%s\", \"%s\") -> \"%s\"\n",
		application ? application : "[NIL]",
		domain ? domain : "[NIL]",
		res ? res : "[NIL]"));
    return res;
}

void
netsnmp_register_service_handlers(void)
{
    register_config_handler("snmp:", "defDomain",
			    netsnmp_register_user_domain,
			    netsnmp_clear_user_domain,
			    "application domain");
    register_config_handler("snmp:", "defTarget",
			    netsnmp_register_user_target,
			    netsnmp_clear_user_target,
			    "application domain target");
}
