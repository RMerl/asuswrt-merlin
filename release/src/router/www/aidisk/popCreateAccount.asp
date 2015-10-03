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
<script type="text/javascript" src="../help.js"></script>
<script type="text/javascript" src="../validator.js"></script>
<script type="text/javascript">
function clickevent(){
	document.getElementById("Submit").onclick = function(){
			if(validForm()){
				parent.showLoading();
				document.createAccountForm.submit();
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

	if(document.getElementById("account").value.length == 0){
		alert("<#File_Pop_content_alert_desc1#>");
		document.getElementById("account").focus();
		return false;
	}
	else{
		var alert_str = validator.hostName(document.getElementById("account"));

		if(alert_str != ""){
			alert(alert_str);			
			document.getElementById("account").focus();
			return false;
		}			

		document.getElementById("account").value = trim(document.getElementById("account").value);

		if(document.getElementById("account").value == "root"
				|| document.getElementById("account").value == "guest"
				|| document.getElementById("account").value == "anonymous"
		){
				alert("<#USB_Application_account_alert#>");				
				document.getElementById("account").focus();
				return false;
		}
		else if(checkDuplicateName(document.getElementById("account").value, parent.get_accounts())){
				alert("<#File_Pop_content_alert_desc5#>");				
				document.getElementById("account").focus();
				return false;
		}
	}	

	// password
	if(document.getElementById("password").value.length <= 0 || document.getElementById("confirm_password").value.length <= 0){
		showtext(document.getElementById("alert_msg2"),"*<#File_Pop_content_alert_desc6#>");
		if(document.getElementById("password").value.length <= 0){
				document.getElementById("password").focus();
				document.getElementById("password").select();
		}else{
				document.getElementById("confirm_password").focus();
				document.getElementById("confirm_password").select();
		}
		return false;
	}

	if(document.getElementById("password").value != document.getElementById("confirm_password").value){
		showtext(document.getElementById("alert_msg2"),"*<#File_Pop_content_alert_desc7#>");
		document.getElementById("confirm_password").focus();
		return false;
	}

	if(!validator.string(document.createAccountForm.password)){
		document.getElementById("password").focus();
		document.getElementById("password").select();
		return false;
	}

	if(document.getElementById("password").value.length > 16){
		showtext(document.getElementById("alert_msg2"),"*<#LANHostConfig_x_Password_itemdesc#>");
		document.getElementById("password").focus();
		document.getElementById("password").select();
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
      <td>
      	<input class="input_15_table" name="account" id="account" type="text" maxlength="20" autocorrect="off" autocapitalize="off">      		
      </td>
    </tr>
    <tr>
      <th><#HSDPAConfig_Password_itemname#>: </th>
      <td><input type="password" class="input_15_table" name="password" id="password" onKeyPress="return validator.isString(this, event);" maxlength="17" autocorrect="off" autocapitalize="off"></td>
    </tr>
    <tr>
      <th><#Confirmpassword#>: </th>
      <td><input type="password" class="input_15_table" name="confirm_password" id="confirm_password" onKeyPress="return validator.isString(this, event);" maxlength="17" autocorrect="off" autocapitalize="off">
      		<br/><span id="alert_msg2" style="color:#FC0;margin-left:8px;"></span>	
      </td>
    </tr>
	</tbody>	
    <tr bgcolor="#E6E6E6">
    <th colspan="2" align="right"><input id="Submit" type="button" class="button_gen" value="<#CTL_add#>"></td>    </tr>
  </table>
</form>
</body>
</html>
