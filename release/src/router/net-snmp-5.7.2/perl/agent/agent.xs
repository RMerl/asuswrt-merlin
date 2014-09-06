/* -*- C -*- */
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x501
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <netdb.h>
#include <sys/socket.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#ifndef sv_undef
#define sv_undef PL_sv_undef
#endif

typedef netsnmp_handler_registration *NetSNMP__agent__netsnmp_handler_registration;

/*
 * needs to be in sync with the definitions in snmplib/snmpUDPDomain.c
 * and snmplib/snmpTCPDomain.c
 */
typedef netsnmp_indexed_addr_pair netsnmp_udp_addr_pair;

typedef struct handler_cb_data_s {
   SV *perl_cb;
} handler_cb_data;

typedef struct netsnmp_oid_s {
    oid                 *name;
    size_t               len;
    oid                  namebuf[ MAX_OID_LEN ];
} netsnmp_oid;

#define TEST_CONSTANT(value, name, C)           \
    if (strEQ(name, #C)) {                      \
        *value = C;                             \
        return 0;                               \
    }

static int constant_MODE_G(double *value, const char *name, const int len)
{
    switch (len >= 8 ? name[8] : -1) {
    case '\0':
        TEST_CONSTANT(value, name, MODE_GET);
        break;
    case 'B':
        TEST_CONSTANT(value, name, MODE_GETBULK);
        break;
    case 'N':
        TEST_CONSTANT(value, name, MODE_GETNEXT);
        break;
    }
    return EINVAL;
}

static int constant_MODE_SET_R(double *value, const char *name, const int len)
{
    switch (len >= 16 ? name[16] : -1) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case '1':
	TEST_CONSTANT(value, name, MODE_SET_RESERVE1);
        break;
    case '2':
	TEST_CONSTANT(value, name, MODE_SET_RESERVE2);
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }
    return EINVAL;
}

static int constant_SNMP_ERR(double *value, char *name, int len)
{
    switch (len >= 9 ? name[9] : -1) {
    case 'A':
        TEST_CONSTANT(value, name, SNMP_ERR_AUTHORIZATIONERROR);
        break;
    case 'B':
        TEST_CONSTANT(value, name, SNMP_ERR_BADVALUE);
        break;
    case 'C':
        TEST_CONSTANT(value, name, SNMP_ERR_COMMITFAILED);
        break;
    case 'G':
	TEST_CONSTANT(value, name, SNMP_ERR_GENERR);
        break;
    case 'I':
	TEST_CONSTANT(value, name, SNMP_ERR_INCONSISTENTVALUE);
        break;
    case 'N':
        TEST_CONSTANT(value, name, SNMP_ERR_NOACCESS);
        TEST_CONSTANT(value, name, SNMP_ERR_NOCREATION);
        TEST_CONSTANT(value, name, SNMP_ERR_NOERROR);
        TEST_CONSTANT(value, name, SNMP_ERR_NOSUCHNAME);
        TEST_CONSTANT(value, name, SNMP_ERR_NOTWRITABLE);
        break;
    case 'R':
        TEST_CONSTANT(value, name, SNMP_ERR_READONLY);
        TEST_CONSTANT(value, name, SNMP_ERR_RESOURCEUNAVAILABLE);
        break;
    case 'T':
        TEST_CONSTANT(value, name, SNMP_ERR_TOOBIG);
        break;
    case 'U':
        TEST_CONSTANT(value, name, SNMP_ERR_UNDOFAILED);
        break;
    case 'W':
        TEST_CONSTANT(value, name, SNMP_ERR_WRONGENCODING);
        TEST_CONSTANT(value, name, SNMP_ERR_WRONGLENGTH);
        TEST_CONSTANT(value, name, SNMP_ERR_WRONGTYPE);
        TEST_CONSTANT(value, name, SNMP_ERR_WRONGVALUE);
        break;
    }
    return EINVAL;
}
    
static int constant_MODE_S(double *value, char *name, int len)
{
    if (!strnEQ(name + 6, "ET_", 3))
        return EINVAL;

    switch (len >= 9 ? name[9] : -1) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case 'A':
        TEST_CONSTANT(value, name, MODE_SET_ACTION);
        break;
    case 'B':
        TEST_CONSTANT(value, name, MODE_SET_BEGIN);
        break;
    case 'C':
        TEST_CONSTANT(value, name, MODE_SET_COMMIT);
        break;
    case 'F':
        TEST_CONSTANT(value, name, MODE_SET_FREE);
        break;
    case 'U':
        TEST_CONSTANT(value, name, MODE_SET_UNDO);
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    case 'R':
        return constant_MODE_SET_R(value, name, len);
    }
    return EINVAL;
}

static int constant(double *value, char *name, int len)
{
    switch (len >= 5 ? name[5] : -1) {
    case 'G':
	if (strnEQ(name + 0,"MODE_", 5))
            return constant_MODE_G(value, name, len);
        break;
    case 'S':
	if (strnEQ(name + 0,"MODE_", 5))
            return constant_MODE_S(value, name, len);
        break;
    case 'E':
	if (strnEQ(name + 0,"SNMP_ERR_", 9))
            return constant_SNMP_ERR(value, name, len);
        break;
    }
    return EINVAL;
}

int
handler_wrapper(netsnmp_mib_handler          *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info   *reqinfo,
                netsnmp_request_info         *requests) 
{
    handler_cb_data *cb_data = (handler_cb_data *) handler->myvoid;
    SV *cb;

    if (cb_data && (cb = cb_data->perl_cb)) {
        SV *arg;
        SV *rarg;
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(sp);
        rarg = newSViv(0);
        arg = newSVrv(rarg, "NetSNMP::agent::netsnmp_mib_handler");
        sv_setiv(arg, (IV) handler);
        XPUSHs(sv_2mortal(rarg));
        rarg = newSViv(0);
        arg = newSVrv(rarg, "NetSNMP::agent::netsnmp_handler_registrationPtr");
        sv_setiv(arg, (IV) reginfo);
        XPUSHs(sv_2mortal(rarg));
        rarg = newSViv(0);
        arg = newSVrv(rarg, "NetSNMP::agent::netsnmp_agent_request_info");
        sv_setiv(arg, (IV) reqinfo);
        XPUSHs(sv_2mortal(rarg));
        rarg = newSViv(0);
        arg = newSVrv(rarg, "NetSNMP::agent::netsnmp_request_infoPtr");
        sv_setiv(arg, (IV) requests);
        XPUSHs(sv_2mortal(rarg));
        PUTBACK;
        if (SvTYPE(cb) == SVt_PVCV) {
            perl_call_sv(cb, G_DISCARD);
                                       
        } else if (SvROK(cb) && SvTYPE(SvRV(cb)) == SVt_PVCV) {
            perl_call_sv(SvRV(cb), G_DISCARD); 
        }
        SPAGAIN;
        PUTBACK;
        FREETMPS;
        LEAVE;
    }
    return SNMP_ERR_NOERROR;
}

MODULE = NetSNMP::agent		PACKAGE = NetSNMP::agent		

void
constant(sv)
    PREINIT:
	STRLEN		len;
    INPUT:
	SV *		sv
	char *		s = SvPV(sv, len);
    INIT:
        int status;
        double value;
    PPCODE:
        value = 0;
        status = constant(&value, s, len);
        XPUSHs(sv_2mortal(newSVuv(status)));
        XPUSHs(sv_2mortal(newSVnv(value)));

int
__agent_check_and_process(block = 1)
	int block;
    CODE:
	RETVAL = agent_check_and_process(block);
    OUTPUT:
	RETVAL

int
_uptime()
    CODE:
        RETVAL = netsnmp_get_agent_uptime();
    OUTPUT:
	RETVAL

void
init_mib()
    CODE:
    {
        netsnmp_init_mib();
    }

int
init_agent(name)
        const char *name;
    CODE:
        SOCK_STARTUP;
        RETVAL = init_agent(name);
    OUTPUT:
        RETVAL

void
init_snmp(name)
        const char *name;

int
init_master_agent()

void    
snmp_enable_stderrlog()    

MODULE = NetSNMP::agent  PACKAGE = NetSNMP::agent PREFIX = na_

void
na_shutdown(me)
    SV *me;
    CODE:
    {
        if (0)
            printf("me = %p\n", me);
        snmp_shutdown("perl");
    }

void
na_errlog(me,value)
    SV *me;
    SV *value;
   PREINIT:
        STRLEN stringlen;
        char * stringptr;
    CODE:
    {
        if (0)
            printf("me = %p\n", me);
        stringptr = SvPV(value, stringlen);
        snmp_log(LOG_ERR, "%s", stringptr );
    }



MODULE = NetSNMP::agent  PACKAGE = NetSNMP::agent::netsnmp_handler_registration  PREFIX = nsahr_

NetSNMP::agent::netsnmp_handler_registration
nsahr_new(name, regoid, perlcallback)
        char *name;
	char *regoid;
        SV   *perlcallback;
    PREINIT:
	oid myoid[MAX_OID_LEN];
	size_t myoid_len = MAX_OID_LEN;
        handler_cb_data *cb_data;
        int gotit=1;
    CODE:
	if (!snmp_parse_oid(regoid, myoid, &myoid_len)) {
            if (!read_objid(regoid, myoid, &myoid_len)) {
                snmp_log(LOG_ERR, "couldn't parse %s (reg name: %s)\n",
                        regoid, name);
                RETVAL = NULL;
                gotit = 0;
            }
        }
        if (gotit) {
            cb_data = (handler_cb_data *) malloc(sizeof(handler_cb_data));
            RETVAL = netsnmp_create_handler_registration(name, handler_wrapper,
                                                 myoid, myoid_len,
                                                 HANDLER_CAN_RWRITE);
            cb_data->perl_cb = newSVsv(perlcallback);
            RETVAL->handler->myvoid = cb_data;
        }
    OUTPUT:
        RETVAL

void
nsahr_DESTROY(reginfo)
	netsnmp_handler_registration *reginfo
    PREINIT:
        handler_cb_data *cb_data;
    CODE:
        if (reginfo && reginfo->handler && reginfo->handler->myvoid) {
	    cb_data = (handler_cb_data *) (reginfo->handler->myvoid);
	    SvREFCNT_dec(cb_data->perl_cb);
	    free(cb_data);
        }
	netsnmp_handler_registration_free(reginfo);

int
nsahr_register(me)
        SV *me;
        PREINIT:
        netsnmp_handler_registration *reginfo;
        handler_cb_data *cb_data = NULL;
        CODE:
            {
                reginfo = (netsnmp_handler_registration *) SvIV(SvRV(me));
                if (reginfo && reginfo->handler && reginfo->handler->myvoid)
                    cb_data = (handler_cb_data *) (reginfo->handler->myvoid);
                RETVAL = netsnmp_register_handler(reginfo);
                if (!RETVAL) {
                    /* the agent now has a "reference" to this reg pointer */
                    SvREFCNT_inc(me);
                } else {
                    /*
                     * The reginfo was freed by netsnmp_register_handler,
                     * don't touch it in nsahr_DESTROY!
                     */
                    sv_setiv(SvRV(me), 0);
                    if (cb_data) {
                        /* And just free the callback. */
                        SvREFCNT_dec(cb_data->perl_cb);
                        free(cb_data);
                    }
                }
            }
    OUTPUT:
	RETVAL


MODULE = NetSNMP::agent PACKAGE = NetSNMP::agent::netsnmp_handler_registrationPtr PREFIX = nsahr_

void
nsahr_getRootOID(me)
    SV *me;
    PREINIT:
        int i;
        netsnmp_oid *o;
        netsnmp_handler_registration *reginfo;
        SV *arg, *rarg;
    PPCODE:
    {
        dSP;
        PUSHMARK(SP);
        reginfo = (netsnmp_handler_registration *) SvIV(SvRV(me));

        o = malloc(sizeof(netsnmp_oid));
        o->name = o->namebuf;
        o->len = reginfo->rootoid_len;
        memcpy(o->name, reginfo->rootoid,
               reginfo->rootoid_len * sizeof(oid));

        rarg = newSViv((int) 0);
        arg = newSVrv(rarg, "netsnmp_oidPtr");
        sv_setiv(arg, (IV) o);

        XPUSHs(sv_2mortal(rarg));

        PUTBACK;
        i = perl_call_pv("NetSNMP::OID::newwithptr", G_SCALAR);
        SPAGAIN;
        if (i != 1) {
            snmp_log(LOG_ERR, "unhandled OID error.\n");
            /* ack XXX */
        }
        ST(0) = POPs;
        XSRETURN(1);
    }

MODULE = NetSNMP::agent  PACKAGE = NetSNMP::agent::netsnmp_request_infoPtr PREFIX = nari_

void
getOID(me)
    SV *me;
    PREINIT:
        int i;
        netsnmp_oid *o;
        netsnmp_request_info *request;
        SV *arg, *rarg;
    PPCODE:
    {
        dSP;
        PUSHMARK(SP);
        request = (netsnmp_request_info *) SvIV(SvRV(me));

        o = malloc(sizeof(netsnmp_oid));
        o->name = o->namebuf;
        o->len = request->requestvb->name_length;
        memcpy(o->name, request->requestvb->name,
               request->requestvb->name_length * sizeof(oid));

        rarg = newSViv((int) 0);
        arg = newSVrv(rarg, "netsnmp_oidPtr");
        sv_setiv(arg, (IV) o);

        XPUSHs(sv_2mortal(rarg));

        PUTBACK;
        i = perl_call_pv("NetSNMP::OID::newwithptr", G_SCALAR);
        SPAGAIN;
        if (i != 1) {
            snmp_log(LOG_ERR, "unhandled OID error.\n");
            /* ack XXX */
        }
        ST(0) = POPs;
        XSRETURN(1);
    }
        
netsnmp_oid *
nari_getOIDptr(me)
        SV *me;
        PREINIT:
        netsnmp_request_info *request;
        CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        RETVAL = malloc(sizeof(netsnmp_oid));
        RETVAL->name = RETVAL->namebuf;
        RETVAL->len = request->requestvb->name_length;
        memcpy(RETVAL->name, request->requestvb->name,
               request->requestvb->name_length * sizeof(oid));
    OUTPUT:
        RETVAL

int
nari_getType(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));

        RETVAL =  request->requestvb->type ;
    OUTPUT:
        RETVAL 

void
nari_setType(me, newvalue)
        SV *me;
        int newvalue;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        request->requestvb->type=newvalue;

SV *
nari_getValue(me)
        SV *me;
    PREINIT:
        char *outbuf = NULL;
        size_t ob_len = 0, oo_len = 0;
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
	sprint_realloc_by_type((u_char **) &outbuf, &ob_len, &oo_len, 1,
                               request->requestvb, 0, 0, 0);
        RETVAL = newSVpv(outbuf, 0);
	netsnmp_free(outbuf);
    OUTPUT:
        RETVAL

int
nari_getDelegated(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        RETVAL = request->delegated;
    OUTPUT:
        RETVAL

void
nari_setDelegated(me, newdelegated)
        SV *me;
        int newdelegated;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        request->delegated = newdelegated;

int
nari_getProcessed(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        RETVAL = request->processed;
    OUTPUT:
        RETVAL

void
nari_setProcessed(me, newprocessed)
        SV *me;
        int newprocessed;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        request->processed = newprocessed;

int
nari_getStatus(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        RETVAL = request->status;
    OUTPUT:
        RETVAL

void
nari_setStatus(me, newstatus)
        SV *me;
        int newstatus;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        request->status = newstatus;

int
nari_getRepeat(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        RETVAL = request->repeat;
    OUTPUT:
        RETVAL

void
nari_setRepeat(me, newrepeat)
        SV *me;
        int newrepeat;
    PREINIT:
        netsnmp_request_info *request;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        request->repeat = newrepeat;

int
nari_setValue(me, type, value)
        SV *me;
        int type;
        SV *value;
    PREINIT:
        netsnmp_request_info *request;
        u_long utmp;
        long ltmp;
        uint64_t ulltmp;
        struct counter64 c64;
	oid myoid[MAX_OID_LEN];
	size_t myoid_len;
        STRLEN stringlen;
        char * stringptr;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        switch(type) {
          case  SNMP_NOSUCHINSTANCE :
              snmp_set_var_typed_value(request->requestvb,SNMP_NOSUCHINSTANCE,0,0) ;
              RETVAL = 1;
              break ;
          case  SNMP_NOSUCHOBJECT :
              snmp_set_var_typed_value(request->requestvb,SNMP_NOSUCHOBJECT,0,0) ;
              RETVAL = 1;
              break ;
          case  SNMP_ENDOFMIBVIEW :
              snmp_set_var_typed_value(request->requestvb,SNMP_ENDOFMIBVIEW,0,0) ;
              RETVAL = 1;
              break ;
          case ASN_INTEGER:
	      /* We want an integer here */
	      if ((SvTYPE(value) == SVt_IV) || (SvTYPE(value) == SVt_PVMG) ||
                   SvIOK(value)) {
		  /* Good - got a real one (or a blessed object that we hope will turn out OK) */
		  ltmp = SvIV(value);
		  snmp_set_var_typed_value(request->requestvb, (u_char)type,
					   (u_char *) &ltmp, sizeof(ltmp));
		  RETVAL = 1;
		  break;
	      }
	      else if (SvPOKp(value)) {
	          /* Might be OK - got a string, so try to convert it, allowing base 10, octal, and hex forms */
	          stringptr = SvPV(value, stringlen);
		  ltmp = strtol( stringptr, NULL, 0 );
		  if (errno == EINVAL) {
		  	snmp_log(LOG_ERR, "Could not convert string to number in setValue: '%s'", stringptr);
			RETVAL = 0;
			break;
		  }

		  snmp_set_var_typed_value(request->requestvb, (u_char)type,
					   (u_char *) &ltmp, sizeof(ltmp));
		  RETVAL = 1;
		  break;
	      }
	      else {
		snmp_log(LOG_ERR, "Non-integer value passed to setValue with ASN_INTEGER: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
		break;
	      }


          case ASN_UNSIGNED:
          case ASN_COUNTER:
          case ASN_TIMETICKS:
	      /* We want an integer here */
	      if ((SvTYPE(value) == SVt_IV) || (SvTYPE(value) == SVt_PVMG) ||
                   SvIOK(value)) {
		  /* Good - got a real one (or a blessed scalar which we have to hope will turn out OK) */
		  utmp = SvIV(value);
                  snmp_set_var_typed_value(request->requestvb, (u_char)type,
                                       (u_char *) &utmp, sizeof(utmp));
		  RETVAL = 1;
		  break;
	      }
	      else if (SvPOKp(value)) {
	          /* Might be OK - got a string, so try to convert it, allowing base 10, octal, and hex forms */
	          stringptr = SvPV(value, stringlen);
		  utmp = strtoul( stringptr, NULL, 0 );
		  if (errno == EINVAL) {
		  	snmp_log(LOG_ERR, "Could not convert string to number in setValue: '%s'", stringptr);
			RETVAL = 0;
			break;
		  }

                  snmp_set_var_typed_value(request->requestvb, (u_char)type,
                                       (u_char *) &utmp, sizeof(utmp));
		  RETVAL = 1;
		  break;
	      }
	      else {
		snmp_log(LOG_ERR, "Non-unsigned-integer value passed to setValue with ASN_UNSIGNED/ASN_COUNTER/ASN_TIMETICKS: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
		break;
	      }

          case ASN_COUNTER64:
	      /* We want an integer here */
	      if ((SvTYPE(value) == SVt_IV) || (SvTYPE(value) == SVt_PVMG)) {
		  /* Good - got a real one (or a blessed scalar which we have to hope will turn out OK) */
		  ulltmp = SvIV(value);
		  RETVAL = 1;
	      }
	      else if (SvPOKp(value)) {
	          /* Might be OK - got a string, so try to convert it, allowing base 10, octal, and hex forms */
	          stringptr = SvPV(value, stringlen);
	          errno = 0;
		  ulltmp = strtoull( stringptr, NULL, 0 );
		  if (errno != 0) {
		      snmp_log(LOG_ERR, "Could not convert string to number in setValue: '%s'", stringptr);
		      RETVAL = 0;
		  } else

		      RETVAL = 1;
	      }
	      else {
		snmp_log(LOG_ERR, "Non-unsigned-integer value passed to setValue with ASN_COUNTER64: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
	      }
	      if (RETVAL) {
		  c64.high = (uint32_t)(ulltmp >> 32);
		  c64.low  = (uint32_t)(ulltmp >> 0);
		  snmp_set_var_typed_value(request->requestvb, (u_char)type,
				     (u_char *) &c64, sizeof(struct counter64));
	      }
	      break;

          case ASN_OCTET_STR:
          case ASN_BIT_STR:
          case ASN_OPAQUE:
	      /* Check that we have been passed something with a string value (or a blessed scalar) */
	      if (!SvPOKp(value) && (SvTYPE(value) != SVt_PVMG)) {
		snmp_log(LOG_ERR, "Non-string value passed to setValue with ASN_OCTET_STR/ASN_BIT_STR: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
		break;
	      }

	      /* Find length of string (strlen will *not* do, as these are binary strings) */
	      stringptr = SvPV(value, stringlen);

              snmp_set_var_typed_value(request->requestvb, (u_char)type,
                                       (u_char *) stringptr,
                                       stringlen);
              RETVAL = 1;
              break;

          case ASN_IPADDRESS:
	      /* IP addresses are passed as *binary* strings.
	       * In the case of IPv4 addresses, these are 4 bytes long.
	       * NOTE: the use of binary strings rather than dotted-quad or FQDNs was
	       * introduced here by Andrew Findlay's patch of March 17th 2003,
	       * and is effectively a change to the previous implied API which assumed
	       * the value was a (valid) hostname.
	       * Responsibility for decoding and resolving has been passed up to the Perl script.
	       */

	      /* Check that we have been passed something with a string value (or a blessed scalar) */
	      if (!SvPOKp(value) && (SvTYPE(value) != SVt_PVMG)) {
		snmp_log(LOG_ERR, "Non-string value passed to setValue with ASN_IPADDRESS: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
		break;
	      }

	      /* Find length of string (strlen will *not* do, as these are binary strings) */
	      stringptr = SvPV(value, stringlen);

	      # snmp_log(LOG_ERR, "IP address returned with length %d: %u.%u.%u.%u\n", stringlen, stringptr[0],
	      #     stringptr[1], stringptr[2], stringptr[3] );

	      # Sanity check on address length
	      if ((stringlen != 4) && (stringlen != 16)) {
	      		snmp_log(LOG_ERR, "IP address of %" NETSNMP_PRIz "d bytes passed to setValue with ASN_IPADDRESS\n", stringlen);
			RETVAL = 0;
			break;
	      }

              snmp_set_var_typed_value(request->requestvb, (u_char)type,
                                   stringptr, stringlen);
              RETVAL = 1;
              break;

          case ASN_OBJECT_ID:
	      /* Check that we have been passed something with a string value (or a blessed scalar) */
	      if (!SvPOKp(value) && (SvTYPE(value) != SVt_PVMG)) {
		snmp_log(LOG_ERR, "Non-string value passed to setValue with ASN_OBJECT_ID: type was %lu\n",
			(unsigned long)SvTYPE(value));
		RETVAL = 0;
		break;
	      }

	      /* Extract the string */
	      stringptr = SvPV(value, stringlen);

	      /* snmp_log(LOG_ERR, "setValue returning OID '%s'\n", stringptr); */

	      myoid_len = MAX_OID_LEN;
              if (!snmp_parse_oid(stringptr, myoid, &myoid_len)) {
                  snmp_log(LOG_ERR, "couldn't parse %s in setValue\n", stringptr);
		  RETVAL = 0;
		  break;
              } else {
		  /* snmp_log(LOG_ERR, "setValue returning OID length %d\n", myoid_len); */

                  request = (netsnmp_request_info *) SvIV(SvRV(me));
                  snmp_set_var_typed_value(request->requestvb, (u_char)type,
                                           (u_char *) myoid, (myoid_len * sizeof(myoid[0])) );
              }

              RETVAL = 1;
              break;
              
            default:
                snmp_log(LOG_ERR, "unknown var value type: %d\n",
                        type);
                RETVAL = 0;
                break;
        }

    OUTPUT:
        RETVAL
        
void
nari_setOID(me, value)
        SV *me;
        char *value;
    PREINIT:
	oid myoid[MAX_OID_LEN];
	size_t myoid_len = MAX_OID_LEN;
        netsnmp_request_info *request;
    CODE:
	myoid_len = MAX_OID_LEN;
	if (!snmp_parse_oid(value, myoid, &myoid_len)) {
            snmp_log(LOG_ERR, "couldn't parse %s in setOID\n", value);
        } else {
            request = (netsnmp_request_info *) SvIV(SvRV(me));
            snmp_set_var_objid(request->requestvb, myoid, myoid_len);
        }

void
nari_setError(me, rinfo, ecode)
        SV *me;
        SV *rinfo;
        int ecode;
    PREINIT:
        netsnmp_request_info *request;
        netsnmp_agent_request_info *reqinfo;
    CODE:
        request = (netsnmp_request_info *) SvIV(SvRV(me));
        reqinfo = (netsnmp_agent_request_info *) SvIV(SvRV(rinfo));
        netsnmp_set_request_error(reqinfo, request, ecode);

SV *
nari_next(me)
        SV *me;
    PREINIT:
        netsnmp_request_info *request;
        SV *arg, *rarg;
    CODE:
        {
            request = (netsnmp_request_info *) SvIV(SvRV(me));
            if (request && request->next) {
                request = request->next;
                rarg = newSViv(0);
                arg = newSVrv(rarg, "NetSNMP::agent::netsnmp_request_infoPtr");
                sv_setiv(arg, (IV) request);
                RETVAL = rarg;				
            } else {
                RETVAL = &sv_undef;
            }
        }
    OUTPUT:
        RETVAL

MODULE = NetSNMP::agent  PACKAGE = NetSNMP::agent::netsnmp_agent_request_info PREFIX = narqi_


SV *
narqi_getSourceIp(me)
        SV *me;
    PREINIT:
        netsnmp_agent_request_info *reqinfo;
	netsnmp_udp_addr_pair *addr_pair;
	struct sockaddr_in *from;
        SV *rarg;

    CODE:
        reqinfo = (netsnmp_agent_request_info *) SvIV(SvRV(me));

        /* XXX: transport-specific: UDP/IPv4 only! */
	addr_pair = (netsnmp_udp_addr_pair *) (reqinfo->asp->pdu->transport_data);
	from = (struct sockaddr_in *) &(addr_pair->remote_addr);
        rarg = newSVpv((const char *)(&from->sin_addr.s_addr), sizeof(from->sin_addr.s_addr));
        RETVAL = rarg;
    OUTPUT:
        RETVAL


SV *
narqi_getDestIp(me)
        SV *me;
    PREINIT:
        netsnmp_agent_request_info *reqinfo;
	netsnmp_udp_addr_pair *addr_pair;
	struct in_addr *to;
        SV *rarg;

    CODE:
        reqinfo = (netsnmp_agent_request_info *) SvIV(SvRV(me));

        /* XXX: transport-specific: UDP/IPv4 only! */
	addr_pair = (netsnmp_udp_addr_pair *) (reqinfo->asp->pdu->transport_data);
	to = (struct in_addr *) &(addr_pair->local_addr);
        rarg = newSVpv((const char *)(&to->s_addr), sizeof(to->s_addr));
        RETVAL = rarg;
    OUTPUT:
        RETVAL

int
narqi_getMode(me)
        SV *me;
    PREINIT:
        netsnmp_agent_request_info *reqinfo;
    CODE:
        reqinfo = (netsnmp_agent_request_info *) SvIV(SvRV(me));
        RETVAL = reqinfo->mode;
    OUTPUT:
        RETVAL

void
narqi_setMode(me, newvalue)
        SV *me;
        int newvalue;
    PREINIT:
        netsnmp_agent_request_info *reqinfo;
    CODE:
        reqinfo = (netsnmp_agent_request_info *) SvIV(SvRV(me));
        reqinfo->mode = newvalue;
        

MODULE = NetSNMP::agent		PACKAGE = NetSNMP::agent		

