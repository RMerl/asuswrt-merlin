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
	height:526px;
	background:rgba(40,52,55,0.1);
}
.wrapper{
	background:url(images/New_ui/login_bg.png) #283437 no-repeat;
	background-size: 1280px 1076px;
	background-position: center 0%;
	margin: 0px; 
	background:#283437\9;
}
.title_name {
	font-family:Arial;
	font-size: 40pt;
	color:#93d2d9;
}
.prod_madelName{
	font-family: Arial;
	font-size: 26pt;
	color:#fff;
}
.p1{
	font-family: Arial;
	font-size: 16pt;
	color:#fff;
}
.button{
	background-color: #279FD9;
	border: 0px;
	border-radius: 4px ;
	transition: visibility 0s linear 0.218s,opacity 0.218s,background-color 0.218s;
	height: 68px;
	width: 300px;
	font-family: Arial;
	font-size: 28pt;
	color:#fff;
	text-align:center;
	vertical-align:center
}
.button_text{
	font-family: Arial;
	font-size: 28pt;
	color:#fff;
	text-align:center;
	vertical-align:center
}
.form_input{
	background-color:rgba(255,255,255,0.2);
	background-color:#576D73\9;
	border-radius: 4px;
	padding:26px 22px;
	width: 480px;
	border: 0;
	height:25px;
	color:#fff;
	font-size:28px
}
.form_input_text{
	font-family: Arial;
	font-size: 28pt;
	color:#a9a9a9;
}
.p2{
	font-family: Arial;
	font-size: 18pt;
	color:#28fff7;
}
</style>
<script>
/* String splice function */
String.prototype.splice = function( idx, rem, s ) {
    return (this.slice(0,idx) + s + this.slice(idx + Math.abs(rem)));
};

/* String repeat function */
String.prototype.repeat = function(times) {
   return (new Array(times + 1)).join(this);
};

String.prototype.strReverse = function() {
	var newstring = "";
	for (var s=0; s < this.length; s++) {
		newstring = this.charAt(s) + newstring;
	}
	return newstring;
	//strOrig = ' texttotrim ';
	//strReversed = strOrig.revstring();
};
	
var is_KR_sku = (function(){
	var ttc = '<% nvram_get("territory_code"); %>';
	return (ttc.search("KR") == -1) ? false : true;
})();
var isIE8 = navigator.userAgent.search("MSIE 8") > -1; 
var isIE9 = navigator.userAgent.search("MSIE 9") > -1; 

function initial(){
	if(is_KR_sku)
		document.getElementById("KRHint").style.display = "";

	if(isIE8 || isIE9){
		document.getElementById("router_name_tr").style.display = "";
		document.getElementById("router_password_tr").style.display = "";
		document.getElementById("router_password_confirm_tr").style.display = "";
	}

	var windowHeight = (function(){
		if(window.innerHeight)
			return window.innerHeight;
		else if(document.body && document.body.clientHeight)
			return document.body.clientHeight;
		else if(document.documentElement && document.documentElement.clientHeight)
			return document.documentElement.clientHeight;
		else
			return 800;
	})();

	document.getElementById("loginTable").style.height = windowHeight + "px";
	document.getElementById("loginTable").style.display = "";
	document.form.http_username_x.focus();

	if(navigator.userAgent.search("MSIE 8") === -1){
		document.form.http_username_x.onkeyup = function(e){
			if(e.keyCode == 13){
				document.form.http_passwd_x.focus();
			}
		};

		document.form.http_passwd_x.onkeyup = function(e){
			if(e.keyCode == 13){
				document.form.http_passwd_2_x.focus();
			}
		};

		document.form.http_passwd_2_x.onkeyup = function(e){
			if(e.keyCode == 13){
				submitForm();
			}
		};
	}
}

// ---------- Viz add common string check for password 2015.09 start--------
function check_common_string(pwd, flag){
	//Sequential
	var termAlphas = "abcdefghijklmnopqrstuvwxyz";
	var termNumerics = "01234567890";
	var termSymbols = "~!@#$%^&*()_+";
	var termKeyboards1 = "qwertyuiop";
	var termKeyboards2 = "asdfghjkl";
	var termKeyboards3 = "zxcvbnm";
	var termCommon5 = ["123123","abc123","letmein","master","qazwsx","admin"];
	var termCommon8 = ["adminpassword","loginpassword","passw0rd","password","useradmin","userpassword"];
	var nSeqString = 0;
	if(flag == "httpd_password"){	//at lease length 5		
		if(termAlphas.toLowerCase().indexOf(pwd) != -1 || termAlphas.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termNumerics.toLowerCase().indexOf(pwd) != -1 || termNumerics.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termSymbols.toLowerCase().indexOf(pwd) != -1 || termSymbols.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termKeyboards1.toLowerCase().indexOf(pwd) != -1 || termKeyboards1.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termKeyboards2.toLowerCase().indexOf(pwd) != -1 || termKeyboards2.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termKeyboards3.toLowerCase().indexOf(pwd) != -1 || termKeyboards3.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		for(var s=0;s<termCommon5.length;s++){
			if(pwd == termCommon5[s])	{ nSeqString++; }	
		}
		for(var t=0;t<termCommon8.length;t++){
			if(pwd == termCommon8[t])	{ nSeqString++; }	
		}		
	}
	else if(flag == "wpa_key"){	//at lease length 8
		if(termAlphas.toLowerCase().indexOf(pwd) != -1 || termAlphas.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termNumerics.toLowerCase().indexOf(pwd) != -1 || termNumerics.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termSymbols.toLowerCase().indexOf(pwd) != -1 || termSymbols.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termKeyboards1.toLowerCase().indexOf(pwd) != -1 || termKeyboards1.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		if(termKeyboards2.toLowerCase().indexOf(pwd) != -1 || termKeyboards2.strReverse().toLowerCase().indexOf(pwd) != -1) { nSeqString++; }
		for(var s=0;s<termCommon8.length;s++){
			if(pwd == termCommon8[s])	{ nSeqString++; }	
		}		
	}
	
	//pure repeat character string
	if(pwd == pwd.charAt(0).repeat(pwd.length)) { nSeqString++; }
	
	if(nSeqString > 0)
		return true;
	else		
		return false;
}
// ---------- Viz add common string check for password 2015.09 end--------

function validForm(){
	if(!validator.chkLoginId(document.form.http_username_x)){
		return false;
	}
	
	if(document.form.http_passwd_x.value == ""){
			showError("<#File_Pop_content_alert_desc6#>");
			document.form.http_passwd_x.value = "";
			document.form.http_passwd_x.focus();
			document.form.http_passwd_x.select();
			return false;
		}
		
	if(document.form.http_passwd_x.value != document.form.http_passwd_2_x.value){
			showError("<#File_Pop_content_alert_desc7#>");
			document.form.http_passwd_x.value = "";
			document.form.http_passwd_x.focus();
			document.form.http_passwd_x.select();
			return false;                   
	}

	if(is_KR_sku){		/* MODELDEP by Territory Code */
		if(!validator.chkLoginPw_KR(document.form.http_passwd_x)){
			return false;
		}
	}
	else{
		if(!validator.chkLoginPw(document.form.http_passwd_x)){
			return false;
		}
	}
	
	if(document.form.http_passwd_x.value == '<% nvram_default_get("http_passwd"); %>'){
			showError("<#QIS_adminpass_confirm0#>");
			document.form.http_passwd_x.value = "";
			document.form.http_passwd_x.focus();
			document.form.http_passwd_x.select();
			return false;
	}
	
	//confirm common string combination	#JS_common_passwd#
	var is_common_string = check_common_string(document.form.http_passwd_x.value, "httpd_password");
	if(document.form.http_passwd_x.value.length > 0 && is_common_string){
		if(!confirm("<#JS_common_passwd#>")){
			document.form.http_passwd_x.focus();
			document.form.http_passwd_x.select();
			return false;	
		}	
	}

	return true;	
}

function submitForm(){
	if(validForm()){
		document.getElementById("error_status_field").style.display = "none";
		document.form.http_username.value = document.form.http_username_x.value;
		document.form.http_passwd.value = document.form.http_passwd_x.value;
		document.form.http_username_x.disabled = true;
		document.form.http_passwd_x.disabled = true;
		document.form.http_passwd_2_x.disabled = true;
		document.form.btn_modify.style.display = "none";
		document.getElementById('loadingIcon').style.display = '';
		document.form.submit();

		var nextPage = decodeURIComponent('<% get_ascii_parameter("nextPage"); %>');
		setTimeout(function(){
			location.href = (nextPage != "") ? nextPage : "index.asp";
		}, 3000);
	}
	else
		return;
}	


var validator = {
	chkLoginId: function(obj){
		var re = new RegExp("^[a-zA-Z0-9][a-zA-Z0-9\-\_]+$","gi");

		if(obj.value == ""){
			showError("<#File_Pop_content_alert_desc1#>");
			obj.value = "";
			obj.focus();
			obj.select();
			return false;
		}
		else if(re.test(obj.value)){
			if(obj.value == "root" || obj.value == "guest" || obj.value == "anonymous"){
				showError("<#USB_Application_account_alert#>");
				obj.value = "";
				obj.focus();
				obj.select();
				return false;
			}

			return true;
		}
		else{
			showError("<#JS_validhostname#>");
			obj.value = "";
			obj.focus();
			obj.select();
			return false;
		}
	},

	chkLoginPw: function(obj){
		
		if(obj.value.length > 0 && obj.value.length < 5){
			showError("<#JS_short_password#>");
			obj.value = "";
			obj.focus();
			obj.select();
			return false;
		}		

		if(obj.value.charAt(0) == '"'){
			showError('<#JS_validstr1#> ["]');
			obj.value = "";
			obj.focus();
			obj.select();
			return false;
		}
		else{
			var invalid_char = ""; 
			for(var i = 0; i < obj.value.length; ++i){
				if(obj.value.charAt(i) < ' ' || obj.value.charAt(i) > '~'){
					invalid_char = invalid_char+obj.value.charAt(i);
				}
			}

			if(invalid_char != ""){
				showError("<#JS_validstr2#> '"+invalid_char+"' !");
				obj.value = "";
				obj.focus();
				obj.select();
				return false;
			}
		}		

		return true;
	},
	
	chkLoginPw_KR: function(obj){		//Alphabets, numbers, specialcharacters mixed
		var string_length = obj.value.length;		
		
		if(!/[A-Za-z]/.test(obj.value) || !/[0-9]/.test(obj.value) || string_length < 8
				|| !/[\!\"\#\$\%\&\'\(\)\*\+\,\-\.\/\:\;\<\=\>\?\@\[\\\]\^\_\`\{\|\}\~]/.test(obj.value)){
				
				showError("<#JS_validPWD#>");
				obj.value = "";
				obj.focus();
				obj.select();
				return false;	
		}	
		
		var invalid_char = "";
		for(var i = 0; i < obj.value.length; ++i){
			if(obj.value.charAt(i) <= ' ' || obj.value.charAt(i) > '~'){
				invalid_char = invalid_char+obj.value.charAt(i);
			}
		}

		if(invalid_char != ""){
			showError("<#JS_validstr2#> '"+invalid_char+"' !");
			obj.value = "";
			obj.focus();
			obj.select();
			return false;
		}
		
		return true;
	}
}

function showError(str){
	document.getElementById("error_status_field").style.display = "";
	document.getElementById("error_status_field").innerHTML = str;
}
</script>
</head>
<body class="wrapper" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="saveNvram">
<input type="hidden" name="action_wait" value="0">
<input type="hidden" name="current_page" value="Main_Login.asp">
<input type="hidden" name="next_page" value="index.asp">
<input type="hidden" name="flag" value="">
<input type="hidden" name="login_authorization" value="">
<input name="foilautofill" style="display: none;" type="password">
<input type="hidden" name="http_username" value="">
<input type="hidden" name="http_passwd" value="">
<table id="loginTable" align="center" cellpadding="0" cellspacing="0" style="display:none">
	<tr>
		<td>
			<div>
				<table class="content">
					<tr style="height:43px;">
						<td style="width:73px" align="left">
							<div><img src="/images/New_ui/icon_titleName.png"></div>
						</td>
						<td align="left">
							<div class="title_name"><#PASS_changepasswd#></div>
						</td>
					</tr>
					<tr>
						<td colspan="2">
							<div class="p1" style="margin:35px 0px 0px 78px;">
								<div style="margin-bottom:10px;">
									<#Web_Title2#> is currently not protected and uses an unsafe default username and password.
								</div>
								<div style="margin-bottom:10px;">
									<#QIS_pass_desc1#>
								</div>
								<div id="KRHint" style="margin-bottom:10px;display:none">
									<#JS_validPWD#>
								</div>
							</div>
						</td>
					</tr>
					<tr id="router_name_tr" style="display:none">
						<td colspan="2">
							<div style="color:#FFF;margin:20px 0px -10px 78px;"><#Router_Login_Name#></div>
						</td>
					</tr>				
					<tr style="height:72px;">
						<td colspan="2">
							<div style="margin:20px 0px 0px 78px;">
								<input type="text" name="http_username_x" tabindex="1" class="form_input" maxlength="20" value="" autocapitalize="off" autocomplete="off" placeholder="<#Router_Login_Name#>">
							</div>
						</td>
					</tr>
					<tr id="router_password_tr" style="display:none">
						<td colspan="2">
							<div style="color:#FFF;margin:20px 0px -20px 78px;"><#PASS_new#></div>
						</td>
					</tr>					
					<tr style="height:72px;">
						<td colspan="2">
							<div style="margin:30px 0px 0px 78px;">
								<input type="password" autocapitalize="off" autocomplete="off" value="" name="http_passwd_x" tabindex="2" class="form_input" maxlength="16" onkeyup="" onpaste="return false;"/ onBlur="" placeholder="<#PASS_new#>">
							</div>
						</td>
					</tr>
					<tr id="router_password_confirm_tr" style="display:none">
						<td colspan="2">
							<div style="color:#FFF;margin:20px 0px -20px 78px;"><#Confirmpassword#></div>
						</td>
					</tr>							
					<tr style="height:72px;">
						<td colspan="2">
							<div style="margin:30px 0px 0px 78px;">
								<input type="password" autocapitalize="off" autocomplete="off" value="" name="http_passwd_2_x" tabindex="3" class="form_input" maxlength="16" onkeyup="" onpaste="return false;"/ onBlur="" placeholder="<#Confirmpassword#>">
							</div>
						</td>
					</tr>
					<tr style="heigh:72px">
						<td colspan="2">
							<div style="color: rgb(255, 204, 0); margin:10px 0px -10px 78px; display:none;" id="error_status_field"></div>
						</td>
					</tr>
					<tr align="right" style="height:68px;">
						<td colspan="2">
							<div style="text-align: center;float:right; margin:50px 0px 0px 78px;">
								<input name="btn_modify" type="button" class="button" tabindex="4" onclick="submitForm();" value="<#CTL_modify#>">								
							</div>
							<div id="loadingIcon" style="display:none; margin:50px 0px 0px 0px;">
								<img style="width:35px;height:35px;" src="/images/InternetScan.gif">
							</div>
						</td>
					</tr>
				</table>
			</div>
		</td>
	</tr>
</table>
</form>
</body>
</html>

