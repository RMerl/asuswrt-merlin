if ('<% nvram_get("vpn_server_unit"); %>' == '1') {
	vpnd_state='<% nvram_get("vpn_server1_state"); %>';
	vpnd_errno='<% nvram_get("vpn_server1_errno"); %>';
} else {
	vpnd_state='<% nvram_get("vpn_server2_state"); %>';
	vpnd_errno='<% nvram_get("vpn_server2_errno"); %>';
}

vpn_upload_state='<% nvram_get("vpn_upload_state"); %>';
vpn_crt_client1_ca='<% nvram_clean_get("vpn_crt_client1_ca"); %>';
vpn_crt_client1_crt='<% nvram_clean_get("vpn_crt_client1_crt"); %>';
vpn_crt_client1_key='<% nvram_clean_get("vpn_crt_client1_key"); %>';
vpn_crt_client1_static='<% nvram_clean_get("vpn_crt_client1_static"); %>';
vpn_crt_client1_crl='<% nvram_get("vpn_crt_client1_crl"); %>';

vpn_crt_server1_ca='<% nvram_clean_get("vpn_crt_server1_ca"); %>';
vpn_crt_server1_crt='<% nvram_clean_get("vpn_crt_server1_crt"); %>';
vpn_crt_server1_key='<% nvram_clean_get("vpn_crt_server1_key"); %>';
vpn_crt_server1_dh='<% nvram_clean_get("vpn_crt_server1_dh"); %>';
vpn_crt_server1_crl='<% nvram_clean_get("vpn_crt_server1_crl"); %>';
vpn_crt_server1_static='<% nvram_clean_get("vpn_crt_server1_static"); %>';
vpn_crt_server2_ca='<% nvram_clean_get("vpn_crt_server2_ca"); %>';
vpn_crt_server2_crt='<% nvram_clean_get("vpn_crt_server2_crt"); %>';
vpn_crt_server2_key='<% nvram_clean_get("vpn_crt_server2_key"); %>';
vpn_crt_server2_dh='<% nvram_clean_get("vpn_crt_server2_dh"); %>';
vpn_crt_server2_crl='<% nvram_clean_get("vpn_crt_server2_crl"); %>';
vpn_crt_server2_static='<% nvram_clean_get("vpn_crt_server2_static"); %>';

