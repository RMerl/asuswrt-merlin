<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Mod New Account</title>
<link rel="stylesheet" href="../form_style.css"  type="text/css">

<script type="text/javascript" src="../state.js"></script>
<script type="text/javascript" src="../help.js"></script>
<script type="text/javascript" src="../validator.js"></script>
<script type="text/javascript">
var selectedAccount = parent.getSelectedAccount();

function initial(){
	document.getElementById("new_account").value = decodeURIComponent(selectedAccount);
	showtext(document.getElementById("selected_account"),  htmlEnDeCode.htmlEncode(decodeURIComponent(selectedAccount)));
	clickevent();
}

function clickevent(){
	document.getElementById("Submit").onclick = function(){
		if(validForm()){
			document.getElementById("account").value = decodeURIComponent(selectedAccount);			
			parent.showLoading();
			document.modifyAccountForm.submit();
			parent.hidePop("apply");
		}
	};
}

function checkDuplicateName(newname, teststr){
	var existing_string = decodeURIComponent(teststr.join(','));
	existing_string = "," + existing_string + ",";
	var newstr = "," + trim(newname) + ","; 
	var re = new RegExp(newstr,"gi")
	var matchArray =  existing_string.match(re);
	
	if (matchArray != null)
		return true;
	else
		return false;
}

function validForm(){
	showtext(document.getElementById("alert_msg2"), "");

	if(document.getElementById("new_account").value.length == 0){
			alert("<#File_Pop_content_alert_desc1#>");
			document.getElementById("new_account").focus();
			return false;
	}
	else{				
			var alert_str = validator.hostName(document.getElementById("new_account"));
			if(alert_str != ""){
				alert(alert_str);
				document.getElementById("new_account").focus();
				return false;
			}

			document.getElementById("new_account").value = trim(document.getElementById("new_account").value);
				
			if(document.getElementById("new_account").value == "root"
					|| document.getElementById("new_account").value == "guest"
					|| document.getElementById("new_account").value == "anonymous"
			){
					alert("<#USB_Application_account_alert#>");
					document.getElementById("new_account").focus();
					return false;
			}
			else if(checkDuplicateName(document.getElementById("new_account").value, parent.get_accounts()) &&
				document.getElementById("new_account").value != decodeURIComponent(selectedAccount)){			
					alert("<#File_Pop_content_alert_desc5#>");
					document.getElementById("new_account").focus();
					return false;
			}
	}

	// password
	if(document.getElementById("new_password").value.length <= 0 || document.getElementById("confirm_password").value.length <= 0){
		showtext(document.getElementById("alert_msg2"),"*<#File_Pop_content_alert_desc6#>");
		if(document.getElementById("new_password").value.length <= 0){
			document.getElementById("new_password").focus();
			document.getElementById("new_password").select();
		}else{
			document.getElementById("confirm_password").focus();
			document.getElementById("confirm_password").select();
		}
		
		return false;
	}

	if(document.getElementById("new_password").value != document.getElementById("confirm_password").value){
		showtext(document.getElementById("alert_msg2"),"*<#File_Pop_content_alert_desc7#>");
		document.getElementById("confirm_password").focus();
		return false;
	}

	if(!validator.string(document.modifyAccountForm.new_password)){
		document.getElementById("new_password").focus();
		document.getElementById("new_password").select();
		return false;
	}

	if(document.getElementById("new_password").value.length > 16){
		showtext(document.getElementById("alert_msg2"),"*<#LANHostConfig_x_Password_itemdesc#>");
		document.getElementById("password").focus();
		document.getElementById("password").select();
		return false;
	}

	return true;
}
</script>
</head>

<body onLoad="initial();">
<form method="post" name="modifyAccountForm" action="modify_account.asp" target="hidden_frame">
<input name="account" id="account" type="hidden" value="">
	<table width="90%" class="popTable" border="0" align="center" cellpadding="0" cellspacing="0">
	<thead>
    <tr>
      <td colspan="2"><span style="color:#FFF"><#ModAccountTitle#>: </span><span style="color:#FFF" id="selected_account"></span><img src="../images/button-close.gif" onClick="parent.hidePop('OverlayMask');"></td>
    </tr>
    </thead>	
	<tbody>
    <tr valign="middle">
      <td height="30" colspan="2" class="hint_word"><#ModAccountAlert#></td>
    </tr>
    <tr>
      <th><#AiDisk_Account#>: </th>
      <td>
      	<input class="input_15_table" name="new_account" id="new_account" type="text" maxlength="20" autocorrect="off" autocapitalize="off">
      </td>
    </tr>
    <tr>
      <th><#ModAccountPassword#>: </th>
      <td><input type="password" class="input_15_table" name="new_password" id="new_password" onKeyPress="return validator.isString(this, event);" maxlength="17" autocorrect="off" autocapitalize="off"></td>
    </tr>
    <tr>
      <th><#Confirmpassword#>: </th>
      <td><input type="password" class="input_15_table" name="confirm_password" id="confirm_password" onKeyPress="return validator.isString(this, event);" maxlength="17" autocorrect="off" autocapitalize="off">
      		<br/><span id="alert_msg2" style="color:#FC0;margin-left:8px;"></span>	
      </td>
    </tr>
	</tbody>	
    <tr bgcolor="#E6E6E6">
      <th colspan="2" align="right"><input id="Submit" type="button" class="button_gen" value="<#CTL_modify#>"></th>
    </tr>
  </table>
</form>
</body>
</html>
