
var HINTPASS = "PASS"; 

//function keycode for Firefox/Opera
function tableValid_isFunctionButton(e) {
	var keyCode = e.keyCode;
	if(e.which == 0) {
		if (keyCode == 0
			|| keyCode == 27 //Esc
			|| keyCode == 35 //end
			|| keyCode == 36 //home
			|| keyCode == 37 //<-
			|| keyCode == 39 //->
			|| keyCode == 45 //Insert
			|| keyCode == 46 //Del
			){
			return true;
		}
	}
	if (keyCode == 8 	//backspace
		|| keyCode == 9 	//tab
		){
		return true;
	}
	return false;
}

function tableValid_range(_value, _min, _max) {
	if(isNaN(_value))
		return false;

	var _valueTemp = parseInt(_value);

	if(_min > _max) {
		var tmpNum = "";
		tmpNum = _min;
		_min = _max;
		_max = tmpNum;
	}

	if(_valueTemp < _min || _valueTemp > _max)
		return false;

	return true;
}

function tableValid_block_chars(_value, keywordArray){
	// bolck ascii code 32~126 first
	var invalid_char = "";		
	for(var i = 0; i < _value.length; ++i) {
		if(_value.charCodeAt(i) < '32' || _value.charCodeAt(i) > '126'){
			invalid_char += _value.charAt(i);
		}
	}
	if(invalid_char != "")
		return'<#JS_validstr2#>" '+ invalid_char +'" !';

	// check if char in the specified array
	if(_value) {
		for(var i = 0; i < keywordArray.length; i++) {
			if( _value.indexOf(keywordArray[i]) >= 0) {						
				return keywordArray + " <#JS_invalid_chars#>";
			}	
		}
	}
	
	return HINTPASS;
}

function tableValid_getOriginalEditRuleArray($obj, _dataArray) {
	var originalEditRuleArray = [];
	var eleID = $obj.attr('id');
	var colIdx = eleID.replace("edit_item_", "").split("_")[0];
	var originalEditRuleArray = tableApi._attr.data[colIdx].slice();
	return originalEditRuleArray;
}
function tableValid_getCurrentEditRuleArray($obj, _dataArray) {
	var currentEditRuleArray = [];
	var eleID = $obj.attr('id');
	var eleValue = $obj.val();
	var colIdx = eleID.replace("edit_item_", "").split("_")[0];
	var rowIdx = eleID.replace("edit_item_", "").split("_")[1];
	var currentEditRuleArray = tableApi._attr.data[colIdx].slice();
	currentEditRuleArray[rowIdx] = eleValue;
	return currentEditRuleArray;
}
function tableValid_getFilterCurrentEditRuleArray($obj, _dataArray) {
	var filterCurrentEditRuleArray = [];
	var eleID = $obj.attr('id');
	var colIdx = eleID.replace("edit_item_", "").split("_")[0];
	var filterCurrentEditRuleArray = tableApi._attr.data.slice();
	filterCurrentEditRuleArray.splice(colIdx, 1);
	return filterCurrentEditRuleArray;
}
function tableValid_ipAddrToIPDecimal(ip_str) {
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4; //valid
	}
	
	return -2; //not valid
}
function tableValid_decimalToIPAddr(number) {
	var part1 = number & 255;
    var part2 = ((number >> 8) & 255);
    var part3 = ((number >> 16) & 255);
    var part4 = ((number >> 24) & 255);

    return part4 + "." + part3 + "." + part2 + "." + part1;
}

var tableValidator = {
	description : {
		keyPress : function($obj,event) {
			var keyPressed = event.keyCode ? event.keyCode : event.which;
			if(keyPressed >= 0 && keyPressed <= 126) {;
				if(keyPressed == 60 || keyPressed == 62)
					return false;
				else 
					return true;
			}
			else {
				return false;
			}
		},
		blur : function(_$obj) {
			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_$obj.val(_value);
			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else
				hintMsg = tableValid_block_chars(_value, ["<" ,">"]);

			if(_$obj.next().closest(".hint").length) {
				_$obj.next().closest(".hint").remove();
			}

			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.after($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	},

	portRange : {
		keyPress : function($obj,event) {
			var objValue = $obj.val();
			var keyPressed = event.keyCode ? event.keyCode : event.which;
			if (tableValid_isFunctionButton(event)) {
				return true;
			}

			if ((keyPressed > 47 && keyPressed < 58)) {	//0~9
				return true;
			}
			else if (keyPressed == 58 && objValue.length > 0) {
				for(var i = 0; i < objValue.length; i++) {
					var c = objValue.charAt(i);
					if (c == ':' || c == '>' || c == '<' || c == '=')
						return false;
				}
				return true;
			}

			return false;
		},
		blur : function(_$obj) {
			var eachPort = function(num, min, max) {	
				if(num < min || num > max) {
					return false;
				}
				return true;
			};
			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_$obj.val(_value);

			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else {
				var mini = 1;
				var maxi = 65535;
				var PortRange = _value;
				var rangere = new RegExp("^([0-9]{1,5})\:([0-9]{1,5})$", "gi");

				if(rangere.test(PortRange)) {
					if(parseInt(RegExp.$1) >= parseInt(RegExp.$2)) {
						hintMsg = _value + " <#JS_validportrange#>";
					}
					else{
						if(!eachPort(RegExp.$1, mini, maxi) || !eachPort(RegExp.$2, mini, maxi)) {
							hintMsg = "<#JS_validrange#> " + mini + " <#JS_validrange_to#> " + maxi;
						}
						else 
							hintMsg =  HINTPASS;
					}
				}
				else{
					if(!tableValid_range(_value, mini, maxi)) {
						hintMsg = "<#JS_validrange#> " + mini + " <#JS_validrange_to#> " + maxi;
					}
					else 
						hintMsg =  HINTPASS;
				}
			}
			if(_$obj.next().closest(".hint").length) {
				_$obj.next().closest(".hint").remove();
			}
			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.after($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	},

	portNum : {
		keyPress : function($obj, event) {
			var keyPressed = event.keyCode ? event.keyCode : event.which;

			if (tableValid_isFunctionButton(event)) {
				return true;
			}

			if ((keyPressed > 47 && keyPressed < 58)) {	//0~9
				return true;
			}
			
			return false;
		},
		blur : function(_$obj) {
			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_$obj.val(_value);
			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else {
				var mini = 1;
				var maxi = 65535;
				if(!tableValid_range(_value, mini, maxi)) {
					hintMsg = "<#JS_validrange#> " + mini + " <#JS_validrange_to#> " + maxi;
				}
				else 
					hintMsg = HINTPASS;
			}

			if(_$obj.next().closest(".hint").length) {
				_$obj.next().closest(".hint").remove();
			}
			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.after($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	},

	dualWanRoutingRules : { // only IP or IP plus netmask
		keyPress : function($obj,event) {
			var objValue = $obj.val();
			var keyPressed = event.keyCode ? event.keyCode : event.which;

			if (tableValid_isFunctionButton(event)) {
				return true;
			}

			var i,j;
			if((keyPressed > 47 && keyPressed < 58)){
				j = 0;
				
				for(i = 0; i < objValue.length; i++){
					if(objValue.charAt(i) == '.'){
						j++;
					}
				}
				
				if(j < 3 && i >= 3){
					if(objValue.charAt(i-3) != '.' && objValue.charAt(i-2) != '.' && objValue.charAt(i-1) != '.'){
						$obj.val(objValue + '.');
					}
				}
				
				return true;
			}
			else if(keyPressed == 46){
				j = 0;
				
				for(i = 0; i < objValue.length; i++){
					if(objValue.charAt(i) == '.'){
						j++;
					}
				}
				
				if(objValue.charAt(i-1) == '.' || j == 3){
					return false;
				}
				
				return true;
			}
			else if(keyPressed == 47){
				j = 0;
				
				for(i = 0; i < objValue.length; i++){
					if(objValue.charAt(i) == '.'){
						j++;
					}
				}
				
				if( j < 3){
					return false;
				}
				
				return true;
			}
			else if(keyPressed == 97 || keyPressed == 65) { //a or A
				return true;
			}
			else if(keyPressed == 108 || keyPressed == 76) { // l or L
				return true;
			}
			return false;
		},
		blur : function(_$obj) {
			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_value = _value.toLowerCase();
			_$obj.val(_value);
			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else {
				var startIPAddr = tableValid_ipAddrToIPDecimal("0.0.0.0");
				var endIPAddr = tableValid_ipAddrToIPDecimal("255.255.255.255");
				var ipNum = 0;
				if(_value == "all") {
					hintMsg = HINTPASS;
				}
				else if(_value.search("/") == -1) {	// only IP
					ipNum = tableValid_ipAddrToIPDecimal(_value);
					if(ipNum > startIPAddr && ipNum < endIPAddr) {
						hintMsg = HINTPASS;
						//convert number to ip address
						_$obj.val(tableValid_decimalToIPAddr(ipNum));
					}
					else {
						hintMsg = _value + " <#JS_validip#>";
					}
				}
				else{ // IP plus netmask
					if(_value.split("/").length > 2) {
						hintMsg = _value + " <#JS_validip#>";
					}
					else {
						var ip_tmp = _value.split("/")[0];
						var mask_tmp = parseInt(_value.split("/")[1]);
						ipNum = tableValid_ipAddrToIPDecimal(ip_tmp);
						if(ipNum > startIPAddr && ipNum < endIPAddr) {
							if(mask_tmp == "" || isNaN(mask_tmp))
								hintMsg = _value + " <#JS_validip#>";
							else if(mask_tmp == 0 || mask_tmp > 32)
								hintMsg = _value + " <#JS_validip#>";
							else {
								hintMsg = HINTPASS;
								//convert number to ip address
								_$obj.val(tableValid_decimalToIPAddr(ipNum) + "/" + mask_tmp);
							}
						}
						else {
							hintMsg = _value + " <#JS_validip#>";
						}
					}
				}
			}

			if(_$obj.next().closest(".hint").length) {
				_$obj.next().closest(".hint").remove();
			}

			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.after($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	},

	macAddress : {
		keyPress : function($obj,event) {
			var objValue = $obj.val();
			var keyPressed = event.keyCode ? event.keyCode : event.which;

			if (tableValid_isFunctionButton(event)) {
				return true;
			}

			var i, j;
			if((keyPressed > 47 && keyPressed < 58) || (keyPressed > 64 && keyPressed < 71) || (keyPressed > 96 && keyPressed < 103)) { //Hex
				j = 0;
				
				for(i = 0; i < objValue.length; i += 1) {
					if(objValue.charAt(i) == ':'){
						j++;
					}
				}

				if(j < 5 && i >= 2) {
					if(objValue.charAt(i-2) != ':' && objValue.charAt(i-1) != ':') {
						$obj.val(objValue + ":");
					}
				}
				
				return true;
			}	
			else if(keyPressed == 58 || keyPressed == 13) {	//symbol ':' & 'ENTER'
				return true;
			}
			else{
				return false;
			}
		},
		blur : function(_$obj) {
			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_$obj.val(_value);
			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else {
				var hwaddr = new RegExp("(([a-fA-F0-9]{2}(\:|$)){6})", "gi");			
				var legal_hwaddr = new RegExp("(^([a-fA-F0-9][aAcCeE02468])(\:))", "gi"); // for legal MAC, unicast & globally unique (OUI enforced)

				if(!hwaddr.test(_value))
					hintMsg = "<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
				else if(!legal_hwaddr.test(_value))
					hintMsg = "<#IPConnection_x_illegal_mac#>";
				else
					hintMsg = HINTPASS;
			}

			if(_$obj.parent().siblings('.hint').length) {
				_$obj.parent().siblings('.hint').remove();
			}
			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.parent().parent().append($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	},

	ipAddressDHCP : {
		keyPress : function($obj,event) {
			var objValue = $obj.val();
			var keyPressed = event.keyCode ? event.keyCode : event.which;

			var i, j;
			if (tableValid_isFunctionButton(event)) {
				return true;
			}

			if((keyPressed > 47 && keyPressed < 58)) {
				j = 0;

				for(i = 0; i < objValue.length; i += 1) {
					if(objValue.charAt(i) == '.') {
						j++;
					}
				}

				if(j < 3 && i >= 3) {
					if(objValue.charAt(i-3) != '.' && objValue.charAt(i-2) != '.' && objValue.charAt(i-1) != '.') {
						$obj.val(objValue + ".");
					}
				}

				return true;
			}
			else if(keyPressed == 46) {
				j = 0;

				for(i = 0; i < objValue.length; i += 1) {
					if(objValue.charAt(i) == '.') {
						j++;
					}
				}

				if(objValue.charAt(i-1) == '.' || j == 3) {
					return false;
				}

				return true;
			}
			else if(keyPressed == 13) {	// 'ENTER'
				return true;
			}

			return false;
		},
		blur : function(_$obj) {
			var validate_dhcp_range = function(_setIPAddr) {
				var dhcp_range_flag = true;
				var dhcpIPAddr = tableValid_ipAddrToIPDecimal('<% nvram_get("lan_ipaddr"); %>');
				var dhcpMASKAddr = tableValid_ipAddrToIPDecimal('<% nvram_get("lan_netmask"); %>');
				var dhcpSubnetStart = dhcpIPAddr-(dhcpIPAddr&~dhcpMASKAddr);
				var dhcpSubnetEnd =  dhcpSubnetStart+~dhcpMASKAddr;
				var setIPAddr = tableValid_ipAddrToIPDecimal(_setIPAddr);
				if(setIPAddr <= dhcpSubnetStart || setIPAddr >= dhcpSubnetEnd)
					dhcp_range_flag = false;

				return dhcp_range_flag;
			};

			var hintMsg = "";
			var _value = _$obj.val();
			_value = $.trim(_value);
			_$obj.val(_value);
			if(_value == "") {
				if(_$obj.hasClass("valueMust"))
					hintMsg = "<#JS_fieldblank#>";
				else 
					hintMsg = HINTPASS;
			}
			else {
				var ipformat = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;  
				if(!_value.match(ipformat))
					hintMsg = _value + " <#JS_validip#>";
				else if(!validate_dhcp_range(_value))
					hintMsg = _value + " <#JS_validip#>";
				else
					hintMsg = HINTPASS;
			}

			if(_$obj.next().closest(".hint").length) {
				_$obj.next().closest(".hint").remove();
			}
			if(hintMsg != HINTPASS) {
				var $hintHtml = $('<div>');
				$hintHtml.addClass("hint");
				$hintHtml.html(hintMsg);
				_$obj.after($hintHtml);
				_$obj.focus();
				return false;
			}
			return true;
		}
	}
};

var tableRuleDuplicateValidation = {
	triggerPort : function(_newRuleArray, _currentRuleArray) {
		//check Trigger Port and Protocol and Incoming Port and Protocol whether duplicate or not 
		if(_currentRuleArray.length == 0)
			return true;
		else {
			var newRuleArrayTemp = _newRuleArray.slice();
			newRuleArrayTemp.splice(0, 1); 
			for(var i = 0; i < _currentRuleArray.length; i += 1) {
				var currentRuleArrayTemp = _currentRuleArray[i].slice();
				currentRuleArrayTemp.splice(0, 1);
				if(newRuleArrayTemp.toString() == currentRuleArrayTemp.toString())
					return false;
			}

		}
		return true;
	},
	dualWANRoutingRules : function(_newRuleArray, _currentRuleArray) {
		//check Source IP and Destination IP whether duplicate or not 
		if(_currentRuleArray.length == 0)
			return true;
		else {
			var newRuleArrayTemp = _newRuleArray.slice();
			newRuleArrayTemp.splice(2, 1);
			for(var i = 0; i < _currentRuleArray.length; i += 1) {
				var currentRuleArrayTemp = _currentRuleArray[i].slice();
				currentRuleArrayTemp.splice(2, 1);
				if(newRuleArrayTemp.toString() == currentRuleArrayTemp.toString())
					return false;
			}

		}
		return true;
	},
	vpncExceptionList  : function(_newRuleArray, _currentRuleArray) {
		if(_currentRuleArray.length == 0)
			return true;
		else {
			var newRuleArrayTempMAC = _newRuleArray.slice();
			var newRuleArrayTempIP = _newRuleArray.slice();
			newRuleArrayTempMAC.splice(1, 3);
			newRuleArrayTempIP.splice(0, 1);
			newRuleArrayTempIP.splice(1, 2);
			for(var i = 0; i < _currentRuleArray.length; i += 1) {
				var currentRuleArrayTempMAC = _currentRuleArray[i].slice();
				var currentRuleArrayTempIP = _currentRuleArray[i].slice();
				currentRuleArrayTempMAC.splice(1, 3);
				currentRuleArrayTempIP.splice(0, 1);
				currentRuleArrayTempIP.splice(1, 2);
				if(newRuleArrayTempMAC.toString() == currentRuleArrayTempMAC.toString()) //compare mac
					return false;
				if(newRuleArrayTempIP.toString() == currentRuleArrayTempIP.toString()) //compare ip
					return false;
			}

		}
		return true;
	}
}

var tableRuleValidation = {
	dualWANRoutingRules : function(_newRuleArray) {
		if(_newRuleArray.length == 3) {
			if(_newRuleArray[0] == _newRuleArray[1]) {
				return "Source IP and Destination IP can not be ALL at the same time";
			}
			return HINTPASS;
		}
	}
}
