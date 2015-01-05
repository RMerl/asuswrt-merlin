#ifndef FSPROBE_H
#define FSPROBE_H
/*
 * THIS IS DEPRECATED -- use blkid_evaluate_* API rather than this extra layer
 *
 * This is the generic interface for filesystem guessing libraries.
 * Implementations are provided by
 */
extern void fsprobe_init(void);
extern void fsprobe_exit(void);

extern int fsprobe_parse_spec(const char *spec, char **name, char **value);

/* all routines return newly allocated string */
extern char *fsprobe_get_devname_by_uuid(const char *uuid);
extern char *fsprobe_get_devname_by_label(const char *label);
extern char *fsprobe_get_devname_by_spec(const char *spec);

extern char *fsprobe_get_label_by_devname(const char *devname);
extern char *fsprobe_get_uuid_by_devname(const char *devname);
extern char *fsprobe_get_fstype_by_devname(const char *devname);
extern char *fsprobe_get_fstype_by_devname_ambi(const char *devname, int *ambi);


extern int fsprobe_known_fstype(const char *fstype);

#endif /* FSPROBE_H */
