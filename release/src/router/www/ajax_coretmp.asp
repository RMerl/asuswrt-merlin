curr_coreTmp_2_raw = "<% sysinfo("temperature.2"); %>";
curr_coreTmp_2 = (curr_coreTmp_2_raw.indexOf("disabled") > 0 ? 0 : curr_coreTmp_2_raw.replace("&deg;C", ""));

curr_coreTmp_5_raw = "<% sysinfo("temperature.5"); %>";
curr_coreTmp_5 = (curr_coreTmp_5_raw.indexOf("disabled") > 0 ? 0 : curr_coreTmp_5_raw.replace("&deg;C", ""));

curr_coreTmp_cpu = "<% get_cpu_temperature(); %>";
