/*
Old naming rule:
1. "-" to ""
2. "+" to "Plus"
*/

function supportsite_model(support_site_modelid, hwver){
	
var real_model_name = "";
real_model_name = support_site_modelid.replace("-", "");
real_model_name = real_model_name.replace("+", "Plus");

if(support_site_modelid.search("RT-N12E_B") != -1)    //RT-N12E_B or RT-N12E_B1
	real_model_name = "RTN12E_B1";
else if(support_site_modelid == "RT-AC1900P") //MODELDEP : RT-AC1900P
	real_model_name = "RT-AC1900P";
else if(support_site_modelid =="RT-N12" || hw_ver.search("RTN12") != -1){	//MODELDEP : RT-N12
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

}
else if(support_site_modelid == "RT-N56UB1"){	//MODELDEP : RT-N56UB1
	real_model_name = "RTN56U_B1";
}
else if(support_site_modelid == "RT-N56UB2"){	//MODELDEP : RT-N56UB2
	real_model_name = "RTN56U_B2";
}
else if(support_site_modelid == "DSL-N55U"){	//MODELDEP : DSL-N55U
	real_model_name = "DSLN55U_Annex_A";
}	
else if(support_site_modelid == "DSL-N55U-B"){	//MODELDEP : DSL-N55U-B
	real_model_name = "DSLN55U_Annex_B";
}	
else if(support_site_modelid == "RT-AC68R"){	//MODELDEP : RT-AC68R
	real_model_name = "RT-AC68R";
}
else if(support_site_modelid == "RT-AC66U_B1"){    //MODELDEP : RT-AC66U B1
        real_model_name = "RT-AC66U-B1";
}
else if(support_site_modelid == "RT-N66U_C1"){	//MODELDEP : RT-N66U_C1
	real_model_name = "RT-N66U-C1";
}

return real_model_name;
}	

	
