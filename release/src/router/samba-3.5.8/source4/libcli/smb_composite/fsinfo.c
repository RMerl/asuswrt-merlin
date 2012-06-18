/*
  a composite API for quering file system information
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/resolve/resolve.h"

/* the stages of this call */
enum fsinfo_stage {FSINFO_CONNECT, FSINFO_QUERY};


static void fsinfo_raw_handler(struct smbcli_request *req);
static void fsinfo_composite_handler(struct composite_context *c);
static void fsinfo_state_handler(struct composite_context *c);

struct fsinfo_state {
	enum fsinfo_stage stage;
	struct composite_context *creq;
	struct smb_composite_fsinfo *io;
	struct smb_composite_connect *connect;
	union smb_fsinfo *fsinfo;
	struct smbcli_tree *tree;
	struct smbcli_request *req;
};

static NTSTATUS fsinfo_connect(struct composite_context *c,
			       struct smb_composite_fsinfo *io)
{
	NTSTATUS status;
	struct fsinfo_state *state;
	state = talloc_get_type(c->private_data, struct fsinfo_state);

	status = smb_composite_connect_recv(state->creq, c);
	NT_STATUS_NOT_OK_RETURN(status);

	state->fsinfo = talloc(state, union smb_fsinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->fsinfo);

	state->fsinfo->generic.level = io->in.level;

	state->req = smb_raw_fsinfo_send(state->connect->out.tree,
					 state,
					 state->fsinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	state->req->async.private_data = c;
	state->req->async.fn = fsinfo_raw_handler;

	state->stage = FSINFO_QUERY;

	return NT_STATUS_OK;
}

static NTSTATUS fsinfo_query(struct composite_context *c,
			       struct smb_composite_fsinfo *io)
{
	NTSTATUS status;
	struct fsinfo_state *state;
	state = talloc_get_type(c->private_data, struct fsinfo_state);

	status = smb_raw_fsinfo_recv(state->req, state, state->fsinfo);
	NT_STATUS_NOT_OK_RETURN(status);

	state->io->out.fsinfo = state->fsinfo;

	c->state = COMPOSITE_STATE_DONE;

	if (c->async.fn)
		c->async.fn(c);

	return NT_STATUS_OK;

}

/*
  handler for completion of a sub-request in fsinfo
*/
static void fsinfo_state_handler(struct composite_context *creq)
{
	struct fsinfo_state *state = talloc_get_type(creq->private_data, struct fsinfo_state);

	/* when this handler is called, the stage indicates what
	   call has just finished */
	switch (state->stage) {
	case FSINFO_CONNECT:
		creq->status = fsinfo_connect(creq, state->io);
		break;

	case FSINFO_QUERY:
		creq->status = fsinfo_query(creq, state->io);
		break;
	}

	if (!NT_STATUS_IS_OK(creq->status)) {
		creq->state = COMPOSITE_STATE_ERROR;
	}

	if (creq->state >= COMPOSITE_STATE_DONE && creq->async.fn) {
		creq->async.fn(creq);
	}
}

/* 
   As raw and composite handlers take different requests, we need to handlers
   to adapt both for the same state machine in fsinfo_state_handler()
*/
static void fsinfo_raw_handler(struct smbcli_request *req)
{
	struct composite_context *c = talloc_get_type(req->async.private_data,
						      struct composite_context);
	fsinfo_state_handler(c);
}

static void fsinfo_composite_handler(struct composite_context *creq)
{
	struct composite_context *c = talloc_get_type(creq->async.private_data, 
						      struct composite_context);
	fsinfo_state_handler(c);
}

/*
  composite fsinfo call - connects to a tree and queries a file system information
*/
struct composite_context *smb_composite_fsinfo_send(struct smbcli_tree *tree, 
						    struct smb_composite_fsinfo *io,
						    struct resolve_context *resolve_ctx)
{
	struct composite_context *c;
	struct fsinfo_state *state;

	c = talloc_zero(tree, struct composite_context);
	if (c == NULL) goto failed;

	state = talloc(c, struct fsinfo_state);
	if (state == NULL) goto failed;

	state->io = io;

	state->connect = talloc(state, struct smb_composite_connect);

	if (state->connect == NULL) goto failed;

	state->connect->in.dest_host    = io->in.dest_host;
	state->connect->in.dest_ports   = io->in.dest_ports;
	state->connect->in.socket_options = io->in.socket_options;
	state->connect->in.called_name  = io->in.called_name;
	state->connect->in.service      = io->in.service;
	state->connect->in.service_type = io->in.service_type;
	state->connect->in.credentials  = io->in.credentials;
	state->connect->in.fallback_to_anonymous = false;
	state->connect->in.workgroup    = io->in.workgroup;
	state->connect->in.iconv_convenience = io->in.iconv_convenience;
	state->connect->in.gensec_settings = io->in.gensec_settings;

	state->connect->in.options = tree->session->transport->options;
	state->connect->in.session_options = tree->session->options;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	state->stage = FSINFO_CONNECT;
	c->private_data = state;

	state->creq = smb_composite_connect_send(state->connect, state,
			 resolve_ctx, c->event_ctx);

	if (state->creq == NULL) goto failed;
  
	state->creq->async.private_data = c;
	state->creq->async.fn = fsinfo_composite_handler;
  
	return c;
failed:
	talloc_free(c);
	return NULL;
}

/*
  composite fsinfo call - recv side
*/
NTSTATUS smb_composite_fsinfo_recv(struct composite_context *c, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct fsinfo_state *state = talloc_get_type(c->private_data, struct fsinfo_state);
		talloc_steal(mem_ctx, state->io->out.fsinfo);
	}

	talloc_free(c);
	return status;
}


/*
  composite fsinfo call - sync interface
*/
NTSTATUS smb_composite_fsinfo(struct smbcli_tree *tree, 
			      TALLOC_CTX *mem_ctx,
			      struct smb_composite_fsinfo *io,
			      struct resolve_context *resolve_ctx)
{
	struct composite_context *c = smb_composite_fsinfo_send(tree, io, resolve_ctx);
	return smb_composite_fsinfo_recv(c, mem_ctx);
}

