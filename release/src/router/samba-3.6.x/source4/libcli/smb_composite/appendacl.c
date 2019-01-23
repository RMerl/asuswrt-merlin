#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/composite/composite.h"
#include "libcli/security/security.h"
#include "libcli/smb_composite/smb_composite.h"

/* the stages of this call */
enum appendacl_stage {APPENDACL_OPENPATH, APPENDACL_GET, 
		       APPENDACL_SET, APPENDACL_GETAGAIN, APPENDACL_CLOSEPATH};

static void appendacl_handler(struct smbcli_request *req);

struct appendacl_state {
	enum appendacl_stage stage;
	struct smb_composite_appendacl *io;

	union smb_open *io_open;
	union smb_setfileinfo *io_setfileinfo;
	union smb_fileinfo *io_fileinfo;

	struct smbcli_request *req;
};


static NTSTATUS appendacl_open(struct composite_context *c, 
			      struct smb_composite_appendacl *io)
{
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;

	status = smb_raw_open_recv(state->req, c, state->io_open);
	NT_STATUS_NOT_OK_RETURN(status);

	/* setup structures for getting fileinfo */
	state->io_fileinfo = talloc(c, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->io_fileinfo);
	
	state->io_fileinfo->query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	state->io_fileinfo->query_secdesc.in.file.fnum = state->io_open->ntcreatex.out.file.fnum;
	state->io_fileinfo->query_secdesc.in.secinfo_flags = SECINFO_DACL;

	state->req = smb_raw_fileinfo_send(tree, state->io_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* set the handler */
	state->req->async.fn = appendacl_handler;
	state->req->async.private_data = c;
	state->stage = APPENDACL_GET;
	
	talloc_free (state->io_open);

	return NT_STATUS_OK;
}

static NTSTATUS appendacl_get(struct composite_context *c, 
			       struct smb_composite_appendacl *io)
{
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
	struct smbcli_tree *tree = state->req->tree;
	int i;
	NTSTATUS status;
	
	status = smb_raw_fileinfo_recv(state->req, state->io_fileinfo, state->io_fileinfo);
	NT_STATUS_NOT_OK_RETURN(status);
	
	/* setup structures for setting fileinfo */
	state->io_setfileinfo = talloc(c, union smb_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->io_setfileinfo);
	
	state->io_setfileinfo->set_secdesc.level            = RAW_SFILEINFO_SEC_DESC;
	state->io_setfileinfo->set_secdesc.in.file.fnum     = state->io_fileinfo->query_secdesc.in.file.fnum;
	
	state->io_setfileinfo->set_secdesc.in.secinfo_flags = SECINFO_DACL;
	state->io_setfileinfo->set_secdesc.in.sd            = state->io_fileinfo->query_secdesc.out.sd;
	talloc_steal(state->io_setfileinfo, state->io_setfileinfo->set_secdesc.in.sd);

	/* append all aces from io->in.sd->dacl to new security descriptor */
	if (io->in.sd->dacl != NULL) {
		for (i = 0; i < io->in.sd->dacl->num_aces; i++) {
			security_descriptor_dacl_add(state->io_setfileinfo->set_secdesc.in.sd,
						     &(io->in.sd->dacl->aces[i]));
		}
	}

	status = smb_raw_setfileinfo(tree, state->io_setfileinfo);
	NT_STATUS_NOT_OK_RETURN(status);

	state->req = smb_raw_setfileinfo_send(tree, state->io_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call handler when done setting new security descriptor on file */
	state->req->async.fn = appendacl_handler;
	state->req->async.private_data = c;
	state->stage = APPENDACL_SET;

	talloc_free (state->io_fileinfo);

	return NT_STATUS_OK;
}

static NTSTATUS appendacl_set(struct composite_context *c, 
			      struct smb_composite_appendacl *io)
{
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;

	status = smbcli_request_simple_recv(state->req);
	NT_STATUS_NOT_OK_RETURN(status);

	/* setup structures for getting fileinfo */
	state->io_fileinfo = talloc(c, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->io_fileinfo);
	

	state->io_fileinfo->query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	state->io_fileinfo->query_secdesc.in.file.fnum = state->io_setfileinfo->set_secdesc.in.file.fnum;
	state->io_fileinfo->query_secdesc.in.secinfo_flags = SECINFO_DACL;

	state->req = smb_raw_fileinfo_send(tree, state->io_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* set the handler */
	state->req->async.fn = appendacl_handler;
	state->req->async.private_data = c;
	state->stage = APPENDACL_GETAGAIN;
	
	talloc_free (state->io_setfileinfo);

	return NT_STATUS_OK;
}


static NTSTATUS appendacl_getagain(struct composite_context *c, 
				   struct smb_composite_appendacl *io)
{
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
	struct smbcli_tree *tree = state->req->tree;
	union smb_close *io_close;
	NTSTATUS status;
	
	status = smb_raw_fileinfo_recv(state->req, c, state->io_fileinfo);
	NT_STATUS_NOT_OK_RETURN(status);

	io->out.sd = state->io_fileinfo->query_secdesc.out.sd;

	/* setup structures for close */
	io_close = talloc(c, union smb_close);
	NT_STATUS_HAVE_NO_MEMORY(io_close);
	
	io_close->close.level = RAW_CLOSE_CLOSE;
	io_close->close.in.file.fnum = state->io_fileinfo->query_secdesc.in.file.fnum;
	io_close->close.in.write_time = 0;

	state->req = smb_raw_close_send(tree, io_close);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler */
	state->req->async.fn = appendacl_handler;
	state->req->async.private_data = c;
	state->stage = APPENDACL_CLOSEPATH;

	talloc_free (state->io_fileinfo);

	return NT_STATUS_OK;
}



static NTSTATUS appendacl_close(struct composite_context *c, 
				struct smb_composite_appendacl *io)
{
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
	NTSTATUS status;

	status = smbcli_request_simple_recv(state->req);
	NT_STATUS_NOT_OK_RETURN(status);
	
	c->state = COMPOSITE_STATE_DONE;

	return NT_STATUS_OK;
}

/*
  handler for completion of a sub-request in appendacl
*/
static void appendacl_handler(struct smbcli_request *req)
{
	struct composite_context *c = (struct composite_context *)req->async.private_data;
	struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);

	/* when this handler is called, the stage indicates what
	   call has just finished */
	switch (state->stage) {
	case APPENDACL_OPENPATH:
		c->status = appendacl_open(c, state->io);
		break;

	case APPENDACL_GET:
		c->status = appendacl_get(c, state->io);
		break;

	case APPENDACL_SET:
		c->status = appendacl_set(c, state->io);
		break;

	case APPENDACL_GETAGAIN:
		c->status = appendacl_getagain(c, state->io);
		break;

	case APPENDACL_CLOSEPATH:
		c->status = appendacl_close(c, state->io);
		break;
	}

	/* We should get here if c->state >= SMBCLI_REQUEST_DONE */
	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE &&
	    c->async.fn) {
		c->async.fn(c);
	}
}


/*
  composite appendacl call - does an open followed by a number setfileinfo,
  after that new acls are read with fileinfo, followed by a close
*/
struct composite_context *smb_composite_appendacl_send(struct smbcli_tree *tree, 
							struct smb_composite_appendacl *io)
{
	struct composite_context *c;
	struct appendacl_state *state;

	c = talloc_zero(tree, struct composite_context);
	if (c == NULL) goto failed;

	state = talloc(c, struct appendacl_state);
	if (state == NULL) goto failed;

	state->io = io;

	c->private_data = state;
	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx = tree->session->transport->socket->event.ctx;

	/* setup structures for opening file */
	state->io_open = talloc_zero(c, union smb_open);
	if (state->io_open == NULL) goto failed;
	
	state->io_open->ntcreatex.level               = RAW_OPEN_NTCREATEX;
	state->io_open->ntcreatex.in.root_fid.fnum    = 0;
	state->io_open->ntcreatex.in.flags            = 0;
	state->io_open->ntcreatex.in.access_mask      = SEC_FLAG_MAXIMUM_ALLOWED;
	state->io_open->ntcreatex.in.file_attr        = FILE_ATTRIBUTE_NORMAL;
	state->io_open->ntcreatex.in.share_access     = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	state->io_open->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	state->io_open->ntcreatex.in.impersonation    = NTCREATEX_IMPERSONATION_ANONYMOUS;
	state->io_open->ntcreatex.in.security_flags   = 0;
	state->io_open->ntcreatex.in.fname            = io->in.fname;

	/* send the open on its way */
	state->req = smb_raw_open_send(tree, state->io_open);
	if (state->req == NULL) goto failed;

	/* setup the callback handler */
	state->req->async.fn = appendacl_handler;
	state->req->async.private_data = c;
	state->stage = APPENDACL_OPENPATH;

	return c;

failed:
	talloc_free(c);
	return NULL;
}


/*
  composite appendacl call - recv side
*/
NTSTATUS smb_composite_appendacl_recv(struct composite_context *c, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct appendacl_state *state = talloc_get_type(c->private_data, struct appendacl_state);
		state->io->out.sd = security_descriptor_copy (mem_ctx, state->io->out.sd);
	}

	talloc_free(c);
	return status;
}


/*
  composite appendacl call - sync interface
*/
NTSTATUS smb_composite_appendacl(struct smbcli_tree *tree, 
				TALLOC_CTX *mem_ctx,
				struct smb_composite_appendacl *io)
{
	struct composite_context *c = smb_composite_appendacl_send(tree, io);
	return smb_composite_appendacl_recv(c, mem_ctx);
}

