# $Id: pjsua.py 2976 2009-10-29 08:16:46Z bennylp $
#
# Object oriented PJSUA wrapper.
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
#

"""Multimedia communication client library based on SIP protocol.

This implements a fully featured multimedia communication client 
library based on PJSIP stack (http://www.pjsip.org)


1. FEATURES

  - Session Initiation Protocol (SIP) features:
     - Basic registration and call
     - Multiple accounts
     - Call hold, attended and unattended call transfer
     - Presence
     - Instant messaging
     - Multiple SIP accounts
  - Media features:
     - Audio
     - Conferencing
     - Narrowband and wideband
     - Codecs: PCMA, PCMU, GSM, iLBC, Speex, G.722, L16
     - RTP/RTCP
     - Secure RTP (SRTP)
     - WAV playback, recording, and playlist
  - NAT traversal features
     - Symmetric RTP
     - STUN
     - TURN
     - ICE
 

2. USING

See http://www.pjsip.org/trac/wiki/Python_SIP_Tutorial for a more thorough
tutorial. The paragraphs below explain basic tasks on using this module.


"""
import _pjsua
import thread
import threading
import weakref
import time

class Error:
    """Error exception class.
    
    Member documentation:

    op_name  -- name of the operation that generated this error.
    obj      -- the object that generated this error.
    err_code -- the error code.

    """
    op_name = ""
    obj = None
    err_code = -1
    _err_msg = ""

    def __init__(self, op_name, obj, err_code, err_msg=""):
        self.op_name = op_name
        self.obj = obj
        self.err_code = err_code
        self._err_msg = err_msg

    def err_msg(self):
        "Retrieve the description of the error."
        if self._err_msg != "":
            return self._err_msg
        self._err_msg = Lib.strerror(self.err_code)
        return self._err_msg

    def __str__(self):
        return "Object: " + str(self.obj) + ", operation=" + self.op_name + \
               ", error=" + self.err_msg()

# 
# Constants
#

class TransportType:
    """SIP transport type constants.
    
    Member documentation:
    UNSPECIFIED -- transport type is unknown or unspecified
    UDP         -- UDP transport
    TCP         -- TCP transport
    TLS         -- TLS transport
    IPV6        -- this is not a transport type but rather a flag
                   to select the IPv6 version of a transport
    UDP_IPV6    -- IPv6 UDP transport
    TCP_IPV6    -- IPv6 TCP transport
    """
    UNSPECIFIED = 0
    UDP = 1
    TCP = 2
    TLS = 3
    IPV6 = 128
    UDP_IPV6 = UDP + IPV6
    TCP_IPV6 = TCP + IPV6

class TransportFlag:
    """Transport flags to indicate the characteristics of the transport.
    
    Member documentation:
    
    RELIABLE    -- transport is reliable.
    SECURE      -- transport is secure.
    DATAGRAM    -- transport is datagram based.
    
    """
    RELIABLE = 1
    SECURE = 2
    DATAGRAM = 4

class CallRole:
    """Call role constants.
    
    Member documentation:

    CALLER  -- role is caller
    CALLEE  -- role is callee

    """
    CALLER = 0
    CALLEE = 1

class CallState:
    """Call state constants.
    
    Member documentation:

    NULL            -- call is not initialized.
    CALLING         -- initial INVITE is sent.
    INCOMING        -- initial INVITE is received.
    EARLY           -- provisional response has been sent or received.
    CONNECTING      -- 200/OK response has been sent or received.
    CONFIRMED       -- ACK has been sent or received.
    DISCONNECTED    -- call is disconnected.
    """
    NULL = 0
    CALLING = 1
    INCOMING = 2
    EARLY = 3
    CONNECTING = 4
    CONFIRMED = 5
    DISCONNECTED = 6


class MediaState:
    """Call media state constants.
    
    Member documentation:

    NULL        -- media is not available.
    ACTIVE      -- media is active.
    LOCAL_HOLD  -- media is put on-hold by local party.
    REMOTE_HOLD -- media is put on-hold by remote party.
    ERROR       -- media error (e.g. ICE negotiation failure).
    """
    NULL = 0
    ACTIVE = 1
    LOCAL_HOLD = 2
    REMOTE_HOLD = 3
    ERROR = 4


class MediaDir:
    """Media direction constants.
    
    Member documentation:

    NULL              -- media is not active
    ENCODING          -- media is active in transmit/encoding direction only.
    DECODING          -- media is active in receive/decoding direction only
    ENCODING_DECODING -- media is active in both directions.
    """
    NULL = 0
    ENCODING = 1
    DECODING = 2
    ENCODING_DECODING = 3


class PresenceActivity:
    """Presence activities constants.
    
    Member documentation:

    UNKNOWN -- the person activity is unknown
    AWAY    -- the person is currently away
    BUSY    -- the person is currently engaging in other activity
    """
    UNKNOWN = 0
    AWAY = 1
    BUSY = 2


class SubscriptionState:
    """Presence subscription state constants.

    """
    NULL = 0
    SENT = 1
    ACCEPTED = 2
    PENDING = 3
    ACTIVE = 4
    TERMINATED = 5
    UNKNOWN = 6


class TURNConnType:
    """These constants specifies the connection type to TURN server.
    
    Member documentation:
    UDP     -- use UDP transport.
    TCP     -- use TCP transport.
    TLS     -- use TLS transport.
    """
    UDP = 17
    TCP = 6
    TLS = 255


class UAConfig:
    """User agent configuration to be specified in Lib.init().
    
    Member documentation:

    max_calls   -- maximum number of calls to be supported.
    nameserver  -- list of nameserver hostnames or IP addresses. Nameserver
                   must be configured if DNS SRV resolution is desired.
    stun_domain -- if nameserver is configured, this can be used to query
                   the STUN server with DNS SRV.
    stun_host   -- the hostname or IP address of the STUN server. This will
                   also be used if DNS SRV resolution for stun_domain fails.
    user_agent  -- Optionally specify the user agent name.
    """
    max_calls = 4
    nameserver = []
    stun_domain = ""
    stun_host = ""
    user_agent = "pjsip python"
    
    def _cvt_from_pjsua(self, cfg):
        self.max_calls = cfg.max_calls
        self.thread_cnt = cfg.thread_cnt
        self.nameserver = cfg.nameserver
        self.stun_domain = cfg.stun_domain
        self.stun_host = cfg.stun_host
        self.user_agent = cfg.user_agent

    def _cvt_to_pjsua(self):
        cfg = _pjsua.config_default()
        cfg.max_calls = self.max_calls
        cfg.thread_cnt = 0
        cfg.nameserver = self.nameserver
        cfg.stun_domain = self.stun_domain
        cfg.stun_host = self.stun_host
        cfg.user_agent = self.user_agent
        return cfg


class LogConfig:
    """Logging configuration to be specified in Lib.init().
    
    Member documentation:

    msg_logging   -- specify if SIP messages should be logged. Set to
                     True.
    level         -- specify the input verbosity level.
    console_level -- specify the output verbosity level.
    decor         -- specify log decoration.
    filename      -- specify the log filename.
    callback      -- specify callback to be called to write the logging
                     messages. Sample function:

                     def log_cb(level, str, len):
                        print str,

    """
    msg_logging = True
    level = 5
    console_level = 5
    decor = 0
    filename = ""
    callback = None
    
    def __init__(self, level=-1, filename="", callback=None,
                 console_level=-1):
        self._cvt_from_pjsua(_pjsua.logging_config_default())
        if level != -1:
            self.level = level
        if filename != "":
            self.filename = filename
        if callback != None:
            self.callback = callback
        if console_level != -1:
            self.console_level = console_level

    def _cvt_from_pjsua(self, cfg):
        self.msg_logging = cfg.msg_logging
        self.level = cfg.level
        self.console_level = cfg.console_level
        self.decor = cfg.decor
        self.filename = cfg.log_filename
        self.callback = cfg.cb

    def _cvt_to_pjsua(self):
        cfg = _pjsua.logging_config_default()
        cfg.msg_logging = self.msg_logging
        cfg.level = self.level
        cfg.console_level = self.console_level
        cfg.decor = self.decor
        cfg.log_filename = self.filename
        cfg.cb = self.callback
        return cfg


class MediaConfig:
    """Media configuration to be specified in Lib.init().
    
    Member documentation:
    
    clock_rate          -- specify the core clock rate of the audio,
                           most notably the conference bridge.
    snd_clock_rate      -- optionally specify different clock rate for
                           the sound device.
    snd_auto_close_time -- specify the duration in seconds when the
                           sound device should be closed after inactivity
                           period.
    channel_count       -- specify the number of channels to open the sound
                           device and the conference bridge.
    audio_frame_ptime   -- specify the length of audio frames in millisecond.
    max_media_ports     -- specify maximum number of audio ports to be
                           supported by the conference bridge.
    quality             -- specify the audio quality setting (1-10)
    ptime               -- specify the audio packet length of transmitted
                           RTP packet.
    no_vad              -- disable Voice Activity Detector (VAD) or Silence
                           Detector (SD)
    ilbc_mode           -- specify iLBC codec mode (must be 30 for now)
    tx_drop_pct         -- randomly drop transmitted RTP packets (for
                           simulation). Number is in percent.
    rx_drop_pct         -- randomly drop received RTP packets (for
                           simulation). Number is in percent.
    ec_options          -- Echo Canceller option (specify zero).
    ec_tail_len         -- specify Echo Canceller tail length in milliseconds.
                           Value zero will disable the echo canceller.
    jb_min              -- specify the minimum jitter buffer size in
                           milliseconds. Put -1 for default.
    jb_max              -- specify the maximum jitter buffer size in
                           milliseconds. Put -1 for default.
    enable_ice          -- enable Interactive Connectivity Establishment (ICE)
    enable_turn         -- enable TURN relay. TURN server settings must also
                           be configured.
    turn_server         -- specify the domain or hostname or IP address of
                           the TURN server, in "host[:port]" format.
    turn_conn_type      -- specify connection type to the TURN server, from
                           the TURNConnType constant.
    turn_cred           -- specify AuthCred for the TURN credential.
    """
    clock_rate = 16000
    snd_clock_rate = 0
    snd_auto_close_time = 5
    channel_count = 1
    audio_frame_ptime = 20
    max_media_ports = 32
    quality = 6
    ptime = 0
    no_vad = False
    ilbc_mode = 30
    tx_drop_pct = 0
    rx_drop_pct = 0
    ec_options = 0
    ec_tail_len = 256
    jb_min = -1
    jb_max = -1
    enable_ice = True
    enable_turn = False
    turn_server = ""
    turn_conn_type = TURNConnType.UDP
    turn_cred = None
     
    def __init__(self):
        default = _pjsua.media_config_default()
        self._cvt_from_pjsua(default)

    def _cvt_from_pjsua(self, cfg):
        self.clock_rate = cfg.clock_rate
        self.snd_clock_rate = cfg.snd_clock_rate
        self.snd_auto_close_time = cfg.snd_auto_close_time
        self.channel_count = cfg.channel_count
        self.audio_frame_ptime = cfg.audio_frame_ptime
        self.max_media_ports = cfg.max_media_ports
        self.quality = cfg.quality
        self.ptime = cfg.ptime
        self.no_vad = cfg.no_vad
        self.ilbc_mode = cfg.ilbc_mode
        self.tx_drop_pct = cfg.tx_drop_pct
        self.rx_drop_pct = cfg.rx_drop_pct
        self.ec_options = cfg.ec_options
        self.ec_tail_len = cfg.ec_tail_len
        self.jb_min = cfg.jb_min
        self.jb_max = cfg.jb_max
        self.enable_ice = cfg.enable_ice
        self.enable_turn = cfg.enable_turn
        self.turn_server = cfg.turn_server
        self.turn_conn_type = cfg.turn_conn_type
        if cfg.turn_username:
            self.turn_cred = AuthCred(cfg.turn_realm, cfg.turn_username,
                                      cfg.turn_passwd, cfg.turn_passwd_type)
        else:
            self.turn_cred = None

    def _cvt_to_pjsua(self):
        cfg = _pjsua.media_config_default()
        cfg.clock_rate = self.clock_rate
        cfg.snd_clock_rate = self.snd_clock_rate
        cfg.snd_auto_close_time = self.snd_auto_close_time
        cfg.channel_count = self.channel_count
        cfg.audio_frame_ptime = self.audio_frame_ptime
        cfg.max_media_ports = self.max_media_ports
        cfg.quality = self.quality
        cfg.ptime = self.ptime
        cfg.no_vad = self.no_vad
        cfg.ilbc_mode = self.ilbc_mode
        cfg.tx_drop_pct = self.tx_drop_pct
        cfg.rx_drop_pct = self.rx_drop_pct
        cfg.ec_options = self.ec_options
        cfg.ec_tail_len = self.ec_tail_len
        cfg.jb_min = self.jb_min
        cfg.jb_max = self.jb_max
        cfg.enable_ice = self.enable_ice
        cfg.enable_turn = self.enable_turn
        cfg.turn_server = self.turn_server
        cfg.turn_conn_type = self.turn_conn_type
        if self.turn_cred:
            cfg.turn_realm = self.turn_cred.realm
            cfg.turn_username = self.turn_cred.username
            cfg.turn_passwd_type = self.turn_cred.passwd_type
            cfg.turn_passwd = self.turn_cred.passwd
        return cfg


class TransportConfig:
    """SIP transport configuration class.
    
    Member configuration:

    port        -- port number.
    bound_addr  -- optionally specify the address to bind the socket to.
                   Default is empty to bind to INADDR_ANY.
    public_addr -- optionally override the published address for this
                   transport. If empty, the default behavior is to get
                   the public address from STUN or from the selected
                   local interface. Format is "host:port".
    """
    port = 0
    bound_addr = ""
    public_addr = ""

    def __init__(self, port=0, 
                 bound_addr="", public_addr=""):
        self.port = port
        self.bound_addr = bound_addr
        self.public_addr = public_addr

    def _cvt_to_pjsua(self):
        cfg = _pjsua.transport_config_default()
        cfg.port = self.port
        cfg.bound_addr = self.bound_addr
        cfg.public_addr = self.public_addr
        return cfg


class TransportInfo:
    """SIP transport info.
    
    Member documentation:

    type        -- transport type, from TransportType constants.
    description -- longer description for this transport.
    is_reliable -- True if transport is reliable.
    is_secure   -- True if transport is secure.
    is_datagram -- True if transport is datagram based.
    host        -- the IP address of this transport.
    port        -- the port number.
    ref_cnt     -- number of objects referencing this transport.
    """
    type = ""
    description = ""
    is_reliable = False
    is_secure = False
    is_datagram = False
    host = ""
    port = 0
    ref_cnt = 0
    
    def __init__(self, ti):
        self.type = ti.type_name
        self.description = ti.info
        self.is_reliable = (ti.flag & TransportFlag.RELIABLE)
        self.is_secure = (ti.flag & TransportFlag.SECURE)
        self.is_datagram = (ti.flag & TransportFlag.DATAGRAM)
        self.host = ti.addr
        self.port = ti.port
        self.ref_cnt = ti.usage_count
    
    
class Transport:
    "SIP transport class."
    _id = -1
    _lib = None
    _obj_name = ""

    def __init__(self, lib, id):
        self._lib = weakref.proxy(lib)
        self._id = id
        self._obj_name = "{Transport " + self.info().description + "}"
        _Trace((self, 'created'))

    def __del__(self):
        _Trace((self, 'destroyed'))
        
    def __str__(self):
        return self._obj_name

    def info(self):
        """Get TransportInfo.
        """
        lck = self._lib.auto_lock()
        ti = _pjsua.transport_get_info(self._id)
        if not ti:
            self._lib._err_check("info()", self, -1, "Invalid transport")
        return TransportInfo(ti)

    def enable(self):
        """Enable this transport."""
        lck = self._lib.auto_lock()
        err = _pjsua.transport_set_enable(self._id, True)
        self._lib._err_check("enable()", self, err)

    def disable(self):
        """Disable this transport."""
        lck = self._lib.auto_lock()
        err = _pjsua.transport_set_enable(self._id, 0)
        self._lib._err_check("disable()", self, err)

    def close(self, force=False):
        """Close and destroy this transport.

        Keyword argument:
        force   -- force deletion of this transport (not recommended).
        """
        lck = self._lib.auto_lock()
        err = _pjsua.transport_close(self._id, force)
        self._lib._err_check("close()", self, err)


class SIPUri:
    """Helper class to parse the most important components of SIP URI.

    Member documentation:

    scheme    -- URI scheme ("sip" or "sips")
    user      -- user part of the URI (may be empty)
    host      -- host name part
    port      -- optional port number (zero if port is not specified).
    transport -- transport parameter, or empty if transport is not
                 specified.

    """
    scheme = ""
    user = ""
    host = ""
    port = 0
    transport = ""

    def __init__(self, uri=None):
        if uri:
            self.decode(uri)

    def decode(self, uri):
        """Parse SIP URL.

        Keyword argument:
        uri -- the URI string.

        """
        self.scheme, self.user, self.host, self.port, self.transport = \
            _pjsua.parse_simple_uri(uri)

    def encode(self):
        """Encode this object into SIP URI string.

        Return:
            URI string.

        """
        output = self.scheme + ":"
        if self.user and len(self.user):
            output = output + self.user + "@"
        output = output + self.host
        if self.port:
            output = output + ":" + output(self.port)
        if self.transport:
            output = output + ";transport=" + self.transport
        return output


class AuthCred:
    """Authentication credential for SIP or TURN account.
    
    Member documentation:

    scheme      -- authentication scheme (default is "Digest")
    realm       -- realm
    username    -- username
    passwd_type -- password encoding (zero for plain-text)
    passwd      -- the password
    """
    scheme = "Digest"
    realm = "*"
    username = ""
    passwd_type = 0
    passwd = ""

    def __init__(self, realm, username, passwd, scheme="Digest", passwd_type=0):
        self.scheme = scheme
        self.realm = realm
        self.username = username
        self.passwd_type = passwd_type
        self.passwd = passwd


class AccountConfig:
    """ This describes account configuration to create an account.

    Member documentation:

    priority                -- account priority for matching incoming
                               messages.
    id                      -- SIP URI of this account. This setting is
                               mandatory.
    force_contact           -- force to use this URI as Contact URI. Setting
                               this value is generally not recommended.
    reg_uri                 -- specify the registrar URI. Mandatory if
                               registration is required.
    reg_timeout             -- specify the SIP registration refresh interval
                               in seconds.
    require_100rel          -- specify if reliable provisional response is
                               to be enforced (with Require header).
    publish_enabled         -- specify if PUBLISH should be used. When
                               enabled, the PUBLISH will be sent to the
                               registrar.
    pidf_tuple_id           -- optionally specify the tuple ID in outgoing
                               PIDF document.
    proxy                   -- list of proxy URI.
    auth_cred               -- list of AuthCred containing credentials to
                               authenticate against the registrars and
                               the proxies.
    auth_initial_send       -- specify if empty Authorization header should be
                               sent. May be needed for IMS.
    auth_initial_algorithm  -- when auth_initial_send is enabled, optionally
                               specify the authentication algorithm to use.
                               Valid values are "md5", "akav1-md5", or
                               "akav2-md5". 
    transport_id            -- optionally specify the transport ID to be used
                               by this account. Shouldn't be needed unless
                               for specific requirements (e.g. in multi-homed
                               scenario).
    allow_contact_rewrite   -- specify whether the account should learn its
                               Contact address from REGISTER response and 
                               update the registration accordingly. Default is
                               True.
    ka_interval             -- specify the interval to send NAT keep-alive 
                               packet.
    ka_data                 -- specify the NAT keep-alive packet contents.
    use_srtp                -- specify the SRTP usage policy. Valid values
                               are: 0=disable, 1=optional, 2=mandatory.
                               Default is 0.
    srtp_secure_signaling   -- specify the signaling security level required
                               by SRTP. Valid values are: 0=no secure 
                               transport is required, 1=hop-by-hop secure
                               transport such as TLS is required, 2=end-to-
                               end secure transport is required (i.e. "sips").
    """
    priority = 0
    id = ""
    force_contact = ""
    reg_uri = ""
    reg_timeout = 0
    require_100rel = False
    publish_enabled = False
    pidf_tuple_id = ""
    proxy = []
    auth_cred = []
    auth_initial_send = False
    auth_initial_algorithm = ""
    transport_id = -1
    allow_contact_rewrite = True
    ka_interval = 15
    ka_data = "\r\n"
    use_srtp = 0
    srtp_secure_signaling = 1

    def __init__(self, domain="", username="", password="", 
                 display="", registrar="", proxy=""):
        """
        Construct account config. If domain argument is specified, 
        a typical configuration will be built.

        Keyword arguments:
        domain    -- domain name of the server.
        username  -- user name.
        password  -- plain-text password.
        display   -- optional display name for the user name.
        registrar -- the registrar URI. If domain name is specified
                     and this argument is empty, the registrar URI
                     will be constructed from the domain name.
        proxy     -- the proxy URI. If domain name is specified
                     and this argument is empty, the proxy URI
                     will be constructed from the domain name.

        """
        default = _pjsua.acc_config_default()
        self._cvt_from_pjsua(default)
        if domain!="":
            self.build_config(domain, username, password,
                              display, registrar, proxy)

    def build_config(self, domain, username, password, display="",
                     registrar="", proxy=""):
        """
        Construct account config. If domain argument is specified, 
        a typical configuration will be built.

        Keyword arguments:
        domain    -- domain name of the server.
        username  -- user name.
        password  -- plain-text password.
        display   -- optional display name for the user name.
        registrar -- the registrar URI. If domain name is specified
                     and this argument is empty, the registrar URI
                     will be constructed from the domain name.
        proxy     -- the proxy URI. If domain name is specified
                     and this argument is empty, the proxy URI
                     will be constructed from the domain name.

        """
        if display != "":
            display = display + " "
        userpart = username
        if userpart != "":
            userpart = userpart + "@"
        self.id = display + "<sip:" + userpart + domain + ">"
        self.reg_uri = registrar
        if self.reg_uri == "":
            self.reg_uri = "sip:" + domain
        if proxy == "":
            proxy = "sip:" + domain + ";lr"
        if proxy.find(";lr") == -1:
            proxy = proxy + ";lr"
        self.proxy.append(proxy)
        if username != "":
            self.auth_cred.append(AuthCred("*", username, password))
    
    def _cvt_from_pjsua(self, cfg):
        self.priority = cfg.priority
        self.id = cfg.id
        self.force_contact = cfg.force_contact
        self.reg_uri = cfg.reg_uri
        self.reg_timeout = cfg.reg_timeout
        self.require_100rel = cfg.require_100rel
        self.publish_enabled = cfg.publish_enabled
        self.pidf_tuple_id = cfg.pidf_tuple_id
        self.proxy = cfg.proxy
        for cred in cfg.cred_info:
            self.auth_cred.append(AuthCred(cred.realm, cred.username, 
                                           cred.data, cred.scheme,
                                           cred.data_type))
        self.auth_initial_send = cfg.auth_initial_send
        self.auth_initial_algorithm = cfg.auth_initial_algorithm
        self.transport_id = cfg.transport_id
        self.allow_contact_rewrite = cfg.allow_contact_rewrite
        self.ka_interval = cfg.ka_interval
        self.ka_data = cfg.ka_data
        self.use_srtp = cfg.use_srtp
        self.srtp_secure_signaling = cfg.srtp_secure_signaling

    def _cvt_to_pjsua(self):
        cfg = _pjsua.acc_config_default()
        cfg.priority = self.priority
        cfg.id = self.id
        cfg.force_contact = self.force_contact
        cfg.reg_uri = self.reg_uri
        cfg.reg_timeout = self.reg_timeout
        cfg.require_100rel = self.require_100rel
        cfg.publish_enabled = self.publish_enabled
        cfg.pidf_tuple_id = self.pidf_tuple_id
        cfg.proxy = self.proxy
        for cred in self.auth_cred:
            c = _pjsua.Pjsip_Cred_Info()
            c.realm = cred.realm
            c.scheme = cred.scheme
            c.username = cred.username
            c.data_type = cred.passwd_type
            c.data = cred.passwd
            cfg.cred_info.append(c)
        cfg.auth_initial_send = self.auth_initial_send
        cfg.auth_initial_algorithm = self.auth_initial_algorithm
        cfg.transport_id = self.transport_id
        cfg.allow_contact_rewrite = self.allow_contact_rewrite
        cfg.ka_interval = self.ka_interval
        cfg.ka_data = self.ka_data
        cfg.use_srtp = self.use_srtp
        cfg.srtp_secure_signaling = self.srtp_secure_signaling
        return cfg
 
 
# Account information
class AccountInfo:
    """This describes Account info. Application retrives account info
    with Account.info().

    Member documentation:

    is_default      -- True if this is the default account.
    uri             -- the account URI.
    reg_active      -- True if registration is active for this account.
    reg_expires     -- contains the current registration expiration value,
                       in seconds.
    reg_status      -- the registration status. If the value is less than
                       700, it specifies SIP status code. Value greater than
                       this specifies the error code.
    reg_reason      -- contains the registration status text (e.g. the
                       error message).
    online_status   -- the account's presence online status, True if it's 
                       publishing itself as online.
    online_text     -- the account's presence status text.

    """
    is_default = False
    uri = ""
    reg_active = False
    reg_expires = -1
    reg_status = 0
    reg_reason = ""
    online_status = False
    online_text = ""

    def __init__(self, ai):
        self.is_default = ai.is_default
        self.uri = ai.acc_uri
        self.reg_active = ai.has_registration
        self.reg_expires = ai.expires
        self.reg_status = ai.status
        self.reg_reason = ai.status_text
        self.online_status = ai.online_status
        self.online_text = ai.online_status_text

# Account callback
class AccountCallback:
    """Class to receive notifications on account's events.

    Derive a class from this class and register it to the Account object
    using Account.set_callback() to start receiving events from the Account
    object.

    Member documentation:

    account     -- the Account object.

    """
    account = None

    def __init__(self, account=None):
        self._set_account(account)

    def __del__(self):
        pass

    def _set_account(self, account):
        if account:
            self.account = weakref.proxy(account)
        else:
            self.account = None

    def on_reg_state(self):
        """Notification that the registration status has changed.
        """
        pass

    def on_incoming_call(self, call):
        """Notification about incoming call.

        Unless this callback is implemented, the default behavior is to
        reject the call with default status code.

        Keyword arguments:
        call    -- the new incoming call
        """
        call.hangup()

    def on_incoming_subscribe(self, buddy, from_uri, contact_uri, pres_obj):
        """Notification when incoming SUBSCRIBE request is received. 
        
        Application may use this callback to authorize the incoming 
        subscribe request (e.g. ask user permission if the request 
        should be granted)

        Keyword arguments:
        buddy       -- The buddy object, if buddy is found. Otherwise
                       the value is None.
        from_uri    -- The URI string of the sender.
        pres_obj    -- Opaque presence subscription object, which is
                       needed by Account.pres_notify()

        Return:
            Tuple (code, reason), where:
             code:      The status code. If code is >= 300, the
                        request is rejected. If code is 200, the
                        request is accepted and NOTIFY will be sent
                        automatically. If code is 202, application
                        must accept or reject the request later with
                        Account.press_notify().
             reason:    Optional reason phrase, or None to use the
                        default reasoh phrase for the status code.
        """
        return (200, None)

    def on_pager(self, from_uri, contact, mime_type, body):
        """
        Notification that incoming instant message is received on
        this account.

        Keyword arguments:
        from_uri   -- sender's URI
        contact    -- sender's Contact URI
        mime_type  -- MIME type of the instant message body
        body       -- the instant message body

        """
        pass

    def on_pager_status(self, to_uri, body, im_id, code, reason):
        """
        Notification about the delivery status of previously sent
        instant message.

        Keyword arguments:
        to_uri  -- the destination URI of the message
        body    -- the message body
        im_id   -- message ID
        code    -- SIP status code
        reason  -- SIP reason phrase

        """
        pass

    def on_typing(self, from_uri, contact, is_typing):
        """
        Notification that remote is typing or stop typing.

        Keyword arguments:
        buddy     -- Buddy object for the sender, if found. Otherwise
                     this will be None
        from_uri  -- sender's URI of the indication
        contact   -- sender's contact URI
        is_typing -- boolean to indicate whether remote is currently
                     typing an instant message.

        """
        pass

    def on_mwi_info(self, body):
        """
        Notification about change in Message Summary / Message Waiting
	Indication (RFC 3842) status. MWI subscription must be enabled
	in the account config to receive this notification.

        Keyword arguments:
        body      -- String containing message body as received in the
                     NOTIFY request.

        """
        pass



class Account:
    """This describes SIP account class.

    PJSUA accounts provide identity (or identities) of the user who is 
    currently using the application. In SIP terms, the identity is used 
    as the From header in outgoing requests.

    Account may or may not have client registration associated with it. 
    An account is also associated with route set and some authentication 
    credentials, which are used when sending SIP request messages using 
    the account. An account also has presence's online status, which 
    will be reported to remote peer when they subscribe to the account's 
    presence, or which is published to a presence server if presence 
    publication is enabled for the account.

    Account is created with Lib.create_account(). At least one account 
    MUST be created. If no user association is required, application can 
    create a userless account by calling Lib.create_account_for_transport().
    A userless account identifies local endpoint instead of a particular 
    user, and it correspond with a particular transport instance.

    Also one account must be set as the default account, which is used as 
    the account to use when PJSUA fails to match a request with any other
    accounts.

    """
    _id = -1        
    _lib = None
    _cb = AccountCallback(None)
    _obj_name = ""

    def __init__(self, lib, id, cb=None):
        """Construct this class. This is normally called by Lib class and
        not by application.

        Keyword arguments:
        lib -- the Lib instance.
        id  -- the pjsua account ID.
        cb  -- AccountCallback instance to receive events from this Account.
               If callback is not specified here, it must be set later
               using set_callback().
        """
        self._id = id
        self._lib = weakref.ref(lib)
        self._obj_name = "{Account " + self.info().uri + "}"
        self.set_callback(cb)
        _pjsua.acc_set_user_data(self._id, self)
        _Trace((self, 'created'))

    def __del__(self):
        if self._id != -1:
            _pjsua.acc_set_user_data(self._id, 0)
        _Trace((self, 'destroyed'))

    def __str__(self):
        return self._obj_name

    def info(self):
        """Retrieve AccountInfo for this account.
        """
        lck = self._lib().auto_lock()
        ai = _pjsua.acc_get_info(self._id)
        if ai==None:
            self._lib()._err_check("info()", self, -1, "Invalid account")
        return AccountInfo(ai)

    def is_valid(self):
        """
        Check if this account is still valid.

        """
        lck = self._lib().auto_lock()
        return _pjsua.acc_is_valid(self._id)

    def set_callback(self, cb):
        """Register callback to receive notifications from this object.

        Keyword argument:
        cb  -- AccountCallback instance.

        """
        if cb:
            self._cb = cb
        else:
            self._cb = AccountCallback(self)
        self._cb._set_account(self)

    def set_default(self):
        """ Set this account as default account to send outgoing requests
        and as the account to receive incoming requests when more exact
        matching criteria fails.
        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_default(self._id)
        self._lib()._err_check("set_default()", self, err)

    def is_default(self):
        """ Check if this account is the default account.

        """
        lck = self._lib().auto_lock()
        def_id = _pjsua.acc_get_default()
        return self.is_valid() and def_id==self._id

    def delete(self):
        """ Delete this account.
        
        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_user_data(self._id, 0)
        self._lib()._err_check("delete()", self, err)
        err = _pjsua.acc_del(self._id)
        self._lib()._err_check("delete()", self, err)
        self._id = -1

    def set_basic_status(self, is_online):
        """ Set basic presence status of this account.

        Keyword argument:
        is_online   -- boolean to indicate basic presence availability.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_online_status(self._id, is_online)
        self._lib()._err_check("set_basic_status()", self, err)

    def set_presence_status(self, is_online, 
                            activity=PresenceActivity.UNKNOWN, 
                            pres_text="", rpid_id=""):
        """ Set presence status of this account. 
        
        Keyword arguments:
        is_online   -- boolean to indicate basic presence availability
        activity    -- value from PresenceActivity
        pres_text   -- optional string to convey additional information about
                       the activity (such as "On the phone")
        rpid_id     -- optional string to be placed as RPID ID. 

        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_online_status2(self._id, is_online, activity,
                                            pres_text, rpid_id)
        self._lib()._err_check("set_presence_status()", self, err)

    def set_registration(self, renew):
        """Manually renew registration or unregister from the server.

        Keyword argument:
        renew   -- boolean to indicate whether registration is renewed.
                   Setting this value for False will trigger unregistration.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_registration(self._id, renew)
        self._lib()._err_check("set_registration()", self, err)

    def set_transport(self, transport):
        """Set this account to only use the specified transport to send
        outgoing requests.

        Keyword argument:
        transport   -- Transport object.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.acc_set_transport(self._id, transport._id)
        self._lib()._err_check("set_transport()", self, err)

    def make_call(self, dst_uri, cb=None, hdr_list=None):
        """Make outgoing call to the specified URI.

        Keyword arguments:
        dst_uri  -- Destination SIP URI.
        cb       -- CallCallback instance to be installed to the newly
                    created Call object. If this CallCallback is not
                    specified (i.e. None is given), it must be installed
                    later using call.set_callback().
        hdr_list -- Optional list of headers to be sent with outgoing
                    INVITE

        Return:
            Call instance.
        """
        lck = self._lib().auto_lock()
        call = Call(self._lib(), -1, cb)
        err, cid = _pjsua.call_make_call(self._id, dst_uri, 0, 
                                         call, Lib._create_msg_data(hdr_list))
        self._lib()._err_check("make_call()", self, err)
        call.attach_to_id(cid)
        return call

    def add_buddy(self, uri, cb=None):
        """Add new buddy.

        Keyword argument:
        uri     -- SIP URI of the buddy
        cb      -- BuddyCallback instance to be installed to the newly
                   created Buddy object. If this callback is not specified
                   (i.e. None is given), it must be installed later using
                   buddy.set_callback().

        Return:
            Buddy object
        """
        lck = self._lib().auto_lock()
        buddy_cfg = _pjsua.buddy_config_default()
        buddy_cfg.uri = uri
        buddy_cfg.subscribe = False
        err, buddy_id = _pjsua.buddy_add(buddy_cfg)
        self._lib()._err_check("add_buddy()", self, err)
        buddy = Buddy(self._lib(), buddy_id, self, cb)
        return buddy

    def pres_notify(self, pres_obj, state, reason="", hdr_list=None):
        """Send NOTIFY to inform account presence status or to terminate
        server side presence subscription.
        
        Keyword arguments:
        pres_obj    -- The subscription object from on_incoming_subscribe()
                       callback
        state       -- Subscription state, from SubscriptionState
        reason      -- Optional reason phrase.
        hdr_list    -- Optional header list.
        """
        lck = self._lib().auto_lock()
        _pjsua.acc_pres_notify(self._id, pres_obj, state, reason, 
                               Lib._create_msg_data(hdr_list))
    
    def send_pager(self, uri, text, im_id=0, content_type="text/plain", \
                   hdr_list=None):
        """Send instant message to arbitrary URI.

        Keyword arguments:
        text         -- Instant message to be sent
        uri          -- URI to send the Instant Message to.
        im_id        -- Optional instant message ID to identify this
                        instant message when delivery status callback
                        is called.
        content_type -- MIME type identifying the instant message
        hdr_list     -- Optional list of headers to be sent with the
                        request.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.im_send(self._id, uri, \
                             content_type, text, \
                             Lib._create_msg_data(hdr_list), \
                             im_id)
        self._lib()._err_check("send_pager()", self, err)

class CallCallback:
    """Class to receive event notification from Call objects. 

    Use Call.set_callback() method to install instance of this callback 
    class to receive event notifications from the call object.

    Member documentation:

    call    -- the Call object.

    """
    call = None

    def __init__(self, call=None):
        self._set_call(call)

    def __del__(self):
        pass

    def _set_call(self, call):
        if call:
            self.call = weakref.proxy(call)
        else:
            self.call = None

    def on_state(self):
        """Notification that the call's state has changed.

        """
        pass

    def on_media_state(self):
        """Notification that the call's media state has changed.

        """
        pass

    def on_dtmf_digit(self, digits):
        """Notification on incoming DTMF digits.

        Keyword argument:
        digits  -- string containing the received digits.

        """
        pass

    def on_transfer_request(self, dst, code):
        """Notification that call is being transfered by remote party. 

        Application can decide to accept/reject transfer request by returning
        code greater than or equal to 500. The default behavior is to accept 
        the transfer by returning 202.

        Keyword arguments:
        dst     -- string containing the destination URI
        code    -- the suggested status code to return to accept the request.

        Return:
        the callback should return 202 to accept the request, or 300-699 to
        reject the request.

        """
        return code

    def on_transfer_status(self, code, reason, final, cont):
        """
        Notification about the status of previous call transfer request. 

        Keyword arguments:
        code    -- SIP status code to indicate completion status.
        text    -- SIP status reason phrase.
        final   -- if True then this is a final status and no further
                   notifications will be sent for this call transfer
                   status.
        cont    -- suggested return value.

        Return:
        If the callback returns false then no further notification will
        be sent for the transfer request for this call.

        """
        return cont

    def on_replace_request(self, code, reason):
        """Notification when incoming INVITE with Replaces header is received. 

        Application may reject the request by returning value greather than
        or equal to 500. The default behavior is to accept the request.

        Keyword arguments:
        code    -- default status code to return
        reason  -- default reason phrase to return

        Return:
        The callback should return (code, reason) tuple.

        """
        return code, reason

    def on_replaced(self, new_call):
        """
        Notification that this call will be replaced with new_call. 
        After this callback is called, this call will be disconnected.

        Keyword arguments:
        new_call    -- the new call that will replace this call.
        """
        pass

    def on_pager(self, mime_type, body):
        """
        Notification that incoming instant message is received on
        this call.

        Keyword arguments:
        mime_type  -- MIME type of the instant message body.
        body       -- the instant message body.

        """
        pass

    def on_pager_status(self, body, im_id, code, reason):
        """
        Notification about the delivery status of previously sent
        instant message.

        Keyword arguments:
        body    -- message body
        im_id   -- message ID
        code    -- SIP status code
        reason  -- SIP reason phrase

        """
        pass

    def on_typing(self, is_typing):
        """
        Notification that remote is typing or stop typing.

        Keyword arguments:
        is_typing -- boolean to indicate whether remote is currently
                     typing an instant message.

        """
        pass


class CallInfo:
    """This structure contains various information about Call.

    Application may retrieve this information with Call.info().

    Member documentation:

    role            -- CallRole
    account         -- Account object.
    uri             -- SIP URI of local account.
    contact         -- local Contact URI.
    remote_uri      -- remote SIP URI.
    remote_contact  -- remote Contact URI
    sip_call_id     -- call's Call-ID identification
    state           -- CallState
    state_text      -- state text.
    last_code       -- last SIP status code
    last_reason     -- text phrase for last_code
    media_state     -- MediaState
    media_dir       -- MediaDir
    conf_slot       -- conference slot number for this call.
    call_time       -- call's connected duration in seconds.
    total_time      -- total call duration in seconds.
    """
    role = CallRole.CALLER
    account = None
    uri = ""
    contact = ""
    remote_uri = ""
    remote_contact = ""
    sip_call_id = ""
    state = CallState.NULL
    state_text = ""
    last_code = 0
    last_reason = ""
    media_state = MediaState.NULL
    media_dir = MediaDir.NULL
    conf_slot = -1
    call_time = 0
    total_time = 0

    def __init__(self, lib=None, ci=None):
        if lib and ci:
            self._cvt_from_pjsua(lib, ci)

    def _cvt_from_pjsua(self, lib, ci):
        self.role = ci.role
        self.account = lib._lookup_account(ci.acc_id)
        self.uri = ci.local_info
        self.contact = ci.local_contact
        self.remote_uri = ci.remote_info
        self.remote_contact = ci.remote_contact
        self.sip_call_id = ci.call_id
        self.state = ci.state
        self.state_text = ci.state_text
        self.last_code = ci.last_status
        self.last_reason = ci.last_status_text
        self.media_state = ci.media_status
        self.media_dir = ci.media_dir
        self.conf_slot = ci.conf_slot
        self.call_time = ci.connect_duration / 1000
        self.total_time = ci.total_duration / 1000


class Call:
    """This class represents SIP call.

    Application initiates outgoing call with Account.make_call(), and
    incoming calls are reported in AccountCallback.on_incoming_call().
    """
    _id = -1
    _cb = None
    _lib = None
    _obj_name = ""

    def __init__(self, lib, call_id, cb=None):
        self._lib = weakref.ref(lib)
        self.set_callback(cb)
        self.attach_to_id(call_id)
        _Trace((self, 'created'))

    def __del__(self):
        if self._id != -1:
            _pjsua.call_set_user_data(self._id, 0)
        _Trace((self, 'destroyed'))

    def __str__(self):
        return self._obj_name

    def attach_to_id(self, call_id):
        lck = self._lib().auto_lock()
        if self._id != -1:
            _pjsua.call_set_user_data(self._id, 0)
        self._id = call_id
        if self._id != -1:
            _pjsua.call_set_user_data(self._id, self)
            self._obj_name = "{Call " + self.info().remote_uri + "}"
        else:
            self._obj_name = "{Call object}"

    def set_callback(self, cb):
        """
        Set callback object to retrieve event notifications from this call.

        Keyword arguments:
        cb  -- CallCallback instance.
        """
        if cb:
            self._cb = cb
        else:
            self._cb = CallCallback(self)
        self._cb._set_call(self)

    def info(self):
        """
        Get the CallInfo.
        """
        lck = self._lib().auto_lock()
        ci = _pjsua.call_get_info(self._id)
        if not ci:
            self._lib()._err_check("info", self, -1, "Invalid call")
        call_info = CallInfo(self._lib(), ci)
        return call_info

    def is_valid(self):
        """
        Check if this call is still valid.
        """
        lck = self._lib().auto_lock()
        return _pjsua.call_is_active(self._id)

    def dump_status(self, with_media=True, indent="", max_len=1024):
        """
        Dump the call status.
        """
        lck = self._lib().auto_lock()
        return _pjsua.call_dump(self._id, with_media, max_len, indent)

    def answer(self, code=200, reason="", hdr_list=None):
        """
        Send provisional or final response to incoming call.

        Keyword arguments:
        code     -- SIP status code.
        reason   -- Reason phrase. Put empty to send default reason
                    phrase for the status code.
        hdr_list -- Optional list of headers to be sent with the
                    INVITE response.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_answer(self._id, code, reason, 
                                   Lib._create_msg_data(hdr_list))
        self._lib()._err_check("answer()", self, err)

    def hangup(self, code=603, reason="", hdr_list=None):
        """
        Terminate the call.

        Keyword arguments:
        code     -- SIP status code.
        reason   -- Reason phrase. Put empty to send default reason
                    phrase for the status code.
        hdr_list -- Optional list of headers to be sent with the
                    message.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_hangup(self._id, code, reason, 
                                   Lib._create_msg_data(hdr_list))
        self._lib()._err_check("hangup()", self, err)

    def hold(self, hdr_list=None):
        """
        Put the call on hold.

        Keyword arguments:
        hdr_list -- Optional list of headers to be sent with the
                    message.
        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_set_hold(self._id, Lib._create_msg_data(hdr_list))
        self._lib()._err_check("hold()", self, err)

    def unhold(self, hdr_list=None):
        """
        Release the call from hold.

        Keyword arguments:
        hdr_list -- Optional list of headers to be sent with the
                    message.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_reinvite(self._id, True, 
                                     Lib._create_msg_data(hdr_list))
        self._lib()._err_check("unhold()", self, err)

    def reinvite(self, hdr_list=None):
        """
        Send re-INVITE and optionally offer new codecs to use.

        Keyword arguments:
        hdr_list   -- Optional list of headers to be sent with the
                      message.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_reinvite(self._id, True, 
                                     Lib._create_msg_data(hdr_list))
        self._lib()._err_check("reinvite()", self, err)

    def update(self, hdr_list=None, options=0):
        """
        Send UPDATE and optionally offer new codecs to use.

        Keyword arguments:
        hdr_list   -- Optional list of headers to be sent with the
                      message.
        options    -- Must be zero for now.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_update(self._id, options, 
                                   Lib._create_msg_data(hdr_list))
        self._lib()._err_check("update()", self, err)

    def transfer(self, dest_uri, hdr_list=None):
        """
        Transfer the call to new destination.

        Keyword arguments:
        dest_uri -- Specify the SIP URI to transfer the call to.
        hdr_list -- Optional list of headers to be sent with the
                    message.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_xfer(self._id, dest_uri, 
                                 Lib._create_msg_data(hdr_list))
        self._lib()._err_check("transfer()", self, err)

    def transfer_to_call(self, call, hdr_list=None, options=0):
        """
        Attended call transfer.

        Keyword arguments:
        call     -- The Call object to transfer call to.
        hdr_list -- Optional list of headers to be sent with the
                    message.
        options  -- Must be zero for now.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_xfer_replaces(self._id, call._id, options,
                                          Lib._create_msg_data(hdr_list))
        self._lib()._err_check("transfer_to_call()", self, err)

    def dial_dtmf(self, digits):
        """
        Send DTMF digits with RTP event package.

        Keyword arguments:
        digits  -- DTMF digit string.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_dial_dtmf(self._id, digits)
        self._lib()._err_check("dial_dtmf()", self, err)

    def send_request(self, method, hdr_list=None, content_type=None,
                     body=None):
        """
        Send arbitrary request to remote call. 
        
        This is useful for example to send INFO request. Note that this 
        function should not be used to send request that will change the 
        call state such as CANCEL or BYE.

        Keyword arguments:
        method       -- SIP method name.
        hdr_list     -- Optional header list to be sent with the request.
        content_type -- Content type to describe the body, if the body
                        is present
        body         -- Optional SIP message body.

        """
        lck = self._lib().auto_lock()
        if hdr_list or body:
            msg_data = _pjsua.Msg_Data()
            if hdr_list:
                msg_data.hdr_list = hdr_list
            if content_type:
                msg_data.content_type = content_type
            if body:
                msg_data.msg_body = body
        else:
            msg_data = None
                
        err = _pjsua.call_send_request(self._id, method, msg_data)
        self._lib()._err_check("send_request()", self, err)

    def send_pager(self, text, im_id=0, content_type="text/plain", 
    		   hdr_list=None):
        """Send instant message inside a call.

        Keyword arguments:
        text         -- Instant message to be sent
        im_id        -- Optional instant message ID to identify this
                        instant message when delivery status callback
                        is called.
        content_type -- MIME type identifying the instant message
        hdr_list     -- Optional list of headers to be sent with the
                        request.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.call_send_im(self._id, \
                             content_type, text, \
                             Lib._create_msg_data(hdr_list), \
                             im_id)
        self._lib()._err_check("send_pager()", self, err)

  
class BuddyInfo:
    """This class contains information about Buddy. Application may 
    retrieve this information by calling Buddy.info().

    Member documentation:

    uri             -- the Buddy URI.
    contact         -- the Buddy Contact URI, if available.
    online_status   -- the presence online status.
    online_text     -- the presence online status text.
    activity        -- the PresenceActivity
    subscribed      -- specify whether buddy's presence status is currently
                       being subscribed.
    sub_state       -- SubscriptionState
    sub_term_reason -- The termination reason string of the last presence
                       subscription to this buddy, if any.
    """
    uri = ""
    contact = ""
    online_status = 0
    online_text = ""
    activity = PresenceActivity.UNKNOWN
    subscribed = False
    sub_state = SubscriptionState.NULL
    sub_term_reason = ""

    def __init__(self, pjsua_bi=None):
        if pjsua_bi:
            self._cvt_from_pjsua(pjsua_bi)

    def _cvt_from_pjsua(self, inf):
        self.uri = inf.uri
        self.contact = inf.contact
        self.online_status = inf.status
        self.online_text = inf.status_text
        self.activity = inf.activity
        self.subscribed = inf.monitor_pres
        self.sub_state = inf.sub_state
        self.sub_term_reason = inf.sub_term_reason


class BuddyCallback:
    """This class can be used to receive notifications about Buddy's
    presence status change. Application needs to derive a class from
    this class, and register the instance with Buddy.set_callback().

    Member documentation:

    buddy   -- the Buddy object.
    """
    buddy = None

    def __init__(self, buddy=None):
        self._set_buddy(buddy)

    def _set_buddy(self, buddy):
        if buddy:
            self.buddy = weakref.proxy(buddy)
        else:
            self.buddy = None

    def on_state(self):
        """
        Notification that buddy's presence state has changed. Application
        may then retrieve the new status with Buddy.info() function.
        """
        pass
   
    def on_pager(self, mime_type, body):
        """Notification that incoming instant message is received from
        this buddy.

        Keyword arguments:
        mime_type  -- MIME type of the instant message body
        body       -- the instant message body

        """
        pass

    def on_pager_status(self, body, im_id, code, reason):
        """Notification about the delivery status of previously sent
        instant message.

        Keyword arguments:
        body    -- the message body
        im_id   -- message ID
        code    -- SIP status code
        reason  -- SIP reason phrase

        """
        pass

    def on_typing(self, is_typing):
        """Notification that remote is typing or stop typing.

        Keyword arguments:
        is_typing -- boolean to indicate whether remote is currently
                     typing an instant message.

        """
        pass


class Buddy:
    """A Buddy represents person or remote agent.

    This class provides functions to subscribe to buddy's presence and
    to send or receive instant messages from the buddy.
    """
    _id = -1
    _lib = None
    _cb = None
    _obj_name = ""
    _acc = None

    def __init__(self, lib, id, account, cb):
        self._id = id
        self._lib = weakref.ref(lib)
        self._acc = weakref.ref(account)
        self._obj_name = "{Buddy " + self.info().uri + "}"
        self.set_callback(cb)
        _pjsua.buddy_set_user_data(self._id, self)
        _Trace((self, 'created'))

    def __del__(self):
        if self._id != -1:
            _pjsua.buddy_set_user_data(self._id, 0)
        _Trace((self, 'destroyed'))

    def __str__(self):
        return self._obj_name

    def info(self):
        """
        Get buddy info as BuddyInfo.
        """
        lck = self._lib().auto_lock()
        return BuddyInfo(_pjsua.buddy_get_info(self._id))

    def set_callback(self, cb):
        """Install callback to receive notifications from this object.

        Keyword argument:
        cb  -- BuddyCallback instance.
        """
        if cb:
            self._cb = cb
        else:
            self._cb = BuddyCallback(self)
        self._cb._set_buddy(self)

    def subscribe(self):
        """
        Subscribe to buddy's presence status notification.
        """
        lck = self._lib().auto_lock()
        err = _pjsua.buddy_subscribe_pres(self._id, True)
        self._lib()._err_check("subscribe()", self, err)

    def unsubscribe(self):
        """
        Unsubscribe from buddy's presence status notification.
        """
        lck = self._lib().auto_lock()
        err = _pjsua.buddy_subscribe_pres(self._id, False)
        self._lib()._err_check("unsubscribe()", self, err)

    def delete(self):
        """
        Remove this buddy from the buddy list.
        """
        lck = self._lib().auto_lock()
        if self._id != -1:
            _pjsua.buddy_set_user_data(self._id, 0)
        err = _pjsua.buddy_del(self._id)
        self._lib()._err_check("delete()", self, err)

    def send_pager(self, text, im_id=0, content_type="text/plain", \
                   hdr_list=None):
        """Send instant message to remote buddy.

        Keyword arguments:
        text         -- Instant message to be sent
        im_id        -- Optional instant message ID to identify this
                        instant message when delivery status callback
                        is called.
        content_type -- MIME type identifying the instant message
        hdr_list     -- Optional list of headers to be sent with the
                        request.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.im_send(self._acc()._id, self.info().uri, \
                             content_type, text, \
                             Lib._create_msg_data(hdr_list), \
                             im_id)
        self._lib()._err_check("send_pager()", self, err)

    def send_typing_ind(self, is_typing=True, hdr_list=None):
        """Send typing indication to remote buddy.

        Keyword argument:
        is_typing -- boolean to indicate wheter user is typing.
        hdr_list  -- Optional list of headers to be sent with the
                     request.

        """
        lck = self._lib().auto_lock()
        err = _pjsua.im_typing(self._acc()._id, self.info().uri, \
                               is_typing, Lib._create_msg_data(hdr_list))
        self._lib()._err_check("send_typing_ind()", self, err)



# Sound device info
class SoundDeviceInfo:
    """This described the sound device info.

    Member documentation:
    name                -- device name.
    input_channels      -- number of capture channels supported.
    output_channels     -- number of playback channels supported.
    default_clock_rate  -- default sampling rate.
    """
    name = ""
    input_channels = 0
    output_channels = 0
    default_clock_rate = 0

    def __init__(self, sdi):
        self.name = sdi.name
        self.input_channels = sdi.input_count
        self.output_channels = sdi.output_count
        self.default_clock_rate = sdi.default_samples_per_sec


# Codec info
class CodecInfo:
    """This describes codec info.

    Member documentation:
    name            -- codec name
    priority        -- codec priority (0-255)
    clock_rate      -- clock rate
    channel_count   -- number of channels
    avg_bps         -- average bandwidth in bits per second
    frm_ptime       -- base frame length in milliseconds
    ptime           -- RTP frame length in milliseconds.
    pt              -- payload type.
    vad_enabled     -- specify if Voice Activity Detection is currently
                       enabled.
    plc_enabled     -- specify if Packet Lost Concealment is currently
                       enabled.
    """
    name = ""
    priority = 0
    clock_rate = 0
    channel_count = 0
    avg_bps = 0
    frm_ptime = 0
    ptime = 0
    pt = 0
    vad_enabled = False
    plc_enabled = False

    def __init__(self, codec_info, codec_param):
        self.name = codec_info.codec_id
        self.priority = codec_info.priority
        self.clock_rate = codec_param.info.clock_rate
        self.channel_count = codec_param.info.channel_cnt
        self.avg_bps = codec_param.info.avg_bps
        self.frm_ptime = codec_param.info.frm_ptime
        self.ptime = codec_param.info.frm_ptime * \
                        codec_param.setting.frm_per_pkt
        self.ptime = codec_param.info.pt
        self.vad_enabled = codec_param.setting.vad
        self.plc_enabled = codec_param.setting.plc

    def _cvt_to_pjsua(self):
        ci = _pjsua.Codec_Info()
        ci.codec_id = self.name
        ci.priority = self.priority
        return ci


# Codec parameter
class CodecParameter:
    """This specifies various parameters that can be configured for codec.

    Member documentation:

    ptime       -- specify the outgoing RTP packet length in milliseconds.
    vad_enabled -- specify if VAD should be enabled.
    plc_enabled -- specify if PLC should be enabled.
    """
    ptime = 0
    vad_enabled = False
    plc_enabled = False
    _codec_param = None
    
    def __init__(self, codec_param):
        self.ptime = codec_param.info.frm_ptime * \
                        codec_param.setting.frm_per_pkt
        self.vad_enabled = codec_param.setting.vad
        self.plc_enabled = codec_param.setting.plc
        self._codec_param = codec_param

    def _cvt_to_pjsua(self):
        self._codec_param.setting.frm_per_pkt = self.ptime / \
                                                self._codec_param.info.frm_ptime
        self._codec_param.setting.vad = self.vad_enabled
        self._codec_param.setting.plc = self.plc_enabled
        return self._codec_param


# Library mutex
class _LibMutex:
    def __init__(self, lck):
        self._lck = lck
        self._lck.acquire()
	#_Trace(('lock acquired',))

    def __del__(self):
        try:
            self._lck.release()
	    #_Trace(('lock released',))
        except:
	    #_Trace(('lock release error',))
            pass


# PJSUA Library
_lib = None
enable_trace = False

class Lib:
    """Library instance.
    
    """
    _quit = False
    _has_thread = False
    _lock = None

    def __init__(self):
        global _lib
        if _lib:
            raise Error("__init()__", None, -1, 
                        "Library instance already exist")

        self._lock = threading.RLock()
        err = _pjsua.create()
        self._err_check("_pjsua.create()", None, err)
        _lib = self

    def __del__(self):
        _pjsua.destroy()
        del self._lock
        _Trace(('Lib destroyed',))

    def __str__(self):
        return "Lib"

    @staticmethod
    def instance():
        """Return singleton instance of Lib.
        """
        return _lib

    def init(self, ua_cfg=None, log_cfg=None, media_cfg=None):
        """
        Initialize pjsua with the specified configurations.

        Keyword arguments:
        ua_cfg      -- optional UAConfig instance
        log_cfg     -- optional LogConfig instance
        media_cfg   -- optional MediaConfig instance

        """
        if not ua_cfg: ua_cfg = UAConfig()
        if not log_cfg: log_cfg = LogConfig()
        if not media_cfg: media_cfg = MediaConfig()

        py_ua_cfg = ua_cfg._cvt_to_pjsua()
        py_ua_cfg.cb.on_call_state = _cb_on_call_state
        py_ua_cfg.cb.on_incoming_call = _cb_on_incoming_call
        py_ua_cfg.cb.on_call_media_state = _cb_on_call_media_state
        py_ua_cfg.cb.on_dtmf_digit = _cb_on_dtmf_digit
        py_ua_cfg.cb.on_call_transfer_request = _cb_on_call_transfer_request
        py_ua_cfg.cb.on_call_transfer_status = _cb_on_call_transfer_status
        py_ua_cfg.cb.on_call_replace_request = _cb_on_call_replace_request
        py_ua_cfg.cb.on_call_replaced = _cb_on_call_replaced
        py_ua_cfg.cb.on_reg_state = _cb_on_reg_state
        py_ua_cfg.cb.on_incoming_subscribe = _cb_on_incoming_subscribe
        py_ua_cfg.cb.on_buddy_state = _cb_on_buddy_state
        py_ua_cfg.cb.on_pager = _cb_on_pager
        py_ua_cfg.cb.on_pager_status = _cb_on_pager_status
        py_ua_cfg.cb.on_typing = _cb_on_typing
	py_ua_cfg.cb.on_mwi_info = _cb_on_mwi_info;

        err = _pjsua.init(py_ua_cfg, log_cfg._cvt_to_pjsua(), 
                          media_cfg._cvt_to_pjsua())
        self._err_check("init()", self, err)

    def destroy(self):
        """Destroy the library, and pjsua."""
        global _lib
        if self._has_thread:
            self._quit = 1
            loop = 0
            while self._quit != 2 and loop < 400:
                self.handle_events(5)
                loop = loop + 1
                time.sleep(0.050)
        _pjsua.destroy()
        _lib = None

    def start(self, with_thread=True):
        """Start the library. 

        Keyword argument:
        with_thread -- specify whether the module should create worker
                       thread.

        """
        lck = self.auto_lock()
        err = _pjsua.start()
        self._err_check("start()", self, err)
        self._has_thread = with_thread
        if self._has_thread:
            thread.start_new(_worker_thread_main, (0,))

    def handle_events(self, timeout=50):
        """Poll the events from underlying pjsua library.
        
        Application must poll the stack periodically if worker thread
        is disable when starting the library.

        Keyword argument:
        timeout -- in milliseconds.

        """
        lck = self.auto_lock()
        return _pjsua.handle_events(timeout)

    def thread_register(self, name):
	"""Register external threads (threads that are not created by PJSIP,
	such as threads that are created by Python API) to PJSIP.

	The call must be made from the new thread before calling any pjlib 
	functions.

	Keyword arguments:
	name	-- Non descriptive name for the thread
	"""
	dummy = 1
	err = _pjsua.thread_register(name, dummy)
	self._err_check("thread_register()", self, err)

    def verify_sip_url(self, sip_url):
        """Verify that the specified string is a valid URI. 
        
        Keyword argument:
        sip_url -- the URL string.
        
        Return:
            0 is the the URI is valid, otherwise the appropriate error 
            code is returned.

        """
        lck = self.auto_lock()
        return _pjsua.verify_sip_url(sip_url)

    def create_transport(self, type, cfg=None):
        """Create SIP transport instance of the specified type. 
        
        Keyword arguments:
        type    -- transport type from TransportType constant.
        cfg     -- TransportConfig instance

        Return:
            Transport object

        """
        lck = self.auto_lock()
        if not cfg: cfg=TransportConfig()
        err, tp_id = _pjsua.transport_create(type, cfg._cvt_to_pjsua())
        self._err_check("create_transport()", self, err)
        return Transport(self, tp_id)

    def create_account(self, acc_config, set_default=True, cb=None):
        """
        Create a new local pjsua account using the specified configuration.

        Keyword arguments:
        acc_config  -- AccountConfig
        set_default -- boolean to specify whether to use this as the
                       default account.
        cb          -- AccountCallback instance.

        Return:
            Account instance

        """
        lck = self.auto_lock()
        err, acc_id = _pjsua.acc_add(acc_config._cvt_to_pjsua(), set_default)
        self._err_check("create_account()", self, err)
        return Account(self, acc_id, cb)

    def create_account_for_transport(self, transport, set_default=True,
                                     cb=None):
        """Create a new local pjsua transport for the specified transport.

        Keyword arguments:
        transport   -- the Transport instance.
        set_default -- boolean to specify whether to use this as the
                       default account.
        cb          -- AccountCallback instance.

        Return:
            Account instance

        """
        lck = self.auto_lock()
        err, acc_id = _pjsua.acc_add_local(transport._id, set_default)
        self._err_check("create_account_for_transport()", self, err)
        return Account(self, acc_id, cb)

    def hangup_all(self):
        """Hangup all calls.

        """
        lck = self.auto_lock()
        _pjsua.call_hangup_all()

    # Sound device API

    def enum_snd_dev(self):
        """Enumerate sound devices in the system.

        Return:
            list of SoundDeviceInfo. The index of the element specifies
            the device ID for the device.
        """
        lck = self.auto_lock()
        sdi_list = _pjsua.enum_snd_devs()
        info = []
        for sdi in sdi_list:
            info.append(SoundDeviceInfo(sdi))
        return info

    def get_snd_dev(self):
        """Get the device IDs of current sound devices used by pjsua.

        Return:
            (capture_dev_id, playback_dev_id) tuple
        """
        lck = self.auto_lock()
        return _pjsua.get_snd_dev()

    def set_snd_dev(self, capture_dev, playback_dev):
        """Change the current sound devices.

        Keyword arguments:
        capture_dev  -- the device ID of capture device to be used
        playback_dev -- the device ID of playback device to be used.

        """
        lck = self.auto_lock()
        err = _pjsua.set_snd_dev(capture_dev, playback_dev)
        self._err_check("set_current_sound_devices()", self, err)
    
    def set_null_snd_dev(self):
        """Disable the sound devices. This is useful if the system
        does not have sound device installed.

        """
        lck = self.auto_lock()
        err = _pjsua.set_null_snd_dev()
        self._err_check("set_null_snd_dev()", self, err)

    
    # Conference bridge

    def conf_get_max_ports(self):
        """Get the conference bridge capacity.

        Return:
            conference bridge capacity.

        """
        lck = self.auto_lock()
        return _pjsua.conf_get_max_ports()

    def conf_connect(self, src_slot, dst_slot):
        """Establish unidirectional media flow from souce to sink. 
        
        One source may transmit to multiple destinations/sink. And if 
        multiple sources are transmitting to the same sink, the media 
        will be mixed together. Source and sink may refer to the same ID, 
        effectively looping the media.

        If bidirectional media flow is desired, application needs to call
        this function twice, with the second one having the arguments 
        reversed.

        Keyword arguments:
        src_slot    -- integer to identify the conference slot number of
                       the source/transmitter.
        dst_slot    -- integer to identify the conference slot number of    
                       the destination/receiver.

        """
        lck = self.auto_lock()
        err = _pjsua.conf_connect(src_slot, dst_slot)
        self._err_check("conf_connect()", self, err)
    
    def conf_disconnect(self, src_slot, dst_slot):
        """Disconnect media flow from the source to destination port.

        Keyword arguments:
        src_slot    -- integer to identify the conference slot number of
                       the source/transmitter.
        dst_slot    -- integer to identify the conference slot number of    
                       the destination/receiver.

        """
        lck = self.auto_lock()
        err = _pjsua.conf_disconnect(src_slot, dst_slot)
        self._err_check("conf_disconnect()", self, err)

    def conf_set_tx_level(self, slot, level):
        """Adjust the signal level to be transmitted from the bridge to 
        the specified port by making it louder or quieter.

        Keyword arguments:
        slot        -- integer to identify the conference slot number.
        level       -- Signal level adjustment. Value 1.0 means no level
                       adjustment, while value 0 means to mute the port.
        """
        lck = self.auto_lock()
        err = _pjsua.conf_set_tx_level(slot, level)
        self._err_check("conf_set_tx_level()", self, err)
        
    def conf_set_rx_level(self, slot, level):
        """Adjust the signal level to be received from the specified port
        (to the bridge) by making it louder or quieter.

        Keyword arguments:
        slot        -- integer to identify the conference slot number.
        level       -- Signal level adjustment. Value 1.0 means no level
                       adjustment, while value 0 means to mute the port.
        """
        lck = self.auto_lock()
        err = _pjsua.conf_set_rx_level(slot, level)
        self._err_check("conf_set_rx_level()", self, err)
        
    def conf_get_signal_level(self, slot):
        """Get last signal level transmitted to or received from the 
        specified port. The signal levels are float values from 0.0 to 1.0,
        with 0.0 indicates no signal, and 1.0 indicates the loudest signal
        level.

        Keyword arguments:
        slot        -- integer to identify the conference slot number.

        Return value:
            (tx_level, rx_level) tuple.
        """
        lck = self.auto_lock()
        err, tx_level, rx_level = _pjsua.conf_get_signal_level(slot)
        self._err_check("conf_get_signal_level()", self, err)
        return (tx_level, rx_level)
        


    # Codecs API

    def enum_codecs(self):
        """Return list of codecs supported by pjsua.

        Return:
            list of CodecInfo

        """
        lck = self.auto_lock()
        ci_list = _pjsua.enum_codecs()
        codec_info = []
        for ci in ci_list:
            cp = _pjsua.codec_get_param(ci.codec_id)
            if cp:
                codec_info.append(CodecInfo(ci, cp))
        return codec_info

    def set_codec_priority(self, name, priority):
        """Change the codec priority.

        Keyword arguments:
        name     -- Codec name
        priority -- Codec priority, which range is 0-255.

        """
        lck = self.auto_lock()
        err = _pjsua.codec_set_priority(name, priority)
        self._err_check("set_codec_priority()", self, err)

    def get_codec_parameter(self, name):
        """Get codec parameter for the specified codec.

        Keyword arguments:
        name    -- codec name.

        """
        lck = self.auto_lock()
        cp = _pjsua.codec_get_param(name)
        if not cp:
            self._err_check("get_codec_parameter()", self, -1, 
                            "Invalid codec name")
        return CodecParameter(cp)

    def set_codec_parameter(self, name, param):
        """Modify codec parameter for the specified codec.

        Keyword arguments:
        name    -- codec name
        param   -- codec parameter.

        """
        lck = self.auto_lock()
        err = _pjsua.codec_set_param(name, param._cvt_to_pjsua())
        self._err_check("set_codec_parameter()", self, err)
    
    # WAV playback and recording

    def create_player(self, filename, loop=False):
        """Create WAV file player.

        Keyword arguments
        filename    -- WAV file name
        loop        -- boolean to specify whether playback should
                       automatically restart upon EOF
        Return:
            WAV player ID

        """
        lck = self.auto_lock()
        opt = 0
        if not loop:
            opt = opt + 1
        err, player_id = _pjsua.player_create(filename, opt)
        self._err_check("create_player()", self, err)
        return player_id
        
    def player_get_slot(self, player_id):
        """Get the conference port ID for the specified player.

        Keyword arguments:
        player_id  -- the WAV player ID
        
        Return:
            Conference slot number for the player

        """
        lck = self.auto_lock()
        slot = _pjsua.player_get_conf_port(player_id)
        if slot < 0:
                self._err_check("player_get_slot()", self, -1, 
                                "Invalid player id")
        return slot

    def player_set_pos(self, player_id, pos):
        """Set WAV playback position.

        Keyword arguments:
        player_id   -- WAV player ID
        pos         -- playback position, in samples

        """
        lck = self.auto_lock()
        err = _pjsua.player_set_pos(player_id, pos)
        self._err_check("player_set_pos()", self, err)
        
    def player_destroy(self, player_id):
        """Destroy the WAV player.

        Keyword arguments:
        player_id   -- the WAV player ID.

        """
        lck = self.auto_lock()
        err = _pjsua.player_destroy(player_id)
        self._err_check("player_destroy()", self, err)

    def create_playlist(self, filelist, label="playlist", loop=True):
        """Create WAV playlist.

        Keyword arguments:
        filelist    -- List of WAV file names.
        label       -- Optional name to be assigned to the playlist
                       object (useful for logging)
        loop        -- boolean to specify whether playback should
                       automatically restart upon EOF

        Return:
            playlist_id
        """
        lck = self.auto_lock()
        opt = 0
        if not loop:
            opt = opt + 1
        err, playlist_id = _pjsua.playlist_create(label, filelist, opt)
        self._err_check("create_playlist()", self, err)
        return playlist_id 

    def playlist_get_slot(self, playlist_id):
        """Get the conference port ID for the specified playlist.

        Keyword arguments:
        playlist_id  -- the WAV playlist ID
        
        Return:
            Conference slot number for the playlist

        """
        lck = self.auto_lock()
        slot = _pjsua.player_get_conf_port(playlist_id)
        if slot < 0:
                self._err_check("playlist_get_slot()", self, -1, 
                                "Invalid playlist id")
        return slot

    def playlist_destroy(self, playlist_id):
        """Destroy the WAV playlist.

        Keyword arguments:
        playlist_id   -- the WAV playlist ID.

        """
        lck = self.auto_lock()
        err = _pjsua.player_destroy(playlist_id)
        self._err_check("playlist_destroy()", self, err)

    def create_recorder(self, filename):
        """Create WAV file recorder.

        Keyword arguments
        filename    -- WAV file name

        Return:
            WAV recorder ID

        """
        lck = self.auto_lock()
        err, rec_id = _pjsua.recorder_create(filename, 0, None, -1, 0)
        self._err_check("create_recorder()", self, err)
        return rec_id
        
    def recorder_get_slot(self, rec_id):
        """Get the conference port ID for the specified recorder.

        Keyword arguments:
        rec_id  -- the WAV recorder ID
        
        Return:
            Conference slot number for the recorder

        """
        lck = self.auto_lock()
        slot = _pjsua.recorder_get_conf_port(rec_id)
        if slot < 1:
            self._err_check("recorder_get_slot()", self, -1, 
                            "Invalid recorder id")
        return slot

    def recorder_destroy(self, rec_id):
        """Destroy the WAV recorder.

        Keyword arguments:
        rec_id   -- the WAV recorder ID.

        """
        lck = self.auto_lock()
        err = _pjsua.recorder_destroy(rec_id)
        self._err_check("recorder_destroy()", self, err)


    # Internal functions

    @staticmethod
    def strerror(err):
        return _pjsua.strerror(err)
    
    def _err_check(self, op_name, obj, err_code, err_msg=""):
        if err_code != 0:
            raise Error(op_name, obj, err_code, err_msg)

    @staticmethod
    def _create_msg_data(hdr_list):
        if not hdr_list:
            return None
        msg_data = _pjsua.Msg_Data()
        msg_data.hdr_list = hdr_list
        return msg_data
    
    def auto_lock(self):
        return _LibMutex(self._lock)

    # Internal dictionary manipulation for calls, accounts, and buddies

    def _lookup_call(self, call_id):
        return _pjsua.call_get_user_data(call_id)

    def _lookup_account(self, acc_id):
        return _pjsua.acc_get_user_data(acc_id)

    def _lookup_buddy(self, buddy_id, uri=None):
        if buddy_id != -1:
            buddy = _pjsua.buddy_get_user_data(buddy_id)
        elif uri:
            buddy_id = _pjsua.buddy_find(uri)
            if buddy_id != -1:
                buddy = _pjsua.buddy_get_user_data(buddy_id)
            else:
                buddy = None
        else:
            buddy = None
            
        return buddy 

    # Account allbacks

    def _cb_on_reg_state(self, acc_id):
        acc = self._lookup_account(acc_id)
        if acc:
            acc._cb.on_reg_state()

    def _cb_on_incoming_subscribe(self, acc_id, buddy_id, from_uri, 
                                  contact_uri, pres_obj):
        acc = self._lookup_account(acc_id)
        if acc:
            buddy = self._lookup_buddy(buddy_id)
            return acc._cb.on_incoming_subscribe(buddy, from_uri, contact_uri,
                                                 pres_obj)
        else:
            return (404, None)

    def _cb_on_incoming_call(self, acc_id, call_id, rdata):
        acc = self._lookup_account(acc_id)
        if acc:
            acc._cb.on_incoming_call( Call(self, call_id) )
        else:
            _pjsua.call_hangup(call_id, 603, None, None)

    # Call callbacks 

    def _cb_on_call_state(self, call_id):
        call = self._lookup_call(call_id)
        if call:
            if call._id == -1:
                call.attach_to_id(call_id)
            done = (call.info().state == CallState.DISCONNECTED)
            call._cb.on_state()
            if done:
                _pjsua.call_set_user_data(call_id, 0)
        else:
            pass

    def _cb_on_call_media_state(self, call_id):
        call = self._lookup_call(call_id)
        if call:
            call._cb.on_media_state()

    def _cb_on_dtmf_digit(self, call_id, digits):
        call = self._lookup_call(call_id)
        if call:
            call._cb.on_dtmf_digit(digits)

    def _cb_on_call_transfer_request(self, call_id, dst, code):
        call = self._lookup_call(call_id)
        if call:
            return call._cb.on_transfer_request(dst, code)
        else:
            return 603

    def _cb_on_call_transfer_status(self, call_id, code, text, final, cont):
        call = self._lookup_call(call_id)
        if call:
            return call._cb.on_transfer_status(code, text, final, cont)
        else:
            return cont

    def _cb_on_call_replace_request(self, call_id, rdata, code, reason):
        call = self._lookup_call(call_id)
        if call:
            return call._cb.on_replace_request(code, reason)
        else:
            return code, reason

    def _cb_on_call_replaced(self, old_call_id, new_call_id):
        old_call = self._lookup_call(old_call_id)
        new_call = self._lookup_call(new_call_id)
        if old_call and new_call:
            old_call._cb.on_replaced(new_call)

    def _cb_on_pager(self, call_id, from_uri, to_uri, contact, mime_type, 
                     body, acc_id):
        call = None
        if call_id != -1:
            call = self._lookup_call(call_id)
        if call:
            call._cb.on_pager(mime_type, body)
        else:
            acc = self._lookup_account(acc_id)
            buddy = self._lookup_buddy(-1, from_uri)
            if buddy:
                buddy._cb.on_pager(mime_type, body)
            else:
                acc._cb.on_pager(from_uri, contact, mime_type, body)

    def _cb_on_pager_status(self, call_id, to_uri, body, user_data, 
                            code, reason, acc_id):
        call = None
        if call_id != -1:
            call = self._lookup_call(call_id)
        if call:
            call._cb.on_pager_status(body, user_data, code, reason)
        else:
            acc = self._lookup_account(acc_id)
            buddy = self._lookup_buddy(-1, to_uri)
            if buddy:
                buddy._cb.on_pager_status(body, user_data, code, reason)
            else:
                acc._cb.on_pager_status(to_uri, body, user_data, code, reason)

    def _cb_on_typing(self, call_id, from_uri, to_uri, contact, is_typing, 
                      acc_id):
        call = None
        if call_id != -1:
            call = self._lookup_call(call_id)
        if call:
            call._cb.on_typing(is_typing)
        else:
            acc = self._lookup_account(acc_id)
            buddy = self._lookup_buddy(-1, from_uri)
            if buddy:
                buddy._cb.on_typing(is_typing)
            else:
                acc._cb.on_typing(from_uri, contact, is_typing)

    def _cb_on_mwi_info(self, acc_id, body):
        acc = self._lookup_account(acc_id)
        if acc:
            return acc._cb.on_mwi_info(body)

    def _cb_on_buddy_state(self, buddy_id):
        buddy = self._lookup_buddy(buddy_id)
        if buddy:
            buddy._cb.on_state()

#
# Internal
#

def _cb_on_call_state(call_id, e):
    _lib._cb_on_call_state(call_id)

def _cb_on_incoming_call(acc_id, call_id, rdata):
    _lib._cb_on_incoming_call(acc_id, call_id, rdata)

def _cb_on_call_media_state(call_id):
    _lib._cb_on_call_media_state(call_id)

def _cb_on_dtmf_digit(call_id, digits):
    _lib._cb_on_dtmf_digit(call_id, digits)

def _cb_on_call_transfer_request(call_id, dst, code):
    return _lib._cb_on_call_transfer_request(call_id, dst, code)

def _cb_on_call_transfer_status(call_id, code, reason, final, cont):
    return _lib._cb_on_call_transfer_status(call_id, code, reason, 
                                             final, cont)
def _cb_on_call_replace_request(call_id, rdata, code, reason):
    return _lib._cb_on_call_replace_request(call_id, rdata, code, reason)

def _cb_on_call_replaced(old_call_id, new_call_id):
    _lib._cb_on_call_replaced(old_call_id, new_call_id)

def _cb_on_reg_state(acc_id):
    _lib._cb_on_reg_state(acc_id)

def _cb_on_incoming_subscribe(acc_id, buddy_id, from_uri, contact_uri, pres):
    return _lib._cb_on_incoming_subscribe(acc_id, buddy_id, from_uri, 
                                          contact_uri, pres)

def _cb_on_buddy_state(buddy_id):
    _lib._cb_on_buddy_state(buddy_id)

def _cb_on_pager(call_id, from_uri, to, contact, mime_type, body, acc_id):
    _lib._cb_on_pager(call_id, from_uri, to, contact, mime_type, body, acc_id)

def _cb_on_pager_status(call_id, to, body, user_data, status, reason, acc_id):
    _lib._cb_on_pager_status(call_id, to, body, user_data, 
                             status, reason, acc_id)

def _cb_on_typing(call_id, from_uri, to, contact, is_typing, acc_id):
    _lib._cb_on_typing(call_id, from_uri, to, contact, is_typing, acc_id)

def _cb_on_mwi_info(acc_id, body):
    _lib._cb_on_mwi_info(acc_id, body)

# Worker thread
def _worker_thread_main(arg):
    global _lib
    _Trace(('worker thread started..',))
    thread_desc = 0;
    err = _pjsua.thread_register("python worker", thread_desc)
    _lib._err_check("thread_register()", _lib, err)
    while _lib and _lib._quit == 0:
        _lib.handle_events(1)
	time.sleep(0.050)
    if _lib:
        _lib._quit = 2
    _Trace(('worker thread exited..',))

def _Trace(args):
    global enable_trace
    if enable_trace:
        print "** ",
        for arg in args:
            print arg,
        print " **"

