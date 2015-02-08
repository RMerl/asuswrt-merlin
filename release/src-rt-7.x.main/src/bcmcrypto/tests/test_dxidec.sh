#!/bin/sh

echo "test: wep 128, expect success."
./dxidec -i dxidec_data/pkt-1.txt -alg wep -key 01234567890123456789012345
echo "test: wep 128, expect failure."
./dxidec -i dxidec_data/pkt-1.txt -alg wep -key 01234567890123456789012365

echo "test: wep 128, expect success."
./dxidec -i dxidec_data/pkt-2.txt -alg wep -key 01234
echo "test: wep 128, expect failure."
./dxidec -i dxidec_data/pkt-2.txt -alg wep -key 01235

echo "test: aes ccm 128, expect success."
./dxidec -i dxidec_data/pkt-3.txt -alg aes_ccm -key c97c1f67ce371185514a8a19f2bdd52f
echo "test: aes ccm 128, expect failue."
./dxidec -i dxidec_data/pkt-3.txt -alg aes_ccm -key c97c1f67ce371185514a8a19f2bdd5af

echo "test:tkip,expect success."
./dxidec -i dxidec_data/pkt-4.txt -alg tkip -key 1234567890123456789012345678901234567890123456789012345678901234

echo "test:tkip,expect failure."
./dxidec -i dxidec_data/pkt-4.txt -alg tkip -key a234567890123456789012345678901234567890123456789012345678901234

echo "test:tkip,expect failure."
./dxidec -i dxidec_data/pkt-4.txt -alg tkip -key 1234567890123456789012345678901234567890123a56789012345678901234


echo "test aes, mfp: expect success"
./dxidec -i dxidec_data/pkt-5.txt -alg aes_ccm -key 0x22702069C55753C08A5B03BB1D8BAE65 -debug 1

echo "test aes, mfp: expect success"
./dxidec -i dxidec_data/pkt-6.txt -alg aes_ccm -key 0x22702069C55753C08A5B03BB1D8BAE65 -debug 1 -tx 1

echo "test bip, mfp: expect success"
./dxidec -i dxidec_data/pkt-7.txt -alg bip -key 0x4ea9543e09cf2b1eca66ffc58bdecbcf -debug 1

echo "test bip, mfp: expect success"
./dxidec -i dxidec_data/pkt-8.txt -alg bip -key 0x4ea9543e09cf2b1eca66ffc58bdecbcf -debug 1 -tx 1
