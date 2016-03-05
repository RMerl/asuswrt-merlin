gcc -g -W -Wall -Werror --pedantic -DTRACE_MODULE -I. -o tcache test/test_cache.c dpkt.c util.c cache.c extrn.c rtp.c
