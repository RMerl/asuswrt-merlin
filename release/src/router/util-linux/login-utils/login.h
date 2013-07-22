/* defined in login.c */
extern void badlogin(const char *s);
extern void sleepexit(int);
extern char hostaddress[16];
extern char *hostname;
extern sa_family_t hostfamily;

/* defined in checktty.c */
extern void checktty(const char *user, const char *tty, struct passwd *pwd);
