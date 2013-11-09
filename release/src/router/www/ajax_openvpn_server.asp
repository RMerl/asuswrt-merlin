if ('<% nvram_get("vpn_server_unit"); %>' == '1')
	vpnd_state='<% nvram_get("vpn_server1_state"); %>';
else
	vpnd_state='<% nvram_get("vpn_server2_state"); %>';
vpn_upload_state='<% nvram_get("vpn_upload_state"); %>';
vpn_crt_client1_ca='<% nvram_clean_get("vpn_crt_client1_ca"); %>';
vpn_crt_client1_crt='<% nvram_clean_get("vpn_crt_client1_crt"); %>';
vpn_crt_client1_key='<% nvram_clean_get("vpn_crt_client1_key"); %>';
vpn_crt_client1_static='<% nvram_clean_get("vpn_crt_client1_static"); %>';
