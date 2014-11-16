/* Process handling
 *
 * Copyright © 2013, Benoît Knecht <benoit.knecht@fsfe.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <unistd.h>
#include "clients.h"

struct child {
	pid_t pid;
	time_t age;
	struct client_cache_s *client;
};

extern struct child *children;
extern int number_of_children;

/**
 * Fork a new child (just like fork()) but keep track of how many childs are
 * already running, and refuse fo fork if there are too many.
 * @return -1 if it couldn't fork, 0 in the child process, the pid of the
 *         child process in the parent process.
 */
pid_t process_fork(struct client_cache_s *client);

/**
 * Handler to be called upon receiving SIGCHLD. This signal is received by the
 * parent process when a child terminates, and this handler updates the number
 * of running childs accordingly.
 * @param signal The signal number.
 */
void process_handle_child_termination(int signal);

/**
 * Daemonize the current process by forking itself and redirecting standard
 * input, standard output and standard error to /dev/null.
 * @return The pid of the process.
 */ 
int process_daemonize(void);

/**
 * Check if the process corresponding to the pid found in the pid file is
 * running.
 * @param fname The path to the pid file.
 * @return 0 if no other instance is running, -1 if the file name is invalid,
 *         -2 if another instance is running.
 */
int process_check_if_running(const char *fname);

/**
 * Kill all child processes
 */
void process_reap_children(void);

#endif // __PROCESS_H__
