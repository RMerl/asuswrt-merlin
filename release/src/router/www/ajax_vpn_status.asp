vpn_server1_status = "<% sysinfo("vpnstatus.server.1"); %>";
vpn_server2_status = "<% sysinfo("vpnstatus.server.2"); %>";
vpn_client1_status = "<% sysinfo("vpnstatus.client.1"); %>";
vpn_client2_status = "<% sysinfo("vpnstatus.client.2"); %>";
vpn_client3_status = "<% sysinfo("vpnstatus.client.3"); %>";
vpn_client4_status = "<% sysinfo("vpnstatus.client.4"); %>";
vpn_client5_status = "<% sysinfo("vpnstatus.client.5"); %>";

server1pid = "<% sysinfo("pid.vpnserver1"); %>";
server2pid = "<% sysinfo("pid.vpnserver2"); %>";
pptpdpid = "<% sysinfo("pid.pptpd"); %>";

vpn_client1_ip = "<% sysinfo("vpnip.1"); %>";
vpn_client2_ip = "<% sysinfo("vpnip.2"); %>";
vpn_client3_ip = "<% sysinfo("vpnip.3"); %>";
vpn_client4_ip = "<% sysinfo("vpnip.4"); %>";
vpn_client5_ip = "<% sysinfo("vpnip.5"); %>";

vpn_client1_rip = "<% nvram_get("vpn_client1_rip"); %>";
vpn_client2_rip = "<% nvram_get("vpn_client2_rip"); %>";
vpn_client3_rip = "<% nvram_get("vpn_client3_rip"); %>";
vpn_client4_rip = "<% nvram_get("vpn_client4_rip"); %>";
vpn_client5_rip = "<% nvram_get("vpn_client5_rip"); %>";

