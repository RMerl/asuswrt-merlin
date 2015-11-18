#! /usr/bin/awk -f

/^Len/ { len = $3 }
/^Msg/ { msg = $3 }
/^MD/ { md = $3;
  if (len % 8 == 0)
    printf("test_hash(&nettle_sha3_xxx, /* %d octets */\nSHEX(\"%s\"),\nSHEX(\"%s\"));\n",
	   len / 8, len ? msg : "", md);
}
