(function(jQuery){
	router_ip = "<% nvram_get("lan_gateway"); %>";
	
	function testRemote(){
		return router_ip;
	}
	
})(jQuery);
