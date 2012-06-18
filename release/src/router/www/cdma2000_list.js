function show_cdma2000_country_list(){
	countrylist = new Array();
	countrylist[0] = new Array("China", "CN");
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

function gen_cdma2000_list(){
	var country = $("isp_countrys").value;

	if(country == "CN"){
		isplist = new Array("China Telecom");
		apnlist = new Array("ctnet");
		daillist = new Array("#777");
		userlist = new Array("card");
		passlist = new Array("card");
	}
	else{
		isplist = new Array("");
		apnlist = new Array("");
		daillist = new Array("#777");
		userlist = new Array("");
		passlist = new Array("");
	}
}
