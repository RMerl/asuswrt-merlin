function wl_chanspec_list_change(){
	var phytype = "n";
	var band = document.form.wl_unit.value;
	var bw_cap = document.form.wl_bw.value;
	var chanspecs = new Array(0);
	var cur = 0;
	var sel = 0;

	if(country == ""){
		country = prompt("The Country Code is not exist! Please enter Country Code.", "");
	}

	/* Save current chanspec */
	cur = '<% nvram_get("wl_chanspec"); %>';

	if (phytype == "a") {	// a mode
		chanspecs = new Array(0); 
	}
	else if (phytype == "n") { // n mode
		
		if (band == "1") { // ---- 5 GHz
				if(wl_channel_list_5g instanceof Array && wl_channel_list_5g != ["0"]){	//With wireless channel 5g hook or return not ["0"]
						wl_channel_list_5g = eval('<% channel_list_5g(); %>');
						
						if (bw_cap != "0" && bw_cap != "1" && wl_channel_list_5g.getIndexByValue("165") >= 0 ) // rm 165, If not [20 MHz] or not [Auto]
								wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue("165"),1);
						
						if(bw_cap == "0"){	// [20/40/80 MHz] (auto)
								for(var i=0;i<wl_channel_list_5g.length;i++){
										if(wl_channel_list_5g[i] == "165")		//165 belong to 20MHz
												wl_channel_list_5g[i] = wl_channel_list_5g[i];
										else if(document.form.preferred_lang.value == "UK")		// auto for UK
												wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
										else{
												if(country == "EU" && parseInt(wl_channel_list_5g[i]) >= 116 && parseInt(wl_channel_list_5g[i]) <= 140){	// belong to 40MHz	
														wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
												}else if(country == "TW" && parseInt(wl_channel_list_5g[i]) >= 56 && parseInt(wl_channel_list_5g[i]) <= 64){	// belong to 40MHz
														wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
												}else{																																		
														wl_channel_list_5g[i] = wl_channel_list_5g[i]+"/80";
												}		
										}		
								}								
						}
						else if(bw_cap == "3"){	// [80 MHz]
								for(var i=wl_channel_list_5g.length-1;i>=0;i--){									
										if(document.form.preferred_lang.value == "UK")		// auto for UK
												wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
										else{												
												if(country == "EU" && parseInt(wl_channel_list_5g[i]) >= 116 && parseInt(wl_channel_list_5g[i]) <= 140){	// rm 80MHz invalid channel
														wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
												}else if(country == "TW" && parseInt(wl_channel_list_5g[i]) >= 56 && parseInt(wl_channel_list_5g[i]) <= 64){	// rm 80MHz invalid channel														
														wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
												}else{																																																										
														wl_channel_list_5g[i] = wl_channel_list_5g[i]+"/80";
												}
										}		
								}							
								
						}
						else if(bw_cap == "2"){		// 40MHz
								for(var i=0;i<wl_channel_list_5g.length;i++){
										wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);	
								}
						}
												
						if(wl_channel_list_5g[0] != "0")
								wl_channel_list_5g.splice(0,0,"0");
								
						chanspecs = wl_channel_list_5g;	
						
				}else{	// hook failure to set chennel by static channels array
									
						if (bw_cap == "1") { // -- 20 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "36", "40", "44", "48", "149", "153", "157", "161", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36", "40", "44", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "132", "136", "140");
							else if (country == "TW")
								chanspecs = new Array(0, "56", "60", "64", "149", "153", "157", "161", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149", "153", "157", "161", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "34", "36", "38", "40", "42", "44", "46", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "120", "124", "128", "132", "136", "140", "144", "149", "153", "157", "161", "165");
							else // US
								chanspecs = new Array(0, "36", "40", "44", "48", "149", "153", "157", "161", "165");
						}					 
						else if (bw_cap == "2" || (bw_cap == "0" && document.form.preferred_lang.value == "UK")) { // -- [40 MHz] || [20/40 MHz](auto) for UK
							if (country == "Q2")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "149l", "153u", "157l", "161u", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "52l", "56u", "60l", "64u", "100l", "104u", "108l", "112u", "116l", "132l", "136u", "140l");
							else if (country == "TW")
								chanspecs = new Array(0, "60l", "64u", "149l", "153u", "157l", "161u", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149l", "153u", "157l", "161u", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "52l", "56u", "60l", "64u", "100l", "104u", "108l", "112u", "116l", "120u", "124l", "128u", "132l", "136u", "140l", "144u", "149l", "153u", "157l", "161u", "165");
							else // US
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "149l", "153u", "157l", "161u", "165");
						} 
						else if (bw_cap == "3") { // -- 80 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80");
							else if (country == "EU")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80");
							else if (country == "TW")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80");
							else if (country == "CN")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80");
							else if (country == "XX")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116/80", "120/80", "124/80", "128/80", "132/80", "136/80", "140/80", "144/80", "149/80", "153/80", "157/80", "161/80");
							else // US
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80");	
						}
						else if (bw_cap == "0" && document.form.preferred_lang.value != "UK") { // -- [20/40/80 MHz] (Auto) for not UK
							if (country == "Q2")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116l", "132l", "136u", "140l");
							else if (country == "TW")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116/80", "120/80", "124/80", "128/80", "132/80", "136/80", "140/80", "144/80", "149/80", "153/80", "157/80", "161/80", "165");
							else // US
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80", "165");
						} 
						else { // ...
								chanspecs = [0];
						}
						
				}
		} 
		else if (band == "0") { // - 2.4 GHz
				
				if(wl_channel_list_2g instanceof Array && wl_channel_list_2g != ["0"]){	//With wireless channel 2.4g hook or return ["0"]
						wl_channel_list_2g = eval('<% channel_list_2g(); %>');
						
						if(bw_cap == "2" || bw_cap == "0") { // -- [40 MHz]  | [20/40 MHz]
							
							  if(wl_channel_list_2g.length == 14){		//1~14
										for(var j=0; j<9; j++){
												wl_channel_list_2g[j] = wl_channel_list_2g[j] + "l";
										}
										for(var k=9; k<14; k++){
												wl_channel_list_2g[k] = wl_channel_list_2g[k] + "u";
										}
										
										//add coexistence extension channel
										wl_channel_list_2g.splice(9,0,"9u");
										wl_channel_list_2g.splice(8,0,"8u");
										wl_channel_list_2g.splice(7,0,"7u");
										wl_channel_list_2g.splice(6,0,"6u");
										wl_channel_list_2g.splice(5,0,"5u");
								
								}
								else if(wl_channel_list_2g.length == 13){		//1~13
										for(var j=0; j<9; j++){
												wl_channel_list_2g[j] = wl_channel_list_2g[j] + "l";
										}
										for(var k=9; k<13; k++){
												wl_channel_list_2g[k] = wl_channel_list_2g[k] + "u";
										}
										
										//add coexistence extension channel
										wl_channel_list_2g.splice(9,0,"9u");
										wl_channel_list_2g.splice(8,0,"8u");
										wl_channel_list_2g.splice(7,0,"7u");
										wl_channel_list_2g.splice(6,0,"6u");
										wl_channel_list_2g.splice(5,0,"5u");
								
								}
								else if(wl_channel_list_2g.length == 11){		//1~11
										for(var j=0; j<7; j++){
												wl_channel_list_2g[j] = wl_channel_list_2g[j] + "l";
										}
										for(var k=7; k<11; k++){
												wl_channel_list_2g[k] = wl_channel_list_2g[k] + "u";
										}
										
										//add coexistence extension channel
										wl_channel_list_2g.splice(7,0,"7u");
										wl_channel_list_2g.splice(6,0,"6u");
										wl_channel_list_2g.splice(5,0,"5u");
																				 
								}						
						}
						
						if(wl_channel_list_2g[0] != "0")
								wl_channel_list_2g.splice(0,0,"0");
								
						chanspecs = wl_channel_list_2g;
						
				}else{
				
						if (bw_cap == "1") { // -- 20 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
							else if (country == "EU")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
							else if (country == "TW")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
							else if (country == "CN")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
							else if (country == "XX")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14");
							else // US
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
						} 
						else if (bw_cap == "2" || bw_cap == "0") { // -- 40 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
							else if (country == "EU")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u");
							else if (country == "TW")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
							else if (country == "CN")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u");
							else if (country == "XX")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u", "14u");
							else // US
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
						}
						else { // ...
							chanspecs = [0];
						}
				}		
		}
	} 
	else { // b/g mode 
		chanspecs = new Array(0);
	}

	/* displaying chanspec even if the BW is auto.
	if(chanspecs[0] == 0 && chanspecs.length == 1)
		document.form.wl_chanspec.parentNode.parentNode.style.display = "none";
	else
		document.form.wl_chanspec.parentNode.parentNode.style.display = "";
	*/

	/* Reconstruct channel array from new chanspecs */
	document.form.wl_chanspec.length = chanspecs.length;
	for (var i in chanspecs){
		if (chanspecs[i] == 0){
			document.form.wl_chanspec[i] = new Option("<#Auto#>", chanspecs[i]);
		}
		else{
			if(band == "1")
				document.form.wl_chanspec[i] = new Option(chanspecs[i].toString().replace("/80", "").replace("u", "").replace("l", ""), chanspecs[i]);
			else
				document.form.wl_chanspec[i] = new Option(chanspecs[i].toString(), chanspecs[i]);
		}

		document.form.wl_chanspec[i].value = chanspecs[i];
		if (chanspecs[i] == cur && bw_cap == '<% nvram_get("wl_bw"); %>') {
			document.form.wl_chanspec[i].selected = true;
			sel = 1;
		}
	}

	if (sel == 0 && document.form.wl_chanspec.length > 0)
		document.form.wl_chanspec[0].selected = true;
}


function wlextchannel_fourty(v){
		var tmp_wl_channel_5g = "";
		if(v > 144){
				tmp_wl_channel_5g = v - 1;
				if(tmp_wl_channel_5g%8 == 0)
						v = v + "u";
				else		
						v = v + "l";
		}else{
				if(v%8 == 0)
						v = v + "u";
				else		
						v = v + "l";											
		}
		return v;
}
