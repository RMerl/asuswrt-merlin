/*
Old naming rule:
1. "-" to ""
2. "+" to "Plus"
*/

function supportsite_model(modelname, odmpid, hwver){
	
var real_model_name = "";
real_model_name = modelname.replace("-", "");
real_model_name = real_model_name.replace("+", "Plus");	

if(based_modelid =="RT-N12" || hw_ver.search("RTN12") != -1){	//MODELDEP : RT-N12
	if( hw_ver.search("RTN12HP") != -1){	//RT-N12HP
		real_model_name = "RTN12HP";
	}
	else if(hw_ver.search("RTN12B1") != -1){ //RT-N12B1
		real_model_name = "RTN12_B1";
	}
	else if(hw_ver.search("RTN12C1") != -1){ //RT-N12C1
		real_model_name = "RTN12_C1";
	}	
	else if(hw_ver.search("RTN12D1") != -1){ //RT-N12D1
		real_model_name = "RTN12_D1";
	}	
	else{	//RT-N12 else
		real_model_name = real_model_name;
	}
}
else if(based_modelid == "RT-N56UB1"){	//MODELDEP : RT-N56UB1
		real_model_name = "RTN56U_B1";
}
else if(based_modelid == "RT-N56UB2"){	//MODELDEP : RT-N56UB2
		real_model_name = "RTN56U_B2";
}
else if(based_modelid == "DSL-N55U"){	//MODELDEP : DSL-N55U
		real_model_name = "DSLN55U_Annex_A";
}	
else if(based_modelid == "DSL-N55U-B"){	//MODELDEP : DSL-N55U-B
		real_model_name = "DSLN55U_Annex_B";
}	
else if(based_modelid == "RT-AC68R"){	//MODELDEP : RT-AC68R
		real_model_name = "RT-AC68R";
}	
else{
	if(odmpid.search("RT-N12E_B") != -1)	//RT-N12E_B or RT-N12E_B1
		real_model_name = "RTN12E_B1";
	else if(odmpid == "RT-AC1900P") //MODELDEP : RT-AC1900P
		real_model_name = "RT-AC1900P";
}

return real_model_name;
}	

	
