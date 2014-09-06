/* -*- c -*- */
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x501
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "perl_snmptrapd.h"

#include "const-c.inc"

typedef struct trapd_cb_data_s {
   SV *perl_cb;
} trapd_cb_data;

typedef struct netsnmp_oid_s {
    oid                 *name;
    size_t               len;
    oid                  namebuf[ MAX_OID_LEN ];
} netsnmp_oid;

int   perl_trapd_handler( netsnmp_pdu           *pdu,
                          netsnmp_transport     *transport,
                          netsnmp_trapd_handler *handler)
{
    trapd_cb_data *cb_data;
    SV *pcallback;
    netsnmp_variable_list *vb;
    netsnmp_oid *o;
    SV **tmparray;
    int i, c = 0;
    u_char *outbuf;
    size_t ob_len = 0, oo_len = 0;
    AV *varbinds;
    HV *pduinfo;
    int noValuesReturned;
    int callingCFfailed = 0;
    int result = NETSNMPTRAPD_HANDLER_OK;
    netsnmp_pdu * v2pdu = NULL;

    dSP;
    ENTER;
    SAVETMPS;

    if (!pdu || !handler)
        return 0;

    /* nuke v1 PDUs */
    if (pdu->command == SNMP_MSG_TRAP) {
        v2pdu = convert_v1pdu_to_v2(pdu);
        pdu = v2pdu;
    }

    cb_data = handler->handler_data;
    if (!cb_data || !cb_data->perl_cb)
        return 0;

    pcallback = cb_data->perl_cb;

    /* get PDU related info */
    pduinfo = newHV();
#define STOREPDU(n, v) (void)hv_store(pduinfo, n, strlen(n), v, 0)
#define STOREPDUi(n, v) STOREPDU(n, newSViv(v))
#define STOREPDUs(n, v) STOREPDU(n, newSVpv(v, 0))
    STOREPDUi("version", pdu->version);
    STOREPDUs("notificationtype", ((pdu->command == SNMP_MSG_INFORM) ? "INFORM":"TRAP"));
    STOREPDUi("requestid", pdu->reqid);
    STOREPDUi("messageid", pdu->msgid);
    STOREPDUi("transactionid", pdu->transid);
    STOREPDUi("errorstatus", pdu->errstat);
    STOREPDUi("errorindex", pdu->errindex);
    if (pdu->version == 3) {
        STOREPDUi("securitymodel", pdu->securityModel);
        STOREPDUi("securitylevel", pdu->securityLevel);
        STOREPDU("contextName",
                 newSVpv(pdu->contextName, pdu->contextNameLen));
        STOREPDU("contextEngineID",
                 newSVpv((char *) pdu->contextEngineID,
                                    pdu->contextEngineIDLen));
        STOREPDU("securityEngineID",
                 newSVpv((char *) pdu->securityEngineID,
                                    pdu->securityEngineIDLen));
        STOREPDU("securityName",
                 newSVpv((char *) pdu->securityName, pdu->securityNameLen));
    } else {
        STOREPDU("community",
                 newSVpv((char *) pdu->community, pdu->community_len));
    }

    if (transport && transport->f_fmtaddr) {
        char *tstr = transport->f_fmtaddr(transport, pdu->transport_data,
                                          pdu->transport_data_length);
        STOREPDUs("receivedfrom", tstr);
        netsnmp_free(tstr);
    }


    /*
     * collect OID objects in a temp array first
     */
    /* get VARBIND related info */
    i = count_varbinds(pdu->variables);
    tmparray = malloc(sizeof(*tmparray) * i);

    for(vb = pdu->variables; vb; vb = vb->next_variable) {

        /* get the oid */
        o = malloc(sizeof(netsnmp_oid));
        o->name = o->namebuf;
        o->len = vb->name_length;
        memcpy(o->name, vb->name, vb->name_length * sizeof(oid));

#undef CALL_EXTERNAL_OID_NEW

#ifdef CALL_EXTERNAL_OID_NEW
        {
        SV *arg;
        SV *rarg;

        PUSHMARK(sp);

        rarg = sv_2mortal(newSViv((IV) 0));
        arg = sv_2mortal(newSVrv(rarg, "netsnmp_oidPtr"));
        sv_setiv(arg, (IV) o);
        XPUSHs(rarg);

        PUTBACK;
        i = perl_call_pv("NetSNMP::OID::newwithptr", G_SCALAR);
        SPAGAIN;

        if (i != 1) {
            snmp_log(LOG_ERR, "unhandled OID error.\n");
            /* ack XXX */
        }
        /* get the value */
        tmparray[c++] = POPs;
        SvREFCNT_inc(tmparray[c-1]);
        PUTBACK;
        }
#else /* build it and bless ourselves */
        {
            HV *hv = newHV();
            SV *rv = newRV_noinc((SV *) hv);
            SV *rvsub = newRV_noinc((SV *) newSViv((UV) o));
            rvsub = sv_bless(rvsub, gv_stashpv("netsnmp_oidPtr", 1));
            (void)hv_store(hv, "oidptr", 6,  rvsub, 0);
            rv = sv_bless(rv, gv_stashpv("NetSNMP::OID", 1));
            tmparray[c++] = rv;
        }
        
#endif /* build oid ourselves */
    }

    /*
     * build the varbind lists
     */
    varbinds = newAV();
    for(vb = pdu->variables, i = 0; vb; vb = vb->next_variable, i++) {
        /* push the oid */
        AV *vba;
        vba = newAV();


        /* get the value */
        outbuf = NULL;
        ob_len = 0;
        oo_len = 0;
	sprint_realloc_by_type(&outbuf, &ob_len, &oo_len, 1,
                               vb, 0, 0, 0);

        av_push(vba,tmparray[i]);
        av_push(vba,newSVpvn((char *) outbuf, oo_len));
        netsnmp_free(outbuf);
        av_push(vba,newSViv(vb->type));
        av_push(varbinds, (SV *) newRV_noinc((SV *) vba));
    }

    PUSHMARK(sp);

    /* store the collected information on the stack */
    XPUSHs(sv_2mortal(newRV_noinc((SV*) pduinfo)));
    XPUSHs(sv_2mortal(newRV_noinc((SV*) varbinds)));

    /* put the stack back in order */
    PUTBACK;

    /* actually call the callback function */
    if (SvTYPE(pcallback) == SVt_PVCV) {
        noValuesReturned = perl_call_sv(pcallback, G_SCALAR);
        /* XXX: it discards the results, which isn't right */
    } else if (SvROK(pcallback) && SvTYPE(SvRV(pcallback)) == SVt_PVCV) {
        /* reference to code */
        noValuesReturned = perl_call_sv(SvRV(pcallback), G_SCALAR);
    } else {
        snmp_log(LOG_ERR, " tried to call a perl function but failed to understand its type: (ref = %p, svrok: %lu, SVTYPE: %lu)\n", pcallback, (unsigned long)SvROK(pcallback), (unsigned long)SvTYPE(pcallback));
	callingCFfailed = 1;
    }

    if (!callingCFfailed) {
      SPAGAIN;

      if ( noValuesReturned == 0 ) {
        snmp_log(LOG_WARNING, " perl callback function %p did not return a scalar, assuming %d (NETSNMPTRAPD_HANDLER_OK)\n", pcallback, NETSNMPTRAPD_HANDLER_OK);
      }
      else {
	SV *rv = POPs;

	if (SvTYPE(rv) != SVt_IV) {
	  snmp_log(LOG_WARNING, " perl callback function %p returned a scalar of type %lu instead of an integer, assuming %d (NETSNMPTRAPD_HANDLER_OK)\n", pcallback, (unsigned long)SvTYPE(rv), NETSNMPTRAPD_HANDLER_OK);
	}
	else {
	  int rvi = (IV)SvIVx(rv);

	  if ((NETSNMPTRAPD_HANDLER_OK <= rvi) && (rvi <= NETSNMPTRAPD_HANDLER_FINISH)) {
	    snmp_log(LOG_DEBUG, " perl callback function %p returns %d\n", pcallback, rvi);
	    result = rvi;
	  }
	  else {
	    snmp_log(LOG_WARNING, " perl callback function %p returned an invalid scalar integer value (%d), assuming %d (NETSNMPTRAPD_HANDLER_OK)\n", pcallback, rvi, NETSNMPTRAPD_HANDLER_OK);
	  }
	}
      }

      PUTBACK;
    }

#ifdef DUMPIT
    fprintf(stderr, "DUMPDUMPDUMPDUMPDUMPDUMP\n");
    sv_dump(pduinfo);
    fprintf(stderr, "--------------------\n");
    sv_dump(varbinds);
#endif
    
    /* svREFCNT_dec((SV *) pduinfo); */
#ifdef NOT_THIS
    {
        SV *vba;
        while(vba = av_pop(varbinds)) {
            av_undef((AV *) vba);
        }
    }
    av_undef(varbinds);
#endif    
    free(tmparray);

      if (v2pdu) {
              snmp_free_pdu(v2pdu);
      }

    FREETMPS;
    LEAVE;
    return result;
}

MODULE = NetSNMP::TrapReceiver		PACKAGE = NetSNMP::TrapReceiver		

INCLUDE: const-xs.inc

MODULE = NetSNMP::TrapReceiver PACKAGE = NetSNMP::TrapReceiver PREFIX=trapd_
int
trapd_register(regoid, perlcallback)
	char *regoid;
        SV   *perlcallback;
    PREINIT:
	oid myoid[MAX_OID_LEN];
	size_t myoid_len = MAX_OID_LEN;
        trapd_cb_data *cb_data;
        netsnmp_trapd_handler *handler = NULL;
    CODE:
        {
            if (!regoid || !perlcallback) {
                RETVAL = 0;
                return;
            }
            if (strcmp(regoid,"all") == 0) {
                handler = 
                    netsnmp_add_global_traphandler(NETSNMPTRAPD_POST_HANDLER,
                                                   perl_trapd_handler);
            } else if (strcmp(regoid,"default") == 0) {
                handler = 
                    netsnmp_add_default_traphandler(perl_trapd_handler);
            } else if (!snmp_parse_oid(regoid, myoid, &myoid_len)) {
                snmp_log(LOG_ERR,
                         "Failed to parse oid for perl registration: %s\n",
                         regoid);
                RETVAL = 0;
                return;
            } else {
                handler = 
                    netsnmp_add_traphandler(perl_trapd_handler,
                                            myoid, myoid_len);
            }
        
            if (handler) {
                cb_data = malloc(sizeof(trapd_cb_data));
                cb_data->perl_cb = newSVsv(perlcallback);
                handler->handler_data = cb_data;
                handler->authtypes = (1 << VACM_VIEW_EXECUTE);
                RETVAL = 1;
            } else {
                RETVAL = 0;
            }
        }
    OUTPUT:
        RETVAL
