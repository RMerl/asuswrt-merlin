<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<script type="text/javascript" src="/disk_msg.js"></script>
<script>
function create_account_error(error_msg){
	parent.alert_error_msg(error_msg);
}

function create_account_success(){
	// For the different use of AiDisk wizard and Advanced AiDisk pages,
	// define the "resultOfCreateAccount()" in every pages.
	parent.resultOfCreateAccount();
}
</script>
</head>

<body>
<% create_account(); %>
</body>
</html>
