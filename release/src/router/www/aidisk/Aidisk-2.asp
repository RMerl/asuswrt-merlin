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
<script>
var next_page = "";
var dummyShareway = 0;

function initial(){
	//parent.show_help_iframe(2);
	parent.hideLoading();
	
	parent.restore_help_td();	
	//parent.openHint(15, 2);
	
	parent.$("dummyShareway").value = "<% nvram_get("dummyShareway"); %>";
	if(parent.$("dummyShareway").value == "")
		parent.$("dummyShareway").value = 0;
	showTextinWizard(parent.$("dummyShareway").value);		
}

function showTextinWizard(flag){
	dummyShareway = flag;
	
	if(dummyShareway == 0){
		parent.$("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[dummyShareway].focus();
		document.getElementsByName('dummyoption')[dummyShareway].checked = true;
		
		$("share1").style.display = "none";				
		$("target1").style.display = "none";
		$("target2").style.display = "none";
	}
	else if(dummyShareway == 1){
		parent.$("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[dummyShareway].focus();
		document.getElementsByName('dummyoption')[dummyShareway].checked = true;
		
		showtext($("user1"), '<% nvram_get("http_username"); %>');
		
		showtext($("user2"), "Family");
		$("userpasswd2").value =  "Family";

		$("share1").style.display = "block";		
		$("target1").style.display = "";
		$("target2").style.display = "";
	}
	else if(dummyShareway == 2){
		parent.$("dummyShareway").value = dummyShareway;
		
		document.getElementsByName('dummyoption')[dummyShareway].focus();
		document.getElementsByName('dummyoption')[dummyShareway].checked = true;
		
		showtext($("user1"), '<% nvram_get("http_username"); %>');
		
		$("share1").style.display = "";		
		$("target1").style.display = "";
		$("target2").style.display = "none";
	}
	else
		alert("System error: No this choice");	// no translate*/
}

function passTheResult(){
	if(dummyShareway == 0){
		parent.$("accountNum").value = 0;
		
		/*parent.$("account0").value = "";
		parent.$("passwd0").value = "";
		parent.$("permission0").value = "";//*/
		
		parent.$("account1").value = "";
		parent.$("passwd1").value = "";
		parent.$("permission1").value = "";
	}
	else if(dummyShareway == 1){
		parent.$("accountNum").value = 2;
		
		/*if(checkPasswdValid($("userpasswd1").value)){
			parent.$("account0").value = $("user1").firstChild.nodeValue;
			parent.$("passwd0").value = $("userpasswd1").value;
			parent.$("permission0").value = "3";
		}
		else{
			$("userpasswd1").focus();
			return;
		}//*/
		
		if(checkPasswdValid(document.smartForm.userpasswd2)){
			parent.$("account1").value = $("user2").firstChild.nodeValue;
			parent.$("passwd1").value = $("userpasswd2").value;
			parent.$("permission1").value = "1";
		}
		else{
			document.smartForm.action = "/aidisk/Aidisk-2.asp";
			document.smartForm.submit();
			return;
		}
	}
	else if(dummyShareway == 2){
		parent.$("accountNum").value = 1;
		
		/*if(checkPasswdValid($("userpasswd1").value)){
			parent.$("account0").value = $("user1").firstChild.nodeValue;
			parent.$("passwd0").value = $("userpasswd1").value;
			parent.$("permission0").value = "3";
		}
		else{
			$("userpasswd1").focus();
			return;
		}//*/
		
		parent.$("account1").value = "";
		parent.$("passwd1").value = "";
		parent.$("permission1").value = "";
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
	
	if(!validate_string(obj)){
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
            <br/><p><input type="radio" id="d1" name="dummyoption" value="0" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d1"><#Step2_method1#></label>
            		</p>
            <br/><p><input type="radio" id="d2" name="dummyoption" value="1" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d2"><#Step2_method2#></label>
            		</p>
            <br/><p><input type="radio" id="d3" name="dummyoption" value="2" width="10" onclick="showTextinWizard(this.value);"/> 
            				<label for="d3"><#Step2_method3#></label>
            		</p>
						</div>
            <div id="share1" style="margin-top:30px;">
            	<table width="80%" border="1" align="center" cellpadding="2" cellspacing="0" bordercolor="#7ea7bd" class="FormTable_table">
                	<tr>
                  	<th width="100"><#AiDisk_Account#></th>
                  	<th><#PPPConnection_Password_itemname#></th>
                  	<th width="50" ><#AiDisk_Read#></th>
                  	<th width="50" ><#AiDisk_Write#></th>
                	</tr>
                
                	<tr id="target1">
                  	<td height="35"><span id="user1" style="color:#FFFFFF;"></span></td>
                  	<td><!--input type="text" name="userpasswd1" id="userpasswd1" value="" class="input_25_table"--></td>
                  	<td align="center"><img src="/images/New_ui/checkbox.png"></td>
                  	<td align="center"><img src="/images/New_ui/checkbox.png"></td>
                	</tr>
                
                	<tr id="target2">
                  	<td height="35"><span id="user2" style="color:#FFFFFF;"></span></td>
                  	<td><input type="text" name="userpasswd2" id="userpasswd2" value="" class="input_25_table" onKeyPress="return is_string(this, event);" maxlength="16"></td>
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
  				<input type="button" id="prevButton" value="<#btn_pre#>" onclick="go_pre_page();" class="button_gen">
  				<input type="submit" id="nextButton" value="<#btn_next#>" class="button_gen">  							
        	<!--a href="javascript:go_pre_page();"><div class="titlebtn" align="center" style="margin-left:275px;_margin-left:137px;width:80px;"><span><#btn_pre#></span></div></a>
        	<a href="javascript:passTheResult();"><div class="titlebtn" align="center" style="width:80px;"><span><#btn_next#></span></div></a-->
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
