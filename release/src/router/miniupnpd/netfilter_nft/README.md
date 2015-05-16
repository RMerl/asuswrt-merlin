Miniupnpd nftables support by Tomofumi Hayashi (s1061123@gmail.com).

##Current Status
nftables support is 'alpha' version, not "so much" stable.

##Supported Features
- IPv4 NAT/Filter add/del.

##How to build miniupnpd with nftables:
Run 'make' command with 'Makefile.linux_nft',

`make -f Makefile.linux_nft`

##How to Run
Please run 'netfilter_nft/scripts/nft_init.sh' to add miniupnpd chain.

`sudo ./netfilter_nft/scripts/nft_init.sh`

##FAQ
I will add this section when I get question.
Comments and Questions are welcome ;)
