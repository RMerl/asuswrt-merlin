<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0"/>
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<link rel="stylesheet" href="/iui/default.css"  type="text/css"/>
<title>ASUS Router - Mobile QIS</title>                          
<script type="text/javascript" src="/jquery.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<script language="JavaScript" type="text/javascript" src="/iui/registerEvent.js"></script>
<script type="text/javascript">
var x_setting = '<% nvram_get("x_Setting"); %>';	//0: first time
function login(){
	var make_base_auth = function (user, password) {
		var tok = user + ':' + password;
		var hash = btoa(tok);
		return "Basic " + hash;
	}

	$j("#errorHint").html("");

	if($j("#wfps_login").val() == ""){
		$j("#wfps_login").focus();
		$j("#errorHint").html("Username cannot be blank!");
		return false;
	}

	if($j("#wfps_password").val() == ""){
		$j("#wfps_password").focus();
		$j("#errorHint").html("Password cannot be blank!");
		return false;
	}

	$j.ajax({
		type: "POST",
		url: "/AjaxLogin.asp",
		dataType: 'text',
		beforeSend: function (xhr){ 
			xhr.setRequestHeader('Authorization', make_base_auth($j("#wfps_login").val(), $j("#wfps_password").val())); 
		},
		success: function (){
			//location.href = location.protocol + "//" + $("#loginID").val() + ":" + $("#loginPW").val() + "@" + location.host + "/index.asp";
			location.href = location.protocol + "//" + $j("#wfps_login").val() + ":" + $j("#wfps_password").val() + "@" + location.host + "/QIS_wizard_m.htm";	
		},
		error : function (xhr, text, err) {   
			alert("Password incorrect");
			//$j("#errorHint").html(xhr.statusCode().status + " " + xhr.statusCode().statusText);
		}
	});
}

function check_setting(){
	if((document.getElementById('wfps_login').value.length >=1 && document.getElementById('wfps_login').value.length <= 32) && 
		(document.getElementById('wfps_password').value.length >= 1 && document.getElementById('wfps_password').value.length <= 32)){		
			document.getElementById('login_button').disabled = false;
	}
	else{
		document.getElementById('login_button').disabled = true;
	}
}

function initial(){
	if(x_setting == 1){
		document.getElementById("desc1").style.display = ""; 
	}
	else{
		document.getElementById("desc2").style.display = ""; 
		document.getElementById("desc3").style.display = ""; 
	}		
}
</script>
</head>
<body onload="initial();">
	<div class="container">
		<header>
			<a class="brand-logo">T-Mobile</a> 
			<a class="product-logo"></a>
		</header>
		<section>
			<h1>Wi-Fi CellSpot Router</h1>
			<p>Securely manage your Wi-Fi network names (SSIDs) and passwords.</p>
			<div class="hr"></div>
				<strong id="desc1" style="display:none;"><p>Please log in</p></strong>
				<strong id="desc2" style="display:none;"><p>To get started, you first need to log in and change your admin password.</p></strong>
				<p id="desc3" style="display:none;">Please use <strong>"admin"</strong> in the log-in field, and <strong>"password"</strong> as your temporary password.</p>
			<!--form class="wfps_form"-->
				<label for="wfps_login">Log in</label>
				<input type="text" name="login" id="wfps_login" onkeyup="check_setting();">
				<label for="wfps_password">Password</label>
				<a class="wfps_form_action" data-change="wfps_password" href="#">Show</a>
				<input type="password" name="password" id="wfps_password" onkeyup="check_setting();">
				<input type="button" value="Log In" id="login_button" onclick="login();" disabled="disabled">
			<!--/form-->
		</section>
		<footer>
          &copy; 2002-2014 T-Mobile USA, Inc.
		</footer>
	</div>
<span id="errorHint"></span>
</body>
</html>
