FLAGS="-DTERMIO -O3 -DB_ENDIAN -fomit-frame-pointer -mv8 -Wall -Iinclude"
SHFLAGS="-DPIC -fpic"

gcc -c -Icrypto $SHFLAGS -fpic $FLAGS -o crypto.o crypto/crypto.c
ld -G -z text -o libcrypto-1.0.0q.so crypto.o

gcc -c -Issl $SHFLAGS $FLAGS -o ssl.o ssl/ssl.c
ld -G -z text -o libssl-1.0.0q.so ssl.o
