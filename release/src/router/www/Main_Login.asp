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
<title>ASUS Login</title>
<style>
.content{
width:580px;
height:426px;
margin: 20px auto 40px auto;
background:rgba(40,52,55,0.1);
}
.wrapper{
background:url(images/New_ui/login_bg.png) #283437 no-repeat;
background-size: 1280px 1076px;
background-position: center 0%;
width:99%;
height:100%;
}

.title_name {
font-family:Arial;
font-size: 30pt;
color:#93d2d9;
}
.prod_madelName{
font-family: Arial;
font-size: 20pt;
color:#fff;
}
.p1{
font-family: Arial;
font-size: 12pt;
color:#fff;
}

.button{
background:rgba(255,255,255,0.1);
border: solid 1px #6e8385;
border-radius: 4px ;
transition: visibility 0s linear 0.218s,opacity 0.218s,background-color 0.218s;
height: 48px;
width: 253px;
font-family: Arial;
font-size: 18pt;
color:#fff;
color:#000\9; /* IE6 IE7 IE8 */
text-align:center;
Vertical-align:center
}

.button_text{
font-family: Arial;
font-size: 18pt;
color:#fff;
text-align:center;
Vertical-align:center
}

.form_input{
background-color:rgba(255,255,255,0.2);
border-radius: 4px;
padding:13px 11px;
width: 480px;
border: 0;
height:25px;
color:#fff;
color:#000\9; /* IE6 IE7 IE8 */
font-size:22px
}

.form_input_text{
font-family: Arial;
font-size: 22pt;
color:#a9a9a9;
}

.p2{
font-family: Arial;
font-size: 12pt;
color:#28fff7;
}
</style>
<script>
var flag = '<% get_parameter("error_status"); %>';

function initial(){
	document.form.login_username.focus();
	if(flag != ""){
		document.getElementById("error_status_field").style.display ="";
		if(flag == 3)
			document.getElementById("error_status_field").innerHTML ="* Invalid username or password";
		else if(flag == 7){
			document.getElementById("error_status_field").innerHTML ="* Detect abnormal logins many times, please try again after 1 minute.";
			document.form.login_username.disabled = true;
			document.form.login_passwd.disabled = true;
		}else
			document.getElementById("error_status_field").style.display ="none";
	}
	history.pushState("", document.title, window.location.pathname);
}

function trim(val){
	val = val+'';
	for (var startIndex=0;startIndex<val.length && val.substring(startIndex,startIndex+1) == ' ';startIndex++);
	for (var endIndex=val.length-1; endIndex>startIndex && val.substring(endIndex,endIndex+1) == ' ';endIndex--);
	return val.substring(startIndex,endIndex+1);
}

function login(){
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
	document.form.foilautofill.disabled = true;
	document.form.submit();
}
</script>
</head>
<body class="wrapper" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/login.cgi" target="hidden_frame" onsubmit="return login();">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="current_page" value="Main_Login.asp">
<input type="hidden" name="next_page" value="Main_Login.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="login_authorization" value="">
<input name="foilautofill" style="display: none;" type="password">
<table align="center" cellpadding="0" cellspacing="0">
	<tr height="35px"></tr>
	<tr>
		<td>
			<div>
				<table class="content">
					<tr style="height:23px;">
						<td style="width:73px" align="left">
							<div><img src="/images/New_ui/icon_titleName.png"></div>
						</td>
						<td align="left">
							<div class="title_name">SIGN IN</div>
						</td>
					</tr>
					<tr>
						<td colspan="2"><div class="prod_madelName" style="margin-left:78px;"><#Web_Title2#></div></td>
					</tr>
					<tr>
						<td colspan="2"><div class="p1" style="margin:25px 0px 0px 78px;">Sign In with Your ASUS Router Account</div></td>
					</tr>					
					<tr style="height:42px;">
						<td colspan="2">
							<div style="margin:20px 0px 0px 78px;">
								<input type="text" id="login_username" name="login_username" tabindex="1" class="form_input" maxlength="20" value="" autocapitalization="off" autocomplete="off" placeholder="Username">
							</div>
						</td>
					</tr>
					<tr style="height:42px;">
						<td colspan="2">
							<div style="margin:20px 0px 0px 78px;">
								<input type="password" autocapitalization="off" autocomplete="off" value="" name="login_passwd" tabindex="2" class="form_input" maxlength="16" onkeyup="" onpaste="return false;"/ onBlur="" placeholder="Password">
							</div>
						</td>
					</tr>
					<tr style="heigh:60px">
						<td colspan="2">
							<div style="color: rgb(255, 204, 0); margin:10px 0px -30px 78px; display:none;" id="error_status_field" class="formfontdesc"></div>
						</td>
					</tr>
					<tr align="right" style="height:38px;">
						<td colspan="2">
							<div style="text-align: center;float:right; margin:30px 0px 0px 78px;">
								<input type="submit" class="button" onclick="login();" value="Sign In">
							</div>	
						</td>
					</tr>
				</table>
			</div>
		</td>
	</tr>
	<tr></tr>
</table>
</form>
</body>
</html>

