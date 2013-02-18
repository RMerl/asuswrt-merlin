<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Add New Folder</title>
<link rel="stylesheet" href="../form_style.css"  type="text/css">
<script type="text/javascript" src="../state.js"></script>
<script type="text/javascript">

var PoolDevice = parent.pool_devices()[parent.getSelectedPoolOrder()];
var PoolName = parent.pool_names()[parent.getSelectedPoolOrder()];
var folderlist = parent.get_sharedfolder_in_pool(PoolDevice);

function initial(){
	showtext($("poolName"), PoolName);
	document.createFolderForm.folder.focus();
	clickevent();
}

function clickevent(){
	if(navigator.userAgent.search("MSIE") == -1)
		document.getElementById('folder').addEventListener('keydown',keyDownHandler,false);
	else
		document.getElementById('folder').attachEvent('onkeydown',keyDownHandler);
	
	$("Submit").onclick = submit;
}
function submit(){
	if(validForm()){
				if(parent.get_manage_type(parent.document.aidiskForm.protocol.value) == 1)
					document.createFolderForm.account.value = parent.getSelectedAccount();
				else
					document.createFolderForm.account.disabled = 1;
					
				document.createFolderForm.pool.value = PoolDevice;
				if(parent.document.form.current_page.value != "Advanced_AiDisk_NFS.asp" && parent.document.form.current_page.value != "Tools_OtherSettings.asp" && parent.document.form.current_page.value != "mediaserver.asp" && parent.document.form.current_page.value != "cloud_sync.asp")
					parent.showLoading();
					
				document.createFolderForm.submit();
				parent.hidePop("apply");
				setTimeout(" ",5000);							
				if(parent.document.form.current_page.value == "Advanced_AiDisk_NFS.asp" || parent.document.form.current_page.value == "Tools_OtherSettings.asp" || parent.document.form.current_page.value == "mediaserver.asp" || parent.document.form.current_page.value == "cloud_sync.asp"){
					if(parent.document.aidiskForm.test_flag.value == 1){
						parent.FromObject = parent.document.aidiskForm.layer_order.value.substring(0,3);
						setTimeout(" ",3000);
						parent.get_layer_items(parent.document.aidiskForm.layer_order.value.substring(0,3));
					}	
					else{
						parent.FromObject = parent.document.aidiskForm.layer_order.value;
						setTimeout(" ",3000);
						parent.get_layer_items(parent.document.aidiskForm.layer_order.value);						
					}
				}	
			}
}
function keyDownHandler(event){
	var keyPressed = event.keyCode ? event.keyCode : event.which;

	if(keyPressed == 13){   // Enter key
		submit();
	}	
	else if(keyPressed == 27){  // Escape key
		parent.hidePop("apply");
	}	
}

function validForm(){
	$("folder").value = trim($("folder").value);

	// share name
	if($("folder").value.length == 0){
		alert("<#File_content_alert_desc6#>");
		$("folder").focus();
		return false;
	}
	
	var re = new RegExp("[^a-zA-Z0-9 _-]+", "gi");
	if(re.test($("folder").value)){
		alert("<#File_content_alert_desc7#>");
		$("folder").focus();
		return false;
	}
	
	if(parent.checkDuplicateName($("folder").value, folderlist)){
		alert("<#File_content_alert_desc8#>");
		$("folder").focus();
		return false;
	}
	
	if(trim($("folder").value).length > 12)
		if (!(confirm("<#File_content_alert_desc10#>")))
			return false;
	
	return true;
}

function NoSubmit(e){
    e = e || window.event;  
    var keynum = e.keyCode || e.which;
    if(keynum === 13){        
        return false;
    }
}
</script>
</head>

<body onLoad="initial();">
	
<form name="createFolderForm" method="post" action="create_sharedfolder.asp" target="hidden_frame">
<input type="hidden" name="account" id="account">
<input type="hidden" name="pool" id="pool">
	<table width="100%" class="popTable" border="0" align="center" cellpadding="0" cellspacing="0">
	<thead>
    <tr>
      <td colspan="2"><span style="color:#FFF"><#AddFolderTitle#> <#in#> </span><span style="color:#FFF" id="poolName"></span><img src="../images/button-close.gif" onClick="parent.hidePop('OverlayMask');"></td>
    </tr>
	</thead>
	<tbody>
    <tr align="center">
      <td height="50" colspan="2"><#AddFolderAlert#></td>
    </tr>
    <tr>
      <th width="100"><#FolderName#>: </th>
      <td height="50"><input class="input_25_table" type="text" name="folder" id="folder" style="width:220px;" onkeypress="return NoSubmit(event)"></td>
    </tr>
    <tr bgcolor="#E6E6E6">
      <th colspan="2"><input id="Submit" type="button" class="button_gen" value="<#CTL_add#>"></th>
    </tr>
  </tbody>	
  </table>
</form>
</body>
</html>
