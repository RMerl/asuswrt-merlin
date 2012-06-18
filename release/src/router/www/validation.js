function validate_string(string_obj, flag){
	if(string_obj.value.charAt(0) == '"'){
		if(flag != "noalert")
			document.getElementById("alert_msg").innerHTML = '<#JS_validstr1#> ["]';
		
		string_obj.value = "";
		string_obj.focus();
		
		return false;
	}
	else{
		invalid_char = "";
		
		for(var i = 0; i < string_obj.value.length; ++i){
			if(string_obj.value.charAt(i) < ' ' || string_obj.value.charAt(i) > '~'){
				invalid_char = invalid_char+string_obj.value.charAt(i);
			}
		}
		
		if(invalid_char != ""){
			if(flag != "noalert")
				document.getElementById("alert_msg").innerHTML = '<#JS_validstr2#>" '+ invalid_char +'" !';
				//$("#alert_msg").text('<#JS_validstr2#>" '+ invalid_char +'" !');
				
				//alert("<#JS_validstr2#> '"+invalid_char+"' !");
			string_obj.value = "";
			string_obj.focus();
			
			return false;
		}
	}
	
	return true;
}

function validate_hex(obj){
	var obj_value = obj.value
	var re = new RegExp("[^a-fA-F0-9]+","gi");
	
	if(re.test(obj_value))
		return false;
	else
		return true;
}

function validate_psk(psk_obj){
	var psk_length = psk_obj.value.length;
	
	if(psk_length < 8){
		//$("#alert_msg").text("<#JS_PSK64Hex#>");
		document.getElementById("alert_msg").innerHTML = '<#JS_PSK64Hex#>';
		psk_obj.focus();
		psk_obj.select();
		return false;
	}
	
	if(psk_length > 64){
		//$("#alert_msg").text("<#JS_PSK64Hex#>");
		document.getElementById("alert_msg").innerHTML = '<#JS_PSK64Hex#>';
		psk_obj.value = psk_obj.value.substring(0, 64);
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	if(psk_length >= 8 && psk_length <= 63 && !validate_string(psk_obj)){
		//$("#alert_msg").text("<#JS_PSK64Hex#>");
		document.getElementById("alert_msg").innerHTML = '<#JS_PSK64Hex#>';
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	if(psk_length == 64 && !validate_hex(psk_obj)){
		//$("#alert_msg").text("<#JS_PSK64Hex#>");
		document.getElementById("alert_msg").innerHTML = '<#JS_PSK64Hex#>';
		psk_obj.focus();
		psk_obj.select();
		
		return false;
	}
	
	return true;
}

function validate_wlkey(key_obj){

	var iscurrect = true;

	if(key_obj.value.length == 5 && validate_string(key_obj)){
		iscurrect = true;
	}
	else if(key_obj.value.length == 10 && validate_hex(key_obj)){
		iscurrect = true;
	}
	else if(key_obj.value.length == 13 && validate_string(key_obj)){
			iscurrect = true;
	}
	else if(key_obj.value.length == 26 && validate_hex(key_obj)){
			iscurrect = true;
	}
	else{
		iscurrect = false;
	}
	
	if(iscurrect == false){
		//alert("<#JS_wepkey#>\n<#WLANConfig11b_WEPKey_itemtype1#>\n<#WLANConfig11b_WEPKey_itemtype1#>");
		document.getElementById("alert_msg").innerHTML = "<#JS_wepkey#>\n<#WLANConfig11b_WEPKey_itemtype1#>\n<#WLANConfig11b_WEPKey_itemtype1#>";
		key_obj.focus();
		key_obj.select();
	}
	
	return iscurrect;
}

