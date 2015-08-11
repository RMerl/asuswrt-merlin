/* $Id: ssl_sock_symbian.cpp 3942 2012-01-16 05:05:47Z nanang $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/compat/socket.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>

#include "os_symbian.h"
#include <securesocket.h>
#include <x509cert.h>
#include <e32des8.h>

#define THIS_FILE "ssl_sock_symbian.cpp"


/* Cipher name structure */
typedef struct cipher_name_t {
    pj_ssl_cipher    cipher;
    const char	    *name;
} cipher_name_t;

/* Cipher name constants */
static cipher_name_t cipher_names[] =
{
    {PJ_TLS_NULL_WITH_NULL_NULL,               "NULL"},

    /* TLS/SSLv3 */
    {PJ_TLS_RSA_WITH_NULL_MD5,                 "TLS_RSA_WITH_NULL_MD5"},
    {PJ_TLS_RSA_WITH_NULL_SHA,                 "TLS_RSA_WITH_NULL_SHA"},
    {PJ_TLS_RSA_WITH_NULL_SHA256,              "TLS_RSA_WITH_NULL_SHA256"},
    {PJ_TLS_RSA_WITH_RC4_128_MD5,              "TLS_RSA_WITH_RC4_128_MD5"},
    {PJ_TLS_RSA_WITH_RC4_128_SHA,              "TLS_RSA_WITH_RC4_128_SHA"},
    {PJ_TLS_RSA_WITH_3DES_EDE_CBC_SHA,         "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_RSA_WITH_AES_128_CBC_SHA,          "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_RSA_WITH_AES_256_CBC_SHA,          "TLS_RSA_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_RSA_WITH_AES_128_CBC_SHA256,       "TLS_RSA_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_RSA_WITH_AES_256_CBC_SHA256,       "TLS_RSA_WITH_AES_256_CBC_SHA256"},
    {PJ_TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA,      "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA,      "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,     "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,     "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA,       "TLS_DH_DSS_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA,       "TLS_DH_RSA_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA,      "TLS_DHE_DSS_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA,      "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA,       "TLS_DH_DSS_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA,       "TLS_DH_RSA_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA,      "TLS_DHE_DSS_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA,      "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA256,    "TLS_DH_DSS_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA256,    "TLS_DH_RSA_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,   "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,   "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA256,    "TLS_DH_DSS_WITH_AES_256_CBC_SHA256"},
    {PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA256,    "TLS_DH_RSA_WITH_AES_256_CBC_SHA256"},
    {PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,   "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256"},
    {PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,   "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},
    {PJ_TLS_DH_anon_WITH_RC4_128_MD5,          "TLS_DH_anon_WITH_RC4_128_MD5"},
    {PJ_TLS_DH_anon_WITH_3DES_EDE_CBC_SHA,     "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA"},
    {PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA,      "TLS_DH_anon_WITH_AES_128_CBC_SHA"},
    {PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA,      "TLS_DH_anon_WITH_AES_256_CBC_SHA"},
    {PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA256,   "TLS_DH_anon_WITH_AES_128_CBC_SHA256"},
    {PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA256,   "TLS_DH_anon_WITH_AES_256_CBC_SHA256"},

    /* TLS (deprecated) */
    {PJ_TLS_RSA_EXPORT_WITH_RC4_40_MD5,        "TLS_RSA_EXPORT_WITH_RC4_40_MD5"},
    {PJ_TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5,    "TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5"},
    {PJ_TLS_RSA_WITH_IDEA_CBC_SHA,             "TLS_RSA_WITH_IDEA_CBC_SHA"},
    {PJ_TLS_RSA_EXPORT_WITH_DES40_CBC_SHA,     "TLS_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_RSA_WITH_DES_CBC_SHA,              "TLS_RSA_WITH_DES_CBC_SHA"},
    {PJ_TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,  "TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_DH_DSS_WITH_DES_CBC_SHA,           "TLS_DH_DSS_WITH_DES_CBC_SHA"},
    {PJ_TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,  "TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_DH_RSA_WITH_DES_CBC_SHA,           "TLS_DH_RSA_WITH_DES_CBC_SHA"},
    {PJ_TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA, "TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_DHE_DSS_WITH_DES_CBC_SHA,          "TLS_DHE_DSS_WITH_DES_CBC_SHA"},
    {PJ_TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA, "TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_DHE_RSA_WITH_DES_CBC_SHA,          "TLS_DHE_RSA_WITH_DES_CBC_SHA"},
    {PJ_TLS_DH_anon_EXPORT_WITH_RC4_40_MD5,    "TLS_DH_anon_EXPORT_WITH_RC4_40_MD5"},
    {PJ_TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA, "TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA"},
    {PJ_TLS_DH_anon_WITH_DES_CBC_SHA,          "TLS_DH_anon_WITH_DES_CBC_SHA"},

    /* SSLv3 */
    {PJ_SSL_FORTEZZA_KEA_WITH_NULL_SHA,        "SSL_FORTEZZA_KEA_WITH_NULL_SHA"},
    {PJ_SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA,"SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA"},
    {PJ_SSL_FORTEZZA_KEA_WITH_RC4_128_SHA,     "SSL_FORTEZZA_KEA_WITH_RC4_128_SHA"},

    /* SSLv2 */
    {PJ_SSL_CK_RC4_128_WITH_MD5,               "SSL_CK_RC4_128_WITH_MD5"},
    {PJ_SSL_CK_RC4_128_EXPORT40_WITH_MD5,      "SSL_CK_RC4_128_EXPORT40_WITH_MD5"},
    {PJ_SSL_CK_RC2_128_CBC_WITH_MD5,           "SSL_CK_RC2_128_CBC_WITH_MD5"},
    {PJ_SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5,  "SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5"},
    {PJ_SSL_CK_IDEA_128_CBC_WITH_MD5,          "SSL_CK_IDEA_128_CBC_WITH_MD5"},
    {PJ_SSL_CK_DES_64_CBC_WITH_MD5,            "SSL_CK_DES_64_CBC_WITH_MD5"},
    {PJ_SSL_CK_DES_192_EDE3_CBC_WITH_MD5,      "SSL_CK_DES_192_EDE3_CBC_WITH_MD5"}
};


/* Get cipher name string */
static const char* get_cipher_name(pj_ssl_cipher cipher)
{
    unsigned i, n;

    n = PJ_ARRAY_SIZE(cipher_names);
    for (i = 0; i < n; ++i) {
       if (cipher == cipher_names[i].cipher)
           return cipher_names[i].name;
    }

    return "CIPHER_UNKNOWN";
}

typedef void (*CPjSSLSocket_cb)(int err, void *key);

class CPjSSLSocketReader : public CActive
{
public:
    static CPjSSLSocketReader *NewL(CSecureSocket &sock) 
    {
	CPjSSLSocketReader *self = new (ELeave) 
				   CPjSSLSocketReader(sock);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
    }

    ~CPjSSLSocketReader() {
	Cancel();
    }

    /* Asynchronous read from the socket. */
    int Read(CPjSSLSocket_cb cb, void *key, TPtr8 &data, TUint flags)
    {
	PJ_ASSERT_RETURN(!IsActive(), PJ_EBUSY);
	
	cb_ = cb;
	key_ = key;
	sock_.RecvOneOrMore(data, iStatus, len_received_);
	SetActive();
	
	return PJ_EPENDING;
    }

private:
    CSecureSocket  	&sock_;
    CPjSSLSocket_cb	 cb_;
    void		*key_;
    TSockXfrLength  	 len_received_; /* not really useful? */

    void DoCancel() {
	sock_.CancelAll();
    }
    
    void RunL() {
	(*cb_)(iStatus.Int(), key_);
    }

    CPjSSLSocketReader(CSecureSocket &sock) : 
	CActive(0), sock_(sock), cb_(NULL), key_(NULL) 
    {}
    
    void ConstructL() {
	CActiveScheduler::Add(this);
    }
};

class CPjSSLSocket : public CActive
{
public:
    enum ssl_state {
	SSL_STATE_NULL,
	SSL_STATE_CONNECTING,
	SSL_STATE_HANDSHAKING,
	SSL_STATE_ESTABLISHED
    };
    
    static CPjSSLSocket *NewL(const TDesC8 &ssl_proto,
			      pj_qos_type qos_type,
			      const pj_qos_params &qos_params) 
    {
	CPjSSLSocket *self = new (ELeave) CPjSSLSocket(qos_type, qos_params);
	CleanupStack::PushL(self);
	self->ConstructL(ssl_proto);
	CleanupStack::Pop(self);
	return self;
    }

    ~CPjSSLSocket() {
	Cancel();
	CleanupSubObjects();
    }

    int Connect(CPjSSLSocket_cb cb, void *key, const TInetAddr &local_addr, 
		const TInetAddr &rem_addr, 
		const TDesC8 &servername = TPtrC8(NULL,0),
		const TDesC8 &ciphers = TPtrC8(NULL,0));
    int Send(CPjSSLSocket_cb cb, void *key, const TDesC8 &aDesc, TUint flags);
    int SendSync(const TDesC8 &aDesc, TUint flags);

    CPjSSLSocketReader* GetReader();
    enum ssl_state GetState() const { return state_; }
    const TInetAddr* GetLocalAddr() const { return &local_addr_; }
    int GetCipher(TDes8 &cipher) const {
	if (securesock_)
	    return securesock_->CurrentCipherSuite(cipher);
	return KErrNotFound;
    }
    const CX509Certificate *GetPeerCert() {
	if (securesock_)
	    return securesock_->ServerCert();
	return NULL;
    }

private:
    enum ssl_state	 state_;
    pj_sock_t	    	 sock_;
    CSecureSocket  	*securesock_;
    bool	    	 is_connected_;
    
    pj_qos_type 	 qos_type_;
    pj_qos_params 	 qos_params_;
    			      
    CPjSSLSocketReader  *reader_;
    TBuf<32> 	    	 ssl_proto_;
    TInetAddr       	 rem_addr_;
    TPtrC8		 servername_;
    TPtrC8		 ciphers_;
    TInetAddr       	 local_addr_;
    TSockXfrLength 	 sent_len_;

    CPjSSLSocket_cb 	 cb_;
    void 	   	*key_;
    
    void DoCancel();
    void RunL();

    CPjSSLSocket(pj_qos_type qos_type, const pj_qos_params &qos_params) :
	CActive(0), state_(SSL_STATE_NULL), sock_(PJ_INVALID_SOCKET), 
	securesock_(NULL), is_connected_(false),
	qos_type_(qos_type), qos_params_(qos_params),
	reader_(NULL), 	cb_(NULL), key_(NULL)
    {}
    
    void ConstructL(const TDesC8 &ssl_proto) {
	ssl_proto_.Copy(ssl_proto);
	CActiveScheduler::Add(this);
    }

    void CleanupSubObjects() {
	delete reader_;
	reader_ = NULL;
	if (securesock_) {
	    if (state_ == SSL_STATE_ESTABLISHED)
		securesock_->Close();
	    delete securesock_;
	    securesock_ = NULL;
	}
	if (sock_ != PJ_INVALID_SOCKET) {
	    pj_sock_close(sock_);
	    sock_ = PJ_INVALID_SOCKET;
	}	    
    }
};

int CPjSSLSocket::Connect(CPjSSLSocket_cb cb, void *key, 
			  const TInetAddr &local_addr, 
			  const TInetAddr &rem_addr,
			  const TDesC8 &servername,
			  const TDesC8 &ciphers)
{
    pj_status_t status;
    
    PJ_ASSERT_RETURN(state_ == SSL_STATE_NULL, PJ_EINVALIDOP);
    
    status = pj_sock_socket(rem_addr.Family(), pj_SOCK_STREAM(), 0, &sock_);
    if (status != PJ_SUCCESS)
	return status;

    // Apply QoS
    status = pj_sock_apply_qos2(sock_, qos_type_, &qos_params_, 
    				2,  THIS_FILE, NULL);
    
    RSocket &rSock = ((CPjSocket*)sock_)->Socket();

    local_addr_ = local_addr;
    
    if (!local_addr_.IsUnspecified()) {
	TInt err = rSock.Bind(local_addr_);
	if (err != KErrNone)
	    return PJ_RETURN_OS_ERROR(err);
    }
    
    cb_ = cb;
    key_ = key;
    rem_addr_ = rem_addr;
    
    /* Note: the following members only keep the pointer, not the data */
    servername_.Set(servername);
    ciphers_.Set(ciphers);

    rSock.Connect(rem_addr_, iStatus);
    SetActive();
    state_ = SSL_STATE_CONNECTING;
    
    rSock.LocalName(local_addr_);

    return PJ_EPENDING;
}

int CPjSSLSocket::Send(CPjSSLSocket_cb cb, void *key, const TDesC8 &aDesc, 
		       TUint flags)
{
    PJ_UNUSED_ARG(flags);

    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, PJ_EINVALIDOP);
    
    if (IsActive())
	return PJ_EBUSY;
    
    cb_ = cb;
    key_ = key;
    
    securesock_->Send(aDesc, iStatus, sent_len_);
    SetActive();
    
    return PJ_EPENDING;
}

int CPjSSLSocket::SendSync(const TDesC8 &aDesc, TUint flags)
{
    PJ_UNUSED_ARG(flags);

    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, PJ_EINVALIDOP);
    
    TRequestStatus reqStatus;
    securesock_->Send(aDesc, reqStatus, sent_len_);
    User::WaitForRequest(reqStatus);
    
    return PJ_RETURN_OS_ERROR(reqStatus.Int());
}

CPjSSLSocketReader* CPjSSLSocket::GetReader()
{
    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, NULL);
    
    if (reader_)
	return reader_;
    
    TRAPD(err,	reader_ = CPjSSLSocketReader::NewL(*securesock_));
    if (err != KErrNone)
	return NULL;
    
    return reader_;
}

void CPjSSLSocket::DoCancel()
{
    /* Operation to be cancelled depends on current state */
    switch (state_) {
    case SSL_STATE_CONNECTING:
	{
	    RSocket &rSock = ((CPjSocket*)sock_)->Socket();

	    rSock.CancelConnect();
	    CleanupSubObjects();
	    state_ = SSL_STATE_NULL;
	}
	break;
    case SSL_STATE_HANDSHAKING:
	{
	    securesock_->CancelHandshake();
	    CleanupSubObjects();
	    state_ = SSL_STATE_NULL;
	}
	break;
    case SSL_STATE_ESTABLISHED:
	securesock_->CancelSend();
	break;
    default:
	break;
    }
}

void CPjSSLSocket::RunL()
{
    switch (state_) {
    case SSL_STATE_CONNECTING:
	if (iStatus != KErrNone) {
	    CleanupSubObjects();
	    state_ = SSL_STATE_NULL;
	    /* Dispatch connect failure notification */
	    if (cb_) (*cb_)(iStatus.Int(), key_);
	} else {
	    RSocket &rSock = ((CPjSocket*)sock_)->Socket();

	    /* Get local addr */
	    rSock.LocalName(local_addr_);
	    
	    /* Prepare and start handshake */
	    securesock_ = CSecureSocket::NewL(rSock, ssl_proto_);
	    securesock_->SetDialogMode(EDialogModeAttended);
	    if (servername_.Length() > 0)
		securesock_->SetOpt(KSoSSLDomainName, KSolInetSSL,
				    servername_);
	    if (ciphers_.Length() > 0)
		securesock_->SetAvailableCipherSuites(ciphers_);

	    // FlushSessionCache() seems to also fire signals to all 
	    // completed AOs (something like CActiveScheduler::RunIfReady())
	    // which may cause problem, e.g: we've experienced that when 
	    // SSL timeout is set to 1s, the SSL timeout timer fires up
	    // at this point and securesock_ instance gets deleted here!
	    // So be careful using this. And we don't think we need it here.
	    //securesock_->FlushSessionCache();

	    securesock_->StartClientHandshake(iStatus);
	    SetActive();
	    state_ = SSL_STATE_HANDSHAKING;
	}
	break;
    case SSL_STATE_HANDSHAKING:
	if (iStatus == KErrNone) {
	    state_ = SSL_STATE_ESTABLISHED;
	} else {
	    state_ = SSL_STATE_NULL;
	    CleanupSubObjects();
	}
	/* Dispatch connect status notification */
	if (cb_) (*cb_)(iStatus.Int(), key_);
	break;
    case SSL_STATE_ESTABLISHED:
	/* Dispatch data sent notification */
	if (cb_) (*cb_)(iStatus.Int(), key_);
	break;
    default:
	pj_assert(0);
	break;
    }
}

typedef void (*CPjTimer_cb)(void *user_data);

class CPjTimer : public CActive 
{
public:
    CPjTimer(const pj_time_val *delay, CPjTimer_cb cb, void *user_data) : 
	CActive(0), cb_(cb), user_data_(user_data)
    {
	CActiveScheduler::Add(this);

	rtimer_.CreateLocal();
	pj_int32_t interval = PJ_TIME_VAL_MSEC(*delay) * 1000;
	if (interval < 0) {
	    interval = 0;
	}
	rtimer_.After(iStatus, interval);
	SetActive();
    }
    
    ~CPjTimer() { Cancel(); }
    
private:	
    RTimer		 rtimer_;
    CPjTimer_cb		 cb_;
    void		*user_data_;
    
    void RunL() { if (cb_) (*cb_)(user_data_); }
    void DoCancel() { rtimer_.Cancel(); }
};

/*
 * Structure of recv/read state.
 */
typedef struct read_state_t {
    TPtr8		*read_buf;
    TPtr8		*orig_buf;
    pj_uint32_t		 flags;    
} read_state_t;

/*
 * Structure of send/write data.
 */
typedef struct write_data_t {
    pj_size_t 	 	 len;
    pj_ioqueue_op_key_t	*key;
    pj_size_t 	 	 data_len;
    char		 data[1];
} write_data_t;

/*
 * Structure of send/write state.
 */
typedef struct write_state_t {
    char		*buf;
    pj_size_t		 max_len;    
    char		*start;
    pj_size_t		 len;
    write_data_t	*current_data;
    TPtrC8		 send_ptr;
} write_state_t;

/*
 * Secure socket structure definition.
 */
struct pj_ssl_sock_t
{
    pj_pool_t		*pool;
    pj_ssl_sock_cb	 cb;
    void		*user_data;
    
    pj_bool_t		 established;
    write_state_t	 write_state;
    read_state_t	 read_state;
    CPjTimer		*connect_timer;

    CPjSSLSocket   	*sock;
    int			 sock_af;
    int			 sock_type;
    pj_sockaddr		 local_addr;
    pj_sockaddr		 rem_addr;

    /* QoS settings */
    pj_qos_type		 qos_type;
    pj_qos_params	 qos_params;
    pj_bool_t		 qos_ignore_error;


    pj_ssl_sock_proto	 proto;
    pj_time_val		 timeout;
    pj_str_t		 servername;
    pj_str_t		 ciphers;
    pj_ssl_cert_info	 remote_cert_info;
};


static pj_str_t get_cert_name(char *buf, unsigned buf_len,
                              const CX500DistinguishedName &name)
{
    TInt i;
    TUint8 *p;
    TInt l = buf_len;
    
    p = (TUint8*)buf;
    for(i = 0; i < name.Count(); ++i) {
	const CX520AttributeTypeAndValue &attr = name.Element(i);

	/* Print element separator */
	*p++ = '/';
	if (0 == --l) break;

	/* Print the type. */
	TPtr8 type(p, l);
	type.Copy(attr.Type());
	p += type.Length();
	l -= type.Length();
	if (0 >= --l) break;

	/* Print equal sign */
	*p++ = '=';
	if (0 == --l) break;
	
	/* Print the value. Let's just get the raw data here */
	TPtr8 value(p, l);
	value.Copy(attr.EncodedValue().Mid(2));
	p += value.Length();
	l -= value.Length();
	if (0 >= --l) break;
    }
    
    pj_str_t src;
    pj_strset(&src, buf, buf_len - l);
    
    return src;
}
                            
/* Get certificate info from CX509Certificate.
 */
static void get_cert_info(pj_pool_t *pool, pj_ssl_cert_info *ci,
                          const CX509Certificate *x)
{
    enum { tmp_buf_len = 512 };
    char *tmp_buf;
    unsigned len;
    
    pj_assert(pool && ci && x);
    
    /* Init */
    tmp_buf = new char[tmp_buf_len];
    pj_bzero(ci, sizeof(*ci));
    
    /* Version */
    ci->version = x->Version();
    
    /* Serial number */
    len = x->SerialNumber().Length();
    if (len > sizeof(ci->serial_no)) 
	len = sizeof(ci->serial_no);
    pj_memcpy(ci->serial_no + sizeof(ci->serial_no) - len, 
              x->SerialNumber().Ptr(), len);
    
    /* Subject */
    {
	HBufC *subject = NULL;
	TRAPD(err, subject = x->SubjectL());
	if (err == KErrNone) {
	    TPtr16 ptr16(subject->Des());
	    len = ptr16.Length();
	    TPtr8 ptr8((TUint8*)pj_pool_alloc(pool, len), len);
	    ptr8.Copy(ptr16);
	    pj_strset(&ci->subject.cn, (char*)ptr8.Ptr(), ptr8.Length());
	}
	pj_str_t tmp = get_cert_name(tmp_buf, tmp_buf_len,
				     x->SubjectName());
	pj_strdup(pool, &ci->subject.info, &tmp);
    }

    /* Issuer */
    {
	HBufC *issuer = NULL;
	TRAPD(err, issuer = x->IssuerL());
	if (err == KErrNone) {
	    TPtr16 ptr16(issuer->Des());
	    len = ptr16.Length();
	    TPtr8 ptr8((TUint8*)pj_pool_alloc(pool, len), len);
	    ptr8.Copy(ptr16);
	    pj_strset(&ci->issuer.cn, (char*)ptr8.Ptr(), ptr8.Length());
	}
	pj_str_t tmp = get_cert_name(tmp_buf, tmp_buf_len,
				     x->IssuerName());
	pj_strdup(pool, &ci->issuer.info, &tmp);
    }
    
    /* Validity */
    const CValidityPeriod &valid_period = x->ValidityPeriod();
    TTime base_time(TDateTime(1970, EJanuary, 0, 0, 0, 0, 0));
    TTimeIntervalSeconds tmp_sec;
    valid_period.Start().SecondsFrom(base_time, tmp_sec);
    ci->validity.start.sec = tmp_sec.Int(); 
    valid_period.Finish().SecondsFrom(base_time, tmp_sec);
    ci->validity.end.sec = tmp_sec.Int();
    
    /* Deinit */
    delete [] tmp_buf;
}


/* Update certificates info. This function should be called after handshake
 * or renegotiation successfully completed.
 */
static void update_certs_info(pj_ssl_sock_t *ssock)
{
    const CX509Certificate *x;

    pj_assert(ssock && ssock->sock &&
              ssock->sock->GetState() == CPjSSLSocket::SSL_STATE_ESTABLISHED);
        
    /* Active remote certificate */
    x = ssock->sock->GetPeerCert();
    if (x) {
	get_cert_info(ssock->pool, &ssock->remote_cert_info, x);
    } else {
	pj_bzero(&ssock->remote_cert_info, sizeof(pj_ssl_cert_info));
    }
}


/* Available ciphers */
static unsigned ciphers_num_ = 0;
static struct ciphers_t
{
    pj_ssl_cipher    id;
    const char	    *name;
} ciphers_[64];

/*
 * Get cipher list supported by SSL/TLS backend.
 */
PJ_DEF(pj_status_t) pj_ssl_cipher_get_availables (pj_ssl_cipher ciphers[],
					          unsigned *cipher_num)
{
    unsigned i;

    PJ_ASSERT_RETURN(ciphers && cipher_num, PJ_EINVAL);
    
    if (ciphers_num_ == 0) {
        RSocket sock;
        CSecureSocket *secure_sock;
        TPtrC16 proto(_L16("TLS1.0"));

        secure_sock = CSecureSocket::NewL(sock, proto);
        if (secure_sock) {
            TBuf8<128> ciphers_buf(0);
            secure_sock->AvailableCipherSuites(ciphers_buf);
            
            ciphers_num_ = ciphers_buf.Length() / 2;
            if (ciphers_num_ > PJ_ARRAY_SIZE(ciphers_))
        	ciphers_num_ = PJ_ARRAY_SIZE(ciphers_);
            for (i = 0; i < ciphers_num_; ++i) {
                ciphers_[i].id = (pj_ssl_cipher)(ciphers_buf[i*2]*10 + 
					         ciphers_buf[i*2+1]);
		ciphers_[i].name = get_cipher_name(ciphers_[i].id);
	    }
        }
        
        delete secure_sock;
    }
    
    if (ciphers_num_ == 0) {
	*cipher_num = 0;
	return PJ_ENOTFOUND;
    }
    
    *cipher_num = PJ_MIN(*cipher_num, ciphers_num_);
    for (i = 0; i < *cipher_num; ++i)
        ciphers[i] = ciphers_[i].id;
    
    return PJ_SUCCESS;
}


/* Get cipher name string */
PJ_DEF(const char*) pj_ssl_cipher_name(pj_ssl_cipher cipher)
{
    unsigned i;

    if (ciphers_num_ == 0) {
	pj_ssl_cipher c[1];
	i = 0;
	pj_ssl_cipher_get_availables(c, &i);
    }
	
    for (i = 0; i < ciphers_num_; ++i) {
	if (cipher == ciphers_[i].id)
	    return ciphers_[i].name;
    }

    return NULL;
}


/* Check if the specified cipher is supported by SSL/TLS backend. */
PJ_DEF(pj_bool_t) pj_ssl_cipher_is_supported(pj_ssl_cipher cipher)
{
    unsigned i;

    if (ciphers_num_ == 0) {
	pj_ssl_cipher c[1];
	i = 0;
	pj_ssl_cipher_get_availables(c, &i);
    }
	
    for (i = 0; i < ciphers_num_; ++i) {
	if (cipher == ciphers_[i].id)
	    return PJ_TRUE;
    }

    return PJ_FALSE;
}


/*
 * Create SSL socket instance. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_create (pj_pool_t *pool,
					const pj_ssl_sock_param *param,
					pj_ssl_sock_t **p_ssock)
{
    pj_ssl_sock_t *ssock;

    PJ_ASSERT_RETURN(param->async_cnt == 1, PJ_EINVAL);
    PJ_ASSERT_RETURN(pool && param && p_ssock, PJ_EINVAL);

    /* Allocate secure socket */
    ssock = PJ_POOL_ZALLOC_T(pool, pj_ssl_sock_t);
    
    /* Allocate write buffer */
    ssock->write_state.buf = (char*)pj_pool_alloc(pool, 
						  param->send_buffer_size);
    ssock->write_state.max_len = param->send_buffer_size;
    ssock->write_state.start = ssock->write_state.buf;
    
    /* Init secure socket */
    ssock->pool = pool;
    ssock->sock_af = param->sock_af;
    ssock->sock_type = param->sock_type;
    ssock->cb = param->cb;
    ssock->user_data = param->user_data;
    ssock->timeout = param->timeout;
    if (param->ciphers_num > 0) {
	/* Cipher list in Symbian is represented as array of two-octets. */
	ssock->ciphers.slen = param->ciphers_num*2;
	ssock->ciphers.ptr  = (char*)pj_pool_alloc(pool, ssock->ciphers.slen);
	pj_uint8_t *c = (pj_uint8_t*)ssock->ciphers.ptr;
	for (unsigned i = 0; i < param->ciphers_num; ++i) {
	    *c++ = (pj_uint8_t)(param->ciphers[i] & 0xFF00) >> 8;
	    *c++ = (pj_uint8_t)(param->ciphers[i] & 0xFF);
	}
    }
    pj_strdup_with_null(pool, &ssock->servername, &param->server_name);

    ssock->qos_type = param->qos_type;
    ssock->qos_ignore_error = param->qos_ignore_error;
    pj_memcpy(&ssock->qos_params, &param->qos_params,
	      sizeof(param->qos_params));

    /* Finally */
    *p_ssock = ssock;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ssl_cert_load_from_files(pj_pool_t *pool,
                                        	const pj_str_t *CA_file,
                                        	const pj_str_t *cert_file,
                                        	const pj_str_t *privkey_file,
                                        	const pj_str_t *privkey_pass,
                                        	pj_ssl_cert_t **p_cert)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(CA_file);
    PJ_UNUSED_ARG(cert_file);
    PJ_UNUSED_ARG(privkey_file);
    PJ_UNUSED_ARG(privkey_pass);
    PJ_UNUSED_ARG(p_cert);
    return PJ_ENOTSUP;
}

/*
 * Set SSL socket credential.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_set_certificate(
					    pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    const pj_ssl_cert_t *cert)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(cert);
    return PJ_ENOTSUP;
}

/*
 * Close the SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);
    
    delete ssock->connect_timer;
    ssock->connect_timer = NULL;
    
    delete ssock->sock;
    ssock->sock = NULL;

    delete ssock->read_state.read_buf;
    delete ssock->read_state.orig_buf;
    ssock->read_state.read_buf = NULL;
    ssock->read_state.orig_buf = NULL;
    
    return PJ_SUCCESS;
}


/*
 * Associate arbitrary data with the SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_set_user_data (pj_ssl_sock_t *ssock,
					       void *user_data)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);
    
    ssock->user_data = user_data;
    
    return PJ_SUCCESS;
}
					       

/*
 * Retrieve the user data previously associated with this SSL
 * socket.
 */
PJ_DEF(void*) pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, NULL);
    
    return ssock->user_data;
}


/*
 * Retrieve the local address and port used by specified SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_get_info (pj_ssl_sock_t *ssock,
					  pj_ssl_sock_info *info)
{
    PJ_ASSERT_RETURN(ssock && info, PJ_EINVAL);
    
    pj_bzero(info, sizeof(*info));
    
    info->established = ssock->established;
    
    /* Local address */
    if (ssock->sock) {
	const TInetAddr* local_addr_ = ssock->sock->GetLocalAddr();
	int addrlen = sizeof(pj_sockaddr);
	pj_status_t status;
	
	status = PjSymbianOS::Addr2pj(*local_addr_, info->local_addr, &addrlen);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	pj_sockaddr_cp(&info->local_addr, &ssock->local_addr);
    }

    if (info->established) {
	/* Cipher suite */
	TBuf8<4> cipher;
	if (ssock->sock->GetCipher(cipher) == KErrNone) {
	    info->cipher = (pj_ssl_cipher)cipher[1]; 
	}

	/* Remote address */
        pj_sockaddr_cp((pj_sockaddr_t*)&info->remote_addr, 
    		       (pj_sockaddr_t*)&ssock->rem_addr);
        
        /* Certificates info */
        info->remote_cert_info = &ssock->remote_cert_info;
    }

    /* Protocol */
    info->proto = ssock->proto;
    
    return PJ_SUCCESS;
}


/*
 * Starts read operation on this SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read (pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    unsigned buff_size,
					    pj_uint32_t flags)
{
    PJ_ASSERT_RETURN(ssock && pool && buff_size, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->established, PJ_EINVALIDOP);

    /* Reading is already started */
    if (ssock->read_state.orig_buf) {
	return PJ_SUCCESS;
    }

    void *readbuf[1];
    readbuf[0] = pj_pool_alloc(pool, buff_size);
    return pj_ssl_sock_start_read2(ssock, pool, buff_size, readbuf, flags);
}

static void read_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    pj_status_t status;

    status = (err == KErrNone)? PJ_SUCCESS : PJ_RETURN_OS_ERROR(err);

    /* Check connection status */
    if (err == KErrEof || !PjSymbianOS::Instance()->IsConnectionUp() ||
	!ssock->established) 
    {
	status = PJ_EEOF;
    }
    
    /* Notify data arrival */
    if (ssock->cb.on_data_read) {
	pj_size_t remainder = 0;
	char *data = (char*)ssock->read_state.orig_buf->Ptr();
	pj_size_t data_len = ssock->read_state.read_buf->Length() + 
			     ssock->read_state.read_buf->Ptr() -
			     ssock->read_state.orig_buf->Ptr();
	
	if (data_len > 0) {
	    /* Notify received data */
	    pj_bool_t ret = (*ssock->cb.on_data_read)(ssock, data, data_len, 
						      status, &remainder);
	    if (!ret) {
		/* We've been destroyed */
		return;
	    }
	    
	    /* Calculate available data for next READ operation */
	    if (remainder > 0) {
		pj_size_t data_maxlen = ssock->read_state.orig_buf->MaxLength();
		
		/* There is some data left unconsumed by application, we give
		 * smaller buffer for next READ operation.
		 */
		ssock->read_state.read_buf->Set((TUint8*)data+remainder, 0, 
					        data_maxlen - remainder);
	    } else {
		/* Give all buffer for next READ operation. 
		 */
		ssock->read_state.read_buf->Set(*ssock->read_state.orig_buf);
	    }
	}
    }

    if (status == PJ_SUCCESS) {
	/* Perform the "next" READ operation */
	CPjSSLSocketReader *reader = ssock->sock->GetReader(); 
	ssock->read_state.read_buf->SetLength(0);
	status = reader->Read(&read_cb, ssock, *ssock->read_state.read_buf, 
			      ssock->read_state.flags);
    }
    
    /* Connection closed or something goes wrong */
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	/* Notify error */
	if (ssock->cb.on_data_read) {
	    pj_bool_t ret = (*ssock->cb.on_data_read)(ssock, NULL, 0, 
						      status, NULL);
	    if (!ret) {
		/* We've been destroyed */
		return;
	    }
	}
	
	delete ssock->read_state.read_buf;
	delete ssock->read_state.orig_buf;
	ssock->read_state.read_buf = NULL;
	ssock->read_state.orig_buf = NULL;
	ssock->established = PJ_FALSE;
    }
}

/*
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read2 (pj_ssl_sock_t *ssock,
					     pj_pool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     pj_uint32_t flags)
{
    PJ_ASSERT_RETURN(ssock && buff_size && readbuf, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->established, PJ_EINVALIDOP);
    
    /* Return failure if access point is marked as down by app. */
    PJ_SYMBIAN_CHECK_CONNECTION();
    
    /* Reading is already started */
    if (ssock->read_state.orig_buf) {
	return PJ_SUCCESS;
    }
    
    PJ_UNUSED_ARG(pool);

    /* Get reader instance */
    CPjSSLSocketReader *reader = ssock->sock->GetReader();
    if (!reader)
	return PJ_ENOMEM;
    
    /* We manage two buffer pointers here:
     * 1. orig_buf keeps the orginal buffer address (and its max length).
     * 2. read_buf provides buffer for READ operation, mind that there may be
     *    some remainder data left by application.
     */
    ssock->read_state.read_buf = new TPtr8((TUint8*)readbuf[0], 0, buff_size);
    ssock->read_state.orig_buf = new TPtr8((TUint8*)readbuf[0], 0, buff_size);
    ssock->read_state.flags = flags;
    
    pj_status_t status;
    status = reader->Read(&read_cb, ssock, *ssock->read_state.read_buf, 
			  ssock->read_state.flags);
    
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	delete ssock->read_state.read_buf;
	delete ssock->read_state.orig_buf;
	ssock->read_state.read_buf = NULL;
	ssock->read_state.orig_buf = NULL;
	
	return status;
    }
    
    return PJ_SUCCESS;
}

/*
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom (pj_ssl_sock_t *ssock,
						pj_pool_t *pool,
						unsigned buff_size,
						pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(flags);
    return PJ_ENOTSUP;
}

/*
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom2 (pj_ssl_sock_t *ssock,
						 pj_pool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(readbuf);
    PJ_UNUSED_ARG(flags);
    return PJ_ENOTSUP;
}

static void send_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    write_state_t *st = &ssock->write_state;

    /* Check connection status */
    if (err != KErrNone || !PjSymbianOS::Instance()->IsConnectionUp() ||
	!ssock->established) 
    {
	ssock->established = PJ_FALSE;
	return;
    }

    /* Remove sent data from buffer */
    st->start += st->current_data->len;
    st->len -= st->current_data->len;

    /* Reset current outstanding send */
    st->current_data = NULL;

    /* Let's check if there is pending data to send */
    if (st->len) {
	write_data_t *wdata = (write_data_t*)st->start;
	pj_status_t status;
	
	st->send_ptr.Set((TUint8*)wdata->data, (TInt)wdata->data_len);
	st->current_data = wdata;
	status = ssock->sock->Send(&send_cb, ssock, st->send_ptr, 0);
	if (status != PJ_EPENDING) {
	    ssock->established = PJ_FALSE;
	    st->len = 0;
	    return;
	}
    } else {
        /* Buffer empty, reset the start position */
        st->start = st->buf;
    }
}

/*
 * Send data using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_send (pj_ssl_sock_t *ssock,
				      pj_ioqueue_op_key_t *send_key,
				      const void *data,
				      pj_ssize_t *size,
				      unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(ssock && data && size, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->write_state.max_len == 0 || 
		     ssock->write_state.max_len >= (pj_size_t)*size, 
		     PJ_ETOOSMALL);
    
    /* Check connection status */
    if (!PjSymbianOS::Instance()->IsConnectionUp() || !ssock->established) 
    {
	ssock->established = PJ_FALSE;
	return PJ_ECANCELLED;
    }

    write_state_t *st = &ssock->write_state;
    
    /* Synchronous mode */
    if (st->max_len == 0) {
	st->send_ptr.Set((TUint8*)data, (TInt)*size);
	return ssock->sock->SendSync(st->send_ptr, flags);
    }

    /* CSecureSocket only allows one outstanding send operation, so
     * we use buffering mechanism to allow application to perform send 
     * operations at any time.
     */
    
    pj_size_t needed_len = *size + sizeof(write_data_t) - 1;
    
    /* Align needed_len to be multiplication of 4 */
    needed_len = ((needed_len + 3) >> 2) << 2; 

    /* Block until there is buffer slot available and contiguous! */
    while (st->start + st->len + needed_len > st->buf + st->max_len) {
	pj_symbianos_poll(-1, -1);
    }

    /* Push back the send data into the buffer */
    write_data_t *wdata = (write_data_t*)(st->start + st->len);
    
    wdata->len = needed_len;
    wdata->key = send_key;
    wdata->data_len = (pj_size_t)*size;
    pj_memcpy(wdata->data, data, *size);
    st->len += needed_len;

    /* If no outstanding send, send it */
    if (st->current_data == NULL) {
	pj_status_t status;
	    
	wdata = (write_data_t*)st->start;
	st->current_data = wdata;
	st->send_ptr.Set((TUint8*)wdata->data, (TInt)wdata->data_len);
	status = ssock->sock->Send(&send_cb, ssock, st->send_ptr, flags);
	
	if (status != PJ_EPENDING) {
	    *size = -status;
	    return status;
	}
    }
    
    return PJ_SUCCESS;
}

/*
 * Send datagram using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_sendto (pj_ssl_sock_t *ssock,
					pj_ioqueue_op_key_t *send_key,
					const void *data,
					pj_ssize_t *size,
					unsigned flags,
					const pj_sockaddr_t *addr,
					int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(data);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);
    return PJ_ENOTSUP;
}

/*
 * Starts asynchronous socket accept() operations on this SSL socket. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_accept (pj_ssl_sock_t *ssock,
					      pj_pool_t *pool,
					      const pj_sockaddr_t *local_addr,
					      int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(local_addr);
    PJ_UNUSED_ARG(addr_len);
    
    return PJ_ENOTSUP;
}

static void connect_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    pj_status_t status;
    
    if (ssock->connect_timer) {
	delete ssock->connect_timer;
	ssock->connect_timer = NULL;
    }

    status = (err == KErrNone)? PJ_SUCCESS : PJ_RETURN_OS_ERROR(err);
    if (status == PJ_SUCCESS) {
	ssock->established = PJ_TRUE;
	update_certs_info(ssock);
    } else {
	delete ssock->sock;
	ssock->sock = NULL;
	if (err == KErrTimedOut) status = PJ_ETIMEDOUT;
    }
    
    if (ssock->cb.on_connect_complete) {
	pj_bool_t ret = (*ssock->cb.on_connect_complete)(ssock, status);
	if (!ret) {
	    /* We've been destroyed */
	    return;
	}
    }
}

static void connect_timer_cb(void *key)
{
    connect_cb(KErrTimedOut, key);
}

/*
 * Starts asynchronous socket connect() operation and SSL/TLS handshaking 
 * for this socket. Once the connection is done (either successfully or not),
 * the \a on_connect_complete() callback will be called.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_connect (pj_ssl_sock_t *ssock,
					       pj_pool_t *pool,
					       const pj_sockaddr_t *localaddr,
					       const pj_sockaddr_t *remaddr,
					       int addr_len)
{
    CPjSSLSocket *sock = NULL;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
		     PJ_EINVAL);

    /* Check connection status */
    PJ_SYMBIAN_CHECK_CONNECTION();
    
    if (ssock->sock != NULL) {
	CPjSSLSocket::ssl_state state = ssock->sock->GetState();
	switch (state) {
	case CPjSSLSocket::SSL_STATE_ESTABLISHED:
	    return PJ_SUCCESS;
	default:
	    return PJ_EPENDING;
	}
    }

    /* Set SSL protocol */
    TPtrC8 proto;
    
    if (ssock->proto == PJ_SSL_SOCK_PROTO_DEFAULT)
	ssock->proto = PJ_SSL_SOCK_PROTO_TLS1;

    /* CSecureSocket only support TLS1.0 and SSL3.0 */
    switch(ssock->proto) {
    case PJ_SSL_SOCK_PROTO_TLS1:
	proto.Set((const TUint8*)"TLS1.0", 6);
	break;
    case PJ_SSL_SOCK_PROTO_SSL3:
	proto.Set((const TUint8*)"SSL3.0", 6);
	break;
    default:
	return PJ_ENOTSUP;
    }

    /* Prepare addresses */
    TInetAddr localaddr_, remaddr_;
    status = PjSymbianOS::pj2Addr(*(pj_sockaddr*)localaddr, addr_len, 
				  localaddr_);
    if (status != PJ_SUCCESS)
	return status;
    
    status = PjSymbianOS::pj2Addr(*(pj_sockaddr*)remaddr, addr_len,
				  remaddr_);
    if (status != PJ_SUCCESS)
	return status;

    pj_sockaddr_cp((pj_sockaddr_t*)&ssock->rem_addr, remaddr);

    /* Init SSL engine */
    TRAPD(err, sock = CPjSSLSocket::NewL(proto, ssock->qos_type, 
				         ssock->qos_params));
    if (err != KErrNone)
	return PJ_ENOMEM;
    
    if (ssock->timeout.sec != 0 || ssock->timeout.msec != 0) {
	ssock->connect_timer = new CPjTimer(&ssock->timeout, 
					    &connect_timer_cb, ssock);
    }
    
    /* Convert server name to Symbian descriptor */
    TPtrC8 servername_((TUint8*)ssock->servername.ptr, 
		       ssock->servername.slen);
    
    /* Convert cipher list to Symbian descriptor */
    TPtrC8 ciphers_((TUint8*)ssock->ciphers.ptr, 
		    ssock->ciphers.slen);
    
    /* Try to connect */
    status = sock->Connect(&connect_cb, ssock, localaddr_, remaddr_,
			   servername_, ciphers_);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	delete sock;
	return status;
    }

    ssock->sock = sock;
    return status;
}


PJ_DEF(pj_status_t) pj_ssl_sock_renegotiate(pj_ssl_sock_t *ssock)
{
    PJ_UNUSED_ARG(ssock);
    return PJ_ENOTSUP;
}
