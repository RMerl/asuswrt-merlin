#include "or.h"
#include "test.h"
#define HIBERNATE_PRIVATE
#include "hibernate.h"
#include "config.h"
#define STATEFILE_PRIVATE
#include "statefile.h"

#define NS_MODULE accounting

#define NS_SUBMODULE limits

/*
 * Test to make sure accounting triggers hibernation
 * correctly with both sum or max rules set
 */

static or_state_t *or_state;
NS_DECL(or_state_t *, get_or_state, (void));
static or_state_t *
NS(get_or_state)(void)
{
  return or_state;
}

static void
test_accounting_limits(void *arg)
{
  or_options_t *options = get_options_mutable();
  time_t fake_time = time(NULL);
  (void) arg;

  NS_MOCK(get_or_state);
  or_state = or_state_new();

  options->AccountingMax = 100;
  options->AccountingRule = ACCT_MAX;

  tor_assert(accounting_is_enabled(options));
  configure_accounting(fake_time);

  accounting_add_bytes(10, 0, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 0);

  accounting_add_bytes(90, 0, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 1);

  options->AccountingMax = 200;
  options->AccountingRule = ACCT_SUM;

  accounting_add_bytes(0, 10, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 0);

  accounting_add_bytes(0, 90, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 1);

  options->AccountingRule = ACCT_OUT;

  accounting_add_bytes(100, 10, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 0);

  accounting_add_bytes(0, 90, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 1);

  options->AccountingMax = 300;
  options->AccountingRule = ACCT_IN;

  accounting_add_bytes(10, 100, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 0);

  accounting_add_bytes(90, 0, 1);
  fake_time += 1;
  consider_hibernation(fake_time);
  tor_assert(we_are_hibernating() == 1);

  goto done;
 done:
  NS_UNMOCK(get_or_state);
  or_state_free(or_state);
}

#undef NS_SUBMODULE

struct testcase_t accounting_tests[] = {
  { "bwlimits", test_accounting_limits, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

