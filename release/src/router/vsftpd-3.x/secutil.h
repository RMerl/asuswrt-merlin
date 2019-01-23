#ifndef VSF_SECUTIL_H
#define VSF_SECUTIL_H

struct mystr;

/* vsf_secutil_change_credentials()
 * PURPOSE
 * This function securely switches process credentials to the user specified.
 * There are options to enter a chroot() jail, and supplementary groups may
 * or may not be activated.
 * PARAMETERS
 * p_user_str     - the name of the user to become
 * p_dir_str      - the directory to chdir() and possibly chroot() to.
 *                  (if NULL, the user's home directory is used)
 * p_ext_dir_str  - the directory to chdir() and possibly chroot() to,
 *                  applied in addition to the directory calculated by
 *                  p_user_str and p_dir_str.
 * caps           - bitmap of capabilities to adopt. NOTE, if the underlying
 *                  OS does not support capabilities as a non-root user, and
 *                  the capability bitset is non-empty, then root privileges
 *                  will have to be retained.
 * options        - see bitmask definitions below
 */

/* chroot() the user into the new directory */
#define VSF_SECUTIL_OPTION_CHROOT                   1
/* Activate any supplementary groups the user may have */
#define VSF_SECUTIL_OPTION_USE_GROUPS               2
/* Do the chdir() as the effective userid of the target user */
#define VSF_SECUTIL_OPTION_CHANGE_EUID              4
/* Use RLIMIT_NOFILE to prevent the opening of new fds */
#define VSF_SECUTIL_OPTION_NO_FDS                   8
/* Use RLIMIT_NPROC to prevent the launching of new processes */
#define VSF_SECUTIL_OPTION_NO_PROCS                 16
/* Permit a writeable chroot() root */
#define VSF_SECUTIL_OPTION_ALLOW_WRITEABLE_ROOT     32

void vsf_secutil_change_credentials(const struct mystr* p_user_str,
                                    const struct mystr* p_dir_str,
                                    const struct mystr* p_ext_dir_str,
                                    unsigned int caps, unsigned int options);
#endif /* VSF_SECUTIL_H */

