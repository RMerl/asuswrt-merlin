/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the tsocket
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TSOCKET_H
#define _TSOCKET_H

#include <talloc.h>
#include <tevent.h>

struct tsocket_address;
struct tdgram_context;
struct tstream_context;
struct iovec;

/**
 * @mainpage
 *
 * The tsocket abstraction is an API ...
 */

/**
 * @defgroup tsocket The tsocket API
 *
 * The tsocket abstraction is split into two different kinds of
 * communication interfaces.
 *
 * There's the "tstream_context" interface with abstracts the communication
 * through a bidirectional byte stream between two endpoints.
 *
 * And there's the "tdgram_context" interface with abstracts datagram based
 * communication between any number of endpoints.
 *
 * Both interfaces share the "tsocket_address" abstraction for endpoint
 * addresses.
 *
 * The whole library is based on the talloc(3) and 'tevent' libraries and
 * provides "tevent_req" based "foo_send()"/"foo_recv()" functions pairs for
 * all abstracted methods that need to be async.
 *
 * @section vsock Virtual Sockets
 *
 * The abstracted layout of tdgram_context and tstream_context allow
 * implementations around virtual sockets for encrypted tunnels (like TLS,
 * SASL or GSSAPI) or named pipes over smb.
 *
 * @section npa Named Pipe Auth (NPA) Sockets
 *
 * Samba has an implementation to abstract named pipes over smb (within the
 * server side). See libcli/named_pipe_auth/npa_tstream.[ch] for the core code.
 * The current callers are located in source4/ntvfs/ipc/vfs_ipc.c and
 * source4/rpc_server/service_rpc.c for the users.
 */

/**
 * @defgroup tsocket_address The tsocket_address abstraction
 * @ingroup tsocket
 *
 * The tsocket_address represents an socket endpoint genericly.
 * As it's like an abstract class it has no specific constructor.
 * The specific constructors are descripted in later sections.
 *
 * @{
 */

/**
 * @brief Get a string representation of the endpoint.
 *
 * This function creates a string representation of the endpoint for debugging.
 * The output will look as followed:
 *      prefix:address:port
 *
 * e.g.
 *      ipv4:192.168.1.1:143
 *
 * Callers should not try to parse the string! The should use additional methods
 * of the specific tsocket_address implemention to get more details.
 *
 * @param[in]  addr     The address to convert.
 *
 * @param[in]  mem_ctx  The talloc memory context to allocate the memory.
 *
 * @return              The address as a string representation, NULL on error.
 *
 * @see tsocket_address_is_inet()
 * @see tsocket_address_inet_addr_string()
 * @see tsocket_address_inet_port()
 */
char *tsocket_address_string(const struct tsocket_address *addr,
			     TALLOC_CTX *mem_ctx);

#ifdef DOXYGEN
/**
 * @brief This creates a copy of a tsocket_address.
 *
 * This is useful when before doing modifications to a socket via additional
 * methods of the specific tsocket_address implementation.
 *
 * @param[in]  addr     The address to create the copy from.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @return              A newly allocated copy of addr (tsocket_address *), NULL
 *                      on error.
 */
struct tsocket_address *tsocket_address_copy(const struct tsocket_address *addr,
		TALLOC_CTX *mem_ctx);
#else
struct tsocket_address *_tsocket_address_copy(const struct tsocket_address *addr,
					      TALLOC_CTX *mem_ctx,
					      const char *location);

#define tsocket_address_copy(addr, mem_ctx) \
	_tsocket_address_copy(addr, mem_ctx, __location__)
#endif

/**
 * @}
 */

/**
 * @defgroup tdgram_context The tdgram_context abstraction
 * @ingroup tsocket
 *
 * The tdgram_context is like an abstract class for datagram based sockets. The
 * interface provides async 'tevent_req' based functions on top functionality
 * is similar to the recvfrom(2)/sendto(2)/close(2) syscalls.
 *
 * @note You can always use talloc_free(tdgram) to cleanup the resources
 * of the tdgram_context on a fatal error.
 * @{
 */

/**
 * @brief Ask for next available datagram on the abstracted tdgram_context.
 *
 * It returns a 'tevent_req' handle, where the caller can register
 * a callback with tevent_req_set_callback(). The callback is triggered
 * when a datagram is available or an error happened.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  dgram    The dgram context to work on.
 *
 * @return              Returns a 'tevent_req' handle, where the caller can
 *                      register a callback with tevent_req_set_callback().
 *                      NULL on fatal error.
 *
 * @see tdgram_inet_udp_socket()
 * @see tdgram_unix_socket()
 */
struct tevent_req *tdgram_recvfrom_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct tdgram_context *dgram);

/**
 * @brief Receive the next available datagram on the abstracted tdgram_context.
 *
 * This function should be called by the callback when a datagram is available
 * or an error happened.
 *
 * The caller can only have one outstanding tdgram_recvfrom_send() at a time
 * otherwise the caller will get '*perrno = EBUSY'.
 *
 * @param[in]  req      The tevent request from tdgram_recvfrom_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[out] buf      This will hold the buffer of the datagram.
 *
 * @param[out] src      The abstracted tsocket_address of the sender of the
 *                      received datagram.
 *
 * @return              The length of the datagram (0 is never returned!),
 *                      -1 on error with perrno set to the actual errno.
 *
 * @see tdgram_recvfrom_send()
 */
ssize_t tdgram_recvfrom_recv(struct tevent_req *req,
			     int *perrno,
			     TALLOC_CTX *mem_ctx,
			     uint8_t **buf,
			     struct tsocket_address **src);

/**
 * @brief Send a datagram to a destination endpoint.
 *
 * The function can be called to send a datagram (specified by a buf/len) to a
 * destination endpoint (specified by dst). It's not allowed for len to be 0.
 *
 * It returns a 'tevent_req' handle, where the caller can register a callback
 * with tevent_req_set_callback(). The callback is triggered when the specific
 * implementation (assumes it) has delivered the datagram to the "wire".
 *
 * The callback is then supposed to get the result by calling
 * tdgram_sendto_recv() on the 'tevent_req'.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  dgram    The dgram context to work on.
 *
 * @param[in]  buf      The buffer to send.
 *
 * @param[in]  len      The length of the buffer to send. It has to be bigger
 *                      than 0.
 *
 * @param[in]  dst      The destination to send the datagram to in form of a
 *                      tsocket_address.
 *
 * @return              Returns a 'tevent_req' handle, where the caller can
 *                      register a callback with tevent_req_set_callback().
 *                      NULL on fatal error.
 *
 * @see tdgram_inet_udp_socket()
 * @see tdgram_unix_socket()
 * @see tdgram_sendto_recv()
 */
struct tevent_req *tdgram_sendto_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tdgram_context *dgram,
				      const uint8_t *buf, size_t len,
				      const struct tsocket_address *dst);

/**
 * @brief Receive the result of the sent datagram.
 *
 * The caller can only have one outstanding tdgram_sendto_send() at a time
 * otherwise the caller will get '*perrno = EBUSY'.
 *
 * @param[in]  req      The tevent request from tdgram_sendto_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @return              The length of the datagram (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 *
 * @see tdgram_sendto_send()
 */
ssize_t tdgram_sendto_recv(struct tevent_req *req,
			   int *perrno);

/**
 * @brief Shutdown/close an abstracted socket.
 *
 * It returns a 'tevent_req' handle, where the caller can register a callback
 * with tevent_req_set_callback(). The callback is triggered when the specific
 * implementation (assumes it) has delivered the datagram to the "wire".
 *
 * The callback is then supposed to get the result by calling
 * tdgram_sendto_recv() on the 'tevent_req'.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  dgram    The dgram context diconnect from.
 *
 * @return              Returns a 'tevent_req' handle, where the caller can
 *                      register a callback with tevent_req_set_callback().
 *                      NULL on fatal error.
 *
 * @see tdgram_disconnect_recv()
 */
struct tevent_req *tdgram_disconnect_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct tdgram_context *dgram);

/**
 * @brief Receive the result from a tdgram_disconnect_send() request.
 *
 * The caller should make sure there're no outstanding tdgram_recvfrom_send()
 * and tdgram_sendto_send() calls otherwise the caller will get
 * '*perrno = EBUSY'.
 *
 * @param[in]  req      The tevent request from tdgram_disconnect_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @return              The length of the datagram (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 *
 * @see tdgram_disconnect_send()
 */
int tdgram_disconnect_recv(struct tevent_req *req,
			   int *perrno);

/**
 * @}
 */

/**
 * @defgroup tstream_context The tstream_context abstraction
 * @ingroup tsocket
 *
 * The tstream_context is like an abstract class for stream based sockets. The
 * interface provides async 'tevent_req' based functions on top functionality
 * is similar to the readv(2)/writev(2)/close(2) syscalls.
 *
 * @note You can always use talloc_free(tstream) to cleanup the resources
 * of the tstream_context on a fatal error.
 *
 * @{
 */

/**
 * @brief Report the number of bytes received but not consumed yet.
 *
 * The tstream_pending_bytes() function reports how much bytes of the incoming
 * stream have been received but not consumed yet.
 *
 * @param[in]  stream   The tstream_context to check for pending bytes.
 *
 * @return              The number of bytes received, -1 on error with errno
 *                      set.
 */
ssize_t tstream_pending_bytes(struct tstream_context *stream);

/**
 * @brief Read a specific amount of bytes from a stream socket.
 *
 * The function can be called to read for a specific amount of bytes from the
 * stream into given buffers. The caller has to preallocate the buffers.
 *
 * The caller might need to use tstream_pending_bytes() if the protocol doesn't
 * have a fixed pdu header containing the pdu size.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  stream   The tstream context to work on.
 *
 * @param[out] vector   A preallocated iovec to store the data to read.
 *
 * @param[in]  count    The number of buffers in the vector allocated.
 *
 * @return              A 'tevent_req' handle, where the caller can register
 *                      a callback with tevent_req_set_callback(). NULL on
 *                      fatal error.
 *
 * @see tstream_unix_connect_send()
 * @see tstream_inet_tcp_connect_send()
 */
struct tevent_req *tstream_readv_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct tstream_context *stream,
				      struct iovec *vector,
				      size_t count);

/**
 * @brief Get the result of a tstream_readv_send().
 *
 * The caller can only have one outstanding tstream_readv_send()
 * at a time otherwise the caller will get *perrno = EBUSY.
 *
 * @param[in]  req      The tevent request from tstream_readv_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @return              The length of the stream (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 */
int tstream_readv_recv(struct tevent_req *req,
		       int *perrno);

/**
 * @brief Write buffers from a vector into a stream socket.
 *
 * The function can be called to write buffers from a given vector
 * to a stream socket.
 *
 * You have to ensure that the vector is not empty.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  stream   The tstream context to work on.
 *
 * @param[in]  vector   The iovec vector with data to write on a stream socket.
 *
 * @param[in]  count    The number of buffers in the vector to write.
 *
 * @return              A 'tevent_req' handle, where the caller can register
 *                      a callback with tevent_req_set_callback(). NULL on
 *                      fatal error.
 */
struct tevent_req *tstream_writev_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct tstream_context *stream,
				       const struct iovec *vector,
				       size_t count);

/**
 * @brief Get the result of a tstream_writev_send().
 *
 * The caller can only have one outstanding tstream_writev_send()
 * at a time otherwise the caller will get *perrno = EBUSY.
 *
 * @param[in]  req      The tevent request from tstream_writev_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @return              The length of the stream (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 */
int tstream_writev_recv(struct tevent_req *req,
			int *perrno);

/**
 * @brief Shutdown/close an abstracted socket.
 *
 * It returns a 'tevent_req' handle, where the caller can register a callback
 * with tevent_req_set_callback(). The callback is triggered when the specific
 * implementation (assumes it) has delivered the stream to the "wire".
 *
 * The callback is then supposed to get the result by calling
 * tdgram_sendto_recv() on the 'tevent_req'.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  stream   The tstream context to work on.
 *
 * @return              A 'tevent_req' handle, where the caller can register
 *                      a callback with tevent_req_set_callback(). NULL on
 *                      fatal error.
 */
struct tevent_req *tstream_disconnect_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct tstream_context *stream);

/**
 * @brief Get the result of a tstream_disconnect_send().
 *
 * The caller can only have one outstanding tstream_writev_send()
 * at a time otherwise the caller will get *perrno = EBUSY.
 *
 * @param[in]  req      The tevent request from tstream_disconnect_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @return              The length of the stream (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 */
int tstream_disconnect_recv(struct tevent_req *req,
			    int *perrno);

/**
 * @}
 */


/**
 * @defgroup tsocket_bsd  tsocket_bsd - inet, inet6 and unix
 * @ingroup tsocket
 *
 * The main tsocket library comes with implentations for BSD style ipv4, ipv6
 * and unix sockets.
 *
 * @{
 */

/**
 * @brief Find out if the tsocket_address represents an ipv4 or ipv6 endpoint.
 *
 * @param[in]  addr     The tsocket_address pointer
 *
 * @param[in]  fam      The family can be can be "ipv4", "ipv6" or "ip". With
 *                      "ip" is autodetects "ipv4" or "ipv6" based on the
 *                      addr.
 *
 * @return              true if addr represents an address of the given family,
 *                      otherwise false.
 */
bool tsocket_address_is_inet(const struct tsocket_address *addr, const char *fam);

#if DOXYGEN
/**
 * @brief Create a tsocket_address for ipv4 and ipv6 endpoint addresses.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  fam      The family can be can be "ipv4", "ipv6" or "ip". With
 *                      "ip" is autodetects "ipv4" or "ipv6" based on the
 *                      addr.
 *
 * @param[in]  addr     A valid ip address string based on the selected family
 *                      (dns names are not allowed!). It's valid to pass NULL,
 *                      which gets mapped to "0.0.0.0" or "::".
 *
 * @param[in]  port     A valid port number.
 *
 * @param[out] _addr    A tsocket_address pointer to store the information.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tsocket_address_inet_from_strings(TALLOC_CTX *mem_ctx,
				      const char *fam,
				      const char *addr,
				      uint16_t port,
				      struct tsocket_address **_addr);
#else
int _tsocket_address_inet_from_strings(TALLOC_CTX *mem_ctx,
				       const char *fam,
				       const char *addr,
				       uint16_t port,
				       struct tsocket_address **_addr,
				       const char *location);

#define tsocket_address_inet_from_strings(mem_ctx, fam, addr, port, _addr) \
	_tsocket_address_inet_from_strings(mem_ctx, fam, addr, port, _addr, \
					   __location__)
#endif

/**
 * @brief Get the address of an 'inet' tsocket_address as a string.
 *
 * @param[in]  addr     The address to convert to a string.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @return              A newly allocated string of the address, NULL on error
 *                      with errno set.
 *
 * @see tsocket_address_is_inet()
 */
char *tsocket_address_inet_addr_string(const struct tsocket_address *addr,
				       TALLOC_CTX *mem_ctx);

/**
 * @brief Get the port number as an integer from an 'inet' tsocket_address.
 *
 * @param[in]  addr     The tsocket address to use.
 *
 * @return              The port number, 0 on error with errno set.
 */
uint16_t tsocket_address_inet_port(const struct tsocket_address *addr);

/**
 * @brief Set the port number of an existing 'inet' tsocket_address.
 *
 * @param[in]  addr     The existing tsocket_address to use.
 *
 * @param[in]  port     The valid port number to set.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tsocket_address_inet_set_port(struct tsocket_address *addr,
				  uint16_t port);

/**
 * @brief Find out if the tsocket_address represents an unix domain endpoint.
 *
 * @param[in]  addr     The tsocket_address pointer
 *
 * @return              true if addr represents an unix domain endpoint,
 *                      otherwise false.
 */
bool tsocket_address_is_unix(const struct tsocket_address *addr);

#ifdef DOXYGEN
/**
 * @brief Create a tsocket_address for a unix domain endpoint addresses.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  path     The filesystem path, NULL will map "".
 *
 * @param[in]  _addr    The tsocket_address pointer to store the information.
 *
 * @return              0 on success, -1 on error with errno set.
 *
 * @see tsocket_address_is_unix()
 */
int tsocket_address_unix_from_path(TALLOC_CTX *mem_ctx,
				   const char *path,
				   struct tsocket_address **_addr);
#else
int _tsocket_address_unix_from_path(TALLOC_CTX *mem_ctx,
				    const char *path,
				    struct tsocket_address **_addr,
				    const char *location);

#define tsocket_address_unix_from_path(mem_ctx, path, _addr) \
	_tsocket_address_unix_from_path(mem_ctx, path, _addr, \
					__location__)
#endif

/**
 * @brief Get the address of an 'unix' tsocket_address.
 *
 * @param[in]  addr     A valid 'unix' tsocket_address.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @return              The path of the unix domain socket, NULL on error or if
 *                      the tsocket_address doesn't represent an unix domain
 *                      endpoint path.
 */
char *tsocket_address_unix_path(const struct tsocket_address *addr,
				TALLOC_CTX *mem_ctx);

/**
 * @brief Request a syscall optimization for tdgram_recvfrom_send()
 *
 * This function is only used to reduce the amount of syscalls and
 * optimize performance. You should only use this if you know
 * what you're doing.
 *
 * The optimization is off by default.
 *
 * @param[in]  dgram    The tdgram_context of a bsd socket, if this
 *                      not a bsd socket the function does nothing.
 *
 * @param[in]  on       The boolean value to turn the optimization on and off.
 *
 * @return              The old boolean value.
 *
 * @see tdgram_recvfrom_send()
 */
bool tdgram_bsd_optimize_recvfrom(struct tdgram_context *dgram,
				  bool on);

#ifdef DOXYGEN
/**
 * @brief Create a tdgram_context for a ipv4 or ipv6 UDP communication.
 *
 * @param[in]  local    An 'inet' tsocket_address for the local endpoint.
 *
 * @param[in]  remote   An 'inet' tsocket_address for the remote endpoint or
 *                      NULL (??? to create a listener?).
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  dgram    The tdgram_context pointer to setup the udp
 *                      communication. The function will allocate the memory.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tdgram_inet_udp_socket(const struct tsocket_address *local,
			    const struct tsocket_address *remote,
			    TALLOC_CTX *mem_ctx,
			    struct tdgram_context **dgram);
#else
int _tdgram_inet_udp_socket(const struct tsocket_address *local,
			    const struct tsocket_address *remote,
			    TALLOC_CTX *mem_ctx,
			    struct tdgram_context **dgram,
			    const char *location);
#define tdgram_inet_udp_socket(local, remote, mem_ctx, dgram) \
	_tdgram_inet_udp_socket(local, remote, mem_ctx, dgram, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Create a tdgram_context for unix domain datagram communication.
 *
 * @param[in]  local    An 'unix' tsocket_address for the local endpoint.
 *
 * @param[in]  remote   An 'unix' tsocket_address for the remote endpoint or
 *                      NULL (??? to create a listener?).
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  dgram    The tdgram_context pointer to setup the udp
 *                      communication. The function will allocate the memory.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tdgram_unix_socket(const struct tsocket_address *local,
			const struct tsocket_address *remote,
			TALLOC_CTX *mem_ctx,
			struct tdgram_context **dgram);
#else
int _tdgram_unix_socket(const struct tsocket_address *local,
			const struct tsocket_address *remote,
			TALLOC_CTX *mem_ctx,
			struct tdgram_context **dgram,
			const char *location);

#define tdgram_unix_socket(local, remote, mem_ctx, dgram) \
	_tdgram_unix_socket(local, remote, mem_ctx, dgram, __location__)
#endif

/**
 * @brief Request a syscall optimization for tstream_readv_send()
 *
 * This function is only used to reduce the amount of syscalls and
 * optimize performance. You should only use this if you know
 * what you're doing.
 *
 * The optimization is off by default.
 *
 * @param[in]  stream   The tstream_context of a bsd socket, if this
 *                      not a bsd socket the function does nothing.
 *
 * @param[in]  on       The boolean value to turn the optimization on and off.
 *
 * @return              The old boolean value.
 *
 * @see tstream_readv_send()
 */
bool tstream_bsd_optimize_readv(struct tstream_context *stream,
				bool on);

/**
 * @brief Connect async to a TCP endpoint and create a tstream_context for the
 * stream based communication.
 *
 * Use this function to connenct asynchronously to a remote ipv4 or ipv6 TCP
 * endpoint and create a tstream_context for the stream based communication.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  local    An 'inet' tsocket_address for the local endpoint.
 *
 * @param[in]  remote   An 'inet' tsocket_address for the remote endpoint.
 *
 * @return              A 'tevent_req' handle, where the caller can register a
 *                      callback with tevent_req_set_callback(). NULL on a fatal
 *                      error.
 *
 * @see tstream_inet_tcp_connect_recv()
 */
struct tevent_req *tstream_inet_tcp_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct tsocket_address *local,
					const struct tsocket_address *remote);

#ifdef DOXYGEN
/**
 * @brief Receive the result from a tstream_inet_tcp_connect_send().
 *
 * @param[in]  req      The tevent request from tstream_inet_tcp_connect_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[out] stream   A tstream_context pointer to setup the tcp communication
 *                      on. This function will allocate the memory.
 *
 * @param[out] local    The real 'inet' tsocket_address of the local endpoint.
 *                      This parameter is optional and can be NULL.
 *
 * @return              0 on success, -1 on error with perrno set.
 */
int tstream_inet_tcp_connect_recv(struct tevent_req *req,
				  int *perrno,
				  TALLOC_CTX *mem_ctx,
				  struct tstream_context **stream,
				  struct tsocket_address **local)
#else
int _tstream_inet_tcp_connect_recv(struct tevent_req *req,
				   int *perrno,
				   TALLOC_CTX *mem_ctx,
				   struct tstream_context **stream,
				   struct tsocket_address **local,
				   const char *location);
#define tstream_inet_tcp_connect_recv(req, perrno, mem_ctx, stream, local) \
	_tstream_inet_tcp_connect_recv(req, perrno, mem_ctx, stream, local, \
				       __location__)
#endif

/**
 * @brief Connect async to a unix domain endpoint and create a tstream_context
 * for the stream based communication.
 *
 * Use this function to connenct asynchronously to a unix domainendpoint and
 * create a tstream_context for the stream based communication.
 *
 * The callback is triggered when a socket is connected and ready for IO or an
 * error happened.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  local    An 'unix' tsocket_address for the local endpoint.
 *
 * @param[in]  remote   An 'unix' tsocket_address for the remote endpoint.
 *
 * @return              A 'tevent_req' handle, where the caller can register a
 *                      callback with tevent_req_set_callback(). NULL on a falal
 *                      error.
 *
 * @see tstream_unix_connect_recv()
 */
struct tevent_req * tstream_unix_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct tsocket_address *local,
					const struct tsocket_address *remote);

#ifdef DOXYGEN
/**
 * @brief Receive the result from a tstream_unix_connect_send().
 *
 * @param[in]  req      The tevent request from tstream_inet_tcp_connect_send().
 *
 * @param[out] perrno   The error number, set if an error occurred.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  stream   The tstream context to work on.
 *
 * @return              0 on success, -1 on error with perrno set.
 */
int tstream_unix_connect_recv(struct tevent_req *req,
			      int *perrno,
			      TALLOC_CTX *mem_ctx,
			      struct tstream_context **stream);
#else
int _tstream_unix_connect_recv(struct tevent_req *req,
			       int *perrno,
			       TALLOC_CTX *mem_ctx,
			       struct tstream_context **stream,
			       const char *location);
#define tstream_unix_connect_recv(req, perrno, mem_ctx, stream) \
	_tstream_unix_connect_recv(req, perrno, mem_ctx, stream, \
					  __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Create two connected 'unix' tsocket_contexts for stream based
 *        communication.
 *
 * @param[in]  mem_ctx1 The talloc memory context to use for stream1.
 *
 * @param[in]  stream1  The first stream to connect.
 *
 * @param[in]  mem_ctx2 The talloc memory context to use for stream2.
 *
 * @param[in]  stream2  The second stream to connect.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tstream_unix_socketpair(TALLOC_CTX *mem_ctx1,
			    struct tstream_context **stream1,
			    TALLOC_CTX *mem_ctx2,
			    struct tstream_context **stream2);
#else
int _tstream_unix_socketpair(TALLOC_CTX *mem_ctx1,
			     struct tstream_context **_stream1,
			     TALLOC_CTX *mem_ctx2,
			     struct tstream_context **_stream2,
			     const char *location);

#define tstream_unix_socketpair(mem_ctx1, stream1, mem_ctx2, stream2) \
	_tstream_unix_socketpair(mem_ctx1, stream1, mem_ctx2, stream2, \
				 __location__)
#endif

struct sockaddr;

#ifdef DOXYGEN
/**
 * @brief Convert a tsocket address to a bsd socket address.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  sa       The sockaddr structure to convert.
 *
 * @param[in]  sa_socklen   The lenth of the sockaddr sturucte.
 *
 * @param[out] addr     The tsocket pointer to allocate and fill.
 *
 * @return              0 on success, -1 on error with errno set.
 */
int tsocket_address_bsd_from_sockaddr(TALLOC_CTX *mem_ctx,
				      struct sockaddr *sa,
				      size_t sa_socklen,
				      struct tsocket_address **addr);
#else
int _tsocket_address_bsd_from_sockaddr(TALLOC_CTX *mem_ctx,
				       struct sockaddr *sa,
				       size_t sa_socklen,
				       struct tsocket_address **_addr,
				       const char *location);

#define tsocket_address_bsd_from_sockaddr(mem_ctx, sa, sa_socklen, _addr) \
	_tsocket_address_bsd_from_sockaddr(mem_ctx, sa, sa_socklen, _addr, \
					   __location__)
#endif

/**
 * @brief Fill a bsd sockaddr structure.
 *
 * @param[in]  addr     The tsocket address structure to use.
 *
 * @param[in]  sa       The bsd sockaddr structure to fill out.
 *
 * @param[in]  sa_socklen   The length of the  bsd sockaddr structure to fill out.
 *
 * @return              The actual size of the sockaddr structure, -1 on error
 *                      with errno set. The size could differ from sa_socklen.
 *
 * @code
 *   ssize_t socklen;
 *   struct sockaddr_storage ss;
 *
 *   socklen = tsocket_address_bsd_sockaddr(taddr,
 *                    (struct sockaddr *) &ss,
 *                    sizeof(struct sockaddr_storage));
 *   if (socklen < 0) {
 *     return -1;
 *   }
 * @endcode
 */
ssize_t tsocket_address_bsd_sockaddr(const struct tsocket_address *addr,
				     struct sockaddr *sa,
				     size_t sa_socklen);

#ifdef DOXYGEN
/**
 * @brief Wrap an existing file descriptors into the tstream abstraction.
 *
 * You can use this function to wrap an existing file descriptors into the
 * tstream abstraction. After that you're not able to use this file descriptor
 * for anything else. The file descriptor will be closed when the stream gets
 * freed. If you still want to use the fd you have have to create a duplicate.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  fd       The non blocking fd to use!
 *
 * @param[out] stream   A pointer to store an allocated tstream_context.
 *
 * @return              0 on success, -1 on error.
 *
 * Example:
 * @code
 *   fd2 = dup(fd);
 *   rc = tstream_bsd_existing_socket(mem_ctx, fd2, &tstream);
 *   if (rc < 0) {
 *     stream_terminate_connection(conn, "named_pipe_accept: out of memory");
 *     return;
 *   }
 * @endcode
 *
 * @warning This is an internal function. You should read the code to fully
 *          understand it if you plan to use it.
 */
int tstream_bsd_existing_socket(TALLOC_CTX *mem_ctx,
				int fd,
				struct tstream_context **stream);
#else
int _tstream_bsd_existing_socket(TALLOC_CTX *mem_ctx,
				 int fd,
				 struct tstream_context **_stream,
				 const char *location);
#define tstream_bsd_existing_socket(mem_ctx, fd, stream) \
	_tstream_bsd_existing_socket(mem_ctx, fd, stream, \
				     __location__)
#endif

/**
 * @}
 */

/**
 * @defgroup tsocket_helper Queue and PDU helpers
 * @ingroup tsocket
 *
 * In order to make the live easier for callers which want to implement a
 * function to receive a full PDU with a single async function pair, there're
 * some helper functions.
 *
 * There're some cases where the caller wants doesn't care about the order of
 * doing IO on the abstracted sockets.
 *
 * @{
 */

/**
 * @brief Queue a dgram blob for sending through the socket.
 *
 * This function queues a blob for sending to destination through an existing
 * dgram socket. The async callback is triggered when the whole blob is
 * delivered to the underlying system socket.
 *
 * The caller needs to make sure that all non-scalar input parameters hang
 * around for the whole lifetime of the request.
 *
 * @param[in]  mem_ctx  The memory context for the result.
 *
 * @param[in]  ev       The event context the operation should work on.
 *
 * @param[in]  dgram    The tdgram_context to send the message buffer.
 *
 * @param[in]  queue    The existing dgram queue.
 *
 * @param[in]  buf      The message buffer to send.
 *
 * @param[in]  len      The message length.
 *
 * @param[in]  dst      The destination socket address.
 *
 * @return              The async request handle. NULL on fatal error.
 *
 * @see tdgram_sendto_queue_recv()
 */
struct tevent_req *tdgram_sendto_queue_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tdgram_context *dgram,
					    struct tevent_queue *queue,
					    const uint8_t *buf,
					    size_t len,
					    struct tsocket_address *dst);

/**
 * @brief Receive the result of the sent dgram blob.
 *
 * @param[in]  req      The tevent request from tdgram_sendto_queue_send().
 *
 * @param[out] perrno   The error set to the actual errno.
 *
 * @return              The length of the datagram (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 */
ssize_t tdgram_sendto_queue_recv(struct tevent_req *req, int *perrno);

typedef int (*tstream_readv_pdu_next_vector_t)(struct tstream_context *stream,
					       void *private_data,
					       TALLOC_CTX *mem_ctx,
					       struct iovec **vector,
					       size_t *count);

struct tevent_req *tstream_readv_pdu_send(TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				struct tstream_context *stream,
				tstream_readv_pdu_next_vector_t next_vector_fn,
				void *next_vector_private);
int tstream_readv_pdu_recv(struct tevent_req *req, int *perrno);

/**
 * @brief Queue a read request for a PDU on the socket.
 *
 * This function queues a read request for a PDU on a stream socket. The async
 * callback is triggered when a full PDU has been read from the socket.
 *
 * The caller needs to make sure that all non-scalar input parameters hang
 * around for the whole lifetime of the request.
 *
 * @param[in]  mem_ctx  The memory context for the result
 *
 * @param[in]  ev       The tevent_context to run on
 *
 * @param[in]  stream   The stream to send data through
 *
 * @param[in]  queue    The existing send queue
 *
 * @param[in]  next_vector_fn  The next vector function
 *
 * @param[in]  next_vector_private  The private_data of the next vector function
 *
 * @return              The async request handle. NULL on fatal error.
 *
 * @see tstream_readv_pdu_queue_recv()
 */
struct tevent_req *tstream_readv_pdu_queue_send(TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				struct tstream_context *stream,
				struct tevent_queue *queue,
				tstream_readv_pdu_next_vector_t next_vector_fn,
				void *next_vector_private);

/**
 * @brief Receive the PDU blob read from the stream.
 *
 * @param[in]  req      The tevent request from tstream_readv_pdu_queue_send().
 *
 * @param[out] perrno   The error set to the actual errno.
 *
 * @return              The number of bytes read on success, -1 on error with
 *                      perrno set to the actual errno.
 */
int tstream_readv_pdu_queue_recv(struct tevent_req *req, int *perrno);

/**
 * @brief Queue an iovector for sending through the socket
 *
 * This function queues an iovector for sending to destination through an
 * existing stream socket. The async callback is triggered when the whole
 * vectror has been delivered to the underlying system socket.
 *
 * The caller needs to make sure that all non-scalar input parameters hang
 * around for the whole lifetime of the request.
 *
 * @param[in]  mem_ctx  The memory context for the result.
 *
 * @param[in]  ev       The tevent_context to run on.
 *
 * @param[in]  stream   The stream to send data through.
 *
 * @param[in]  queue    The existing send queue.
 *
 * @param[in]  vector   The iovec vector so write.
 *
 * @param[in]  count    The size of the vector.
 *
 * @return              The async request handle. NULL on fatal error.
 */
struct tevent_req *tstream_writev_queue_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct tstream_context *stream,
					     struct tevent_queue *queue,
					     const struct iovec *vector,
					     size_t count);

/**
 * @brief Receive the result of the sent iovector.
 *
 * @param[in]  req      The tevent request from tstream_writev_queue_send().
 *
 * @param[out] perrno   The error set to the actual errno.
 *
 * @return              The length of the iovector (0 is never returned!), -1 on
 *                      error with perrno set to the actual errno.
 */
int tstream_writev_queue_recv(struct tevent_req *req, int *perrno);

/**
 * @}
 */

#endif /* _TSOCKET_H */

