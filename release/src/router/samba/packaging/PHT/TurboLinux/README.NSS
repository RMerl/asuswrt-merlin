Instructions:

1) cd into the source directory for samba.
2) Extract nss_wins.tar.gz into the source directory of the samba
   distribution.  (It will create a directory called nsswitch)
3) Apply the Makefile.in patch
4) run configure
5) run "make nsswitch/libnss_wins.so"
6) cp nsswitch/libnss_wins.so /lib/libnss_wins.so.2
7) Add "wins" to the hosts entry in your nsswitch.conf
8) If you don't have a WINS server on your subnet, you'll need to
   add a line to your /etc/resolv.conf file of the form:

        wins_server 123.45.56.78

tridge@linuxcare.com
