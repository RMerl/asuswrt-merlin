
var keyPressed;
var wItem;
var ip = "";

// 2010.07 James. {
function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	
	return -2;
}

//Filtering ip address with leading zero, if end-user keyin IP 192.168.02.1, system auto filtering IP 192.168.2.1
function ipFilterZero(ip_str){ 
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		return v1+"."+v2+"."+v3+"."+v4;
	}
	return -2;
}

function isMask(ip_str){
	if(!ip_str)
		return 0;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v4 == 255 || !(v4 == 0 || (is1to0(v4) && v1 == 255 && v2 == 255 && v3 == 255)))
			return -4;
		
		if(!(v3 == 0 || (is1to0(v3) && v1 == 255 && v2 == 255)))
			return -3;
		
		if(!(v2 == 0 || (is1to0(v2) && v1 == 255)))
			return -2;
		
		if(!is1to0(v1))
			return -1;
	}
	
	return 1;
}

function is1to0(num){
	if(typeof(num) != "number")
		return 0;
	
	if(num == 255 || num == 254 || num == 252 || num == 248
			|| num == 240 || num == 224 || num == 192 || num == 128)
		return 1;
	
	return 0;
}

function getSubnet(ip_str, mask_str, flag){
	var ip_num, mask_num;
	var sub_head, sub_end;
	
	if(!ip_str || !mask_str)
		return -1;
	
	if(isMask(mask_str) <= 0)
		return -2;
	
	if(!flag || (flag != "head" && flag != "end"))
		flag = "head";
	
	ip_num = inet_network(ip_str);
	mask_num = inet_network(mask_str);
	
	if(ip_num < 0 || mask_num < 0)
		return -3;
	
	sub_head = ip_num-(ip_num&~mask_num);
	sub_end = sub_head+~mask_num;
	
	if(flag == "head")
		return sub_head;
	else
		return sub_end;
}

function str2val(v){
	for(i=0; i<v.length; i++){
		if (v.charAt(i) !='0') 
			break;
	}
	return v.substring(i);
}
function inputRCtrl1(o, flag){
	if (flag == 0){
		o[0].disabled = 1;
		o[1].disabled = 1;
	}
	else{
		o[0].disabled = 0;
		o[1].disabled = 0;
	}
}
function inputRCtrl2(o, flag){
	if (flag == 0){
		o[0].checked = true;
		o[1].checked = false;
	}
	else{
		o[0].checked = false;
		o[1].checked = true;
	}
}

function portrange_min(o, v){
	var num = 0;
	var common_index = o.substring(0, v).indexOf(':');
	
	if(common_index == -1)
		num = parseInt(o.substring(0, v));
	else
		num = parseInt(o.substring(0, common_index));
		
	return num;
}
function portrange_max(o, v){
	var num = 0;
	var common_index = o.substring(0, v).indexOf(':');

	if(common_index == -1)
		num = parseInt(o.substring(0, v));
	else
		num = parseInt(o.substring(common_index+1, v+1));
		
	return num;
}

function entry_cmp(entry, match, len){  //compare string length function	
	var j;
	
	if(entry.length < match.length)
		return (1);
	
	for(j=0; j < entry.length && j<len; j++){
		c1 = entry.charCodeAt(j);
		if (j>=match.length) 
			c2=160;
		else 
			c2 = match.charCodeAt(j);
			
		if (c1==160) 
			c1 = 32;
			
		if (c2==160) 
			c2 = 32;
			
		if (c1>c2) 
			return (1);
		else if (c1<c2) 
			return(-1);
	}
	return 0;
}

function add_options_x2(o, arr, varr, orig){
	free_options(o);
	
	for(var i = 0; i < arr.length; ++i){
		if(orig == varr[i])
			add_option(o, arr[i], varr[i], 1);
		else
			add_option(o, arr[i], varr[i], 0);
	}
}

function rcheck(o){
	if(o.value == "1")
		return("1");
	else
		return("0");
}

function getDateCheck(str, pos){
	if (str.charAt(pos) == '1')
		return true;
	else
		return false;
}
function getTimeRange(str, pos){
	if (pos == 0)
		return str.substring(0,2);
	else if (pos == 1)
		return str.substring(2,4);
	else if (pos == 2)
		return str.substring(4,6);
	else if (pos == 3)
		return str.substring(6,8);
}
function setDateCheck(d1, d2, d3, d4, d5, d6, d7){
	str = "";
	if (d7.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d6.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d5.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d4.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d3.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d2.checked == true ) str = "1" + str;
		else str = "0" + str;
	if (d1.checked == true ) str = "1" + str;
		else str = "0" + str;
		
	return str;
}
function setTimeRange(sh, sm, eh, em){
	return(sh.value+sm.value+eh.value+em.value);
}

function change_firewall(r){
	if (r == "0"){
		inputCtrl(document.form.fw_log_x, 0);
		inputRCtrl1(document.form.fw_dos_x, 0);
		inputRCtrl1(document.form.misc_ping_x, 0);
	}
	else{		//r=="1"
		inputCtrl(document.form.fw_log_x, 1);
		inputRCtrl1(document.form.fw_dos_x, 1);
		inputRCtrl1(document.form.misc_ping_x, 1);
	}
}

function change_wireless_bridge(m){
	if (m == "0"){
		inputRCtrl2(document.form.wl_wdsapply_x, 1);
		inputRCtrl1(document.form.wl_wdsapply_x, 0);
		if(Qcawifi_support)
		{
			inputRCtrl2(document.form.wl_wds_vht, 1);
			inputRCtrl1(document.form.wl_wds_vht, 0);
		}   
	}else if (m == "1" && Rawifi_support){	 // N66U-spec
		inputRCtrl2(document.form.wl_wdsapply_x, 0);
		inputRCtrl1(document.form.wl_wdsapply_x, 0);
	}else{
		inputRCtrl1(document.form.wl_wdsapply_x, 1);
		if(Qcawifi_support)
			inputRCtrl1(document.form.wl_wds_vht, 1);
		
	}
}

function onSubmitCtrl(o, s) {
	document.form.action_mode.value = s;
	return true;
}

function onSubmitCtrlOnly(o, s){
	if(s != 'Upload' && s != 'Upload1')
		document.form.action_mode.value = s;
	
	if(s == 'Upload1'){
		if(document.form.file.value){
			disableCheckChangedStatus();			
			dr_advise();
			document.form.submit();			
		}
		else{
			alert("<#JS_Shareblanktest#>");
			document.form.file.focus();
		}
	}
	stopFlag = 1;
	return true;
}

function handle_11ac_80MHz(){
	if(band5g_support == false || band5g_11ac_support == false || document.form.wl_unit[0].selected == true || document.form.wl_nmode_x.value=='2' || document.form.wl_nmode_x.value=='1') {
		document.form.wl_bw[0].text = "20/40 MHz";
		document.form.wl_bw.remove(3); //remove 80 Mhz when not when not required required
	} else {
		document.form.wl_bw[0].text = "20/40/80 MHz";
		if(document.form.wl_bw.length == 3)
			document.form.wl_bw[3] = new Option("80 MHz", "3");
	}
}

function change_ddns_setting(v){
		var hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
		if (v == "WWW.ASUS.COM"){
				document.getElementById("ddns_hostname_info_tr").style.display = "none";
				document.getElementById("ddns_hostname_tr").style.display="";
				document.form.ddns_hostname_x.parentNode.style.display = "none";
				document.form.DDNSName.parentNode.style.display = "";
				var ddns_hostname_title = hostname_x.substring(0, hostname_x.indexOf('.asuscomm.com'));
				if(hostname_x != '' && ddns_hostname_title)
						document.getElementById("DDNSName").value = ddns_hostname_title;						
				else
						document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
	
				inputCtrl(document.form.ddns_username_x, 0);
				inputCtrl(document.form.ddns_passwd_x, 0);
				document.form.ddns_wildcard_x[0].disabled= 1;
				document.form.ddns_wildcard_x[1].disabled= 1;
				showhide("link", 0);
				showhide("linkToHome", 0);	
				showhide("wildcard_field",0);
				document.form.ddns_regular_check.value = 0;
				showhide("check_ddns_field", 0);
				inputCtrl(document.form.ddns_regular_period, 0);
				showhide("customnote", 0);
				showhide("need_custom_scripts", 0);
		}else if (v == "CUSTOM"){
				document.form.ddns_hostname_x.parentNode.style.display = "";
				document.form.DDNSName.parentNode.style.display = "none";
				inputCtrl(document.form.ddns_username_x, 0);
				inputCtrl(document.form.ddns_passwd_x, 0);
				document.form.ddns_wildcard_x[0].disabled= 1;
				document.form.ddns_wildcard_x[1].disabled= 1;
				showhide("customnote", 1);
				showhide("link", 0);
				showhide("linkToHome", 0);
				showhide("wildcard_field",0);
				showhide("check_ddns_field", 0);
				if (('<% nvram_get("jffs2_enable"); %>' != '1') || ('<% nvram_get("jffs2_scripts"); %>' != '1'))
					showhide("need_custom_scripts", 1);
				else
					showhide("need_custom_scripts", 0);
		}
		else if( v == "WWW.ORAY.COM"){
			document.getElementById("ddns_hostname_tr").style.display="none";
			inputCtrl(document.form.ddns_username_x, 1);
			inputCtrl(document.form.ddns_passwd_x, 1);
			document.form.ddns_wildcard_x[0].disabled= 1;
			document.form.ddns_wildcard_x[1].disabled= 1;
			showhide("link", 1);
			showhide("linkToHome", 0);
			showhide("wildcard_field",0);
			document.form.ddns_regular_check.value = 0;
			showhide("check_ddns_field", 0);
			inputCtrl(document.form.ddns_regular_period, 0);			
		}
		else{
				document.getElementById("ddns_hostname_info_tr").style.display = "none";
				document.getElementById("ddns_hostname_tr").style.display="";
				document.form.ddns_hostname_x.parentNode.style.display = "";
				document.form.DDNSName.parentNode.style.display = "none";
				inputCtrl(document.form.ddns_username_x, 1);
				inputCtrl(document.form.ddns_passwd_x, 1);
				var disable_wild = 0;
				if(v == "WWW.TUNNELBROKER.NET" || v == "WWW.NAMECHEAP.COM" || v == "WWW.SELFHOST.DE")
					var disable_wild = 1;
				else
					var disable_wild = 0;
				document.form.ddns_wildcard_x[0].disabled= disable_wild;
				document.form.ddns_wildcard_x[1].disabled= disable_wild;
				if(v == "WWW.ZONEEDIT.COM" || v == "WWW.NAMECHEAP.COM"){	 // Jieming added at 2013/03/06, remove free trail of zoneedit and add a link to direct to zoneedit 
					showhide("link", 0);
					showhide("linkToHome", 1);
				}
				else{
					showhide("link", 1);
					showhide("linkToHome", 0);				
				}
				
				showhide("wildcard_field",!disable_wild);
				showhide("check_ddns_field", 1);
				if(document.form.ddns_regular_check.value == 0)
					inputCtrl(document.form.ddns_regular_period, 0);
				else
					inputCtrl(document.form.ddns_regular_period, 1);
				showhide("customnote", 0);
				showhide("need_custom_scripts", 0);
		}
		if(v == "WWW.NAMECHEAP.COM")
			document.getElementById("ddns_username_th").innerHTML = Untranslated.namecheap_username_title;
		else
			document.getElementById("ddns_username_th").innerHTML = "<#LANHostConfig_x_DDNSUserName_itemname#>";
}

function change_common_radio(o, s, v, r){
	if(v == "ddns_enable_x"){
		var hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
		var ddns_updated = '<% nvram_get("ddns_updated"); %>';
		if(r == 1){
			inputCtrl(document.form.ddns_server_x, 1);
			if('<% nvram_get("ddns_server_x"); %>' == 'WWW.ASUS.COM'){
				document.form.DDNSName.disabled = false;
				document.form.DDNSName.parentNode.parentNode.parentNode.style.display = "";
				if(hostname_x != ''){
					var ddns_hostname_title = hostname_x.substring(0, hostname_x.indexOf('.asuscomm.com'));
					if(!ddns_hostname_title)
						document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
					else
						document.getElementById("DDNSName").value = ddns_hostname_title;			
				}else{
					document.getElementById("DDNSName").value = "<#asusddns_inputhint#>";
				}
				showhide("wildcard_field",0);
			}else{
				if(document.form.ddns_server_x.value == "WWW.ORAY.COM"){
					if(ddns_updated == "1")
						document.getElementById("ddns_hostname_info_tr").style.display = "";
				}
				else
					document.form.ddns_hostname_x.parentNode.parentNode.parentNode.style.display = "";
				inputCtrl(document.form.ddns_username_x, 1);
				inputCtrl(document.form.ddns_passwd_x, 1);
				showhide("wildcard_field",1);				
			}
			change_ddns_setting(document.form.ddns_server_x.value);			
		}else{
			if(document.form.ddns_server_x.value == "WWW.ASUS.COM"){
				document.form.DDNSName.parentNode.parentNode.parentNode.style.display = "none";
			}
			else{
				if(document.form.ddns_server_x.value == "WWW.ORAY.COM")
					document.getElementById("ddns_hostname_info_tr").style.display = "none";
				document.form.ddns_hostname_x.parentNode.parentNode.parentNode.style.display = "none";
				inputCtrl(document.form.ddns_username_x, 0);
				inputCtrl(document.form.ddns_passwd_x, 0);			
			}
			inputCtrl(document.form.ddns_server_x, 0);
			document.form.ddns_wildcard_x[0].disabled= 1;
			document.form.ddns_wildcard_x[1].disabled= 1;
			showhide("wildcard_field",0);
			document.form.ddns_regular_check.value = 0;
			showhide("check_ddns_field", 0);
			inputCtrl(document.form.ddns_regular_period, 0);
		}	
	}
	else if(v == "wan_dnsenable_x"){
		if(r == 1){
			inputCtrl(document.form.wan_dns1_x, 0);
			inputCtrl(document.form.wan_dns2_x, 0);
		}
		else{
			inputCtrl(document.form.wan_dns1_x, 1);
			inputCtrl(document.form.wan_dns2_x, 1);
		}
	}
	else if(v=="fw_enable_x"){
		change_firewall(r);
	}else if(v=="wl_closed"){
			if(r==1)
					showhide("WPS_hideSSID_hint",1);
			else
					showhide("WPS_hideSSID_hint",0);
	}
	
	return true;
}


function change_wlweptype(o, isload){
	var wflag = 0;

	if(o.value != "0"){
		wflag = 1;
		if(document.form.wl_phrase_x.value.length > 0 && isload == 0)
			is_wlphrase("WLANConfig11b", "wl_phrase_x", document.form.wl_phrase_x);
	}

	wl_wep_change();
}

function change_wlkey(o, s){
		wep = document.form.wl_wep_x.value;
	
	if(wep == "1"){
		if(o.value.length > 10)
			o.value = o.value.substring(0, 10);
	}
	else if(wep == "2"){
		if(o.value.length > 26)
			o.value = o.value.substring(0, 26);
	}
	
	return true;
}


function subnetPrefix(ip, orig, count)
{r='';
c=0;
for(i=0;i<ip.length;i++)
{if (ip.charAt(i) == '.')
c++;
if (c==count) break;
r = r + ip.charAt(i);
}
c=0;
for(i=0;i<orig.length;i++)
{if (orig.charAt(i) == '.')
{c++;
}
if (c>=count)
r = r + orig.charAt(i);
}
return (r);
}

//Viz add 2011.10 For DHCP pool changed
function subnetPostfix(ip, num, count){		//Change subnet postfix .xxx
	r='';
	orig="";
	c=0;
	for(i=0;i<ip.length;i++){
			if (ip.charAt(i) == '.')	c++;
			r = r + ip.charAt(i);
			if (c==count) break;			
	}
	c=0;
	orig = String(num);
	for(i=0;i<orig.length;i++){
			r = r + orig.charAt(i);
	}
	return (r);
}

function openLink(s){
	if (s == 'x_DDNSServer'){
		if (document.form.ddns_server_x.value.indexOf("WWW.DYNDNS.ORG")!=-1)
			tourl = "https://account.dyn.com/services/zones/svc/add.html?_add_dns=c&trial=standarddns";
		else if (document.form.ddns_server_x.value == 'WWW.ZONEEDIT.COM')
			tourl = "http://www.zoneedit.com/";
		else if (document.form.ddns_server_x.value == 'WWW.SELFHOST.DE')
			tourl = "http://WWW.SELFHOST.DE";
		else if (document.form.ddns_server_x.value == 'WWW.DNSOMATIC.COM')
			tourl = "http://dnsomatic.com/create/";
		else if (document.form.ddns_server_x.value == 'WWW.TUNNELBROKER.NET')
			tourl = "http://www.tunnelbroker.net/register.php";
		else if (document.form.ddns_server_x.value == 'WWW.ASUS.COM')
			tourl = "";
		else if (document.form.ddns_server_x.value == 'WWW.NO-IP.COM')
			tourl = "http://www.no-ip.com/newUser.php";
		else if (document.form.ddns_server_x.value == 'WWW.NAMECHEAP.COM')
			tourl = "https://www.namecheap.com";
		else if (document.form.ddns_server_x.value == 'WWW.ORAY.COM')
			tourl = "http://www.oray.com/";
		else	tourl = "";
		link = window.open(tourl, "DDNSLink","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");
	}
	else if (s=='x_NTPServer1'){
		tourl = "http://support.ntp.org/bin/view/Main/WebHome";
		link = window.open(tourl, "NTPLink","toolbar=yes,location=yes,directories=no,status=yes,menubar=yes,scrollbars=yes,resizable=yes,copyhistory=no,width=640,height=480");
	}
}

/* input : s: service id, v: value name, o: current value */
/* output: wep key1~4       */
function is_wlphrase(s, v, o){
	var pseed = new Array;
	var wep_key = new Array(5);

	var valid_WPAPSK = function(o){
		if(o.value.length >= 64){
			o.value = o.value.substring(0, 63);
			alert("<#JS_wpapass#>");
			return false;
		}
		
		return true;
	}
	
	if(v=='wl_wpa_psk')
		return(valid_WPAPSK(o));

		// note: current_page == "Advanced_Wireless_Content.asp"
		wepType = document.form.wl_wep_x.value;
		wepKey1 = document.form.wl_key1;
		wepKey2 = document.form.wl_key2;
		wepKey3 = document.form.wl_key3;
		wepKey4 = document.form.wl_key4;

	
	phrase = o.value;
	if(wepType == "1"){
		for(var i = 0; i < phrase.length; i++){
			pseed[i%4] ^= phrase.charCodeAt(i);
		}
		
		randNumber = pseed[0] | (pseed[1]<<8) | (pseed[2]<<16) | (pseed[3]<<24);
		for(var j = 0; j < 5; j++){
			randNumber = ((randNumber*0x343fd)%0x1000000);
			randNumber = ((randNumber+0x269ec3)%0x1000000);
			wep_key[j] = ((randNumber>>16)&0xff);
		}
		
		wepKey1.value = binl2hex_c(wep_key);
		for(var j = 0; j < 5; j++){
			randNumber = ((randNumber * 0x343fd) % 0x1000000);
			randNumber = ((randNumber + 0x269ec3) % 0x1000000);
			wep_key[j] = ((randNumber>>16) & 0xff);
		}
		
		wepKey2.value = binl2hex_c(wep_key);
		for(var j = 0; j < 5; j++){
			randNumber = ((randNumber * 0x343fd) % 0x1000000);
			randNumber = ((randNumber + 0x269ec3) % 0x1000000);
			wep_key[j] = ((randNumber>>16) & 0xff);
		}
		
		wepKey3.value = binl2hex_c(wep_key);
		for(var j = 0; j < 5; j++){
			randNumber = ((randNumber * 0x343fd) % 0x1000000);
			randNumber = ((randNumber + 0x269ec3) % 0x1000000);
			wep_key[j] = ((randNumber>>16) & 0xff);
		}
		
		wepKey4.value = binl2hex_c(wep_key);
	}
	else if(wepType == "2"){
		password = "";
		
		if(phrase.length > 0){
			for(var i = 0; i < 64; i++){
				ch = phrase.charAt(i%phrase.length);
				password = password+ch;
			}
		}
		
		for(i=0;i<4;i++){
			password = calcMD5(password);
			if(i == 0)
				wepKey1.value = password.substr(0, 26);
			else if(i == 1)
				wepKey2.value = password.substr(0, 26);
			else if(i == 2)
				wepKey3.value = password.substr(0, 26);
			else
				wepKey4.value = password.substr(0, 26);
		}
	}
	
	return true;
}

function wl_wep_change(){
	var mode = document.form.wl_auth_mode_x.value;
	var wep = document.form.wl_wep_x.value;

	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2"){
		if(mode != "wpa" && mode != "wpa2" && mode != "wpawpa2"){
			inputCtrl(document.form.wl_crypto, 1);
			inputCtrl(document.form.wl_wpa_psk, 1);
		}
		inputCtrl(document.form.wl_wpa_gtk_rekey, 1);
		inputCtrl(document.form.wl_wep_x, 0);
		inputCtrl(document.form.wl_phrase_x, 0);
		inputCtrl(document.form.wl_key1, 0);
		inputCtrl(document.form.wl_key2, 0);
		inputCtrl(document.form.wl_key3, 0);
		inputCtrl(document.form.wl_key4, 0);
		inputCtrl(document.form.wl_key, 0);
	}
	else if(mode == "radius"){ //2009.01 magic
		inputCtrl(document.form.wl_crypto, 0);
		inputCtrl(document.form.wl_wpa_psk, 0);
		inputCtrl(document.form.wl_wpa_gtk_rekey, 0);
		inputCtrl(document.form.wl_wep_x, 0); //2009.0310 Lock 
		inputCtrl(document.form.wl_phrase_x, 0);
		inputCtrl(document.form.wl_key1, 0);
		inputCtrl(document.form.wl_key2, 0);
		inputCtrl(document.form.wl_key3, 0);
		inputCtrl(document.form.wl_key4, 0);
		inputCtrl(document.form.wl_key, 0);
	}
	else{
		inputCtrl(document.form.wl_crypto, 0);
		inputCtrl(document.form.wl_wpa_psk, 0);
		inputCtrl(document.form.wl_wpa_gtk_rekey, 0);
		if(mode == "open" && document.form.wl_nmode_x.value != 2){
			document.form.wl_wep_x.parentNode.parentNode.style.display = "none";
			document.form.wl_wep_x.value = "0";
		}	
		else	
			inputCtrl(document.form.wl_wep_x, 1);
			
		if(wep != "0"){
			inputCtrl(document.form.wl_phrase_x, 1);
			inputCtrl(document.form.wl_key1, 1);
			inputCtrl(document.form.wl_key2, 1);
			inputCtrl(document.form.wl_key3, 1);
			inputCtrl(document.form.wl_key4, 1);
			inputCtrl(document.form.wl_key, 1);
		}
		else{
			inputCtrl(document.form.wl_phrase_x, 0);
			inputCtrl(document.form.wl_key1, 0);
			inputCtrl(document.form.wl_key2, 0);
			inputCtrl(document.form.wl_key3, 0);
			inputCtrl(document.form.wl_key4, 0);
			inputCtrl(document.form.wl_key, 0);
		}
	}
	
	change_key_des();	// 2008.01 James.
}

function change_wep_type(mode, isload){
	var cur_wep = document.form.wl_wep_x.value;
	var wep_type_array;
	var value_array;
	
	free_options(document.form.wl_wep_x);
	if(mode == "shared"){ //2009.0310 Lock
		wep_type_array = new Array("WEP-64bits", "WEP-128bits");
		value_array = new Array("1", "2");
	}
	else{
		if(document.form.wl_nmode_x.value != 2 && sw_mode != 2){
			wep_type_array = new Array("None");
			value_array = new Array("0");					
		}
		else{
			wep_type_array = new Array("None", "WEP-64bits", "WEP-128bits");
			value_array = new Array("0", "1", "2");
		}
	}

	add_options_x2(document.form.wl_wep_x, wep_type_array, value_array, cur_wep);	
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "radius") //2009.03 magic
		document.form.wl_wep_x.value = "0";
		
	change_wlweptype(document.form.wl_wep_x, isload);
}

function insertExtChannelOption(){
	if('<% nvram_get("wl_unit"); %>' == '1'){
				insertExtChannelOption_5g();	
	}else{
				insertExtChannelOption_2g();
	}	
}

function insertExtChannelOption_5g(){
    var country = "<% nvram_get("wl1_country_code"); %>";
    var orig = document.form.wl_channel.value;
    free_options(document.form.wl_channel);
		if(wl_channel_list_5g != ""){	//With wireless channel 5g hook
				wl_channel_list_5g = eval('<% channel_list_5g(); %>');
				if(document.form.wl_bw.value != "0" && document.form.wl_nmode_x.value != "2")
				{ //not Legacy mode and BW > 20MHz
					var i;
					//cut channels >= 165 when bw != 20MHz
					for(i=0; i < wl_channel_list_5g.length; i++)
						if(wl_channel_list_5g[i] == "165")
						{
							wl_channel_list_5g.splice(i,(wl_channel_list_5g.length - i));
							break;
						}
					for(i=0; i < wl_channel_list_5g.length; i++)
					{
						//remove ch56 when bw == 40MHz or remove ch56,60,64 when bw == 80MHz, on NO ch52 is provided.
						if(wl_channel_list_5g[i] == "56" && (i == 0 || wl_channel_list_5g[i-1] != "52"))
						{
							if(!(Rawifi_support || Qcawifi_support))
								;
							else if(band5g_11ac_support && (document.form.wl_bw.value == "3" || document.form.wl_bw.value == "1") && (document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "8"))
							{
								for(var j=wl_channel_list_5g.length; j>=i ; j--)
									if(wl_channel_list_5g[j] >= "56" && wl_channel_list_5g[j] <= "64")
										wl_channel_list_5g.splice(j,1);
								i--;
							} else {
								wl_channel_list_5g.splice(i,1);
								i--;
							}
						}
						//remove ch116 when bw != 20MHz when bw == 80MHz, on NO ch120 is provided.
						if(wl_channel_list_5g[i] == "116" && (i + 1 < wl_channel_list_5g.length && wl_channel_list_5g[i+1] != "120"))
						{
							if(Rawifi_support || Qcawifi_support)
							{
								wl_channel_list_5g.splice(i,1);
								i--;
							}
						}
						//remove ch132~140 when bw == 80MHz or ch140 when bw != 20MHz, on NO ch120 is provided.
						if(!(Rawifi_support || Qcawifi_support))
							;
						else if((document.form.wl_bw.value == "3" || document.form.wl_bw.value == "1") && (document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "8"))
						{
							if(wl_channel_list_5g[i] == "132" || wl_channel_list_5g[i] == "136" || wl_channel_list_5g[i] == "140")
							{
								wl_channel_list_5g.splice(i,1);
								i--;
							}
						}
						else if(wl_channel_list_5g[i] == "140")
						{
							wl_channel_list_5g.splice(i,1);
							i--;
						}
					}
				}
				if(wl_channel_list_5g[0] != "<#Auto#>")
						wl_channel_list_5g.splice(0,0,"0");
												
				channels = wl_channel_list_5g;
		}else{   	//start Without wireless channel 5g hook
        if (document.form.wl_bw.value == "0"){ // 20 MHz
						inputCtrl(document.form.wl_nctrlsb, 0);
                	if (country == "AL" || 
                	 country == "DZ" || 
			 country == "AU" || 
			 country == "BH" || 
              	         country == "BY" ||
              	         country == "CA" || 
              	         country == "CL" || 
              	         country == "CO" || 
                	 country == "CR" ||
                	 country == "DO" || 
                	 country == "SV" || 
                	 country == "GT" || 
			 country == "HN" || 
			 country == "HK" || 
              	         country == "IN" ||
              	         country == "IL" || 
              	         country == "JO" || 
              	         country == "KZ" || 
                	 country == "LB" ||
                	 country == "MO" || 
                	 country == "MK" ||                	 
                	 country == "MY" ||
                	 country == "MX" || 
			 country == "NZ" || 
			 country == "NO" || 
              	         country == "OM" ||
              	         country == "PK" || 
              	         country == "PA" || 
              	         country == "PR" || 
                	 country == "QA" ||
                	 country == "RO" || 
                	 country == "RU" || 
                	 country == "SA" || 
			 country == "SG" || 
			 country == "SY" || 
              	         country == "TH" ||
              	         country == "UA" || 
              	         country == "AE" || 
              	         country == "US" || 
              	         country == "Q2" || 
                	 country == "VN" ||
                	 country == "YE" || 
                	 country == "ZW")
                		channels = new Array(0, 36, 40, 44, 48, 149, 153, 157, 161, 165); // Region 0
                		
                	else if(country == "AT" ||
                		country == "BE" ||
            		    	country == "BR" ||
            		    	country == "BG" ||
            		    	country == "CY" || 
            		    	country == "DK" || 
            		    	country == "EE" ||
            		    	country == "FI" || 
            	  	        country == "DE" || 
            	  	        country == "GR" || 
                		country == "HU" ||
             		   	country == "IS" ||
             		   	country == "IE" || 
            		    	country == "IT" || 
            		    	country == "LV" ||
            		    	country == "LI" || 
            		    	country == "LT" || 
            		    	country == "LU" || 
            		    	country == "NL" ||
            		    	country == "PL" || 
            		    	country == "PT" || 
            		    	country == "SK" || 
            		    	country == "SI" ||
            		    	country == "ZA" || 
            		    	country == "ES" || 
            		    	country == "SE" || 
            		    	country == "CH" ||
            		    	country == "GB" || 
            		    	country == "EU" || 
            		    	country == "UZ")
                		channels = new Array(0, 36, 40, 44, 48);  // Region 1
                	
                	else if(country == "AM" ||
            		    	country == "AZ" || 
            		    	country == "HR" ||
            		    	country == "CZ" || 
            		    	country == "EG" || 
            		    	country == "FR" || 
            		    	country == "GE" ||
            		    	country == "MC" ||
            		    	country == "TT" || 
            		    	country == "TN" ||
            		    	country == "TR")
                		channels = new Array(0, 36, 40, 44, 48);  // Region 2
                	
                	else if(country == "AR" || country == "TW")
				channels = new Array(0, 52, 56, 60, 64, 149, 153, 157, 161, 165);  // Region 3
                	
                	else if(country == "BZ" ||
                		country == "BO" || 
            		    	country == "BN" ||
            		    	country == "CN" || 
            		    	country == "ID" || 
            		    	country == "IR" || 
            		    	country == "PE" ||
            		    	country == "PH")
                		channels = new Array(0, 149, 153, 157, 161, 165);  // Region 4
                	
                	else if(country == "KP" ||
                		country == "KR" || 
            		    	country == "UY" ||
            		    	country == "VE")
                		channels = new Array(0, 149, 153, 157, 161, 165);  // Region 5
                   	
									else if(country == "JP")
                		channels = new Array(0, 36, 40, 44, 48); // Region 9
									
									else
                		channels = new Array(0, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165); // Region 7
                }
								else if(document.form.wl_bw.value == "1"){  // 20/40 MHz
									inputCtrl(document.form.wl_nctrlsb, 1);
                	if (country == "AL" || 
                	 country == "DZ" || 
			 country == "AU" || 
			 country == "BH" || 
              	         country == "BY" ||
              	         country == "CA" || 
              	         country == "CL" || 
              	         country == "CO" || 
                	 country == "CR" ||
                	 country == "DO" || 
                	 country == "SV" || 
                	 country == "GT" || 
			 country == "HN" || 
			 country == "HK" || 
              	         country == "IN" ||
              	         country == "IL" || 
              	         country == "JO" || 
              	         country == "KZ" || 
                	 country == "LB" ||
                	 country == "MO" || 
                	 country == "MK" ||                	 
                	 country == "MY" ||
                	 country == "MX" || 
			 country == "NZ" || 
			 country == "NO" || 
              	         country == "OM" ||
              	         country == "PK" || 
              	         country == "PA" || 
              	         country == "PR" || 
                	 country == "QA" ||
                	 country == "RO" || 
                	 country == "RU" || 
                	 country == "SA" || 
			 country == "SG" || 
			 country == "SY" || 
              	         country == "TH" ||
              	         country == "UA" || 
              	         country == "AE" || 
              	         country == "US" || 
              	         country == "Q2" || 
                	 country == "VN" ||
                	 country == "YE" || 
                	 country == "ZW")
                		channels = new Array(0, 36, 40, 44, 48, 149, 153, 157, 161); // Region 0
                		
                	else if(country == "AT" ||
                		country == "BE" ||
            		    	country == "BR" ||
            		    	country == "BG" ||
            		    	country == "CY" || 
            		    	country == "DK" || 
            		    	country == "EE" ||
            		    	country == "FI" || 
            	  	        country == "DE" || 
            	  	        country == "GR" || 
                		country == "HU" ||
             		   	country == "IS" ||
             		   	country == "IE" || 
            		    	country == "IT" || 
            		    	country == "LV" ||
            		    	country == "LI" || 
            		    	country == "LT" || 
            		    	country == "LU" || 
            		    	country == "NL" ||
            		    	country == "PL" || 
            		    	country == "PT" || 
            		    	country == "SK" || 
            		    	country == "SI" ||
            		    	country == "ZA" || 
            		    	country == "ES" || 
            		    	country == "SE" || 
            		    	country == "CH" ||
            		    	country == "GB" || 
            		    	country == "EU" || 
            		    	country == "UZ")
                		channels = new Array(0, 36, 40, 44, 48); // Region 1
                	
                	else if(country == "AM" ||
            		    	country == "AZ" || 
            		    	country == "HR" ||
            		    	country == "CZ" || 
            		    	country == "EG" || 
            		    	country == "FR" || 
            		    	country == "GE" ||
            		    	country == "MC" ||
            		    	country == "TT" || 
            		    	country == "TN" ||
            		    	country == "TR")
                		channels = new Array(0, 36, 40, 44, 48); // Region 2
                	
                	else if(country == "AR" || country == "TW")
				channels = new Array(0, 52, 56, 60, 64, 149, 153, 157, 161);  // Region 3
                	
                	else if(country == "BZ" ||
                		country == "BO" || 
            		    	country == "BN" ||
            		    	country == "CN" || 
            		    	country == "ID" || 
            		    	country == "IR" || 
            		    	country == "PE" ||
            		    	country == "PH")
                		channels = new Array(0, 149, 153, 157, 161); // Region 4
                	
                	else if(country == "KP" ||
                		country == "KR" || 
            		    	country == "UY" ||
            		    	country == "VE")
                		channels = new Array(0, 149, 153, 157, 161); // Region 5
                	
									else if(country == "JP")
                		channels = new Array(0, 36, 40, 44, 48); // Region 9

									else
                		channels = new Array(0, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161); // Region 7
                }		
                else{  // 40 MHz
                	inputCtrl(document.form.wl_nctrlsb, 1);
                	if (country == "AL" || 
                	 country == "DZ" || 
			 country == "AU" || 
			 country == "BH" || 
              	         country == "BY" ||
              	         country == "CA" || 
              	         country == "CL" || 
              	         country == "CO" || 
                	 country == "CR" ||
                	 country == "DO" || 
                	 country == "SV" || 
                	 country == "GT" || 
			 country == "HN" || 
			 country == "HK" || 
              	         country == "IN" ||
              	         country == "IL" || 
              	         country == "JO" || 
              	         country == "KZ" || 
                	 country == "LB" ||
                	 country == "MO" || 
                	 country == "MK" ||                	 
                	 country == "MY" ||
                	 country == "MX" || 
			 country == "NZ" || 
			 country == "NO" || 
              	         country == "OM" ||
              	         country == "PK" || 
              	         country == "PA" || 
              	         country == "PR" || 
                	 country == "QA" ||
                	 country == "RO" || 
                	 country == "RU" || 
                	 country == "SA" || 
			 country == "SG" || 
			 country == "SY" || 
              	         country == "TH" ||
              	         country == "UA" || 
              	         country == "AE" || 
              	         country == "US" || 
              	         country == "Q2" || 
                	 country == "VN" ||
                	 country == "YE" || 
                	 country == "ZW")
                		channels = new Array(0, 36, 40, 44, 48, 149, 153, 157, 161);
                		
                	else if(country == "AT" ||
                		country == "BE" ||
            		    	country == "BR" ||
            		    	country == "BG" ||
            		    	country == "CY" || 
            		    	country == "DK" || 
            		    	country == "EE" ||
            		    	country == "FI" || 
            	  	        country == "DE" || 
            	  	        country == "GR" || 
                		country == "HU" ||
             		   	country == "IS" ||
             		   	country == "IE" || 
            		    	country == "IT" || 
            		    	country == "LV" ||
            		    	country == "LI" || 
            		    	country == "LT" || 
            		    	country == "LU" || 
            		    	country == "NL" ||
            		    	country == "PL" || 
            		    	country == "PT" || 
            		    	country == "SK" || 
            		    	country == "SI" ||
            		    	country == "ZA" || 
            		    	country == "ES" || 
            		    	country == "SE" || 
            		    	country == "CH" ||
            		    	country == "GB" || 
            		    	country == "EU" || 
            		    	country == "UZ")
                		channels = new Array(0, 36, 40, 44, 48);
                	
                	else if(country == "AM" ||
            		    	country == "AZ" || 
            		    	country == "HR" ||
            		    	country == "CZ" || 
            		    	country == "EG" || 
            		    	country == "FR" || 
            		    	country == "GE" ||
            		    	country == "MC" ||
            		    	country == "TT" || 
            		    	country == "TN" ||
            		    	country == "TR")
                		channels = new Array(0, 36, 40, 44, 48);
                	
                	else if(country == "AR" || country == "TW")
				channels = new Array(0, 52, 56, 60, 64, 149, 153, 157, 161);  // Region 3
                	
                	else if(country == "BZ" ||
                		country == "BO" || 
            		    	country == "BN" ||
            		    	country == "CN" || 
            		    	country == "ID" || 
            		    	country == "IR" || 
            		    	country == "PE" ||
            		    	country == "PH")
                		channels = new Array(0, 149, 153, 157, 161);
                	
                	else if(country == "KP" ||
                		country == "KR" || 
            		    	country == "UY" ||
            		    	country == "VE")
                		channels = new Array(0, 149, 153, 157, 161);
                	
									else if(country == "JP")
                		channels = new Array(0, 36, 40, 44, 48);

									else
                		channels = new Array(0, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161);
                }                
    }	//end Without wireless channel 5g hook
    
        var ch_v = new Array();
        for(var i=0; i<channels.length; i++){
        	ch_v[i] = channels[i];
        }
        if(ch_v[0] == "0")
        	channels[0] = "<#Auto#>";
        add_options_x2(document.form.wl_channel, channels, ch_v, orig);
				var x = document.form.wl_nctrlsb;
				//x.remove(x.selectedIndex);
				free_options(x);
				add_option(x, "<#Auto#>", "lower");
				x.selectedIndex = 0;
}

function insertExtChannelOption_2g(){
	var orig2 = document.form.wl_channel.value;
	var wmode = document.form.wl_nmode_x.value;	
  free_options(document.form.wl_channel);
  
  if(wl_channel_list_2g != ""){
  			wl_channel_list_2g = eval('<% channel_list_2g(); %>');
  			if(wl_channel_list_2g[0] != "<#Auto#>")
  					wl_channel_list_2g.splice(0,0,"0");
        var ch_v2 = new Array();
        for(var i=0; i<wl_channel_list_2g.length; i++){
        	ch_v2[i] = wl_channel_list_2g[i];
        }
        if(ch_v2[0] == "0")
        	wl_channel_list_2g[0] = "<#Auto#>";                	
        add_options_x2(document.form.wl_channel, wl_channel_list_2g, ch_v2, orig2);
	}else{
			document.form.wl_channel.innerHTML = '<% select_channel("WLANConfig11b"); %>';
	}
	
	var CurrentCh = document.form.wl_channel.value;	
	var option_length = document.form.wl_channel.options.length;
	if ((wmode == "0"||wmode == "1") && document.form.wl_bw.value != "0"){
		inputCtrl(document.form.wl_nctrlsb, 1);
		var x = document.form.wl_nctrlsb;
		var length = document.form.wl_nctrlsb.options.length;
		if (length > 1){
			x.selectedIndex = 1;
			x.remove(x.selectedIndex);
		}
		
		if ((CurrentCh >=1) && (CurrentCh <= 4)){
			x.options[0].text = "Above";
			x.options[0].value = "lower";
		}
		else if ((CurrentCh >= 5) && (CurrentCh <= 7)){
			x.options[0].text = "Above";
			x.options[0].value = "lower";
			add_option(document.form.wl_nctrlsb, "Below", "upper");
			if (document.form.wl_nctrlsb_old.value == "upper")
				document.form.wl_nctrlsb.options.selectedIndex=1;
			
			if(is_high_power && CurrentCh == 5)  // for high power model, Jieming added at 2013/08/19
				document.form.wl_nctrlsb.remove(1);
			else if(is_high_power && CurrentCh == 7)
				document.form.wl_nctrlsb.remove(0);
		}
		else if ((CurrentCh >= 8) && (CurrentCh <= 9)){
			x.options[0].text = "Below";
			x.options[0].value = "upper";
			if (option_length >=14){
				add_option(document.form.wl_nctrlsb, "Above", "lower");
				if (document.form.wl_nctrlsb_old.value == "lower")
					document.form.wl_nctrlsb.options.selectedIndex=1;
			}
		}
		else if (CurrentCh == 10){
			x.options[0].text = "Below";
			x.options[0].value = "upper";
			if (option_length > 14){
				add_option(document.form.wl_nctrlsb, "Above", "lower");
				if (document.form.wl_nctrlsb_old.value == "lower")
					document.form.wl_nctrlsb.options.selectedIndex=1;
			}
		}
		else if (CurrentCh >= 11){
			x.options[0].text = "Below";
			x.options[0].value = "upper";
		}
		else{
			x.options[0].text = "<#Auto#>";
			x.options[0].value = "1";
		}
	}
	else
		inputCtrl(document.form.wl_nctrlsb, 0);
}

function wl_auth_mode_change(isload){
	var mode = document.form.wl_auth_mode_x.value;
	var wireless_mode = document.form.wl_nmode_x.value;
	var i, cur, algos;
	inputCtrl(document.form.wl_wep_x,  1);

	/* enable/disable crypto algorithm */
	if(mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		inputCtrl(document.form.wl_crypto,  1);
	else
		inputCtrl(document.form.wl_crypto,  0);
	
	/* enable/disable psk passphrase */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2")
		inputCtrl(document.form.wl_wpa_psk,  1);
	else
		inputCtrl(document.form.wl_wpa_psk,  0);

	/* update wl_crypto */
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" ||mode == "wpawpa2"){
		/* Save current crypto algorithm */
		for(var i = 0; i < document.form.wl_crypto.length; i++){
			if(document.form.wl_crypto[i].selected){
				cur = document.form.wl_crypto[i].value;
				break;
			}
		}

		opts = document.form.wl_auth_mode_x.options;
		if(opts[opts.selectedIndex].text == "WPA-Personal" || opts[opts.selectedIndex].text == "WPA-Enterprise")
			algos = new Array("TKIP");
		else if(opts[opts.selectedIndex].text == "WPA2-Personal" || opts[opts.selectedIndex].text == "WPA2-Enterprise")
			algos = new Array("AES");
		else
			algos = new Array("AES", "TKIP+AES");
		
		/* Reconstruct algorithm array from new crypto algorithms */
		free_options(document.form.wl_crypto);
		document.form.wl_crypto.length = algos.length;
		for(i=0; i<algos.length; i++){
			document.form.wl_crypto[i] = new Option(algos[i], algos[i].toLowerCase());
			document.form.wl_crypto[i].value = algos[i].toLowerCase();	
			if(algos[i].toLowerCase() == cur)
				document.form.wl_crypto[i].selected = true;
		}
	}
	
	/*For Protected Management Frames, only enable for "(wpa)psk2" or "wpa2" on ARM platform (wl_mfp_support)*/
	/* QTN_5G support PMF too*/
	if(wl_mfp_support && (document.form.wl_mfp != null)){
		if ((mode.search("psk2") >= 0 || mode.search("wpa2") >= 0)){
			inputCtrl(document.form.wl_mfp,  1);	
		}
		else{
			inputCtrl(document.form.wl_mfp,  0);	
		}
	}
		
	change_wep_type(mode, isload);
	
	/* Save current network key index */
	cur = "1";
	for(var i = 0; i < document.form.wl_key.length; i++){
		if(document.form.wl_key[i].selected){
			cur = document.form.wl_key[i].value;
			break;
		}
	}
	
	/* Define new network key indices */
	if(mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "radius")
		algos = new Array("1", "2", "3", "4");
	else{
		algos = new Array("1", "2", "3", "4");
	}
	
	/* Reconstruct network key indices array from new network key indices */
	free_options(document.form.wl_key);
	document.form.wl_key.length = algos.length;
	for(i=0; i<algos.length; i++){
		document.form.wl_key[i] = new Option(algos[i], algos[i]);
		document.form.wl_key[i].value = algos[i];
		if(algos[i] == cur)
			document.form.wl_key[i].selected = true;
	}
	
	wl_wep_change();
}

function showhide(element, sh)
{
	var status;
	if ((sh == 1) || (sh == true)){
		status = "";
	}
	else{
		status = "none"
	}
	
	if(document.getElementById){
		document.getElementById(element).style.display = status;
	}
	else if (document.all){
		document.all[element].style.display = status;
	}
	else if (document.layers){
		document.layers[element].display = status;
	}
}

function check_port_input(obj, emp){	
	if(document.getElementById("check_port_input"))
		obj.parentNode.removeChild(obj.parentNode.childNodes[2]);	
	if(!Check_multi_range(obj, 1, 65535) || (emp == 1 && obj.value == "")){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_port_input");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_port_input").innerHTML="<#BM_alert_port1#> 1 <#BM_alert_to#> 65535";		
		document.getElementById("check_port_input").style.display = "";
		obj.value = obj.parentNode.childNodes[0].innerHTML;
		obj.focus();
		obj.select();
		return false;	
	}else
				return true;
}

//Viz add 2012.07 check Editable table macaddr field
function check_macaddr_input(obj,flag,emp){ //control hint of input mac address
	if(document.getElementById("check_mac_input"))
		obj.parentNode.removeChild(obj.parentNode.childNodes[2]);
	
	if(flag == 1 || (emp == 1 && obj.value == "")){		
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac_input");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac_input").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";		
		document.getElementById("check_mac_input").style.display = "";
		obj.value = obj.parentNode.childNodes[0].innerHTML;
		obj.focus();
		obj.select();
		return false;	
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac_input");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		document.getElementById("check_mac_input").innerHTML=Untranslated.illegal_MAC;		
		document.getElementById("check_mac_input").style.display = "";
		obj.value = obj.parentNode.childNodes[0].innerHTML;
		obj.focus();
		obj.select();
		return false;			
	}else		
		return true;
}

function check_hwaddr_flag(obj){  //check_hwaddr() remove alert() 
	if(obj.value == ""){
			return 0;
	}else{
		var hwaddr = new RegExp("(([a-fA-F0-9]{2}(\:|$)){6})", "gi");
		var legal_hwaddr = new RegExp("(^([a-fA-F0-9][aAcCeE02468])(\:))", "gi"); // for legal MAC, unicast & globally unique (OUI enforced)
		
		if(!hwaddr.test(obj.value))
    	return 1;
  	else if(!legal_hwaddr.test(obj.value))
    	return 2;
		else
			 return 0;
  }
}

function change_key_des(){
	var objs = getElementsByName_iefix("span", "key_des");
	var wep_type = document.form.wl_wep_x.value;
	var str = "";
	
	if(wep_type == "1")
		str = "(<#WLANConfig11b_WEPKey_itemtype1#>)";
	else if(wep_type == "2")
		str = "(<#WLANConfig11b_WEPKey_itemtype2#>)";
	
	for(var i = 0; i < objs.length; ++i)
		showtext(objs[i], str);
}

function wl_gmode_protection_check(){
	if (document.form.wl_gmode_check.checked == true)
		document.form.wl_gmode_protection.value = "auto";
	else
		document.form.wl_gmode_protection.value = "off";
}

/* Handle WEP key index changed */
function wep_key_index_change(obj){
	var selected_key = eval("document.form.wl_key" + obj.value);
		selected_key.focus();
		selected_key.select();
}

/* Handle WEP encryption changed */
function wep_encryption_change(obj){
	change_wlweptype(obj, 0);
}

/* Handle Authentication Method changed */
function authentication_method_change(obj){
	wl_auth_mode_change(0);
}

/* Handle wireless mode changed */
function wireless_mode_change(obj){
	if(obj.value=='0' || obj.value=='2')
		inputCtrl(document.form.wl_gmode_check, 1);
	else
		inputCtrl(document.form.wl_gmode_check, 0);
		
	if(obj.value == "2")
		inputCtrl(document.form.wl_bw, 0);
	else
		inputCtrl(document.form.wl_bw, 1);

	handle_11ac_80MHz();
	insertExtChannelOption();
	if(obj.value == "3"){
		document.form.wl_wme.value = "on";
	}

	limit_auth_method();
	check_NOnly_to_GN();
}

/* To hide Shared key, WPA-Personal/Enterprise and RADIUS with 802.1X*/
function limit_auth_method(g_unit){
	var auth_method_array = document.form.wl_auth_mode_x.value;
	if(sw_mode == 2){
			if(based_modelid == "RT-AC87U" && g_unit)
					var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			else if(based_modelid == "RT-AC87U" && g_unit =='0')
					var auth_array = [["Open System", "open"], ["Shared Key", "shared"], ["WPA-Personal", "psk"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			else{		
				if((based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1'))
					var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
				else
					var auth_array = [["Open System", "open"], ["Shared Key", "shared"], ["WPA-Personal", "psk"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			}
	}
	else if(document.form.wl_nmode_x.value != "2"){
		if(based_modelid == "RT-AC87U" && g_unit)
				var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			else if(based_modelid == "RT-AC87U" && g_unit=='0')
				var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"], ["WPA2-Enterprise", "wpa2"], ["WPA-Auto-Enterprise", "wpawpa2"]];
		else{
			if((based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1') || (based_modelid == "RT-AC87U" && g_unit))
				var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			else
				var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"], ["WPA2-Enterprise", "wpa2"], ["WPA-Auto-Enterprise", "wpawpa2"]];
		}
	}
	else{		//Legacy
		if(based_modelid == "RT-AC87U" && g_unit)
			var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
		else if(based_modelid == "RT-AC87U" && g_unit=='0')
			var auth_array = [["Open System", "open"], ["Shared Key", "shared"], ["WPA-Personal", "psk"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"], ["WPA-Enterprise", "wpa"], ["WPA2-Enterprise", "wpa2"], ["WPA-Auto-Enterprise", "wpawpa2"], ["Radius with 802.1x", "radius"]];
		else{
			if((based_modelid == "RT-AC87U" && '<% nvram_get("wl_unit"); %>' == '1') || (based_modelid == "RT-AC87U" && g_unit))
				var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"]];
			else{
				if(wifi_logo_support)
					var auth_array = [["Open System", "open"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"], ["WPA-Enterprise", "wpa"], ["WPA2-Enterprise", "wpa2"], ["WPA-Auto-Enterprise", "wpawpa2"]];
				else
					var auth_array = [["Open System", "open"], ["Shared Key", "shared"], ["WPA-Personal", "psk"], ["WPA2-Personal", "psk2"], ["WPA-Auto-Personal", "pskpsk2"], ["WPA-Enterprise", "wpa"], ["WPA2-Enterprise", "wpa2"], ["WPA-Auto-Enterprise", "wpawpa2"], ["Radius with 802.1x", "radius"]];
			}	
			
		}
	}
		
	if(is_KR_sku){	// MODELDEP by Territory_code
		auth_array.splice(0, 1); //remove Open System
	}	
	
	free_options(document.form.wl_auth_mode_x);
	for(i = 0; i < auth_array.length; i++){
		if(auth_method_array  == auth_array[i][1])
			add_option(document.form.wl_auth_mode_x, auth_array[i][0], auth_array[i][1], 1);
		else
			add_option(document.form.wl_auth_mode_x, auth_array[i][0], auth_array[i][1], 0);	
	}
		
	authentication_method_change(document.form.wl_auth_mode_x);
}

function getDDNSState(ddns_return_code, ddns_hostname, ddns_old_hostname)
{
	var ddnsStateHint = "";
	if(ddns_return_code.indexOf('-1')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_2#>";
	else if(ddns_return_code.indexOf('200')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_3#>";
	else if(ddns_return_code.indexOf('203')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_hostname#> '"+ddns_hostname+"' <#LANHostConfig_x_DDNS_alarm_registered#>";
	else if(ddns_return_code.indexOf('220')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_4#>";
	else if(ddns_return_code.indexOf('230')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_5#>";
	else if(ddns_return_code.indexOf('233')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_hostname#> '"+ddns_hostname+"' <#LANHostConfig_x_DDNS_alarm_registered_2#> '"+ddns_old_hostname+"'";
	else if(ddns_return_code.indexOf('296')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_6#>";
	else if(ddns_return_code.indexOf('297')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_7#>";
	else if(ddns_return_code.indexOf('298')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_8#>";
	else if(ddns_return_code.indexOf('299')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_9#>";
	else if(ddns_return_code.indexOf('401')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_10#>";
	else if(ddns_return_code.indexOf('407')!=-1)
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_11#>";
	else if(ddns_return_code == 'Time-out')
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_1#>";
	else if(ddns_return_code =='unknown_error')
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_2#>";
	else if(ddns_return_code =='connect_fail')
		ddnsStateHint = "<#qis_fail_desc7#>";
	else if(ddns_return_code =='no_change')
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_nochange#>";
	/*else if(ddns_return_code =='ddns_query')
		ddnsStateHint = "<#LANHostConfig_x_DDNSHostnameCheck_buttonname#>";*/
	else if(ddns_return_code =='auth_fail')
		ddnsStateHint = "<#qis_fail_desc1#>";
	else if(ddns_return_code !='')
		ddnsStateHint = "<#LANHostConfig_x_DDNS_alarm_2#>";

	return ddnsStateHint;
}

function get_yadns_modedesc(mode)
{
	if (mode == 0)
		return "<#YandexDNS_mode0#>";
	else if(mode == 1)
		return "<#YandexDNS_mode1#>";
	else if(mode == 2)
		return "<#YandexDNS_mode2#>";
	else if(mode == -1)
		return "<#btn_Disabled#>";

	return "";
}
