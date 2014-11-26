// Form validation and smooth UI

function combineIP(obj){ //combine all IP info before validation and submit
	
	var IP_List = document.getElementById(obj+"_div").childNodes;
	var IP_str = "";
	
	for(var i=0; i < IP_List.length; i++){	
		//alert("\'"+ IP_List[i].nodeValue+ "\'");
		if(IP_List[i].type == "text"){
			if(IP_List[i].value != ""){
				IP_List[i].value = parseInt(IP_List[i].value,10);
			}
			
			IP_str += IP_List[i].value;
		}
		else if(IP_List[i].nodeValue.indexOf(".") != -1){
			IP_str += ".";                                  
		}
	}
	if(IP_str != "..."){
		document.getElementById(obj).value = IP_str;
	}else{
		document.getElementById(obj).value = "";
	}
}

function IPinputCtrl(obj, t){
	var IP_List = document.getElementById(obj.name+"_div").childNodes;
	
	document.getElementById(obj.name+"_div").style.background = (t==0)?"#999999":"#FFFFFF";
	for(var i=0; i < IP_List.length; i++){
		if(IP_List[i].type == "text"){
			IP_List[i].disabled = (t==0)?true:false;
			IP_List[i].style.color = (t==0)?"#FFFFFF":"#000000";
			IP_List[i].style.background = (t==0)?"#999999":"#FFFFFF";
		}
	}
}
