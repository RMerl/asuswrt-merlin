// JavaScript Document 2007_12_20 Add by lock
function add_options_x2(o, arr, varr, orig){
	free_options(o);
	
	for(var i = 0; i < arr.length; ++i){
		if(orig == varr[i])
			add_option(o, arr[i], varr[i], 1);
		else
			add_option(o, arr[i], varr[i], 0);
	}
}
