<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<title><#Web_Title#> - <#menu5_1_1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link href="other.css"  rel="stylesheet" type="text/css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/md5.js"></script>
<script type="text/javascript" src="/chanspec.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
<% wl_get_parameter(); %>

wl_channel_list_2g = <% channel_list_2g(); %>;
wl_channel_list_5g = <% channel_list_5g(); %>;
var cur_control_channel = [<% wl_control_channel(); %>][0];
var wl_unit = <% nvram_get("wl_unit"); %>;
var country = '';
if(wl_unit == '1')
	country = '<% nvram_get("wl1_country_code"); %>';
else		
	country = '<% nvram_get("wl0_country_code"); %>';

function initial(){
	show_menu();
	if(band5g_11ac_support){
		regen_5G_mode(document.form.wl_nmode_x, wl_unit)		
	}
	
	genBWTable(wl_unit);	

	if((sw_mode == 2 || sw_mode == 4) && wl_unit == '<% nvram_get("wlc_band"); %>' && '<% nvram_get("wl_subunit"); %>' != '1'){
		_change_wl_unit(wl_unit);
	}

	//Change wireless mode help desc
	if(band5g_support && band5g_11ac_support && document.form.wl_unit[1].selected == true){ //AC 5G
		if(based_modelid == "RT-AC87U") 
			document.getElementById('wl_mode_desc').onclick=function(){return openHint(1, 6)};//#WLANConfig11b_x_Mode_itemdescAC2#	
		else if(based_modelid == "DSL-AC68U" || based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "RT-AC69U" || 
				based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S" || based_modelid == "RT-AC53U"){
			document.getElementById('wl_mode_desc').onclick=function(){return openHint(1, 7)};//#WLANConfig11b_x_Mode_itemdescAC3#
		}	
		else
			document.getElementById('wl_mode_desc').onclick=function(){return openHint(1, 5)};//#WLANConfig11b_x_Mode_itemdescAC#
	}
	else if(band5g_support && document.form.wl_unit[1].selected == true){	//N 5G
		document.getElementById('wl_mode_desc').onclick=function(){return openHint(1, 4)};
	}

	// special case after modifing GuestNetwork
	if(wl_unit == "-1" && "<% nvram_get("wl_subunit"); %>" == "-1"){
		change_wl_unit();
	}

	wl_auth_mode_change(1);

	if(optimizeXbox_support){
		document.getElementById("wl_optimizexbox_span").style.display = "";
		document.form.wl_optimizexbox_ckb.checked = ('<% nvram_get("wl_optimizexbox"); %>' == 1) ? true : false;
	}

	document.form.wl_ssid.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_ssid"); %>');
	document.form.wl_wpa_psk.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_wpa_psk"); %>');
	document.form.wl_key1.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key1"); %>');
	document.form.wl_key2.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key2"); %>');
	document.form.wl_key3.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key3"); %>');
	document.form.wl_key4.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_key4"); %>');
	document.form.wl_phrase_x.value = decodeURIComponent('<% nvram_char_to_ascii("", "wl_phrase_x"); %>');
	document.form.wl_channel.value = document.form.wl_channel_orig.value;

	regen_band(document.form.wl_unit);

	if(document.form.wl_unit[0].selected == true){
		document.getElementById("wl_gmode_checkbox").style.display = "";
	}

	if(tmo_support)
		tmo_wl_nmode();

	if(document.form.wl_nmode_x.value=='1'){
		document.form.wl_gmode_check.checked = false;
		document.getElementById("wl_gmode_check").disabled = true;
	}
	else{
		document.form.wl_gmode_check.checked = true;
		document.getElementById("wl_gmode_check").disabled = false;
	}

	if(document.form.wl_gmode_protection.value == "auto")
		document.form.wl_gmode_check.checked = true;
	else
		document.form.wl_gmode_check.checked = false;

	if(!band5g_support)	
		document.getElementById("wl_unit_field").style.display = "none";

	if(sw_mode == 2 || sw_mode == 4)
		document.form.wl_subunit.value = (wl_unit == '<% nvram_get("wlc_band"); %>') ? 1 : -1;
				
	change_wl_nmode(document.form.wl_nmode_x);
	if(country == "EU"){		//display checkbox of DFS channel under 5GHz
		if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "DSL-AC68U" || based_modelid == "RT-AC69U"
		|| based_modelid == "RT-AC87U"
		|| based_modelid == "RT-AC3200"
		|| (based_modelid == "RT-AC66U" && wl1_dfs == "1")		//0: A2 not support, 1: B0 support
		|| based_modelid == "RT-N66U"){
				if(document.form.wl_channel.value  == '0' && wl_unit == '1'){
						document.getElementById('dfs_checkbox').style.display = "";
						check_DFS_support(document.form.acs_dfs_checkbox);
				}
		}
	}
	else if(country == "US" || country == "SG"){		//display checkbox of band1 channel under 5GHz
		if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "RT-AC69U" || based_modelid == "DSL-AC68U"
		|| based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S"
		|| based_modelid == "RT-N18U"
		|| based_modelid == "RT-AC66U"
		|| based_modelid == "RT-N66U"
		|| based_modelid == "RT-AC53U"){		
			if(document.form.wl_channel.value  == '0' && wl_unit == '1')
				document.getElementById('acs_band1_checkbox').style.display = "";					
		}
	}

	if(smart_connect_support){
		var flag = '<% get_parameter("flag"); %>';		
		var smart_connect_flag_t;

		if(based_modelid == "RT-AC5300")
			inputCtrl(document.form.smart_connect_t, 1);

		document.getElementById("smartcon_enable_field").style.display = "";

		if(flag == '')
			smart_connect_flag_t = '<% nvram_get("smart_connect_x"); %>';
		else
			smart_connect_flag_t = flag;	

		document.form.smart_connect_x.value = smart_connect_flag_t;
		if(smart_connect_flag_t == 0)
			document.form.smart_connect_t.value = 1;
		else    
			document.form.smart_connect_t.value = smart_connect_flag_t;

		enableSmartCon(smart_connect_flag_t);
	}
	if(history.pushState != undefined) history.pushState("", document.title, window.location.pathname);
	
	if(document.form.wl_channel.value  == '0'){
		document.getElementById("auto_channel").style.display = "";
		document.getElementById("auto_channel").innerHTML = "Current control channel: "+cur_control_channel[wl_unit];
	}
}


function change_wl_nmode(o){
	if(o.value=='1') /* Jerry5: to be verified */
		inputCtrl(document.form.wl_gmode_check, 0);
	else
		inputCtrl(document.form.wl_gmode_check, 1);

	limit_auth_method();
	if(o.value == "3"){
		document.form.wl_wme.value = "on";
	}

	
	wl_chanspec_list_change();
	genBWTable(wl_unit);
}

function genBWTable(_unit){
	cur = '<% nvram_get("wl_bw"); %>';
	var bws = new Array();
	var bwsDesc = new Array();
	
	if(document.form.wl_nmode_x.value == 2){
		bws = [1];
		bwsDesc = ["20 MHz"];
		if(tmo_support && _unit == 0){		// for 2.4G B/G Mixed
			inputCtrl(document.form.wl_bw,0);
		}
		else{
			inputCtrl(document.form.wl_bw,1);
		}
	}
	else if(_unit == 0){
		bws = [0, 1, 2];
		bwsDesc = ["20/40 MHz", "20 MHz", "40 MHz"];
		if(tmo_support){
			if(document.form.wl_nmode_x.value == 6 || document.form.wl_nmode_x.value == 5){		// B only or G only
				inputCtrl(document.form.wl_bw,0);				
			}
			else{
				inputCtrl(document.form.wl_bw,1);
			}
		}
	}
	else{
		if(tmo_support){
			if(document.form.wl_nmode_x.value == 7){		// A only
				inputCtrl(document.form.wl_bw,0);	
			}
			else{
				inputCtrl(document.form.wl_bw,1);
			
				if(document.form.wl_nmode_x.value == 0 || document.form.wl_nmode_x.value == 3){			// Auto or AC only
					bws = [0, 1, 2, 3];
					bwsDesc = ["20/40/80 MHz", "20 MHz", "40 MHz", "80 MHz"];		
				}
				else{			// N only or A/N Mixed
					bws = [0, 1, 2];
					bwsDesc = ["20/40 MHz", "20 MHz", "40 MHz"];			
				}
			}
		}
		else{
			if (!band5g_11ac_support){	//for RT-N66U SDK 6.x
				bws = [0, 1, 2];
				bwsDesc = ["20/40 MHz", "20 MHz", "40 MHz"];		
			}else if (based_modelid == "RT-AC87U"){
				if(document.form.wl_nmode_x.value == 1){
					bws = [1, 2];
					bwsDesc = ["20 MHz", "40 MHz"];
				}else{
					bws = [1, 2, 3];
					bwsDesc = ["20 MHz", "40 MHz", "80 MHz"];
				}
			}
			else if((based_modelid == "DSL-AC68U" || based_modelid == "RT-AC68U" || based_modelid == "RT-AC68U_V2" || based_modelid == "RT-AC69U" || 
				based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S" || 
				based_modelid == "RT-AC66U" || 
				based_modelid == "RT-AC3200" || 
				based_modelid == "RT-AC53U") && document.form.wl_nmode_x.value == 1){		//N only
				bws = [0, 1, 2];
				bwsDesc = ["20/40 MHz", "20 MHz", "40 MHz"];
			}
			else{	
				bws = [0, 1, 2, 3];
				bwsDesc = ["20/40/80 MHz", "20 MHz", "40 MHz", "80 MHz"];
			}
		}		
	}

	add_options_x2(document.form.wl_bw, bwsDesc, bws, cur);
	wl_chanspec_list_change();
}

function mbss_display_ctrl(){
	// generate options
	if(multissid_support){
		for(var i=1; i<multissid_support+1; i++)
			add_options_value(document.form.wl_subunit, i, '<% nvram_get("wl_subunit"); %>');
	}	
	else
		document.getElementById("wl_subunit_field").style.display = "none";

	if(document.form.wl_subunit.value != 0){
		document.getElementById("wl_bw_field").style.display = "none";
		document.getElementById("wl_channel_field").style.display = "none";
		document.getElementById("wl_nctrlsb_field").style.display = "none";
	}
	else
		document.getElementById("wl_bss_enabled_field").style.display = "none";
}

function add_options_value(o, arr, orig){
	if(orig == arr)
		add_option(o, "mbss_"+arr, arr, 1);
	else
		add_option(o, "mbss_"+arr, arr, 0);
}

function detect_qtn_ready(){
	if(qtn_state_t != "1")
		setTimeout('detect_qtn_ready();', 1000);
	else
		document.form.submit();		
}

function applyRule(){
	var auth_mode = document.form.wl_auth_mode_x.value;
	
	if(document.form.wl_wpa_psk.value == "<#wireless_psk_fillin#>")
		document.form.wl_wpa_psk.value = "";

	if(validForm()){
        if(document.form.wl_closed[0].checked && document.form.wps_enable.value == 1){ 
            if(!confirm("<#wireless_JS_Hide_SSID#>")){	/*Selecting Hide SSID will disable WPS. Are you sure ?*/
                return false;           
            }
 
             document.form.wps_enable.value = "0";
        }
	
        if(document.form.wps_enable.value == 1){
            if(document.form.wps_dualband.value == "1" || document.form.wl_unit.value == document.form.wps_band.value){         //9: RT-AC87U dual band WPS
                if(document.form.wl_auth_mode_x.value == "open" && document.form.wl_wep_x.value == "0"){
					if(!confirm("<#wireless_JS_WPS_open#>"))	/*Are you sure to configure WPS in Open System (no security) ?*/
						return false;           
                }
                
                if( document.form.wl_auth_mode_x.value == "shared"
                 || document.form.wl_auth_mode_x.value == "psk" || document.form.wl_auth_mode_x.value == "wpa"
                 || document.form.wl_auth_mode_x.value == "open" && (document.form.wl_wep_x.value == "1" || document.form.wl_wep_x.value == "2")){              //open wep case
                    if(!confirm("<#wireless_JS_disable_WPS#>"))	/*Selecting WEP or TKIP Encryption will disable the WPS. Are you sure ?*/
                        return false;   
					
                    document.form.wps_enable.value = "0";   
                }       
            }
			else{
				if(document.form.wl_auth_mode_x.value == "open" && document.form.wl_wep_x.value == "0"){
					if(!confirm("<#wireless_JS_WPS_open#>"))
						return false;		
				}
			}
		}

               if(smart_connect_support && document.form.smart_connect_x.value != 0 && based_modelid == "RT-AC5300")
                       document.form.smart_connect_x.value = document.form.smart_connect_t.value;

		showLoading();
		if(based_modelid == "RT-AC87U" && wl_unit == "1")
			stopFlag = '0';
			
		document.form.wps_config_state.value = "1";		
		if((auth_mode == "shared" || auth_mode == "wpa" || auth_mode == "wpa2"  || auth_mode == "wpawpa2" || auth_mode == "radius" 
		||((auth_mode == "open") && !(document.form.wl_wep_x.value == "0")))
		 && document.form.wps_mode.value == "enabled"){
			document.form.wps_mode.value = "disabled";
		}	
		
		if(auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2" || auth_mode == "radius")
			document.form.next_page.value = "/Advanced_WSecurity_Content.asp";

		if(document.form.wl_nmode_x.value == "1" && wl_unit == "0")
			document.form.wl_gmode_protection.value = "off";			

		if(sw_mode == 2 || sw_mode == 4)
			document.form.action_wait.value = "5";

		if(document.form.wl_bw.value == 1)	// 20MHz
			document.form.wl_chanspec.value = document.form.wl_channel.value;
		else{
			if(document.form.wl_channel.value == 0)			// Auto case
				document.form.wl_chanspec.value = document.form.wl_channel.value;
			else{
				if(tmo_support && (document.form.wl_nmode_x.value == 6 || document.form.wl_nmode_x.value == 5 || document.form.wl_nmode_x.value == 2 || document.form.wl_nmode_x.value == 7)){		// B only, G only, B/G Mixed, A only				
					document.form.wl_chanspec.value = document.form.wl_channel.value;
				}
				else{
					document.form.wl_chanspec.value = document.form.wl_channel.value + document.form.wl_nctrlsb.value;
				}
			}	
		}

		if(country == "EU" && based_modelid == "RT-AC87U" && wl_unit == '1'){			//Interlocking setting to enable 'wl1_80211h' in EU RT-AC87U under 5GHz
			if(document.form.wl_channel.value  == '0' && document.form.acs_dfs.value == '1')			//Auto channel with DFS channel
				document.form.wl1_80211h.value = "1";	
		}

		if(smart_connect_support && document.form.smart_connect_x.value == '1')
			document.form.wl_unit.value = 0;

		if (based_modelid == "RT-AC87U" && wl_unit == "1")
			detect_qtn_ready();
		else
			document.form.submit();
	}
} 

function validForm(){
	var auth_mode = document.form.wl_auth_mode_x.value;
	
	if(!validator.stringSSID(document.form.wl_ssid))
		return false;
	
	if(!check_NOnly_to_GN()){
		autoFocus('wl_nmode_x');
		return false;
	}
	
	if(document.form.wl_wep_x.value != "0")
		if(!validate_wlphrase('WLANConfig11b', 'wl_phrase_x', document.form.wl_phrase_x))
			return false;	
	if(auth_mode == "psk" || auth_mode == "psk2" || auth_mode == "pskpsk2"){ //2008.08.04 lock modified
		if(is_KR_sku){
			if(!validator.psk_KR(document.form.wl_wpa_psk))
				return false;
		}
		else{
			if(!validator.psk(document.form.wl_wpa_psk))
				return false;
		}
		
		//confirm common string combination	#JS_common_passwd#
		var is_common_string = check_common_string(document.form.wl_wpa_psk.value, "wpa_key");
		if(is_common_string){
			if(confirm("<#JS_common_passwd#>")){
				document.form.wl_wpa_psk.focus();
				document.form.wl_wpa_psk.select();
				return false;	
			}	
		}
		
		if(!validator.range(document.form.wl_wpa_gtk_rekey, 0, 2592000))
			return false;
	}
	else if(auth_mode == "wpa" || auth_mode == "wpa2" || auth_mode == "wpawpa2"){
		if(!validator.range(document.form.wl_wpa_gtk_rekey, 0, 2592000))
			return false;
	}
	else{
		var cur_wep_key = eval('document.form.wl_key'+document.form.wl_key.value);		
		if(auth_mode != "radius" && !validator.wlKey(cur_wep_key))
			return false;
	}	
	return true;
}

function done_validating(action){
	refreshpage();
}

function validate_wlphrase(s, v, obj){
	if(!validator.string(obj)){
		is_wlphrase(s, v, obj);
		return false;
	}
	return true;
}

function disableAdvFn(){
	for(var i=18; i>=3; i--)
		document.getElementById("WLgeneral").deleteRow(i);
}

function _change_wl_unit(val){
	if(sw_mode == 2 || sw_mode == 4)
		document.form.wl_subunit.value = (val == '<% nvram_get("wlc_band"); %>') ? 1 : -1;
	
	if(smart_connect_support){
		if(document.form.smart_connect_x.value != 0)
  			document.form.smart_connect_x.value = document.form.smart_connect_t.value;
		var smart_connect_flag = document.form.smart_connect_x.value;
		document.form.current_page.value = "Advanced_Wireless_Content.asp?flag=" + smart_connect_flag;
	}	
	change_wl_unit();
}

function _change_smart_connect(val){
	current_band = wl_unit;
	document.getElementById("wl_unit_field").style.display = "";
	var band_desc = new Array();
	var band_value = new Array();
	if(val == 0){
		band_value = [0, 1, 2];
		band_desc = ['2.4GHz', '5GHz-1', '5GHz-2'];
	}else if(val == 1){
		document.getElementById("wl_unit_field").style.display = "none";
		band_value = [0];
		band_desc = ['2.4GHz, 5GHz-1 and 5GHz-2'];
	}else if(val == 2){
		band_value = [0, 1];
		band_desc = ['2.4GHz', '5GHz-1 and 5GHz-2'];
	}
	add_options_x2(document.form.wl_unit, band_desc, band_value, current_band);
}

function checkBW(){
	if(wifilogo_support)
		return false;

	if(document.form.wl_channel.value != 0 && document.form.wl_bw.value == 0){	//Auto but set specific channel
		if(document.form.wl_channel.value == "165")	// channel 165 only for 20MHz
			document.form.wl_bw.selectedIndex = 1;
		else if(wl_unit == 0)	//2.4GHz for 40MHz
			document.form.wl_bw.selectedIndex = 2;
		else{	//5GHz else for 80MHz
			if(band5g_11ac_support)
				document.form.wl_bw.selectedIndex = 3;
			else
				document.form.wl_bw.selectedIndex = 2;
				
			if (wl_channel_list_5g.getIndexByValue("165") >= 0 ) // rm option 165 if not Auto
				document.form.wl_channel.remove(wl_channel_list_5g.getIndexByValue("165"));			
		}
	}
}

function check_NOnly_to_GN(){
	//var gn_array_2g = [["1", "ASUS_Guest1", "psk", "tkip", "1234567890", "0", "1", "", "", "", "", "0", "off", "0"], ["1", "ASUS_Guest2", "shared", "aes", "", "1", "1", "55555", "", "", "", "0", "off", "0"], ["1", "ASUS_Guest3", "open", "aes", "", "2", "4", "", "", "", "1234567890123", "0", "off", "0"]];
	//                    Y/N        mssid      auth    asyn    wpa_psk wl_wep_x wl_key k1	k2 k3 k4                                        
	//var gn_array_5g = [["1", "ASUS_5G_Guest1", "open", "aes", "", "0", "1", "", "", "", "", "0", "off", "0"], ["0", "ASUS_5G_Guest2", "open", "aes", "", "0", "1", "", "", "", "", "0", "off", ""], ["0", "ASUS_5G_Guest3", "open", "aes", "", "0", "1", "", "", "", "", "0", "off", ""]];
	// Viz add 2012.11.05 restriction for 'N Only' mode  ( start 	
	
	if(document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "1"){
		if(wl_unit == "1"){		//5G
			for(var i=0;i<gn_array_5g.length;i++){
				if(gn_array_5g[i][0] == "1" && (gn_array_5g[i][3] == "tkip" || gn_array_5g[i][5] == "1" || gn_array_5g[i][5] == "2")){
					if(document.form.wl_nmode_x.value == "0")
						document.getElementById('wl_NOnly_note').innerHTML = '<br>* <#WLANConfig11n_Auto_note#>';
					else{
						if(band5g_11ac_support)
							document.getElementById('wl_NOnly_note').innerHTML = '<br>* <#WLANConfig11n_NAC_note#>';
						else
							document.getElementById('wl_NOnly_note').innerHTML = '<br>* <#WLANConfig11n_NOnly_note#>';
					}	
						
					document.getElementById('wl_NOnly_note').style.display = "";
					return false;
				}
			}		
		}
		else if(wl_unit == "0"){		//2.4G
			for(var i=0;i<gn_array_2g.length;i++){
				if(gn_array_2g[i][0] == "1" && (gn_array_2g[i][3] == "tkip" || gn_array_2g[i][5] == "1" || gn_array_2g[i][5] == "2")){
					if(document.form.wl_nmode_x.value == "0")
						document.getElementById('wl_NOnly_note').innerHTML = '<br>* <#WLANConfig11n_Auto_note#>';
					else	
						document.getElementById('wl_NOnly_note').innerHTML = '<br>* <#WLANConfig11n_NOnly_note#>';
						
					document.getElementById('wl_NOnly_note').style.display = "";
					return false;
				}
			}	
		}	
	}
		
	document.getElementById('wl_NOnly_note').style.display = "none";
	return true;
//  Viz add 2012.11.05 restriction for 'N Only' mode  ) end		
}

function regen_5G_mode(obj,flag){	//please sync to initial() : //Change wireless mode help desc
	free_options(obj);
	if(flag == 1 || flag == 2){
		if(based_modelid == "RT-AC87U"){
			obj.options[0] = new Option("<#Auto#>", 0);
			obj.options[1] = new Option("N only", 1);			
		}
		else{
			obj.options[0] = new Option("<#Auto#>", 0);
			obj.options[1] = new Option("N only", 1);
			obj.options[2] = new Option("N/AC mixed", 8);
			obj.options[3] = new Option("Legacy", 2);
		}	
	}else{
		obj.options[0] = new Option("<#Auto#>", 0);
		obj.options[1] = new Option("N only", 1);
		obj.options[2] = new Option("Legacy", 2);		
	}
	obj.value = '<% nvram_get("wl_nmode_x"); %>';
}

function check_DFS_support(obj){
	if(obj.checked)
		document.form.acs_dfs.value = 1;
	else
		document.form.acs_dfs.value = 0;
}

function check_acs_band1_support(obj){
	if(obj.checked)
		document.form.acs_band1.value = 1;
	else
		document.form.acs_band1.value = 0;
}

function tmo_wl_nmode(){
	var tmo2nmode = [["0",  "<#Auto#>"],["6",       "B Only"],["5", "G Only"],["1", "N Only"],["2",	"B/G Mixed"],["4", "G/N Mixed"]];
	var tmo5nmode = [["0",  "<#Auto#>"],["7",       "A Only"],["1", "N Only"],["3", "AC Only"],["4", "A/N Mixed"]];
	free_options(document.form.wl_nmode_x);
	if(wl_unit == "0"){               //2.4GHz
		for(var i = 0; i < tmo2nmode.length; i++){
			add_option(document.form.wl_nmode_x,tmo2nmode[i][1], tmo2nmode[i][0],(document.form.wl_nmode_x_orig.value == tmo2nmode[i][0]));
		}
	}
	else{           //5GHz
		for(var i = 0; i < tmo5nmode.length; i++){
			add_option(document.form.wl_nmode_x,tmo5nmode[i][1], tmo5nmode[i][0],(document.form.wl_nmode_x_orig.value == tmo5nmode[i][0]));
		}
	}
}

function enableSmartCon(val){

	document.form.smart_connect_x.value = val;

	if(val == 0){
		if(based_modelid=="RT-AC5300"){
			document.getElementById("smart_connect_field").style.display = "none";
			document.getElementById("smartcon_rule_link").style.display = "none";
		}

		document.getElementById("wl_unit_field").style.display = "";
		document.form.wl_nmode_x.disabled = "";
		document.getElementById("wl_optimizexbox_span").style.display = "";
		if(document.form.wl_unit[0].selected == true){
			document.getElementById("wl_gmode_checkbox").style.display = "";
		}
		if(band5g_11ac_support){
			regen_5G_mode(document.form.wl_nmode_x, wl_unit)		
		}else{
			free_options(document.form.wl_nmode_x);
			obj.options[0] = new Option("<#Auto#>", 0);
			obj.options[1] = new Option("N only", 1);
			obj.options[2] = new Option("Legacy", 2);
		}
		change_wl_nmode(document.form.wl_nmode_x);
	}
	else if(val > 0){
		if(based_modelid=="RT-AC5300"){
			document.getElementById("smart_connect_field").style.display = "";
			document.getElementById("smartcon_rule_link").style.display = "table-cell";
		}

		document.getElementById("wl_unit_field").style.display = "none";
		regen_auto_option(document.form.wl_nmode_x);
		document.getElementById("wl_optimizexbox_span").style.display = "none";
		document.getElementById("wl_gmode_checkbox").style.display = "none";
		regen_auto_option(document.form.wl_bw);
		regen_auto_option(document.form.wl_channel);
		regen_auto_option(document.form.wl_nctrlsb);
	}
	if(based_modelid=="RT-AC5300")
		_change_smart_connect(val);

}

function regen_auto_option(obj){
	free_options(obj);
	obj.options[0] = new Option("<#Auto#>", 0);
	obj.selectedIndex = 0;
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg">
	<table cellpadding="4" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
		<tr>
		<td>
			<div class="drword" id="drword"><#Main_alert_proceeding_desc4#> <#Main_alert_proceeding_desc1#>...
				<br/>
				<div id="disconnect_hint" style="display:none;"><#Main_alert_proceeding_desc2#></div>
				<br/>
		    </div>
			<div id="wireless_client_detect" style="margin-left:10px;position:absolute;display:none;width:400px;">
				<img src="images/loading.gif">
				<div style="margin:-55px 0 0 75px;"><#QKSet_Internet_Setup_fail_method1#></div>
			</div> 
			<div class="drImg"><img src="images/alertImg.png"></div>
			<div style="height:100px; "></div>
		</td>
		</tr>
	</table>
<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
</div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="autochannelform" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Wireless_Content.asp">
<input type="hidden" name="next_page" value="Advanced_Wireless_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="wl_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_chanspec" value="">
<input type="hidden" name="wl_unit" value="">
<input type="hidden" name="force_change" value="<% nvram_get("force_change"); %>">
</form>	
<form method="post" name="form" action="/start_apply2.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_Wireless_Content.asp">
<input type="hidden" name="next_page" value="Advanced_Wireless_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply_new">
<input type="hidden" name="action_script" value="restart_wireless">
<input type="hidden" name="action_wait" value="10">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="wl_country_code" value="<% nvram_get("wl0_country_code"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wps_mode" value="<% nvram_get("wps_mode"); %>">
<input type="hidden" name="wps_config_state" value="<% nvram_get("wps_config_state"); %>">
<input type="hidden" name="wl_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_ssid"); %>">
<input type="hidden" name="wlc_ure_ssid_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wlc_ure_ssid"); %>" disabled>
<input type="hidden" name="wl_wpa_psk_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_wpa_psk"); %>">
<input type="hidden" name="wl_key1_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key1"); %>">
<input type="hidden" name="wl_key2_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key2"); %>">
<input type="hidden" name="wl_key3_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key3"); %>">
<input type="hidden" name="wl_key4_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_key4"); %>">
<input type="hidden" name="wl_phrase_x_org" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_phrase_x"); %>">
<input type="hidden" maxlength="15" size="15" name="x_RegulatoryDomain" value="<% nvram_get("x_RegulatoryDomain"); %>" readonly="1">
<input type="hidden" name="wl_gmode_protection" value="<% nvram_get("wl_gmode_protection"); %>">
<input type="hidden" name="wl_wme" value="<% nvram_get("wl_wme"); %>">
<input type="hidden" name="wl_mode_x" value="<% nvram_get("wl_mode_x"); %>">
<input type="hidden" name="wl_nmode" value="<% nvram_get("wl_nmode"); %>">
<input type="hidden" name="wl_nmode_x_orig" value="<% nvram_get("wl_nmode_x"); %>">
<input type="hidden" name="wl_nctrlsb_old" value="<% nvram_get("wl_nctrlsb"); %>">
<input type="hidden" name="wl_key_type" value='<% nvram_get("wl_key_type"); %>'> <!--Lock Add 2009.03.10 for ralink platform-->
<input type="hidden" name="wl_channel_orig" value='<% nvram_get("wl_channel"); %>'>
<input type="hidden" name="wl_chanspec" value=''>
<input type="hidden" name="wl_wep_x_orig" value='<% nvram_get("wl_wep_x"); %>'>
<input type="hidden" name="wl_optimizexbox" value='<% nvram_get("wl_optimizexbox"); %>'>
<input type="hidden" name="wl_subunit" value='-1'>
<input type="hidden" name="acs_dfs" value='<% nvram_get("acs_dfs"); %>'>
<input type="hidden" name="acs_band1" value='<% nvram_get("acs_band1"); %>'>
<input type="hidden" name="wps_enable" value="<% nvram_get("wps_enable"); %>">
<input type="hidden" name="wps_band" value="<% nvram_get("wps_band"); %>">
<input type="hidden" name="wps_dualband" value="<% nvram_get("wps_dualband"); %>">
<input type="hidden" name="smart_connect_x" value="<% nvram_get("smart_connect_x"); %>">
<input type="hidden" name="wl1_80211h" value="<% nvram_get("wl1_80211h"); %>" >
<input type="hidden" name="w_Setting" value="1">
<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
	<td valign="top">
	  <div id="tabMenu" class="submenuBlock"></div>

<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
  <tr>
	<td align="left" valign="top" >
	  <table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
		<tr>
		  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_1#> - <#menu5_1_1#></div>
      <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
      <div class="formfontdesc"><#adv_wl_desc#></div>
		
			<table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" id="WLgeneral" class="FormTable">

			<tr id="smartcon_enable_field" style="display:none;">
			  	<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(13,1);"><#smart_connect_enable#></a></th>	<!-- Enable Smart Connect -->
			  	<td>
			    	<div id="smartcon_enable_block" style="display:none;">
			    		<span style="color:#FFF;" id="smart_connect_enable_word">&nbsp;&nbsp;</span>
			    		<input type="button" name="enableSmartConbtn" id="enableSmartConbtn" value="" class="button_gen" onClick="enableSmartCon();">
			    		<br>
			    	</div>
						
			    	<div id="radio_smartcon_enable" class="left" style="width: 94px;display:table-cell;"></div><div id="smartcon_rule_link" style="display:table-cell; vertical-align: middle;"><a href="Advanced_Smart_Connect.asp" style="font-family:Lucida Console;color:#FC0;text-decoration:underline;cursor:pointer;">Smart Connect Rule</a></div>
						<div class="clear"></div>					
						<script type="text/javascript">
								var flag = '<% get_parameter("flag"); %>';
								var smart_connect_flag_t;

							if(flag == '')
								smart_connect_flag_t = '<% nvram_get("smart_connect_x"); %>';
							else
								smart_connect_flag_t = flag;

								$('#radio_smartcon_enable').iphoneSwitch( smart_connect_flag_t > 0, 
								 function() {
									enableSmartCon(1);
								 },
								 function() {
									enableSmartCon(0);
								 }
							);
						</script>
		  	  </td>
			</tr>
				<tr id="smart_connect_field" style="display:none;">                     
					<th>Smart Connect</th>                                            
					<td id="smart_connect_switch">
					<select name="smart_connect_t" class="input_option" onChange="_change_smart_connect(this.value);">
						<option class="content_input_fd" value="1" >Tri-band Smart Connect (2.4GHz, 5GHz-1 and 5GHz-2)</optio>
						<option class="content_input_fd" value="2">5GHz Smart Connect (5GHz-1 and 5GHz-2)</option>
					</select>                       
					</td>
				</tr>
				<tr id="wl_unit_field">
					<th><#Interface#></th>
					<td>
						<select name="wl_unit" class="input_option" onChange="_change_wl_unit(this.value);">
							<option class="content_input_fd" value="0" <% nvram_match("wl_unit", "0","selected"); %>>2.4GHz</option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_unit", "1","selected"); %>>5GHz</option>
						</select>			
					</td>
		  	</tr>

				<!--tr id="wl_subunit_field" style="display:none">
					<th>Multiple SSID index</th>
					<td>
						<select name="wl_subunit" class="input_option" onChange="change_wl_unit();">
							<option class="content_input_fd" value="0" <% nvram_match("wl_subunit", "0","selected"); %>>Primary</option>
						</select>			
						<select id="wl_bss_enabled_field" name="wl_bss_enabled" class="input_option" onChange="mbss_switch();">
							<option class="content_input_fd" value="0" <% nvram_match("wl_bss_enabled", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_bss_enabled", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
						</select>			
					</td>
		  	</tr-->

				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 1);"><#WLANConfig11b_SSID_itemname#></a></th>
					<td>
						<input type="text" maxlength="32" class="input_32_table" id="wl_ssid" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>" onkeypress="return validator.isString(this, event)" autocorrect="off" autocapitalize="off">
					</td>
		  	</tr>
			  
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 2);"><#WLANConfig11b_x_BlockBCSSID_itemname#></a></th>
					<td>
						<input type="radio" value="1" name="wl_closed" class="input" <% nvram_match("wl_closed", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" value="0" name="wl_closed" class="input" <% nvram_match("wl_closed", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
					  
			  <tr>
					<th><a id="wl_mode_desc" class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 4);"><#WLANConfig11b_x_Mode_itemname#></a></th>
					<td>									
						<select name="wl_nmode_x" class="input_option" onChange="change_wl_nmode(this);check_NOnly_to_GN();">
							<option value="0" <% nvram_match("wl_nmode_x", "0","selected"); %>><#Auto#></option>
							<option value="1" <% nvram_match("wl_nmode_x", "1","selected"); %>>N Only</option>
							<option value="2" <% nvram_match("wl_nmode_x", "2","selected"); %>>Legacy</option>
						</select>
						<span id="wl_optimizexbox_span" style="display:none"><input type="checkbox" name="wl_optimizexbox_ckb" id="wl_optimizexbox_ckb" value="<% nvram_get("wl_optimizexbox"); %>" onclick="document.form.wl_optimizexbox.value=(this.checked==true)?1:0;"> <#WLANConfig11b_x_Mode_xbox#></input></span>
						<span id="wl_gmode_checkbox" style="display:none;"><input type="checkbox" name="wl_gmode_check" id="wl_gmode_check" value="" onClick="wl_gmode_protection_check();"> <#WLANConfig11b_x_Mode_protectbg#></input></span>
						<span id="wl_nmode_x_hint" style="display:none;"><br><#WLANConfig11n_automode_limition_hint#><br></span>
						<span id="wl_NOnly_note" style="display:none;"></span>
						<!-- [N + AC] is not compatible with current guest network authentication method(TKIP or WEP),  Please go to <a id="gn_link" href="/Guest_network.asp?af=wl_NOnly_note" target="_blank" style="color:#FFCC00;font-family:Lucida Console;text-decoration:underline;">guest network</a> and change the authentication method. -->
					</td>
			  </tr>
			 	<tr id="wl_bw_field">
			   	<th><#WLANConfig11b_ChannelBW_itemname#></th>
			   	<td>				    			
						<select name="wl_bw" class="input_option" onChange="wl_chanspec_list_change();">
							<option class="content_input_fd" value="0" <% nvram_match("wl_bw", "0","selected"); %>>20 MHz</option>
							<option class="content_input_fd" value="1" <% nvram_match("wl_bw", "1","selected"); %>>20/40 MHz</option>
							<option class="content_input_fd" value="2" <% nvram_match("wl_bw", "2","selected"); %>>40 MHz</option>
						</select>				
			   	</td>
			 	</tr>
				<!-- ac channel -->			  
				<tr>
					<th>						
						<a id="wl_channel_select" class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 3);"><#WLANConfig11b_Channel_itemname#></a>						
					</th>
					<td>			
				 		<select name="wl_channel" class="input_option" onChange="change_channel(this);"></select>
						<span id="auto_channel" style="display:none;margin-left:10px;"></span><br>
						<span id="dfs_checkbox" style="display:none"><input type="checkbox" onClick="check_DFS_support(this);" name="acs_dfs_checkbox" <% nvram_match("acs_dfs", "1", "checked"); %>><#WLANConfig11b_EChannel_dfs#></input></span>
						<span id="acs_band1_checkbox" style="display:none;"><input type="checkbox" onClick="check_acs_band1_support(this);" <% nvram_match("acs_band1", "1", "checked"); %>><#WLANConfig11b_EChannel_band1#></input></span>
					</td>
			  </tr> 
		  	<!-- end -->
				<tr id="wl_nctrlsb_field">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 15);"><#WLANConfig11b_EChannel_itemname#></a></th>
					<td>
						<select name="wl_nctrlsb" class="input_option">
							<option value=""></option>
							<option value=""></option>
						</select>
					</td>
				</tr>
			  
			  <tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 5);"><#WLANConfig11b_AuthenticationMethod_itemname#></a></th>
					<td>
				  		<select name="wl_auth_mode_x" class="input_option" onChange="authentication_method_change(this);">
							<option value="open"    <% nvram_match("wl_auth_mode_x", "open",   "selected"); %>>Open System</option>
							<option value="shared"  <% nvram_match("wl_auth_mode_x", "shared", "selected"); %>>Shared Key</option>
							<option value="psk"     <% nvram_match("wl_auth_mode_x", "psk",    "selected"); %>>WPA-Personal</option>
							<option value="psk2"    <% nvram_match("wl_auth_mode_x", "psk2",   "selected"); %>>WPA2-Personal</option>
							<option value="pskpsk2" <% nvram_match("wl_auth_mode_x", "pskpsk2","selected"); %>>WPA-Auto-Personal</option>
							<option value="wpa"     <% nvram_match("wl_auth_mode_x", "wpa",    "selected"); %>>WPA-Enterprise</option>
							<option value="wpa2"    <% nvram_match("wl_auth_mode_x", "wpa2",   "selected"); %>>WPA2-Enterprise</option>
							<option value="wpawpa2" <% nvram_match("wl_auth_mode_x", "wpawpa2","selected"); %>>WPA-Auto-Enterprise</option>
							<option value="radius"  <% nvram_match("wl_auth_mode_x", "radius", "selected"); %>>Radius with 802.1x</option>
				  		</select>
					</td>
			  	</tr>
			  	
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 6);"><#WLANConfig11b_WPAType_itemname#></a></th>
					<td>		
				  		<select name="wl_crypto" class="input_option">
								<option value="aes" <% nvram_match("wl_crypto", "aes", "selected"); %>>AES</option>
								<option value="tkip+aes" <% nvram_match("wl_crypto", "tkip+aes", "selected"); %>>TKIP+AES</option>
				  		</select>
					</td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 7);"><#WLANConfig11b_x_PSKKey_itemname#></a></th>
					<td>
				  		<input type="text" name="wl_wpa_psk" maxlength="64" class="input_32_table" value="<% nvram_get("wl_wpa_psk"); %>" autocorrect="off" autocapitalize="off">					</td>
			  	</tr>
			  		  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 9);"><#WLANConfig11b_WEPType_itemname#></a></th>
					<td>
				  		<select name="wl_wep_x" class="input_option" onChange="wep_encryption_change(this);">
								<option value="0" <% nvram_match("wl_wep_x", "0", "selected"); %>><#wl_securitylevel_0#></option>
								<option value="1" <% nvram_match("wl_wep_x", "1", "selected"); %>>WEP-64bits</option>
								<option value="2" <% nvram_match("wl_wep_x", "2", "selected"); %>>WEP-128bits</option>
				  		</select>
				  		<span name="key_des"></span>
					</td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 10);"><#WLANConfig11b_WEPDefaultKey_itemname#></a></th>
					<td>		
				  		<select name="wl_key" class="input_option"  onChange="wep_key_index_change(this);">
							<option value="1" <% nvram_match("wl_key", "1","selected"); %>>1</option>
							<option value="2" <% nvram_match("wl_key", "2","selected"); %>>2</option>
							<option value="3" <% nvram_match("wl_key", "3","selected"); %>>3</option>
							<option value="4" <% nvram_match("wl_key", "4","selected"); %>>4</option>
				  		</select>
					</td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey1_itemname#></th>
					<td><input type="text" name="wl_key1" id="wl_key1" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key1"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" autocorrect="off" autocapitalize="off"></td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey2_itemname#></th>
					<td><input type="text" name="wl_key2" id="wl_key2" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key2"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" autocorrect="off" autocapitalize="off"></td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey3_itemname#></th>
					<td><input type="text" name="wl_key3" id="wl_key3" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key3"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" autocorrect="off" autocapitalize="off"></td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 18);"><#WLANConfig11b_WEPKey4_itemname#></th>
					<td><input type="text" name="wl_key4" id="wl_key4" maxlength="32" class="input_32_table" value="<% nvram_get("wl_key4"); %>" onKeyUp="return change_wlkey(this, 'WLANConfig11b');" autocorrect="off" autocapitalize="off"></td>
		  		</tr>

			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 8);"><#WLANConfig11b_x_Phrase_itemname#></a></th>
					<td>
				  		<input type="text" name="wl_phrase_x" maxlength="64" class="input_32_table" value="<% nvram_get("wl_phrase_x"); %>" onKeyUp="return is_wlphrase('WLANConfig11b', 'wl_phrase_x', this);" autocorrect="off" autocapitalize="off">
					</td>
			  	</tr>
			  
				<tr style="display:none">
					<th><#WLANConfig11b_x_mfp#></th>	<!-- Protected Management Frames -->
					<td>
				  		<select name="wl_mfp" class="input_option" >
								<option value="0" <% nvram_match("wl_mfp", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
								<option value="1" <% nvram_match("wl_mfp", "1", "selected"); %>><#WLANConfig11b_x_mfp_opt1#></option>	<!-- Capable -->
								<option value="2" <% nvram_match("wl_mfp", "2", "selected"); %>><#WLANConfig11b_x_mfp_opt2#></option>	<!-- Required -->
				  		</select>
					</td>
			  	</tr>
			  
			  	<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 11);"><#WLANConfig11b_x_Rekey_itemname#></a></th>
					<td><input type="text" maxlength="7" name="wl_wpa_gtk_rekey" class="input_6_table"  value="<% nvram_get("wl_wpa_gtk_rekey"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"></td>
			  	</tr>
		  	</table>
			  
				<div class="apply_gen">
					<input type="button" id="applyButton" class="button_gen" value="<#CTL_apply#>" onclick="applyRule();">
				</div>			  	
			  	
		  	</td>
		</tr>
		</tbody>
		
	  </table>
	</td>
</form>
</tr>
</table>
<!--===================================Ending of Main Content===========================================-->

	</td>
	<td width="10" align="center" valign="top"></td>
  </tr>
</table>

<div id="footer"></div>
</body>
</html>
