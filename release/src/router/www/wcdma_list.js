function show_wcdma_country_list(){
	countrylist = new Array();
	countrylist.push(["Australia", "AU"]);
	countrylist.push(["Bulgaria", "BUL"]);
	countrylist.push(["Brazil", "BZ"]);
	countrylist.push(["Canada", "CA"]);
	countrylist.push(["China", "CN"]);
	countrylist.push(["Czech", "CZ"]);
	countrylist.push(["Dominican Republic", "DR"]);
	countrylist.push(["Denmark", "DK"]);
	countrylist.push(["Germany", "DE"]);
	countrylist.push(["Egypt", "EG"]);
	countrylist.push(["El Salvador", "SV"]);
	countrylist.push(["Finland", "FI"]);
	countrylist.push(["Hong Kong", "HK"]);
	countrylist.push(["Indonesia", "IN"]);
	countrylist.push(["India", "INDI"]);
	countrylist.push(["Italy", "IT"]);
	countrylist.push(["Japan", "JP"]);
	countrylist.push(["Malaysia", "MA"]);
	countrylist.push(["Netherland", "NE"]);
	countrylist.push(["Norway", "NO"]);
	countrylist.push(["New Zealand", "NZ"]);
	countrylist.push(["Portugal", "PO"]);
	countrylist.push(["Poland", "POL"]);
	countrylist.push(["Philippine", "PH"]);
	countrylist.push(["Romania", "RO"]);
	countrylist.push(["Russia", "RU"]);
	countrylist.push(["Singapore", "SG"]);
	countrylist.push(["Slovakia", "SVK"]);
	countrylist.push(["South Africa", "SA"]);
	countrylist.push(["Spain", "ES"]);
	countrylist.push(["Sweden", "SE"]);
	countrylist.push(["Thiland", "TH"]);
	countrylist.push(["Turkey", "TR"]);
	countrylist.push(["Taiwan", "TW"]);
	countrylist.push(["Ukraine", "UA"]);
	countrylist.push(["UK", "UK"]);
	countrylist.push(["USA", "US"]);
	countrylist.push(["Manual", ""]);

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

function gen_wcdma_list(){
	var country = $("isp_countrys").value;

	if(country == "AU"){
		isplist = new Array("Telstra", "Optus", "Bigpond", "Hutchison 3G", "Vodafone", "iburst", "Dodo", "Exetel", "Internode", "Three", "Three PrePaid", "TPG", "Virgin", "A1", "3", "Orange", "T-Mobile Austria", "YESSS!");
		apnlist = new Array("telstra.internet", "Internet", "telstra.bigpond", "3netaccess", "vfprepaymbb", "internet", "DODOLNS1", "exetel1", "splns333a1", "3netaccess", "3services","internet", "VirginBroadband", "A1.net", "drei.at", "web.one.at", "gprsinternet", "web.yesss.at")
		daillist = new Array("*99#", "*99#","*99#", "*99***1#", "*99#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#")
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "", "", "", "ppp@a1plus.at", "", "web", "GPRS", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "", "", "", "ppp", "", "web", "", "");
	}
	else if(country == "CA"){
		isplist = new Array("Rogers");
		apnlist = new Array("internet.com");
		daillist = new Array("");
		userlist = new Array("wapuser1");
		passlist = new Array("wap");
	}
	else if(country == "CZ"){
		isplist = new Array("T-mobile", "O2", "Vodafone", "Ufon");
		apnlist = new Array("internet.t-mobile.cz", "internet", "internet", "");
		daillist = new Array("*99#", "*99#", "*99***1#", "#777");
		userlist = new Array("gprs", "", "", "ufon");
		passlist = new Array("gprs", "", "", "ufon");
	}
	else if(country == "CN"){
		isplist = new Array("China Unicom", "China Mobile", "China Telecom");
		apnlist = new Array("3gnet", "cmnet", "ctnet");
		daillist = new Array("*99#", "*99#", "#777");
		userlist = new Array("", "net", "card");
		passlist = new Array("", "net", "card");
	}
	else if(country == "US"){
		isplist = new Array("Telecom Italia Mobile", "Bell Mobility" ,"Cellular One" ,"Cincinnati Bell" ,"T-Mobile (T-Zone)" ,"T-Mobile (Internet)" ,"Verizon", "AT&T", "Sprint");
		apnlist = new Array("proxy", "", "cellular1wap", "wap.gocbw.com", "wap.voicestream.com", "internet2.voicestream.com", "", "broadband", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "#777", "*99***1#", "#777");
		userlist = new Array("", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "");
	}
	else if(country == "IT"){
		isplist = new Array("OTelecom Italia Mobile", "Vodafone", "TIM", "Wind", "Tre");
		apnlist = new Array("", "web.omnitel.it", "ibox.tim.it", "internet.wind", "tre.it");
		daillist = new Array("", "", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "");
		passlist = new Array("", "", "", "", "");
	}
	else if(country == "PO"){
		isplist = new Array("Optimus", "TMN", "Optimus");
		apnlist = new Array("", "internet", "myconnection");
		daillist = new Array("", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "UK"){
		isplist = new Array("3", "O2", "Vodafone", "Orange");
		apnlist = new Array("orangenet", "m-bb.o2.co.uk", "PPBUNDLE.INTERNET", "internetvpn");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "o2bb", "web", "");
		passlist = new Array("", "password", "web", "");
	}
	else if(country == "DE"){
		isplist = new Array("Vodafone", "T-mobile", "E-Plus", "o2 Germany", "vistream", "EDEKAmobil", "Tchibo", "mp3mobile", "Ring", "BILDmobil", "Alice", "Congstar", "klarmobil", "callmobile", "REWE", "simply", "Tangens", "ja!mobil", "penny", "Milleni.com", "PAYBACK", "smobil", "BASE", "Blau", "MEDION-Mobile", "simyo", "uboot", "vybemobile", "Aldi Talk", "o2", "o2 Prepaid", "Fonic", "igge&ko", "PTTmobile", "solomo", "sunsim", "telesim");
		apnlist = new Array("web.vodafone.de", "internet.t-mobile", "internet.eplus.de", "internet", "internet.vistream.net", "data.access.de", "webmobil1", "gprs.gtcom.de", "internet.ring.de", "access.vodafone.de", "internet.partner1", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "web.vodafone.de", "web.vodafone.de", "web.vodafone.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet", "pinternet.interkom.de", "pinternet.interkom.de", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("vodafone", "tm", "eplus", "", "WAP", "", "", "", "", "", "", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "vodafone", "vodafone", "vodafone", "eplus", "eplus", "eplus", "eplus", "eplus", "eplus", "eplus", "", "", "", "WAP", "WAP", "WAP", "WAP", "WAP");
		passlist = new Array("nternet", "tm", "gprs", "", "Vistream", "", "", "", "", "", "", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "internet", "internet", "internet", "gprs", "gprs", "gprs", "gprs", "gprs", "gprs", "gprs", "", "", "", "Vistream", "Vistream", "Vistream", "Vistream", "Vistream");
	}
	else if(country == "IN"){
		isplist = new Array("IM2", "INDOSAT", "XL", "Telkomsel Flash", "3", "Smartfren", "Axis", "Esia", "StarOne", "Telkom Flexi", "AHA");
		apnlist = new Array("indosatm2", "indosat3g", "www.xlgprs.net", "flash", "3gprs", "", "AXIS", "", "", "", "AHA");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99***1#", "#777", "*99***1#", "#777", "#777", "#777", "#777");
		userlist = new Array("", "indosat", "xlgprs", "", "3gprs", "smart", "axis", "esia", "starone", "telkomnet@flexi", "aha@aha.co.id");
		passlist = new Array("", "indosat", "proxl", "", "3gprs", "smart", "123456", "esia", "indosat", "telkom", "aha");
	}
	else if(country == "MA"){
		isplist = new Array("Celcom", "Maxis", "Digi");
		apnlist = new Array("celcom3g", "unet", "3gdgnet");
		daillist = new Array("*99***1#", "*99***1#", "*99#");
		userlist = new Array("", "maxis", "");
		passlist = new Array("", "wap", "");
	}
	else if(country == "SG"){
		isplist = new Array("M1", "Singtel", "StarHub", "Power Grid");
		apnlist = new Array("sunsurf", "internet", "shinternet", "");
		daillist = new Array("*99#", "*99#", "*99#", "*99***1#");
		userlist = new Array("65", "", "", "");
		passlist = new Array("user123", "", "", "");
	}
	else if(country == "PH"){
		isplist = new Array("Globe", "Smart", "Sun Cellula");
		apnlist = new Array("internet.globe.com.ph", "internet", "minternet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "SA"){
		isplist = new Array("Vodacom", "MTN", "Cell-c");
		apnlist = new Array("internet", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "HK"){
		isplist = new Array("SmarTone-Vodafone", "3 Hong Kong", "One2Free", "PCCW mobile", "All 3G ISP support", "New World", "3HK", "CSL", "People", "Sunday");
		apnlist = new Array("internet", "mobile.three.com.hk", "internet", "pccw", "", "ineternet", "ipc.three.com.hk", "internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "");
	}
	else if(country == "TW"){
		isplist = new Array("Far Eastern", "Far Eastern(fetims)", "Chunghua Telecom", "Taiwan Mobile", "Vibo", "Taiwan Cellular", "GMC");
		apnlist = new Array("internet", "fetims", "internet", "internet", "internet", "internet", "");
		daillist = new Array("*99#", "*99***1#", "*99***1#","*99#" ,"*99#" ,"*99***1#", "");
		userlist = new Array("", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "");
	}
	else if(country == "EG"){
		isplist = new Array("Etisalat");
		apnlist = new Array("internet");
		daillist = new Array("*99***1#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "DR"){
		isplist = new Array("Telmex");
		apnlist = new Array("internet.ideasclaro.com.do");
		daillist = new Array("*99#");
		userlist = new Array("claro");
		passlist = new Array("claro");
	}
	else if(country == "SV"){
		isplist = new Array("Telmex");
		apnlist = new Array("internet.ideasclaro");
		daillist = new Array("*99#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "NE"){
		isplist = new Array("T-Mobile", "KPN", "Telfort", "Vodafone");
		apnlist = new Array("internet", "internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "RU"){
		isplist = new Array("BeeLine", "Megafon", "MTS", "PrimTel", "Yota");
		apnlist = new Array("internet.beeline.ru", "internet.nw", "internet.mts.ru", "internet.primtel.ru", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "");
		userlist = new Array("beeline", "", "", "", "");
		passlist = new Array("beeline", "", "", "", "");
	}
  else if(country == "UA"){
    isplist = new Array("BeeLine", "Kyivstar Contract", "Kyivstar Prepaid", "Kyivstar XL", "Kyivstart 3G", "Djuice", "MTS", "Utel", "PEOPLEnet", "Intertelecom", "Intertelecom.Rev.B", "Life", "CDMA-UA", "FreshTel");
    apnlist = new Array("internet.beeline.ua", "www.kyivstar.net", "www.ab.kyivstar.net", "xl.kyivstar.net", "3g.kyivstar.net", "www.djuice.com.ua", "", "3g.utel.ua", "", "", "", "Internet", "", "");
    daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "#777", "*99#", "#777", "#777", "#777", "*99#", "#777", "");
    userlist = new Array("mobile", "", "", "", "", "", "mobile", "", "", "IT", "3G_TURBO", "", "", "freshtel");
    passlist = new Array("internet", "", "", "", "", "", "internet", "", "000000", "IT", "3G_TURBO", "", "", "freshtel");
  }
	else if(country == "TH"){
		isplist = new Array("TOT", "TH GSM");
		apnlist = new Array("internet", "internet");
		daillist = new Array("", "*99#");
		userlist = new Array("", "");
		passlist = new Array("", "");
	}
  else if(country == "POL"){
  	isplist = new Array("Play", "Cyfrowy Polsat", "ERA", "Orange", "Plus", "Heyah", "Aster");
    apnlist = new Array("internet", "multi.internet", "internet", "internet", "internet", "internet", "aster.internet");
    daillist = new Array("*99#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99#");
    userlist = new Array("", "", "erainternet", "internet", "plusgsm", "heyah", "internet");
    passlist = new Array("", "", "erainternet", "internet", "plusgsm", "heyah", "internet");
  }
	else if(country == "INDI"){
		isplist = new Array("Reliance", "Tata", "MTS", "Airtel", "Idea", "MTNL");
		apnlist = new Array("reliance", "TATA", "", "airtelgprs.com", "Internet", "gprsppsmum");
		daillist = new Array("#777", "#777", "#777", "*99#", "*99#", "*99#");
		userlist = new Array("", "internet", "internet@internet.mtsindia.in", "", "", "mtnl");
		passlist = new Array("", "internet", "mts", "", "", "mtnl123");
	}
  else if(country == "BZ"){
    isplist = new Array("Vivo", "Tim", "Oi", "Claro");
    apnlist = new Array("zap.vivo.com.br", "tim.br", "gprs.oi.com.br", "bandalarga.claro.com.br");
    daillist = new Array("*99#", "*99#", "*99***1#", "*99***1#");
    userlist = new Array("vivo", "tim", "oi", "claro");
    passlist = new Array("vivo", "tim", "oi", "claro");
  }
  else if(country == "RO"){
    isplist = new Array("Vodafone", "Orange", "Cosmote", "RCS-RDS");
    apnlist = new Array("internet.vodafone.ro", "internet", "broadband", "internet");
    daillist = new Array("*99#", "*99#", "*99#", "*99#");
    userlist = new Array("internet.vodafone.ro", "", "", "");
    passlist = new Array("vodafone", "", "", "");
  }
  else if(country == "BUL"){
    isplist = new Array("Globul", "Mtel", "Vivacom");
    apnlist = new Array("internet.globul.bg", "inet-gprs.mtel.bg", "internet.vivatel.bg");
    daillist = new Array("359891000", "359881000", "359871000");
    userlist = new Array("globul", "", "vivatel");
    passlist = new Array("", "", "vivatel");
  }
	else if(country == "NZ"){
		isplist = new Array("CHT", "Vodafone NZ", "2 Degrees");
		apnlist = new Array("internet", "www.vodafone.net.nz", "2degrees");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "FI"){
		isplist = new Array("Sonera", "Elisa", "Saunalahti", "DNA");
		apnlist = new Array("internet", "internet", "internet.saunalahti", "internet");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "SE"){
		isplist = new Array("3", "Telia", "Telenor", "Tele2");
		apnlist = new Array("data.tre.se", "online.telia.se", "internet.telenor.se", "internet.tele2.se");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "SVK"){
		isplist = new Array("O2", "Orange", "T-mobile");
		apnlist = new Array("o2internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99#", "*99***1#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "NO"){
		isplist = new Array("Tele2", "Telenor", "Netcom");
		apnlist = new Array("internet.tele2.se", "Telenor", "internet.netcom.no");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "DK"){
		isplist = new Array("3", "TDC", "Orange");
		apnlist = new Array("data.tre.dk", "internet", "web.orange.dk");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "ES"){
		isplist = new Array("Vodafone", "Movistar", "Simyo", "Ono");
		apnlist = new Array("airtelnet.es", "movistar.es", "gprs-service.com", "internet.ono.com");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("vodafone", "movistar", "", "");
		passlist = new Array("vodafone", "movistar", "", "");
	}
	else if(country == "JP"){
		isplist = new Array("Softbank", "b-mobile", "AU");
		apnlist = new Array("emb.ne.jp", "dm.jplat.net", "au.NET");
		daillist = new Array("*99***1#", "*99***1#", "*99**24#");
		userlist = new Array("em", "bmobile@u300", "au@au-win.ne.jp");
		passlist = new Array("em", "bmobile", "au");
	}
  else if(country == "TR"){
                isplist = new Array("Turkcell Vinn", "Vodafone Vodem", "Avea Jet");
                apnlist = new Array("mgb", "internet", "internet");
                daillist = new Array("*99#", "*99#", "*99#");
                userlist = new Array("", "", "");
                passlist = new Array("", "", "");
  }
	else{
		isplist = new Array("");
		apnlist = new Array("");
		daillist = new Array("");
		userlist = new Array("");
		passlist = new Array("");
	}
}
