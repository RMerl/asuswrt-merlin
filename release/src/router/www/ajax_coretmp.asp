
curr_coreTmp_2 = "<% sysinfo("temperature.2"); %>".replace("&deg;C", "");
if (curr_coreTmp_2 == "diasbled") curr_coreTmp_2 = 0;
curr_coreTmp_5 = "<% sysinfo("temperature.5"); %>".replace("&deg;C", "");
if (curr_coreTmp_5 == "disabled") curr_coreTmp_5 = 0;

