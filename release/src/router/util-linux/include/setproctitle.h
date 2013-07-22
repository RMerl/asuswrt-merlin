
void initproctitle (int argc, char **argv);
#if 0
void setproctitle (const char *fmt, ...);
#else
void setproctitle (const char *prog, const char *txt);
#endif
