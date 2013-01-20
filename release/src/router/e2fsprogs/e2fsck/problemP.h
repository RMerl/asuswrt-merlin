/*
 * problemP.h --- Private header file for fix_problem()
 *
 * Copyright 1997 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

struct e2fsck_problem {
	problem_t	e2p_code;
	const char *	e2p_description;
	char		prompt;
	int		flags;
	problem_t	second_code;
};

struct latch_descr {
	int		latch_code;
	problem_t	question;
	problem_t	end_message;
	int		flags;
};

#define PR_PREEN_OK	0x000001 /* Don't need to do preenhalt */
#define PR_NO_OK	0x000002 /* If user answers no, don't make fs invalid */
#define PR_NO_DEFAULT	0x000004 /* Default to no */
#define PR_MSG_ONLY	0x000008 /* Print message only */

/* Bit positions 0x000ff0 are reserved for the PR_LATCH flags */

#define PR_FATAL	0x001000 /* Fatal error */
#define PR_AFTER_CODE	0x002000 /* After asking the first question, */
				 /* ask another */
#define PR_PREEN_NOMSG	0x004000 /* Don't print a message if we're preening */
#define PR_NOCOLLATE	0x008000 /* Don't collate answers for this latch */
#define PR_NO_NOMSG	0x010000 /* Don't print a message if e2fsck -n */
#define PR_PREEN_NO	0x020000 /* Use No as an answer if preening */
#define PR_PREEN_NOHDR	0x040000 /* Don't print the preen header */
#define PR_CONFIG	0x080000 /* This problem has been customized
				    from the config file */
#define PR_FORCE_NO	0x100000 /* Force the answer to be no */
