# fetch version number from input file and write them to STDOUT
BEGIN {
  while ((getline < ARGV[1]) > 0) {
    if (match ($0, /^VERSION=/)) {
      split($1, t, "=");
      my_ver_str = t[2];
      split(my_ver_str, v, ".");
      gsub("[^0-9].*$", "", v[3]);
      my_ver = v[1] "," v[2] "," v[3];
    }
  }
  print "GEOIP_VERSION = " my_ver "";
  print "GEOIP_VERSION_STR = " my_ver_str "";
}
