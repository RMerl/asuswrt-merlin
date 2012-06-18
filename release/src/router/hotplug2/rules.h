/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#ifndef RULES_H
#define RULES_H 1

#define COND_MATCH_CMP			0	/* == */
#define COND_MATCH_RE			1	/* ~~ */
#define COND_NMATCH_CMP			2	/* != */
#define COND_NMATCH_RE			3	/* !~ */
#define COND_MATCH_IS			4	/* is */

#define ACT_RUN_SHELL			0	/* system app <...>, run app <...> */
#define ACT_RUN_NOSHELL			1	/* exec app <...> */
#define ACT_STOP_PROCESSING		2	/* break */
#define ACT_STOP_IF_FAILED		3	/* break_if_failed */
#define ACT_MAKE_DEVICE			4	/* makedev <...> */
#define ACT_CHMOD			5	/* chmod <...> */
#define ACT_CHGRP			6	/* chgrp <...> */
#define ACT_CHOWN			7	/* chown <...> */
#define ACT_SYMLINK			8	/* symlink <...> */
#define ACT_NEXT_EVENT			9	/* next */
#define ACT_NEXT_IF_FAILED		10	/* next_if_failed */
#define ACT_SETENV			11	/* setenv <...> */
#define ACT_REMOVE			12	/* remove <...> */
#define ACT_DEBUG			13	/* debug */
#define ACT_FLAG_NOTHROTTLE		14	/* sets 'nothrottle' flag */

#define EVAL_MATCH			1
#define EVAL_NOT_MATCH			0
#define EVAL_NOT_AVAILABLE		-1

#define QUOTES_NONE			0
#define QUOTES_SINGLE			1
#define QUOTES_DOUBLE			2

#define STATUS_KEY			0	/* just about anything */
#define STATUS_CONDTYPE			1	/* viz COND_* */
#define STATUS_VALUE			2	/* just about anything */
#define STATUS_INITIATOR		3	/* ',' for next cond, '{' for block*/
#define STATUS_ACTION			4	/* viz ACT_* and '}' for end of block */

#define FLAG_UNSET			0
#define FLAG_ALL			0xffffffff
#define FLAG_NOTHROTTLE			1	/* We want this rule to ignore max_children limit */

struct key_rec_t {
	char *key;
	int param;
	int type;
};

struct condition_t {
	int type;
	char *key;
	char *value;
};

struct action_t {
	int type;
	char **parameter;
};

struct rule_t {
	struct condition_t *conditions;
	int conditions_c;
	
	struct action_t *actions;
	int actions_c;

	unsigned int flags;
};

struct rules_t {
	struct rule_t *rules;
	int rules_c;
};

int rule_condition_eval(struct hotplug2_event_t *, struct condition_t *);
int rule_execute(struct hotplug2_event_t *, struct rule_t *);
void rule_flags(struct rule_t *);
void rules_free(struct rules_t *);
struct rules_t *rules_from_config(char *, struct rules_t *);

#endif /* ifndef RULES_H*/
