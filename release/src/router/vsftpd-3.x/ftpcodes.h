#ifndef VSF_FTPCODES_H
#define VSF_FTPCODES_H

#define FTP_DATACONN          150

#define FTP_NOOPOK            200
#define FTP_TYPEOK            200
#define FTP_PORTOK            200
#define FTP_EPRTOK            200
#define FTP_UMASKOK           200
#define FTP_CHMODOK           200
#define FTP_EPSVALLOK         200
#define FTP_STRUOK            200
#define FTP_MODEOK            200
#define FTP_PBSZOK            200
#define FTP_PROTOK            200
#define FTP_OPTSOK            200
#define FTP_ALLOOK            202
#define FTP_FEAT              211
#define FTP_STATOK            211
#define FTP_SIZEOK            213
#define FTP_MDTMOK            213
#define FTP_STATFILE_OK       213
#define FTP_SITEHELP          214
#define FTP_HELP              214
#define FTP_SYSTOK            215
#define FTP_GREET             220
#define FTP_GOODBYE           221
#define FTP_ABOR_NOCONN       225
#define FTP_TRANSFEROK        226
#define FTP_ABOROK            226
#define FTP_PASVOK            227
#define FTP_EPSVOK            229
#define FTP_LOGINOK           230
#define FTP_AUTHOK            234
#define FTP_CWDOK             250
#define FTP_RMDIROK           250
#define FTP_DELEOK            250
#define FTP_RENAMEOK          250
#define FTP_PWDOK             257
#define FTP_MKDIROK           257

#define FTP_GIVEPWORD         331
#define FTP_RESTOK            350
#define FTP_RNFROK            350

#define FTP_IDLE_TIMEOUT      421
#define FTP_DATA_TIMEOUT      421
#define FTP_TOO_MANY_USERS    421
#define FTP_IP_LIMIT          421
#define FTP_IP_DENY           421
#define FTP_TLS_FAIL          421
#define FTP_BADSENDCONN       425
#define FTP_BADSENDNET        426
#define FTP_BADSENDFILE       451

#define FTP_BADCMD            500
#define FTP_BADOPTS           501
#define FTP_COMMANDNOTIMPL    502
#define FTP_NEEDUSER          503
#define FTP_NEEDRNFR          503
#define FTP_BADPBSZ           503
#define FTP_BADPROT           503
#define FTP_BADSTRU           504
#define FTP_BADMODE           504
#define FTP_BADAUTH           504
#define FTP_NOSUCHPROT        504
#define FTP_NEEDENCRYPT       522
#define FTP_EPSVBAD           522
#define FTP_DATATLSBAD        522
#define FTP_LOGINERR          530
#define FTP_NOHANDLEPROT      536
#define FTP_FILEFAIL          550
#define FTP_NOPERM            550
#define FTP_UPLOADFAIL        553

#endif /* VSF_FTPCODES_H */
