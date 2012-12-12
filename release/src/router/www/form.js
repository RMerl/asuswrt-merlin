// Form validation and smooth UI

function checkIP(o,e){
	var nextInputBlock = o.nextSibling.nextSibling; //find the next input (sibling include ".")
	var nc = window.event ? e.keyCode : e.which;
	var s = o.value;
	//alert("nc="+nc);
	//document.frm.test.value = nc;
	if((nc>=48 && nc<=57) || (nc>=97 && nc<=105) || nc==8 || nc==0)
	{
		return true;
	}
	else if(nc==190 || nc==32 || nc==110 || nc==46){
		;
	}else if((nc==37 || nc==8) && s==""){
		;
	}	
	else{
		nc=0;
		return false;
	}
	//Range handler
}


var moveLeft_key = 0;
var moveRight_key = 0;
function checkWord(o,e){
	var nextInputBlock = o.nextSibling.nextSibling; //find the next input (sibling include ".")
	var prevInputBlock = o.previousSibling;
	prevInputBlock = (prevInputBlock != null)?prevInputBlock.previousSibling:o;
	//alert(prevInputBlock.value);
	
	var sk = window.event ? e.keyCode : e.which;
	var s = o.value;
	
	if((sk>=48 && sk<=57) || (sk>=97 && sk<=105) || sk==0) // 0->other key
	{
		if(s.length == 3){
			if(getCaretPos(o) == 3 && nextInputBlock){
				if(validateRange(o)){
					nextInputBlock.focus();
					nextInputBlock.select();
				}
			}
			else{
				validateRange(o);
			}
		}
		else if(s.length == getCaretPos(o)){
			moveRight_key = 1;
		}
		moveLeft_key = 0;
	}
	else if(sk==190 || sk==110 || sk==32){ //190 & 110-> "dot", 32->"space"
		if(o.value == '.' || o.value == '..' || o.value == '...'
				|| o.value == ' ' || o.value == '  ' || o.value == '   '){
			o.value = "";
			o.focus();
		}
		else{
			validateIP(o);
			if(nextInputBlock){
				nextInputBlock.focus();
				nextInputBlock.select();
			}
		}
	}
	else if(sk==8){ //8->backspace
		if(getCaretPos(o) == 0 && moveLeft_key == 0){
			moveLeft_key = 1;
		}
		else if(getCaretPos(o) == 0){
			if(prevInputBlock){
				prevInputBlock.focus();
				prevInputBlock.select();
				moveLeft_key = 0;
				moveRight_key = 1;
			}
		}
	}
	else if(sk==37){ // 37-> 鍵盤向左鍵
		if(getCaretPos(o) == 0 && moveLeft_key == 0){
			moveLeft_key = 1;
		}
		else if(prevInputBlock && getCaretPos(o) == 0 && moveLeft_key == 1){
			prevInputBlock.focus();
			prevInputBlock.select();
			moveLeft_key = 0;
		}
		else{
			moveLeft_key = 0;
		}
		moveRight_key = 0;
	}
	else if(sk==39){ //39 ->鍵盤向右鍵
		if(getCaretPos(o) == 0 && s.length == 0 && nextInputBlock && validateRange(o)){
			nextInputBlock.focus();
			moveRight_key = 0;
			moveLeft_key = 1;
		}
		else if(getCaretPos(o) == s.length && moveRight_key == 0){
			moveRight_key = 1;
			moveLeft_key = 0;
		}
		else if(moveRight_key == 1 && nextInputBlock && validateRange(o)){
			
			nextInputBlock.focus();
			moveRight_key = 0;
			moveLeft_key = 1;
		}
		else{
			moveLeft_key = 0;
		}
	}
	else{
		if(isNaN(s) && s.length >= 1 && sk != 13){
				alert("<#LANHostConfig_x_DDNS_alarm_9#>");
				o.focus();
				o.select();	
				return false;
		}
		else{
			nc=0;
			return false;
		}
	}	
}

function validateIP(o){
	//alert("blur"+o.value);
	var s = o.value;	

	if(s.indexOf(".") == 0 || s.indexOf(" ") == 0){
		o.value = s.substring(1,2);
	}
	else if(s.indexOf(".") == 1 || s.indexOf(" ") == 1){
		o.value = s.substring(0,1);
	}
	else if(s.indexOf(".") == 2 || s.indexOf(" ") == 2){
		o.value = s.substring(0,2);
	}	
}

function validateRange(v){
	if(v.value < 0 || v.value >= 256){
		alert(v.value+" <#BM_alert_IP2#>. \n<#BM_alert_port1#> 0<#BM_alert_to#>255.");
		v.focus();
		v.select();
		return false;
	}else{
		return true;
	}
}

function combineIP(obj){ //combine all IP info before validation and submit
	
	var IP_List = document.getElementById(obj+"_div").childNodes;
	var IP_str = "";
	
	for(var i=0; i < IP_List.length; i++){	
		//alert("\'"+ IP_List[i].nodeValue+ "\'");
		if(IP_List[i].type == "text"){
			if(IP_List[i].value != ""){
				IP_List[i].value = parseInt(IP_List[i].value,10);
			}
			
			IP_str += IP_List[i].value;
		}
		else if(IP_List[i].nodeValue.indexOf(".") != -1){
			IP_str += ".";                                  
		}
	}
	if(IP_str != "..."){
		document.getElementById(obj).value = IP_str;
	}else{
		document.getElementById(obj).value = "";
	}
}

function fulfillIP(obj){

	var src_ip = obj.value.split(".");
	var IP_List = document.getElementById(obj.name+"_div").childNodes;
	
	for(var i=0,j=0; i < IP_List.length, j<src_ip.length; i++){
		if(IP_List[i].type == "text"){
			IP_List[i].value = src_ip[j];
			j++;
		}
	}
}

function IPinputCtrl(obj, t){
	var IP_List = document.getElementById(obj.name+"_div").childNodes;
	
	document.getElementById(obj.name+"_div").style.background = (t==0)?"#999999":"#FFFFFF";
	for(var i=0; i < IP_List.length; i++){
		if(IP_List[i].type == "text"){
			IP_List[i].disabled = (t==0)?true:false;
			IP_List[i].style.color = (t==0)?"#FFFFFF":"#000000";
			IP_List[i].style.background = (t==0)?"#999999":"#FFFFFF";
		}
	}
}

function getCaretPos(obj){
 	
 	if(obj.selectionStart >= 0){
 		return obj.selectionStart; //Gecko
 	}
 	else{                        //For IE
    var currentRange=document.selection.createRange();   
    var workRange=currentRange.duplicate();
    obj.select();
    var allRange=document.selection.createRange();   
    var len=0;   
    while(workRange.compareEndPoints("StartToStart",allRange)>0){   
      workRange.moveStart("character",-1);   
      len++;   
    }   
    currentRange.select();   
    return   len;
  }
}

function IP_Validate(o){
	var ip_reg=/^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/;
	
	if(ip_reg.test(o.value)){ //區分是否為IP
		return true;
	}
	else{
		return false;
	}
}

function validate_ipaddr_final(o,v){
	if(v == 'wan_ipaddr_x'){
		if(o.value.length == 0){    /*Blank.*/
			alert(o.title+"<#JS_fieldblank#>");
			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			document.form.wan_ipaddr_x1.focus();
			return false;
		}
		else if(o.value.indexOf("0") == 0){ /*首字不能為0*/
			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			alert(document.form.wan_ipaddr_x.value + " <#JS_validip#>");
			document.form.wan_ipaddr_x1.focus();
			return false;
		}		
		else if(!(IP_Validate(o))){ /*IP格式錯誤*/
			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			alert(document.form.wan_ipaddr_x.value + " <#JS_validip#>");
			document.form.wan_ipaddr_x4.focus();
			return false;
		}
		else if(IP_Validate(o)){    
			if(document.form.wan_ipaddr_x1.value >= 224){
				alert("<#JS_field_wanip_rule2#>");
				document.form.wan_ipaddr_x1.focus();
				document.form.wan_ipaddr_x1.select();
				return false;
			}
			else if(document.form.wan_ipaddr_x1.value == 127){
				alert(document.form.wan_ipaddr_x1.value + "<#JS_field_wanip_rule1#>");
				document.form.wan_ipaddr_x1.focus();
				document.form.wan_ipaddr_x1.select();
				return false;
			}
			else{
				return true;
			}
		}
	}
	else if(v == 'wan_netmask_x'){
		var wan_ipaddr_x1 = document.form.wan_ipaddr_x1.value;
		if(o.value.length == 0){    /*Blank.*/

			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			
			if(confirm(o.title+"<#JS_fieldblank#>\n<#JS_field_fulfillSubmask#>")){
				if((wan_ipaddr_x1 > 0) && (wan_ipaddr_x1 < 127)) o.value = "255.0.0.0";
				else if ((wan_ipaddr_x1 > 127) && (wan_ipaddr_x1 < 192)) o.value = "255.255.0.0";
				else if ((wan_ipaddr_x1 > 191) && (wan_ipaddr_x1 < 224)) o.value = "255.255.255.0";
				else o.value = "0.0.0.0";
				fulfillIP(o);
			}
			document.form.wan_netmask_x1.focus();
			return false;
		}
		else if(!(IP_Validate(o))){ /*IP格式錯誤*/
			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			alert(o.value + " <#JS_validip#>");
			return false;
		}
		else if(IP_Validate(o)){    
			if(requireWANIP(v) && (
			(matchSubnet2(document.form.wan_ipaddr_x.value, o, document.form.lan_ipaddr.value, document.form.lan_netmask))
			)){
				alert(o.value + " <#JS_validip#>");
				document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
				return false;
			}
			else{
				return true;
			}
		}
	}
	else if(v == 'wan_gateway_x'){
		if(o.value.length > 0){
			if(!(IP_Validate(o))){ /* IP格式錯誤*/
				document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
				alert(o.value + " <#JS_validip#>");
				return false;
			}
			else if(o.value == document.form.wan_ipaddr_x.value){
				alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
				document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
				return false;			
			}
		}
		
		return true;	
	}
	else if(v == 'wan_dns1_x' || v == 'wan_dns2_x'){
		
		var split_IP = o.value.split(".");
		
		if(!(IP_Validate(o))){ 
			document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
			alert(o.value + " <#JS_validip#>");
			return false;
		}
		return true;
		/*
		else if(IP_Validate(o)){ 
			if(split_IP[0]==255||split_IP[1]==255||split_IP[2]==255||split_IP[3]==255||split_IP[0]==0||split_IP[3]==0||split_IP[0]==127||split_IP[0]==224){
				alert(o.value +" <#JS_validip#>");
				document.getElementById(o.name+"_div").style.border = "2px solid #CE1E1E";
				return false;
			}			
			else{
				return true;
			}
		}
		*/
	}
}

