/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Blocking Locking functions
   Copyright (C) Jeremy Allison 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
extern int DEBUGLEVEL;
extern int Client;
extern char *OutBuffer;

/****************************************************************************
 This is the structure to queue to implement blocking locks.
 notify. It consists of the requesting SMB and the expiry time.
*****************************************************************************/

typedef struct {
  ubi_slNode msg_next;
  int com_type;
  files_struct *fsp;
  time_t expire_time;
  int lock_num;
  char *inbuf;
  int length;
} blocking_lock_record;

static ubi_slList blocking_lock_queue = { NULL, (ubi_slNodePtr)&blocking_lock_queue, 0};

/****************************************************************************
 Destructor for the above structure.
****************************************************************************/

static void free_blocking_lock_record(blocking_lock_record *blr)
{
  free(blr->inbuf);
  free((char *)blr);
}

/****************************************************************************
 Get the files_struct given a particular queued SMB.
*****************************************************************************/

static files_struct *get_fsp_from_pkt(char *inbuf)
{
  switch(CVAL(inbuf,smb_com)) {
  case SMBlock:
  case SMBlockread:
    return file_fsp(inbuf,smb_vwv0);
  case SMBlockingX:
    return file_fsp(inbuf,smb_vwv2);
  default:
    DEBUG(0,("get_fsp_from_pkt: PANIC - unknown type on blocking lock queue - exiting.!\n"));
    exit_server("PANIC - unknown type on blocking lock queue");
  }
  return NULL; /* Keep compiler happy. */
}

/****************************************************************************
 Determine if this is a secondary element of a chained SMB.
  **************************************************************************/

static BOOL in_chained_smb(void)
{
  return (chain_size != 0);
}

/****************************************************************************
 Function to push a blocking lock request onto the lock queue.
****************************************************************************/

BOOL push_blocking_lock_request( char *inbuf, int length, int lock_timeout, int lock_num)
{
  blocking_lock_record *blr;

  if(in_chained_smb() ) {
    DEBUG(0,("push_blocking_lock_request: cannot queue a chained request (currently).\n"));
    return False;
  }

  /*
   * Now queue an entry on the blocking lock queue. We setup
   * the expiration time here.
   */

  if((blr = (blocking_lock_record *)malloc(sizeof(blocking_lock_record))) == NULL) {
    DEBUG(0,("push_blocking_lock_request: Malloc fail !\n" ));
    return False;
  }

  if((blr->inbuf = (char *)malloc(length)) == NULL) {
    DEBUG(0,("push_blocking_lock_request: Malloc fail (2)!\n" ));
    free((char *)blr);
    return False;
  }

  blr->com_type = CVAL(inbuf,smb_com);
  blr->fsp = get_fsp_from_pkt(inbuf);
  blr->expire_time = (lock_timeout == -1) ? (time_t)-1 : time(NULL) + (time_t)lock_timeout;
  blr->lock_num = lock_num;
  memcpy(blr->inbuf, inbuf, length);
  blr->length = length;

  ubi_slAddTail(&blocking_lock_queue, blr);


  DEBUG(3,("push_blocking_lock_request: lock request length=%d blocked with expiry time %d \
for fnum = %d, name = %s\n", length, (int)blr->expire_time, 
        blr->fsp->fnum, blr->fsp->fsp_name ));

  return True;
}

/****************************************************************************
 Return a smd with a given size.
*****************************************************************************/

static void send_blocking_reply(char *outbuf, int outsize)
{
  if(outsize > 4)
    smb_setlen(outbuf,outsize - 4);

  send_smb(Client,outbuf);
}

/****************************************************************************
 Return a lockingX success SMB.
*****************************************************************************/

static void reply_lockingX_success(blocking_lock_record *blr)
{
  char *outbuf = OutBuffer;
  int bufsize = BUFFER_SIZE;
  char *inbuf = blr->inbuf;
  int outsize = 0;

  construct_reply_common(inbuf, outbuf);
  set_message(outbuf,2,0,True);

  /*
   * As this message is a lockingX call we must handle
   * any following chained message correctly.
   * This is normally handled in construct_reply(),
   * but as that calls switch_message, we can't use
   * that here and must set up the chain info manually.
   */

  outsize = chain_reply(inbuf,outbuf,blr->length,bufsize);

  outsize += chain_size;

  send_blocking_reply(outbuf,outsize);
}

/****************************************************************************
 Return a generic lock fail error blocking call.
*****************************************************************************/

static void generic_blocking_lock_error(blocking_lock_record *blr, int eclass, int32 ecode)
{
  char *outbuf = OutBuffer;
  char *inbuf = blr->inbuf;
  construct_reply_common(inbuf, outbuf);

  if(eclass == 0) /* NT Error. */
    SSVAL(outbuf,smb_flg2, SVAL(outbuf,smb_flg2) | FLAGS2_32_BIT_ERROR_CODES);

  ERROR(eclass,ecode);
  send_smb(Client,outbuf);
}

/****************************************************************************
 Return a lock fail error for a lockingX call. Undo all the locks we have 
 obtained first.
*****************************************************************************/

static void reply_lockingX_error(blocking_lock_record *blr, int eclass, int32 ecode)
{
  char *inbuf = blr->inbuf;
  files_struct *fsp = blr->fsp;
  connection_struct *conn = conn_find(SVAL(inbuf,smb_tid));
  uint16 num_ulocks = SVAL(inbuf,smb_vwv6);
  SMB_OFF_T count = (SMB_OFF_T) 0, offset = (SMB_OFF_T) 0;
  unsigned char locktype = CVAL(inbuf,smb_vwv3);
  BOOL large_file_format = (locktype & LOCKING_ANDX_LARGE_FILES);
  char *data;
  int i;

  data = smb_buf(inbuf) + ((large_file_format ? 20 : 10)*num_ulocks);

  /* 
   * Data now points at the beginning of the list
   * of smb_lkrng structs.
   */

  /*
   * Ensure we don't do a remove on the lock that just failed,
   * as under POSIX rules, if we have a lock already there, we
   * will delete it (and we shouldn't) .....
   */

  for(i = blr->lock_num - 1; i >= 0; i--) {
    int dummy1;
    uint32 dummy2;
    BOOL err;

    count = get_lock_count( data, i, large_file_format, &err);
    offset = get_lock_offset( data, i, large_file_format, &err);

    /*
     * We know err cannot be set as if it was the lock
     * request would never have been queued. JRA.
     */

    do_unlock(fsp,conn,count,offset,&dummy1,&dummy2);
  }

  generic_blocking_lock_error(blr, eclass, ecode);
}

/****************************************************************************
 Return a lock fail error.
*****************************************************************************/

static void blocking_lock_reply_error(blocking_lock_record *blr, int eclass, int32 ecode)
{
  switch(blr->com_type) {
  case SMBlock:
    generic_blocking_lock_error(blr, eclass, ecode);
    break;
  case SMBlockread:
    generic_blocking_lock_error(blr, eclass, ecode);
    break;
  case SMBlockingX:
    reply_lockingX_error(blr, eclass, ecode);
    break;
  default:
    DEBUG(0,("blocking_lock_reply_error: PANIC - unknown type on blocking lock queue - exiting.!\n"));
    exit_server("PANIC - unknown type on blocking lock queue");
  }
}

/****************************************************************************
 Attempt to finish off getting all pending blocking locks for a lockread call.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static BOOL process_lockread(blocking_lock_record *blr)
{
  char *outbuf = OutBuffer;
  char *inbuf = blr->inbuf;
  ssize_t nread = -1;
  char *data;
  int outsize = 0;
  SMB_OFF_T startpos;
  size_t numtoread;
  int eclass;
  uint32 ecode;
  connection_struct *conn = conn_find(SVAL(inbuf,smb_tid));
  files_struct *fsp = blr->fsp;

  numtoread = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
 
  numtoread = MIN(BUFFER_SIZE-outsize,numtoread);
  data = smb_buf(outbuf) + 3;
 
  if(!do_lock( fsp, conn, numtoread, startpos, F_RDLCK, &eclass, &ecode)) {
    if((errno != EACCES) && (errno != EAGAIN)) {
      /*
       * We have other than a "can't get lock" POSIX
       * error. Send an error.
       * Return True so we get dequeued.
       */

      generic_blocking_lock_error(blr, eclass, ecode);
      return True;
    }

    /*
     * Still waiting for lock....
     */

    DEBUG(10,("process_lockread: failed to get lock for file = %s. Still waiting....\n",
          fsp->fsp_name));
    return False;
  }

  nread = read_file(fsp,data,startpos,numtoread);

  if (nread < 0) {
    generic_blocking_lock_error(blr,ERRDOS,ERRnoaccess);
    return True;
  }

  construct_reply_common(inbuf, outbuf);
  outsize = set_message(outbuf,5,3,True);

  outsize += nread;
  SSVAL(outbuf,smb_vwv0,nread);
  SSVAL(outbuf,smb_vwv5,nread+3);
  SSVAL(smb_buf(outbuf),1,nread);

  DEBUG(3, ( "process_lockread file = %s, fnum=%d num=%d nread=%d\n",
        fsp->fsp_name, fsp->fnum, (int)numtoread, (int)nread ) );

  send_blocking_reply(outbuf,outsize);
  return True;
}

/****************************************************************************
 Attempt to finish off getting all pending blocking locks for a lock call.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static BOOL process_lock(blocking_lock_record *blr)
{
  char *outbuf = OutBuffer;
  char *inbuf = blr->inbuf;
  int outsize;
  SMB_OFF_T count = 0, offset = 0;
  int eclass;
  uint32 ecode;
  connection_struct *conn = conn_find(SVAL(inbuf,smb_tid));
  files_struct *fsp = blr->fsp;

  count = IVAL(inbuf,smb_vwv1);
  offset = IVAL(inbuf,smb_vwv3);

  errno = 0;
  if (!do_lock(fsp, conn, count, offset, F_WRLCK, &eclass, &ecode)) {
    if((errno != EACCES) && (errno != EAGAIN)) {

      /*
       * We have other than a "can't get lock" POSIX
       * error. Send an error.
       * Return True so we get dequeued.
       */

      blocking_lock_reply_error(blr, eclass, ecode);
      return True;
    }

    /*
     * Still can't get the lock - keep waiting.
     */

    DEBUG(10,("process_lock: failed to get lock for file = %s. Still waiting....\n",
          fsp->fsp_name));
    return False;
  }

  /*
   * Success - we got the lock.
   */

  DEBUG(3,("process_lock : file=%s fnum=%d offset=%.0f count=%.0f\n",
       fsp->fsp_name, fsp->fnum, (double)offset, (double)count));

  construct_reply_common(inbuf, outbuf);
  outsize = set_message(outbuf,0,0,True);
  send_blocking_reply(outbuf,outsize);
  return True;
}

/****************************************************************************
 Attempt to finish off getting all pending blocking locks for a lockingX call.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static BOOL process_lockingX(blocking_lock_record *blr)
{
  char *inbuf = blr->inbuf;
  unsigned char locktype = CVAL(inbuf,smb_vwv3);
  files_struct *fsp = blr->fsp;
  connection_struct *conn = conn_find(SVAL(inbuf,smb_tid));
  uint16 num_ulocks = SVAL(inbuf,smb_vwv6);
  uint16 num_locks = SVAL(inbuf,smb_vwv7);
  SMB_OFF_T count = 0, offset = 0;
  BOOL large_file_format = (locktype & LOCKING_ANDX_LARGE_FILES);
  char *data;
  int eclass=0;
  uint32 ecode=0;

  data = smb_buf(inbuf) + ((large_file_format ? 20 : 10)*num_ulocks);

  /* 
   * Data now points at the beginning of the list
   * of smb_lkrng structs.
   */

  for(; blr->lock_num < num_locks; blr->lock_num++) {
    BOOL err;

    count = get_lock_count( data, blr->lock_num, large_file_format, &err);
    offset = get_lock_offset( data, blr->lock_num, large_file_format, &err);

    /*
     * We know err cannot be set as if it was the lock
     * request would never have been queued. JRA.
     */
    errno = 0;
    if(!do_lock(fsp,conn,count,offset, ((locktype & 1) ? F_RDLCK : F_WRLCK),
                &eclass, &ecode))
      break;
  }

  if(blr->lock_num == num_locks) {

    /*
     * Success - we got all the locks.
     */

    DEBUG(3,("process_lockingX file = %s, fnum=%d type=%d num_locks=%d\n",
          fsp->fsp_name, fsp->fnum, (unsigned int)locktype, num_locks) );

    reply_lockingX_success(blr);
    return True;

  } else if((errno != EACCES) && (errno != EAGAIN)) {

    /*
     * We have other than a "can't get lock" POSIX
     * error. Free any locks we had and return an error.
     * Return True so we get dequeued.
     */

    blocking_lock_reply_error(blr, eclass, ecode);
    return True;
  }

  /*
   * Still can't get all the locks - keep waiting.
   */

  DEBUG(10,("process_lockingX: only got %d locks of %d needed for file %s, fnum = %d. \
Waiting....\n", blr->lock_num, num_locks, fsp->fsp_name, fsp->fnum));

  return False;
}

/****************************************************************************
 Process a blocking lock SMB.
 Returns True if we want to be removed from the list.
*****************************************************************************/

static BOOL blocking_lock_record_process(blocking_lock_record *blr)
{
  switch(blr->com_type) {
  case SMBlock:
    return process_lock(blr);
  case SMBlockread:
    return process_lockread(blr);
  case SMBlockingX:
    return process_lockingX(blr);
  default:
    DEBUG(0,("blocking_lock_record_process: PANIC - unknown type on blocking lock queue - exiting.!\n"));
    exit_server("PANIC - unknown type on blocking lock queue");
  }
  return False; /* Keep compiler happy. */
}

/****************************************************************************
 Delete entries by fnum from the blocking lock pending queue.
*****************************************************************************/

void remove_pending_lock_requests_by_fid(files_struct *fsp)
{
  blocking_lock_record *blr = (blocking_lock_record *)ubi_slFirst( &blocking_lock_queue );
  blocking_lock_record *prev = NULL;

  while(blr != NULL) {
    if(blr->fsp->fnum == fsp->fnum) {

      DEBUG(10,("remove_pending_lock_requests_by_fid - removing request type %d for \
file %s fnum = %d\n", blr->com_type, fsp->fsp_name, fsp->fnum ));

      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      continue;
    }

    prev = blr;
    blr = (blocking_lock_record *)ubi_slNext(blr);
  }
}

/****************************************************************************
 Delete entries by mid from the blocking lock pending queue. Always send reply.
*****************************************************************************/

void remove_pending_lock_requests_by_mid(int mid)
{
  blocking_lock_record *blr = (blocking_lock_record *)ubi_slFirst( &blocking_lock_queue );
  blocking_lock_record *prev = NULL;

  while(blr != NULL) {
    if(SVAL(blr->inbuf,smb_mid) == mid) {
      files_struct *fsp = blr->fsp;

      DEBUG(10,("remove_pending_lock_requests_by_mid - removing request type %d for \
file %s fnum = %d\n", blr->com_type, fsp->fsp_name, fsp->fnum ));

      blocking_lock_reply_error(blr,0,NT_STATUS_CANCELLED);
      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      continue;
    }

    prev = blr;
    blr = (blocking_lock_record *)ubi_slNext(blr);
  }
}

/****************************************************************************
 Return True if the blocking lock queue has entries.
*****************************************************************************/

BOOL blocking_locks_pending(void)
{
  blocking_lock_record *blr = (blocking_lock_record *)ubi_slFirst( &blocking_lock_queue );
  return (blr == NULL ? False : True);
}

/****************************************************************************
 Process the blocking lock queue. Note that this is only called as root.
*****************************************************************************/

void process_blocking_lock_queue(time_t t)
{
  blocking_lock_record *blr = (blocking_lock_record *)ubi_slFirst( &blocking_lock_queue );
  blocking_lock_record *prev = NULL;

  if(blr == NULL)
    return;

  /*
   * Go through the queue and see if we can get any of the locks.
   */

  while(blr != NULL) {
    connection_struct *conn = NULL;
    uint16 vuid;
    files_struct *fsp = NULL;

    /*
     * Ensure we don't have any old chain_fsp values
     * sitting around....
     */
    chain_size = 0;
    file_chain_reset();
    fsp = blr->fsp;

    conn = conn_find(SVAL(blr->inbuf,smb_tid));
    vuid = (lp_security() == SEC_SHARE) ? UID_FIELD_INVALID :
                  SVAL(blr->inbuf,smb_uid);

    DEBUG(5,("process_blocking_lock_queue: examining pending lock fnum = %d for file %s\n",
          fsp->fnum, fsp->fsp_name ));

    if((blr->expire_time != -1) && (blr->expire_time > t)) {
      /*
       * Lock expired - throw away all previously
       * obtained locks and return lock error.
       */
      DEBUG(5,("process_blocking_lock_queue: pending lock fnum = %d for file %s timed out.\n",
          fsp->fnum, fsp->fsp_name ));

      blocking_lock_reply_error(blr,ERRSRV,ERRaccess);
      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      continue;
    }

    if(!become_user(conn,vuid)) {
      DEBUG(0,("process_blocking_lock_queue: Unable to become user vuid=%d.\n",
            vuid ));
      /*
       * Remove the entry and return an error to the client.
       */
      blocking_lock_reply_error(blr,ERRSRV,ERRaccess);
      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      continue;
    }

    if(!become_service(conn,True)) {
      DEBUG(0,("process_blocking_lock_queue: Unable to become service Error was %s.\n", strerror(errno) ));
      /*
       * Remove the entry and return an error to the client.
       */
      blocking_lock_reply_error(blr,ERRSRV,ERRaccess);
      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      unbecome_user();
      continue;
    }

    /*
     * Go through the remaining locks and try and obtain them.
     * The call returns True if all locks were obtained successfully
     * and False if we still need to wait.
     */

    if(blocking_lock_record_process(blr)) {
      free_blocking_lock_record((blocking_lock_record *)ubi_slRemNext( &blocking_lock_queue, prev));
      blr = (blocking_lock_record *)(prev ? ubi_slNext(prev) : ubi_slFirst(&blocking_lock_queue));
      unbecome_user();
      continue;
    }

    unbecome_user();

    /*
     * Move to the next in the list.
     */
    prev = blr;
    blr = (blocking_lock_record *)ubi_slNext(blr);
  }
}
