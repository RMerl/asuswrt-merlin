/* $Id: os_symbian.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __OS_SYMBIAN_H__
#define __OS_SYMBIAN_H__

#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/string.h>

#include <e32base.h>
#include <e32cmn.h>
#include <e32std.h>
#include <es_sock.h>
#include <in_sock.h>
#include <charconv.h>
#include <utf.h>
#include <e32cons.h>

// Forward declarations
class CPjSocketReader;

#ifndef PJ_SYMBIAN_TIMER_PRIORITY
#    define PJ_SYMBIAN_TIMER_PRIORITY	EPriorityNormal
#endif

//
// PJLIB Symbian's Socket
//
class CPjSocket
{
public:
    enum
    {
	MAX_LEN = 1500,
    };

    // Construct CPjSocket
    CPjSocket(int af, int sock_type, RSocket &sock)
	: af_(af), sock_(sock), sock_type_(sock_type), connected_(false), 
	  sockReader_(NULL)
    { 
    }

    // Destroy CPjSocket
    ~CPjSocket();

    // Get address family
    int GetAf() const 
    {
    	return af_;	
    }
    
    // Get the internal RSocket
    RSocket& Socket()
    {
	return sock_;
    }

    // Get socket connected flag.
    bool IsConnected() const
    {
	return connected_;
    }

    // Set socket connected flag.
    void SetConnected(bool connected)
    {
	connected_ = connected;
    }

    // Get socket type
    int GetSockType() const
    {
	return sock_type_;
    }
    
    // Returns true if socket is a datagram
    bool IsDatagram() const
    {
	return sock_type_ == KSockDatagram;
    }
    
    // Get socket reader, if any.
    // May return NULL.
    CPjSocketReader *Reader()
    {
	return sockReader_;
    }

    // Create socket reader.
    CPjSocketReader *CreateReader(unsigned max_len=CPjSocket::MAX_LEN);

    // Delete socket reader when it's not wanted.
    void DestroyReader();
    
private:
    int		     af_;
    RSocket	     sock_;	    // Must not be reference, or otherwise
				    // it may point to local variable!
    unsigned   	     sock_type_;
    
    bool	     connected_;
    CPjSocketReader *sockReader_;
};


//
// Socket reader, used by select() and ioqueue abstraction
//
class CPjSocketReader : public CActive
{
public:
    // Construct.
    static CPjSocketReader *NewL(CPjSocket &sock, unsigned max_len=CPjSocket::MAX_LEN);

    // Destroy;
    ~CPjSocketReader();

    // Start asynchronous read from the socket.
    void StartRecv(void (*cb)(void *key)=NULL, 
		   void *key=NULL, 
		   TDes8 *aDesc = NULL,
		   TUint flags = 0);

    // Start asynchronous read from the socket.
    void StartRecvFrom(void (*cb)(void *key)=NULL, 
		       void *key=NULL, 
		       TDes8 *aDesc = NULL,
		       TUint flags = 0,
		       TSockAddr *fromAddr = NULL);

    // Cancel asynchronous read.
    void DoCancel();

    // Implementation: called when read has completed.
    void RunL();

    // Check if there's pending data.
    bool HasData() const
    {
	return buffer_.Length() != 0;
    }

    // Append data to aDesc, up to aDesc's maximum size.
    // If socket is datagram based, buffer_ will be clared.
    void ReadData(TDes8 &aDesc, TInetAddr *addr=NULL);

private:
    CPjSocket	   &sock_;
    bool	    isDatagram_;
    TPtr8	    buffer_;
    TInetAddr	    recvAddr_;

    void	   (*readCb_)(void *key);
    void	    *key_;

    //
    // Constructor
    //
    CPjSocketReader(CPjSocket &sock);
    void ConstructL(unsigned max_len);
};



//
// Time-out Timer Active Object
//
class CPjTimeoutTimer : public CActive
{
public:
    static CPjTimeoutTimer *NewL();
    ~CPjTimeoutTimer();

    void StartTimer(TUint miliSeconds);
    bool HasTimedOut() const;

protected:
    virtual void RunL();
    virtual void DoCancel();
    virtual TInt RunError(TInt aError);

private:
    RTimer	timer_;
    pj_bool_t	hasTimedOut_;

    CPjTimeoutTimer();
    void ConstructL();
};



//
// Symbian OS helper for PJLIB
//
class PjSymbianOS
{
public:
    //
    // Get the singleton instance of PjSymbianOS
    //
    static PjSymbianOS *Instance();

    //
    // Set parameters
    //
    void SetParameters(pj_symbianos_params *params);
    
    //
    // Initialize.
    //
    TInt Initialize();

    //
    // Shutdown.
    //
    void Shutdown();


    //
    // Socket helper.
    //

    // Get RSocketServ instance to be used by all sockets.
    RSocketServ &SocketServ()
    {
	return appSocketServ_ ? *appSocketServ_ : socketServ_;
    }

    // Get RConnection instance, if any.
    RConnection *Connection() 
    {
    	return appConnection_;
    }
    
    // Convert TInetAddr to pj_sockaddr_in
    static inline pj_status_t Addr2pj(const TInetAddr & sym_addr,
			       	      pj_sockaddr &pj_addr,
			       	      int *addr_len,
			       	      pj_bool_t convert_ipv4_mapped_addr = PJ_FALSE)
    {
    TUint fam = sym_addr.Family();
	pj_bzero(&pj_addr, *addr_len);
	if (fam == PJ_AF_INET || 
			(convert_ipv4_mapped_addr && 
			 fam == PJ_AF_INET6 && 
			 sym_addr.IsV4Mapped())) 
	{
		pj_addr.addr.sa_family = PJ_AF_INET;
	    PJ_ASSERT_RETURN(*addr_len>=(int)sizeof(pj_sockaddr_in), PJ_ETOOSMALL);
	    pj_addr.ipv4.sin_addr.s_addr = pj_htonl(sym_addr.Address());
	    pj_addr.ipv4.sin_port = pj_htons((pj_uint16_t) sym_addr.Port());
	    *addr_len = sizeof(pj_sockaddr_in);
	} else if (fam == PJ_AF_INET6) {
	    PJ_ASSERT_RETURN(*addr_len>=(int)sizeof(pj_sockaddr_in6), PJ_ETOOSMALL);
	    const TIp6Addr & ip6 = sym_addr.Ip6Address();
	    pj_addr.addr.sa_family = PJ_AF_INET6;
	    pj_memcpy(&pj_addr.ipv6.sin6_addr, ip6.u.iAddr8, 16);
	    pj_addr.ipv6.sin6_port = pj_htons((pj_uint16_t) sym_addr.Port());
	    pj_addr.ipv6.sin6_scope_id = pj_htonl(sym_addr.Scope());
	    pj_addr.ipv6.sin6_flowinfo = pj_htonl(sym_addr.FlowLabel());
	    *addr_len = sizeof(pj_sockaddr_in6);
	} else {
	    pj_assert(!"Unsupported address family");
	    return PJ_EAFNOTSUP;
	}
	
	return PJ_SUCCESS;
    }


    // Convert pj_sockaddr_in to TInetAddr
    static inline pj_status_t pj2Addr(const pj_sockaddr &pj_addr,
    				      int addrlen,
			       	      TInetAddr & sym_addr)
    {
    	if (pj_addr.addr.sa_family == PJ_AF_INET) {
    	    PJ_ASSERT_RETURN(addrlen >= (int)sizeof(pj_sockaddr_in), PJ_EINVAL);
	    sym_addr.Init(KAfInet);
    	    sym_addr.SetAddress((TUint32)pj_ntohl(pj_addr.ipv4.sin_addr.s_addr));
    	    sym_addr.SetPort(pj_ntohs(pj_addr.ipv4.sin_port));
    	} else if (pj_addr.addr.sa_family == PJ_AF_INET6) {
    	    TIp6Addr ip6;
    	
    	    PJ_ASSERT_RETURN(addrlen>=(int)sizeof(pj_sockaddr_in6), PJ_EINVAL);
    	    pj_memcpy(ip6.u.iAddr8, &pj_addr.ipv6.sin6_addr, 16);
    	    sym_addr.Init(KAfInet6);
    	    sym_addr.SetAddress(ip6);
    	    sym_addr.SetScope(pj_ntohl(pj_addr.ipv6.sin6_scope_id));
    	    sym_addr.SetFlowLabel(pj_ntohl(pj_addr.ipv6.sin6_flowinfo));
    	} else {
    	    pj_assert(!"Unsupported address family");
    	}
    	return PJ_SUCCESS;
    }


    //
    // Resolver helper
    //

    // Get RHostResolver instance
    RHostResolver & GetResolver(int af)
    {
    	if (af==PJ_AF_INET6) {
    	    return appHostResolver6_ ? *appHostResolver6_ : hostResolver6_;
    	} else {
    	    return appHostResolver_ ? *appHostResolver_ : hostResolver_;
    	}
    }

    //
    // Return true if the access point connection is up
    //
    bool IsConnectionUp() const
    {
	return isConnectionUp_;
    }

    //
    // Set access point connection status
    //
    void SetConnectionStatus(bool up)
    {
	isConnectionUp_ = up;
    }

    //
    // Unicode Converter
    //

    // Convert to Unicode
    TInt ConvertToUnicode(TDes16 &aUnicode, const TDesC8 &aForeign);

    // Convert from Unicode
    TInt ConvertFromUnicode(TDes8 &aForeign, const TDesC16 &aUnicode);

    //
    // Get console
    //
    
    // Get console
    CConsoleBase *Console()
    {
	return console_;
    }
    
    //
    // Get select() timeout timer.
    //
    CPjTimeoutTimer *SelectTimeoutTimer()
    {
	return selectTimeoutTimer_;
    }

    //
    // Wait for any active objects to run.
    //
    void WaitForActiveObjects(TInt aPriority = CActive::EPriorityStandard)
    {
	TInt aError;
	CActiveScheduler::Current()->WaitForAnyRequest();
	CActiveScheduler::RunIfReady(aError, aPriority);
    }

private:
    bool isConnectionUp_;
    
    bool isSocketServInitialized_;
    RSocketServ socketServ_;

    bool isResolverInitialized_;
    RHostResolver hostResolver_;
    RHostResolver hostResolver6_;

    CConsoleBase* console_;

    CPjTimeoutTimer *selectTimeoutTimer_;

    // App parameters
    RSocketServ *appSocketServ_;
    RConnection *appConnection_;
    RHostResolver *appHostResolver_;
    RHostResolver *appHostResolver6_;
    
private:
    PjSymbianOS();
};

// This macro is used to check the access point connection status and return
// failure if the AP connection is down or unusable. See the documentation
// of pj_symbianos_set_connection_status() for more info
#define PJ_SYMBIAN_CHECK_CONNECTION() \
    PJ_SYMBIAN_CHECK_CONNECTION2(PJ_ECANCELLED)

#define PJ_SYMBIAN_CHECK_CONNECTION2(retval) \
    do { \
	if (!PjSymbianOS::Instance()->IsConnectionUp()) \
	    return retval; \
    } while (0);

#endif	/* __OS_SYMBIAN_H__ */

