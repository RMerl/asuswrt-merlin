var wans_dualwan = '<% nvram_get("wans_dualwan"); %>'.split(" ");
if(wans_dualwan != ""){
	var ewan_index = wans_dualwan.indexOf("wan");
	autodet_state = (ewan_index == 0)? '<% nvram_get("autodet_state"); %>': '<% nvram_get("autodet1_state"); %>';
}
else{
	autodet_state = '<% nvram_get("autodet_state"); %>';
}
<% wanstate(); %>
<% dual_wanstate(); %>
