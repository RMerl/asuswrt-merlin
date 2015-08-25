import py_pjsua

status = py_pjsua.create()
print "py status " + `status`


#
# Create configuration objects
#
ua_cfg = py_pjsua.config_default()
log_cfg = py_pjsua.logging_config_default()
media_cfg = py_pjsua.media_config_default()

#
# Logging callback.
#
def logging_cb1(level, str, len):
    print str,


#
# Configure logging
#
log_cfg.cb = logging_cb1
log_cfg.console_level = 4

#
# Initialize pjsua!
#
status = py_pjsua.init(ua_cfg, log_cfg, media_cfg);
print "py status after initialization :" + `status`


#
# Start pjsua!
#
status = py_pjsua.start()
if status != 0:
    exit(1)


message = py_pjsua.msg_data_init()

print "identitas object message data :" + `message`

sipaddr = 'sip:167.205.34.99'
print "checking sip address [%s] : %d" % (sipaddr, py_pjsua.verify_sip_url(sipaddr))

sipaddr = '167.205.34.99'
print "checking invalid sip address [%s] : %d" % (sipaddr, py_pjsua.verify_sip_url(sipaddr))

object = py_pjsua.get_pjsip_endpt()
print "identitas Endpoint :" + `object` + ""

mediaend = py_pjsua.get_pjmedia_endpt()
print "identitas Media Endpoint :" + `mediaend` + ""

pool = py_pjsua.get_pool_factory()
print "identitas pool factory :" + `pool` + ""

status = py_pjsua.handle_events(3000)
print "py status after 3 second of blocking wait :" + `status`



# end of new testrun

#

# lib transport
stunc = py_pjsua.stun_config_default();


tc = py_pjsua.transport_config_default();


py_pjsua.normalize_stun_config(stunc);


status, id = py_pjsua.transport_create(1, tc);
print "py transport create status " + `status`

ti = py_pjsua.Transport_Info();
ti = py_pjsua.transport_get_info(id)
print "py transport get info status " + `status`

status = py_pjsua.transport_set_enable(id,1)
print "py transport set enable status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set enable",status)


status = py_pjsua.transport_close(id,1)
print "py transport close status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","close",status)

# end of lib transport

# lib account 

accfg = py_pjsua.acc_config_default()
status, accid = py_pjsua.acc_add(accfg, 1)
print "py acc add status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","add acc",status)
count = py_pjsua.acc_get_count()
print "acc count " + `count`

accid = py_pjsua.acc_get_default()

print "acc id default " + `accid`

# end of lib account

#lib buddy

bcfg = py_pjsua.Buddy_Config()
status, id = py_pjsua.buddy_add(bcfg)
acc_id = id
print "py buddy add status " + `status` + " id " + `id`
bool = py_pjsua.buddy_is_valid(id)
print "py buddy is valid " + `bool`
count = py_pjsua.get_buddy_count()
print "buddy count " + `count`
binfo = py_pjsua.buddy_get_info(id)
ids = py_pjsua.enum_buddies()
status = py_pjsua.buddy_del(id)
print "py buddy del status " + `status`
status = py_pjsua.buddy_subscribe_pres(id, 1)
print "py buddy subscribe pres status " + `status`
py_pjsua.pres_dump(1)
status = py_pjsua.im_send(accid, "fahris@divusi.com", "", "hallo", message, 0)
print "py im send status " + `status`
status = py_pjsua.im_typing(accid, "fahris@divusi.com", 1, message)
print "py im typing status " + `status`
#print "binfo " + `binfo`

#end of lib buddy

#lib media
count = py_pjsua.conf_get_max_ports()
print "py media conf get max ports " + `count`
count = py_pjsua.conf_get_active_ports()
print "py media conf get active ports " + `count`
ids = py_pjsua.enum_conf_ports()
for id in ids:
	print "py media conf ports " + `id`
	cp_info = py_pjsua.conf_get_port_info(id)
	print "port info name " + cp_info.name
pool = py_pjsua.PJ_Pool()
port = py_pjsua.PJMedia_Port()
status, id = py_pjsua.conf_add_port(pool,port)
print "py media conf add port status " + `status` + " id " + `id`
if status != 0 :
	py_pjsua.perror("py_pjsua","add port",status)
status = py_pjsua.conf_remove_port(id)
print "py media conf remove port status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","remove port",status)
status = py_pjsua.conf_connect(id, id)
print "py media conf connect status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","connect",status)
status = py_pjsua.conf_disconnect(id, id)
print "py media conf disconnect status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","disconnect",status)
status, id = py_pjsua.player_create("test.wav", 0)
print "py media player create status " + `status` + " id " + `id`
if status != 0 :
	py_pjsua.perror("py_pjsua","player create",status)
c_id = py_pjsua.player_get_conf_port(id)
print "py media player get conf port id " + `c_id`
status = py_pjsua.player_set_pos(id, 10)
if status != 0 :
	py_pjsua.perror("py_pjsua","player set pos",status)
status = py_pjsua.player_destroy(id)
if status != 0 :
	py_pjsua.perror("py_pjsua","player destroy",status)
status, id = py_pjsua.recorder_create("rec.wav", 0, "None", 1000, 0)
print "py media recorder create status " + `status` + " id " + `id`
if status != 0 :
	py_pjsua.perror("py_pjsua","recorder create",status)
status = py_pjsua.recorder_get_conf_port(id)
print "py media recorder get conf port status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","recorder get conf port",status)
status = py_pjsua.recorder_destroy(id)
print "py media recorder destroy status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","recorder destroy",status)
#cdev, pdev = py_pjsua.get_snd_dev()
#print "py media get snd dev capture dev " + `cdev` + " playback dev " + `pdev`
status = py_pjsua.set_snd_dev(0,1)
print "py media set snd dev status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set snd dev",status)
status = py_pjsua.set_null_snd_dev()
print "py media set null snd dev status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set null snd dev",status)
port = py_pjsua.set_no_snd_dev()
status = py_pjsua.set_ec(0,0)
print "py media set ec status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set ec",status)
tail = py_pjsua.get_ec_tail()
print "py media get ec tail " + `tail`
infos = py_pjsua.enum_codecs()
for info in infos:
	print "py media enum codecs " + `info`
status = py_pjsua.codec_set_priority("coba", 0)
print "py media codec set priority " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","codec set priority",status)
c_param = py_pjsua.codec_get_param("coba")
status = py_pjsua.codec_set_param("coba", c_param)
print "py media codec set param " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","codec set param",status)
	
#end of lib media

#lib call

count = py_pjsua.call_get_max_count()
print "py call get max count " + `count`
count = py_pjsua.call_get_count()
print "py call get count " + `count`
ids = py_pjsua.enum_calls()
for id in ids:
	print "py enum calls id " + `id`
msg_data = py_pjsua.Msg_Data()
status, id = py_pjsua.call_make_call(-1, "sip:bulukucing1@iptel.org", 0, 0, msg_data)
print "py call make call " + `status` + " id " + `id`
if status != 0 :
	py_pjsua.perror("py_pjsua","call make call",status)
bool = py_pjsua.call_is_active(id)
print "py call is active " + `bool`
bool = py_pjsua.call_has_media(id)
print "py call has media " + `bool`
cp_id = py_pjsua.call_get_conf_port(id)
print "py call get conf port " + `cp_id`
info = py_pjsua.call_get_info(id)
if info != None :
	print "py info id " + `info.id`
status = py_pjsua.call_set_user_data(id, 0)
print "py call set user data status " + `status`
if status != 0 :
	py_pjsua.perror("py_pjsua","set user data",status)
user_data = py_pjsua.call_get_user_data(id)
print "py call get user data " + `user_data`



#end of lib call

py_pjsua.perror("saya","hallo",70006)

status = py_pjsua.destroy()
print "py status " + `status`


