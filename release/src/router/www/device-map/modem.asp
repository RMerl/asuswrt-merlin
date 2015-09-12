<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title></title>
<link href="../NM_style.css" rel="stylesheet" type="text/css" />
<link href="../form_style.css" rel="stylesheet" type="text/css" />
<script type="text/javascript" src="/state.js"></script>
<script>
if(parent.location.pathname.search("index") === -1) top.location.href = "../index.asp";

function initial(){
		showtext(document.getElementById("disk_model_name"), parent.usbPorts[parent.currentUsbPort].deviceName);
		if(sw_mode != "1")
			inputHideCtrl(document.diskForm.btn_Hspda, 0);
}

function goHspdaWizard(){
	parent.location.href = "/Advanced_Modem_Content.asp";
}
</script>
</head>

<body class="statusbody" onload="initial();">
<form method="post" name="diskForm" action="">
<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px">
	<tr>
    <td style="padding:5px 10px 0px 15px;">
    	<p class="formfonttitle_nwm"><#Modelname#>:</p>
    	<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px; color:#FFFFFF;" id="disk_model_name"></p>
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>

  <tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    	<p class="formfonttitle_nwm" style="float:left;width:138px;"><#GO_HSDPA_SETTING#></p>
    	<input type="button" name="btn_Hspda" class="button_gen" onclick="goHspdaWizard();" value="<#btn_go#>" >
      <img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
  </tr>
</table>


<input type="hidden" name="disk" value="">
</form>
</body>
</html>
