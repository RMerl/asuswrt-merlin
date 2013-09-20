#ifndef __SMTPCOMMANDS_H
#define __SMTPCOMMANDS_H  1

#include "dnet.h"

char *smtpGetErr(void);
int smtpInitAuth(dsocket *sd, const char *auth, const char *user, const char *pass);
int smtpInit(dsocket *sd, const char *domain);
int smtpStartTls(dsocket *sd);
int smtpSetMailFrom(dsocket *sd, const char *from);
int smtpSetRcpt(dsocket *sd, const char *to);
int smtpStartData(dsocket *sd);
int smtpSendData(dsocket *sd, const char *data, size_t len);
int smtpEndData(dsocket *sd);
int smtpQuit(dsocket *sd);

#endif /* __SMTPCOMMANDS_H */
