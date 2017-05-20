define(function(){
	var timeZone = {
		get: function(lang){
			Date.prototype.stdTimezoneOffset_if_dst = function() {
				var jan = new Date(this.getFullYear(), 0, 1);
				var jul = new Date(this.getFullYear(), 6, 1);
				return (jan.getTimezoneOffset() != jul.getTimezoneOffset());
			}

			Date.prototype.stdTimezoneOffset_withoutDST = function() {
				var jan = new Date(this.getFullYear(), 0, 1);
				var jul = new Date(this.getFullYear(), 6, 1);
			    return Math.max(jan.getTimezoneOffset(), jul.getTimezoneOffset());
			}

			Date.prototype.dst = function() {
				if(this.stdTimezoneOffset_if_dst()){
					if(this.getTimezoneOffset() < this.stdTimezoneOffset_withoutDST()){
						// in DST now
						return 2;
					}
					else{
						// not in DST now
						return 1;
					}
				}
				else{
					// no DST
					return 0;
				}
			}

			var timezones = {
				"720":"UTC12",
				"660":"UTC11",
				"600":"UTC10",
				"540":"NAST9DST",
				"480":"PST8DST",
				"420":"MST7_2",
				"360":"CST6_2",
				"300":"UTC5_1",
				"270":"UTC4.30",
				"240":"UTC4_1",
				"210":"NST3.30DST",
				"180":"UTC3",
				"120":"NORO2DST",
				"60":"UTC1",
				"0":"GMT0",
				"-60":"UTC-1_2",
				"-120":"UTC-2_1",
				"-180":"UTC-3_1",
				"-210":"UTC-3.30DST",
				"-240":"UTC-4_5",
				"-270":"UTC-4.30",
				"-300":"UTC-5",
				"-330":"UTC-5.30_1",
				"-345":"UTC-5.45",
				"-360":"UTC-6",
				"-390":"UTC-6.30",
				"-420":"UTC-7",
				"-480":"CCT-8",
				"-540":"JST",
				"-570":"CST-9.30",
				"-600":"UTC-10_2",
				"-660":"UTC-11",
				"-720":"UTC-12",
				"-780":"UTC-13"
			};

			var timezones_dst = {
				"720":"UTC12",
				"660":"UTC11",
				"600":"UTC10",
				"540":"NAST9DST",
				"480":"PST8DST",
				"420":"MST7DST_1",
				"360":"UTC6DST",
				"300":"EST5DST",
				"270":"UTC4.30",
				"240":"AST4DST",
				"210":"NST3.30DST",
				"180":"EBST3DST_1",
				"120":"NORO2DST",
				"60":"EUT1DST",
				"0":"GMT0DST_1",
				"-60":"UTC-1DST_1",
				"-120":"UTC-2DST",
				"-180":"IST-3",
				"-210":"UTC-3.30DST",
				"-240":"UTC-4DST_2",
				"-270":"UTC-4.30",
				"-300":"UTC-5",
				"-330":"UTC-5.30_1",
				"-345":"UTC-5.45",
				"-360":"RFT-6",
				"-390":"UTC-6.30",
				"-420":"UTC-7",
				"-480":"CCT-8",
				"-540":"JST",
				"-570":"UTC-9.30DST",
				"-600":"UTC-10DST_1",
				"-660":"UTC-11",
				"-720":"NZST-12DST",
				"-780":"UTC-13"
			};

			var timezone_by_preferred_lang = function(lang, v){
				if(lang == "BR"){
					return v;
				}
				else if(lang == "CN"){
					if(v == "CCT-8") return "CST-8";
				}
				else if(lang == "CZ"){
					if(v == "UTC-1DST_1") return "UTC-1DST_1_1";
				}
				else if(lang == "DA"){
					if(v == "UTC-1DST_1") return "MET-1DST";
				}
				else if(lang == "DE"){
					if(v == "UTC-1DST_1") return "MEZ-1DST";
				}
				else if(lang == "ES"){
					if(v == "UTC-1DST_1") return "MET-1DST_1";
				}
				else if(lang == "FI"){
					if(v == "UTC-2DST") return "UTC-2DST_3";
				}
				else if(lang == "FR"){
					if(v == "UTC-1DST_1") return "MET-1DST_1";
				}
				else if(lang == "HU"){
					return v;
				}
				else if(lang == "IT"){
					if(v == "UTC-1DST_1") return "MEZ-1DST_1";
				}
				else if(lang == "JP"){
					return v;
				}
				else if(lang == "KR"){
					if(v == "JST") return "UTC-9_1";
				}
				else if(lang == "MS"){
					if(v == "CCT-8") return "SST-8";
				}
				else if(lang == "NL"){
					if(v == "UTC-1DST_1") return "MEZ-1DST";
				}
				else if(lang == "NO"){
					if(v == "UTC-1DST_1") return "MET-1DST";
				}
				else if(lang == "PL"){
					if(v == "UTC-1DST_1") return "UTC-1DST_2";
				}
				else if(lang == "RO"){
					return v;
				}
				else if(lang == "RU"){
					if(v == "UTC-3_1") return "UTC-3_4";
				}
				else if(lang == "SL"){
					if(v == "UTC-1DST_1") return "UTC-1DST_1_1";
				}
				else if(lang == "SV"){
					if(v == "UTC-1DST_1") return "MET-1DST";
				}
				else if(lang == "TH"){
					return v;
				}
				else if(lang == "TR"){
					if(v == "UTC-3_1") return "UTC-3_6";
				}
				else if(lang == "TW"){
					return v;
				}
				else if(lang == "UK"){
					if(v == "UTC-2DST") return "EET-2DST";
				}

				return v;
			}

			var browserTime = new Date();
			var offset = (browserTime.dst() == 2) ? browserTime.getTimezoneOffset() + 60 : browserTime.getTimezoneOffset();
			var timeZone = (browserTime.dst() == 0) ? timezones[offset] : timezones_dst[offset];
			if(!timeZone) timeZone = (browserTime.dst() == 0) ? timezones[0] : timezones_dst[0];

			this.time_zone = timezone_by_preferred_lang(lang, timeZone);
			this.hasDst = (browserTime.dst() == 0) ? false : true;
			this.dstEnable = (browserTime.dst() == 2) ? true : false;
		}
	}

	return timeZone;
});
