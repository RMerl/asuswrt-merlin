#include "base.h"
#include "connections.h"

const char *connection_get_state(connection_state_t state) {
	switch (state) {
	case CON_STATE_CONNECT: return "connect";
	case CON_STATE_READ: return "read";
	case CON_STATE_READ_POST: return "readpost";
	case CON_STATE_WRITE: return "write";
	case CON_STATE_CLOSE: return "close";
	case CON_STATE_ERROR: return "error";
	case CON_STATE_HANDLE_REQUEST: return "handle-req";
	case CON_STATE_REQUEST_START: return "req-start";
	case CON_STATE_REQUEST_END: return "req-end";
	case CON_STATE_RESPONSE_START: return "resp-start";
	case CON_STATE_RESPONSE_END: return "resp-end";
	default: return "(unknown)";
	}
}

const char *connection_get_short_state(connection_state_t state) {
	switch (state) {
	case CON_STATE_CONNECT: return ".";
	case CON_STATE_READ: return "r";
	case CON_STATE_READ_POST: return "R";
	case CON_STATE_WRITE: return "W";
	case CON_STATE_CLOSE: return "C";
	case CON_STATE_ERROR: return "E";
	case CON_STATE_HANDLE_REQUEST: return "h";
	case CON_STATE_REQUEST_START: return "q";
	case CON_STATE_REQUEST_END: return "Q";
	case CON_STATE_RESPONSE_START: return "s";
	case CON_STATE_RESPONSE_END: return "S";
	default: return "x";
	}
}

int connection_set_state(server *srv, connection *con, connection_state_t state) {
	UNUSED(srv);

	con->state = state;

	return 0;
}

