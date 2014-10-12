/*
 *  Unix SMB/CIFS implementation.
 *  NBT netbios routines and daemon - version 2
 *
 *  Copyright (C) Andrew Tridgell			1994-1998
 *  Copyright (C) Jeremy Allison			1994-2005
 *  Copyright (C) Luke Kenneth Casson Leighton		1994-1998
 *  Copyright (C) John H Terpstra			1995-1998
 *  Copyright (C) Christopher R. Hertel			1998
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com>	2002
 *  Copyright (C) Jelmer Vernooij			2002,2003
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The following definitions come from nmbd/asyncdns.c  */

int asyncdns_fd(void);
void kill_async_dns_child(void);
void start_async_dns(void);
void run_dns_queue(void);
bool queue_dns_query(struct packet_struct *p,struct nmb_name *question);
bool queue_dns_query(struct packet_struct *p,struct nmb_name *question);
void kill_async_dns_child(void);

/* The following definitions come from nmbd/nmbd.c  */

struct event_context *nmbd_event_context(void);
struct messaging_context *nmbd_messaging_context(void);

/* The following definitions come from nmbd/nmbd_become_dmb.c  */

void add_domain_names(time_t t);

/* The following definitions come from nmbd/nmbd_become_lmb.c  */

void insert_permanent_name_into_unicast( struct subnet_record *subrec,
                                                struct nmb_name *nmbname, uint16 nb_type );
void unbecome_local_master_browser(struct subnet_record *subrec, struct work_record *work,
                                   bool force_new_election);
void become_local_master_browser(struct subnet_record *subrec, struct work_record *work);
void set_workgroup_local_master_browser_name( struct work_record *work, const char *newname);

/* The following definitions come from nmbd/nmbd_browserdb.c  */

void update_browser_death_time( struct browse_cache_record *browc );
struct browse_cache_record *create_browser_in_lmb_cache( const char *work_name,
                                                         const char *browser_name,
                                                         struct in_addr ip );
struct browse_cache_record *find_browser_in_lmb_cache( const char *browser_name );
void expire_lmb_browsers( time_t t );

/* The following definitions come from nmbd/nmbd_browsesync.c  */

void dmb_expire_and_sync_browser_lists(time_t t);
void announce_and_sync_with_domain_master_browser( struct subnet_record *subrec,
                                                   struct work_record *work);
void collect_all_workgroup_names_from_wins_server(time_t t);
void sync_all_dmbs(time_t t);

/* The following definitions come from nmbd/nmbd_elections.c  */

void check_master_browser_exists(time_t t);
void run_elections(time_t t);
void process_election(struct subnet_record *subrec, struct packet_struct *p, const char *buf);
bool check_elections(void);
void nmbd_message_election(struct messaging_context *msg,
			   void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id,
			   DATA_BLOB *data);

/* The following definitions come from nmbd/nmbd_incomingdgrams.c  */

void tell_become_backup(void);
void process_host_announce(struct subnet_record *subrec, struct packet_struct *p, const char *buf);
void process_workgroup_announce(struct subnet_record *subrec, struct packet_struct *p, const char *buf);
void process_local_master_announce(struct subnet_record *subrec, struct packet_struct *p, const char *buf);
void process_master_browser_announce(struct subnet_record *subrec,
                                     struct packet_struct *p,const char *buf);
void process_lm_host_announce(struct subnet_record *subrec, struct packet_struct *p, const char *buf, int len);
void process_get_backup_list_request(struct subnet_record *subrec,
                                     struct packet_struct *p,const char *buf);
void process_reset_browser(struct subnet_record *subrec,
                                  struct packet_struct *p,const char *buf);
void process_announce_request(struct subnet_record *subrec, struct packet_struct *p, const char *buf);
void process_lm_announce_request(struct subnet_record *subrec, struct packet_struct *p, const char *buf, int len);

/* The following definitions come from nmbd/nmbd_incomingrequests.c  */

void process_name_release_request(struct subnet_record *subrec,
                                  struct packet_struct *p);
void process_name_refresh_request(struct subnet_record *subrec,
                                  struct packet_struct *p);
void process_name_registration_request(struct subnet_record *subrec,
                                       struct packet_struct *p);
void process_node_status_request(struct subnet_record *subrec, struct packet_struct *p);
void process_name_query_request(struct subnet_record *subrec, struct packet_struct *p);

/* The following definitions come from nmbd/nmbd_lmhosts.c  */

void load_lmhosts_file(const char *fname);
bool find_name_in_lmhosts(struct nmb_name *nmbname, struct name_record **namerecp);

/* The following definitions come from nmbd/nmbd_logonnames.c  */

void add_logon_names(void);

/* The following definitions come from nmbd/nmbd_mynames.c  */

void register_my_workgroup_one_subnet(struct subnet_record *subrec);
bool register_my_workgroup_and_names(void);
void release_wins_names(void);
void refresh_my_names(time_t t);

/* The following definitions come from nmbd/nmbd_namelistdb.c  */

void set_samba_nb_type(void);
void remove_name_from_namelist(struct subnet_record *subrec,
				struct name_record *namerec );
struct name_record *find_name_on_subnet(struct subnet_record *subrec,
				const struct nmb_name *nmbname,
				bool self_only);
struct name_record *find_name_for_remote_broadcast_subnet(struct nmb_name *nmbname,
						bool self_only);
void update_name_ttl( struct name_record *namerec, int ttl );
bool add_name_to_subnet( struct subnet_record *subrec,
			const char *name,
			int type,
			uint16 nb_flags,
			int ttl,
			enum name_source source,
			int num_ips,
			struct in_addr *iplist);
void standard_success_register(struct subnet_record *subrec,
                             struct userdata_struct *userdata,
                             struct nmb_name *nmbname, uint16 nb_flags, int ttl,
                             struct in_addr registered_ip);
void standard_fail_register( struct subnet_record   *subrec,
                             struct nmb_name        *nmbname );
bool find_ip_in_name_record( struct name_record *namerec, struct in_addr ip );
void add_ip_to_name_record( struct name_record *namerec, struct in_addr new_ip );
void remove_ip_from_name_record( struct name_record *namerec,
                                 struct in_addr      remove_ip );
void standard_success_release( struct subnet_record   *subrec,
                               struct userdata_struct *userdata,
                               struct nmb_name        *nmbname,
                               struct in_addr          released_ip );
void expire_names(time_t t);
void add_samba_names_to_subnet( struct subnet_record *subrec );
void dump_name_record( struct name_record *namerec, XFILE *fp);
void dump_all_namelists(void);

/* The following definitions come from nmbd/nmbd_namequery.c  */

bool query_name(struct subnet_record *subrec, const char *name, int type,
                   query_name_success_function success_fn,
                   query_name_fail_function fail_fn,
                   struct userdata_struct *userdata);
bool query_name_from_wins_server(struct in_addr ip_to,
                   const char *name, int type,
                   query_name_success_function success_fn,
                   query_name_fail_function fail_fn,
                   struct userdata_struct *userdata);

/* The following definitions come from nmbd/nmbd_nameregister.c  */

void register_name(struct subnet_record *subrec,
                   const char *name, int type, uint16 nb_flags,
                   register_name_success_function success_fn,
                   register_name_fail_function fail_fn,
                   struct userdata_struct *userdata);
void wins_refresh_name(struct name_record *namerec);

/* The following definitions come from nmbd/nmbd_namerelease.c  */

void release_name(struct subnet_record *subrec, struct name_record *namerec,
		  release_name_success_function success_fn,
		  release_name_fail_function fail_fn,
		  struct userdata_struct *userdata);

/* The following definitions come from nmbd/nmbd_nodestatus.c  */

bool node_status(struct subnet_record *subrec, struct nmb_name *nmbname,
                 struct in_addr send_ip, node_status_success_function success_fn,
                 node_status_fail_function fail_fn, struct userdata_struct *userdata);

/* The following definitions come from nmbd/nmbd_packets.c  */

bool nmbd_init_packet_server(void);

uint16 get_nb_flags(char *buf);
void set_nb_flags(char *buf, uint16 nb_flags);
struct response_record *queue_register_name( struct subnet_record *subrec,
                          response_function resp_fn,
                          timeout_response_function timeout_fn,
                          register_name_success_function success_fn,
                          register_name_fail_function fail_fn,
                          struct userdata_struct *userdata,
                          struct nmb_name *nmbname,
                          uint16 nb_flags);
void queue_wins_refresh(struct nmb_name *nmbname,
			response_function resp_fn,
			timeout_response_function timeout_fn,
			uint16 nb_flags,
			struct in_addr refresh_ip,
			const char *tag);
struct response_record *queue_register_multihomed_name( struct subnet_record *subrec,
							response_function resp_fn,
							timeout_response_function timeout_fn,
							register_name_success_function success_fn,
							register_name_fail_function fail_fn,
							struct userdata_struct *userdata,
							struct nmb_name *nmbname,
							uint16 nb_flags,
							struct in_addr register_ip,
							struct in_addr wins_ip);
struct response_record *queue_release_name( struct subnet_record *subrec,
					    response_function resp_fn,
					    timeout_response_function timeout_fn,
					    release_name_success_function success_fn,
					    release_name_fail_function fail_fn,
					    struct userdata_struct *userdata,
					    struct nmb_name *nmbname,
					    uint16 nb_flags,
					    struct in_addr release_ip,
					    struct in_addr dest_ip);
struct response_record *queue_query_name( struct subnet_record *subrec,
                          response_function resp_fn,
                          timeout_response_function timeout_fn,
                          query_name_success_function success_fn,
                          query_name_fail_function fail_fn,
                          struct userdata_struct *userdata,
                          struct nmb_name *nmbname);
struct response_record *queue_query_name_from_wins_server( struct in_addr to_ip,
                          response_function resp_fn,
                          timeout_response_function timeout_fn,
                          query_name_success_function success_fn,
                          query_name_fail_function fail_fn,
                          struct userdata_struct *userdata,
                          struct nmb_name *nmbname);
struct response_record *queue_node_status( struct subnet_record *subrec,
                          response_function resp_fn,
                          timeout_response_function timeout_fn,
                          node_status_success_function success_fn,
                          node_status_fail_function fail_fn,
                          struct userdata_struct *userdata,
                          struct nmb_name *nmbname,
                          struct in_addr send_ip);
void reply_netbios_packet(struct packet_struct *orig_packet,
                          int rcode, enum netbios_reply_type_code rcv_code, int opcode,
                          int ttl, char *data,int len);
void queue_packet(struct packet_struct *packet);
void run_packet_queue(void);
void retransmit_or_expire_response_records(time_t t);
bool listen_for_packets(bool run_election);
bool send_mailslot(bool unique, const char *mailslot,char *buf, size_t len,
                   const char *srcname, int src_type,
                   const char *dstname, int dest_type,
                   struct in_addr dest_ip,struct in_addr src_ip,
		   int dest_port);

/* The following definitions come from nmbd/nmbd_processlogon.c  */

bool initialize_nmbd_proxy_logon(void);

void process_logon_packet(struct packet_struct *p, const char *buf,int len,
                          const char *mailslot);

/* The following definitions come from nmbd/nmbd_responserecordsdb.c  */

void remove_response_record(struct subnet_record *subrec,
				struct response_record *rrec);
struct response_record *make_response_record( struct subnet_record *subrec,
					      struct packet_struct *p,
					      response_function resp_fn,
					      timeout_response_function timeout_fn,
					      success_function success_fn,
					      fail_function fail_fn,
					      struct userdata_struct *userdata);
struct response_record *find_response_record(struct subnet_record **ppsubrec,
				uint16 id);
bool is_refresh_already_queued(struct subnet_record *subrec, struct name_record *namerec);

/* The following definitions come from nmbd/nmbd_sendannounce.c  */

void send_browser_reset(int reset_type, const char *to_name, int to_type, struct in_addr to_ip);
void broadcast_announce_request(struct subnet_record *subrec, struct work_record *work);
void announce_my_server_names(time_t t);
void announce_my_lm_server_names(time_t t);
void reset_announce_timer(void);
void announce_myself_to_domain_master_browser(time_t t);
void announce_my_servers_removed(void);
void announce_remote(time_t t);
void browse_sync_remote(time_t t);

/* The following definitions come from nmbd/nmbd_serverlistdb.c  */

void remove_all_servers(struct work_record *work);
struct server_record *find_server_in_workgroup(struct work_record *work, const char *name);
void remove_server_from_workgroup(struct work_record *work, struct server_record *servrec);
struct server_record *create_server_on_workgroup(struct work_record *work,
                                                 const char *name,int servertype,
                                                 int ttl, const char *comment);
void update_server_ttl(struct server_record *servrec, int ttl);
void expire_servers(struct work_record *work, time_t t);
void write_browse_list_entry(XFILE *fp, const char *name, uint32 rec_type,
		const char *local_master_browser_name, const char *description);
void write_browse_list(time_t t, bool force_write);

/* The following definitions come from nmbd/nmbd_subnetdb.c  */

void close_subnet(struct subnet_record *subrec);
struct subnet_record *make_normal_subnet(const struct interface *iface);
bool create_subnets(void);
bool we_are_a_wins_client(void);
struct subnet_record *get_next_subnet_maybe_unicast(struct subnet_record *subrec);
struct subnet_record *get_next_subnet_maybe_unicast_or_wins_server(struct subnet_record *subrec);

/* The following definitions come from nmbd/nmbd_synclists.c  */

void sync_browse_lists(struct work_record *work,
		       char *name, int nm_type,
		       struct in_addr ip, bool local, bool servers);
void sync_check_completion(void);

/* The following definitions come from nmbd/nmbd_winsproxy.c  */

void make_wins_proxy_name_query_request( struct subnet_record *subrec,
                                         struct packet_struct *incoming_packet,
                                         struct nmb_name *question_name);

/* The following definitions come from nmbd/nmbd_winsserver.c  */

struct name_record *find_name_on_wins_subnet(const struct nmb_name *nmbname, bool self_only);
bool wins_store_changed_namerec(const struct name_record *namerec);
bool add_name_to_wins_subnet(const struct name_record *namerec);
bool remove_name_from_wins_namelist(struct name_record *namerec);
void dump_wins_subnet_namelist(XFILE *fp);
bool packet_is_for_wins_server(struct packet_struct *packet);
bool initialise_wins(void);
void wins_process_name_refresh_request( struct subnet_record *subrec,
                                        struct packet_struct *p );
void wins_process_name_registration_request(struct subnet_record *subrec,
                                            struct packet_struct *p);
void wins_process_multihomed_name_registration_request( struct subnet_record *subrec,
                                                        struct packet_struct *p);
void fetch_all_active_wins_1b_names(void);
void send_wins_name_query_response(int rcode, struct packet_struct *p,
                                          struct name_record *namerec);
void wins_process_name_query_request(struct subnet_record *subrec,
                                     struct packet_struct *p);
void wins_process_name_release_request(struct subnet_record *subrec,
                                       struct packet_struct *p);
void initiate_wins_processing(time_t t);
void wins_write_name_record(struct name_record *namerec, XFILE *fp);
void wins_write_database(time_t t, bool background);
void nmbd_wins_new_entry(struct messaging_context *msg,
                                       void *private_data,
                                       uint32_t msg_type,
                                       struct server_id server_id,
                                       DATA_BLOB *data);

/* The following definitions come from nmbd/nmbd_workgroupdb.c  */

struct work_record *find_workgroup_on_subnet(struct subnet_record *subrec,
                                             const char *name);
struct work_record *create_workgroup_on_subnet(struct subnet_record *subrec,
                                               const char *name, int ttl);
void update_workgroup_ttl(struct work_record *work, int ttl);
void initiate_myworkgroup_startup(struct subnet_record *subrec, struct work_record *work);
void dump_workgroups(bool force_write);
void expire_workgroups_and_servers(time_t t);
