/* $Id: sock.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJPP_SOCK_HPP__
#define __PJPP_SOCK_HPP__

#include <pj/sock.h>
#include <pj/string.h>

class Pj_Event_Handler;

//
// Base class for address.
//
class Pj_Addr
{
};

//
// Internet address.
//
class Pj_Inet_Addr : public pj_sockaddr_in, public Pj_Addr
{
public:
    //
    // Get port number.
    //
    pj_uint16_t get_port_number() const
    {
	return pj_sockaddr_in_get_port(this);
    }

    //
    // Set port number.
    //
    void set_port_number(pj_uint16_t port)
    {
	sin_family = PJ_AF_INET;
	pj_sockaddr_in_set_port(this, port);
    }

    //
    // Get IP address.
    //
    pj_uint32_t get_ip_address() const
    {
	return pj_sockaddr_in_get_addr(this).s_addr;
    }

    //
    // Get address string.
    //
    const char *get_address() const
    {
	return pj_inet_ntoa(sin_addr);
    }

    //
    // Set IP address.
    //
    void set_ip_address(pj_uint32_t addr)
    {
	sin_family = PJ_AF_INET;
	pj_sockaddr_in_set_addr(this, addr);
    }

    //
    // Set address.
    //
    pj_status_t set_address(const pj_str_t *addr)
    {
	return pj_sockaddr_in_set_str_addr(this, addr);
    }

    //
    // Set address.
    //
    pj_status_t set_address(const char *addr)
    {
        pj_str_t s;
	return pj_sockaddr_in_set_str_addr(this, pj_cstr(&s, addr));
    }

    //
    // Compare for equality.
    //
    bool operator==(const Pj_Inet_Addr &rhs) const
    {
	return sin_family == rhs.sin_family &&
               sin_addr.s_addr == rhs.sin_addr.s_addr &&
               sin_port == rhs.sin_port;
    }

private:
    //
    // Dummy length used in pj_ioqueue_recvfrom() etc
    //
    friend class Pj_Event_Handler;
    friend class Pj_Socket;
    friend class Pj_Sock_Stream;
    friend class Pj_Sock_Dgram;

    int addrlen_;
};


//
// Socket base class.
//
// Note:
//  socket will not automatically be closed on destructor.
//
class Pj_Socket
{
public:
    //
    // Default constructor.
    //
    Pj_Socket()
        : sock_(PJ_INVALID_SOCKET) 
    {
    }

    //
    // Initialize from a socket handle.
    //
    explicit Pj_Socket(pj_sock_t sock)
        : sock_(sock)
    {
    }

    //
    // Copy constructor.
    //
    Pj_Socket(const Pj_Socket &rhs) 
        : sock_(rhs.sock_) 
    {
    }

    //
    // Destructor will not close the socket.
    // You must call close() explicitly.
    //
    ~Pj_Socket()
    {
    }

    //
    // Set socket handle.
    //
    void set_handle(pj_sock_t sock)
    {
	sock_ = sock;
    }

    //
    // Get socket handle.
    //
    pj_sock_t get_handle() const
    {
	return sock_;
    }

    //
    // Get socket handle.
    //
    pj_sock_t& get_handle()
    {
	return sock_;
    }

    //
    // See if the socket is valid.
    //
    bool is_valid() const
    {
        return sock_ != PJ_INVALID_SOCKET;
    }

    //
    // Create the socket.
    //
    pj_status_t create(int af, int type, int proto)
    {
	return pj_sock_socket(af, type, proto, &sock_);
    }

    //
    // Bind socket.
    //
    pj_status_t bind(const Pj_Inet_Addr &addr)
    {
	return pj_sock_bind(sock_, &addr, sizeof(Pj_Inet_Addr));
    }

    //
    // Close socket.
    //
    pj_status_t close()
    {
	pj_sock_close(sock_);
    }

    //
    // Get peer socket name.
    //
    pj_status_t getpeername(Pj_Inet_Addr *addr)
    {
	return pj_sock_getpeername(sock_, addr, &addr->addrlen_);
    }

    //
    // getsockname
    //
    pj_status_t getsockname(Pj_Inet_Addr *addr)
    {
	return pj_sock_getsockname(sock_, addr, &addr->addrlen_);
    }

    //
    // getsockopt.
    //
    pj_status_t getsockopt(pj_uint16_t level, pj_uint16_t optname, 
                           void *optval, int *optlen)
    {
	return pj_sock_getsockopt(sock_, level, optname, optval, optlen);
    }

    //
    // setsockopt
    // 
    pj_status_t setsockopt(pj_uint16_t level, pj_uint16_t optname, 
                           const void *optval, int optlen)
    {
	return pj_sock_setsockopt(sock_, level, optname, optval, optlen);
    }

    //
    // receive data.
    //
    pj_ssize_t recv(void *buf, pj_size_t len, int flag = 0)
    {
        pj_ssize_t bytes = len;
	if (pj_sock_recv(sock_, buf, &bytes, flag) != PJ_SUCCESS)
            return -1;
        return bytes;
    }

    //
    // send data.
    //
    pj_ssize_t send(const void *buf, pj_ssize_t len, int flag = 0)
    {
        pj_ssize_t bytes = len;
	if (pj_sock_send(sock_, buf, &bytes, flag) != PJ_SUCCESS)
            return -1;
        return bytes;
    }

    //
    // connect.
    //
    pj_status_t connect(const Pj_Inet_Addr &addr)
    {
	return pj_sock_connect(sock_, &addr, sizeof(Pj_Inet_Addr));
    }

    //
    // assignment.
    //
    Pj_Socket &operator=(const Pj_Socket &rhs)
    {
        sock_ = rhs.sock_;
        return *this;
    }

protected:
    friend class Pj_Event_Handler;
    pj_sock_t sock_;
};


#if PJ_HAS_TCP
//
// Stream socket.
//
class Pj_Sock_Stream : public Pj_Socket
{
public:
    //
    // Default constructor.
    //
    Pj_Sock_Stream() 
    {
    }

    //
    // Initialize from a socket handle.
    //
    explicit Pj_Sock_Stream(pj_sock_t sock)
        : Pj_Socket(sock)
    {
    }

    //
    // Copy constructor.
    //
    Pj_Sock_Stream(const Pj_Sock_Stream &rhs) : Pj_Socket(rhs) 
    {
    }

    //
    // Assignment.
    //
    Pj_Sock_Stream &operator=(const Pj_Sock_Stream &rhs) 
    { 
        sock_ = rhs.sock_; 
        return *this; 
    }

    //
    // listen()
    //
    pj_status_t listen(int backlog = 5)
    {
	return pj_sock_listen(sock_, backlog);
    }

    //
    // blocking accept()
    //
    Pj_Sock_Stream accept(Pj_Inet_Addr *remote_addr = NULL)
    {
        pj_sock_t newsock;
        int *addrlen = remote_addr ? &remote_addr->addrlen_ : NULL;
        pj_status_t status;
        
        status = pj_sock_accept(sock_, &newsock, remote_addr, addrlen);
        if (status != PJ_SUCCESS)
            return Pj_Sock_Stream(-1);

        return Pj_Sock_Stream(newsock);
    }

    //
    // shutdown()
    //
    pj_status_t shutdown(int how = PJ_SHUT_RDWR)
    {
	return pj_sock_shutdown(sock_, how);
    }

};
#endif

//
// Datagram socket.
//
class Pj_Sock_Dgram : public Pj_Socket
{
public:
    //
    // Default constructor.
    //
    Pj_Sock_Dgram() 
    {
    }

    //
    // Initialize from a socket handle.
    //
    explicit Pj_Sock_Dgram(pj_sock_t sock)
        : Pj_Socket(sock)
    {
    }

    //
    // Copy constructor.
    //
    Pj_Sock_Dgram(const Pj_Sock_Dgram &rhs) 
        : Pj_Socket(rhs) 
    {
    }

    //
    // Assignment.
    //
    Pj_Sock_Dgram &operator=(const Pj_Sock_Dgram &rhs) 
    { 
        Pj_Socket::operator =(rhs);
        return *this; 
    }

    //
    // recvfrom()
    //
    pj_ssize_t recvfrom( void *buf, pj_size_t len, int flag = 0, 
                         Pj_Inet_Addr *fromaddr = NULL)
    {
        pj_ssize_t bytes = len;
        int *addrlen = fromaddr ? &fromaddr->addrlen_ : NULL;
	if (pj_sock_recvfrom( sock_, buf, &bytes, flag, 
                              fromaddr, addrlen) != PJ_SUCCESS)
        {
            return -1;
        }
        return bytes;
    }

    //
    // sendto()
    //
    pj_ssize_t sendto( const void *buf, pj_size_t len, int flag, 
                       const Pj_Inet_Addr &addr)
    {
        pj_ssize_t bytes = len;
	if (pj_sock_sendto( sock_, buf, &bytes, flag, 
                            &addr, sizeof(pj_sockaddr_in)) != PJ_SUCCESS)
        {
            return -1;
        }
        return bytes;
    }
};


#endif	/* __PJPP_SOCK_HPP__ */

