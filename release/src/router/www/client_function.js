var selectedClientOrder;

function getclients_noMonitor(flag){							// return the number of the client
	var clients = new Array();
	var wired_client_num = 0;
	
	// get wired clients
	/*for(var i = 0, wired_client_num = 0; i < arls.length; ++i, ++wired_client_num){
		clients[i] = new Array(9);
		//clients[i][0] = "<#CTL_unknown#>";	// hostname
		clients[i][0] = "";	// hostname
		clients[i][1] = "";	// ip
		clients[i][2] = arls[i][0].toUpperCase();	// MAC
		clients[i][3] = parseInt(arls[i][1]);	// where port the wired client is.
		clients[i][4] = null;
		clients[i][5] = null;
		clients[i][6] = "0";
		clients[i][7] = "0";
		clients[i][8] = "0";
	}*/
	// get wired clients  2008.12 magic  read arp table
	for(var i = 0, wired_client_num = 0; i < arps.length; ++i, ++wired_client_num){
		clients[i] = new Array(9);
		//clients[i][0] = "<#CTL_unknown#>";	// hostname
		clients[i][0] = "";	// hostname
		clients[i][1] = arps[i][0];	// ip
		clients[i][2] = arps[i][3];	// MAC
		clients[i][3] = "";	
		clients[i][4] = null;
		clients[i][5] = null;
		clients[i][6] = "0";
		clients[i][7] = "0";
		clients[i][8] = "0";
	}
	/*// get wireless clients 2008.12 magic
	for(var i = wired_client_num; i < wired_client_num+wireless.length; ++i){
		var wireless_order = i-wired_client_num;
		
		clients[i] = new Array(9);
		//clients[i][0] = "<#CTL_unknown#>";	// hostname
		clients[i][0] = "";	// hostname
		clients[i][1] = "";	// ip
		clients[i][2] = wireless[wireless_order][0];	// MAC
		clients[i][3] = 10;	// 10 is meant the client is wireless.
		
		clients[i][4] = new Array(2);
		clients[i][4][0] = wireless[wireless_order][1];
		clients[i][4][1] = wireless[wireless_order][2];
		
		if(clients[i][4][0] == "Yes")
			clients[i][4][0] = "Associated";
		else
			clients[i][4][0] = "Disassociated";
		
		clients[i][5] = null;
		clients[i][6] = "0";
		clients[i][7] = "0";
		clients[i][8] = "0";
	}*/
	
	// get the hostnames and IPs of the clients
	for(var i = 0; i < clients.length; ++i){
		// a client can have more leases and the later the newer.
		for(var j = leases.length-1; j >= 0; --j){
			if(leases[j][3] == "Expired")
				continue;
			
			if(clients[i][2] == leases[j][1]){
				// get hostname
				if(leases[j][0].length > 0)
					clients[i][0] = leases[j][0];
				
				// get ip
				if(leases[j][2].length > 0)
					clients[i][1] = leases[j][2];
				
				break;
			}
		}
		
		// a MAC can have more ip and the fronter the newer.
		for(var j = 0; j < arps.length; ++j){
			if(clients[i][2] == arps[j][3]){
				// get ip
				if(arps[j][0].length > 0)
					clients[i][1] = arps[j][0];
				
				break;
			}
		}
		// 2008.12 magic which is the wireless clients.
		for(var j = 0; j < wireless.length; ++j){
			if(clients[i][2] == wireless[j][0]){
				clients[i][3] = 10;	// 10 is meant the client is wireless.
				
				clients[i][4] = new Array(2);
				clients[i][4][0] = wireless[j][1];
				clients[i][4][1] = wireless[j][2];
				
				if(clients[i][4][0] == "Yes")
					clients[i][4][0] = "Associated";
				else
					clients[i][4][0] = "Disassociated";
				
				break;
			}
		}

		// judge which is the local device.
		if(clients[i][1] == login_ip_str()
				&& clients[i][2] == login_mac_str()){
			if(clients[i][0] == null || clients[i][0].length <= 0)
				clients[i][0] = "<#CTL_localdevice#>";
			else
				clients[i][0] += "(<#CTL_localdevice#>)";
		}
		
		if(flag == 1)
			clients[i][2] = simplyMAC(clients[i][2]);
	}
	
	return clients;
}

function getclients_Monitor(flag){
	var clients = new Array();
	
	for(var i = 0; i < ipmonitor.length; ++i){
		clients[i] = new Array(9);
								
		clients[i][0] = ipmonitor[i][2];  // Device name
		
		for(var j = leases.length-1; j >= 0; --j){
			if(leases[j][3] == "Expired")
				continue;
			
			if(ipmonitor[i][0] == leases[j][2]){
				if(ipmonitor[i][2] != leases[j][0]){
					clients[i][0] = leases[j][0];
				}
				else{
					clients[i][0] = ipmonitor[i][2];
				}
			}
		}			
		clients[i][1] = ipmonitor[i][0];	// IP
		clients[i][2] = ipmonitor[i][1];	// MAC
		clients[i][3] = null;	// if this is a wireless client.
		clients[i][4] = null;	// wireless information.
		clients[i][5] = ipmonitor[i][3];	// TYPE
		clients[i][6] = ipmonitor[i][4];	// if there's the HTTP service.
		clients[i][7] = ipmonitor[i][5];	// if there's the Printer service.
		clients[i][8] = ipmonitor[i][6];	// if there's the iTune service.
		
		//2008.12 magic
		/*for(var j = 0; j < arls.length; ++j){			
			if(clients[i][2] == arls[j][0].toUpperCase()){
				clients[i][3] = parseInt(arls[j][1]);	// where port the wired client is.
				
				break;
			}
		}*/
		
		for(var j = 0; j < wireless.length; ++j){
			if(clients[i][2] == wireless[j][0]){
				clients[i][3] = 10;	// 10 is meant the client is wireless.
				
				clients[i][4] = new Array(2);
				clients[i][4][0] = wireless[j][1];
				clients[i][4][1] = wireless[j][2];
				
				if(clients[i][4][0] == "Yes")
					clients[i][4][0] = "Associated";
				else
					clients[i][4][0] = "Disassociated";
				
				break;
			}
		}
		
		// judge which is the local device.
		if(clients[i][1] == login_ip_str()
				&& clients[i][2] == login_mac_str()){
			if(clients[i][0] == null || clients[i][0].length <= 0)
				clients[i][0] = "<#CTL_localdevice#>";
			else
				clients[i][0] += "(<#CTL_localdevice#>)";
		}
		
		if(flag == 1)
			clients[i][2] = simplyMAC(clients[i][2]);
	}
	
	return clients;
}

function getclients(flag){
	if(networkmap_fullscan == "done")
		return getclients_Monitor(flag);
	else
		return getclients_noMonitor(flag);
}

function simplyMAC(fullMAC){
	var ptr;
	var tempMAC;
	var pos1, pos2;
	
	ptr = fullMAC;
	tempMAC = "";
	pos1 = pos2 = 0;
	
	for(var i = 0; i < 5; ++i){
		pos2 = pos1+ptr.indexOf(":");
		
		tempMAC += fullMAC.substring(pos1, pos2);
		
		pos1 = pos2+1;
		ptr = fullMAC.substring(pos1);
	}
	
	tempMAC += fullMAC.substring(pos1);
	
	return tempMAC;
}

function test_all_clients(clients){
	var str = "";
	var Row;
	var Item;
	
	str += clients.length+"\n";
	
	Row = 1;
	for(var i = 0; i < clients.length; ++i){
		if(Row == 1)
			Row = 0;
		else
			str += "\n";
		
		Item = 1;
		for(var j = 0; j < 9; ++j){
			if(Item == 1)
				Item = 0;
			else
				str += ", ";
			
			str += clients[i][j];
		}
		
		str += "\n";
	}
	
	alert(str);
}
