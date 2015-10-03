originDataTmp = originData;
originData = {
	customList: decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	asusDevice: decodeURIComponent('<% nvram_char_to_ascii("", "asus_device_list"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromDHCPLease: <% dhcpLeaseMacList(); %>,
	staticList: decodeURIComponent('<% nvram_char_to_ascii("", "dhcp_staticlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromNetworkmapd: '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromBWDPI: '<% bwdpi_device_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	nmpClient: decodeURIComponent('<% nvram_char_to_ascii("", "nmp_client_list"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	wlList_2g: [<% wl_sta_list_2g(); %>],
	wlList_5g: [<% wl_sta_list_5g(); %>],
	wlList_5g_2: [<% wl_sta_list_5g_2(); %>],
	wlListInfo_2g: [<% wl_stainfo_list_2g(); %>],
	wlListInfo_5g: [<% wl_stainfo_list_5g(); %>],
	wlListInfo_5g_2: [<% wl_stainfo_list_5g_2(); %>],
	qosRuleList: decodeURIComponent('<% nvram_char_to_ascii("", "qos_rulelist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	time_scheduling_enable: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_ENABLE"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_mac: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_MAC"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_devicename: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_DEVICENAME"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	time_scheduling_daytime: decodeURIComponent('<% nvram_char_to_ascii("", "MULTIFILTER_MACFILTER_DAYTIME"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('>'),
	current_time: '<% uptime(); %>'
}
networkmap_fullscan = '<% nvram_get("networkmap_fullscan"); %>';
if(networkmap_fullscan == 1) genClientList();
