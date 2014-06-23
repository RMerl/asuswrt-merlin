var array_traffic = new Array();
var router_traffic = new Array();
array_traffic = <% bwdpi_status("traffic", "", "realtime", ""); %>;

router_traffic = <% bwdpi_status("traffic_wan", "", "realtime", ""); %>;