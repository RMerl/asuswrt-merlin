# $Id: inc_sdp.py 2392 2008-12-22 18:54:58Z bennylp $

# SDP template
sdp_templ = \
"""v=0\r
o=- 1 1 $NET_TYPE $ADDR_TYPE $LOCAL_IP\r
s=pjmedia\r
t=0 0\r
$SDP_LINES"""

sdp_media_templ = \
"""m=$MEDIA_TYPE $PORT $TRANSPORT 0\r
c=$NET_TYPE $ADDR_TYPE $LOCAL_IP\r
$SDP_LINES"""

# Create SDP session
def session(local_ip="127.0.0.1", extra_lines="", net_type="IN", addr_type="IP4"):
	sdp = sdp_templ
	sdp = sdp.replace("$NET_TYPE", net_type)
	sdp = sdp.replace("$ADDR_TYPE", addr_type)
	sdp = sdp.replace("$LOCAL_IP", local_ip)
	sdp = sdp.replace("$SDP_LINES", extra_lines)
	return sdp

# Create basic SDP media
def media(media_type="audio", local_port=4000, local_ip="127.0.0.1", extra_lines="", 
			  net_type = "IN", addr_type="IP4", transport="RTP/AVP"):
	sdp = sdp_media_templ
	sdp = sdp.replace("$MEDIA_TYPE", media_type)
	sdp = sdp.replace("$LOCAL_IP", local_ip)
	sdp = sdp.replace("$PORT", str(local_port))
	sdp = sdp.replace("$NET_TYPE", net_type)
	sdp = sdp.replace("$ADDR_TYPE", addr_type)
	sdp = sdp.replace("$TRANSPORT", transport)
	sdp = sdp.replace("$SDP_LINES", extra_lines)
	return sdp


