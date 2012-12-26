/* combined all protocol into this dattabase */
function gen_country_list(){
	countrylist = new Array();
	countrylist.push(["Australia", "AU"]);
	countrylist.push(["Brazil", "BZ"]);
	countrylist.push(["Bulgaria", "BUL"]);
	countrylist.push(["Canada", "CA"]);
	countrylist.push(["China", "CN"]);
	countrylist.push(["Czech", "CZ"]);
	countrylist.push(["Denmark", "DK"]);
	countrylist.push(["Dominican Republic", "DR"]);
	countrylist.push(["Egypt", "EG"]);
	countrylist.push(["El Salvador", "SV"]);
	countrylist.push(["Finland", "FI"]);
	countrylist.push(["Germany", "DE"]);
	countrylist.push(["Hong Kong", "HK"]);
	countrylist.push(["India", "INDI"]);
	countrylist.push(["Indonesia", "IN"]);
	countrylist.push(["Italy", "IT"]);
	countrylist.push(["Japan", "JP"]);
	countrylist.push(["Malaysia", "MA"]);
	countrylist.push(["Netherland", "NE"]);
	countrylist.push(["New Zealand", "NZ"]);
	countrylist.push(["Norway", "NO"]);
	countrylist.push(["Philippine", "PH"]);
	countrylist.push(["Poland", "POL"]);
	countrylist.push(["Portugal", "PO"]);
	countrylist.push(["Romania", "RO"]);
	countrylist.push(["Russia", "RU"]);
	countrylist.push(["Singapore", "SG"]);
	countrylist.push(["Slovakia", "SVK"]);
	countrylist.push(["South Africa", "SA"]);
	countrylist.push(["Spain", "ES"]);
	countrylist.push(["Sweden", "SE"]);
	countrylist.push(["Taiwan", "TW"]);
	countrylist.push(["Thiland", "TH"]);
	countrylist.push(["Turkey", "TR"]);
	countrylist.push(["UK", "UK"]);
	countrylist.push(["Ukraine", "UA"]);
	countrylist.push(["USA", "US"]);
	countrylist.push(["Vietnam", "VN"]);
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

function gen_list(){
	var country = $("isp_countrys").value;

	if(country == "AU"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("Telstra", "Optus", "Bigpond", "Hutchison 3G", "Vodafone", "iburst", "Dodo", "Exetel", "Internode", "Three", "Three PrePaid", "TPG", "Virgin", "A1", "3", "Orange", "T-Mobile Austria", "YESSS!");
		apnlist = new Array("telstra.internet", "Internet", "telstra.bigpond", "3netaccess", "vfprepaymbb", "internet", "DODOLNS1", "exetel1", "splns333a1", "3netaccess", "3services","internet", "VirginBroadband", "A1.net", "drei.at", "web.one.at", "gprsinternet", "web.yesss.at")
		daillist = new Array("*99#", "*99#","*99#", "*99***1#", "*99#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#")
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "", "", "", "ppp@a1plus.at", "", "web", "GPRS", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "", "", "", "ppp", "", "web", "", "");
	}
	else if(country == "CA"){
		protolist = new Array("1");
		isplist = new Array("Rogers");
		apnlist = new Array("internet.com");
		daillist = new Array("");
		userlist = new Array("wapuser1");
		passlist = new Array("wap");
	}
	else if(country == "CZ"){
		protolist = new Array("1", "1", "1", "2");
		isplist = new Array("T-mobile", "O2", "Vodafone", "Ufon");
		apnlist = new Array("internet.t-mobile.cz", "internet", "internet", "");
		daillist = new Array("*99#", "*99#", "*99***1#", "#777");
		userlist = new Array("gprs", "", "", "ufon");
		passlist = new Array("gprs", "", "", "ufon");
	}
	else if(country == "CN"){
		protolist = new Array("1", "3", "2");
		isplist = new Array("China Unicom", "China Mobile", "China Telecom");
		apnlist = new Array("3gnet", "cmnet", "ctnet");
		daillist = new Array("*99#", "*99#", "#777");
		userlist = new Array("", "net", "card");
		passlist = new Array("", "net", "card");
	}
	else if(country == "US"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "2", "1", "2");
		isplist = new Array("Telecom Italia Mobile", "Bell Mobility" ,"Cellular One" ,"Cincinnati Bell" ,"T-Mobile (T-Zone)" ,"T-Mobile (Internet)" ,"Verizon", "AT&T", "Sprint");
		apnlist = new Array("proxy", "", "cellular1wap", "wap.gocbw.com", "wap.voicestream.com", "internet2.voicestream.com", "", "broadband", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "#777", "*99***1#", "#777");
		userlist = new Array("", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "");
	}
	else if(country == "IT"){
		protolist = new Array("1", "1", "1", "1", "1");
		isplist = new Array("OTelecom Italia Mobile", "Vodafone", "TIM", "Wind", "Tre");
		apnlist = new Array("", "web.omnitel.it", "ibox.tim.it", "internet.wind", "tre.it");
		daillist = new Array("", "", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "");
		passlist = new Array("", "", "", "", "");
	}
	else if(country == "PO"){
		protolist = new Array("1", "1");
		isplist = new Array("TMN", "Optimus");
		apnlist = new Array("internet", "myconnection");
		daillist = new Array("*99#", "*99#");
		userlist = new Array("", "");
		passlist = new Array("", "");
	}
	else if(country == "UK"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("3", "O2", "Vodafone", "Orange");
		apnlist = new Array("orangenet", "m-bb.o2.co.uk", "PPBUNDLE.INTERNET", "internetvpn");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "o2bb", "web", "");
		passlist = new Array("", "password", "web", "");
	}
	else if(country == "DE"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("Vodafone", "T-mobile", "E-Plus", "o2 Germany", "vistream", "EDEKAmobil", "Tchibo", "mp3mobile", "Ring", "BILDmobil", "Alice", "Congstar", "klarmobil", "callmobile", "REWE", "simply", "Tangens", "ja!mobil", "penny", "Milleni.com", "PAYBACK", "smobil", "BASE", "Blau", "MEDION-Mobile", "simyo", "uboot", "vybemobile", "Aldi Talk", "o2", "o2 Prepaid", "Fonic", "igge&ko", "PTTmobile", "solomo", "sunsim", "telesim");
		apnlist = new Array("web.vodafone.de", "internet.t-mobile", "internet.eplus.de", "internet", "internet.vistream.net", "data.access.de", "webmobil1", "gprs.gtcom.de", "internet.ring.de", "access.vodafone.de", "internet.partner1", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "internet.t-mobile", "web.vodafone.de", "web.vodafone.de", "web.vodafone.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet", "pinternet.interkom.de", "pinternet.interkom.de", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net", "internet.vistream.net");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("vodafone", "tm", "eplus", "", "WAP", "", "", "", "", "", "", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "vodafone", "vodafone", "vodafone", "eplus", "eplus", "eplus", "eplus", "eplus", "eplus", "eplus", "", "", "", "WAP", "WAP", "WAP", "WAP", "WAP");
		passlist = new Array("nternet", "tm", "gprs", "", "Vistream", "", "", "", "", "", "", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "tm", "internet", "internet", "internet", "gprs", "gprs", "gprs", "gprs", "gprs", "gprs", "gprs", "", "", "", "Vistream", "Vistream", "Vistream", "Vistream", "Vistream");
	}
	else if(country == "IN"){
		protolist = new Array("1", "1", "1", "1", "1", "2", "1", "2", "2", "2", "2");
		isplist = new Array("IM2", "INDOSAT", "XL", "Telkomsel Flash", "3", "Smartfren", "Axis", "Esia", "StarOne", "Telkom Flexi", "AHA");
		apnlist = new Array("indosatm2", "indosat3g", "www.xlgprs.net", "flash", "3gprs", "", "AXIS", "", "", "", "AHA");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99***1#", "#777", "*99***1#", "#777", "#777", "#777", "#777");
		userlist = new Array("", "indosat", "xlgprs", "", "3gprs", "smart", "axis", "esia", "starone", "telkomnet@flexi", "aha@aha.co.id");
		passlist = new Array("", "indosat", "proxl", "", "3gprs", "smart", "123456", "esia", "indosat", "telkom", "aha");
	}
	else if(country == "MA"){
		protolist = new Array("1", "1", "1", "4");
		isplist = new Array("Celcom", "Maxis", "Digi", "Yes");
		apnlist = new Array("celcom3g", "unet", "3gdgnet", "");
		daillist = new Array("*99***1#", "*99***1#", "*99#", "");
		userlist = new Array("", "maxis", "", "");
		passlist = new Array("", "wap", "", "");
	}
	else if(country == "SG"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("M1", "Singtel", "StarHub", "Power Grid");
		apnlist = new Array("sunsurf", "internet", "shinternet", "");
		daillist = new Array("*99#", "*99#", "*99#", "*99***1#");
		userlist = new Array("65", "", "", "");
		passlist = new Array("user123", "", "", "");
	}
	else if(country == "PH"){
		protolist = new Array("1", "1", "1", "1", "1", "1");
		isplist = new Array("Globe Prepaid", "Globe Postpaid", "Smart Bro", "Smart Buddy", "Sun Prepaid", "Sun Postpaid");
		apnlist = new Array("http.globe.com.ph", "internet.globe.com.ph", "smartbro", "internet", "minternet", "fbband");
		daillist = new Array("*99***1#", "*99***1#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "");
	}
	else if(country == "SA"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Vodacom", "MTN", "Cell-c");
		apnlist = new Array("internet", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "HK"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("SmarTone-Vodafone", "3 Hong Kong", "One2Free", "PCCW mobile", "All 3G ISP support", "New World", "3HK", "CSL", "People", "Sunday");
		apnlist = new Array("internet", "mobile.three.com.hk", "internet", "pccw", "", "ineternet", "ipc.three.com.hk", "internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "");
	}
	else if(country == "TW"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "4");
		isplist = new Array("Far Eastern", "Far Eastern(fetims)", "Chunghua Telecom", "Taiwan Mobile", "Vibo", "Taiwan Cellular", "GMC");
		apnlist = new Array("internet", "fetims", "internet", "internet", "internet", "internet", "");
		daillist = new Array("*99#", "*99***1#", "*99***1#","*99#" ,"*99#" ,"*99***1#", "");
		userlist = new Array("", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "");
	}
	else if(country == "EG"){
		protolist = new Array("1");
		isplist = new Array("Etisalat");
		apnlist = new Array("internet");
		daillist = new Array("*99***1#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "DR"){
		protolist = new Array("1");
		isplist = new Array("Telmex");
		apnlist = new Array("internet.ideasclaro.com.do");
		daillist = new Array("*99#");
		userlist = new Array("claro");
		passlist = new Array("claro");
	}
	else if(country == "SV"){
		protolist = new Array("1");
		isplist = new Array("Telmex");
		apnlist = new Array("internet.ideasclaro");
		daillist = new Array("*99#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "NE"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("T-Mobile", "KPN", "Telfort", "Vodafone");
		apnlist = new Array("internet", "internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "RU"){
		protolist = new Array("1", "1", "1", "1", "4");
		isplist = new Array("BeeLine", "Megafon", "MTS", "PrimTel", "Yota");
		apnlist = new Array("internet.beeline.ru", "internet.nw", "internet.mts.ru", "internet.primtel.ru", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "");
		userlist = new Array("beeline", "", "", "", "");
		passlist = new Array("beeline", "", "", "", "");
	}
  else if(country == "UA"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "2", "1", "2", "2", "2", "1", "2", "4", "4");
		isplist = new Array("BeeLine", "Kyivstar Contract", "Kyivstar Prepaid", "Kyivstar XL", "Kyivstart 3G", "Djuice", "MTS", "Utel", "PEOPLEnet", "Intertelecom", "Intertelecom.Rev.B", "Life", "CDMA-UA", "FreshTel", "Giraffe");
		apnlist = new Array("internet.beeline.ua", "www.kyivstar.net", "www.ab.kyivstar.net", "xl.kyivstar.net", "3g.kyivstar.net", "www.djuice.com.ua", "", "3g.utel.ua", "", "", "", "Internet", "", "", "");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "#777", "*99#", "#777", "#777", "#777", "*99#", "#777", "", "");
		userlist = new Array("mobile", "", "", "", "", "", "mobile", "", "", "IT", "3G_TURBO", "", "", "freshtel", "");
		passlist = new Array("internet", "", "", "", "", "", "internet", "", "000000", "IT", "3G_TURBO", "", "", "freshtel", "");
  }
	else if(country == "TH"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("AIS(TH GSM)", "DTAC", "Truemove H");
		apnlist = new Array("internet", "www.dtac.co.th", "hinternet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "true");
		passlist = new Array("", "", "true");
	}
  else if(country == "POL"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1");
  	isplist = new Array("Play", "Cyfrowy Polsat", "ERA", "Orange", "Plus", "Heyah", "Aster");
    apnlist = new Array("internet", "multi.internet", "internet", "internet", "internet", "internet", "aster.internet");
    daillist = new Array("*99#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99#");
    userlist = new Array("", "", "erainternet", "internet", "plusgsm", "heyah", "internet");
    passlist = new Array("", "", "erainternet", "internet", "plusgsm", "heyah", "internet");
  }
	else if(country == "INDI"){
		protolist = new Array("2", "2", "2", "1", "1", "1");
		isplist = new Array("Reliance", "Tata", "MTS", "Airtel", "Idea", "MTNL");
		apnlist = new Array("reliance", "TATA", "", "airtelgprs.com", "Internet", "gprsppsmum");
		daillist = new Array("#777", "#777", "#777", "*99#", "*99#", "*99#");
		userlist = new Array("", "internet", "internet@internet.mtsindia.in", "", "", "mtnl");
		passlist = new Array("", "internet", "mts", "", "", "mtnl123");
	}
  else if(country == "BZ"){
		protolist = new Array("1", "1", "1", "1");
    isplist = new Array("Vivo", "Tim", "Oi", "Claro");
    apnlist = new Array("zap.vivo.com.br", "tim.br", "gprs.oi.com.br", "bandalarga.claro.com.br");
    daillist = new Array("*99#", "*99#", "*99***1#", "*99***1#");
    userlist = new Array("vivo", "tim", "oi", "claro");
    passlist = new Array("vivo", "tim", "oi", "claro");
  }
  else if(country == "RO"){
		protolist = new Array("1", "1", "1", "1");
    isplist = new Array("Vodafone", "Orange", "Cosmote", "RCS-RDS");
    apnlist = new Array("internet.vodafone.ro", "internet", "broadband", "internet");
    daillist = new Array("*99#", "*99#", "*99#", "*99#");
    userlist = new Array("internet.vodafone.ro", "", "", "");
    passlist = new Array("vodafone", "", "", "");
  }
  else if(country == "BUL"){
		protolist = new Array("1", "1", "1");
    isplist = new Array("Globul", "Mtel", "Vivacom");
    apnlist = new Array("internet.globul.bg", "inet-gprs.mtel.bg", "internet.vivatel.bg");
    daillist = new Array("359891000", "359881000", "359871000");
    userlist = new Array("globul", "", "vivatel");
    passlist = new Array("", "", "vivatel");
  }
	else if(country == "NZ"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("CHT", "Vodafone NZ", "2 Degrees");
		apnlist = new Array("internet", "www.vodafone.net.nz", "2degrees");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "FI"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("DNA", "Saunalahti", "Elisa", "Sonera/TeleFinland", "AinaCom/Sonera", "AinaCom/DNA", "TDC/DNA", "Dicame");
		apnlist = new Array(["dna internet&&internet", "dna mms&&mms"], ["Saunalahti Internet&&internet.saunalahti", "Saunalahti MMS&&mms.saunalahti.fi"], ["Elisa Internet&&internet", "Elisa MMS&&mms"], ["Sonera&&internet", "Sonera MMS&&wap.sonera.net"], ["Sonera&&internet", "MMS&&mediapiste"], ["Aina internet&&internet.aina.fi", "Aina mms&&mms.aina.fi"], ["TDC internet&&inet.tdc.fi", "TDC MMS&&mms.tdc.fi"], ["Dicame Internet&&internet.dicame.fi", "Dicame MMS&&mms.dicame.fi"]);
		daillist = new Array("*99#", "*99#", "*99#", "", "", "", "", "");
		userlist = new Array("", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "");
	}
	else if(country == "SE"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("3", "Telia", "Telenor", "Tele2", "Tre", "Glocalnet", "Halebop", "Ventelo", "Spring Mobile", "UVTC (Universal Telecom)", "TDC", "Cellip");
		apnlist = new Array("data.tre.se", ["Telia SE Internet&&online.telia.se", "Telia SE MMS&&mms.telia.se"], ["Telenor Data&&internet.telenor.se", "Telenor MMS&&services.telenor.se"], ["Tele2 Internet&&internet.tele2.se", "Tele2 MMS&&internet.tele2.se"], "data.tre.se", ["Glocalnet Data&&internet.glocalnet.se", "Glocalnet MMS&&services.glocalnet.se"], ["Halebop Online&&halebop.telia.se", "Halebop MMS&&mms.telia.se"], ["Ventelo Internet&&internet.ventelo.se", "Ventelo MMS&&mms.ventelo.se"], ["Spring data&&data.springmobil.se", "Spring MMS&&mms.springmobil.se"], "-services sp", ["TDC SE&&internet.se", "mms&&mms"], "-services sp");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "", "", "", "", "", "", "", "");
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "SVK"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("O2", "Orange", "T-mobile");
		apnlist = new Array("o2internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99#", "*99***1#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "NO"){
		protolist = new Array("1", "1", "1", "1", "1", "1");
		isplist = new Array("Tele2", "NetCom", "Chess", "Telenor", "Network Norway", "One Call");
		apnlist = new Array(["Tele2&&internet.tele2.no"], ["NetCom internet&&netcom", "NetCom MMS&&netcom", "NetCom Mobile&&netcom"], ["Chess WAP&&netcom", "Chess MMS&&mms.netcom.no"], ["Telenor Internet&&telenor", "Telenor MMS&&telenor"], ["NWN Internet&&internet", "NWN MMS&&mms"], ["One Call Internet&&internet", "One Call MMS&&mms"]);
		daillist = new Array("*99#", "*99#", "", "*99#", "", "");
		userlist = new Array("", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "");
	}
	else if(country == "DK"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("3", "TDC", "Orange", "Telia", "Telenor", "Tre", "Callme", "Onfone", "CBB", "Oister", "Bibob", "Happii mobil");
		apnlist = new Array("data.tre.dk", ["TDC&&internet", "TDC MMS&&mms"], "web.orange.dk", ["Telia Mobile Data&&www.internet.mtelia.dk", "Telia MMS&&www.mms.mtelia.dk"], ["Telenor&&Internet", "Telenor MMS&&mms"], ["Tre Internet&&data.tre.dk", "Tre MMS&&Tre MMS"], ["Callme Internet&&websp", "Callme MMS&&mmssp"], ["Onfone GPRS&&internet.sp.dk", "Onfone MMS&&mms.sp.dk"], ["Telenor internet&&internet", "Telenor MMS&&Telenor"], ["Oister&&data.dk", "OisterMMS&&data.dk"], ["Bibob Internet&&internet.bibob.dk", "Bibob MMS&&mms.bibob.dk"], ["Happii Internet&&internet", "Happii MMS&&mms"]);
		daillist = new Array("*99#", "*99#", "*99#", "", "", "", "", "", "", "", "", "");
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "ES"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("Vodafone", "Movistar", "Simyo", "Ono");
		apnlist = new Array("airtelnet.es", "movistar.es", "gprs-service.com", "internet.ono.com");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("vodafone", "movistar", "", "");
		passlist = new Array("vodafone", "movistar", "", "");
	}
	else if(country == "JP"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Softbank", "b-mobile", "AU");
		apnlist = new Array("emb.ne.jp", "dm.jplat.net", "au.NET");
		daillist = new Array("*99***1#", "*99***1#", "*99**24#");
		userlist = new Array("em", "bmobile@u300", "au@au-win.ne.jp");
		passlist = new Array("em", "bmobile", "au");
	}
  else if(country == "TR"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Turkcell Vinn", "Vodafone Vodem", "Avea Jet");
		apnlist = new Array("mgb", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
  }
	else if(country == "VN"){
		protolist = new Array("1", "1", "1");
                isplist = new Array("Mobifone(Fast Connect)", "Vinaphone(ezCom)", "Viettel(D-Com 3G)");
                apnlist = new Array("m-wap", "m3-card", "e-connect");
                daillist = new Array("*99#", "*99#", "*99#");
                userlist = new Array("mms", "mms", "");
                passlist = new Array("mms", "mms", "");
	}
	else{
		isplist = new Array("");
		apnlist = new Array("");
		daillist = new Array("");
		userlist = new Array("");
		passlist = new Array("");
	}
}
