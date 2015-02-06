/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file cpuworker.h
 * \brief Header file for cpuworker.c.
 **/

#ifndef TOR_CPUWORKER_H
#define TOR_CPUWORKER_H

void cpu_init(void);
void cpuworkers_rotate(void);
int connection_cpu_finished_flushing(connection_t *conn);
int connection_cpu_reached_eof(connection_t *conn);
int connection_cpu_process_inbuf(connection_t *conn);
struct create_cell_t;
int assign_onionskin_to_cpuworker(connection_t *cpuworker,
                                  or_circuit_t *circ,
                                  struct create_cell_t *onionskin);

uint64_t estimated_usec_for_onionskins(uint32_t n_requests,
                                       uint16_t onionskin_type);
void cpuworker_log_onionskin_overhead(int severity, int onionskin_type,
                                      const char *onionskin_type_name);

#endif

