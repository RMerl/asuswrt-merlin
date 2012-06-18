/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

struct wreplsrv_service;
struct wreplsrv_in_connection;
struct wreplsrv_out_connection;
struct wreplsrv_partner;

#define WREPLSRV_VALID_ASSOC_CTX	0x12345678
#define WREPLSRV_INVALID_ASSOC_CTX	0x0000000a

/*
  state of an incoming wrepl call
*/
struct wreplsrv_in_call {
	struct wreplsrv_in_connection *wreplconn;
	struct wrepl_packet req_packet;
	struct wrepl_packet rep_packet;
	bool terminate_after_send;
};

/*
  state of an incoming wrepl connection
*/
struct wreplsrv_in_connection {
	struct wreplsrv_in_connection *prev,*next;
	struct stream_connection *conn;
	struct packet_context *packet;

	/* our global service context */
	struct wreplsrv_service *service;

	/*
	 * the partner that connects us,
	 * can be NULL, when we got a connection
	 * from an unknown address
	 */
	struct wreplsrv_partner *partner;

	/* keep track of the assoc_ctx's */
	struct {
		bool stopped;
		uint32_t our_ctx;
		uint32_t peer_ctx;
	} assoc_ctx;
};

/*
  state of an outgoing wrepl connection
*/
struct wreplsrv_out_connection {
	/* our global service context */
	struct wreplsrv_service *service;

	/*
	 * the partner we connect
	 */
	struct wreplsrv_partner *partner;

	/* keep track of the assoc_ctx's */
	struct {
		uint32_t our_ctx;
		uint32_t peer_ctx;
		uint16_t peer_major;
	} assoc_ctx;

	/* 
	 * the client socket to the partner,
	 * NULL if not yet connected
	 */
	struct wrepl_socket *sock;
};

enum winsrepl_partner_type {
	WINSREPL_PARTNER_NONE = 0x0,
	WINSREPL_PARTNER_PULL = 0x1,
	WINSREPL_PARTNER_PUSH = 0x2,
	WINSREPL_PARTNER_BOTH = (WINSREPL_PARTNER_PULL | WINSREPL_PARTNER_PUSH)
};

#define WINSREPL_DEFAULT_PULL_INTERVAL (30*60)
#define WINSREPL_DEFAULT_PULL_RETRY_INTERVAL (30)

#define WINSREPL_DEFAULT_PUSH_CHANGE_COUNT (0)

/*
 this represents one of our configured partners
*/
struct wreplsrv_partner {
	struct wreplsrv_partner *prev,*next;

	/* our global service context */
	struct wreplsrv_service *service;

	/* the netbios name of the partner, mostly just for debugging */
	const char *name;

	/* the ip-address of the partner */
	const char *address;

	/* 
	 * as wins partners identified by ip-address, we need to use a specific source-ip
	 *  when we want to connect to the partner
	 */
	const char *our_address;

	/* the type of the partner, pull, push or both */
	enum winsrepl_partner_type type;

	/* pull specific options */
	struct {
		/* the interval between 2 pull replications to the partner */
		uint32_t interval;

		/* the retry_interval if a pull cycle failed to the partner */
		uint32_t retry_interval;

		/* the error count till the last success */
		uint32_t error_count;

		/* the status of the last pull cycle */
		NTSTATUS last_status;

		/* the timestamp of the next pull try */
		struct timeval next_run;

		/* this is a list of each wins_owner the partner knows about */
		struct wreplsrv_owner *table;

		/* the outgoing connection to the partner */
		struct wreplsrv_out_connection *wreplconn;

		/* the current pending pull cycle request */
		struct composite_context *creq;

		/* the pull cycle io params */
		struct wreplsrv_pull_cycle_io *cycle_io;

		/* the current timed_event to the next pull cycle */
		struct tevent_timer *te;
	} pull;

	/* push specific options */
	struct {
		/* change count till push notification */
		uint32_t change_count;

		/* the last wins db maxVersion have reported to the partner */
		uint64_t maxVersionID;

		/* we should use WREPL_REPL_INFORM* messages to this partner */
		bool use_inform;

		/* the error count till the last success */
		uint32_t error_count;

		/* the status of the last push cycle */
		NTSTATUS last_status;

		/* the outgoing connection to the partner */
		struct wreplsrv_out_connection *wreplconn;

		/* the current push notification */
		struct composite_context *creq;

		/* the pull cycle io params */
		struct wreplsrv_push_notify_io *notify_io;
	} push;
};

struct wreplsrv_owner {
	struct wreplsrv_owner *prev,*next;

	/* this hold the owner_id (address), min_version, max_version and partner_type */
	struct wrepl_wins_owner owner;

	/* can be NULL if this owner isn't a configure partner */
	struct wreplsrv_partner *partner; 
};

/*
  state of the whole wrepl service
*/
struct wreplsrv_service {
	/* the whole wrepl service is in one task */
	struct task_server *task;

	/* the time the service was started */
	struct timeval startup_time;

	/* the winsdb handle */
	struct winsdb_handle *wins_db;

	/* some configuration */
	struct {
		/* the wins config db handle */
		struct ldb_context *ldb;

		/* the last wins config db seqnumber we know about */
		uint64_t seqnumber;

		/* 
		 * the interval (in secs) till an active record will be marked as RELEASED 
		 */
		uint32_t renew_interval;

		/* 
		 * the interval (in secs) a record remains in RELEASED state,
		 * before it will be marked as TOMBSTONE
		 * (also known as extinction interval)
		 */
		uint32_t tombstone_interval;

		/* 
		 * the interval (in secs) a record remains in TOMBSTONE state,
		 * before it will be removed from the database.
		 * See also 'tombstone_extra_timeout'.
		 * (also known as extinction timeout)
		 */
		uint32_t tombstone_timeout;

		/* 
		 * the interval (in secs) a record remains in TOMBSTONE state,
		 * even after 'tombstone_timeout' passes the current timestamp.
		 * this is the minimum uptime of the wrepl service, before
		 * we start delete tombstones. This is to prevent deletion of
		 * tombstones, without replacte them.
		 */
		uint32_t tombstone_extra_timeout;

		/* 
		 * the interval (in secs) till a replica record will be verified
		 * with the owning wins server
		 */
		uint32_t verify_interval;

		/* 
		 * the interval (in secs) till a do a database cleanup
		 */
		uint32_t scavenging_interval;

		/* 
		 * the interval (in secs) to the next periodic processing
		 * (this is the maximun interval)
		 */
		uint32_t periodic_interval;
	} config;

	/* all incoming connections */
	struct wreplsrv_in_connection *in_connections;

	/* all partners (pull and push) */
	struct wreplsrv_partner *partners;

	/*
	 * this is our local wins_owner entry, this is also in the table list
	 * but we need a pointer to it, because we need to update it on each 
	 * query to wreplsrv_find_owner(), as the local records can be added
	 * to the wins.ldb from external tools and the winsserver
	 */
	struct wreplsrv_owner *owner;

	/* this is a list of each wins_owner we know about in our database */
	struct wreplsrv_owner *table;

	/* some stuff for periodic processing */
	struct {
		/*
		 * the timestamp for the next event,
		 * this is the timstamp passed to event_add_timed()
		 */
		struct timeval next_event;

		/* here we have a reference to the timed event the schedules the periodic stuff */
		struct tevent_timer *te;
	} periodic;

	/* some stuff for scavenging processing */
	struct {
		/*
		 * the timestamp for the next scavenging run,
		 * this is the timstamp passed to event_add_timed()
		 */
		struct timeval next_run;

		/*
		 * are we currently inside a scavenging run
		 */
		bool processing;	
	} scavenging;
};

struct socket_context;
struct wrepl_name;
#include "wrepl_server/wrepl_out_helpers.h"
#include "wrepl_server/wrepl_server_proto.h"
