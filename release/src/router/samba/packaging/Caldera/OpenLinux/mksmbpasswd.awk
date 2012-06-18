#!/usr/bin/awk -f
BEGIN {
  FS=":";
  X="XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
  U="[U          ]";
  L="LCT-00000000";
  printf("#\n# SMB password file.\n#\n");
}
$1 ~ /^[A-Za-z0-9_]/ {
  printf( "%s:%s:%s:%s:%s:%s:%s\n", $1, $3, X, X, U, L, $5);
}

