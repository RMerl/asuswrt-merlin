if ('<% nvram_get("vpn_server_unit"); %>' == '1') {
	vpnd_state='<% nvram_get("vpn_server1_state"); %>';
	vpnd_errno='<% nvram_get("vpn_server1_errno"); %>';
} else {
	vpnd_state='<% nvram_get("vpn_server2_state"); %>';
	vpnd_errno='<% nvram_get("vpn_server2_errno"); %>';
}

vpn_upload_state='<% nvram_get("vpn_upload_state"); %>';

<% vpn_crt_client(); %>
<% vpn_crt_server(); %>
