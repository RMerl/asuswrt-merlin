/* pptp_ctrl.h ... handle PPTP control connection.
 *                 C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: pptp_ctrl.h,v 1.5 2004/11/09 01:42:32 quozl Exp $
 */

#ifndef INC_PPTP_CTRL_H
#define INC_PPTP_CTRL_H
#include <sys/types.h>

typedef struct PPTP_CONN PPTP_CONN;
typedef struct PPTP_CALL PPTP_CALL;

enum call_state { CALL_OPEN_RQST,  CALL_OPEN_DONE, CALL_OPEN_FAIL,
		  CALL_CLOSE_RQST, CALL_CLOSE_DONE };
enum conn_state { CONN_OPEN_RQST,  CONN_OPEN_DONE, CONN_OPEN_FAIL,
		  CONN_CLOSE_RQST, CONN_CLOSE_DONE };

typedef void (*pptp_call_cb)(PPTP_CONN*, PPTP_CALL*, enum call_state);
typedef void (*pptp_conn_cb)(PPTP_CONN*, enum conn_state);

/* if 'isclient' is true, then will send 'conn open' packet to other host.
 * not necessary if this is being opened by a server process after
 * receiving a conn_open packet from client.
 */
PPTP_CONN * pptp_conn_open(int inet_sock, int isclient,
			   pptp_conn_cb callback);
PPTP_CALL * pptp_call_open(PPTP_CONN * conn, int call_id,
			   pptp_call_cb callback, char *phonenr,int window);
int pptp_conn_established(PPTP_CONN * conn);
int pptp_conn_dead(PPTP_CONN *conn);
/* soft close.  Will callback on completion. */
void pptp_call_close(PPTP_CONN * conn, PPTP_CALL * call);
/* hard close. */
void pptp_call_destroy(PPTP_CONN *conn, PPTP_CALL *call);
/* soft close.  Will callback on completion. */
void pptp_conn_close(PPTP_CONN * conn, u_int8_t close_reason);
/* hard close */
void pptp_conn_destroy(PPTP_CONN * conn);
void pptp_conn_free(PPTP_CONN * conn);

/* Add file descriptors used by pptp to fd_set. */
void pptp_fd_set(PPTP_CONN * conn, fd_set * read_set, fd_set * write_set, int *max_fd);
/* handle any pptp file descriptors set in fd_set, and clear them */
int pptp_dispatch(PPTP_CONN * conn, fd_set * read_set, fd_set * write_set);

/* Get info about connection, call */
void pptp_call_get_ids(PPTP_CONN * conn, PPTP_CALL * call,
		       u_int16_t * call_id, u_int16_t * peer_call_id);
/* Arbitrary user data about this call/connection.
 * It is the caller's responsibility to free this data before calling
 * pptp_call|conn_close()
 */
void * pptp_conn_closure_get(PPTP_CONN * conn);
void   pptp_conn_closure_put(PPTP_CONN * conn, void *cl);
void * pptp_call_closure_get(PPTP_CONN * conn, PPTP_CALL * call);
void   pptp_call_closure_put(PPTP_CONN * conn, PPTP_CALL * call, void *cl);

#endif /* INC_PPTP_CTRL_H */
