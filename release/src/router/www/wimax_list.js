function show_4G_country_list(){
	countrylist = new Array();
	countrylist[0] = new Array("Russia", "RU");
	countrylist[1] = new Array("Manual", "");

	var got_country = 0;
	free_options($("isp_countrys"));
	$("isp_countrys").options.length = countrylist.length;
	for(var i = 0; i < countrylist.length; i++){
		$("isp_countrys").options[i] = new Option(countrylist[i][0], countrylist[i][1]);
		if(countrylist[i][1] == country){
			got_country = 1;
			$("isp_countrys").options[i].selected = "1";
		}
	}

	if(!got_country)
		$("isp_countrys").options[0].selected = "1";
}

function gen_4G_list(){
	country = $("isp_countrys").value;

	if(country == "RU"){
		isplist = new Array("Yota");
		apnlist = new Array("");
		daillist = new Array("");
		userlist = new Array("");
		passlist = new Array("");
	}
	else{
		isplist = new Array("");
		apnlist = new Array("");
		daillist = new Array("");
		userlist = new Array("");
		passlist = new Array("");
	}
}
