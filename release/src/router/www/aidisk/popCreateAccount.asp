<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Add New Account</title>
<link rel="stylesheet" href="../form_style.css"  type="text/css">

<script type="text/javascript" src="../state.js"></script>
<script type="text/javascript">
function clickevent(){
	$("Submit").onclick = function(){
			if(validForm()){
				/*alert('action = '+document.createAccountForm.action+'\n'+
					  'account = '+$("account").value+'\n'+
					  'password = '+$("password").value
					  );//*/
				
				
				parent.showLoading();
				document.createAccountForm.submit();
				parent.hidePop("apply");
			}
		};
}

function validForm(){
	$("account").value = trim($("account").value);
	tempPasswd = trim($("password").value);
	$("confirm_password").value = trim($("confirm_password").value);
	
	// account name
	if($("account").value.length == 0){
		alert("<#File_Pop_content_alert_desc1#>");
		$("account").focus();
		return false;
	}
	
	if($("account").value == "root" || $("account").value == "guest"){
		//alert("User's account can not be 'root' or 'guest'! Please enter a valid account.");
		alert("<#USB_Application_account_alert#>");
		$("account").focus();
		return false;
	}
	
	if(trim($("account").value).length <= 1){
		alert("<#File_Pop_content_alert_desc2#>");
		$("account").focus();
		return false;
	}
	
	if(trim($("account").value).length > 20){
		alert("<#File_Pop_content_alert_desc3#>");
		$("account").focus();
		return false;
	}
	
	var re = new RegExp("[^a-zA-Z0-9-]+","gi");
	if(re.test($("account").value)){
		alert("<#File_Pop_content_alert_desc4#>");
		$("account").focus();
		return false;
	}
	
	if(checkDuplicateName($("account").value, parent.get_accounts())){
		alert("<#File_Pop_content_alert_desc5#>");
		$("account").focus();
		return false;
	}
	
	// password
	if(trim(tempPasswd).length <= 0 || trim($("confirm_password").value).length == 0){
		alert("<#File_Pop_content_alert_desc6#>");
		$("password").focus();
		return false;
	}
	
	if(tempPasswd != $("confirm_password").value){
		alert("<#File_Pop_content_alert_desc7#>");
		$("confirm_password").focus();
		return false;
	}
	
	if(tempPasswd.length != $("password").value.length){
		alert("<#File_Pop_content_alert_desc8#>");
		$("password").focus();
		return false;
	}
	
	var re = new RegExp("[^a-zA-Z0-9]+","gi");
	if(re.test($("password").value)){
		alert("<#File_Pop_content_alert_desc9#>");
		$("password").focus();
		return false;
	}
	
	return true;
}
</script>
</head>

<body onLoad="clickevent();">
<form method="post" name="createAccountForm" action="create_account.asp" target="hidden_frame">
  <table width="90%" class="popTable" border="0" align="center" cellpadding="0" cellspacing="0">
   <thead>
    <tr>
      <td colspan="2"><span style="color:#FFF"><#AddAccountTitle#></span><img src="../images/button-close.gif" onClick="parent.hidePop('OverlayMask');"></td>
      </tr>
	</thead>
	<tbody>
    <tr align="center">
      <td height="25" colspan="2"><#AddAccountAlert#></td>
    </tr>
    <tr>
      <th><#AiDisk_Account#>: </th>
      <td><input class="input_15_table" name="account" id="account" type="text" maxlength="20"></td>
    </tr>
    <tr>
      <th><#AiDisk_Password#>: </th>
      <td><input class="input_15_table" name="password" id="password" type="password" maxlength="20"></td>
    </tr>
    <tr>
      <th><#Confirmpassword#>: </th>
      <td><input class="input_15_table" name="confirm_password" id="confirm_password" type="password" maxlength="20"></td>
    </tr>
	</tbody>	
    <tr bgcolor="#E6E6E6">
    <th colspan="2" align="right"><input id="Submit" type="button" class="button_gen" value="<#CTL_add#>"></td>    </tr>
  </table>
</form>
</body>
</html>
