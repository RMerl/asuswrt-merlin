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
		type: 'GET',
		url: "/Logout.asp",
		username: $("#loginID").val(),
		password: $("#loginPW").val(),
		dataType: 'script',
		statusCode: {
			404: function() {
				$("#errorHint").html("page not found!");
			},
			200: function() {
				alert( "Login OK!" );
				location.href = location.protocol + "//" + $("#loginID").val() + ":" + $("#loginPW").val() + "@" + location.host + "/index.asp";
			},
			401: function() {
				$("#errorHint").html("Login Fail!");
			}
		}
	});
}
</script>
</head>
<body onload="">
<input type="text" id="loginID">
<input type="text" id="loginPW">
<input type="button" value="Login" onclick="login();">
<span id="errorHint"></span>
</body>
</html>
