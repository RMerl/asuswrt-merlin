/*
 * etimetest.c
 *
 * HEADER Testing read_config.c shutdown callback
 *
 * Expected SUCCESSes for all tests:    8
 *
 * Test of snmpd_unregister_config_handler                    	SUCCESSes:  4
 * Test of unregister_all_config_handlers	              	SUCCESSes:  4
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/testing.h>

/* #undef TEST_INTERNAL_API */
#define TEST_INTERNAL_API 1

/*
 * Global variables.
 */
int callback_called = 0;

static void test_callback(void);
static void test_callback(void) {
	++callback_called;
};

static void parse_config(const char *token, char *cptr);
static void parse_config(const char *token, char *cptr) {
	/* do nothing */
};

/* each test returns 0 on success, and >0 on failure */
int test1(void);
int test2(void);
int test3(void);

int (*tests[])(void) = { test1, test2, test3 };

int main(int argc, char *argv[])
{
    int i,ret=0;
    for(i=0;i<sizeof(tests)/sizeof(*tests);++i)
	    ret+=tests[i]();
    if (__did_plan == 0) {
       PLAN(__test_counter);
    }
    return ret;
}

int test1() {
    int sum = 0;
    callback_called = 0;
    fprintf(stdout, "# snmpd_unregister_config_handler tests\n");
    init_snmp("testing");
    OKF(callback_called==0, ("Callback shouldn't be called before registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    register_app_config_handler("testing", parse_config, test_callback, "module-name module-path");
    OKF(callback_called==0, ("Callback shouldn't be called after registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    unregister_app_config_handler("testing");
    OKF(callback_called==1, ("Callback should have been called once and only once after unregistering handler - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    snmp_shutdown("testing");
    OKF(callback_called==0, ("Callback should have been called once and only once after shutdown - it was called %d times\n",callback_called));
    sum += callback_called;

    return (sum==1)?0:1;
}

int test2(void) {
    int sum = 0;
    callback_called = 0;
    fprintf(stdout, "# unregister_all_config_handlers tests\n");
    init_snmp("testing");
    OKF(callback_called==0, ("Callback shouldn't be called before registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    register_app_config_handler("testing", parse_config, test_callback, "module-name module-path");
    OKF(callback_called==0, ("Callback shouldn't be called after registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    snmp_shutdown("testing");
    OKF(callback_called==1, ("Callback should have been called once and only once during shutdown - it was called %d times\n",callback_called));
    sum += callback_called;

    return (sum==1)?0:1;
}

int test3(void) {
#ifdef TEST_INTERNAL_API
    int sum = 0;
    callback_called = 0;
    fprintf(stdout, "# unregister_all_config_handlers internal api tests\n");
    init_snmp("testing");
    OKF(callback_called==0, ("Callback shouldn't be called before registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    register_app_config_handler("testing", parse_config, test_callback, "module-name module-path");
    OKF(callback_called==0, ("Callback shouldn't be called after registering it - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    unregister_all_config_handlers();
    OKF(callback_called==1, ("Callback should have been called once and only once after unregistering handler - it was called %d times\n",callback_called));
    sum += callback_called;

    callback_called = 0;
    snmp_shutdown("testing");
    OKF(callback_called==0, ("Callback should not have been called during shutdown after unregistering handler - it was called %d times\n",callback_called));
    sum += callback_called;

    return (sum==1)?0:1;
#else
    return 0;
#endif
}
