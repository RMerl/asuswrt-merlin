/*
New naming rule:
1. " " "/" "-" "_" "&" "!" to "-"
2. "+" to "-plus"
*/

function supportsite_model(support_site_modelid, hwver){
	
var real_model_name = "";
real_model_name = support_site_modelid.replace(" ", "-");
real_model_name = real_model_name.replace("/", "-");
real_model_name = real_model_name.replace("_", "-");
real_model_name = real_model_name.replace("&", "-");
real_model_name = real_model_name.replace("!", "-");
real_model_name = real_model_name.replace("+", "-plus");

return real_model_name;
}	
		
