<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Router - Mobile QIS</title>                          
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript">
function login(){
	var make_base_auth = function (user, password) {
	  var tok = user + ':' + password;
	  var hash = btoa(tok);
	  return "Basic " + hash;
	}

	$("#errorHint").html("");

	if($("#loginID").val() == ""){
		$("#loginID").focus();
		$("#errorHint").html("Username cannot be blank!");
		return false;
	}

	if($("#loginPW").val() == ""){
		$("#loginPW").focus();
		$("#errorHint").html("Password cannot be blank!");
		return false;
	}

	$.ajax({
		type: "GET",
		url: "/AjaxLogin.asp",
		dataType: 'text',
		beforeSend: function (xhr){ 
			xhr.setRequestHeader('Authorization', make_base_auth($("#loginID").val(), $("#loginPW").val())); 
		},
		success: function (){
			$("#errorHint").html("Thanks for your comment!");
		},
		error : function (xhr, text, err) {            
			$("#errorHint").html(xhr.statusCode().status + " " + xhr.statusCode().statusText);
		}
	});
}
</script>
</head>
<body onload="">
<input type="text" id="loginID">
<input type="password" id="loginPW">
<input type="button" value="Login" onclick="login();">
<span id="errorHint"></span>
</body>
</html>
