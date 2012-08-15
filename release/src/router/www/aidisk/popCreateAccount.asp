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
				parent.showLoading();
				document.createAccountForm.submit();
				parent.hidePop("apply");
			}
		};
}

function validForm(){
	showtext($("alert_msg1"), "");
	showtext($("alert_msg2"), "");

	// account name
	var alert_str = validate_account($("account"), "noalert");

	if(alert_str != ""){
		showtext($("alert_msg1"), alert_str);
		$("account").focus();
		return false;
	}

	$("account").value = trim($("account").value);
	
	if($("account").value.length == 0){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc1#>");
		$("account").focus();
		return false;
	}

	if($("account").value == "root"
			|| $("account").value == "guest"
			|| $("account").value == "anonymous"
			){
		showtext($("alert_msg1"), "<#USB_Application_account_alert#>");
		$("account").focus();
		return false;
	}

	if($("account").value.length <= 1){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc2#>");
		$("account").focus();
		return false;
	}

	if($("account").value.length > 20){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc3#>");
		$("account").focus();
		return false;
	}

	if(checkDuplicateName($("account").value, parent.get_accounts())){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc5#>");
		$("account").focus();
		return false;
	}

	// password
	if($("password").value.length <= 0 || $("confirm_password").value.length <= 0){
		showtext($("alert_msg2"),"*<#File_Pop_content_alert_desc6#>");
		if($("password").value.length <= 0){
				$("password").focus();
				$("password").select();
		}else{
				$("confirm_password").focus();
				$("confirm_password").select();
		}
		return false;
	}

	if($("password").value != $("confirm_password").value){
		showtext($("alert_msg2"),"*<#File_Pop_content_alert_desc7#>");
		$("confirm_password").focus();
		return false;
	}

	if(!validate_string(document.createAccountForm.password)){
		$("password").focus();
		$("password").select();
		return false;
	}

	if($("password").value.length > 16){
		showtext($("alert_msg2"),"*<#LANHostConfig_x_Password_itemdesc#>");
		$("password").focus();
		$("password").select();
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
      <td><input class="input_15_table" name="account" id="account" type="text" maxlength="20">
      		<br/><span id="alert_msg1"></span>	
      </td>
    </tr>
    <tr>
      <th><#PPPConnection_Password_itemname#>: </th>
      <td><input type="password" class="input_15_table" autocapitalization="off" name="password" id="password" onKeyPress="return is_string(this, event);" maxlength="17"></td>
    </tr>
    <tr>
      <th><#Confirmpassword#>: </th>
      <td><input type="password" class="input_15_table" autocapitalization="off" name="confirm_password" id="confirm_password" onKeyPress="return is_string(this, event);" maxlength="17">
      		<br/><span id="alert_msg2"></span>	
      </td>
    </tr>
	</tbody>	
    <tr bgcolor="#E6E6E6">
    <th colspan="2" align="right"><input id="Submit" type="button" class="button_gen" value="<#CTL_add#>"></td>    </tr>
  </table>
</form>
</body>
</html>
