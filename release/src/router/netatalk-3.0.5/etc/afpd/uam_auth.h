/*
 * Copyright (c) 1999 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 *
 * interface between uam.c and auth.c
 */

#ifndef AFPD_UAM_AUTH_H
#define AFPD_UAM_AUTH_H 1

#include <pwd.h>

#include <atalk/uam.h>
#include <atalk/globals.h>

struct uam_mod {
    void *uam_module;
    struct uam_export *uam_fcn;
    struct uam_mod *uam_prev, *uam_next;
};

struct uam_obj {
    const char *uam_name; /* authentication method */
    char *uam_path; /* where it's located */
    int uam_count;
    union {
        struct {
            int (*login) (void *, struct passwd **,
                              char *, int, char *, size_t *);
            int (*logincont) (void *, struct passwd **, char *,
                                  int, char *, size_t *);
            void (*logout) (void);
            int (*login_ext) (void *, char *, struct passwd **,
                              char *, int, char *, size_t *);
        } uam_login;
        int (*uam_changepw) (void *, char *, struct passwd *, char *,
                                 int, char *, size_t *);
    } u;
    struct uam_obj *uam_prev, *uam_next;
};

#define uam_attach(a, b) do { \
    (a)->uam_prev->uam_next = (b); \
    (b)->uam_prev = (a)->uam_prev; \
    (b)->uam_next = (a); \
    (a)->uam_prev = (b); \
} while (0)				     

#define uam_detach(a) do { \
    (a)->uam_prev->uam_next = (a)->uam_next; \
    (a)->uam_next->uam_prev = (a)->uam_prev; \
} while (0)

extern struct uam_mod *uam_load (const char *, const char *);
extern void uam_unload (struct uam_mod *);

/* auth.c */
int auth_load (const char *, const char *);
int auth_register (const int, struct uam_obj *);
#define auth_unregister(a) uam_detach(a)
struct uam_obj *auth_uamfind (const int, const char *, const int);
void auth_unload (void);

/* uam.c */
int uam_random_string (AFPObj *,char *, int);

#endif /* uam_auth.h */
