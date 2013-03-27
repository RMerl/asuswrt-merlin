#define SSF_DCOND 0
#define SSF_SCOND 1
#define SSF_OR	  2
#define SSF_AND	  3
#define SSF_NOT	  4
#define SSF_D_GE  5
#define SSF_D_LE  6
#define SSF_S_GE  7
#define SSF_S_LE  8
#define SSF_S_AUTO  9

struct ssfilter
{
	int type;
	struct ssfilter *post;
	struct ssfilter *pred;
};

int ssfilter_parse(struct ssfilter **f, int argc, char **argv, FILE *fp);
void *parse_hostcond(char*);

