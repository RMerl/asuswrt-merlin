var change;
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
// 2010.07 James. }

function changeDate(){
	pageChanged = 1;
	return true;
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

function is_functionButton(e){
//function keycode for Firefox/Opera
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
	if (       keyCode == 8 	//backspace
		|| keyCode == 9 	//tab
		){
		return true;
	}
	return false;
}

function is_hwaddr(o,event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	if (is_functionButton(event)){
		return true;
	}

	if((keyPressed>47 && keyPressed<58)||(keyPressed>64 && keyPressed<71)||(keyPressed>96 && keyPressed<103)){	//Hex
		j = 0;
		
		for(i = 0; i < o.value.length; i++){
			if(o.value.charAt(i) == ':'){
				j++;
			}
		}
		
		if(j < 5 && i >= 2){
			if(o.value.charAt(i-2) != ':' && o.value.charAt(i-1) != ':'){
				o.value = o.value+':';
			}
		}
		
		return true;
	}	
	else if(keyPressed == 58 || keyPressed == 13){	//symbol ':' & 'ENTER'
		return true;
	}
	else{
		return false;
	}
}

function validate_ssidchar(ch){
	if(ch >= 32 && ch <= 126)
		return false;
	
	return true;
}

function validate_string_ssid(o){
	var c;	// character code
	var flag=0; // notify valid characters of SSID except space
	
	if(o.value==""){      // to limit null SSID
		alert('<#JS_fieldblank#>');
		return false;
	}	
	
	for(var i = 0; i < o.value.length; ++i){
		c = o.value.charCodeAt(i);
		
		if(validate_ssidchar(c)){
			alert('<#JS_validSSID1#> '+o.value.charAt(i)+' <#JS_validSSID2#>');
			o.value = "";
			o.focus();
			o.select();
			return false;
		}
		
		if(c != 32)
			flag ++;
	}
	
	if(flag ==0){     // to limit SSID only include space
		alert('<#JS_fieldblank#>');
		return false;
	}
	
	return true;
}

function validate_string_group(o){
	var c;	// character code	
	for(var i = 0; i < o.value.length; ++i){
		c = o.value.charCodeAt(i);
		
		if(!validate_groupchar(c)){
			alert('<#JS_validSSID1#> '+o.value.charAt(i)+' <#JS_validSSID2#>');
			o.focus();
			o.select();
			return false;
		}
	}
	return true;
}

function validate_groupchar(ch){
	if(ch == 60 || ch == 62) /*ch == 39 || */
		return false;
	
	return true;
}

function is_number(o,event){	
	keyPressed = event.keyCode ? event.keyCode : event.which;
	
	if (is_functionButton(event)){
		return true;
	}

	if (keyPressed>47 && keyPressed<58){
		/*if (keyPressed==48 && o.value.length==0){	//single 0
			return false;	
		}*/
		return true;
	}
	else{
		return false;
	}
}

function validate_range(o, min, max) {
	for(i=0; i<o.value.length; i++){
		if ((o.value.charAt(i)<'0' || o.value.charAt(i)>'9') && o.value.charAt(i) != "-"){
			alert('<#JS_validrange#> ' + min + ' <#JS_validrange_to#> ' + max);
			//o.value = max;
			o.focus();
			o.select();
			return false;
		}
	}
	
	if(o.value<min || o.value>max) {
		alert('<#JS_validrange#> ' + min + ' <#JS_validrange_to#> ' + max);
		//o.value = max;
		o.focus();
		o.select();
		return false;
	}
	else {
		o.value = str2val(o.value);
		if(o.value=="")
			o.value="0";
			
		return true;
	}
}

function validate_range_sp(o, min, max, def) {		//allow to set "0"
	if (o.value==0) return true;
	
	if(o.value<min || o.value>max) {
		alert('<#JS_validrange#> ' + min + ' <#JS_validrange_to#> ' + max + '.');
		o.value = def;
		o.focus();
		o.select();
		return false;
	}
	else {
		o.value = str2val(o.value);
		if(o.value=="") 
			o.value="0";
		return true;
	}
}

function is_ipaddr(o,event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	if (is_functionButton(event)){
		return true;
	}

	if((keyPressed > 47 && keyPressed < 58)){
		j = 0;
		
		for(i = 0; i < o.value.length; i++){
			if(o.value.charAt(i) == '.'){
				j++;
			}
		}
		
		if(j < 3 && i >= 3){
			if(o.value.charAt(i-3) != '.' && o.value.charAt(i-2) != '.' && o.value.charAt(i-1) != '.'){
				o.value = o.value+'.';
			}
		}
		
		return true;
	}
	else if(keyPressed == 46){
		j = 0;
		
		for(i = 0; i < o.value.length; i++){
			if(o.value.charAt(i) == '.'){
				j++;
			}
		}
		
		if(o.value.charAt(i-1) == '.' || j == 3){
			return false;
		}
		
		return true;
	}else if(keyPressed == 13){	// 'ENTER'
		return true;
	}
	
	return false;
}

function requireWANIP(v){
	if(v == 'wan_ipaddr_x' || v == 'wan_netmask_x' ||
			v == 'lan_ipaddr' || v == 'lan_netmask' ||
			v == 'lan1_ipaddr' || v == 'lan1_netmask'){
		// 2008.03 James. patch for Oleg's patch. {
		/*if(document.form.wan_proto.value == "static" || document.form.wan_proto.value == "pptp")
			return 1;
		else
			return 0;*/
		if(document.form.wan_proto.value == "static")
			return 1;
		else if(document.form.wan_proto.value == "pppoe" && inet_network(document.form.wan_ipaddr_x.value))
			return 1;
		else if((document.form.wan_proto.value=="pptp" || document.form.wan_proto.value == "l2tp")
				&& document.form.wan_ipaddr_x.value != '0.0.0.0')
			return 1;
		else
			return 0;
		// 2008.03 James. patch for Oleg's patch. }
	}
	
	else return 0;
}

function matchSubnet2(wan_ip1, wan_sb1,lan_ip2,lan_sb2){
	var nsb;
	var nsb1 = inet_network(wan_sb1.value);
	var nsb2 = inet_network(lan_sb2.value);
	var nip1 = inet_network(wan_ip1);

	if(nip1 == 0 || nip1 == -1) // check if WAN IP = 0.0.0.0 or NULL (factory default value)
		return 1;

	if(nsb1 < nsb2 && nsb1 !=0)
		nsb = nsb1;
	else
		nsb = nsb2;
	
	if((inet_network(wan_ip1)&nsb) == (inet_network(lan_ip2)&nsb))
		return 1;
	else
		return 0;
}

function validate_ipaddr_final(o, v){
	var num = -1;
	var pos = 0;
		
	if(o.value.length == 0){
		if(v == 'dhcp_start' || v == 'dhcp_end' ||
				v == 'wan_ipaddr_x' ||
				v == 'dhcp1_start' || v=='dhcp1_end' ||
				v == 'lan_ipaddr' || v=='lan_netmask' ||
				v=='lan1_ipaddr' || v=='lan1_netmask' ||
				v == 'wl_radius_ipaddr') {	
			alert("<#JS_fieldblank#>");
			
			if(v == 'wan_ipaddr_x'){
				document.form.wan_ipaddr_x.value = "10.1.1.1";
				document.form.wan_netmask_x.value = "255.0.0.0";
			}
			else if(v == 'lan_ipaddr'){
				document.form.lan_ipaddr.value = "192.168.1.1";
				document.form.lan_netmask.value = "255.255.255.0";
			}
			else if(v == 'lan1_ipaddr'){
				document.form.lan1_ipaddr.value = "192.168.2.1";
				document.form.lan1_netmask.value = "255.255.255.0";
			}
			else if(v == 'lan_netmask')
				document.form.lan_netmask.value = "255.255.255.0";
			else if(v == 'lan1_netmask')
				document.form.lan1_netmask.value = "255.255.255.0";
			
			o.focus();
			o.select();
			
			return false;
		}
		else
			return true;
	}
	
	if(v == 'wan_ipaddr_x' && document.form.wan_netmask_x.value == "")
		document.form.wan_netmask_x.value="255.255.255.0";
	
	for(var i = 0; i < o.value.length; ++i){
		var c = o.value.charAt(i);
		
		if(c >= '0' && c <= '9'){
			if(num == -1 ){
				num = (c-'0');
			}
			else{
				num = num*10+(c-'0');
			}
		}
		else{
			if(num < 0 || num > 255 || c != '.'){
				alert(o.value+" <#JS_validip#>");
				
				o.value = "";
				o.focus();
				o.select();
				
				return false;
			}
			
			if(pos == 0)
				v1 = num;
			else if(pos == 1)
				v2 = num;
			else if(pos == 2)
				v3 = num;
			
			num = -1;
			++pos;
		}
	}
	
	if(pos!=3 || num<0 || num>255){
		alert(o.value + " <#JS_validip#>");
		o.value = "";
		o.focus();
		o.select();
		
		return false;
	}
	else
		v4 = num;
	
	if(v == 'dhcp_start' || v == 'dhcp_end' ||
			v == 'wan_ipaddr_x' ||
			v == 'dhcp1_start' || v == 'dhcp1_end' ||
			v == 'lan_ipaddr' || v == 'lan1_ipaddr' ||
			v == 'staticip' || v == 'wl_radius_ipaddr' ||
			v == 'dhcp_dns1_x' || v == 'dhcp_gateway_x' || v == 'dhcp_wins_x'){
		if((v!='wan_ipaddr_x')&& (v1==255||v4==255||v1==0||v4==0||v1==127||v1==224)){
			alert(o.value + " <#JS_validip#>");
			
			o.value = "";
			o.focus();
			o.select();
			
			return false;
		}
		
		if((wan_route_x == "IP_Bridged" && wan_nat_x == "0") || sw_mode=="2" || sw_mode=="3")	// variables are defined in state.js
			;	// there is no WAN in AP mode, so it wouldn't be compared with the wan ip..., etc.
		else if(requireWANIP(v) && (
				(v=='wan_ipaddr_x' &&  matchSubnet2(o.value, document.form.wan_netmask_x, document.form.lan_ipaddr.value, document.form.lan_netmask)) ||
				(v=='lan_ipaddr' &&  matchSubnet2(o.value, document.form.lan_netmask, document.form.wan_ipaddr_x.value, document.form.wan_netmask_x))
				)){
			alert(o.value + " <#JS_validip#>");
			if(v == 'wan_ipaddr_x'){
				document.form.wan_ipaddr_x.value = "10.1.1.1";
				document.form.wan_netmask_x.value = "255.0.0.0";
			}
			else if(v == 'lan_ipaddr'){
				document.form.lan_ipaddr.value = "192.168.1.1";
				document.form.lan_netmask.value = "255.255.255.0";
			}
			else if(v == 'lan1_ipaddr'){
				document.form.lan1_ipaddr.value = "192.168.2.1";
				document.form.lan1_netmask.value = "255.255.255.0";
			}
			
			o.focus();
			o.select();
			
			return false;
		}
	}
	else if(v=='lan_netmask' || v=='lan1_netmask'){
		if(v1==255&&v2==255&&v3==255&&v4==255){
			alert(o.value + " <#JS_validip#>");
			o.value = "";
			o.focus();
			o.select();
			return false;
		}
	}
	
	if((wan_route_x == "IP_Bridged" && wan_nat_x == "0") || sw_mode=="2" || sw_mode=="3")	// variables are defined in state.js
			;	// there is no WAN in AP mode, so it wouldn't be compared with the wan ip..., etc.
	else if(requireWANIP(v) && (
			(v=='wan_netmask_x' &&  matchSubnet2(document.form.wan_ipaddr_x.value, o, document.form.lan_ipaddr.value, document.form.lan_netmask)) ||
			(v=='lan_netmask' &&  matchSubnet2(document.form.lan_ipaddr.value, o, document.form.wan_ipaddr_x.value, document.form.wan_netmask_x))
			)){
		alert(o.value + " <#JS_validip#>");
		
		if (v=='wan_netmask_x'){
			document.form.wan_ipaddr_x.value = "10.1.1.1";
			document.form.wan_netmask_x.value = "255.0.0.0";
		}
		else if(v=='lan_netmask'){
			document.form.lan_ipaddr.value = "192.168.1.1";
			document.form.lan_netmask.value = "255.255.255.0";
		}
		else if(v=='lan1_netmask'){
			document.form.lan1_ipaddr.value = "192.168.2.1";
			document.form.lan1_netmask.value = "255.255.255.0";
		}
		
		o.focus();
		o.select();
		return false;
	}
		
		
	o.value = v1 + "." + v2 + "." + v3 + "." + v4;
	
	if((v1 > 0) && (v1 < 127)) mask = "255.0.0.0";
	else if ((v1 > 127) && (v1 < 192)) mask = "255.255.0.0";
	else if ((v1 > 191) && (v1 < 224)) mask = "255.255.255.0";
	else mask = "0.0.0.0";
	
	if(v=='wan_ipaddr_x' && document.form.wan_netmask_x.value==""){
		document.form.wan_netmask_x.value = mask;
	}
	else if (v=='lan_ipaddr' && document.form.lan_netmask.value=="" ){
		document.form.lan_netmask.value = mask;
	}else if (v=='dhcp_start'){
		if (!matchSubnet(document.form.lan_ipaddr.value, document.form.dhcp_start.value, 3)){
			alert(o.value + " <#JS_validip#>");
			o.focus();
			o.select();
			return false;
		}
	}
	else if (v=='dhcp_end'){
		if (!matchSubnet(document.form.lan_ipaddr.value, document.form.dhcp_end.value, 3)){
			alert(o.value + " <#JS_validip#>");
			o.focus();
			o.select();
			return false;
		}
	}
	else if (v=='lan1_ipaddr'){
		if(document.form.lan1_netmask.value=="") document.form.lan1_netmask.value = mask;
	}
	else if (v=='dhcp1_start'){
		if (!matchSubnet(document.form.lan1_ipaddr.value, document.form.dhcp1_start.value, 3)){
			alert(o.value + " <#JS_validip#>");
			o.focus();
			o.select();
			return false;
		}
	}
	else if (v=='dhcp1_end'){
		if (!matchSubnet(document.form.lan1_ipaddr.value, document.form.dhcp1_end.value, 3)){
			alert(o.value + " <#JS_validip#>");
			o.focus();
			o.select();
			return false;
		}
	}
	
	return true;
}

function is_iprange(o, event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	//function keycode for Firefox/Opera
	if (is_functionButton(event)){
		return true;
	}

	if ((keyPressed > 47 && keyPressed < 58)){	// 0~9
		j = 0;
		for(i=0; i<o.value.length; i++){
			if (o.value.charAt(i)=='.'){
				j++;
			}
		}
		if (j<3 && i>=3){
			if (o.value.charAt(i-3)!='.' && o.value.charAt(i-2)!='.' && o.value.charAt(i-1)!='.')
			o.value = o.value + '.';
		}
		return true;
	}
	else if (keyPressed == 46){	// .
		j = 0;
		for(i=0; i<o.value.length; i++){
			if (o.value.charAt(i)=='.'){
				j++;
			}
		}
		if (o.value.charAt(i-1)=='.' || j==3){
			return false;
		}
		return true;
	}
	else if (keyPressed == 42){ // *
		return true;
	}
	
	return false;
}
function validate_iprange(o, v)
{num = -1;
pos = 0;
if (o.value.length==0)
return true;
for(i=0; i<o.value.length; i++)
{c=o.value.charAt(i);
if(c>='0'&&c<='9')
{if ( num==-1 )
{num = (c-'0');
}
else
{num = num*10 + (c-'0');
}
}
else if (c=='*'&&num==-1)
{num = 0;
}
else
{if ( num<0 || num>255 || (c!='.'))
{alert(o.value + " <#JS_validip#>");
o.value = "";
o.focus();
o.select();
return false;
}
num = -1;
pos++;
}
}
if (pos!=3 || num<0 || num>255)
{alert(o.value + " <#JS_validip#>");
o.value = "";
o.focus();
o.select();
return false;
}
if (v=='ExternalIPAddress' && document.form.wan_netmask_x.value == '')
{document.form.wan_netmask_x.value = "255.255.255.0";
}
else if (v=='IPRouters' && document.form.lan_netmask.value == '')
{document.form.lan_netmask.value = "255.255.255.0";
}
return true;
}
function is_portrange(o,event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	if (is_functionButton(event)){
		return true;
	}

	if ((keyPressed > 47 && keyPressed < 58)){	//0~9
		return true;
	}
	else if (keyPressed == 58 && o.value.length>0){
		for(i=0; i<o.value.length; i++){
			c=o.value.charAt(i);
			if (c==':' || c=='>' || c=='<' || c=='=')
			return false;
		}
		return true;
	}
	else if (keyPressed==44){  //"�? can be type in first charAt  ::: 0220 Lock add"
		if (o.value.length==0)
			return false;		
		else
			return true;
	}
	else if (keyPressed==60 || keyPressed==62){  //">" and "<" only can be type in first charAt ::: 0220 Lock add
		if (o.value.length==0)
			return true;		
		else
			return false;
	}

	return false;
}

function is_portlist(o,event){
	keyPressed = event.keyCode ? event.keyCode : event.which;

	//function keycode for Firefox/Opera
	if (is_functionButton(event)){
		return true;
	}
		
	if ((keyPressed>47 && keyPressed<58) || keyPressed == 32){
		return true;
	}
	else{
		return false;
	}
}

function validate_portlist(o, v){
	if (o.value.length==0)
		return true;
	num = 0;
	for(i=0; i<o.value.length; i++){
		c=o.value.charAt(i);
		if (c>='0'&&c<='9'){
			num = num*10 + (c-'0');
		}
		else{
			if (num>255){
				alert(num + " <#JS_validport#>");
				
			o.focus();
			o.select();
			return false;
			}
			num = 0;
		}
	}
		
	if (num>255){
		alert(num + " <#JS_validport#>");
		o.focus();
		o.select();
		return false;
	}
	
	return true;
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

function add_a_option(o, i, s)
{tail = o.options.length;
	o.options[tail] = new Option(s);
	o.options[tail].value = i;
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

function load_body(){
	document.form.next_host.value = location.host;
	
	if(document.form.current_page.value == "Advanced_WMode_Content.asp"){		
		change_wireless_bridge(document.form.wl_mode_x.value, rcheck(document.form.wl_wdsapply_x), 0, 0);		
	}
	else if(document.form.current_page.value == "Advanced_PortTrigger_Content.asp"){
		wItem = new Array(new Array("Quicktime 4 Client", "554", "TCP", "6970:32000", "UDP"),new Array("Real Audio", "7070", "TCP", "6970:7170", "UDP"));
		free_options(document.form.TriggerKnownApps);
		add_option(document.form.TriggerKnownApps, "<#Select_menu_default#>", "User Defined", 1);
		for (i = 0; i < wItem.length; i++){
			add_option(document.form.TriggerKnownApps, wItem[i][0], wItem[i][0], 0);
		}
	}	
	else if(document.form.current_page.value == "Advanced_BasicFirewall_Content.asp"){
		change_firewall(rcheck(document.form.fw_enable_x));
	}
	else if (document.form.current_page.value == "Advanced_Firewall_Content.asp"){
		wItem = new Array(new Array("WWW", "80", "TCP"),new Array("TELNET", "23", "TCP"),new Array("FTP", "20:21", "TCP"));
		free_options(document.form.LWKnownApps);
		add_option(document.form.LWKnownApps, "User Defined", "User Defined", 1);
		for (i = 0; i < wItem.length; i++){
			add_option(document.form.LWKnownApps, wItem[i][0], wItem[i][0], 0);
		}
		document.form.filter_lw_date_x_Sun.checked = getDateCheck(document.form.filter_lw_date_x.value, 0);
		document.form.filter_lw_date_x_Mon.checked = getDateCheck(document.form.filter_lw_date_x.value, 1);
		document.form.filter_lw_date_x_Tue.checked = getDateCheck(document.form.filter_lw_date_x.value, 2);
		document.form.filter_lw_date_x_Wed.checked = getDateCheck(document.form.filter_lw_date_x.value, 3);
		document.form.filter_lw_date_x_Thu.checked = getDateCheck(document.form.filter_lw_date_x.value, 4);
		document.form.filter_lw_date_x_Fri.checked = getDateCheck(document.form.filter_lw_date_x.value, 5);
		document.form.filter_lw_date_x_Sat.checked = getDateCheck(document.form.filter_lw_date_x.value, 6);
		document.form.filter_lw_time_x_starthour.value = getTimeRange(document.form.filter_lw_time_x.value, 0);
		document.form.filter_lw_time_x_startmin.value = getTimeRange(document.form.filter_lw_time_x.value, 1);
		document.form.filter_lw_time_x_endhour.value = getTimeRange(document.form.filter_lw_time_x.value, 2);
		document.form.filter_lw_time_x_endmin.value = getTimeRange(document.form.filter_lw_time_x.value, 3);
		document.form.filter_lw_time2_x_starthour.value = getTimeRange(document.form.filter_lw_time2_x.value, 0);
		document.form.filter_lw_time2_x_startmin.value = getTimeRange(document.form.filter_lw_time2_x.value, 1);
		document.form.filter_lw_time2_x_endhour.value = getTimeRange(document.form.filter_lw_time2_x.value, 2);
		document.form.filter_lw_time2_x_endmin.value = getTimeRange(document.form.filter_lw_time2_x.value, 3);
	}	
	else if(document.form.current_page.value == "Advanced_URLFilter_Content.asp"){
		document.form.url_date_x_Sun.checked = getDateCheck(document.form.url_date_x.value, 0);
		document.form.url_date_x_Mon.checked = getDateCheck(document.form.url_date_x.value, 1);
		document.form.url_date_x_Tue.checked = getDateCheck(document.form.url_date_x.value, 2);
		document.form.url_date_x_Wed.checked = getDateCheck(document.form.url_date_x.value, 3);
		document.form.url_date_x_Thu.checked = getDateCheck(document.form.url_date_x.value, 4);
		document.form.url_date_x_Fri.checked = getDateCheck(document.form.url_date_x.value, 5);
		document.form.url_date_x_Sat.checked = getDateCheck(document.form.url_date_x.value, 6);
		document.form.url_time_x_starthour.value = getTimeRange(document.form.url_time_x.value, 0);
		document.form.url_time_x_startmin.value = getTimeRange(document.form.url_time_x.value, 1);
		document.form.url_time_x_endhour.value = getTimeRange(document.form.url_time_x.value, 2);
		document.form.url_time_x_endmin.value = getTimeRange(document.form.url_time_x.value, 3);
		document.form.url_time_x_starthour_1.value = getTimeRange(document.form.url_time_x_1.value, 0);
		document.form.url_time_x_startmin_1.value = getTimeRange(document.form.url_time_x_1.value, 1);
		document.form.url_time_x_endhour_1.value = getTimeRange(document.form.url_time_x_1.value, 2);
		document.form.url_time_x_endmin_1.value = getTimeRange(document.form.url_time_x_1.value, 3);
	}

	change = 0;
}

function change_firewall(r){
	if (r == "0"){
		//Viz 2012.08.14 move to System page inputRCtrl1(document.form.misc_http_x, 0);
		//Viz 2012.08.14 move to System pageinputCtrl(document.form.misc_httpport_x, 0);	
		inputCtrl(document.form.fw_log_x, 0);
		inputRCtrl1(document.form.misc_ping_x, 0);
	}
	else{		//r=="1"
		//Viz 2012.08.14 move to System page inputRCtrl1(document.form.misc_http_x, 1);
		//Viz 2012.08.14 move to System page inputCtrl(document.form.misc_httpport_x, 1);
		inputCtrl(document.form.fw_log_x, 1);
		inputRCtrl1(document.form.misc_ping_x, 1);
	}
}
function change_wireless_bridge(m, a, r, mflag)
{
	if (m == "0"){
		inputRCtrl2(document.form.wl_wdsapply_x, 1);
		inputRCtrl1(document.form.wl_wdsapply_x, 0);
	}else if (m == "1" && Rawifi_support != -1){	 // N66U-spec
		inputRCtrl2(document.form.wl_wdsapply_x, 0);
		inputRCtrl1(document.form.wl_wdsapply_x, 0);
		if (document.form.wl_channel_orig.value == "0" && document.form.wl_channel.options[0].selected == 1){
				alert("<#JS_fixchannel#>");		
				document.form.wl_channel.options[0].selected = 0;
				document.form.wl_channel.options[1].selected = 1;
		}
	}else{
			inputRCtrl1(document.form.wl_wdsapply_x, 1);
			if (m != 0) {
				if (document.form.wl_channel_orig.value == "0" && document.form.wl_channel.options[0].selected == 1){
					alert("<#JS_fixchannel#>");
					document.form.wl_channel.options[0].selected = 0;
					document.form.wl_channel.options[1].selected = 1;
					document.form.wl_channel.focus();
				}
			}
	}
	return;
	if (a=="0" && r == "0" && mflag != 1)
	{document.form.wl_mode_x.value = "0";
	m = "0";
	}

	if (m == "0") 
	{
	wdsimage = "wds_ap";
	inputRCtrl2(document.form.wl_wdsapply_x, 1);
	inputRCtrl1(document.form.wl_wdsapply_x, 0);
	}
	else if (m == "1") // N66U-spec
	{
	inputRCtrl2(document.form.wl_wdsapply_x, 0);
	inputRCtrl1(document.form.wl_wdsapply_x, 0);
	if (document.form.wl_channel.value == "0")
	{alert("<#JS_fixchannel#>");
	document.form.wl_channel.options[0].selected = 0;
	document.form.wl_channel.options[1].selected = 1;
	}
	}
	else
	{if (a=="0" && r == "0")
	{
	inputRCtrl2(document.form.wl_wdsapply_x, 0);
	}
	inputRCtrl1(document.form.wl_wdsapply_x, 1);
	if (m == "1")
	wdsimage = "wds_wds";
	else
	wdsimage = "wds_mixed";
	if (a == "0")
	{if (r == "0")
	wdsimage += "_connect";
	else
	wdsimage += "_anony";
	}
	else
	{if (r == "0")
	wdsimage += "_connect";
	else
	wdsimage += "_both";
	}
	if (document.form.wl_channel.value == "0")
	{alert("<#JS_fixchannel#>");
	document.form.wl_channel.options[0].selected = 0;
	document.form.wl_channel.options[1].selected = 1;
	}
	}
	wdsimage = "graph/" + wdsimage + ".gif";
}

function onSubmit(){
	change = 0;
	pageChanged = 0;
	pageChangedCount = 0;

	return true;
}

function onSubmitCtrl(o, s) {
	document.form.action_mode.value = s;
	return (onSubmit());
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

function onSubmitApply(s){
	pageChanged = 0;
	pageChangedCount = 0;
	if(document.form.current_page.value == "Advanced_ASUSDDNS_Content.asp"){
		if(s == "hostname_check"){
			showLoading();
			if(!validate_ddns_hostname(document.form.ddns_hostname_x)){
				hideLoading();
				return false;
			}
		}
	}	
	document.form.action_mode.value = "Update";
	document.form.action_script.value = s;
	return true;
}

function automode_hint(){ //Lock add 2009.11.05 for 54Mbps limitation in auto mode + WEP/TKIP.  Jieming modified at 2012.07.05
	if(document.form.wl_nmode_x.value == "0" && 
	  	( 
	  		(document.form.wl_auth_mode_x.value == "open" && document.form.wl_wep_x.value != 0) ||
	    	(document.form.wl_auth_mode_x.value == "shared" && (document.form.wl_wep_x.value == 1 || document.form.wl_wep_x.value == 2) ) || 
	    	(document.form.wl_auth_mode_x.value == "psk" && document.form.wl_crypto.value == "tkip") ||
	    	(document.form.wl_auth_mode_x.value == "wpa" && document.form.wl_crypto.value == "tkip")
	    )
	   ){

		if(document.form.current_page.value == "Advanced_Wireless_Content.asp" || document.form.current_page.value == "device-map/router.asp")
			$("wl_nmode_x_hint").style.display = "block";

		if(psta_support != -1){
			if(!document.form.wl_bw) return false;
			document.form.wl_bw.length = 1;
			document.form.wl_bw[0] = new Option("20 MHz", 1);
			wl_chanspec_list_change();
		}
	}
	else{
		if(psta_support != -1 && document.form.wl_bw){
			genBWTable('<% nvram_get("wl_unit"); %>');
		}
		if(document.form.current_page.value == "Advanced_Wireless_Content.asp" || document.form.current_page.value == "device-map/router.asp")
			$("wl_nmode_x_hint").style.display = "none";
	}	
}

function nmode_limitation(){ //Lock add 2009.11.05 for TKIP limitation in n mode.
	if(document.form.wl_nmode_x.value == "1"){
		if(document.form.wl_auth_mode_x.selectedIndex == 0 && (document.form.wl_wep_x.selectedIndex == "1" || document.form.wl_wep_x.selectedIndex == "2")){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 1){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 2){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 3;
			document.form.wl_wpa_mode.value = 2;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 5){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_auth_mode_x.selectedIndex = 6;
		}
		else if(document.form.wl_auth_mode_x.selectedIndex == 7 && (document.form.wl_crypto.selectedIndex == 0 || document.form.wl_crypto.selectedIndex == 2)){
			alert("<#WLANConfig11n_nmode_limition_hint#>");
			document.form.wl_crypto.selectedIndex = 1;
		}
		wl_auth_mode_change(0);
	}
}

function change_common(o, s, v){
	change = 1;
	pageChanged = 1;
	if(v == "wl_auth_mode_x"){ /* Handle AuthenticationMethod Change */
		wl_auth_mode_change(0);
		if(o.value == "psk" || o.value == "psk2" || o.value == "pskpsk2" || o.value == "wpa" || o.value == "wpawpa2"){
			opts = document.form.wl_auth_mode_x.options;			
			if(opts[opts.selectedIndex].text == "WPA-Personal"){
				document.form.wl_wpa_mode.value="1";
				automode_hint();
			}
			else if(opts[opts.selectedIndex].text == "WPA2-Personal")
				document.form.wl_wpa_mode.value="2";
			else if(opts[opts.selectedIndex].text == "WPA-Auto-Personal")
				document.form.wl_wpa_mode.value="0";
			else if(opts[opts.selectedIndex].text == "WPA-Enterprise")
				document.form.wl_wpa_mode.value="3";
			else if(opts[opts.selectedIndex].text == "WPA-Auto-Enterprise")
				document.form.wl_wpa_mode.value="4";
			
			if(o.value == "psk" || o.value == "psk2" || o.value == "pskpsk2"){
				document.form.wl_wpa_psk.focus();
			}
		}
		else if(o.value == "shared"){ 
					document.form.wl_key.focus();
		}
		nmode_limitation();
		automode_hint();
	}
	else if (v == "wl_crypto"){
		wl_auth_mode_change(0);
		nmode_limitation();
		automode_hint();
	}
	else if(v == "wl_wep_x"){ /* Handle AuthenticationMethod Change */
		change_wlweptype(o, "WLANConfig11b");
		nmode_limitation();
		automode_hint();
	}
	else if(v == "wl_key"){ /* Handle AuthenticationMethod Change */
		var selected_key = eval("document.form.wl_key"+o.value);		
		selected_key.focus();
		selected_key.select();
	}
	else if (s=="WLANConfig11b" && v=="wl_channel" && document.form.current_page.value=="Advanced_WMode_Content.asp"){  // only for AP mode case of WDS page
		if(document.form.wl_mode_x.value != 0 && o.value == 0){
			alert("<#JS_fixchannel#>");
			document.form.wl_channel.options[0].selected = 0;
			document.form.wl_channel.options[1].selected = 1;
			document.form.wl_channel.focus();
		}
	}
	else if(s=="WLANConfig11b" && v == "wl_channel"){
		insertExtChannelOption();
	}
	else if(s=="WLANConfig11b" && v == "wl_bw"){
		insertExtChannelOption();
	}
	else if (s=="WLANConfig11b" && v=="wl_gmode_check"){
		if (document.form.wl_gmode_check.checked == true)
			document.form.wl_gmode_protection.value = "auto";
		else
			document.form.wl_gmode_protection.value = "off";
	}
	else if(s=="WLANConfig11b" && v == "wl_nmode_x"){
		if(o.value=='0' || o.value=='2')
			inputCtrl(document.form.wl_gmode_check, 1);
		else
			inputCtrl(document.form.wl_gmode_check, 0);
		
		if(o.value == "2")
			inputCtrl(document.form.wl_bw, 0);
		else
			inputCtrl(document.form.wl_bw, 1);

		insertExtChannelOption();
		if(o.value == "3"){
			document.form.wl_wme.value = "on";
		}
		nmode_limitation();
		automode_hint();
	}
	else if (v == "wl_mode_x"){
		change_wireless_bridge(o.value, rcheck(document.form.wl_wdsapply_x), 0, 1);
	}	
	else if (v=="ddns_server_x"){
		change_ddns_setting(o.value);
	}
	else if (v == "wl_wme"){
		if(o.value == "off"){
			inputCtrl(document.form.wl_wme_no_ack, 0);
			if($("enable_wl_multicast_forward").style.display != "none")
					inputCtrl(document.form.wl_wmf_bss_enable, 0);
			inputCtrl(document.form.wl_wme_apsd, 0);
		}
		else{
			if(document.form.wl_nmode_x.value == "0" || document.form.wl_nmode_x.value == "1"){	//auto, n only
				document.form.wl_wme_no_ack.value = "off";
				inputCtrl(document.form.wl_wme_no_ack, 0);
			}else		
				inputCtrl(document.form.wl_wme_no_ack, 1);
					
			if(Rawifi_support == -1)
				inputCtrl(document.form.wl_wmf_bss_enable, 1);
			inputCtrl(document.form.wl_wme_apsd, 1);
		}
	}else if (v == "time_zone_select"){
		var tzdst = new RegExp("^[a-z]+[0-9\-\.:]+[a-z]+", "i");
			// match "[std name][offset][dst name]"
		if(o.value.match(tzdst)){
			document.getElementById("chkbox_time_zone_dst").style.display="";	
			document.getElementById("adj_dst").innerHTML = Untranslated.Adj_dst;
			if(!document.getElementById("time_zone_dst_chk").checked){
				document.form.time_zone_dst.value=0;
				document.getElementById("dst_start").style.display="none";
				document.getElementById("dst_end").style.display="none";
			}else{
				document.form.time_zone_dst.value=1;
				document.getElementById("dst_start").style.display="";	
				document.getElementById("dst_end").style.display="";						
			}
		}else{
			document.getElementById("chkbox_time_zone_dst").style.display="none";	
			document.getElementById("time_zone_dst_chk").checked = false;
			document.form.time_zone_dst.value=0;			
			document.getElementById("dst_start").style.display="none";
			document.getElementById("dst_end").style.display="none";
		}		
	}else if (v == "time_zone_dst_chk"){
		if (document.form.time_zone_dst_chk.checked){
			document.form.time_zone_dst.value = "1";
			document.getElementById("dst_start").style.display="";	
			document.getElementById("dst_end").style.display="";					
		}else{
			document.form.time_zone_dst.value = "0";	
			document.getElementById("dst_start").style.display="none";	
			document.getElementById("dst_end").style.display="none";
		}		
	}
	return true;
}

function change_ddns_setting(v){
		var hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
		if (v == "WWW.ASUS.COM"){
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
		}else{
				document.form.ddns_hostname_x.parentNode.style.display = "";
				document.form.DDNSName.parentNode.style.display = "none";
				inputCtrl(document.form.ddns_username_x, 1);
				inputCtrl(document.form.ddns_passwd_x, 1);
				var disable_wild = (v == "WWW.TUNNELBROKER.NET") ? 1 : 0;
				document.form.ddns_wildcard_x[0].disabled= disable_wild;
				document.form.ddns_wildcard_x[1].disabled= disable_wild;
				showhide("link", 1);
		}
}

function change_common_radio(o, s, v, r){
	change = 1;
	pageChanged = 1;
	if (v=='wl_radio'){
		if(sw_mode != "3"){
			if(document.form.wl_radio[0].checked==true){
				if(document.form.wl_radio_date_x_Sun.checked == false
					&& document.form.wl_radio_date_x_Mon.checked == false
					&& document.form.wl_radio_date_x_Tue.checked == false
					&& document.form.wl_radio_date_x_Wed.checked == false
					&& document.form.wl_radio_date_x_Thu.checked == false
					&& document.form.wl_radio_date_x_Fri.checked == false
					&& document.form.wl_radio_date_x_Sat.checked == false){
						document.form.wl_radio_date_x_Sun.focus();
						$('blank_warn').style.display = "";
						return false;
				}
				else{
					$('blank_warn').style.display = "none";
				}
			}else{
				$('blank_warn').style.display = "none";
			}
		}
	}
	else if(v == "ddns_enable_x"){
		var hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
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
			}else{
				document.form.ddns_hostname_x.parentNode.parentNode.parentNode.style.display = "";
				inputCtrl(document.form.ddns_username_x, 1);
				inputCtrl(document.form.ddns_passwd_x, 1);				
			}
			change_ddns_setting(document.form.ddns_server_x.value);			
		}else{
			if(document.form.ddns_server_x.value == "WWW.ASUS.COM"){
				document.form.DDNSName.parentNode.parentNode.parentNode.style.display = "none";
			}else{
				document.form.ddns_hostname_x.parentNode.parentNode.parentNode.style.display = "none";
				inputCtrl(document.form.ddns_username_x, 0);
				inputCtrl(document.form.ddns_passwd_x, 0);			
			}
			inputCtrl(document.form.ddns_server_x, 0);
			document.form.ddns_wildcard_x[0].disabled= 1;
			document.form.ddns_wildcard_x[1].disabled= 1;
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
	else if (v=="fw_enable_x"){
		change_firewall(r);
	}
	
	return true;
}

function valid_WPAPSK(o){
	if(o.value.length >= 64){
		o.value = o.value.substring(0, 63);
		alert("<#JS_wpapass#>");
		return false;
	}
	
	return true;
}

function change_wlweptype(o, s, isload){
	var wflag = 0;

	if(o.value != "0"){
		wflag = 1;
		
		if(document.form.wl_phrase_x.value.length > 0 && isload == 0)
			is_wlphrase("WLANConfig11b", "wl_phrase_x", document.form.wl_phrase_x);
	}

	inputCtrl(document.form.wl_phrase_x, wflag);
	inputCtrl(document.form.wl_key1, wflag);
	inputCtrl(document.form.wl_key2, wflag);
	inputCtrl(document.form.wl_key3, wflag);
	inputCtrl(document.form.wl_key4, wflag);
	inputCtrl(document.form.wl_key, wflag);
	
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

function validate_timerange(o, p)
{
	if (o.value.length==0) 
		o.value = "00";
	else if (o.value.length==1) 
		o.value = "0" + o.value;
		
	if (o.value.charAt(0)<'0' || o.value.charAt(0)>'9') 
		o.value = "00";
	else if (o.value.charAt(1)<'0' || o.value.charAt(1)>'9') 
		o.value = "00";
	else if (p==0 || p==2)
	{
		if(o.value>23){
			alert('<#JS_validrange#> 00 <#JS_validrange_to#> 23');
			o.value = "00";
			o.focus();
			o.select();
			return false;			
		}	
		return true;
	}
	else
	{
		if(o.value>59){
			alert('<#JS_validrange#> 00 <#JS_validrange_to#> 59');
			o.value = "00";
			o.focus();
			o.select();
			return false;			
		}	
		return true;
	}
	return true;
}

function matchSubnet(ip1, ip2, count){
	var c = 0;
	var v1 = 0;
	var v2 = 0;
	for(i=0;i<ip1.length;i++){
		if (ip1.charAt(i) == '.'){
			if (ip2.charAt(i) != '.')
				return false;
				
			c++;
			if (v1!=v2) 
				return false;
				
			v1 = 0;
			v2 = 0;
		}
		else{
			if (ip2.charAt(i)=='.') 
				return false;
				
			v1 = v1*10 + (ip1.charAt(i) - '0');
			v2 = v2*10 + (ip2.charAt(i) - '0');
		}
		if (c==count) 
			return true;
	}
	return false;
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

function updateDateTime(s)
{
	if (s == "Advanced_Firewall_Content.asp")
	{
		document.form.filter_lw_date_x.value = setDateCheck(
		document.form.filter_lw_date_x_Sun,
		document.form.filter_lw_date_x_Mon,
		document.form.filter_lw_date_x_Tue,
		document.form.filter_lw_date_x_Wed,
		document.form.filter_lw_date_x_Thu,
		document.form.filter_lw_date_x_Fri,
		document.form.filter_lw_date_x_Sat);
		document.form.filter_lw_time_x.value = setTimeRange(
		document.form.filter_lw_time_x_starthour,
		document.form.filter_lw_time_x_startmin,
		document.form.filter_lw_time_x_endhour,
		document.form.filter_lw_time_x_endmin);
		document.form.filter_lw_time2_x.value = setTimeRange(
		document.form.filter_lw_time2_x_starthour,
		document.form.filter_lw_time2_x_startmin,
		document.form.filter_lw_time2_x_endhour,
		document.form.filter_lw_time2_x_endmin);
	}
	else if (s == "Advanced_WAdvanced_Content.asp")
	{
			document.form.wl_radio_date_x.value = setDateCheck(
			document.form.wl_radio_date_x_Sun,
			document.form.wl_radio_date_x_Mon,
			document.form.wl_radio_date_x_Tue,
			document.form.wl_radio_date_x_Wed,
			document.form.wl_radio_date_x_Thu,
			document.form.wl_radio_date_x_Fri,
			document.form.wl_radio_date_x_Sat);
			document.form.wl_radio_time_x.value = setTimeRange(
			document.form.wl_radio_time_x_starthour,
			document.form.wl_radio_time_x_startmin,
			document.form.wl_radio_time_x_endhour,
			document.form.wl_radio_time_x_endmin);
			document.form.wl_radio_time2_x.value = setTimeRange(
			document.form.wl_radio_time2_x_starthour,
			document.form.wl_radio_time2_x_startmin,
			document.form.wl_radio_time2_x_endhour,
			document.form.wl_radio_time2_x_endmin);
	}
	else if (s == "Advanced_URLFilter_Content.asp")
	{
		document.form.url_date_x.value = setDateCheck(
		document.form.url_date_x_Sun,
		document.form.url_date_x_Mon,
		document.form.url_date_x_Tue,
		document.form.url_date_x_Wed,
		document.form.url_date_x_Thu,
		document.form.url_date_x_Fri,
		document.form.url_date_x_Sat);
		document.form.url_time_x.value = setTimeRange(
		document.form.url_time_x_starthour,
		document.form.url_time_x_startmin,
		document.form.url_time_x_endhour,
		document.form.url_time_x_endmin);
		document.form.url_time_x_1.value = setTimeRange(
		document.form.url_time_x_starthour_1,
		document.form.url_time_x_startmin_1,
		document.form.url_time_x_endhour_1,
		document.form.url_time_x_endmin_1);
	}		
}

function openLink(s){
	if (s == 'x_DDNSServer'){
		if (document.form.ddns_server_x.value.indexOf("WWW.DYNDNS.ORG")!=-1)
			tourl = "https://account.dyn.com/services/zones/svc/add.html?_add_dns=c&trial=standarddns";
		else if (document.form.ddns_server_x.value == 'WWW.TZO.COM')
			tourl = "https://controlpanel.tzo.com/cgi-bin/tzopanel.exe";
		else if (document.form.ddns_server_x.value == 'WWW.ZONEEDIT.COM')
			tourl = "http://www.zoneedit.com/signUp.html";
		else if (document.form.ddns_server_x.value == 'WWW.DNSOMATIC.COM')
			tourl = "http://dnsomatic.com/create/";
		else if (document.form.ddns_server_x.value == 'WWW.TUNNELBROKER.NET')
			tourl = "http://www.tunnelbroker.net/register.php";
		else if (document.form.ddns_server_x.value == 'WWW.ASUS.COM')
			tourl = "";
		else if (document.form.ddns_server_x.value == 'WWW.NO-IP.COM')
			tourl = "http://www.no-ip.com/newUser.php";
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
	else if(wepType == "2" || wepType == "3"){
		password = "";
		
		if(phrase.length > 0){
			for(var i = 0; i < 64; i++){
				ch = phrase.charAt(i%phrase.length);
				password = password+ch;
			}
		}
		
		password = calcMD5(password);
		if(wepType == "2"){
			wepKey1.value = password.substr(0, 26);
		}
		else{
			wepKey1.value = password.substr(0, 32);
		}
		
		wepKey2.value = wepKey1.value;
		wepKey3.value = wepKey1.value;
		wepKey4.value = wepKey1.value;
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
	
	//if(mode == "shared" || mode == "radius"){ //2009.03 magic
	if(mode == "shared"){ //2009.0310 Lock
		wep_type_array = new Array("WEP-64bits", "WEP-128bits");
		value_array = new Array("1", "2");
	}
	else{
		wep_type_array = new Array("None", "WEP-64bits", "WEP-128bits");
		value_array = new Array("0", "1", "2");
	}

	add_options_x2(document.form.wl_wep_x, wep_type_array, value_array, cur_wep);
	
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2" || mode == "wpa" || mode == "wpa2" || mode == "wpawpa2" || mode == "radius") //2009.03 magic
		document.form.wl_wep_x.value = "0";
	change_wlweptype(document.form.wl_wep_x, "WLANConfig11b", isload);
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
				if(document.form.wl_bw.value != "0" && wl_channel_list_5g[wl_channel_list_5g.length-1] == "165"){
						wl_channel_list_5g.splice(wl_channel_list_5g.length-1,1);
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
                		channels = new Array(0, 149, 153, 157, 161);  // Region 3
                	
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
                		channels = new Array(0, 149, 153, 157, 161); // Region 3
                	
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
                		channels = new Array(36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161); // Region 7
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
                		channels = new Array(0, 149, 153, 157, 161);
                	
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
                		channels = new Array(36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161);
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
				add_a_option(x, "lower", "Auto");
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
			x.options[0].text = "Lower";
			x.options[0].value = "lower";
		}
		else if ((CurrentCh >= 5) && (CurrentCh <= 7)){
			x.options[0].text = "Lower";
			x.options[0].value = "lower";
			add_a_option(document.form.wl_nctrlsb, "upper", "Upper");
			if (document.form.wl_nctrlsb_old.value == "upper")
				document.form.wl_nctrlsb.options.selectedIndex=1;
		}
		else if ((CurrentCh >= 8) && (CurrentCh <= 9)){
			x.options[0].text = "Upper";
			x.options[0].value = "upper";
			if (option_length >=14){
				add_a_option(document.form.wl_nctrlsb, "lower", "Lower");
				if (document.form.wl_nctrlsb_old.value == "lower")
					document.form.wl_nctrlsb.options.selectedIndex=1;
			}
		}
		else if (CurrentCh == 10){
			x.options[0].text = "Upper";
			x.options[0].value = "upper";
			if (option_length > 14){
				add_a_option(document.form.wl_nctrlsb, "lower", "Lower");
				if (document.form.wl_nctrlsb_old.value == "lower")
					document.form.wl_nctrlsb.options.selectedIndex=1;
			}
		}
		else if (CurrentCh >= 11){
			x.options[0].text = "Upper";
			x.options[0].value = "upper";
		}
		else{
			x.options[0].text = "Auto";
			x.options[0].value = "1";
		}
	}
	else
		inputCtrl(document.form.wl_nctrlsb, 0);
}

function wl_auth_mode_change(isload){
	var mode = document.form.wl_auth_mode_x.value;
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
	if(mode == "psk" || mode == "psk2" || mode == "pskpsk2"){
		/* Save current crypto algorithm */
		for(var i = 0; i < document.form.wl_crypto.length; i++){
			if(document.form.wl_crypto[i].selected){
				cur = document.form.wl_crypto[i].value;
				break;
			}
		}
		
			opts = document.form.wl_auth_mode_x.options;
			
			if(opts[opts.selectedIndex].text == "WPA-Personal")
				algos = new Array("TKIP");
			else if(opts[opts.selectedIndex].text == "WPA2-Personal")
				algos = new Array("AES");
			else
				algos = new Array("AES", "TKIP+AES");
				//algos = new Array("TKIP", "AES", "TKIP+AES");
		
		/* Reconstruct algorithm array from new crypto algorithms */
		document.form.wl_crypto.length = algos.length;
		for(i=0; i<algos.length; i++){
			document.form.wl_crypto[i] = new Option(algos[i], algos[i].toLowerCase());
			document.form.wl_crypto[i].value = algos[i].toLowerCase();
			
			if(algos[i].toLowerCase() == cur)
				document.form.wl_crypto[i].selected = true;
		}
	}
	else if(mode == "wpa" || mode == "wpawpa2"){
		for(var i = 0; i < document.form.wl_crypto.length; i++){
			if(document.form.wl_crypto[i].selected){
				cur = document.form.wl_crypto[i].value;
				break;
			}
		}
		
		opts = document.form.wl_auth_mode_x.options;
		if(opts[opts.selectedIndex].text == "WPA-Enterprise")
			algos = new Array("TKIP");
		else
			algos = new Array("AES", "TKIP+AES");
			//algos = new Array("TKIP", "AES", "TKIP+AES");
		
		document.form.wl_crypto.length = algos.length;
		for(i=0; i<algos.length; i++){
			document.form.wl_crypto[i] = new Option(algos[i], algos[i].toLowerCase());
			document.form.wl_crypto[i].value = algos[i].toLowerCase();
			
			if(algos[i].toLowerCase() == cur)
				document.form.wl_crypto[i].selected = true;
		}
	}
	else if(mode == "wpa2"){
		for(var i = 0; i < document.form.wl_crypto.length; i++){
			if(document.form.wl_crypto[i].selected){
				cur = document.form.wl_crypto[i].value;
				break;
			}
		}
		
		algos = new Array("AES");
		
		document.form.wl_crypto.length = algos.length;
		for(i=0; i<algos.length; i++){
			document.form.wl_crypto[i] = new Option(algos[i], algos[i].toLowerCase());
			document.form.wl_crypto[i].value = algos[i].toLowerCase();
			
			if(algos[i].toLowerCase() == cur)
				document.form.wl_crypto[i].selected = true;
		}
	}
	
	change_wep_type(mode, isload);
	
	/* Save current network key index */
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
		if(!isload)
			cur = "1";
	}
	
	/* Reconstruct network key indices array from new network key indices */
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
	if (sh == 1){
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

var pageChanged = 0;
var pageChangedCount = 0;

function valid_IP_form(obj, flag){
	if(obj.value == ""){
			return true;
	}else if(flag==0){	//without netMask
			if(!validate_ipaddr_final(obj, obj.name)){
				obj.focus();
				obj.select();		
				return false;	
			}else
				return true;
	}else if(flag==1){	//with netMask and generate netmask
			var strIP = obj.value;
			var re = new RegExp("^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$", "gi");			
			
			if(!validate_ipaddr_final(obj, obj.name)){
				obj.focus();
				obj.select();		
				return false;	
			}			
			
			if(obj.name=="sr_ipaddr_x_0" && re.test(strIP)){
					if((RegExp.$1 > 0) && (RegExp.$1 < 127)) document.form.sr_netmask_x_0.value = "255.0.0.0";
					else if ((RegExp.$1 > 127) && (RegExp.$1 < 192)) document.form.sr_netmask_x_0.value = "255.255.0.0";
					else if ((RegExp.$1 > 191) && (RegExp.$1 < 224)) document.form.sr_netmask_x_0.value = "255.255.255.0";
					else document.form.sr_netmask_x_0.value = "0.0.0.0";												
			}else if(obj.name=="wan_ipaddr_x" && re.test(strIP)){
					if((RegExp.$1 > 0) && (RegExp.$1 < 127)) document.form.wan_netmask_x.value = "255.0.0.0";
					else if ((RegExp.$1 > 127) && (RegExp.$1 < 192)) document.form.wan_netmask_x.value = "255.255.0.0";
					else if ((RegExp.$1 > 191) && (RegExp.$1 < 224)) document.form.wan_netmask_x.value = "255.255.255.0";
					else document.form.wan_netmask_x.value = "0.0.0.0";												
			}else if(obj.name=="lan_ipaddr" && re.test(strIP)){
					if((RegExp.$1 > 0) && (RegExp.$1 < 127)) document.form.lan_netmask.value = "255.0.0.0";
					else if ((RegExp.$1 > 127) && (RegExp.$1 < 192)) document.form.lan_netmask.value = "255.255.0.0";
					else if ((RegExp.$1 > 191) && (RegExp.$1 < 224)) document.form.lan_netmask.value = "255.255.255.0";
					else document.form.lan_netmask.value = "0.0.0.0";				
			}
			
			return true;
	}else if(flag==2){ 	//ip plus netmask
				
			if(obj.value.search("/") == -1){		// only IP
					if(!validate_ipaddr_final(obj, obj.name)){
							obj.focus();
							obj.select();		
							return false;	
					}else
							return true;
			}else{															// IP plus netmask
					if(obj.value.split("/").length > 2){
							alert(obj.value + " <#JS_validip#>");
							obj.value = "";
							obj.focus();
							obj.select();
							return false;
					}else{
							if(obj.value.split("/")[1] == "" || obj.value.split("/")[1] == 0 || obj.value.split("/")[1] > 30){
									alert(obj.value + " <#JS_validip#>");
									obj.value = "";
									obj.focus();
									obj.select();
									return false;								
							}else{
									var IP_tmp = obj.value;
									obj.value = obj.value.split("/")[0];
									if(!validate_ipaddr_final(obj, obj.name)){
											obj.focus();
											obj.select();		
											return false;	
									}else{
											obj.value = IP_tmp;
											return true;
									}		
							}
					}
			}		
	}else
		return false;
}

function valid_IP_subnet(obj){
	var ipPattern1 = new RegExp("(^([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.(\\*)$)", "gi");
	var ipPattern2 = new RegExp("(^([0-9]{1,3})\\.([0-9]{1,3})\\.(\\*)\\.(\\*)$)", "gi");
	var ipPattern3 = new RegExp("(^([0-9]{1,3})\\.(\\*)\\.(\\*)\\.(\\*)$)", "gi");
	var ipPattern4 = new RegExp("(^(\\*)\\.(\\*)\\.(\\*)\\.(\\*)$)", "gi");
	var parts = obj.value.split(".");
	if(!ipPattern1.test(obj.value) && !ipPattern2.test(obj.value) && !ipPattern3.test(obj.value) && !ipPattern4.test(obj.value)){
			alert(obj.value + " <#JS_validip#>");
    	obj.focus();
    	obj.select();
    	return false;
	
	}else if(parts[0] == 0 || parts[0] > 255 || parts[1] > 255 || parts[2] > 255){			
			alert(obj.value + " <#JS_validip#>");
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

function validate_number_range(obj, mini, maxi){
	var PortRange = obj.value;
	var rangere=new RegExp("^([0-9]{1,5})\:([0-9]{1,5})$", "gi");

	if(rangere.test(PortRange)){
			if(parseInt(RegExp.$1) >= parseInt(RegExp.$2)){
				alert("<#JS_validport#>");	
				obj.focus();
				obj.select();		
				return false;												
			}
			else{
				if(!validate_each_port(obj, RegExp.$1, mini, maxi) || !validate_each_port(obj, RegExp.$2, mini, maxi)){
					obj.focus();
					obj.select();
					return false;											
				}
				return true;								
			}
	}
	else{
		if(!validate_range(obj, mini, maxi)){		
			obj.focus();
			obj.select();
			return false;
		}
		return true;	
	}	
}
	
function validate_each_port(o, num, min, max) {	
	if(num<min || num>max) {
		alert("<#JS_validport#>");
		return false;
	}else {
		//o.value = str2val(o.value);
		if(o.value=="")
			o.value="0";
		return true;
	}
}
/*Viz 2011.03 for Input value check,  end */
