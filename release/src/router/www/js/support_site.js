/*
New naming rule:
1. " " "/" "-" "_" "&" "!" to "-"
2. "+" to "-plus"
*/

function supportsite_model(modelname, odmpid, hwver){
	
var real_model_name = "";
real_model_name = modelname.replace(" ", "-");
real_model_name = real_model_name.replace("/", "-");
real_model_name = real_model_name.replace("_", "-");
real_model_name = real_model_name.replace("&", "-");
real_model_name = real_model_name.replace("!", "-");
real_model_name = real_model_name.replace("+", "-plus");

return real_model_name;
}	
		