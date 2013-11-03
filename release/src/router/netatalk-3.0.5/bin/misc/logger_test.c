/*
  Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdbool.h>

#include <atalk/logger.h>

int main(int argc, char *argv[])
{
  set_processname("logger_Test");
#if 0
  LOG(log_severe, logtype_logger, "Logging Test starting: this should only log to syslog");

  /* syslog testing */
  LOG(log_severe, logtype_logger, "Disabling syslog logging.");
  unsetuplog("Default");
  LOG(log_error, logtype_default, "This shouldn't log to syslog: LOG(log_error, logtype_default).");
  LOG(log_error, logtype_logger, "This shouldn't log to syslog: LOG(log_error, logtype_logger).");
  setuplog("Default LOG_INFO");
  LOG(log_info, logtype_logger, "Set syslog logging to 'log_info', so this should log again. LOG(log_info, logtype_logger).");
  LOG(log_error, logtype_logger, "This should log to syslog: LOG(log_error, logtype_logger).");
  LOG(log_error, logtype_default, "This should log to syslog. LOG(log_error, logtype_default).");
  LOG(log_debug, logtype_logger, "This shouldn't log to syslog. LOG(log_debug, logtype_logger).");
  LOG(log_debug, logtype_default, "This shouldn't log to syslog. LOG(log_debug, logtype_default).");
  LOG(log_severe, logtype_logger, "Disabling syslog logging.");
  unsetuplog("Default");
#endif
  /* filelog testing */

  setuplog("DSI:maxdebug", "test.log");
  LOG(log_info, logtype_dsi, "This should log.");
  LOG(log_error, logtype_default, "This should not log.");

  setuplog("Default:debug", "test.log");
  LOG(log_debug, logtype_default, "This should log.");
  LOG(log_maxdebug, logtype_default, "This should not log.");

  LOG(log_maxdebug, logtype_dsi, "This should still log.");

  /* flooding prevention check */
  LOG(log_debug, logtype_default, "Flooding 3x");
  for (int i = 0; i < 3; i++) {
      LOG(log_debug, logtype_default, "Flooding...");
  }
  /* wipe the array */
  LOG(log_debug, logtype_default, "1"); LOG(log_debug, logtype_default, "2"); LOG(log_debug, logtype_default, "3");

  LOG(log_debug, logtype_default, "-============");
  LOG(log_debug, logtype_default, "Flooding 5x");
  for (int i = 0; i < 5; i++) {
      LOG(log_debug, logtype_default, "Flooding...");
  }
  LOG(log_debug, logtype_default, "1"); LOG(log_debug, logtype_default, "2"); LOG(log_debug, logtype_default, "3");

  LOG(log_debug, logtype_default, "o============");
  LOG(log_debug, logtype_default, "Flooding 2005x");
  for (int i = 0; i < 2005; i++) {
      LOG(log_debug, logtype_default, "Flooding...");
  }
  LOG(log_debug, logtype_default, "1"); LOG(log_debug, logtype_default, "2"); LOG(log_debug, logtype_default, "3");

  LOG(log_debug, logtype_default, "0============");
  LOG(log_debug, logtype_default, "Flooding 11x1");
  for (int i = 0; i < 11; i++) {
      LOG(log_error, logtype_default, "flooding 11x1 1");
  }

  LOG(log_debug, logtype_default, "1============");
  LOG(log_debug, logtype_default, "Flooding 11x2");
  for (int i = 0; i < 11; i++) {
      LOG(log_error, logtype_default, "flooding 11x2 1");
      LOG(log_error, logtype_default, "flooding 11x2 2");
  }

  LOG(log_debug, logtype_default, "2============");
  LOG(log_debug, logtype_default, "Flooding 11x3");
  for (int i = 0; i < 11; i++) {
      LOG(log_error, logtype_default, "flooding 11x3 1");
      LOG(log_error, logtype_default, "flooding 11x3 2");
      LOG(log_error, logtype_default, "flooding 11x3 3");
  }

  LOG(log_debug, logtype_default, "3============");
  LOG(log_debug, logtype_default, "Flooding 11x4");
  for (int i = 0; i < 11; i++) {
      LOG(log_error, logtype_default, "flooding 11x4 1");
      LOG(log_error, logtype_default, "flooding 11x4 2");
      LOG(log_error, logtype_default, "flooding 11x4 3");
      LOG(log_error, logtype_default, "flooding 11x4 4");
  }


  return 0;
}


