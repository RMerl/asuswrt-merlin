#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x501
#endif

#include <EXTERN.h>
#include "perl.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "snmp_perl.h"

static PerlInterpreter *my_perl;

void            boot_DynaLoader(pTHX_ CV * cv);

void
xs_init(pTHX)
{
    char            myfile[] = __FILE__;
    char            modulename[] = "DynaLoader::boot_DynaLoader";
    /*
     * DynaLoader is a special case 
     */
    newXS(modulename, boot_DynaLoader, myfile);
}

void
maybe_source_perl_startup(void)
{
    int             argc;
    char          **argv;
    char          **env;
    char           *embedargs[] = { NULL, NULL };
    const char     *perl_init_file = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
							   NETSNMP_DS_AGENT_PERL_INIT_FILE);
    char            init_file[SNMP_MAXBUF];
    int             res;

    static int      have_done_init = 0;

    if (have_done_init)
        return;
    have_done_init = 1;

    embedargs[0] = strdup("");
    if (!perl_init_file) {
        snprintf(init_file, sizeof(init_file) - 1,
                 "%s/%s", SNMPSHAREPATH, "snmp_perl.pl");
        perl_init_file = init_file;
    }
    embedargs[1] = strdup(perl_init_file);

    DEBUGMSGTL(("perl", "initializing perl (%s)\n", embedargs[1]));
    argc = 0;
    argv = NULL;
    env = NULL;
    PERL_SYS_INIT3(&argc, &argv, &env);
    my_perl = perl_alloc();
    if (!my_perl) {
        snmp_log(LOG_ERR,
                 "embedded perl support failed to initialize (perl_alloc())\n");
        goto bail_out;
    }

    perl_construct(my_perl);

#ifdef PERL_EXIT_DESTRUCT_END
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#endif

    res = perl_parse(my_perl, xs_init, 2, embedargs, NULL);
    if (res) {
        snmp_log(LOG_ERR,
                 "embedded perl support failed to initialize (perl_parse(%s)"
                 " returned %d)\n", embedargs[1], res);
        goto bail_out;
    }

    res = perl_run(my_perl);
    if (res) {
        snmp_log(LOG_ERR,
                 "embedded perl support failed to initialize (perl_run()"
                 " returned %d)\n", res);
        goto bail_out;
    }

    free(embedargs[0]);
    free(embedargs[1]);

    DEBUGMSGTL(("perl", "done initializing perl\n"));

    return;

  bail_out:
    free(embedargs[0]);
    free(embedargs[1]);
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_DISABLE_PERL, 1);
    return;
}

void
do_something_perlish(char *something)
{
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_DISABLE_PERL)) {
        return;
    }
    maybe_source_perl_startup();
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_DISABLE_PERL)) {
        return;
    }
    DEBUGMSGTL(("perl", "calling perl\n"));
#if defined(HAVE_EVAL_PV) || defined(eval_pv)
    /* newer perl */
    eval_pv(something, TRUE);
#else
#if defined(HAVE_PERL_EVAL_PV_LC) || defined(perl_eval_pv)
    /* older perl? */
    perl_eval_pv(something, TRUE);
#else /* HAVE_PERL_EVAL_PV_LC */
#ifdef HAVE_PERL_EVAL_PV_UC
    /* older perl? */
    Perl_eval_pv(my_perl, something, TRUE);
#else /* !HAVE_PERL_EVAL_PV_UC */
#error embedded perl broken 
#endif /* !HAVE_PERL_EVAL_PV_LC */
#endif /* !HAVE_PERL_EVAL_PV_UC */
#endif /* !HAVE_EVAL_PV */
    DEBUGMSGTL(("perl", "finished calling perl\n"));
}

void
perl_config_handler(const char *token, char *line)
{
    do_something_perlish(line);
}

void
init_perl(void)
{
    const char     *appid = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
						  NETSNMP_DS_LIB_APPTYPE);
    const char     *defaultid = "snmpd";

    if (!appid) {
        appid = defaultid;
    }

    /*
     * register config handlers 
     */
    snmpd_register_config_handler("perl", perl_config_handler, NULL,
                                  "PERLCODE");

    /*
     * define the perlInitFile token to point to an init file 
     */
    netsnmp_ds_register_premib(ASN_OCTET_STR, appid, "perlInitFile",
			       NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_PERL_INIT_FILE);

    /*
     * define the perlInitFile token to point to an init file 
     */
    netsnmp_ds_register_premib(ASN_BOOLEAN, appid, "disablePerl",
			       NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_DISABLE_PERL);
}

void
shutdown_perl(void)
{
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_DISABLE_PERL) ||
        my_perl == NULL) {
        return;
    }
    DEBUGMSGTL(("perl", "shutting down perl\n"));
    perl_destruct(my_perl);
    my_perl = NULL;
    DEBUGMSGTL(("perl", "finished shutting down perl\n"));
}
