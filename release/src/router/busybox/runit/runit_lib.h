/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

//extern unsigned byte_chr(char *s,unsigned n,int c);
//
//struct tai {
//	uint64_t x;
//};
//
//#define tai_unix(t,u) ((void) ((t)->x = 0x400000000000000aULL + (uint64_t) (u)))
//
//#define TAI_PACK 8
//extern void tai_unpack(const char *,struct tai *);
//
//extern void tai_uint(struct tai *,unsigned);
//
//struct taia {
//	struct tai sec;
//	unsigned long nano; /* 0...999999999 */
//	unsigned long atto; /* 0...999999999 */
//};
//
//extern void taia_now(struct taia *);
//
//extern void taia_add(struct taia *,const struct taia *,const struct taia *);
//extern void taia_addsec(struct taia *,const struct taia *,int);
//extern void taia_sub(struct taia *,const struct taia *,const struct taia *);
//extern void taia_half(struct taia *,const struct taia *);
//extern int taia_less(const struct taia *,const struct taia *);
//
//#define TAIA_PACK 16
//extern void taia_pack(char *,const struct taia *);
//
//extern void taia_uint(struct taia *,unsigned);
//
//typedef struct pollfd iopause_fd;
//#define IOPAUSE_READ POLLIN
//#define IOPAUSE_WRITE POLLOUT
//
//extern void iopause(iopause_fd *,unsigned,struct taia *,struct taia *);

extern int lock_ex(int);
extern int lock_un(int);
extern int lock_exnb(int);

extern int open_read(const char *);
extern int open_excl(const char *);
extern int open_append(const char *);
extern int open_trunc(const char *);
extern int open_write(const char *);

extern unsigned pmatch(const char *, const char *, unsigned);

#define str_diff(s,t) strcmp((s), (t))
#define str_equal(s,t) (!strcmp((s), (t)))

/*
 * runsv / supervise / sv stuff
 */
typedef struct svstatus_t {
	uint64_t time_be64 PACKED;
	uint32_t time_nsec_be32 PACKED;
	uint32_t pid_le32 PACKED;
	uint8_t  paused;
	uint8_t  want; /* 'u' or 'd' */
	uint8_t  got_term;
	uint8_t  run_or_finish;
} svstatus_t;
struct ERR_svstatus_must_be_20_bytes {
	char ERR_svstatus_must_be_20_bytes[sizeof(svstatus_t) == 20 ? 1 : -1];
};

POP_SAVED_FUNCTION_VISIBILITY
