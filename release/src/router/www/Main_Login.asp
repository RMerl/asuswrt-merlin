<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<link rel="icon" href="images/favicon.png">
<title>ASUS Login</title>
<style>
body{
	font-family: Arial;
}
.wrapper{
	background:url(images/New_ui/login_bg.png) #283437 no-repeat;
	background-size: 1280px 1076px;
	background-position: center 0%;
	margin: 0px; 
}
.title_name {
	font-size: 30pt;
	color:#93d2d9;
}
.prod_madelName{
	font-size: 20pt;
	color:#fff;
	margin-left:78px;
	margin-top: 10px;
}
.login_img{
	width:43px;
	height:43px;
	background-image: url('images/New_ui/icon_titleName.png');
	background-repeat: no-repeat;
}
.p1{
	font-size: 12pt;
	color:#fff;
	width:480px;
}
.button{
	background-color:#007AFF;
	/*background:rgba(255,255,255,0.1);
	border: solid 1px #6e8385;*/
	border-radius: 4px ;
	transition: visibility 0s linear 0.218s,opacity 0.218s,background-color 0.218s;
	height: 48px;
	width: 200px;
	font-size: 14pt;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	text-align: center;
	float:right; 
	margin:25px 79px 0px 39px;
	line-height:48px;
}
.button:hover{
	cursor:pointer;
}
.form_input{
	background-color:rgba(255,255,255,0.2);
	border-radius: 4px;
	padding:13px 11px;
	width: 380px;
	border: 0;
	height:25px;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	font-size:18px
}
.nologin{
	margin:10px 0px 0px 78px;
	background-color:rgba(255,255,255,0.2);
	padding:20px;
	line-height:18px;
	border-radius: 5px;
	width: 380px;
	border: 0;
	color:#fff;
	color:#000\9; /* IE6 IE7 IE8 */
	font-size:16px;
}
.div_table{
	display:table;
}
.div_tr{
	display:table-row;
}
.div_td{
	display:table-cell;
}
.title_gap{
	margin:20px 0px 0px 78px;
}
.img_gap{
	padding-right:30px;
	vertical-align:middle;
}
.password_gap{
	margin:30px 0px 0px 78px;
}
.error_hint{
	color: rgb(255, 204, 0);
	margin:10px 0px -10px 78px; 
	font-size: 12px;
	font-weight: bolder;
}
.main_field_gap{
	margin:100px auto 0;
}
.warming_desc{
	font-size: 12px;
	color:#FC0;
	width: 600px;
}

/*for mobile device*/
@media screen and (max-width: 1000px){
	.title_name {
		font-size: 20pt;
		color:#93d2d9;
		margin-left:15px;
	}
	.prod_madelName{
		font-size: 13pt;
		margin-left: 15px;
	}
	.p1{
		font-size: 12pt;
		width:100%;
	}
	.login_img{
		background-size: 75%;
	}
	.form_input{	
		padding:13px 11px;
		width: 100%;
		height:25px;
		font-size:16px
	}
	.button{
		height: 50px;
		width: 100%;
		font-size: 14pt;
		text-align: center;
		float:right; 
		margin: 25px -22px 40px 15px;
		line-height:50px;
		padding-left: 7px;
	}
	.nologin{
		margin-left:10px; 
		padding:10px;
		line-height:18px;
		width: 100%;
		font-size:14px;
	}
	.error_hint{
		margin-left:10px; 
	}
	.main_field_gap{
		width:80%;
		margin:30px 0 0 15px;
		/*margin:30px auto 0;*/
	}
	.title_gap{
		margin-left:15px; 
	}
	.password_gap{
		margin-left:15px; 
	}
	.img_gap{
		padding-right:0;
		vertical-align:middle;
	}
	.warming_desc{
		margin: 10px 15px;
		width: 100%; 
	}
}
</style>
<script>
function initial(){
	var flag = '<% get_parameter("error_status"); %>';

	if('<% check_asus_model(); %>' == '0'){
		document.getElementById("warming_field").style.display ="";
		disable_input();
		disable_button();
	}

	if(flag != ""){
		document.getElementById("error_status_field").style.display ="";
		if(flag == 3)
			document.getElementById("error_status_field").innerHTML ="* Invalid username or password";
		else if(flag == 7){
			document.getElementById("error_status_field").innerHTML ="* Detect abnormal logins many times, please try again after 1 minute.";
			document.form.login_username.disabled = true;
			document.form.login_passwd.disabled = true;
		}else if(flag == 8){
			document.getElementById("login_filed").style.display ="none";
			document.getElementById("logout_field").style.display ="";
		}else if(flag == 9){
			<% login_state_hook(); %> 

			var loginUserIp = (function(){
				return (typeof login_ip_str === "function") ? login_ip_str().replace("0.0.0.0", "") : "";
			})();

			var getLoginUser = function(){
				if(loginUserIp === "") return "";

				var dhcpLeaseInfo = <% IP_dhcpLeaseInfo(); %>
				var hostName = "";

				dhcpLeaseInfo.forEach(function(elem){
				if(elem[0] === loginUserIp){
					hostName = " (" + elem[1] + ")";
					return false;
					}
				})
				return "<div style='margin-top:15px;word-wrap:break-word;word-break:break-all'>* <#login_hint1#> " + loginUserIp + hostName + "</div>";
			};

			document.getElementById("logined_ip_str").innerHTML = getLoginUser();

			document.getElementById("login_filed").style.display ="none";
			document.getElementById("nologin_field").style.display ="";
		}else
			document.getElementById("error_status_field").style.display ="none";
	}

	document.form.login_username.focus();

	/*register keyboard event*/
	document.form.login_username.onkeyup = function(e){
		e=e||event;
		if(e.keyCode == 13){
			document.form.login_passwd.focus();
			return false;
		}
	};
	document.form.login_username.onkeypress = function(e){
		e=e||event;
		if(e.keyCode == 13){
			return false;		}
	};

	document.form.login_passwd.onkeyup = function(e){
		e=e||event;
		if(e.keyCode == 13){
			login();
			return false;
		}
	};
	document.form.login_passwd.onkeypress = function(e){
		e=e||event;
		if(e.keyCode == 13){
			return false;
		}
	};

	if(history.pushState != undefined) history.pushState("", document.title, window.location.pathname);
}

function login(){
	var redirect_page = '<% get_parameter("page"); %>';

	var trim = function(val){
		val = val+'';
		for (var startIndex=0;startIndex<val.length && val.substring(startIndex,startIndex+1) == ' ';startIndex++);
		for (var endIndex=val.length-1; endIndex>startIndex && val.substring(endIndex,endIndex+1) == ' ';endIndex--);
		return val.substring(startIndex,endIndex+1);
	}

	if(!window.btoa){
		window.btoa = function(input){
			var keyStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
			var output = "";
			var chr1, chr2, chr3, enc1, enc2, enc3, enc4;
			var i = 0;
			var utf8_encode = function(string) {
				string = string.replace(/\r\n/g,"\n");
				var utftext = "";
				for (var n = 0; n < string.length; n++) {
					var c = string.charCodeAt(n);
					if (c < 128) {
						utftext += String.fromCharCode(c);
					}
					else if((c > 127) && (c < 2048)) {
						utftext += String.fromCharCode((c >> 6) | 192);
						utftext += String.fromCharCode((c & 63) | 128);
					}
					else {
						utftext += String.fromCharCode((c >> 12) | 224);
						utftext += String.fromCharCode(((c >> 6) & 63) | 128);
						utftext += String.fromCharCode((c & 63) | 128);
					}
				}
				return utftext;
			};
			input = utf8_encode(input);
			while (i < input.length) {
				chr1 = input.charCodeAt(i++);
				chr2 = input.charCodeAt(i++);
				chr3 = input.charCodeAt(i++);
				enc1 = chr1 >> 2;
				enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
				enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
				enc4 = chr3 & 63;
				if (isNaN(chr2)) {
					enc3 = enc4 = 64;
				}
				else if (isNaN(chr3)) {
					enc4 = 64;
				}
				output = output + 
				keyStr.charAt(enc1) + keyStr.charAt(enc2) + 
				keyStr.charAt(enc3) + keyStr.charAt(enc4);
			}
			return output;
		};
	}

	document.form.login_username.value = trim(document.form.login_username.value);
	document.form.login_authorization.value = btoa(document.form.login_username.value + ':' + document.form.login_passwd.value);
	document.form.login_username.disabled = true;
	document.form.login_passwd.disabled = true;
	if(redirect_page == "" || redirect_page == "Logout.asp" || redirect_page == "Main_Login.asp")
		document.form.next_page.value = "index.asp";
	else
		document.form.next_page.value = redirect_page;
	document.form.submit();
}

function disable_input(){
	var disable_input_x = document.getElementsByClassName('form_input');
	for(i=0;i<disable_input_x.length;i++)
		disable_input_x[i].disabled = true;
}

function disable_button(){
	document.getElementsByClassName('button')[0].style.display = "none";
}
</script>
</head>
<body class="wrapper" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="login.cgi" target="hidden_frame">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="Main_Login.asp">
<input type="hidden" name="next_page" value="Main_Login.asp">
<input type="hidden" name="login_authorization" value="">
<div class="div_table main_field_gap">
	<div class="div_tr">
		<div id="warming_field" style="display:none;" class="warming_desc">Note: the router you are using is not an ASUS device or has not been authorised by ASUS. ASUSWRT might not work properly on this device.</div>
		<div class="title_name">
			<div class="div_td img_gap">
				<div class="login_img"></div>
			</div>
			<div class="div_td">SIGN IN</div>
		</div>	
		<div class="prod_madelName"><#Web_Title2#></div>

		<!-- Login field -->
		<div id="login_filed">
			<div class="p1 title_gap"><#Sign_in_title#></div>
			<div class="title_gap">
				<input type="text" id="login_username" name="login_username" tabindex="1" class="form_input" maxlength="20" autocapitalization="off" autocomplete="off" placeholder="<#HSDPAConfig_Username_itemname#>">
			</div>
			<div class="password_gap">
				<input type="password" name="login_passwd" tabindex="2" class="form_input" maxlength="16" placeholder="<#HSDPAConfig_Password_itemname#>" autocapitalization="off" autocomplete="off">
			</div>
			<div class="error_hint" style="display:none;" id="error_status_field"></div>
				<div class="button" onclick="login();"><#CTL_signin#></div>
		</div>
		
		<!-- No Login field -->
		<div id="nologin_field" style="display:none;">
			<div class="p1 title_gap"></div>
			<div class="nologin">
				<#login_hint2#>
				<div id="logined_ip_str"></div>
			</div>
		</div>

		<!-- Logout field -->
		<div id="logout_field" style="display:none;">
			<div class="p1 title_gap"></div>
			<div class="nologin"><#logoutmessage#>
				<br><br>Click <a style="color: #FFFFFF; font-weight: bolder;text-decoration:underline;" class="hyperlink" href="Main_Login.asp">here</a> to log back in.
			</div>
		</div>
	</div>
</div>
</form>
</body>
</html>
