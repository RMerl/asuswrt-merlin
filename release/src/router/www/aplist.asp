var aplist = <% nvram_dump("apscan",""); %>;

var profile_ssid = "<% nvram_char_to_ascii("", "sta_ssid"); %>";
										