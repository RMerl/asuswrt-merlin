# $Id: config.py 2912 2009-08-24 11:56:13Z bennylp $
#
# Confbot settings
#
import pjsua as pj

# Set of admins. If empty then everyone is admin!
admins = set([])

# acc_cfg holds the account config (set it to None to disable account)
acc_cfg = None
acc_cfg = pj.AccountConfig()
if acc_cfg:
	acc_cfg.id = "sip:bot@pjsip.org"
	acc_cfg.reg_uri = "sip:pjsip.org"
	acc_cfg.proxy = [ "sip:pjsip.org;lr;transport=tcp" ]
	acc_cfg.auth_cred = [ pj.AuthCred("*", "bot", "secretpass") ]
	acc_cfg.publish_enabled = True
	#acc_cfg.require_timer = True

# Transport configs (set them to None to disable the transport)
udp_cfg = pj.TransportConfig(5080)
tcp_cfg = pj.TransportConfig(0)
#tcp_cfg = None

# Logging Config (you can also set it to None to use default values)
def log_cb(level, str, len):
	print str,
	
log_cfg = pj.LogConfig()
#log_cfg.callback = log_cb

# UA Config (you can also set it to None to use default values)
ua_cfg = pj.UAConfig()
ua_cfg.user_agent = "PJSIP ConfBot"
ua_cfg.stun_host = "stun.pjsip.org"

# Media config (you can also set it to None to use default values)
media_cfg = pj.MediaConfig()
media_cfg.enable_ice = True
media_cfg.max_calls = 20
