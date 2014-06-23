
originDataTmp = originData;
originData = {
	customList: decodeURIComponent('<% nvram_char_to_ascii("", "custom_clientlist"); %>').replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	asusDevice: '<% nvram_get("asus_device_list"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromDHCPLease: '',
	fromNetworkmapd: '<% get_client_detail_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	fromBWDPI: '<% bwdpi_device_info(); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<'),
	wlList_2g: [<% wl_sta_list_2g(); %>],
	wlList_5g: [<% wl_sta_list_5g(); %>],
	qosRuleList: '<% nvram_get("qos_rulelist"); %>'.replace(/&#62/g, ">").replace(/&#60/g, "<").split('<')
}
mapscanning = <% nvram_get("networkmap_status"); %>;
