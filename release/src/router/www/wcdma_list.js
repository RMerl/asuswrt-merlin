/* combined all protocol into this database */
function gen_country_list(){
	countrylist = new Array();
	countrylist.push(["Others", ""]);
	countrylist.push(["Australia", "AU"]);
	countrylist.push(["Austria", "AT"]);
	countrylist.push(["Bosnia and Herzegovina", "BA"]);
	countrylist.push(["Brazil", "BR"]);
	countrylist.push(["Brunei", "BN"]);
	countrylist.push(["Bulgaria", "BG"]);
	countrylist.push(["Canada", "CA"]);
	countrylist.push(["China", "CN"]);
	countrylist.push(["Czech", "CZ"]);
	countrylist.push(["Denmark", "DK"]);
	countrylist.push(["Dominican Republic", "DO"]);
	countrylist.push(["Egypt", "EG"]);
	countrylist.push(["Finland", "FI"]);
	countrylist.push(["Germany", "DE"]);
	countrylist.push(["Greece", "GR"]);
	countrylist.push(["Hong Kong", "HK"]);
	countrylist.push(["India", "IN"]);
	countrylist.push(["Indonesia", "ID"]);
	countrylist.push(["Israel", "IL"]);
	countrylist.push(["Italy", "IT"]);
	countrylist.push(["Japan", "JP"]);
	countrylist.push(["Malaysia", "MY"]);
	countrylist.push(["Netherlands", "NL"]);
	countrylist.push(["New Zealand", "NZ"]);
	countrylist.push(["Norway", "NO"]);
	countrylist.push(["Philippines", "PH"]);
	countrylist.push(["Poland", "PL"]);
	countrylist.push(["Portugal", "PT"]);
	countrylist.push(["Romania", "RO"]);
	countrylist.push(["Russia", "RU"]);
	countrylist.push(["Singapore", "SG"]);
	countrylist.push(["Slovakia", "SK"]);
	countrylist.push(["South Africa", "ZA"]);
	countrylist.push(["Spain", "ES"]);
	countrylist.push(["Sweden", "SE"]);
	countrylist.push(["Taiwan", "TW"]);
	countrylist.push(["Thailand", "TH"]);
	countrylist.push(["Turkey", "TR"]);
	countrylist.push(["Ukraine", "UA"]);
	countrylist.push(["United Kingdom", "UK"]);
	countrylist.push(["USA", "US"]);
	countrylist.push(["Vietnam", "VN"]);

	var got_country = 0;
	free_options(document.form.modem_country);
	document.form.modem_country.options.length = countrylist.length;
	for(var i = 0; i < countrylist.length; i++){
		document.form.modem_country.options[i] = new Option(countrylist[i][0], countrylist[i][1]);
		if(countrylist[i][1] == country){
			got_country = 1;
			document.form.modem_country.options[i].selected = "1";
		}
	}

	if(!got_country)
		document.form.modem_country.options[0].selected = "1";
}

// 1:WCDMA  2:CMDA2000  3.TD-SCDMA  4.WiMAX
function gen_list(){
	var country = document.form.modem_country.value;

	if(country == "AU"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("Telstra", "Optus", "3", "3 PrePaid", "Vodafone", "iburst", "Dodo", "Exetel", "Internode", "TPG");
		apnlist = new Array("telstra.internet", "Internet", "3netaccess", "3services", "vfprepaymbb", "internet", "DODOLNS1", "exetel1", "splns333a1", "internet")
		daillist = new Array("*99#", "*99#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#")
		userlist = new Array("", "", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "AT"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("T-Mobile", "Orange APN", "Mobilkom Austria (A1)", "3 Österreich", "Ge org", "Bob.at", "tele.ring", "yesss");
		apnlist = new Array("gprsinternet", "web.one.at", "a1.net", "drei.at", "web", "bob.at", "WEB", "web.yesss.at");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("gprs", "web", "ppp@a1plus.at", "", "web@telering.at", "data@bob.at", "web@telering.at", "");
		passlist = new Array("", "web", "ppp", "", "d2Vi", "ppp", "web", "");
	}
	else if(country == "BA"){
		protolist = new Array("1");
		isplist = new Array("T3");
		apnlist = new Array("");
		daillist = new Array("#777");
		userlist = new Array("t3net");
		passlist = new Array("t3net");
	}
	else if(country == "BR"){
		protolist = new Array("1", "1", "1", "1");
 		isplist = new Array("Vivo", "TIM", "Oi", "Claro BR");
		apnlist = new Array("zap.vivo.com.br", "tim.br", "gprs.oi.com.br", "bandalarga.claro.com.br");
		daillist = new Array("*99#", "*99#", "*99***1#", "*99***1#");
		userlist = new Array("vivo", "tim", "oi", "claro");
		passlist = new Array("vivo", "tim", "oi", "claro");
	}
	else if(country == "BN"){
		protolist = new Array("1");
		isplist = new Array("B-Mobile");
		apnlist = new Array("dm.jplat.net");
		daillist = new Array("*99***1#");
		userlist = new Array("bmobile@u300");
		passlist = new Array("bmobile");
	}
	else if(country == "BG"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("GLOBUL", "M-Tel", "Vivacom");
		apnlist = new Array("internet.globul.bg", "inet-gprs.mtel.bg", "internet.vivatel.bg");
		daillist = new Array("359891000", "359881000", "359871000");
		userlist = new Array("globul", "", "vivatel");
		passlist = new Array("", "", "vivatel");
	}
	else if(country == "CA"){
		protolist = new Array("1");
		isplist = new Array("Rogers Wireless");
		apnlist = new Array("internet.com");
		daillist = new Array("*99***1#");
		userlist = new Array("wapuser1");
		passlist = new Array("wap");
	}
	else if(country == "CN"){
		protolist = new Array("1", "3", "2");
		isplist = new Array("China Unicom", "China Mobile", "China Telecom");
		apnlist = new Array("3gnet", "cmnet", "ctnet");
		daillist = new Array("*99#", "*99#", "#777");
		userlist = new Array("", "net", "card");
		passlist = new Array("", "net", "card");
	}
	else if(country == "CZ"){
		protolist = new Array("1", "1", "1", "2");
		isplist = new Array("T-Mobile", "O2", "Vodafone", "Ufon");
		apnlist = new Array("internet.t-mobile.cz", "internet", "internet", "");
		daillist = new Array("*99#", "*99#", "*99***1#", "#777");
		userlist = new Array("gprs", "", "", "ufon");
		passlist = new Array("gprs", "", "", "ufon");
	}
	else if(country == "DK"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("3", "TDC", "Orange", "Telia", "Telenor", "Callme", "Onfone", "CBB", "Oister", "Bibob", "Happii mobil");
		apnlist = new Array("data.tre.dk", "internet", "web.orange.dk", "www.internet.mtelia.dk", "Internet", ["Callme Internet&&websp", "Callme MMS&&mmssp"], ["Onfone GPRS&&internet.sp.dk", "Onfone MMS&&mms.sp.dk"], ["Telenor internet&&internet", "Telenor MMS&&Telenor"], ["Oister&&data.dk", "OisterMMS&&data.dk"], ["Bibob Internet&&internet.bibob.dk", "Bibob MMS&&mms.bibob.dk"], ["Happii Internet&&internet", "Happii MMS&&mms"]);
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "DO"){
		protolist = new Array("1");
		isplist = new Array("Claro");
		apnlist = new Array("internet.ideasclaro.com.do");
		daillist = new Array("*99#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "EG"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Mobinil", "Vodafone", "Etisalat");
		apnlist = new Array("mobinilweb", "internet.vodafone.net", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "internet", "");
		passlist = new Array("", "internet", "");
	}
	else if(country == "FI"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("DNA", "Saunalahti", "Elisa", "Sonera", "AinaCom/Sonera", "AINA", "TDC/DNA", "Dicame");
		apnlist = new Array("internet", "internet.saunalahti", "internet", "internet", "internet", "internet.aina.fi", "inet.tdc.fi", "internet.dicame.fi");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "");
	}
	else if(country == "DE"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("Call & Surf via Funk LTE", "T-Mobile MagentaMobil", "T-Mobile Data Comfort", "T-Mobile Xtra Prepaid", "congstar", "congstar LTE", "klarmobile (0151)", "Callmobile", "ja!mobile", "Vodafone Vertragstarife", "Vodafone Websessions", "1&1 D-Netz", "klarmobile (0152)", "Bildmobil (auch RTL)", "Fyve Mobil", "otelo Smartphone", "otelo Datentarife", "Edeka Mobil", "BASE Prepaid", "1&1 E-Netz", "Simyo", "Blau", "Blau Tages-Surfflat", "Ay Yildiz", "Aldi Talk", "Aldi Talk Tages-Surfflat", "O2 Blue (Smartphone)", "O2 go mobil", "O2 Prepaid", "Fonic", "klarmobile (0176)", "Tschibo Prepaid", "Tschibo Vertrag", "Lidl Mobil", "Saturn Daten Tarif LTE", "Mediamarkt Datentarif LTE");
		apnlist = new Array("internet.home", "internet.telekom", "internet.telekom", "internet.t-mobile", "internet.t-mobile", "internet.telekom", "internet.t-d1.de", "internet.t-mobile", "internet.t-mobile", "web.vodafone.de", "event.vodafone.de", "web.vodafone.de", "web.vodafone.de", "event.vodafone.de", "event.vodafone.de", "web.vodafone.de", "data.otelo.de", "data.access.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "internet.eplus.de", "tagesflat.eplus.de", "internet.eplus.de", "internet.eplus.de", "tagesflat.eplus.de", "internet", "surfo2", "pinternet.interkom.de", "pinternet.interkom.de", "internet.mobilcom", "webmobil1", "surfmobil2", "pinternet.interkom.de", "surfo2", "surfo2");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("tmobile", "tmobile", "tmobile", "tmobile", "tmobile", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "simyo", "blau", "", "eplus", "eplus", "", "", "", "", "", "", "", "", "", "", "");
		passlist = new Array("tm", "tm", "tm", "tm", "tm", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "simyo", "blau", "", "gprs", "gprs", "", "", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "GR"){
		protolist = new Array("1");
		isplist = new Array("WIND");
		apnlist = new Array("gint.b-online.gr");
		daillist = new Array("*99#");
		userlist = new Array("");
		passlist = new Array("");
        }
	else if(country == "HK"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("SmarTone", "3", "One2Free", "PCCW mobile", "New World Mobility", "CSL", "CMCC HK", "All 3G ISP support");
		apnlist = new Array("internet", "ipc.three.com.hk", "internet", "pccw", "internet", "internet", "internet", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "");
	}
	else if(country == "IL"){
		protolist = new Array("1");
		isplist = new Array("Cellcom");
		apnlist = new Array("sphone");
		daillist = new Array("*99#");
		userlist = new Array("");
		passlist = new Array("");
	}
	else if(country == "IN"){
		protolist = new Array("2", "2", "2", "1", "1", "1");
		isplist = new Array("Reliance", "TATA DOCOMO", "MTS", "AirTel", "IDEA", "MTNL");
		apnlist = new Array("reliance", "TATA", "", "airtelgprs.com", "Internet", "gprsppsmum");
		daillist = new Array("#777", "#777", "#777", "*99#", "*99#", "*99#");
		userlist = new Array("", "internet", "internet@internet.mtsindia.in", "", "", "mtnl");
		passlist = new Array("", "internet", "mts", "", "", "mtnl123");
	}
	else if(country == "ID"){
		protolist = new Array("1", "1", "1", "1", "1", "2", "1", "2", "2", "2", "2");
		isplist = new Array("IM2", "INDOSAT", "XL", "Telkomsel", "3", "SMARTFREN", "AXIS", "Esia", "StarOne", "TelkomFlexi", "AHA");
		apnlist = new Array("indosatm2", "indosat3g", "www.xlgprs.net", "flash", "3gprs", "", "AXIS", "", "", "", "AHA");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99***1#", "#777", "*99***1#", "#777", "#777", "#777", "#777");
		userlist = new Array("", "indosat", "xlgprs", "", "3gprs", "smart", "axis", "esia", "starone", "telkomnet@flexi", "aha@aha.co.id");
		passlist = new Array("", "indosat", "proxl", "", "3gprs", "smart", "123456", "esia", "indosat", "telkom", "aha");
	}
	else if(country == "IT"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("Vodafone", "TIM", "Wind", "3");
		apnlist = new Array("web.omnitel.it", "ibox.tim.it", "internet.wind", "tre.it");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "JP"){
		protolist = new Array("1", "1");
		isplist = new Array("SoftBank", "au");
		apnlist = new Array("emb.ne.jp", "au.NET");
		daillist = new Array("*99***1#", "*99**24#");
		userlist = new Array("em", "au@au-win.ne.jp");
		passlist = new Array("em", "au");
	}
	else if(country == "MY"){
		protolist = new Array("1", "1", "1", "1", "4");
		isplist = new Array("Celcom", "Maxis", "DiGi", "U Mobile", "Yes");
		apnlist = new Array("celcom3g", "unet", "3gdgnet", "my3g", "");
		daillist = new Array("*99***1#", "*99***1#", "*99#", "*99#", "");
		userlist = new Array("", "maxis", "", "", "");
		passlist = new Array("", "wap", "", "", "");
	}
	else if(country == "NL"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("T-Mobile", "KPN", "Telfort", "Vodafone");
		apnlist = new Array("internet", "internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#");
		userlist = new Array("", "", "", "");
		passlist = new Array("", "", "", "");
	}
	else if(country == "NZ"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("CHT", "Vodafone", "2degrees");
		apnlist = new Array("internet", "www.vodafone.net.nz", "2degrees");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "NO"){
		protolist = new Array("1", "1", "1", "1", "1", "1");
		isplist = new Array("Tele2", "NetCom", "Chess", "Telenor", "Network Norway", "One Call");
		apnlist = new Array("internet.tele2.no", "netcom", "netcom", "telenor", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "");
	}
	else if(country == "PH"){
		protolist = new Array("1", "1", "1", "1", "1", "1");
		isplist = new Array("Globe", "Globe Prepaid", "Smart", "Smart Bro", "Sun", "Sun Prepaid");
		apnlist = new Array("internet.globe.com.ph", "http.globe.com.ph", "internet", "smartbro", "fbband", "minternet");
		daillist = new Array("*99***1#", "*99***1#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "");
	}
	else if(country == "PL"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("Play", "Cyfrowy Polsat", "T-Mobile", "Orange", "Plus", "Heyah", "Aster", "SAMISWOI", "Aero2");
		apnlist = new Array("internet", "multi.internet", "internet", "internet", "internet", "internet", "aster.internet", "www.plusgsm.pl", "darmowy");
		daillist = new Array("*99#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99***1#", "*99#", "*99***1#", "*99#");
		userlist = new Array("", "", "", "internet", "plusgsm", "heyah", "internet", "internet", "");
		passlist = new Array("", "", "", "internet", "plusgsm", "heyah", "internet", "internet", "");
	}
	else if(country == "PT"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("TMN", "Optimus", "Vodafone");
		apnlist = new Array("internet", "myconnection", "net2.vodafone.pt");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "vodafone");
		passlist = new Array("", "", "vodafone");
	}
	else if(country == "RO"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("Vodafone", "Orange", "Cosmote", "RCS-RDS");
		apnlist = new Array("internet.vodafone.ro", "internet", "broadband", "internet");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("internet.vodafone.ro", "", "", "");
		passlist = new Array("vodafone", "", "", "");
	}
	else if(country == "RU"){
		protolist = new Array("1", "1", "1", "1", "4");
		isplist = new Array("Beeline", "MegaFon", "MTS", "PrimTel", "Yota");
		apnlist = new Array("internet.beeline.ru", "internet.nw", "internet.mts.ru", "internet.primtel.ru", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "");
		userlist = new Array("beeline", "", "", "", "");
		passlist = new Array("beeline", "", "", "", "");
	}
	else if(country == "SG"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("M1", "SingTel", "StarHub", "Grid");
		apnlist = new Array("sunsurf", "internet", "shinternet", "internet");
		daillist = new Array("*99#", "*99#", "*99#", "*99***1#");
		userlist = new Array("65", "", "", "");
		passlist = new Array("user123", "", "", "");
	}
	else if(country == "SK"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("O2", "Orange", "T-Mobile");
		apnlist = new Array("o2internet", "internet", "internet");
		daillist = new Array("*99***1#", "*99#", "*99***1#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "ZA"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Vodacom", "MTN", "Cell C");
		apnlist = new Array("internet", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "ES"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("Vodafone", "movistar", "Simyo", "ONO");
		apnlist = new Array("airtelnet.es", "movistar.es", "gprs-service.com", "internet.ono.com");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("vodafone", "movistar", "", "");
		passlist = new Array("vodafone", "movistar", "", "");
	}
	else if(country == "SE"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1");
		isplist = new Array("3", "Telia", "Telenor", "Tele2", "Glocalnet", "Halebop", "Ventelo", "Spring Mobile", "UVTC (Universal Telecom)", "TDC", "Cellip");
		apnlist = new Array("data.tre.se", "online.telia.se", "internet.telenor.se", "internet.tele2.se", "internet.glocalnet.se", "halebop.telia.se", "internet.ventelo.se", "data.springmobil.se", "-services sp", "internet.se", "-services sp");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "", "", "", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "", "", "", "");
	}
	else if(country == "TW"){
		protolist = new Array("1", "1", "1", "1", "4");
		isplist = new Array("Far EasTone", "Chunghwa Telecom", "TW Mobile", "T Star", "GMC");
		apnlist = new Array("internet", "internet", "internet", "internet", "");
		daillist = new Array("*99#", "*99***1#", "*99#", "*99#", "");
		userlist = new Array("", "", "", "", "");
		passlist = new Array("", "", "", "", "");
	}
	else if(country == "TH"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("AIS", "dtac", "truemove H");
		apnlist = new Array("internet", "www.dtac.co.th", "hinternet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "true");
		passlist = new Array("", "", "true");
	}
	else if(country == "TR"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("Turkcell", "Vodafone", "Avea");
		apnlist = new Array("mgb", "internet", "internet");
		daillist = new Array("*99#", "*99#", "*99#");
		userlist = new Array("", "", "");
		passlist = new Array("", "", "");
	}
	else if(country == "UA"){
		protolist = new Array("1", "1", "1", "1", "1", "1", "2", "1", "2", "2", "2", "1", "2", "4", "4");
		isplist = new Array("Beeline", "Kyivstar", "Kyivstar Prepaid", "Kyivstar XL", "Kyivstart 3G", "Djuice", "MTS", "Utel", "PEOPLEnet", "Intertelecom", "Intertelecom.Rev.B", "life", "CDMA Ukraine", "FreshTel", "Giraffe");
		apnlist = new Array("internet.beeline.ua", "www.kyivstar.net", "www.ab.kyivstar.net", "xl.kyivstar.net", "3g.kyivstar.net", "www.djuice.com.ua", "", "3g.utel.ua", "", "", "", "Internet", "", "", "");
		daillist = new Array("*99#", "*99#", "*99#", "*99#", "*99#", "*99#", "#777", "*99#", "#777", "#777", "#777", "*99#", "#777", "", "");
		userlist = new Array("mobile", "", "", "", "", "", "mobile", "", "", "IT", "3G_TURBO", "", "", "freshtel", "");
		passlist = new Array("internet", "", "", "", "", "", "internet", "", "000000", "IT", "3G_TURBO", "", "", "freshtel", "");
	}
	else if(country == "UK"){
		protolist = new Array("1", "1", "1", "1");
		isplist = new Array("3", "O2", "Vodafone", "Orange");
		apnlist = new Array("orangenet", "m-bb.o2.co.uk", "PPBUNDLE.INTERNET", "internetvpn");
		daillist = new Array("*99#", "*99#", "*99#", "*99#");
		userlist = new Array("", "o2bb", "web", "");
		passlist = new Array("", "password", "web", "");
	}
	else if(country == "US"){
		protolist = new Array("1", "1", "1", "1", "1", "2", "1", "2");
		isplist = new Array("Telecom Italia Mobile", "Bell Mobility" ,"Cellular One" ,"Cincinnati Bell" ,"T-Mobile" ,"Verizon", "AT&T", "Sprint");
		apnlist = new Array("proxy", "", "cellular1wap", "wap.gocbw.com", "epc.tmobile.com", "", "broadband", "");
		daillist = new Array("*99***1#", "*99***1#", "*99***1#", "*99***1#", "*99***1#", "#777", "*99***1#", "#777");
		userlist = new Array("", "", "", "", "", "", "", "");
		passlist = new Array("", "", "", "", "", "", "", "");
	}
	else if(country == "VN"){
		protolist = new Array("1", "1", "1");
		isplist = new Array("MobiFone", "Vinaphone", "Viettel Mobile");
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
