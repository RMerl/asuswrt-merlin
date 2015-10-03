<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>AiDisk Wizard</title>
<link rel="stylesheet" type="text/css" href="aidisk.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script>
var next_page = "";
var dummyShareway = 0;

function initial(){
	
	parent.hideLoading();
	
	parent.restore_help_td();	
	
	
	parent.document.getElementById("dummyShareway").value = "<% nvram_get("dummyShareway"); %>";
	if(parent.document.getElementById("dummyShareway").value == "")
			parent.document.getElementById("dummyShareway").value = 0;

	if(is_KR_sku){
		document.getElementById("d1").style.display = "none";	
		document.getElementById("d1_desc").style.display = "none";			
	}
	showTextinWizard(2);
}

var lan_hwaddr_array = '<% nvram_get("lan_hwaddr"); %>'.split(':');
function showTextinWizard(flag){
	dummyShareway = flag;
	
	if(dummyShareway == 0){
		parent.document.getElementById("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[2].focus();
		document.getElementsByName('dummyoption')[2].checked = true;
		
		document.getElementById("share0_Hint").style.display = "";
		document.getElementById("share1").style.display = "none";				
		document.getElementById("target1").style.display = "none";
		document.getElementById("target2").style.display = "none";
	}
	else if(dummyShareway == 1){
		parent.document.getElementById("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[1].focus();
		document.getElementsByName('dummyoption')[1].checked = true;
		
		showtext(document.getElementById("user1"), '<% nvram_get("http_username"); %>');
		
		showtext(document.getElementById("user2"), "Family");
		document.getElementById("userpasswd2").value =  "family" + lan_hwaddr_array[lan_hwaddr_array.length-2].toLowerCase() + lan_hwaddr_array[lan_hwaddr_array.length-1].toLowerCase();

		document.getElementById("share0_Hint").style.display = "none";
		document.getElementById("share1").style.display = "block";		
		document.getElementById("target1").style.display = "";
		document.getElementById("target2").style.display = "";
	}
	else if(dummyShareway == 2){
		parent.document.getElementById("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[0].focus();
		document.getElementsByName('dummyoption')[0].checked = true;
		
		showtext(document.getElementById("user1"), '<% nvram_get("http_username"); %>');
		
		document.getElementById("share0_Hint").style.display = "none";
		document.getElementById("share1").style.display = "";		
		document.getElementById("target1").style.display = "";
		document.getElementById("target2").style.display = "none";
	}
	else
		alert("System error: No this choice");	// no translate*/
}

function passTheResult(){
	if(dummyShareway == 0){
		parent.document.getElementById("accountNum").value = 0;
		
		/*parent.document.getElementById("account0").value = "";
		parent.document.getElementById("passwd0").value = "";
		parent.document.getElementById("permission0").value = "";//*/
		
		parent.document.getElementById("account1").value = "";
		parent.document.getElementById("passwd1").value = "";
		parent.document.getElementById("permission1").value = "";
	}
	else if(dummyShareway == 1){
		parent.document.getElementById("accountNum").value = 2;
		
		/*if(checkPasswdValid(document.getElementById("userpasswd1").value)){
			parent.document.getElementById("account0").value = document.getElementById("user1").firstChild.nodeValue;
			parent.document.getElementById("passwd0").value = document.getElementById("userpasswd1").value;
			parent.document.getElementById("permission0").value = "3";
		}
		else{
			document.getElementById("userpasswd1").focus();
			return;
		}//*/
		
		if(checkPasswdValid(document.smartForm.userpasswd2)){
			parent.document.getElementById("account1").value = document.getElementById("user2").firstChild.nodeValue;
			parent.document.getElementById("passwd1").value = document.getElementById("userpasswd2").value;
			parent.document.getElementById("permission1").value = "1";
		}
		else{
			document.smartForm.action = "/aidisk/Aidisk-2.asp";
			document.smartForm.submit();
			return;
		}
	}
	else if(dummyShareway == 2){
		parent.document.getElementById("accountNum").value = 1;
		
		/*if(checkPasswdValid(document.getElementById("userpasswd1").value)){
			parent.document.getElementById("account0").value = document.getElementById("user1").firstChild.nodeValue;
			parent.document.getElementById("passwd0").value = document.getElementById("userpasswd1").value;
			parent.document.getElementById("permission0").value = "3";
		}
		else{
			document.getElementById("userpasswd1").focus();
			return;
		}//*/
		
		parent.document.getElementById("account1").value = "";
		parent.document.getElementById("passwd1").value = "";
		parent.document.getElementById("permission1").value = "";
	}
	
	document.smartForm.action = "/aidisk/Aidisk-3.asp";
	document.smartForm.submit();
}

function go_pre_page(){
	document.smartForm.action = "/aidisk/Aidisk-1.asp";
	document.smartForm.submit();
}

function checkPasswdValid(obj){
	if(obj.value.length <= 0){
		alert("<#File_Pop_content_alert_desc6#>");
		obj.focus();
		obj.select();
		return false;
	}	
	
	if(!validator.string(obj)){
			obj.focus();
			obj.select();
			return false;	
	}	
	
	return true;
}
</script>
</head>

<body onload="initial();">
<form method="post" name="smartForm" id="smartForm" action="Aidisk-3.asp" onsubmit="return passTheResult();">
<input type="hidden" name="accountNum" id="accountNum" value="">
<input type="hidden" name="account0" id="account0" value="">
<input type="hidden" name="passwd0" id="passwd0" value="">
<input type="hidden" name="permission0" id="permission0" value="">
<input type="hidden" name="account1" id="account1" value="">
<input type="hidden" name="passwd1" id="passwd1" value="">
<input type="hidden" name="permission1" id="permission1" value="">
<!--/form-->

<div class="aidisk1_table">
<table>	<!-- width="765" height="760" border="0" bgcolor="#4d595d" cellpadding="0"  cellspacing="0" style="padding:10px; padding-top:20px;"  //Viz-->
  
<tr>
  <td valign="top" bgcolor="#4d595d" style="padding-top:25px;">	<!-- height="780"  align="center" //Viz-->
  <table width="740" height="225" border="0">
    <!-- start Step 1 -->  
  	<tr>
    	<td>
    	<table width="740" border="0">
    		<tr>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step1.png) 0% 95% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step2.png) 0% 0% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step3.png) 0% 0% no-repeat;"></td>
    			<td width="25%">&nbsp;</td>
    		</tr>
    	</table>
    	</td>
    </tr>

    <tr>
    	<td align="left" class="formfonttitle" style="padding-left:20px;" height="72"><#Step2_method#>: <#Step1_desp#></td>
    </tr>
  
    <!--tr>
    	<td align="left" valign="bottom" class="formfontcontent"><span class="formfonttitle" style="margin-left:20px;"></span>
    	</td>
    </tr-->

    <tr>
      <td valign="top">
						<div style="margin-left:20px;">
            <br/><p><input type="radio" id="d3" name="dummyoption" value="2" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d3"><#Step2_method3#></label>
            		</p>
            <br/><p><input type="radio" id="d2" name="dummyoption" value="1" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d2"><#Step2_method2#></label>
            		</p>
            <br/><p><input type="radio" id="d1" name="dummyoption" value="0" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d1" id="d1_desc"><#Step2_method1#></label>
            		</p>
						</div>

						<div id="share0_Hint" style="margin-top:10px;color:#FC0;margin-left:45px;">
							<span><#AiDisk_shareHint#></span>
						</div>

            <div id="share1" style="margin-top:30px;">
            	<table width="80%" border="1" align="center" cellpadding="2" cellspacing="0" bordercolor="#7ea7bd" class="FormTable_table">
                	<tr>
                  	<th width="100"><#AiDisk_Account#></th>
                  	<th><#HSDPAConfig_Password_itemname#></th>
                  	<th width="50" ><#AiDisk_Read#></th>
                  	<th width="50" ><#AiDisk_Write#></th>
                	</tr>
                
                	<tr id="target1">
                  	<td height="35"><span id="user1" style="color:#FFFFFF;"></span></td>
                  	<td>*</td>
                  	<td align="center"><img src="/images/New_ui/checkbox.png"></td>
                  	<td align="center"><img src="/images/New_ui/checkbox.png"></td>
                	</tr>
                
                	<tr id="target2">
                  	<td height="35"><span id="user2" style="color:#FFFFFF;"></span></td>
                  	<td><input type="text" name="userpasswd2" id="userpasswd2" value="" class="input_25_table" onKeyPress="return validator.isString(this, event);" maxlength="16" autocorrect="off" autocapitalize="off"></td>
                  	<td align="center"><img src="/images/New_ui/checkbox.png"></td>
                  	<td align="center">&nbsp;</td>
                	</tr>
              	</table>  
           </div>          
      </td>
    </tr>
    
    <tr>
    	<td>
    		<div style="margin-top:30px;"><img src="/images/New_ui/export/line_export.png" /></div>
    	</td>
    </tr>     
    
    <tr valign="bottom" align="center">
    	<td width="20%">
		<div class="apply_gen" style="margin-top:30px">
  			<input type="button" id="prevButton" value="<#CTL_prev#>" onclick="go_pre_page();" class="button_gen">
			<input type="submit" id="nextButton" value="<#CTL_next#>" class="button_gen">
		</div>	 
	</td>
    </tr>
    <!-- end -->    
  </table>
  </td>
</tr>  
</table>
</div>	<!--//Viz-->
</form>
</body>
</html>
