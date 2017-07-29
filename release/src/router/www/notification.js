var noti_auth_mode_2g = "";
var noti_auth_mode_5g = "";

if(isSwMode('rt') || isSwMode('ap') || '<% nvram_get("wlc_band"); %>' == ''){
	noti_auth_mode_2g = '<% nvram_get("wl0_auth_mode_x"); %>';
	noti_auth_mode_5g = '<% nvram_get("wl1_auth_mode_x"); %>';
}
else if(isSwMode('mb')){
	noti_auth_mode_2g = '';
	noti_auth_mode_5g = '';
}
else{
	noti_auth_mode_2g = ('<% nvram_get("wlc_band"); %>' == 0) ? '<% nvram_get("wl0.1_auth_mode_x"); %>' : '<% nvram_get("wl0_auth_mode_x"); %>';
	noti_auth_mode_5g = ('<% nvram_get("wlc_band"); %>' == 1) ? '<% nvram_get("wl1.1_auth_mode_x"); %>' : '<% nvram_get("wl1_auth_mode_x"); %>';

	if(concurrep_support){
		noti_auth_mode_2g = (wlc_express == 1) ? '' : '<% nvram_get("wl0.1_auth_mode_x"); %>';
		noti_auth_mode_5g = (wlc_express == 2) ? '' : '<% nvram_get("wl1.1_auth_mode_x"); %>';
	}
}

var webs_state_info = '<% nvram_get("webs_state_info"); %>';
var webs_state_info_beta = '<% nvram_get("webs_state_info_beta"); %>';

var st_ftp_mode = '<% nvram_get("st_ftp_mode"); %>';
var st_ftp_force_mode = '<% nvram_get("st_ftp_force_mode"); %>';
var st_samba_mode = '<% nvram_get("st_samba_mode"); %>';
var st_samba_force_mode = '<% nvram_get("st_samba_force_mode"); %>';
var enable_samba = '<% nvram_get("enable_samba"); %>';
var enable_ftp = '<% nvram_get("enable_ftp"); %>';
var autodet_state = '<% nvram_get("autodet_state"); %>';
var autodet_auxstate = '<% nvram_get("autodet_auxstate"); %>';
var wan_proto = '<% nvram_get("wan0_proto"); %>';
// MODELDEP : DSL-AC68U Only for now
if(based_modelid == "DSL-AC68U"){
	var dla_modified = (vdsl_support == false) ? "0" :'<% nvram_get("dsltmp_dla_modified"); %>';	
	var dsl_loss_sync = "";
	if(dla_modified == "1")
		dsl_loss_sync = "1";
	else
		dsl_loss_sync = (vdsl_support == false) ? "0" :'<% nvram_get("dsltmp_syncloss"); %>';	
	var experience_fb = (dsl_support == false) ? "2" : '<% nvram_get("fb_experience"); %>';
}
else{
	var dla_modified = "0";
	var dsl_loss_sync = "0";
	var experience_fb = "2";
}
if(dsl_support){
	var noti_notif_Flag = '<% nvram_get("webs_notif_flag"); %>';
	var notif_hint_index = '<% nvram_get("webs_notif_index"); %>';
	var notif_hint_info = '<% nvram_get("webs_notif_info"); %>';    
	var notif_hint_infomation = notif_hint_info;
	if(notif_hint_infomation.charAt(0) == "+")      //remove start with '++'
		notif_hint_infomation = notif_hint_infomation.substring(2, notif_hint_infomation.length);
	var notif_msg = "";             
	var notif_hint_array = notif_hint_infomation.split("++");
	if(notif_hint_array[0] != ""){ 
		notif_msg = "<ol style=\"margin-left:-20px;*margin-left:20px;\">";
		for(var i=0; i<notif_hint_array.length; i++){
			if(i==0)
				notif_msg += "<li>"+notif_hint_array[i];
			else
				notif_msg += "<div><img src=\"/images/New_ui/export/line_export_notif.png\" style=\"margin-top:2px;margin-bottom:2px;margin-left:-20px;*margin-left:-20px;\"></div><li>"+notif_hint_array[i];
		}
		notif_msg += "</ol>";
    	}	
}

var notification = {
	stat: "off",
	flash: "off",
	flashTimer: 0,
	hoverText: "",
	clickText: "",
	array: [],
	desc: [],
	action_desc: [],
	upgrade: 0,
	wifi_2g: 0,
	wifi_5g: 0,
	ftp: 0,
	samba: 0,
	loss_sync: 0,
	experience_FB: 0,
	notif_hint: 0,
	mobile_traffic: 0,
	send_debug_log: 0,
	clicking: 0,
	sim_record: 0,
	external_ip: 0,
	redirectftp:function(){location.href = 'Advanced_AiDisk_ftp.asp';},
	redirectsamba:function(){location.href = 'Advanced_AiDisk_samba.asp';},
	redirectFeedback:function(){location.href = 'Advanced_Feedback.asp';},
	redirectFeedbackInfo:function(){location.href = 'Feedback_Info.asp';},
	redirectRefresh:function(){
		var header_info = [<% get_header_info(); %>];
		location.href = header_info[0].current_page;
	},
	redirectHint:function(){location.href = location.href;},
	clickCallBack: [],
	pppoe_tw: 0,
	ie_legacy: 0,
	notiClick: function(){
		// stop flashing after the event is checked.
		cookie.set("notification_history", [notification.upgrade, notification.wifi_2g ,notification.wifi_5g ,notification.ftp ,notification.samba ,notification.loss_sync ,notification.experience_FB ,notification.notif_hint, notification.mobile_traffic, notification.send_debug_log, notification.sim_record, notification.external_ip, notification.pppoe_tw, notification.pppoe_tw_static, notification.ie_legacy].join(), 1000);
		clearInterval(notification.flashTimer);
		document.getElementById("notification_status").className = "notification_on";
		if(notification.clicking == 0){
			var txt = '<div id="notiDiv"><table width="100%">'

			for(i=0; i<notification.array.length; i++){
				if(notification.array[i] != null && notification.array[i] != "off"){
						txt += '<tr><td><table id="notiDiv_table3" width="100%" border="0" cellpadding="0" cellspacing="0" bgcolor="#232629">';
		  			txt += '<tr><td><table id="notiDiv_table5" border="0" cellpadding="5" cellspacing="0" bgcolor="#232629" width="100%">';
		  			txt += '<tr><td valign="TOP" width="100%"><div style="white-space:pre-wrap;font-size:13px;color:white;cursor:text">' + notification.desc[i] + '</div>';
		  			txt += '</td></tr>';

		  			if( i == 2 ){					  				
		  				txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></td></tr>';
		  				if(band5g_support && notification.array[3] != null && notification.array[3] != "off"){
		  						txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i+1] + '">' + notification.action_desc[i+1] + '</div></td></tr>';
		  				}else
			  				notification.array[3] = "off";
		  			}
						else if( i == 7){
							if(notification.array[18] != null){
								txt += '<tr><td width="100%"><div style="text-align:right;text-decoration:underline;color:#FFCC00;font-size:14px;"><span style="cursor: pointer" onclick="' + notification.clickCallBack[18] + '">' + notification.action_desc[18] + '</span>';
							}								
							txt += '<span style="margin-left:10px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</span></div></td></tr>';
							notification.array[18] = "off";
						}
						else if( i == 9){
		  				txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></td></tr>';
							if(notification.array[10] != null && notification.array[10] != "off"){
								txt += '<tr><td width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i+1] + '">' + notification.action_desc[i+1] + '</div></td></tr>';
							}		
							notification.array[10] = "off";
						}
		  			else{
	  					txt += '<tr><td><table width="100%"><div style="text-decoration:underline;text-align:right;color:#FFCC00;font-size:14px;cursor: pointer" onclick="' + notification.clickCallBack[i] + '">' + notification.action_desc[i] + '</div></table></td></tr>';
		  			}

		  			txt += '</table></td></tr></table></td></tr>'
	  			}
			}
			txt += '</table></div>';

			document.getElementById("notification_desc").innerHTML = txt;
			notification.clicking = 1;
		}else{
			document.getElementById("notification_desc").innerHTML = "";
			notification.clicking = 0;
		}
	},
	
	updateNTDB_Status: function(){
		var data_usage = tx_bytes + rx_bytes;
		if(gobi_support && (usb_index != -1) && (notification.sim_state != "") && (modem_bytes_data_limit > 0) && (data_usage >= modem_bytes_data_limit)){
			notification.array[12] = 'noti_mobile_traffic';
			notification.mobile_traffic = 1;
			notification.desc[12] = "<#Mobile_limit_warning#>";
			notification.action_desc[12] = "<#ASUSGATE_act_change#>";
			notification.clickCallBack[12] = "setTrafficLimit();";
		}
		else{
			notification.array[12] = 'off';
			notification.mobile_traffic = 0;
		}

		if(gobi_support && (usb_index != -1) && (sim_state != "") && (modem_sim_order == -1)){
			notification.array[13] = 'noti_sim_record';
			notification.sim_record = 1;
			notification.desc[13] = "<#Mobile_record_limit_warning#>";
			notification.action_desc[13] = "Delete now";
			notification.clickCallBack[13] = "upated_sim_record();";
		}
		else{
			notification.array[13] = 'off';
			notification.sim_record = 0;
		}

		if(realip_support && !external_ip){
			notification.array[14] = 'noti_external_ip';
			notification.external_ip = 1;
			notification.desc[14] = "<#external_ip_warning#>";
			notification.action_desc[14] = "<#ASUSGATE_act_change#>";
			if(active_wan_unit == 0)
				notification.clickCallBack[14] = "goToWAN(0);";
			else if(active_wan_unit == 1)
				notification.clickCallBack[14] = "goToWAN(1);";
		}
		else{
			notification.array[14] = 'off';
			notification.external_ip = 0;
		}

		if(notification.stat != "on" && (notification.mobile_traffic || notification.sim_record || notification.external_ip)){
			notification.stat = "on";
			notification.flash = "on";
			notification.run_notice();
		}
		else if(notification.stat == "on" && !notification.mobile_traffic && !notification.sim_record && !notification.external_ip && !notification.upgrade && !notification.wifi_2g && 
				!notification.wifi_5g && !notification.ftp && !notification.samba && !notification.loss_sync && !notification.experience_FB && !notification.notif_hint && !notification.mobile_traffic && 
				!notification.send_debug_log && !notification.pppoe_tw && !notification.pppoe_tw_static && !notification.ie_legacy){
			cookie.unset("notification_history");
			clearInterval(notification.flashTimer);
			document.getElementById("notification_status").className = "notification_off";
		}
	},

	run: function(){
	/*-- notification start --*/
		cookie.unset("notification_history");
		if(notice_pw_is_default == 1){	//case1
			notification.array[0] = 'noti_acpw';
			notification.acpw = 1;
			notification.desc[0] = '<#ASUSGATE_note1#>';
			notification.action_desc[0] = '<#ASUSGATE_act_change#>';
			notification.clickCallBack[0] = "location.href = 'Advanced_System_Content.asp?af=http_passwd2';";	
		}else
			notification.acpw = 0;

//		if(current_firmware_path==1)
//			var latest_state_info = webs_state_info_beta;
//		else
			var latest_state_info = webs_state_info;
			
		if(isNewFW(latest_state_info,current_firmware_path,current_firmware_path)){	//case2
			notification.array[1] = 'noti_upgrade';
			notification.upgrade = 1;
			notification.desc[1] = '<#ASUSGATE_note2#>';
			if(!live_update_support || !HTTPS_support){
				notification.action_desc[1] = '<a id="link_to_downlodpage" target="_blank" href="https://asuswrt.lostrealm.ca/download/" style="color:#FFCC00;"><#ASUSGATE_act_update#></a>';
				notification.clickCallBack[1] = "";
			}
			else{
				notification.action_desc[1] = '<#ASUSGATE_act_update#>';
				notification.clickCallBack[1] = "location.href = 'Advanced_FirmwareUpgrade_Content.asp?confirm_show="+current_firmware_path+"';"
			}
		}else
			notification.upgrade = 0;
		
		if(band2g_support && sw_mode != 4 && noti_auth_mode_2g == 'open'){ //case3-1
				notification.array[2] = 'noti_wifi_2g';
				notification.wifi_2g = 1;
				notification.desc[2] = '<#ASUSGATE_note3#>';
				notification.action_desc[2] = '<#ASUSGATE_act_change#> (2.4GHz)';
				notification.clickCallBack[2] = "change_wl_unit_status(0);";
		}else
			notification.wifi_2g = 0;
			
		if(band5g_support && sw_mode != 4 && noti_auth_mode_5g == 'open'){	//case3-2
				notification.array[3] = 'noti_wifi_5g';
				notification.wifi_5g = 1;
				notification.desc[3] = '<#ASUSGATE_note3#>';
				notification.action_desc[3] = '<#ASUSGATE_act_change#> (5 GHz)';
				notification.clickCallBack[3] = "change_wl_unit_status(1);";
		}else
			notification.wifi_5g = 0;
		
/*		if(usb_support && !noftp_support && enable_ftp == 1 && st_ftp_mode == 1 && st_ftp_force_mode == '' ){ //case4_1
				notification.array[4] = 'noti_ftp';
				notification.ftp = 1;
				notification.desc[4] = '<#ASUSGATE_note4_1#>';
				notification.action_desc[4] = '<#web_redirect_suggestion_etc#>';
				notification.clickCallBack[4] = "showLoading();setTimeout('document.noti_ftp.submit();', 1);setTimeout('notification.redirectftp()', 2000);";
		}else if(usb_support && !noftp_support && enable_ftp == 1 && st_ftp_mode != 2){	//case4
				notification.array[4] = 'noti_ftp';
				notification.ftp = 1;
				notification.desc[4] = '<#ASUSGATE_note4#>';
				notification.action_desc[4] = '<#ASUSGATE_act_change#>';
				notification.clickCallBack[4] = "showLoading();setTimeout('document.noti_ftp.submit();', 1);setTimeout('notification.redirectftp()', 2000);";
		}else */
			notification.ftp = 0;

/*		if(usb_support && enable_samba == 1 && st_samba_mode == 1 && st_samba_force_mode == ''){ //case5_1
				notification.array[5] = 'noti_samba';
				notification.samba = 1;
				notification.desc[5] = '<#ASUSGATE_note5_1#>';
				notification.action_desc[5] = '<#web_redirect_suggestion_etc#>';	
				notification.clickCallBack[5] = "showLoading();setTimeout('document.noti_samba.submit();', 1);setTimeout('notification.redirectsamba()', 2000);";
		}else if(usb_support && enable_samba == 1 && st_samba_mode != 4){	//case5
				notification.array[5] = 'noti_samba';
				notification.samba = 1;
				notification.desc[5] = '<#ASUSGATE_note5#>';
				notification.action_desc[5] = '<#ASUSGATE_act_change#>';
				notification.clickCallBack[5] = "showLoading();setTimeout('document.noti_samba.submit();', 1);setTimeout('notification.redirectsamba()', 2000);";
		}else */
			notification.samba = 0;

		//Higher priority: DLA intervened case dsltmp_dla_modified 0: default / 1:need to feedback / 2:Feedback submitted 
		//Lower priority: dsl_loss_sync  0: default / 1:need to feedback / 2:Feedback submitted
		// Only DSL-AC68U for now
		if(dsl_loss_sync == 1){         //case9(case10 act) + case6
			
			notification.loss_sync = 1;
			if(dla_modified == 1){
					notification.array[9] = 'noti_dla_modified';
					notification.desc[9] = Untranslated.ASUSGATE_note9;
					notification.action_desc[9] = Untranslated.ASUSGATE_DSL_setting;
					notification.clickCallBack[9] = "location.href = '/Advanced_ADSL_Content.asp?af=dslx_dla_enable';";
					notification.array[10] = 'noti_dla_modified_fb';
					notification.action_desc[10] = Untranslated.ASUSGATE_act_feedback;
					notification.clickCallBack[10] = "location.href = '/Advanced_Feedback.asp';";
			}
			else{
					notification.array[6] = 'noti_loss_sync';
					notification.desc[6] = Untranslated.ASUSGATE_note6;
					notification.action_desc[6] = Untranslated.ASUSGATE_act_feedback;
					notification.clickCallBack[6] = "location.href = '/Advanced_Feedback.asp';";
			}						
		}else
			notification.loss_sync = 0;
			
		//experiencing DSL issue experience_fb=0: notif, 1:no display again.
		if(experience_fb == 0){		//case7
				notification.array[7] = 'noti_experience_FB';
				notification.experience_FB = 1;
				notification.desc[7] = Untranslated.ASUSGATE_note7;
				notification.action_desc[7] = Untranslated.ASUSGATE_act_feedback;
				notification.clickCallBack[7] = "setTimeout('document.noti_experience_Feedback.submit();', 1);setTimeout('notification.redirectFeedback()', 1000);";
		}else
				notification.experience_FB = 0;

		//Notification hint-- null&0: default, 1:display info
		if(noti_notif_Flag == 1 && notif_msg != ""){               //case8
			notification.array[8] = 'noti_notif_hint';
			notification.notif_hint = 1;
			notification.desc[8] = notif_msg;
			notification.action_desc[8] = "<#CTL_ok#>";
			notification.clickCallBack[8] = "setTimeout('document.noti_notif_hint.submit();', 1);setTimeout('notification.redirectHint()', 100);"
		}else
			notification.notif_hint = 0;

		//DLA send debug log  -- 4: send by manual, else: nothing to show
		if(wan_diag_state == "4"){               //case11
			notification.array[11] = 'noti_send_debug_log';
			notification.send_debug_log = 1;
			notification.desc[11] = "-	Diagnostic DSL debug log capture completed.";
			notification.action_desc[11] = "Send debug log now";
			notification.clickCallBack[11] = "setTimeout('notification.redirectFeedbackInfo()', 1000);";
		
		}else
			notification.send_debug_log = 0;

		var browser = getBrowser_info();
		if(browser.ie){
			if(browser.ie.indexOf('8') != "-1" || browser.ie.indexOf('9') != "-1" || browser.ie.indexOf('10') != "-1"){
				notification.ie_legacy = 1;
				notification.array[16] = 'noti_ie_legacy';
				notification.desc[16] = '<#IE_notice1#><#IE_notice2#><#IE_notice3#><#IE_notice4#>';
				notification.action_desc[16] = "";
				notification.clickCallBack[16] = "";
			}
		}

	// Low NVRAM
	if((<% sysinfo("nvram.total"); %> - <% sysinfo("nvram.used"); %>) < 3000){
		notification.array[17] = 'noti_low_nvram';
		notification.low_nvram = 1;
		notification.desc[17] = "Your router is running low on free NVRAM, which might affect its stability.<br>Review nvram-intensive settings such as OpenVPN, or consider doing a factory default reset and reconfiguring.";
		notification.action_desc[17] = "Review System Information now";
		notification.clickCallBack[17] = "location.href = 'Tools_Sysinfo.asp';"
	}else
		notification.low_nvram = 0;

		/*if(is_TW_sku && wan_proto == "pppoe" && is_CHT_pppoe && !is_CHT_pppoe_static){
			notification.pppoe_tw_static = 1;
			notification.array[17] = 'noti_pppoe_tw_static';
			notification.desc[17] = '中華電信撥號連線。是否更改成固定IP撥號連線?<br>須與ISP確認是否有申請此服務';
			notification.action_desc[17] = '改成固定IP撥號連線(PPPoE)';
			notification.clickCallBack[17] = "change_cht_pppoe_static();";					
		}
		else if(is_TW_sku && autodet_state == 2 && autodet_auxstate == 6 && !is_CHT_pppoe_static){*/
		if(is_TW_sku && autodet_state == 2 && autodet_auxstate == 6 && wan_proto != "pppoe"){	
			notification.pppoe_tw = 1;
			notification.array[15] = 'noti_pppoe_tw';
			notification.desc[15] = '<#CHT_ppp_notice_1#>';
			notification.action_desc[15] = '<#CHT_ppp_notice_2#>';
			notification.clickCallBack[15] = "location.href = 'Advanced_WAN_Content.asp?af=wan_proto'";			
		}
		
		if( notification.acpw || notification.upgrade || notification.wifi_2g || notification.wifi_5g || notification.ftp || notification.samba || notification.loss_sync || notification.experience_FB || notification.notif_hint || notification.send_debug_log || notification.mobile_traffic || notification.sim_record || notification.external_ip || notification.pppoe_tw || notification.pppoe_tw_static || notification.ie_legacy || notification.low_nvram){
			notification.stat = "on";
			notification.flash = "on";
			notification.run_notice();
		}
		/*--notification end--*/		
	},

	run_notice: function(){
		var tarObj = document.getElementById("notification_status");
		var tarObj1 = document.getElementById("notification_status1");

		if(tarObj === null)	
			return false;		

		if(this.stat == "on"){
			tarObj1.onclick = this.notiClick;
			tarObj.className = "notification_on";
			tarObj1.className = "notification_on1";
		}

		if(this.flash == "on" && cookie.get("notification_history") != [notification.upgrade, notification.wifi_2g ,notification.wifi_5g ,notification.ftp ,notification.samba ,notification.loss_sync ,notification.experience_FB ,notification.notif_hint, notification.mobile_traffic, notification.send_debug_log, notification.sim_record, notification.external_ip, notification.pppoe_tw, notification.pppoe_tw_static, notification.ie_legacy, notification.low_nvram].join()){
			notification.flashTimer = setInterval(function(){
				tarObj.className = (tarObj.className == "notification_on") ? "notification_off" : "notification_on";
			}, 1000);
		}
	},

	reset: function(){
		this.stat = "off";
		this.flash = "off";
		this.flashTimer = 100;
		this.hoverText = "";
		this.clickText = "";
		this.upgrade = 0;
		this.wifi_2g = 0;
		this.wifi_5g = 0;
		this.ftp = 0;
		this.samba = 0;
		this.loss_sync = 0;
		this.experience_FB = 0;
		this.notif_hint = 0;
		this.mobile_traffic = 0;
		this.send_debug_log = 0;
		this.sim_record = 0;
		this.external_ip = 0;
		this.action_desc = [];
		this.desc = [];
		this.array = [];
		this.clickCallBack = [];
		this.run();
	}
}
