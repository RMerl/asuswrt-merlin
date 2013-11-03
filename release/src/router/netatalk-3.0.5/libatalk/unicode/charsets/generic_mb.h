size_t   mb_generic_pull(int (*charfunc)(ucs2_t *, const unsigned char *), void *,char **, size_t *, char **, size_t *);
size_t   mb_generic_push(int (*charfunc)(unsigned char *, ucs2_t), void *,char **, size_t *, char **, size_t *);
