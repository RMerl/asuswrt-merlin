var username = getUrlVars()["username"];
var password = getUrlVars()["password"];
var chal = getUrlVars()["chal"];
var login = getUrlVars()["login"];
var logout = getUrlVars()["logout"];
var prelogin = getUrlVars()["prelogin"];
var res = getUrlVars()["res"];
var uamip = getUrlVars()["uamip"];
var uamport = getUrlVars()["uamport"];
var timeleft = getUrlVars()["timeleft"];
var userurl = getUrlVars()["userurl"];
var challenge = getUrlVars()["challenge"];

function getUrlVars(){
	var vars = [], hash;
	var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
	for(var i = 0; i < hashes.length; i++){
		hash = hashes[i].split('=');
    		vars.push(hash[0]);
 	   	vars[hash[0]] = hash[1];
	}
	return vars;
}

$("document").ready(function() {	
	//alert("res=" + res + ", username=" + username + ", password=" + password);
	

	var divObj = document.createElement("div");
	divObj.setAttribute("id","login_section");
	divObj.setAttribute("style","display: none;");
	var html = "";
	html +='<form name="form" method="get" action="/Uam">';
	html +='<input type="hidden" name="chal" id="chal" value="">';
	html +='<input type="hidden" name="uamip" id="uamip" value="">';
	html +='<input type="hidden" name="uamport" id="uamport" value="">';
	html +='<input type="hidden" name="userurl" id="userurl" value="">';
	html +='<center>';
	html +='<table border="0" cellpadding="5" cellspacing="0" style="width: 217px;">';
	html +='<tbody>';
	html +='<tr>';
	html +='<td align="right">Login:</td>';
	html +='<td><input type="text" name="UserName" id="UserName" size="20" maxlength="255"></td>';
	html +='</tr>';
	html +='<tr>';
	html +='<td align="right">Password:</td>';
	html +='<td><input type="password" name="Password" id="Password" size="20" maxlength="255"></td>';
	html +='</tr>';
	html +='<tr>';
	html +='<td align="center" colspan="2" height="23"><input type="button" onclick="formSubmit(1)" value="Submit"><!--<input type="submit" name="login" value="login">--></td>';
	html +='</tr>';
	html +='<tr>';
	html +='<td align="center" colspan="2" height="23"><input type="button" onclick="formSubmit(0)" value="OK"></td>';
	html +='</tr>';
	html +='</tbody>';
	html +='</table>';
	html +='</center>';
	html +='</form>';
	html +='</div>';
	html +='<div id="success_section" style="display: none;">';
	html +='<center><a href="" id="logoff_a">Logout</a></center>';
	html +='</div>';
	html +='<div id="logoff_section" style="display: none;">';
	html +='center><a href="" id="prelogin_a">Login</a></center>';
	divObj.innerHTML = html;
	document.body.appendChild(divObj);
	
	if (res == "notyet") {
		document.getElementById('chal').value = challenge;
		document.getElementById('uamip').value = uamip;
		document.getElementById('uamport').value = uamport;
		document.getElementById('userurl').value = userurl;
	}
	else if (res == "success") {
		if (userurl != null && userurl != undefined && userurl != '') {
			window.location = userurl;
		}
		else
		{
			document.getElementById("logoff_a").href = "http://" + uamip + ":" + uamport + "/logoff";
		}
	}
	else if (res == "failed") {
		document.getElementById('chal').value = challenge;
		document.getElementById('uamip').value = uamip;
		document.getElementById('uamport').value = uamport;
		document.getElementById('userurl').value = userurl;
	}
	else if (res == "logoff") {
		document.getElementById("prelogin_a").href = "http://" + uamip + ":" + uamport + "/prelogin";
		//document.getElementById('logoff_section').style.display = "block";
	}
	else if (res == "already") {
		document.getElementById("logoff_a").href = "http://" + uamip + ":" + uamport + "/logoff";
	}
	
});

function formSubmit(auth)
{
	var param = {};
	param.chal = document.getElementById('chal').value;
	param.uamip = document.getElementById('uamip').value;
	param.uamport = document.getElementById('uamport').value;
	param.userurl = document.getElementById('userurl').value;
	if (auth == 1) {
		param.UserName = document.getElementById('UserName').value;
		param.Password = document.getElementById('Password').value;
	}
	param.login = "login";

	$.ajax({
	    	url: '',
	  	data: param,
	      	type: 'GET',
	      	dataType: 'text',
		timeout: 20000,
		error: function(){
		      alert('fail');
		},
		success: function(data){
		      //- success
			window.location = data;
		}
	});

}

