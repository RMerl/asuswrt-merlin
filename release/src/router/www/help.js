var Untranslated = {
	wireless_psk_fillin : 'Please type password',
	Adj_dst : 'Manual daylight saving time'
};

var helptitle = new Array(19);
// Wireless
helptitle[0] = [["", ""],
				["<#WLANConfig11b_SSID_itemname#>", "wl_ssid"],
				["<#WLANConfig11b_x_BlockBCSSID_itemname#>", "wl_closed"],				
				["<#WLANConfig11b_Channel_itemname#>", "wl_channel"],
				["<#WLANConfig11b_x_Mode11g_itemname#>", "wl_nmode_x"],
				["<#WLANConfig11b_AuthenticationMethod_itemname#>", "wl_auth_mode"],
				["<#WLANConfig11b_WPAType_itemname#>", "wl_crypto"],
				["<#WLANConfig11b_x_PSKKey_itemname#>", "wl_wpa_psk"],
				["<#WLANConfig11b_x_Phrase_itemname#>", "wl_phrase_x"],
				["<#WLANConfig11b_WEPType_itemname#>", "wl_wep_x"],
				["<#WLANConfig11b_WEPDefaultKey_itemname#>", "wl_key"],
				["<#WLANConfig11b_x_Rekey_itemname#>", "wl_wpa_gtk_rekey"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_asuskey1"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_asuskey1"],
				["<#WLANConfig11b_ChannelBW_itemname#>", "wl_bw"],
				["<#WLANConfig11b_EChannel_itemname#>", "wl_nctrlsb"],
				["<#WLANConfig11b_WLBand_itemname#>", "wl_bw"],
				["<#WLANConfig11b_TxPower_itemname#>", "TxPower"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_key1"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_key2"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_key3"],
				["<#WLANConfig11b_WEPKey_itemname#>", "wl_key4"],
				["", ""]];
helptitle[1] = [["", ""],
				["<#WLANConfig11b_x_APMode_itemname#>", "wl_mode_x"],
				["<#WLANConfig11b_Channel_itemname#>", "wl_channel"],
				["<#WLANConfig11b_x_BRApply_itemname#>", "wl_wdsapply_x"]];
helptitle[2] = [["", ""],
				["<#WLANAuthentication11a_ExAuthDBIPAddr_itemname#>", "wl_radius_ipaddr"],
				["<#WLANAuthentication11a_ExAuthDBPortNumber_itemname#>", "wl_radius_port"],
				["<#WLANAuthentication11a_ExAuthDBPassword_itemname#>", "wl_radius_key"]];
helptitle[3] = [["", ""],
				["<#WLANConfig11b_x_RadioEnable_itemname#>", "wl_radio_x"],
				["<#WLANConfig11b_x_RadioEnableDate_itemname#>", "wl_radio_date_x_"],
				["<#WLANConfig11b_x_RadioEnableTime_itemname#>", "wl_radio_time_x_"],
				["", ""],//["<#WLANConfig11b_x_AfterBurner_itemname#>", "wl_afterburner"],
				["<#WLANConfig11b_x_IsolateAP_itemname#>", "wl_ap_isolate"],
				["<#WLANConfig11b_DataRateAll_itemname#>", "wl_rate"],
				["<#WLANConfig11b_MultiRateAll_itemname#>", "wl_mrate_x"],
				["<#WLANConfig11b_DataRate_itemname#>", "wl_rateset"],
				["<#WLANConfig11b_x_Frag_itemname#>", "wl_frag"],
				["<#WLANConfig11b_x_RTS_itemname#>", "wl_rts"],
				["<#WLANConfig11b_x_DTIM_itemname#>", "wl_dtim"],
				["<#WLANConfig11b_x_Beacon_itemname#>", "wl_bcn"],
				["<#WLANConfig11b_x_TxBurst_itemname#>", "TxBurst"],
				["<#WLANConfig11b_x_WMM_itemname#>", "wl_wme"],
				["<#WLANConfig11b_x_NOACK_itemname#>", "wl_wme_no_ack"],
				["<#WLANConfig11b_x_PktAggregate_itemname#>", "PktAggregate"],
				["<#WLANConfig11b_x_APSD_itemname#>", "APSDCapable"],
				["<#WLANConfig11b_x_DLS_itemname#>", "DLSCapable"],
				["<#WLANConfig11b_x_HT_OpMode_itemname#>", "HT_OpMode"],
				["<#WLANConfig11n_PremblesType_itemname#>", "wl_plcphdr"] // 20
				];
// LAN
helptitle[4] = [["", ""],
				["<#IPConnection_ExternalIPAddress_itemname#>", "lan_ipaddr"],
				["<#IPConnection_x_ExternalSubnetMask_itemname#>", "lan_netmask"],
				["<#IPConnection_x_ExternalGateway_itemname#>", "lan_gateway"]];
helptitle[5] = [["", ""],
			 	["<#LANHostConfig_DHCPServerConfigurable_itemname#>", "dhcp_enable_x"],
				["<#LANHostConfig_DomainName_itemname#>", "lan_domain"],
				["<#LANHostConfig_MinAddress_itemname#>", "dhcp_start"],
				["<#LANHostConfig_MaxAddress_itemname#>", "dhcp_end"],
				["<#LANHostConfig_LeaseTime_itemname#>", "dhcp_lease"],
				["<#IPConnection_x_ExternalGateway_itemname#>", "dhcp_gateway_x"],
				["<#LANHostConfig_x_LDNSServer1_itemname#>", "dhcp_dns1_x"],
				["<#LANHostConfig_x_WINSServer_itemname#>", "dhcp_wins_x"],
				["<#LANHostConfig_ManualDHCPEnable_itemname#>", "dhcp_static_x"],
				["", ""],
				["", ""],
				["", ""]];
helptitle[6] = [["", ""],
				["<#RouterConfig_GWStaticIP_itemname#>", "sr_ipaddr_x_0"],
				["<#RouterConfig_GWStaticMask_itemname#>", "sr_netmask_x_0"],
				["<#RouterConfig_GWStaticGW_itemname#>", "sr_gateway_x_0"],
				["<#RouterConfig_GWStaticMT_itemname#>", "sr_matric_x_0"],
				["<#wan_interface#>", "sr_if_x_0"],
				["<#RouterConfig_IPTV_itemname#>"]];
// WAN
helptitle[7] = [["", ""],
				["<#IPConnection_ExternalIPAddress_itemname#>", "wan_ipaddr_x"],
				["<#IPConnection_x_ExternalSubnetMask_itemname#>", "wan_netmask_x"],
				["<#IPConnection_x_ExternalGateway_itemname#>", "wan_gateway_x"],
				["<#PPPConnection_UserName_itemname#>", "wan_pppoe_username"],
				["<#PPPConnection_Password_itemname#>", "wan_pppoe_passwd"],
				["<#PPPConnection_IdleDisconnectTime_itemname#>", "wan_pppoe_idletime"],
				["<#PPPConnection_x_PPPoEMTU_itemname#>", "wan_pppoe_mtu"],
				["<#PPPConnection_x_PPPoEMRU_itemname#>", "wan_pppoe_mru"],
				["<#PPPConnection_x_ServiceName_itemname#>", "wan_pppoe_service"],
				["<#PPPConnection_x_AccessConcentrator_itemname#>", "wan_pppoe_ac"],
				["<#PPPConnection_x_PPPoERelay_itemname#>", "wan_pppoe_relay"],
				["<#IPConnection_x_DNSServerEnable_itemname#>", "wan_dnsenable_x"],
				["<#IPConnection_x_DNSServer1_itemname#>", "wan_dns1_x"],
				["<#IPConnection_x_DNSServer2_itemname#>", "wan_dns2_x"],
				["<#PPPConnection_x_HostNameForISP_itemname#>", "wan_hostname"],
				["<#PPPConnection_x_MacAddressForISP_itemname#>", "wan_hwaddr_x"],
				["<#PPPConnection_x_PPTPOptions_itemname#>", "wan_pptp_options_x"],
				["<#PPPConnection_x_AdditionalOptions_itemname#>", "wan_pppoe_options_x"],
				["<#BOP_isp_heart_item#>", "wan_heartbeat_x"],
				["<#IPConnection_BattleNet_itemname#>", "sp_battle_ips"],
				["<#Layer3Forwarding_x_STB_itemname#>", "wan_stb_x"],
				["Hardware NAT", "hwnat"],
				["<#isp_profile#>", "switch_wantag"],
				["<#PPPConnection_Authentication_itemname#>", "wan_auth_t"]];
//Firewall
helptitle[8] = [["", ""],
				["<#FirewallConfig_WanLanLog_itemname#>", "fw_log_x"],
				["<#FirewallConfig_x_WanWebEnable_itemname#>", "misc_http_x"],
				["<#FirewallConfig_x_WanWebPort_itemname#>", "misc_httpport_x"],
				["<#FirewallConfig_x_WanLPREnable_itemname#>", "misc_lpr_x"],
				["<#FirewallConfig_x_WanPingEnable_itemname#>", "misc_ping_x"],
				["<#FirewallConfig_FirewallEnable_itemname#>", "fw_enable_x"],
				["<#FirewallConfig_DoSEnable_itemname#>", "fw_dos_x"]];
helptitle[9] = [["", ""],
				["<#FirewallConfig_URLActiveDate_itemname#>", "url_date_x_"],
				["<#FirewallConfig_URLActiveTime_itemname#>", "url_time_x_"]];
helptitle[10] = [["", ""],
				["<#FirewallConfig_LanWanActiveDate_itemname#>", "filter_lw_date_x_"],
				["<#FirewallConfig_LanWanActiveTime_itemname#>", "filter_lw_time_x_"],
				["<#FirewallConfig_LanWanDefaultAct_itemname#>", "filter_lw_default_x"],
				["<#FirewallConfig_LanWanICMP_itemname#>", "filter_lw_icmp_x"],
				["<#FirewallConfig_LanWanFirewallEnable_itemname#>", "fw_lw_enable_x"]];
//Administration
helptitle[11] = [["", ""],
				["<#LANHostConfig_x_ServerLogEnable_itemname#>", "log_ipaddr"],
				["<#LANHostConfig_x_TimeZone_itemname#>", "time_zone"],
				["<#LANHostConfig_x_NTPServer_itemname#>", "ntp_server0"],
				["<#PASS_new#>"]];
//Log
helptitle[12] = [["", ""],
				["<#General_x_SystemUpTime_itemname#>", "system_now_time"],
				["<#PrinterStatus_x_PrinterModel_itemname#>", ""],
				["<#PrinterStatus_x_PrinterStatus_itemname#>", ""],
				["<#PrinterStatus_x_PrinterUser_itemname#>", ""]];
//WPS
helptitle[13] = [["", ""],
				["<#WLANConfig11b_x_WPS_itemname#>", ""],
				["<#WLANConfig11b_x_WPSMode_itemname#>", ""],
				["<#WLANConfig11b_x_WPSPIN_itemname#>", ""],
				["<#WLANConfig11b_x_DevicePIN_itemname#>", ""],
				["<#WLANConfig11b_x_WPSband_itemname#>", ""]];
//UPnP
helptitle[14] = [["", ""],
				["<#UPnPMediaServer#>", ""]];
//AiDisk Wizard
helptitle[15] = [["", ""],
				["<a href='../Advanced_AiDisk_ftp.asp' target='_parent' hidefocus='true'><#menu5_4#></a>", ""],
				["<#AiDisk_Step1_helptitle#>", ""],
				["<#AiDisk_Step2_helptitle#>", ""],
				["<#AiDisk_Step3_helptitle#>", ""]];

helptitle[16] = [["", ""],
				["<#EzQoS_helptitle#>", ""]];
//Others in the USB application
helptitle[17] = [["", ""],
				["<#ShareNode_MaximumLoginUser_itemname#>", "st_max_user"],
				["<#ShareNode_DeviceName_itemname#>", "computer_name"],
				["<#ShareNode_WorkGroup_itemname#>", "st_samba_workgroup"],
				["<#BasicConfig_EnableDownloadMachine_itemname#>", "apps_dl"],
				["<#BasicConfig_EnableDownloadShare_itemname#>", "apps_dl_share"],
				["<#BasicConfig_EnableMediaServer_itemname#>", "upnp_enable"],
                                ["<#ShareNode_Seeding_itemname#>", "apps_seed"],
                                ["<#ShareNode_MaxUpload_itemname#>", "apps_upload_max"]];
				
// MAC filter
helptitle[18] = [["", ""],
				["<#FirewallConfig_MFMethod_itemname#>", "macfilter_enable_x"],
				["<#FirewallConfig_LanWanSrcPort_itemname#>", ""],
				["<#FirewallConfig_LanWanSrcIP_itemname#>/<#FirewallConfig_LanWanDstIP_itemname#>", ""],
				["<#BM_UserList3#>", ""],
				["Transfered", ""],
				["<#FirewallConfig_LanWanSrcIP_itemname#>"]];
// Setting
helptitle[19] = [["", ""],
				["<#Setting_factorydefault_itemname#>", ""],
				["<#Setting_save_itemname#>", ""],
				["<#Setting_upload_itemname#>", ""]];
// QoS
helptitle[20] = [["", ""],
				["<#BM_measured_uplink_speed#>", ""],
				["<#BM_manual_uplink_speed#>", "qos_manual_ubw"],
				["<#max_bound#>", "max_bound"],
				["<#min_bound#>", "min_bound"],
				["<#max_bound#>", "max_bound"]];
// HSDPA
helptitle[21] = [["", ""],
				["<#HSDPAConfig_hsdpa_mode_itemname#>", "hsdpa_mode"],
				["<#PIN_code#>", "pin_code"],
				["<#HSDPAConfig_private_apn_itemname#>", "private_apn"],
				["HSDPA<#PPPConnection_x_PPPoEMTU_itemname#>", "wan_hsdpa_mtu"],
				["HSDPA<#PPPConnection_x_PPPoEMRU_itemname#>", "wan_hsdpa_mru"],
				["<#IPConnection_x_DNSServer1_itemname#>", "wan2_dns1_x"],
				["<#IPConnection_x_DNSServer2_itemname#>", "wan2_dns2_x"],
				["<#HSDPAConfig_ISP_itemname#>", "private_isp"],
				["<#HSDPAConfig_Country_itemname#>", "private_country"],
				["<#HSDPAConfig_DialNum_itemname#>", "private_dialnum"],
				["<#HSDPAConfig_Username_itemname#>", "private_username"],
				["<#HSDPAConfig_Password_itemname#>", "private_passowrd"],
				["<#HSDPAConfig_USBAdapter_itemname#>", "private_usbadaptor"]];

helptitle[22] = [["", ""],
				["Router(<#OP_GW_item#>)", ""],
				["Repeater(<#OP_RE_item#>)", ""],
				["AP(<#OP_AP_item#>)", ""]];

/*
if(psta_support != -1){
	helptitle[22] = [["", ""],["Router(<#OP_GW_item#>)", ""],["Media bridge", ""],["AP(<#OP_AP_item#>)", ""]];
}
*/

helptitle[23] = [["", ""],
				["5GHz SSID:", "ssid_5g"],
				["2.4GHz SSID:", "ssid_2g"]];

var helpcontent = new Array(19);
helpcontent[0] = new Array("",
							 "<#WLANConfig11b_SSID_itemdesc#>",
						   "<#WLANConfig11b_x_BlockBCSSID_itemdesc#>",						   
						   "<#WLANConfig11b_Channel_itemdesc#>",
						   "<#WLANConfig11b_x_Mode11g_itemdesc#>",
						   "<#WLANConfig11b_AuthenticationMethod_itemdesc#>",
						   "<#WLANConfig11b_WPAType_itemdesc#>",
						   "<#WLANConfig11b_x_PSKKey_itemdesc#>",
						   "<#WLANConfig11b_x_Phrase_itemdesc#>",
						   "<#WLANConfig11b_WEPType_itemdesc#>",
						   "<#WLANConfig11b_WEPDefaultKey_itemdesc#>",
						   "<#WLANConfig11b_x_Rekey_itemdesc#><#JS_field_wanip_rule3#>",
						   "<#WLANConfig11b_WEPKey_itemtype1#>",
						   "<#WLANConfig11b_WEPKey_itemtype2#>",
						   "<#WLANConfig11b_ChannelBW_itemdesc#><br/><#WLANConfig11b_Wireless_Speed_itemname_3#>",
						   "<#WLANConfig11b_EChannel_itemdesc#>",
							 "",
							 "Increase Tx Power to enhance the quality of transmission or decrease Tx Power to reduce power consumption.",
							 "WEP-64bits: <#WLANConfig11b_WEPKey_itemtype1#><br/>WEP-128bits: <#WLANConfig11b_WEPKey_itemtype2#>",
							 "<#WLANConfig11b_WEPKey_itemtype1#><br/><#WLANConfig11b_WEPKey_itemtype2#>",
							 "<#WLANConfig11b_WEPKey_itemtype1#><br/><#WLANConfig11b_WEPKey_itemtype2#>",
							 "<#WLANConfig11b_WEPKey_itemtype1#><br/><#WLANConfig11b_WEPKey_itemtype2#>",
							 '<div><#qis_wireless_help1#></div><br/><img src="/images/qis/select_wireless.jpg">',
							 '<div><#qis_wireless_help2#></div><br/><img width="350px" src="/images/qis/security_key.png">',
							 "<#WLANConfig11n_automode_limition_hint#>"
							 );
helpcontent[1] = new Array("",
						   "<#WLANConfig11b_x_APMode_itemdesc#>",
						   "<#WLANConfig11b_Channel_itemdesc#>",
						   "<#WLANConfig11b_x_BRApply_itemdesc#>");
helpcontent[2] = new Array("",
						   "<#WLANAuthentication11a_ExAuthDBIPAddr_itemdesc#>",
						   "<#WLANAuthentication11a_ExAuthDBPortNumber_itemdesc#>",
						   "<#WLANAuthentication11a_ExAuthDBPassword_itemdesc#>");
helpcontent[3] = new Array("",
						   "<#WLANConfig11b_x_RadioEnable_itemdesc#>",
						   "<#WLANConfig11b_x_RadioEnableDate_itemdesc#><p><a href='/Main_LogStatus_Content.asp' target='_blank'><#General_x_SystemTime_itemname#><#btn_go#></a></p>",
						   "<#WLANConfig11b_x_RadioEnableTime_itemdesc#><p><a href='/Main_LogStatus_Content.asp' target='_blank'><#General_x_SystemTime_itemname#><#btn_go#></a></p>",						   
						   "<#WLANConfig11b_x_AfterBurner_itemdesc#>",
						   "<#WLANConfig11b_x_IsolateAP_itemdesc#>",
						   "<#WLANConfig11b_DataRateAll_itemdesc#>",
							 "<#WLANConfig11b_MultiRateAll_itemdesc#>",
							 "<#WLANConfig11b_DataRate_itemdesc#>",
						   "<#WLANConfig11b_x_Frag_itemdesc#>",
						   "<#WLANConfig11b_x_RTS_itemdesc#>",
						   "<#WLANConfig11b_x_DTIM_itemdesc#>",
						   "<#WLANConfig11b_x_Beacon_itemdesc#>",
						   "<#WLANConfig11b_x_TxBurst_itemdesc#>",
						   "<#WLANConfig11b_x_WMM_itemdesc#>",
						   "<#WLANConfig11b_x_NOACK_itemdesc#>",
						   "<#WLANConfig11b_x_PktAggregate_itemdesc#>",
						   "<#WLANConfig11b_x_APSD_itemdesc#>",
						   "<#WLANConfig11b_x_DLS_itemdesc#>",
						   "[n Only]: <#WLANConfig11b_x_HT_OpMode_itemdesc#>",
						   "<#WLANConfig11n_PremblesType_itemdesc#>",	
						   "Enabling this option will improve router performance in an environment where there is more interference, but it might reduce connection stability under some circumstances." //21
						   //"<#WLANConfig11b_x_Xpress_itemdesc#>");
						   );
helpcontent[4] = new Array("",
						   "<#LANHostConfig_IPRouters_itemdesc#>",
						   "<#LANHostConfig_SubnetMask_itemdesc#>",
						   "<#LANHostConfig_x_Gateway_itemdesc#>");
helpcontent[5] = new Array("",
							 "<#LANHostConfig_DHCPServerConfigurable_itemdesc#>",
							 "<#LANHostConfig_DomainName_itemdesc#><#LANHostConfig_DomainName_itemdesc2#>",
							 "<#LANHostConfig_MinAddress_itemdesc#>",
							 "<#LANHostConfig_MaxAddress_itemdesc#>",
							 "<#LANHostConfig_LeaseTime_itemdesc#>",
							 "<#LANHostConfig_x_LGateway_itemdesc#>",
							 "<#LANHostConfig_x_LDNSServer1_itemdesc#>",
							 "<#LANHostConfig_x_WINSServer_itemdesc#>",
							 "<#LANHostConfig_ManualDHCPEnable_itemdesc#>",
							 "<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>",
							 "<#LANHostConfig_ManualDHCPMulticast_itemdesc#>",
							 "<#LANHostConfig_ManualDHCPSTB_itemdesc#>",
							 "<#LANHostConfig_x_DDNSHostNames_itemdesc#>");
helpcontent[6] = new Array("",
						   "<#RHELP_desc4#>",
						   "<#RHELP_desc5#>",
						   "<#RHELP_desc6#>",
						   "<#RHELP_desc7#>",
						   "<#RHELP_desc8#>",
						   "<#RHELP_desc9#>",
						   "<#RouterConfig_GWMulticast_Multicast_all_itemdesc#>");
//WAN
helpcontent[7] = new Array("",
							 "<#IPConnection_ExternalIPAddress_itemdesc#>",
							 "<#IPConnection_x_ExternalSubnetMask_itemdesc#>",
							 "<#IPConnection_x_ExternalGateway_itemdesc#>",
							 "<#PPPConnection_UserName_itemdesc#>",
							 "<#PPPConnection_Password_itemdesc#>",
							 "<#PPPConnection_IdleDisconnectTime_itemdesc#>",
							 "<#PPPConnection_x_PPPoEMTU_itemdesc#>",
							 "<#PPPConnection_x_PPPoEMRU_itemdesc#>",
							 "<#PPPConnection_x_ServiceName_itemdesc#>",
							 "<#PPPConnection_x_AccessConcentrator_itemdesc#>",
							 "<#PPPConnection_x_PPPoERelay_itemdesc#>",
							 "<#IPConnection_x_DNSServerEnable_itemdesc#>",
							 "<#IPConnection_x_DNSServer1_itemdesc#>",
							 "<#IPConnection_x_DNSServer2_itemdesc#>",
							 "<#BOP_isp_host_desc#>",
							 "<#PPPConnection_x_MacAddressForISP_itemdesc#>",
							 "<#PPPConnection_x_PPTPOptions_itemdesc#>",
							 "<#PPPConnection_x_AdditionalOptions_itemdesc#>",
							 "<#BOP_isp_heart_desc#>",
							 "<#IPConnection_BattleNet_itemdesc#>",
							 "<#Layer3Forwarding_x_STB_itemdesc#>",
							 "<#hwnat_desc#>",
							 "<#IPConnection_UPnP_itemdesc#>",
							 "<#IPConnection_PortRange_itemdesc#>",
							 "<#IPConnection_LocalIP_itemdesc#>",
							 "<#IPConnection_LocalPort_itemdesc#>",
							 "<#qis_pppoe_help1#>",
							 "<#isp_profile#>",
							 "<#PPPConnection_Authentication_itemdesc#>");
//Firewall
helpcontent[8] = new Array("",
						   "<#FirewallConfig_WanLanLog_itemdesc#>",
						   "<#FirewallConfig_x_WanWebEnable_itemdesc#>",
						   "<#FirewallConfig_x_WanWebPort_itemdesc#>",
						   "<#FirewallConfig_x_WanLPREnable_itemdesc#>",
						   "<#FirewallConfig_x_WanPingEnable_itemdesc#>",
						   "<#FirewallConfig_FirewallEnable_itemdesc#>",
						   "<#FirewallConfig_DoSEnable_itemdesc#>");
helpcontent[9] = new Array("",
						   "<#FirewallConfig_URLActiveDate_itemdesc#>",
						   "<#FirewallConfig_URLActiveTime_itemdesc#>");
helpcontent[10] = new Array("",
							"<#FirewallConfig_LanWanActiveDate_itemdesc#>",
							"<#FirewallConfig_LanWanActiveTime_itemdesc#>",
							"<#FirewallConfig_LanWanDefaultAct_itemdesc#>",
							"<#FirewallConfig_LanWanICMP_itemdesc#>",
							"<#FirewallConfig_LanWanFirewallEnable_itemdesc#>");
//Administration
helpcontent[11] = new Array("",
							"<#LANHostConfig_x_ServerLogEnable_itemdesc#>",
							"<#LANHostConfig_x_TimeZone_itemdesc#>",
							"<#LANHostConfig_x_NTPServer_itemdesc#>",
							"<#LANHostConfig_x_Password_itemdesc#>");
//Log
helpcontent[12] = new Array("",
							"<#General_x_SystemUpTime_itemdesc#>",
							"<#PrinterStatus_x_PrinterModel_itemdesc#>",
							"<#PrinterStatus_x_PrinterStatus_itemdesc#>",
							"<#PrinterStatus_x_PrinterUser_itemdesc#>");
//WPS
helpcontent[13] = new Array("",
							"<#WLANConfig11b_x_WPS_itemdesc#>",
							"<#WLANConfig11b_x_WPSMode_itemdesc#>",
							"<#WLANConfig11b_x_WPSPIN_itemdesc#>",
							"<#WLANConfig11b_x_DevicePIN_itemdesc#>",
							"<#WLANConfig11b_x_WPSband_itemdesc#>");
//UPnP
helpcontent[14] = new Array("",
							"<#UPnPMediaServer_Help#>");
//AiDisk Wizard
helpcontent[15] = new Array("",
							"", /*<#AiDisk_moreconfig#>*/
							"<#AiDisk_Step1_help#><p><a href='../Advanced_AiDisk_ftp.asp' target='_parent' hidefocus='true'><#MoreConfig#></a></p><!--span style='color:red'><#AiDisk_Step1_help2#></span-->",
							"<#AiDisk_Step2_help#>",
							"<#AiDisk_Step3_help#>");
//EzQoS
helpcontent[16] = new Array("",
							"<#EZQoSDesc1#><p><#EZQoSDesc2#> <a href='/QoS_EZQoS.asp'><#BM_title_User#></a></p>");
//Others in the USB application
helpcontent[17] = new Array("",
							"<#JS_storageMLU#>",
							"<#JS_storageright#>",
							"<#Help_of_Workgroup#>",
							"<#JS_basiconfig1#>",
							"<#JS_basiconfig3#>",
							"<#JS_basiconfig8#>",
							"<#ShareNode_Seeding_itemdesc#>",
							"<#ShareNode_MaxUpload_itemdesc#>",
							"<#ShareNode_FTPLANG_itemdesc#>");
// MAC filter
helpcontent[18] = new Array("",
							"<#FirewallConfig_MFMethod_itemdesc#>",
							"<#Port_format#>",
							"<#IP_format#>",
							"<#Port_format#>",
							"<#UserQoS_transferred_hint#>",
							"<#source_ipmac#>");
// Setting
helpcontent[19] = new Array("",
							"<#Setting_factorydefault_itemdesc#>",
							"<#Setting_save_itemdesc#>",
							"<#Setting_upload_itemdesc#>");
// QoS
helpcontent[20] = new Array("",
							"<#BM_measured_uplink_speed_desc#>",
							"<#BM_manual_uplink_speed_desc#>",
							"<#min_bound_desc#>",
							"<#max_bound_desc#>",
							"<#bound_zero_desc#>");
// HSDPA
helpcontent[21] = new Array("",
							"<#HSDPAConfig_hsdpa_mode_itemdesc#>",
							"<#HSDPAConfig_pin_code_itemdesc#>",
							"<#HSDPAConfig_private_apn_itemdesc#>",
							"HSDPA<#PPPConnection_x_PPPoEMTU_itemdesc#>",
							"HSDPA<#PPPConnection_x_PPPoEMRU_itemdesc#>",
							"<#IPConnection_x_DNSServer1_itemdesc#>",
							"<#IPConnection_x_DNSServer2_itemdesc#>",
							"<#HSDPAConfig_isp_itemdesc#>",
							"<#HSDPAConfig_country_itemdesc#>",
							"<#HSDPAConfig_dialnum_itemdesc#>",
							"<#HSDPAConfig_username_itemdesc#>",
							"<#HSDPAConfig_password_itemdesc#>",
							"<#HSDPAConfig_usbadaptor_itemdesc#>");
							
helpcontent[22] = new Array("",
							"<#OP_GW_desc#>",
							"<#OP_GW_desc#>",
							"<#OP_AP_desc#>");
// title
helpcontent[23] = new Array("",
							"",
							"",
							"<#statusTitle_Internet#>",
							"<#statusTitle_USB_Disk#>",
							"<#statusTitle_Printer#>");

var clicked_help_string = "<#Help_init_word1#> <a class=\"hintstyle\" style=\"background-color:#7aa3bd\"><#Help_init_word2#></a> <#Help_init_word3#>";

function suspendconn(wanenable){
	document.internetForm_title.wan_enable.value = wanenable;
	showLoading();
	document.internetForm_title.submit();	
}

function enableMonomode(){
	showLoading(2);
	document.titleForm.action = "/apply.cgi";
	document.titleForm.action_mode.value = "mfp_monopolize";
	document.titleForm.current_page.value = "/device-map/printer.asp";
	document.form.target = "hidden_frame";
	document.titleForm.submit();
}

function remove_disk(disk_num){
	var str = "<#Safelyremovedisk_confirm#>";
	if(confirm(str)){
		showLoading();		
		document.diskForm_title.disk.value = disk_num;
		setTimeout("document.diskForm_title.submit();", 1);
	}
}

function gotoguestnetwork(){
	top.location.href = "/Guest_network.asp";
}

function gotocooler(){
	top.location.href = "/Advanced_PerformanceTuning_Content.asp";
}

<% available_disk_names_and_sizes(); %>
function overHint(itemNum){
	var statusmenu = "";
	var title2 = 0;
	var title5 = 0;

	<%radio_status();%>

	// wifi hw switch
	if(itemNum == 8){
		statusmenu = "<div class='StatusHint'>Wi-Fi:</div>";

		if(wifi_hw_sw_support != -1)
		{
			if(wifi_hw_switch == "wifi_hw_switch=0")
				wifiDesc = "Wi-Fi=Disabled"
			else
				wifiDesc = "Wi-Fi=Enabled"

		} else {	// Report radio states
			if ( radio_2 )
				radiostate = "2.4G: Enabled"
			else
				radiostate = "2.4G: Disabled"

			if (band5g_support != -1) {
				if ( radio_5)
					radiostate += "<br>&nbsp;&nbsp;5G: Enabled"
				else
					radiostate += "<br>&nbsp;&nbsp;5G: Disabled"
			}
			wifiDesc="Wi-Fi:"+radiostate;

		}
		statusmenu += "<span>" + wifiDesc.substring(6, wifiDesc.length) + "</span>";
	}	
	
	// cooler
	if(itemNum == 7){
		statusmenu = "<div class='StatusHint'>Cooler:</div>";
		if(cooler == "cooler=0")
			coolerDesc = "cooler=Disabled"
		else if(cooler == "cooler=1")
			coolerDesc = "cooler=Manually"
		else
			coolerDesc = "cooler=Automatic"

		statusmenu += "<span>" + coolerDesc.substring(7, coolerDesc.length) + "</span>";
	}

	// printer
	if(itemNum == 6){
		statusmenu = "<div class='StatusHint'><#Printing_button_item#></div>";
		if(monoClient == "monoClient=")
			monoClient = "monoClient=<#CTL_Disabled#>"
		statusmenu += "<span>" + monoClient.substring(11, monoClient.length) + "</span>";
	}
	if(itemNum == 5){
		statusmenu = "<span class='StatusHint'><#no_printer_detect#></span>";	
	}

	// guest network
	if(itemNum == 4){
		for(var i=0; i<gn_array_2g.length; i++){
			if(gn_array_2g[i][0] == 1){
				if(title2 == 0){
					statusmenu += "<div class='StatusHint'>2.4GHz Network:</div>";
					title2 = 1;
				}

				statusmenu += "<span>" + gn_array_2g[i][1] + " (";

				if(gn_array_2g[i][11] == 0)
					statusmenu += '<#Limitless#>';
				else{
					var expire_hr = Math.floor(gn_array_2g[i][13]/3600);
					var expire_min = Math.floor((gn_array_2g[i][13]%3600)/60);
					if(expire_hr > 0)
						statusmenu += '<b id="expire_hr_'+i+'">'+ expire_hr + '</b> Hr <b id="expire_min_'+i+'">' + expire_min +'</b> Min';
					else
						statusmenu += '<b id="expire_min_'+i+'">' + expire_min +'</b> Min';
				}

				statusmenu += " left)</span><br>";
			}
		}
		if(band5g_support != -1){
			for(var i=0; i<gn_array_2g.length; i++){
				if(gn_array_5g[i][0] == 1){
					if(title5 == 0){
						statusmenu += "<div class='StatusHint' style='margin-top:15px;'>5GHz Network:</div>";				
						title5 = 1;
					}
	
					statusmenu += "<span>" + gn_array_5g[i][1] + " (";

					if(gn_array_5g[i][11] == 0)
						statusmenu += '<#Limitless#>';
					else{
						var expire_hr = Math.floor(gn_array_5g[i][13]/3600);
						var expire_min = Math.floor((gn_array_5g[i][13]%3600)/60);
						if(expire_hr > 0)
							statusmenu += '<b id="expire_hr_'+i+'">'+ expire_hr + '</b> Hr <b id="expire_min_'+i+'">' + expire_min +'</b> Min';
						else
							statusmenu += '<b id="expire_min_'+i+'">' + expire_min +'</b> Min';
					}

					statusmenu += " left)</span><br>";
				}
			}
		}
		if(title2 == 0 && title5 == 0)
			statusmenu += "<div class='StatusHint'><#Guest_Network#>:</div><span><#CTL_Disabled#></span>";
	}

	// internet
	if(itemNum == 3){
		if((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2")){
			statusmenu = "<div class='StatusHint'><#statusTitle_Internet#>:</div>";
			statusmenu += "<span><#Connected#></span>";
		}
		else{
			if(sw_mode == 1){
				if(link_auxstatus == 1)
					statusmenu = "<span class='StatusHint'><#QKSet_detect_wanconnfault#></span>";
				else if(link_sbstatus == 1)
					statusmenu = "<span class='StatusHint'><#web_redirect_reason3_2#></span>";
				else if(link_sbstatus == 2)
					statusmenu = "<span class='StatusHint'><#QKSet_Internet_Setup_fail_reason2#></span>";
				else if(link_sbstatus == 3)
					statusmenu = "<span class='StatusHint'><#QKSet_Internet_Setup_fail_reason1#></span>";
				else if(link_sbstatus == 4)
					statusmenu = "<span class='StatusHint'><#web_redirect_reason5_2#></span>";
				else if(link_sbstatus == 5)
					statusmenu = "<span class='StatusHint'><#web_redirect_reason5_1#></span>";
				else if(link_sbstatus == 6)
					statusmenu = "<span class='StatusHint'>WAN_STOPPED_SYSTEM_ERROR</span>";
				else
					statusmenu = "<span class='StatusHint'><#web_redirect_reason2_2#></span>";
			}
			else if(sw_mode == 2){
				if(_wlc_state == "wlc_state=2")
					statusmenu = "<span class='StatusHint'><#APSurvey_msg_connected#></span>";
				else{
					if(_wlc_sbstate == "wlc_sbstate=2")
						statusmenu = "<span class='StatusHint'><#APSurvey_action_ConnectingStatus1#></span>";
					else
						statusmenu = "<span class='StatusHint'><#APSurvey_action_ConnectingStatus0#></span>";
				}
			}
		}
	}

	// usb storage
	if(itemNum == 2){
		var dmStatus = "Not installed";
		if(nodm_support != -1){
			var dmStatus = "Not support.";
		}

		var apps_dev = '<% nvram_get("apps_dev"); %>';

		if(foreign_disk_total_mounted_number()[0] == null){
			statusmenu = "<div class='StatusHint'><#no_usb_found#></div>";
		}
		else if(foreign_disk_total_mounted_number()[0] == "0" && foreign_disk_total_mounted_number()[foreign_disk_total_mounted_number().length-1] == "0"){
			statusmenu = "<span class='StatusHint'><#usb_unmount#></span>";	
		}
		else{
			statusmenu = "<div class='StatusHint'>Download Master:</div>";				
			if(getCookie_help("dm_install") == null || getCookie_help("dm_enable") == null)
				dmStatus = "Please check USB Application";
			else if(getCookie_help("dm_install") == "yes" && getCookie_help("dm_enable") == "yes"){
				//dmStatus = "Enabled";
				if(pool_devices() != ""){ //  avoid no disk error
					partitions_array = pool_devices(); 
					for(var i = 0; i < partitions_array.length; i++){
						if(apps_dev == partitions_array[i]){
							if(i > foreign_disk_total_mounted_number()[0]){
								dmStatus = "" + decodeURIComponent(foreign_disks()[1]) + "";
								DMDiskNum = foreign_disk_interface_names()[0];
							}
							else{
								dmStatus = "" + decodeURIComponent(foreign_disks()[0]) + "";
								DMDiskNum = foreign_disk_interface_names()[1];
							}
						}
					}
				}
			}
			else if(getCookie_help("dm_install") == "no"){
				dmStatus = "Not installed";		
				if(nodm_support != -1){
					dmStatus = "Not support.";
				}
			}
			else if(getCookie_help("dm_enable") == "no" && getCookie_help("dm_install") == "yes")
				dmStatus = "Disabled";
			statusmenu += "<span>"+ dmStatus +"</span>";

			if(usb_path1 == "usb=modem" || usb_path2 == "usb=modem"){
				statusmenu += "<div class='StatusHint'><#HSDPAConfig_USBAdapter_itemname#>:</div>";
				statusmenu += "<span>On</span>";
			}
		}
	}

	return overlib(statusmenu, OFFSETX, -160, LEFT, DELAY, 400);
}

var usb_path_tmp = new Array('usb=<% nvram_get("usb_path1"); %>', 'usb=<% nvram_get("usb_path2"); %>');
function openHint(hint_array_id, hint_show_id, flag){
	if(hint_array_id == 24){
		var _caption = "";

		if(hint_show_id == 5){
			statusmenu = "<span class='StatusClickHint' onclick='gotocooler();' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'>Go to Performance tuning</span>";
			_caption = "Perfomance Tuning";
		}
		else if(hint_show_id == 4){
			statusmenu = "<span class='StatusClickHint' onclick='gotoguestnetwork();' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#go_GuestNetwork#></span>";
			_caption = "Guest Network";
		}
		else if(hint_show_id == 3){
			if(sw_mode == 1){
				if((link_status == "2" && link_auxstatus == "0") || (link_status == "2" && link_auxstatus == "2"))
					statusmenu = "<span class='StatusClickHint' onclick='suspendconn(0);' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#disconnect_internet#></span>";
				else if(link_status == 5)
					statusmenu = "<span class='StatusClickHint' onclick='suspendconn(1);' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'>Resume internet</span>";
				else{
					if(link_auxstatus == 1)
						statusmenu = "<span class='StatusHint'><#QKSet_detect_wanconnfault#></span>";
					else if(link_sbstatus == 1)
						statusmenu = "<span class='StatusHint'><#web_redirect_reason3_2#></span>";
					else if(link_sbstatus == 2)
						statusmenu = "<span class='StatusHint'><#QKSet_Internet_Setup_fail_reason2#></span>";
					else if(link_sbstatus == 3)
						statusmenu = "<span class='StatusHint'><#QKSet_Internet_Setup_fail_reason1#></span>";
					else if(link_sbstatus == 4)
						statusmenu = "<span class='StatusHint'><#web_redirect_reason5_2#></span>";
					else if(link_sbstatus == 5)
						statusmenu = "<span class='StatusHint'><#web_redirect_reason5_1#></span>";
					else if(link_sbstatus == 6)
						statusmenu = "<span class='StatusHint'>WAN_STOPPED_SYSTEM_ERROR</span>";
					else
						statusmenu = "<span class='StatusHint'><#web_redirect_reason2_2#></span>";
				}
			}
			else if(sw_mode == 2){
				if(psta_support == -1)
					statusmenu = "<span class='StatusClickHint' onclick='top.location.href=\"http://www.asusnetwork.net/QIS_wizard.htm?flag=sitesurvey\";' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#APSurvey_action_search_again_hint2#></span>";
				else
					statusmenu = "<span class='StatusClickHint' onclick='top.location.href=\"/QIS_wizard.htm?flag=sitesurvey\";' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#APSurvey_action_search_again_hint2#></span>";
			}
			_caption = "Internet Status";
		}
		else if(hint_show_id == 2){
			var statusmenu = "";

			for(i=0; i<foreign_disk_interface_names().length; i++){
				if(foreign_disk_total_mounted_number()[0] == ""){
					statusmenu = "<span class='StatusHint'><#no_usb_found#></span>";
					break;
				}

				if(foreign_disk_interface_names()[i] == 1)
					_foreign_disk_interface_name = 1;
				else
					_foreign_disk_interface_name = 2;

				var usb_path_curr = eval("usb_path"+_foreign_disk_interface_name);
				if(foreign_disk_total_mounted_number()[i] != "0" && foreign_disk_total_mounted_number()[i] != "" && usb_path_curr != "usb=")
					statusmenu += "<div style='margin-top:2px;' class='StatusClickHint' onclick='remove_disk("+parseInt(foreign_disk_interface_names()[i])+");' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#Eject_usb_disk#> <span style='font-weight:normal'>"+ decodeURIComponent(foreign_disks()[i]) +"</span></div>";
				else 
					continue;
			}
			if(current_url!="index.asp" && current_url!=""){
				if((usb_path_tmp[0] != usb_path1) && usb_path1 != "usb=" && usb_path1 != "usb=printer")
					statusmenu += "<div style='margin-top:2px;' class='StatusClickHint' onclick='remove_disk(1);' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#Eject_usb_disk#> <span style='font-weight:normal'>USB Disk 1</span></div>";
				if((usb_path_tmp[1] != usb_path2) && usb_path2 != "usb=" && usb_path2 != "usb=printer")
					statusmenu += "<div style='margin-top:2px;' class='StatusClickHint' onclick='remove_disk(2);' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'><#Eject_usb_disk#> <span style='font-weight:normal'>USB Disk 2</span></div>";
			}

			_caption = "USB storage";
		}
		else if(hint_show_id == 1){
			if(usb_path1 == "usb=printer" || usb_path2 == "usb=printer")
				statusmenu = "<span class='StatusClickHint' onclick='enableMonomode();' onmouseout='this.className=\"StatusClickHint\"' onmouseover='this.className=\"StatusClickHint_mouseover\"'>Enable Monopoly mode</span>";
			else
				statusmenu = "<span class='StatusHint'><#no_printer_detect#></span>";	
			_caption = "Printer";
		}

		return overlib(statusmenu, OFFSETX, -160, LEFT, STICKY, CAPTION, " ", CLOSETITLE, '');
	}

	var tag_name= document.getElementsByTagName('a');	
	for (var i=0;i<tag_name.length;i++)
		tag_name[i].onmouseout=nd;
	
	if(hint_array_id == 0 && hint_show_id > 21)
		return overlib(helpcontent[hint_array_id][hint_show_id], FIXX, 270, FIXY, 30);
	else
		return overlib(helpcontent[hint_array_id][hint_show_id], HAUTO, VAUTO);
}

//\/////
//\  overLIB 4.21 - You may not remove or change this notice.
//\  Copyright Erik Bosrup 1998-2004. All rights reserved.
//\
//\  Contributors are listed on the homepage.
//\  This file might be old, always check for the latest version at:
//\  http://www.bosrup.com/web/overlib/
//\
//\  Please read the license agreement (available through the link above)
//\  before using overLIB. Direct any licensing questions to erik@bosrup.com.
//\
//\  Do not sell this as your own work or remove this copyright notice. 
//\  For full details on copying or changing this script please read the
//\  license agreement at the link above. Please give credit on sites that
//\  use overLIB and submit changes of the script so other people can use
//\  them as well.
//   $Revision: 1.119 $                $Date: 2005/07/02 23:41:44 $
//\/////
//\mini

////////
// PRE-INIT
// Ignore these lines, configuration is below.
////////
var olLoaded = 0;var pmStart = 10000000; var pmUpper = 10001000; var pmCount = pmStart+1; var pmt=''; var pms = new Array(); var olInfo = new Info('4.21', 1);
var FREPLACE = 0; var FBEFORE = 1; var FAFTER = 2; var FALTERNATE = 3; var FCHAIN=4;
var olHideForm=0;  // parameter for hiding SELECT and ActiveX elements in IE5.5+ 
var olHautoFlag = 0;  // flags for over-riding VAUTO and HAUTO if corresponding
var olVautoFlag = 0;  // positioning commands are used on the command line
var hookPts = new Array(), postParse = new Array(), cmdLine = new Array(), runTime = new Array();
// for plugins
registerCommands('donothing,inarray,caparray,sticky,background,noclose,caption,left,right,center,offsetx,offsety,fgcolor,bgcolor,textcolor,capcolor,closecolor,width,border,cellpad,status,autostatus,autostatuscap,height,closetext,snapx,snapy,fixx,fixy,relx,rely,fgbackground,bgbackground,padx,pady,fullhtml,above,below,capicon,textfont,captionfont,closefont,textsize,captionsize,closesize,timeout,function,delay,hauto,vauto,closeclick,wrap,followmouse,mouseoff,closetitle,cssoff,compatmode,cssclass,fgclass,bgclass,textfontclass,captionfontclass,closefontclass');

////////
// DEFAULT CONFIGURATION
// Settings you want everywhere are set here. All of this can also be
// changed on your html page or through an overLIB call.
////////
if (typeof ol_fgcolor=='undefined') var ol_fgcolor="#EEEEEE";
if (typeof ol_bgcolor=='undefined') var ol_bgcolor="#CCC";
if (typeof ol_textcolor=='undefined') var ol_textcolor="#000000";
if (typeof ol_capcolor=='undefined') var ol_capcolor="#777";
if (typeof ol_closecolor=='undefined') var ol_closecolor="#000000";
if (typeof ol_textfont=='undefined') var ol_textfont="Verdana,Arial,Helvetica";
if (typeof ol_captionfont=='undefined') var ol_captionfont="Verdana,Arial,Helvetica";
if (typeof ol_closefont=='undefined') var ol_closefont="Verdana,Arial,Helvetica";
if (typeof ol_textsize=='undefined') var ol_textsize="2";
if (typeof ol_captionsize=='undefined') var ol_captionsize="1";
if (typeof ol_closesize=='undefined') var ol_closesize="2";
if (typeof ol_width=='undefined') var ol_width="200";
if (typeof ol_border=='undefined') var ol_border="1";
if (typeof ol_cellpad=='undefined') var ol_cellpad=6;
if (typeof ol_offsetx=='undefined') var ol_offsetx=10;
if (typeof ol_offsety=='undefined') var ol_offsety=10;
if (typeof ol_text=='undefined') var ol_text="Default Text";
if (typeof ol_cap=='undefined') var ol_cap="";
if (typeof ol_sticky=='undefined') var ol_sticky=0;
if (typeof ol_background=='undefined') var ol_background="";
if (typeof ol_close=='undefined') var ol_close="&times;&nbsp;";
if (typeof ol_hpos=='undefined') var ol_hpos=RIGHT;
if (typeof ol_status=='undefined') var ol_status="";
if (typeof ol_autostatus=='undefined') var ol_autostatus=0;
if (typeof ol_height=='undefined') var ol_height=-1;
if (typeof ol_snapx=='undefined') var ol_snapx=0;
if (typeof ol_snapy=='undefined') var ol_snapy=0;
if (typeof ol_fixx=='undefined') var ol_fixx=-1;
if (typeof ol_fixy=='undefined') var ol_fixy=-1;
if (typeof ol_relx=='undefined') var ol_relx=null;
if (typeof ol_rely=='undefined') var ol_rely=null;
if (typeof ol_fgbackground=='undefined') var ol_fgbackground="";
if (typeof ol_bgbackground=='undefined') var ol_bgbackground="";
if (typeof ol_padxl=='undefined') var ol_padxl=1;
if (typeof ol_padxr=='undefined') var ol_padxr=1;
if (typeof ol_padyt=='undefined') var ol_padyt=1;
if (typeof ol_padyb=='undefined') var ol_padyb=1;
if (typeof ol_fullhtml=='undefined') var ol_fullhtml=0;
if (typeof ol_vpos=='undefined') var ol_vpos=BELOW;
if (typeof ol_aboveheight=='undefined') var ol_aboveheight=0;
if (typeof ol_capicon=='undefined') var ol_capicon="";
if (typeof ol_frame=='undefined') var ol_frame=self;
if (typeof ol_timeout=='undefined') var ol_timeout=0;
if (typeof ol_function=='undefined') var ol_function=null;
if (typeof ol_delay=='undefined') var ol_delay=0;
if (typeof ol_hauto=='undefined') var ol_hauto=0;
if (typeof ol_vauto=='undefined') var ol_vauto=0;
if (typeof ol_closeclick=='undefined') var ol_closeclick=0;
if (typeof ol_wrap=='undefined') var ol_wrap=0;
if (typeof ol_followmouse=='undefined') var ol_followmouse=1;
if (typeof ol_mouseoff=='undefined') var ol_mouseoff=0;
if (typeof ol_closetitle=='undefined') var ol_closetitle='Close';
if (typeof ol_compatmode=='undefined') var ol_compatmode=0;
if (typeof ol_css=='undefined') var ol_css=CSSOFF;
if (typeof ol_fgclass=='undefined') var ol_fgclass="";
if (typeof ol_bgclass=='undefined') var ol_bgclass="";
if (typeof ol_textfontclass=='undefined') var ol_textfontclass="";
if (typeof ol_captionfontclass=='undefined') var ol_captionfontclass="";
if (typeof ol_closefontclass=='undefined') var ol_closefontclass="";

////////
// ARRAY CONFIGURATION
////////

// You can use these arrays to store popup text here instead of in the html.
if (typeof ol_texts=='undefined') var ol_texts = new Array("Text 0", "Text 1");
if (typeof ol_caps=='undefined') var ol_caps = new Array("Caption 0", "Caption 1");

////////
// END OF CONFIGURATION
// Don't change anything below this line, all configuration is above.
////////





////////
// INIT
////////
// Runtime variables init. Don't change for config!
var o3_text="";
var o3_cap="";
var o3_sticky=0;
var o3_background="";
var o3_close="Close";
var o3_hpos=RIGHT;
var o3_offsetx=2;
var o3_offsety=2;
var o3_fgcolor="";
var o3_bgcolor="";
var o3_textcolor="";
var o3_capcolor="";
var o3_closecolor="";
var o3_width=100;
var o3_border=1;
var o3_cellpad=2;
var o3_status="";
var o3_autostatus=0;
var o3_height=-1;
var o3_snapx=0;
var o3_snapy=0;
var o3_fixx=-1;
var o3_fixy=-1;
var o3_relx=null;
var o3_rely=null;
var o3_fgbackground="";
var o3_bgbackground="";
var o3_padxl=0;
var o3_padxr=0;
var o3_padyt=0;
var o3_padyb=0;
var o3_fullhtml=0;
var o3_vpos=BELOW;
var o3_aboveheight=0;
var o3_capicon="";
var o3_textfont="Verdana,Arial,Helvetica";
var o3_captionfont="Verdana,Arial,Helvetica";
var o3_closefont="Verdana,Arial,Helvetica";
var o3_textsize="1";
var o3_captionsize="1";
var o3_closesize="1";
var o3_frame=self;
var o3_timeout=0;
var o3_timerid=0;
var o3_allowmove=0;
var o3_function=null; 
var o3_delay=0;
var o3_delayid=0;
var o3_hauto=0;
var o3_vauto=0;
var o3_closeclick=0;
var o3_wrap=0;
var o3_followmouse=1;
var o3_mouseoff=0;
var o3_closetitle='';
var o3_compatmode=0;
var o3_css=CSSOFF;
var o3_fgclass="";
var o3_bgclass="";
var o3_textfontclass="";
var o3_captionfontclass="";
var o3_closefontclass="";

// Display state variables
var o3_x = 0;
var o3_y = 0;
var o3_showingsticky = 0;
var o3_removecounter = 0;

// Our layer
var over = null;
var fnRef, hoveringSwitch = false;
var olHideDelay;

// Decide browser version
var isMac = (navigator.userAgent.indexOf("Mac") != -1);
var olOp = (navigator.userAgent.toLowerCase().indexOf('opera') > -1 && document.createTextNode);  // Opera 7
var olNs4 = (navigator.appName=='Netscape' && parseInt(navigator.appVersion) == 4);
var olNs6 = (document.getElementById) ? true : false;
var olKq = (olNs6 && /konqueror/i.test(navigator.userAgent));
var olIe4 = (document.all) ? true : false;
var olIe5 = false; 
var olIe55 = false; // Added additional variable to identify IE5.5+
var docRoot = 'document.body';

// Resize fix for NS4.x to keep track of layer
if (olNs4) {
	var oW = window.innerWidth;
	var oH = window.innerHeight;
	window.onresize = function() { if (oW != window.innerWidth || oH != window.innerHeight) location.reload(); }
}

// Microsoft Stupidity Check(tm).
if (olIe4) {
	var agent = navigator.userAgent;
	if (/MSIE/.test(agent)) {
		var versNum = parseFloat(agent.match(/MSIE[ ](\d\.\d+)\.*/i)[1]);
		if (versNum >= 5){
			olIe5=true;
			olIe55=(versNum>=5.5&&!olOp) ? true : false;
			if (olNs6) olNs6=false;
		}
	}
	if (olNs6) olIe4 = false;
}

// Check for compatability mode.
if (document.compatMode && document.compatMode == 'CSS1Compat') {
	docRoot= ((olIe4 && !olOp) ? 'document.documentElement' : docRoot);
}

// Add window onload handlers to indicate when all modules have been loaded
// For Netscape 6+ and Mozilla, uses addEventListener method on the window object
// For IE it uses the attachEvent method of the window object and for Netscape 4.x
// it sets the window.onload handler to the OLonload_handler function for Bubbling
if(window.addEventListener) window.addEventListener("load",OLonLoad_handler,false);
else if (window.attachEvent) window.attachEvent("onload",OLonLoad_handler);

var capExtent;

////////
// PUBLIC FUNCTIONS
////////

// overlib(arg0,...,argN)
// Loads parameters into global runtime variables.
function overlib() {
	if (!olLoaded || isExclusive(overlib.arguments)) return true;
	if (olCheckMouseCapture) olMouseCapture();
	if (over) {
		over = (typeof over.id != 'string') ? o3_frame.document.all['overDiv'] : over;
		cClick();
	}

	// Load defaults to runtime.
  olHideDelay=0;
	o3_text=ol_text;
	o3_cap=ol_cap;
	o3_sticky=ol_sticky;
	o3_background=ol_background;
	o3_close=ol_close;
	o3_hpos=ol_hpos;
	o3_offsetx=ol_offsetx;
	o3_offsety=ol_offsety;
	o3_fgcolor=ol_fgcolor;
	o3_bgcolor=ol_bgcolor;
	o3_textcolor=ol_textcolor;
	o3_capcolor=ol_capcolor;
	o3_closecolor=ol_closecolor;
	o3_width=ol_width;
	o3_border=ol_border;
	o3_cellpad=ol_cellpad;
	o3_status=ol_status;
	o3_autostatus=ol_autostatus;
	o3_height=ol_height;
	o3_snapx=ol_snapx;
	o3_snapy=ol_snapy;
	o3_fixx=ol_fixx;
	o3_fixy=ol_fixy;
	o3_relx=ol_relx;
	o3_rely=ol_rely;
	o3_fgbackground=ol_fgbackground;
	o3_bgbackground=ol_bgbackground;
	o3_padxl=ol_padxl;
	o3_padxr=ol_padxr;
	o3_padyt=ol_padyt;
	o3_padyb=ol_padyb;
	o3_fullhtml=ol_fullhtml;
	o3_vpos=ol_vpos;
	o3_aboveheight=ol_aboveheight;
	o3_capicon=ol_capicon;
	o3_textfont=ol_textfont;
	o3_captionfont=ol_captionfont;
	o3_closefont=ol_closefont;
	o3_textsize=ol_textsize;
	o3_captionsize=ol_captionsize;
	o3_closesize=ol_closesize;
	o3_timeout=ol_timeout;
	o3_function=ol_function;
	o3_delay=ol_delay;
	o3_hauto=ol_hauto;
	o3_vauto=ol_vauto;
	o3_closeclick=ol_closeclick;
	o3_wrap=ol_wrap;	
	o3_followmouse=ol_followmouse;
	o3_mouseoff=ol_mouseoff;
	o3_closetitle=ol_closetitle;
	o3_css=ol_css;
	o3_compatmode=ol_compatmode;
	o3_fgclass=ol_fgclass;
	o3_bgclass=ol_bgclass;
	o3_textfontclass=ol_textfontclass;
	o3_captionfontclass=ol_captionfontclass;
	o3_closefontclass=ol_closefontclass;
	
	setRunTimeVariables();
	
	fnRef = '';
	
	// Special for frame support, over must be reset...
	o3_frame = ol_frame;
	
	if(!(over=createDivContainer())) return false;

	parseTokens('o3_', overlib.arguments);
	if (!postParseChecks()) return false;

	if (o3_delay == 0) {
		return runHook("olMain", FREPLACE);
 	} else {
		o3_delayid = setTimeout("runHook('olMain', FREPLACE)", o3_delay);
		return false;
	}
}

// Clears popups if appropriate
function nd(time) {
	if (olLoaded && !isExclusive()) {
		hideDelay(time);  // delay popup close if time specified

		if (o3_removecounter >= 1) { o3_showingsticky = 0 };
		
		if (o3_showingsticky == 0) {
			o3_allowmove = 0;
			if (over != null && o3_timerid == 0) runHook("hideObject", FREPLACE, over);
		} else {
			o3_removecounter++;
		}
	}
	
	return true;
}

// The Close onMouseOver function for stickies
function cClick() {
	if (olLoaded) {
		runHook("hideObject", FREPLACE, over);
		o3_showingsticky = 0;	
	}	
	return false;
}

// Method for setting page specific defaults.
function overlib_pagedefaults() {
	parseTokens('ol_', overlib_pagedefaults.arguments);
}


////////
// OVERLIB MAIN FUNCTION
////////

// This function decides what it is we want to display and how we want it done.
function olMain() {
	var layerhtml, styleType;
 	runHook("olMain", FBEFORE);
 	
	if (o3_background!="" || o3_fullhtml) {
		// Use background instead of box.
		layerhtml = runHook('ol_content_background', FALTERNATE, o3_css, o3_text, o3_background, o3_fullhtml);
	} else {
		// They want a popup box.
		styleType = (pms[o3_css-1-pmStart] == "cssoff" || pms[o3_css-1-pmStart] == "cssclass");

		// Prepare popup background
		if (o3_fgbackground != "") o3_fgbackground = "background=\""+o3_fgbackground+"\"";
		if (o3_bgbackground != "") o3_bgbackground = (styleType ? "background=\""+o3_bgbackground+"\"" : o3_bgbackground);

		// Prepare popup colors
		if (o3_fgcolor != "") o3_fgcolor = (styleType ? "bgcolor=\""+o3_fgcolor+"\"" : o3_fgcolor);
		if (o3_bgcolor != "") o3_bgcolor = (styleType ? "bgcolor=\""+o3_bgcolor+"\"" : o3_bgcolor);

		// Prepare popup height
		if (o3_height > 0) o3_height = (styleType ? "height=\""+o3_height+"\"" : o3_height);
		else o3_height = "";

		// Decide which kinda box.
		if (o3_cap=="") {
			// Plain
			layerhtml = runHook('ol_content_simple', FALTERNATE, o3_css, o3_text);
		} else {
			// With caption
			if (o3_sticky) {
				// Show close text
				layerhtml = runHook('ol_content_caption', FALTERNATE, o3_css, o3_text, o3_cap, o3_close);
			} else {
				// No close text
				layerhtml = runHook('ol_content_caption', FALTERNATE, o3_css, o3_text, o3_cap, "");
			}
		}
	}	

	// We want it to stick!
	if (o3_sticky) {
		if (o3_timerid > 0) {
			clearTimeout(o3_timerid);
			o3_timerid = 0;
		}
		o3_showingsticky = 1;
		o3_removecounter = 0;
	}

	// Created a separate routine to generate the popup to make it easier
	// to implement a plugin capability
	if (!runHook("createPopup", FREPLACE, layerhtml)) return false;

	// Prepare status bar
	if (o3_autostatus > 0) {
		o3_status = o3_text;
		if (o3_autostatus > 1) o3_status = o3_cap;
	}

	// When placing the layer the first time, even stickies may be moved.
	o3_allowmove = 0;

	// Initiate a timer for timeout
	if (o3_timeout > 0) {          
		if (o3_timerid > 0) clearTimeout(o3_timerid);
		o3_timerid = setTimeout("cClick()", o3_timeout);
	}

	// Show layer
	runHook("disp", FREPLACE, o3_status);
	runHook("olMain", FAFTER);

	return (olOp && event && event.type == 'mouseover' && !o3_status) ? '' : (o3_status != '');
}

////////
// LAYER GENERATION FUNCTIONS
////////
// These functions just handle popup content with tags that should adhere to the W3C standards specification.

// Makes simple table without caption
function ol_content_simple(text) {
	var cpIsMultiple = /,/.test(o3_cellpad);
	var txt = '<table id="overDiv_table1" width="'+o3_width+ '" border="0" cellpadding="'+o3_border+'" cellspacing="0" '+(o3_bgclass ? 'class="'+o3_bgclass+'"' : o3_bgcolor+' '+o3_height)+'><tr><td><table id="overDiv_table2" width="100%" border="0" '+((olNs4||!cpIsMultiple) ? 'cellpadding="'+o3_cellpad+'" ' : '')+'cellspacing="0" '+(o3_fgclass ? 'class="'+o3_fgclass+'"' : o3_fgcolor+' '+o3_fgbackground+' '+o3_height)+'><tr><td valign="TOP"'+(o3_textfontclass ? ' class="'+o3_textfontclass+'">' : ((!olNs4&&cpIsMultiple) ? ' style="'+setCellPadStr(o3_cellpad)+'">' : '>'))+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize))+'</td></tr></table></td></tr></table>';

	set_background("");
	return txt;
}

// Makes table with caption and optional close link
function ol_content_caption(text,title,close) {
	var nameId, txt, cpIsMultiple = /,/.test(o3_cellpad);
	var closing, closeevent;

	closing = "";
	closeevent = "onclick";
	if (o3_closeclick == 1) closeevent = (o3_closetitle ? "title='" + o3_closetitle +"'" : "") + " onclick";
	if (o3_capicon != "") {
	  nameId = ' hspace = \"5\"'+' align = \"middle\" alt = \"\"';
	  if (typeof o3_dragimg != 'undefined' && o3_dragimg) nameId =' hspace=\"5\"'+' name=\"'+o3_dragimg+'\" id=\"'+o3_dragimg+'\" align=\"middle\" alt=\"Drag Enabled\" title=\"Drag Enabled\"';
	  o3_capicon = '<img src=\"'+o3_capicon+'\"'+nameId+' />';
	}

	if (close != "")
		closing = '<td '+(!o3_compatmode && o3_closefontclass ? 'class="'+o3_closefontclass : 'align="RIGHT')+'"><a href="javascript:return '+fnRef+'cClick();"'+((o3_compatmode && o3_closefontclass) ? ' class="' + o3_closefontclass + '" ' : ' ')+closeevent+'="return '+fnRef+'cClick();"><img width="18px" height="18px" src="/images/button-close.png" onmouseover="this.src=\'/images/button-close2.png\'" onmouseout="this.src=\'/images/button-close.png\'" border="0"></a></td>';
	txt = '<table id="overDiv_table3" width="'+o3_width+ '" border="0" cellpadding="0" cellspacing="0" '+(o3_bgclass ? 'class="'+o3_bgclass+'"' : o3_bgcolor+' '+o3_bgbackground+' '+o3_height)+'><tr><td><table id="overDiv_table4" bgColor="#CCCCCC" width="100%" border="0" cellpadding="0" cellspacing="0"><tr><td'+(o3_captionfontclass ? ' class="'+o3_captionfontclass+'">' : '>')+(o3_captionfontclass ? '' : '<b>'+wrapStr(0,o3_captionsize,'caption'))+o3_capicon+title+(o3_captionfontclass ? '' : wrapStr(1,o3_captionsize)+'</b>')+'</td>'+closing+'</tr></table><table id="overDiv_table5" width="100%"  border="0" '+((olNs4||!cpIsMultiple) ? 'cellpadding="5" ' : '')+'cellspacing="0" '+(o3_fgclass ? 'class="'+o3_fgclass+'"' : o3_fgcolor+' '+o3_fgbackground+' '+o3_height)+'><tr><td valign="TOP"'+(o3_textfontclass ? ' class="'+o3_textfontclass+'">' :((!olNs4&&cpIsMultiple) ? ' style="'+setCellPadStr(o3_cellpad)+'">' : '>'))+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize)) + '</td></tr></table></td></tr></table>';

	set_background("");
	return txt;
}

// Sets the background picture,padding and lots more. :)
function ol_content_background(text,picture,hasfullhtml) {
	if (hasfullhtml) {
		txt=text;
	} else {
		txt='<table id="overDiv_table5" width="'+o3_width+'" border="0" cellpadding="0" cellspacing="0" height="'+o3_height+'"><tr><td colspan="3" height="'+o3_padyt+'"></td></tr><tr><td width="'+o3_padxl+'"></td><td valign="TOP" width="'+(o3_width-o3_padxl-o3_padxr)+(o3_textfontclass ? '" class="'+o3_textfontclass : '')+'">'+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize))+'</td><td width="'+o3_padxr+'"></td></tr><tr><td colspan="3" height="'+o3_padyb+'"></td></tr></table>';
	}

	set_background(picture);
	return txt;
}

// Loads a picture into the div.
function set_background(pic){
	if (pic == "") {
		if (olNs4) {
			over.background.src = null; 
		} else if (over.style) {
			over.style.backgroundImage = "none";
		}
	} else {
		if (olNs4) {
			over.background.src = pic;
		} else if (over.style) {
			over.style.width=o3_width + 'px';
			over.style.backgroundImage = "url("+pic+")";
		}
	}
}

////////
// HANDLING FUNCTIONS
////////
var olShowId=-1;

// Displays the popup
function disp(statustext) {
	runHook("disp", FBEFORE);
	
	if (o3_allowmove == 0) {
		runHook("placeLayer", FREPLACE);
		(olNs6&&olShowId<0) ? olShowId=setTimeout("runHook('showObject', FREPLACE, over)", 1) : runHook("showObject", FREPLACE, over);
		o3_allowmove = (o3_sticky || o3_followmouse==0) ? 0 : 1;
	}
	
	runHook("disp", FAFTER);

	if (statustext != "") self.status = statustext;
}

// Creates the actual popup structure
function createPopup(lyrContent){
	runHook("createPopup", FBEFORE);
	
	if (o3_wrap) {
		var wd,ww,theObj = (olNs4 ? over : over.style);
		theObj.top = theObj.left = ((olIe4&&!olOp) ? 0 : -10000) + (!olNs4 ? 'px' : 0);
		layerWrite(lyrContent);
		wd = (olNs4 ? over.clip.width : over.offsetWidth);
		if (wd > (ww=windowWidth())) {
			lyrContent=lyrContent.replace(/\&nbsp;/g, ' ');
			o3_width=ww;
			o3_wrap=0;
		} 
	}

	layerWrite(lyrContent);
	
	// Have to set o3_width for placeLayer() routine if o3_wrap is turned on
	if (o3_wrap) o3_width=(olNs4 ? over.clip.width : over.offsetWidth);
	
	runHook("createPopup", FAFTER, lyrContent);

	return true;
}

// Decides where we want the popup.
function placeLayer() {
	var placeX, placeY, widthFix = 0;
	
	// HORIZONTAL PLACEMENT, re-arranged to work in Safari
	if (o3_frame.innerWidth) widthFix=18; 
	iwidth = windowWidth();

	// Horizontal scroll offset
	winoffset=(olIe4) ? eval('o3_frame.'+docRoot+'.scrollLeft') : o3_frame.pageXOffset;
	placeX = runHook('horizontalPlacement',FCHAIN,iwidth,winoffset,widthFix);

	// VERTICAL PLACEMENT, re-arranged to work in Safari
	if (o3_frame.innerHeight) {
		iheight=o3_frame.innerHeight;
	} else if (eval('o3_frame.'+docRoot)&&eval("typeof o3_frame."+docRoot+".clientHeight=='number'")&&eval('o3_frame.'+docRoot+'.clientHeight')) { 
		iheight=eval('o3_frame.'+docRoot+'.clientHeight');
	}			

	// Vertical scroll offset
	scrolloffset=(olIe4) ? eval('o3_frame.'+docRoot+'.scrollTop') : o3_frame.pageYOffset;
	placeY = runHook('verticalPlacement',FCHAIN,iheight,scrolloffset);

	// Actually move the object.
	repositionTo(over, placeX, placeY);
}

// Moves the layer
function olMouseMove(e) {
	var e = (e) ? e : event;

	if (e.pageX) {
		o3_x = e.pageX;
		o3_y = e.pageY;
	} else if (e.clientX) {
		o3_x = eval('e.clientX+o3_frame.'+docRoot+'.scrollLeft');
		o3_y = eval('e.clientY+o3_frame.'+docRoot+'.scrollTop');
	}
	
	if (o3_allowmove == 1) runHook("placeLayer", FREPLACE);

	// MouseOut handler
	if (hoveringSwitch && !olNs4 && runHook("cursorOff", FREPLACE)) {
		(olHideDelay ? hideDelay(olHideDelay) : cClick());
		hoveringSwitch = !hoveringSwitch;
	}
}

// Fake function for 3.0 users.
function no_overlib() { return ver3fix; }

// Capture the mouse and chain other scripts.
function olMouseCapture() {
	capExtent = document;
	var fN, str = '', l, k, f, wMv, sS, mseHandler = olMouseMove;
	var re = /function[ ]*(\w*)\(/;
	
	wMv = (!olIe4 && window.onmousemove);
	if (document.onmousemove || wMv) {
		if (wMv) capExtent = window;
		f = capExtent.onmousemove.toString();
		fN = f.match(re);
		if (fN == null) {
			str = f+'(e); ';
		} else if (fN[1] == 'anonymous' || fN[1] == 'olMouseMove' || (wMv && fN[1] == 'onmousemove')) {
			if (!olOp && wMv) {
				l = f.indexOf('{')+1;
				k = f.lastIndexOf('}');
				sS = f.substring(l,k);
				if ((l = sS.indexOf('(')) != -1) {
					sS = sS.substring(0,l).replace(/^\s+/,'').replace(/\s+$/,'');
					if (eval("typeof " + sS + " == 'undefined'")) window.onmousemove = null;
					else str = sS + '(e);';
				}
			}
			if (!str) {
				olCheckMouseCapture = false;
				return;
			}
		} else {
			if (fN[1]) str = fN[1]+'(e); ';
			else {
				l = f.indexOf('{')+1;
				k = f.lastIndexOf('}');
				str = f.substring(l,k) + '\n';
			}
		}
		str += 'olMouseMove(e); ';
		mseHandler = new Function('e', str);
	}

	capExtent.onmousemove = mseHandler;
	if (olNs4) capExtent.captureEvents(Event.MOUSEMOVE);
}

////////
// PARSING FUNCTIONS
////////

// Does the actual command parsing.
function parseTokens(pf, ar) {
	// What the next argument is expected to be.
	var v, i, mode=-1, par = (pf != 'ol_');	
	var fnMark = (par && !ar.length ? 1 : 0);

	for (i = 0; i < ar.length; i++) {
		if (mode < 0) {
			// Arg is maintext,unless its a number between pmStart and pmUpper
			// then its a command.
			if (typeof ar[i] == 'number' && ar[i] > pmStart && ar[i] < pmUpper) {
				fnMark = (par ? 1 : 0);
				i--;   // backup one so that the next block can parse it
			} else {
				switch(pf) {
					case 'ol_':
						ol_text = ar[i].toString();
						break;
					default:
						o3_text=ar[i].toString();  
				}
			}
			mode = 0;
		} else {
			// Note: NS4 doesn't like switch cases with vars.
			if (ar[i] >= pmCount || ar[i]==DONOTHING) { continue; }
			if (ar[i]==INARRAY) { fnMark = 0; eval(pf+'text=ol_texts['+ar[++i]+'].toString()'); continue; }
			if (ar[i]==CAPARRAY) { eval(pf+'cap=ol_caps['+ar[++i]+'].toString()'); continue; }
			if (ar[i]==STICKY) { if (pf!='ol_') eval(pf+'sticky=1'); continue; }
			if (ar[i]==BACKGROUND) { eval(pf+'background="'+ar[++i]+'"'); continue; }
			if (ar[i]==NOCLOSE) { if (pf!='ol_') opt_NOCLOSE(); continue; }
			if (ar[i]==CAPTION) { eval(pf+"cap='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CENTER || ar[i]==LEFT || ar[i]==RIGHT) { eval(pf+'hpos='+ar[i]); if(pf!='ol_') olHautoFlag=1; continue; }
			if (ar[i]==OFFSETX) { eval(pf+'offsetx='+ar[++i]); continue; }
			if (ar[i]==OFFSETY) { eval(pf+'offsety='+ar[++i]); continue; }
			if (ar[i]==FGCOLOR) { eval(pf+'fgcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGCOLOR) { eval(pf+'bgcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTCOLOR) { eval(pf+'textcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPCOLOR) { eval(pf+'capcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSECOLOR) { eval(pf+'closecolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==WIDTH) { eval(pf+'width='+ar[++i]); continue; }
			if (ar[i]==BORDER) { eval(pf+'border='+ar[++i]); continue; }
			if (ar[i]==CELLPAD) { i=opt_MULTIPLEARGS(++i,ar,(pf+'cellpad')); continue; }
			if (ar[i]==STATUS) { eval(pf+"status='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==AUTOSTATUS) { eval(pf +'autostatus=('+pf+'autostatus == 1) ? 0 : 1'); continue; }
			if (ar[i]==AUTOSTATUSCAP) { eval(pf +'autostatus=('+pf+'autostatus == 2) ? 0 : 2'); continue; }
			if (ar[i]==HEIGHT) { eval(pf+'height='+pf+'aboveheight='+ar[++i]); continue; } // Same param again.
			if (ar[i]==CLOSETEXT) { eval(pf+"close='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==SNAPX) { eval(pf+'snapx='+ar[++i]); continue; }
			if (ar[i]==SNAPY) { eval(pf+'snapy='+ar[++i]); continue; }
			if (ar[i]==FIXX) { eval(pf+'fixx='+ar[++i]); continue; }
			if (ar[i]==FIXY) { eval(pf+'fixy='+ar[++i]); continue; }
			if (ar[i]==RELX) { eval(pf+'relx='+ar[++i]); continue; }
			if (ar[i]==RELY) { eval(pf+'rely='+ar[++i]); continue; }
			if (ar[i]==FGBACKGROUND) { eval(pf+'fgbackground="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGBACKGROUND) { eval(pf+'bgbackground="'+ar[++i]+'"'); continue; }
			if (ar[i]==PADX) { eval(pf+'padxl='+ar[++i]); eval(pf+'padxr='+ar[++i]); continue; }
			if (ar[i]==PADY) { eval(pf+'padyt='+ar[++i]); eval(pf+'padyb='+ar[++i]); continue; }
			if (ar[i]==FULLHTML) { if (pf!='ol_') eval(pf+'fullhtml=1'); continue; }
			if (ar[i]==BELOW || ar[i]==ABOVE) { eval(pf+'vpos='+ar[i]); if (pf!='ol_') olVautoFlag=1; continue; }
			if (ar[i]==CAPICON) { eval(pf+'capicon="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTFONT) { eval(pf+"textfont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CAPTIONFONT) { eval(pf+"captionfont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CLOSEFONT) { eval(pf+"closefont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==TEXTSIZE) { eval(pf+'textsize="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPTIONSIZE) { eval(pf+'captionsize="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSESIZE) { eval(pf+'closesize="'+ar[++i]+'"'); continue; }
			if (ar[i]==TIMEOUT) { eval(pf+'timeout='+ar[++i]); continue; }
			if (ar[i]==FUNCTION) { if (pf=='ol_') { if (typeof ar[i+1]!='number') { v=ar[++i]; ol_function=(typeof v=='function' ? v : null); }} else {fnMark = 0; v = null; if (typeof ar[i+1]!='number') v = ar[++i];  opt_FUNCTION(v); } continue; }
			if (ar[i]==DELAY) { eval(pf+'delay='+ar[++i]); continue; }
			if (ar[i]==HAUTO) { eval(pf+'hauto=('+pf+'hauto == 0) ? 1 : 0'); continue; }
			if (ar[i]==VAUTO) { eval(pf+'vauto=('+pf+'vauto == 0) ? 1 : 0'); continue; }
			if (ar[i]==CLOSECLICK) { eval(pf +'closeclick=('+pf+'closeclick == 0) ? 1 : 0'); continue; }
			if (ar[i]==WRAP) { eval(pf +'wrap=('+pf+'wrap == 0) ? 1 : 0'); continue; }
			if (ar[i]==FOLLOWMOUSE) { eval(pf +'followmouse=('+pf+'followmouse == 1) ? 0 : 1'); continue; }
			if (ar[i]==MOUSEOFF) { eval(pf +'mouseoff=('+pf+'mouseoff==0) ? 1 : 0'); v=ar[i+1]; if (pf != 'ol_' && eval(pf+'mouseoff') && typeof v == 'number' && (v < pmStart || v > pmUpper)) olHideDelay=ar[++i]; continue; }
			if (ar[i]==CLOSETITLE) { eval(pf+"closetitle='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CSSOFF||ar[i]==CSSCLASS) { eval(pf+'css='+ar[i]); continue; }
			if (ar[i]==COMPATMODE) { eval(pf+'compatmode=('+pf+'compatmode==0) ? 1 : 0'); continue; }
			if (ar[i]==FGCLASS) { eval(pf+'fgclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGCLASS) { eval(pf+'bgclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTFONTCLASS) { eval(pf+'textfontclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPTIONFONTCLASS) { eval(pf+'captionfontclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSEFONTCLASS) { eval(pf+'closefontclass="'+ar[++i]+'"'); continue; }
			i = parseCmdLine(pf, i, ar);
		}
	}

	if (fnMark && o3_function) o3_text = o3_function();
	
	if ((pf == 'o3_') && o3_wrap) {
		o3_width = 0;
		
		var tReg=/<.*\n*>/ig;
		if (!tReg.test(o3_text)) o3_text = o3_text.replace(/[ ]+/g, '&nbsp;');
		if (!tReg.test(o3_cap))o3_cap = o3_cap.replace(/[ ]+/g, '&nbsp;');
	}
	if ((pf == 'o3_') && o3_sticky) {
		if (!o3_close && (o3_frame != ol_frame)) o3_close = ol_close;
		if (o3_mouseoff && (o3_frame == ol_frame)) opt_NOCLOSE(' ');
	}
}


////////
// LAYER FUNCTIONS
////////

// Writes to a layer
function layerWrite(txt) {
	txt += "\n";
	if (olNs4) {
		var lyr = o3_frame.document.layers['overDiv'].document
		lyr.write(txt)
		lyr.close()
	} else if (typeof over.innerHTML != 'undefined') {
		if (olIe5 && isMac) over.innerHTML = '';
		over.innerHTML = txt;
	} else {
		range = o3_frame.document.createRange();
		range.setStartAfter(over);
		domfrag = range.createContextualFragment(txt);
		
		while (over.hasChildNodes()) {
			over.removeChild(over.lastChild);
		}
		
		over.appendChild(domfrag);
	}
}

// Make an object visible
function showObject(obj) {
	runHook("showObject", FBEFORE);

	var theObj=(olNs4 ? obj : obj.style);
	theObj.visibility = 'visible';

	runHook("showObject", FAFTER);
}

// Hides an object
function hideObject(obj) {
	runHook("hideObject", FBEFORE);

	var theObj=(olNs4 ? obj : obj.style);
	if (olNs6 && olShowId>0) { clearTimeout(olShowId); olShowId=0; }
	theObj.visibility = 'hidden';
	theObj.top = theObj.left = ((olIe4&&!olOp) ? 0 : -10000) + (!olNs4 ? 'px' : 0);

	if (o3_timerid > 0) clearTimeout(o3_timerid);
	if (o3_delayid > 0) clearTimeout(o3_delayid);

	o3_timerid = 0;
	o3_delayid = 0;
	self.status = "";

	if (obj.onmouseout||obj.onmouseover) {
		if (olNs4) obj.releaseEvents(Event.MOUSEOUT || Event.MOUSEOVER);
		obj.onmouseout = obj.onmouseover = null;
	}

	runHook("hideObject", FAFTER);
}

// Move a layer
function repositionTo(obj, xL, yL) {
	var theObj=(olNs4 ? obj : obj.style);
	theObj.left = xL + (!olNs4 ? 'px' : 0);
	theObj.top = yL + (!olNs4 ? 'px' : 0);
}

// Check position of cursor relative to overDiv DIVision; mouseOut function
function cursorOff() {
	var left = parseInt(over.style.left);
	var top = parseInt(over.style.top);
	var right = left + (over.offsetWidth >= parseInt(o3_width) ? over.offsetWidth : parseInt(o3_width));
	var bottom = top + (over.offsetHeight >= o3_aboveheight ? over.offsetHeight : o3_aboveheight);

	if (o3_x < left || o3_x > right || o3_y < top || o3_y > bottom) return true;

	return false;
}


////////
// COMMAND FUNCTIONS
////////

// Calls callme or the default function.
function opt_FUNCTION(callme) {
	o3_text = (callme ? (typeof callme=='string' ? (/.+\(.*\)/.test(callme) ? eval(callme) : callme) : callme()) : (o3_function ? o3_function() : 'No Function'));

	return 0;
}

// Handle hovering
function opt_NOCLOSE(unused) {
	if (!unused) o3_close = "";

	if (olNs4) {
		over.captureEvents(Event.MOUSEOUT || Event.MOUSEOVER);
		over.onmouseover = function () { if (o3_timerid > 0) { clearTimeout(o3_timerid); o3_timerid = 0; } }
		over.onmouseout = function (e) { if (olHideDelay) hideDelay(olHideDelay); else cClick(e); }
	} else {
		over.onmouseover = function () {hoveringSwitch = true; if (o3_timerid > 0) { clearTimeout(o3_timerid); o3_timerid =0; } }
	}

	return 0;
}

// Function to scan command line arguments for multiples
function opt_MULTIPLEARGS(i, args, parameter) {
  var k=i, re, pV, str='';

  for(k=i; k<args.length; k++) {
		if(typeof args[k] == 'number' && args[k]>pmStart) break;
		str += args[k] + ',';
	}
	if (str) str = str.substring(0,--str.length);

	k--;  // reduce by one so the for loop this is in works correctly
	pV=(olNs4 && /cellpad/i.test(parameter)) ? str.split(',')[0] : str;
	eval(parameter + '="' + pV + '"');

	return k;
}

// Remove &nbsp; in texts when done.
function nbspCleanup() {
	if (o3_wrap) {
		o3_text = o3_text.replace(/\&nbsp;/g, ' ');
		o3_cap = o3_cap.replace(/\&nbsp;/g, ' ');
	}
}

// Escape embedded single quotes in text strings
function escSglQuote(str) {
  return str.toString().replace(/'/g,"\\'");
}

// Onload handler for window onload event
function OLonLoad_handler(e) {
	var re = /\w+\(.*\)[;\s]+/g, olre = /overlib\(|nd\(|cClick\(/, fn, l, i;

	if(!olLoaded) olLoaded=1;

  // Remove it for Gecko based browsers
	if(window.removeEventListener && e.eventPhase == 3) window.removeEventListener("load",OLonLoad_handler,false);
	else if(window.detachEvent) { // and for IE and Opera 4.x but execute calls to overlib, nd, or cClick()
		window.detachEvent("onload",OLonLoad_handler);
		var fN = document.body.getAttribute('onload');
		if (fN) {
			fN=fN.toString().match(re);
			if (fN && fN.length) {
				for (i=0; i<fN.length; i++) {
					if (/anonymous/.test(fN[i])) continue;
					while((l=fN[i].search(/\)[;\s]+/)) != -1) {
						fn=fN[i].substring(0,l+1);
						fN[i] = fN[i].substring(l+2);
						if (olre.test(fn)) eval(fn);
					}
				}
			}
		}
	}
}

// Wraps strings in Layer Generation Functions with the correct tags
//    endWrap true(if end tag) or false if start tag
//    fontSizeStr - font size string such as '1' or '10px'
//    whichString is being wrapped -- 'text', 'caption', or 'close'
function wrapStr(endWrap,fontSizeStr,whichString) {
	var fontStr, fontColor, isClose=((whichString=='close') ? 1 : 0), hasDims=/[%\-a-z]+$/.test(fontSizeStr);
	fontSizeStr = (olNs4) ? (!hasDims ? fontSizeStr : '1') : fontSizeStr;
	if (endWrap) return (hasDims&&!olNs4) ? (isClose ? '</span>' : '</div>') : '</font>';
	else {
		fontStr='o3_'+whichString+'font';
		fontColor='o3_'+((whichString=='caption')? 'cap' : whichString)+'color';
		return (hasDims&&!olNs4) ? (isClose ? '<span style="font-family: '+quoteMultiNameFonts(eval(fontStr))+'; color: '+eval(fontColor)+'; font-size: '+fontSizeStr+';">' : '<div style="font-family: '+quoteMultiNameFonts(eval(fontStr))+'; color: '+eval(fontColor)+'; font-size: '+fontSizeStr+';">') : '<font face="'+eval(fontStr)+'" color="'+eval(fontColor)+'" size="'+(parseInt(fontSizeStr)>7 ? '7' : fontSizeStr)+'">';
	}
}

// Quotes Multi word font names; needed for CSS Standards adherence in font-family
function quoteMultiNameFonts(theFont) {
	var v, pM=theFont.split(',');
	for (var i=0; i<pM.length; i++) {
		v=pM[i];
		v=v.replace(/^\s+/,'').replace(/\s+$/,'');
		if(/\s/.test(v) && !/['"]/.test(v)) {
			v="\'"+v+"\'";
			pM[i]=v;
		}
	}
	return pM.join();
}

// dummy function which will be overridden 
function isExclusive(args) {
	return false;
}

// Sets cellpadding style string value
function setCellPadStr(parameter) {
	var Str='', j=0, ary = new Array(), top, bottom, left, right;

	Str+='padding: ';
	ary=parameter.replace(/\s+/g,'').split(',');

	switch(ary.length) {
		case 2:
			top=bottom=ary[j];
			left=right=ary[++j];
			break;
		case 3:
			top=ary[j];
			left=right=ary[++j];
			bottom=ary[++j];
			break;
		case 4:
			top=ary[j];
			right=ary[++j];
			bottom=ary[++j];
			left=ary[++j];
			break;
	}

	Str+= ((ary.length==1) ? ary[0] + 'px;' : top + 'px ' + right + 'px ' + bottom + 'px ' + left + 'px;');

	return Str;
}

// function will delay close by time milliseconds
function hideDelay(time) {
	if (time&&!o3_delay) {
		if (o3_timerid > 0) clearTimeout(o3_timerid);

		o3_timerid=setTimeout("cClick()",(o3_timeout=time));
	}
}

// Was originally in the placeLayer() routine; separated out for future ease
function horizontalPlacement(browserWidth, horizontalScrollAmount, widthFix) {
	var placeX, iwidth=browserWidth, winoffset=horizontalScrollAmount;
	var parsedWidth = parseInt(o3_width);

	if (o3_fixx > -1 || o3_relx != null) {
		// Fixed position
		placeX=(o3_relx != null ? ( o3_relx < 0 ? winoffset +o3_relx+ iwidth - parsedWidth - widthFix : winoffset+o3_relx) : o3_fixx);
	} else {  
		// If HAUTO, decide what to use.
		if (o3_hauto == 1) {
			if ((o3_x - winoffset) > (iwidth / 2)) {
				o3_hpos = LEFT;
			} else {
				o3_hpos = RIGHT;
			}
		}  		

		// From mouse
		if (o3_hpos == CENTER) { // Center
			placeX = o3_x+o3_offsetx-(parsedWidth/2);

			if (placeX < winoffset) placeX = winoffset;
		}

		if (o3_hpos == RIGHT) { // Right
			placeX = o3_x+o3_offsetx;

			if ((placeX+parsedWidth) > (winoffset+iwidth - widthFix)) {
				placeX = iwidth+winoffset - parsedWidth - widthFix;
				if (placeX < 0) placeX = 0;
			}
		}
		if (o3_hpos == LEFT) { // Left
			placeX = o3_x-o3_offsetx-parsedWidth-150;
			if (placeX < winoffset) placeX = winoffset;
		}  	

		// Snapping!
		if (o3_snapx > 1) {
			var snapping = placeX % o3_snapx;

			if (o3_hpos == LEFT) {
				placeX = placeX - (o3_snapx+snapping);
			} else {
				// CENTER and RIGHT
				placeX = placeX+(o3_snapx - snapping);
			}

			if (placeX < winoffset) placeX = winoffset;
		}
	}	

	return placeX;
}

// was originally in the placeLayer() routine; separated out for future ease
function verticalPlacement(browserHeight,verticalScrollAmount) {
	var placeY, iheight=browserHeight, scrolloffset=verticalScrollAmount;
	var parsedHeight=(o3_aboveheight ? parseInt(o3_aboveheight) : (olNs4 ? over.clip.height : over.offsetHeight));

	if (o3_fixy > -1 || o3_rely != null) {
		// Fixed position
		placeY=(o3_rely != null ? (o3_rely < 0 ? scrolloffset+o3_rely+iheight - parsedHeight : scrolloffset+o3_rely) : o3_fixy);
	} else {
		// If VAUTO, decide what to use.
		if (o3_vauto == 1) {
			if ((o3_y - scrolloffset) > (iheight / 2) && o3_vpos == BELOW && (o3_y + parsedHeight + o3_offsety - (scrolloffset + iheight) > 0)) {
				o3_vpos = ABOVE;
			} else if (o3_vpos == ABOVE && (o3_y - (parsedHeight + o3_offsety) - scrolloffset < 0)) {
				o3_vpos = BELOW;
			}
		}

		// From mouse
		if (o3_vpos == ABOVE) {
			if (o3_aboveheight == 0) o3_aboveheight = parsedHeight; 

			placeY = o3_y - (o3_aboveheight+o3_offsety);
			if (placeY < scrolloffset) placeY = scrolloffset;
		} else {
			// BELOW
			placeY = o3_y+o3_offsety;
		} 

		// Snapping!
		if (o3_snapy > 1) {
			var snapping = placeY % o3_snapy;  			

			if (o3_aboveheight > 0 && o3_vpos == ABOVE) {
				placeY = placeY - (o3_snapy+snapping);
			} else {
				placeY = placeY+(o3_snapy - snapping);
			} 			

			if (placeY < scrolloffset) placeY = scrolloffset;
		}
	}

	return placeY;
}

// checks positioning flags
function checkPositionFlags() {
	if (olHautoFlag) olHautoFlag = o3_hauto=0;
	if (olVautoFlag) olVautoFlag = o3_vauto=0;
	return true;
}

// get Browser window width
function windowWidth() {
	var w;
	if (o3_frame.innerWidth) w=o3_frame.innerWidth;
	else if (eval('o3_frame.'+docRoot)&&eval("typeof o3_frame."+docRoot+".clientWidth=='number'")&&eval('o3_frame.'+docRoot+'.clientWidth')) 
		w=eval('o3_frame.'+docRoot+'.clientWidth');
	return w;			
}

// create the div container for popup content if it doesn't exist
function createDivContainer(id,frm,zValue) {
	id = (id || 'overDiv'), frm = (frm || o3_frame), zValue = (zValue || 500);
	var objRef, divContainer = layerReference(id);

	if (divContainer == null) {
		if (olNs4) {
			divContainer = frm.document.layers[id] = new Layer(window.innerWidth, frm);
			objRef = divContainer;
		} else {
			var body = (olIe4 ? frm.document.all.tags('BODY')[0] : frm.document.getElementsByTagName("BODY")[0]);
			if (olIe4&&!document.getElementById) {
				body.insertAdjacentHTML("beforeEnd",'<div id="'+id+'"></div>');
				divContainer=layerReference(id);
			} else {
				divContainer = frm.document.createElement("DIV");
				divContainer.id = id;
				body.appendChild(divContainer);
			}
			objRef = divContainer.style;
		}

		//objRef.marginLeft = '170px';
		objRef.position = 'absolute';
		objRef.visibility = 'hidden';
		objRef.zIndex = zValue;
		if (olIe4&&!olOp) objRef.left = objRef.top = '0px';
		else objRef.left = objRef.top =  -10000 + (!olNs4 ? 'px' : 0);
	}

	var id_name = document.getElementById('overDiv');
	id_name.setAttribute("onclick","return nd();");
	//id_name.setAttribute("onmouseout","setTimeout('nd();', 1000);");

	return divContainer;
}

// get reference to a layer with ID=id
function layerReference(id) {
	return (olNs4 ? o3_frame.document.layers[id] : (document.all ? o3_frame.document.all[id] : o3_frame.document.getElementById(id)));
}
////////
//  UTILITY FUNCTIONS
////////

// Checks if something is a function.
function isFunction(fnRef) {
	var rtn = true;

	if (typeof fnRef == 'object') {
		for (var i = 0; i < fnRef.length; i++) {
			if (typeof fnRef[i]=='function') continue;
			rtn = false;
			break;
		}
	} else if (typeof fnRef != 'function') {
		rtn = false;
	}
	
	return rtn;
}

// Converts an array into an argument string for use in eval.
function argToString(array, strtInd, argName) {
	var jS = strtInd, aS = '', ar = array;
	argName=(argName ? argName : 'ar');
	
	if (ar.length > jS) {
		for (var k = jS; k < ar.length; k++) aS += argName+'['+k+'], ';
		aS = aS.substring(0, aS.length-2);
	}
	
	return aS;
}

// Places a hook in the correct position in a hook point.
function reOrder(hookPt, fnRef, order) {
	var newPt = new Array(), match, i, j;

	if (!order || typeof order == 'undefined' || typeof order == 'number') return hookPt;
	
	if (typeof order=='function') {
		if (typeof fnRef=='object') {
			newPt = newPt.concat(fnRef);
		} else {
			newPt[newPt.length++]=fnRef;
		}
		
		for (i = 0; i < hookPt.length; i++) {
			match = false;
			if (typeof fnRef == 'function' && hookPt[i] == fnRef) {
				continue;
			} else {
				for(j = 0; j < fnRef.length; j++) if (hookPt[i] == fnRef[j]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++] = hookPt[i];
		}

		newPt[newPt.length++] = order;

	} else if (typeof order == 'object') {
		if (typeof fnRef == 'object') {
			newPt = newPt.concat(fnRef);
		} else {
			newPt[newPt.length++] = fnRef;
		}
		
		for (j = 0; j < hookPt.length; j++) {
			match = false;
			if (typeof fnRef == 'function' && hookPt[j] == fnRef) {
				continue;
			} else {
				for (i = 0; i < fnRef.length; i++) if (hookPt[j] == fnRef[i]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++]=hookPt[j];
		}

		for (i = 0; i < newPt.length; i++) hookPt[i] = newPt[i];
		newPt.length = 0;
		
		for (j = 0; j < hookPt.length; j++) {
			match = false;
			for (i = 0; i < order.length; i++) {
				if (hookPt[j] == order[i]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++] = hookPt[j];
		}
		newPt = newPt.concat(order);
	}

	hookPt = newPt;

	return hookPt;
}

////////
//  PLUGIN ACTIVATION FUNCTIONS
////////

// Runs plugin functions to set runtime variables.
function setRunTimeVariables(){
	if (typeof runTime != 'undefined' && runTime.length) {
		for (var k = 0; k < runTime.length; k++) {
			runTime[k]();
		}
	}
}

// Runs plugin functions to parse commands.
function parseCmdLine(pf, i, args) {
	if (typeof cmdLine != 'undefined' && cmdLine.length) { 
		for (var k = 0; k < cmdLine.length; k++) { 
			var j = cmdLine[k](pf, i, args);
			if (j >- 1) {
				i = j;
				break;
			}
		}
	}

	return i;
}

// Runs plugin functions to do things after parse.
function postParseChecks(pf,args){
	if (typeof postParse != 'undefined' && postParse.length) {
		for (var k = 0; k < postParse.length; k++) {
			if (postParse[k](pf,args)) continue;
			return false;  // end now since have an error
		}
	}
	return true;
}


////////
//  PLUGIN REGISTRATION FUNCTIONS
////////

// Registers commands and creates constants.
function registerCommands(cmdStr) {
	if (typeof cmdStr!='string') return;

	var pM = cmdStr.split(',');
	pms = pms.concat(pM);

	for (var i = 0; i< pM.length; i++) {
		eval(pM[i].toUpperCase()+'='+pmCount++);
	}
}

// Registers no-parameter commands
function registerNoParameterCommands(cmdStr) {
	if (!cmdStr && typeof cmdStr != 'string') return;
	pmt=(!pmt) ? cmdStr : pmt + ',' + cmdStr;
}

// Register a function to hook at a certain point.
function registerHook(fnHookTo, fnRef, hookType, optPm) {
	var hookPt, last = typeof optPm;
	
	if (fnHookTo == 'plgIn'||fnHookTo == 'postParse') return;
	if (typeof hookPts[fnHookTo] == 'undefined') hookPts[fnHookTo] = new FunctionReference();

	hookPt = hookPts[fnHookTo];

	if (hookType != null) {
		if (hookType == FREPLACE) {
			hookPt.ovload = fnRef;  // replace normal overlib routine
			if (fnHookTo.indexOf('ol_content_') > -1) hookPt.alt[pms[CSSOFF-1-pmStart]]=fnRef; 

		} else if (hookType == FBEFORE || hookType == FAFTER) {
			var hookPt=(hookType == 1 ? hookPt.before : hookPt.after);

			if (typeof fnRef == 'object') {
				hookPt = hookPt.concat(fnRef);
			} else {
				hookPt[hookPt.length++] = fnRef;
			}

			if (optPm) hookPt = reOrder(hookPt, fnRef, optPm);

		} else if (hookType == FALTERNATE) {
			if (last=='number') hookPt.alt[pms[optPm-1-pmStart]] = fnRef;
		} else if (hookType == FCHAIN) {
			hookPt = hookPt.chain; 
			if (typeof fnRef=='object') hookPt=hookPt.concat(fnRef); // add other functions 
			else hookPt[hookPt.length++]=fnRef;
		}

		return;
	}
}

// Register a function that will set runtime variables.
function registerRunTimeFunction(fn) {
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			runTime = runTime.concat(fn);
		} else {
			runTime[runTime.length++] = fn;
		}
	}
}

// Register a function that will handle command parsing.
function registerCmdLineFunction(fn){
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			cmdLine = cmdLine.concat(fn);
		} else {
			cmdLine[cmdLine.length++] = fn;
		}
	}
}

// Register a function that does things after command parsing. 
function registerPostParseFunction(fn){
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			postParse = postParse.concat(fn);
		} else {
			postParse[postParse.length++] = fn;
		}
	}
}

////////
//  PLUGIN REGISTRATION FUNCTIONS
////////

// Runs any hooks registered.
function runHook(fnHookTo, hookType) {
	var l = hookPts[fnHookTo], k, rtnVal = null, optPm, arS, ar = runHook.arguments;

	if (hookType == FREPLACE) {
		arS = argToString(ar, 2);

		if (typeof l == 'undefined' || !(l = l.ovload)) rtnVal = eval(fnHookTo+'('+arS+')');
		else rtnVal = eval('l('+arS+')');

	} else if (hookType == FBEFORE || hookType == FAFTER) {
		if (typeof l != 'undefined') {
			l=(hookType == 1 ? l.before : l.after);
	
			if (l.length) {
				arS = argToString(ar, 2);
				for (var k = 0; k < l.length; k++) eval('l[k]('+arS+')');
			}
		}
	} else if (hookType == FALTERNATE) {
		optPm = ar[2];
		arS = argToString(ar, 3);

		if (typeof l == 'undefined' || (l = l.alt[pms[optPm-1-pmStart]]) == 'undefined') {
			rtnVal = eval(fnHookTo+'('+arS+')');
		} else {
			rtnVal = eval('l('+arS+')');
		}
	} else if (hookType == FCHAIN) {
		arS=argToString(ar,2);
		l=l.chain;

		for (k=l.length; k > 0; k--) if((rtnVal=eval('l[k-1]('+arS+')'))!=void(0)) break;
	}

	return rtnVal;
}

////////
// OBJECT CONSTRUCTORS
////////

// Object for handling hooks.
function FunctionReference() {
	this.ovload = null;
	this.before = new Array();
	this.after = new Array();
	this.alt = new Array();
	this.chain = new Array();
}

// Object for simple access to the overLIB version used.
// Examples: simpleversion:351 major:3 minor:5 revision:1
function Info(version, prerelease) {
	this.version = version;
	this.prerelease = prerelease;

	this.simpleversion = Math.round(this.version*100);
	this.major = parseInt(this.simpleversion / 100);
	this.minor = parseInt(this.simpleversion / 10) - this.major * 10;
	this.revision = parseInt(this.simpleversion) - this.major * 100 - this.minor * 10;
	this.meets = meets;
}

// checks for Core Version required
function meets(reqdVersion) {
	return (!reqdVersion) ? false : this.simpleversion >= Math.round(100*parseFloat(reqdVersion));
}


////////
// STANDARD REGISTRATIONS
////////
registerHook("ol_content_simple", ol_content_simple, FALTERNATE, CSSOFF);
registerHook("ol_content_caption", ol_content_caption, FALTERNATE, CSSOFF);
registerHook("ol_content_background", ol_content_background, FALTERNATE, CSSOFF);
registerHook("ol_content_simple", ol_content_simple, FALTERNATE, CSSCLASS);
registerHook("ol_content_caption", ol_content_caption, FALTERNATE, CSSCLASS);
registerHook("ol_content_background", ol_content_background, FALTERNATE, CSSCLASS);
registerPostParseFunction(checkPositionFlags);
registerHook("hideObject", nbspCleanup, FAFTER);
registerHook("horizontalPlacement", horizontalPlacement, FCHAIN);
registerHook("verticalPlacement", verticalPlacement, FCHAIN);
if (olNs4||(olIe5&&isMac)||olKq) olLoaded=1;
registerNoParameterCommands('sticky,autostatus,autostatuscap,fullhtml,hauto,vauto,closeclick,wrap,followmouse,mouseoff,compatmode');
///////
// ESTABLISH MOUSECAPTURING
///////

// Capture events, alt. diffuses the overlib function.
var olCheckMouseCapture=true;
if ((olNs4 || olNs6 || olIe4)) {
	olMouseCapture();
} else {
	overlib = no_overlib;
	nd = no_overlib;
	ver3fix = true;
}

// ---------- Viz add for pwd strength check [Start] 2012.12 -----

function chkPass(pwd, flag) {
	var orig_pwd = "";
	var oScorebar = $("scorebar");
	var oScore = $("score");
	var oComplexity = $("complexity");
	// Simultaneous variable declaration and value assignment aren't supported in IE apparently
	// so I'm forced to assign the same value individually per var to support a crappy browser *sigh* 
	var nScore=0, nLength=0, nAlphaUC=0, nAlphaLC=0, nNumber=0, nSymbol=0, nMidChar=0, nRequirements=0, nAlphasOnly=0, nNumbersOnly=0, nUnqChar=0, nRepChar=0, nRepInc=0, nConsecAlphaUC=0, nConsecAlphaLC=0, nConsecNumber=0, nConsecSymbol=0, nConsecCharType=0, nSeqAlpha=0, nSeqNumber=0, nSeqSymbol=0, nSeqChar=0, nReqChar=0, nMultConsecCharType=0;
	var nMultRepChar=1, nMultConsecSymbol=1;
	var nMultMidChar=2, nMultRequirements=2, nMultConsecAlphaUC=2, nMultConsecAlphaLC=2, nMultConsecNumber=2;
	var nReqCharType=3, nMultAlphaUC=3, nMultAlphaLC=3, nMultSeqAlpha=3, nMultSeqNumber=3, nMultSeqSymbol=3;
	var nMultLength=4, nMultNumber=4;
	var nMultSymbol=6;
	var nTmpAlphaUC="", nTmpAlphaLC="", nTmpNumber="", nTmpSymbol="";
	var sAlphaUC="0", sAlphaLC="0", sNumber="0", sSymbol="0", sMidChar="0", sRequirements="0", sAlphasOnly="0", sNumbersOnly="0", sRepChar="0", sConsecAlphaUC="0", sConsecAlphaLC="0", sConsecNumber="0", sSeqAlpha="0", sSeqNumber="0", sSeqSymbol="0";
	var sAlphas = "abcdefghijklmnopqrstuvwxyz";
	var sNumerics = "01234567890";
	var sSymbols = "~!@#$%^&*()_+";
	var sComplexity = "";
	var nMinPwdLen = 8;
	if (document.all) { var nd = 0; } else { var nd = 1; }
	if (pwd) {
		nScore = parseInt(pwd.length * nMultLength);
		nLength = pwd.length;
		var arrPwd = pwd.replace(/\s+/g,"").split(/\s*/);
		var arrPwdLen = arrPwd.length;
		
		/* Main calculation for strength: 
				Loop through password to check for Symbol, Numeric, Lowercase and Uppercase pattern matches */
		for (var a=0; a < arrPwdLen; a++) {
			if (arrPwd[a].match(/[A-Z]/g)) {
				if (nTmpAlphaUC !== "") { if ((nTmpAlphaUC + 1) == a) { nConsecAlphaUC++; nConsecCharType++; } }
				nTmpAlphaUC = a;
				nAlphaUC++;
			}
			else if (arrPwd[a].match(/[a-z]/g)) { 
				if (nTmpAlphaLC !== "") { if ((nTmpAlphaLC + 1) == a) { nConsecAlphaLC++; nConsecCharType++; } }
				nTmpAlphaLC = a;
				nAlphaLC++;
			}
			else if (arrPwd[a].match(/[0-9]/g)) { 
				if (a > 0 && a < (arrPwdLen - 1)) { nMidChar++; }
				if (nTmpNumber !== "") { if ((nTmpNumber + 1) == a) { nConsecNumber++; nConsecCharType++; } }
				nTmpNumber = a;
				nNumber++;
			}
			else if (arrPwd[a].match(/[^a-zA-Z0-9_]/g)) { 
				if (a > 0 && a < (arrPwdLen - 1)) { nMidChar++; }
				if (nTmpSymbol !== "") { if ((nTmpSymbol + 1) == a) { nConsecSymbol++; nConsecCharType++; } }
				nTmpSymbol = a;
				nSymbol++;
			}
			/* Internal loop through password to check for repeat characters */
			var bCharExists = false;
			for (var b=0; b < arrPwdLen; b++) {
				if (arrPwd[a] == arrPwd[b] && a != b) { /* repeat character exists */
					bCharExists = true;
					/* 
					Calculate icrement deduction based on proximity to identical characters
					Deduction is incremented each time a new match is discovered
					Deduction amount is based on total password length divided by the
					difference of distance between currently selected match
					*/
					nRepInc += Math.abs(arrPwdLen/(b-a));
				}
			}
			if (bCharExists) { 
				nRepChar++; 
				nUnqChar = arrPwdLen-nRepChar;
				nRepInc = (nUnqChar) ? Math.ceil(nRepInc/nUnqChar) : Math.ceil(nRepInc); 
			}
		}
		
		/* Check for sequential alpha string patterns (forward and reverse) */
		for (var s=0; s < 23; s++) {
			var sFwd = sAlphas.substring(s,parseInt(s+3));
			var sRev = sFwd.strReverse();
			if (pwd.toLowerCase().indexOf(sFwd) != -1 || pwd.toLowerCase().indexOf(sRev) != -1) { nSeqAlpha++; nSeqChar++;}
		}
		
		/* Check for sequential numeric string patterns (forward and reverse) */
		for (var s=0; s < 8; s++) {
			var sFwd = sNumerics.substring(s,parseInt(s+3));
			var sRev = sFwd.strReverse();
			if (pwd.toLowerCase().indexOf(sFwd) != -1 || pwd.toLowerCase().indexOf(sRev) != -1) { nSeqNumber++; nSeqChar++;}
		}
		
		/* Check for sequential symbol string patterns (forward and reverse) */
		for (var s=0; s < 8; s++) {
			var sFwd = sSymbols.substring(s,parseInt(s+3));
			var sRev = sFwd.strReverse();
			if (pwd.toLowerCase().indexOf(sFwd) != -1 || pwd.toLowerCase().indexOf(sRev) != -1) { nSeqSymbol++; nSeqChar++;}
		}
		
	/* Modify overall score value based on usage vs requirements */

		/* General point assignment */
		//$("nLengthBonus").innerHTML = "+ " + nScore; 
		if (nAlphaUC > 0 && nAlphaUC < nLength) {	
			nScore = parseInt(nScore + ((nLength - nAlphaUC) * 2));
			sAlphaUC = "+ " + parseInt((nLength - nAlphaUC) * 2); 
		}
		if (nAlphaLC > 0 && nAlphaLC < nLength) {	
			nScore = parseInt(nScore + ((nLength - nAlphaLC) * 2)); 
			sAlphaLC = "+ " + parseInt((nLength - nAlphaLC) * 2);
		}
		if (nNumber > 0 && nNumber < nLength) {	
			nScore = parseInt(nScore + (nNumber * nMultNumber));
			sNumber = "+ " + parseInt(nNumber * nMultNumber);
		}
		if (nSymbol > 0) {	
			nScore = parseInt(nScore + (nSymbol * nMultSymbol));
			sSymbol = "+ " + parseInt(nSymbol * nMultSymbol);
		}
		if (nMidChar > 0) {	
			nScore = parseInt(nScore + (nMidChar * nMultMidChar));
			sMidChar = "+ " + parseInt(nMidChar * nMultMidChar);
		}
		//$("nAlphaUCBonus").innerHTML = sAlphaUC; 
		//$("nAlphaLCBonus").innerHTML = sAlphaLC;
		//$("nNumberBonus").innerHTML = sNumber;
		//$("nSymbolBonus").innerHTML = sSymbol;
		//$("nMidCharBonus").innerHTML = sMidChar;
		
		/* Point deductions for poor practices */
		if ((nAlphaLC > 0 || nAlphaUC > 0) && nSymbol === 0 && nNumber === 0) {  // Only Letters
			nScore = parseInt(nScore - nLength);
			nAlphasOnly = nLength;
			sAlphasOnly = "- " + nLength;
		}
		if (nAlphaLC === 0 && nAlphaUC === 0 && nSymbol === 0 && nNumber > 0) {  // Only Numbers
			nScore = parseInt(nScore - nLength); 
			nNumbersOnly = nLength;
			sNumbersOnly = "- " + nLength;
		}
		if (nRepChar > 0) {  // Same character exists more than once
			nScore = parseInt(nScore - nRepInc);
			sRepChar = "- " + nRepInc;
		}
		if (nConsecAlphaUC > 0) {  // Consecutive Uppercase Letters exist
			nScore = parseInt(nScore - (nConsecAlphaUC * nMultConsecAlphaUC)); 
			sConsecAlphaUC = "- " + parseInt(nConsecAlphaUC * nMultConsecAlphaUC);
		}
		if (nConsecAlphaLC > 0) {  // Consecutive Lowercase Letters exist
			nScore = parseInt(nScore - (nConsecAlphaLC * nMultConsecAlphaLC)); 
			sConsecAlphaLC = "- " + parseInt(nConsecAlphaLC * nMultConsecAlphaLC);
		}
		if (nConsecNumber > 0) {  // Consecutive Numbers exist
			nScore = parseInt(nScore - (nConsecNumber * nMultConsecNumber));  
			sConsecNumber = "- " + parseInt(nConsecNumber * nMultConsecNumber);
		}
		if (nSeqAlpha > 0) {  // Sequential alpha strings exist (3 characters or more)
			nScore = parseInt(nScore - (nSeqAlpha * nMultSeqAlpha)); 
			sSeqAlpha = "- " + parseInt(nSeqAlpha * nMultSeqAlpha);
		}
		if (nSeqNumber > 0) {  // Sequential numeric strings exist (3 characters or more)
			nScore = parseInt(nScore - (nSeqNumber * nMultSeqNumber)); 
			sSeqNumber = "- " + parseInt(nSeqNumber * nMultSeqNumber);
		}
		if (nSeqSymbol > 0) {  // Sequential symbol strings exist (3 characters or more)
			nScore = parseInt(nScore - (nSeqSymbol * nMultSeqSymbol)); 
			sSeqSymbol = "- " + parseInt(nSeqSymbol * nMultSeqSymbol);
		}
		//$("nAlphasOnlyBonus").innerHTML = sAlphasOnly; 
		//$("nNumbersOnlyBonus").innerHTML = sNumbersOnly; 
		//$("nRepCharBonus").innerHTML = sRepChar; 
		//$("nConsecAlphaUCBonus").innerHTML = sConsecAlphaUC; 
		//$("nConsecAlphaLCBonus").innerHTML = sConsecAlphaLC; 
		//$("nConsecNumberBonus").innerHTML = sConsecNumber;
		//$("nSeqAlphaBonus").innerHTML = sSeqAlpha; 
		//$("nSeqNumberBonus").innerHTML = sSeqNumber; 
		//$("nSeqSymbolBonus").innerHTML = sSeqSymbol; 

		/* Determine if mandatory requirements have been met and set image indicators accordingly */
		/*
		var arrChars = [nLength,nAlphaUC,nAlphaLC,nNumber,nSymbol];
		var arrCharsIds = ["nLength","nAlphaUC","nAlphaLC","nNumber","nSymbol"];
		var arrCharsLen = arrChars.length;
		for (var c=0; c < arrCharsLen; c++) {
			var oImg = $('div_' + arrCharsIds[c]);
			var oBonus = $(arrCharsIds[c] + 'Bonus');
			//$(arrCharsIds[c]).innerHTML = arrChars[c];
			if (arrCharsIds[c] == "nLength") { var minVal = parseInt(nMinPwdLen - 1); } else { var minVal = 0; }
			//if (arrChars[c] == parseInt(minVal + 1)) { nReqChar++; oImg.className = "pass"; oBonus.parentNode.className = "pass"; }
			//else if (arrChars[c] > parseInt(minVal + 1)) { nReqChar++; oImg.className = "exceed"; oBonus.parentNode.className = "exceed"; }
			//else { oImg.className = "fail"; oBonus.parentNode.className = "fail"; }
		}
		nRequirements = nReqChar;
		if (pwd.length >= nMinPwdLen) { var nMinReqChars = 3; } else { var nMinReqChars = 4; }
		if (nRequirements > nMinReqChars) {  // One or more required characters exist
			nScore = parseInt(nScore + (nRequirements * 2)); 
			sRequirements = "+ " + parseInt(nRequirements * 2);
		}
		//$("nRequirementsBonus").innerHTML = sRequirements;
		*/

		/* Determine if additional bonuses need to be applied and set image indicators accordingly */
		/*
		var arrChars = [nMidChar,nRequirements];
		var arrCharsIds = ["nMidChar","nRequirements"];
		var arrCharsLen = arrChars.length;
		for (var c=0; c < arrCharsLen; c++) {
			var oImg = $('div_' + arrCharsIds[c]);
			var oBonus = $(arrCharsIds[c] + 'Bonus');
			//$(arrCharsIds[c]).innerHTML = arrChars[c];
			if (arrCharsIds[c] == "nRequirements") { var minVal = nMinReqChars; } else { var minVal = 0; }
			//if (arrChars[c] == parseInt(minVal + 1)) { oImg.className = "pass"; oBonus.parentNode.className = "pass"; }
			//else if (arrChars[c] > parseInt(minVal + 1)) { oImg.className = "exceed"; oBonus.parentNode.className = "exceed"; }
			//else { oImg.className = "fail"; oBonus.parentNode.className = "fail"; }
		}
		*/

		/* Determine if suggested requirements have been met and set image indicators accordingly */
		/*
		var arrChars = [nAlphasOnly,nNumbersOnly,nRepChar,nConsecAlphaUC,nConsecAlphaLC,nConsecNumber,nSeqAlpha,nSeqNumber,nSeqSymbol];
		var arrCharsIds = ["nAlphasOnly","nNumbersOnly","nRepChar","nConsecAlphaUC","nConsecAlphaLC","nConsecNumber","nSeqAlpha","nSeqNumber","nSeqSymbol"];
		var arrCharsLen = arrChars.length;
		for (var c=0; c < arrCharsLen; c++) {
			var oImg = $('div_' + arrCharsIds[c]);
			var oBonus = $(arrCharsIds[c] + 'Bonus');
			//$(arrCharsIds[c]).innerHTML = arrChars[c];
			//if (arrChars[c] > 0) { oImg.className = "warn"; oBonus.parentNode.className = "warn"; }
			//else { oImg.className = "pass"; oBonus.parentNode.className = "pass"; }
		}
		*/
		
		/* Determine complexity based on overall score */
		if (nScore > 100) { nScore = 100; } else if (nScore < 0) { nScore = 0; }
		if (nScore >= 0 && nScore < 20) { sComplexity = "Very Weak"; }
		else if (nScore >= 20 && nScore < 40) { sComplexity = "Weak"; }
		else if (nScore >= 40 && nScore < 60) { sComplexity = "Good"; }
		else if (nScore >= 60 && nScore < 80) { sComplexity = "Strong"; }
		else if (nScore >= 80 && nScore <= 100) { sComplexity = "Very Strong"; }
		
		/* Display updated score criteria to client */
		$('scorebarBorder').style.display = "";
		oScorebar.style.backgroundPosition = "-" + parseInt(nScore * 4) + "px";
		oScore.innerHTML = sComplexity;
	}
	else {
		/* Display default score criteria to client */
		if(flag == 'http_passwd'){
				orig_pwd = "<% nvram_get("http_passwd"); %>";
				chkPass(orig_pwd, 'http_passwd');
		}
	}
}

String.prototype.strReverse = function() {
	var newstring = "";
	for (var s=0; s < this.length; s++) {
		newstring = this.charAt(s) + newstring;
	}
	return newstring;
	//strOrig = ' texttotrim ';
	//strReversed = strOrig.revstring();
};

// ---------- Viz add for pwd strength check [End] 2012.12 -----