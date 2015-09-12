<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Del Folder</title>
<link rel="stylesheet" href="../form_style.css"  type="text/css">
<script type="text/javascript" src="../state.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript">
<% get_AiDisk_status(); %>



var PoolDevice = parent.usbDevicesList[parent.getSelectedDiskOrder()].partition[parent.getSelectedPoolOrder()].mountPoint;
var folderlist = get_sharedfolder_in_pool(PoolDevice);
var selectedFolder = folderlist[parent.getSelectedFolderOrder()];
var DisplayFolderName = selectedFolder;
var delete_flag = 0;

function initial(){
	DisplayFolderName = check_folder_length(selectedFolder);
	showtext(document.getElementById("selected_Folder"), showhtmlspace(showhtmland(DisplayFolderName)));
	document.deleteFolderForm.Cancel.focus();
	get_layer_items_test(parent.document.aidiskForm.layer_order.value.substring(0,5));
	clickevent();	
}

function clickevent(){
	if(navigator.userAgent.search("MSIE") == -1)
		window.addEventListener('keydown',keyDownHandler,false);
	else{	
		//window.attachEvent('onkeydown',keyDownHandler);
	}
	document.getElementById("Submit").onclick = submit;
}
function submit(){
	document.getElementById("pool").value = PoolDevice;
	document.getElementById("folder").value = selectedFolder;
	if(parent.document.form.current_page.value != "mediaserver.asp" 
	&& parent.document.form.current_page.value != "Advanced_AiDisk_NFS.asp" 
	&& parent.document.form.current_page.value != "Tools_OtherSettings.asp" 
	&& parent.document.form.current_page.value != "cloud_sync.asp"
	&& parent.document.form.current_page.value != "cloud_router_sync.asp" ){
		parent.showLoading();
	}
	
	document.deleteFolderForm.submit();
	parent.hidePop("apply");
	setTimeout(" ",5000);
	if(parent.document.form.current_page.value == "mediaserver.asp" 
	|| parent.document.form.current_page.value == "Advanced_AiDisk_NFS.asp" 
	|| parent.document.form.current_page.value == "Tools_OtherSettings.asp" 
	|| parent.document.form.current_page.value == "cloud_sync.asp"
	|| parent.document.form.current_page.value == "cloud_router_sync.asp" ){
		if(delete_flag == 1){
			parent.FromObject = parent.document.aidiskForm.layer_order.value.substring(0,3);
			setTimeout(" ",3000);
			parent.get_layer_items(parent.document.aidiskForm.layer_order.value.substring(0,3));
			delete_flag = 0;
		}	
		else{
			parent.FromObject = parent.document.aidiskForm.layer_order.value.substring(0,5);
			setTimeout(" ",3000);
			parent.get_layer_items(parent.document.aidiskForm.layer_order.value.substring(0,5));				
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

function get_layer_items_test(layer_order_t){
	$.ajax({
    		url: '/gettree.asp?layer_order='+layer_order_t,
    		dataType: 'script',
    		error: function(xhr){
    			;
    		},
    		success: function(){
				delete_flag = treeitems.length;
  			}
	});
}
function check_folder_length(folder_name){   //Jieming added at 2013/04/16, to divide folder name when length of folder name exceed 44
	var n;
	var temp_name = "";
	var start = 0;
	var end = 0;

	if(folder_name.length > 44){
		n = parseInt(folder_name.length/44);
		for(i=0;i<n;i++){
			start = 44*i;	
			end = 44*(i+1);
			temp_name += folder_name.substring(start, end);
			temp_name += "<br>";
		}
		temp_name += folder_name.substring(end, folder_name.length);
		folder_name = temp_name;
	}

	return folder_name;
}
</script>
</head>
<body onLoad="initial();" onKeyPress="keyDownHandler(event);">
<form method="post" name="deleteFolderForm" action="delete_sharedfolder.asp" target="hidden_frame">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
		<table width="100%" class="popTable" border="0" align="center" cellpadding="0" cellspacing="0">
			<thead>
			<tr>
				<td>
					<span style="color:#FFF"><#DelFolderTitle#></span><img src="../images/button-close.gif" onClick="parent.hidePop('OverlayMask');">
				</td>
			</tr>
			</thead>
			<tbody>
			<tr>
				<td height="70" valign="middle"><#DelFolderAlert#>
					<span id="selected_Folder" style="color:#333333;"></span>
				</td>
			</tr>
			<tr>
				<td height="30" align="right" bgcolor="#E6E6E6">
					<input name="Submit" id="Submit" type="button" class="button_gen" value="<#CTL_del#>" onclick="">
					<input name="Cancel" id="Cancel" type="button" class="button_gen" value="<#CTL_Cancel#>" onClick="parent.hidePop('OverlayMask');">
				</td>
			</tr>
			</tbody>
		</table>
	</form>  
</body>
</html>
