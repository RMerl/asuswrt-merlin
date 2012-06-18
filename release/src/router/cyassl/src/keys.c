/* keys.c
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */



#include "cyassl_int.h"
#include "cyassl_error.h"
#include <stdlib.h>
#include <string.h>
#ifdef SHOW_SECRETS
    #include <stdio.h>
#endif


#ifndef NO_TLS
    int MakeTlsMasterSecret(SSL*);
    void TLS_hmac(SSL* ssl, byte* digest, const byte* buffer, word32 sz,
                  int content, int verify);
#endif



int SetCipherSpecs(SSL* ssl)
{
    switch (ssl->options.cipherSuite) {

#ifdef BUILD_SSL_RSA_WITH_RC4_128_SHA
    case SSL_RSA_WITH_RC4_128_SHA :
        ssl->specs.bulk_cipher_algorithm = rc4;
        ssl->specs.cipher_type           = stream;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = RC4_KEY_SIZE;
        ssl->specs.iv_size               = 0;
        ssl->specs.block_size            = 0;

        break;
#endif

#ifdef BUILD_SSL_RSA_WITH_RC4_128_MD5
    case SSL_RSA_WITH_RC4_128_MD5 :
        ssl->specs.bulk_cipher_algorithm = rc4;
        ssl->specs.cipher_type           = stream;
        ssl->specs.mac_algorithm         = md5_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = MD5_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_MD5;
        ssl->specs.key_size              = RC4_KEY_SIZE;
        ssl->specs.iv_size               = 0;
        ssl->specs.block_size            = 0;

        break;
#endif

#ifdef BUILD_SSL_RSA_WITH_3DES_EDE_CBC_SHA
    case SSL_RSA_WITH_3DES_EDE_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = triple_des;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = DES3_KEY_SIZE;
        ssl->specs.block_size            = DES_BLOCK_SIZE;
        ssl->specs.iv_size               = DES_IV_SIZE;

        break;
#endif

#ifdef BUILD_TLS_RSA_WITH_AES_128_CBC_SHA
    case TLS_RSA_WITH_AES_128_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_128_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        break;
#endif

#ifdef BUILD_TLS_RSA_WITH_AES_256_CBC_SHA
    case TLS_RSA_WITH_AES_256_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_256_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        break;
#endif

#ifdef BUILD_TLS_PSK_WITH_AES_128_CBC_SHA
    case TLS_PSK_WITH_AES_128_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = psk_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_128_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        ssl->options.usingPSK_cipher     = 1;
        break;
#endif

#ifdef BUILD_TLS_PSK_WITH_AES_256_CBC_SHA
    case TLS_PSK_WITH_AES_256_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = psk_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_256_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        ssl->options.usingPSK_cipher     = 1;
        break;
#endif

#ifdef BUILD_TLS_DHE_RSA_WITH_AES_128_CBC_SHA
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = diffie_hellman_kea;
        ssl->specs.sig_algo              = rsa_sa_algo;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_128_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        break;
#endif

#ifdef BUILD_TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = aes;
        ssl->specs.cipher_type           = block;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = diffie_hellman_kea;
        ssl->specs.sig_algo              = rsa_sa_algo;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = AES_256_KEY_SIZE;
        ssl->specs.block_size            = AES_BLOCK_SIZE;
        ssl->specs.iv_size               = AES_IV_SIZE;

        break;
#endif

#ifdef BUILD_TLS_RSA_WITH_HC_128_CBC_MD5
    case TLS_RSA_WITH_HC_128_CBC_MD5 :
        ssl->specs.bulk_cipher_algorithm = hc128;
        ssl->specs.cipher_type           = stream;
        ssl->specs.mac_algorithm         = md5_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = MD5_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_MD5;
        ssl->specs.key_size              = HC_128_KEY_SIZE;
        ssl->specs.block_size            = 0;
        ssl->specs.iv_size               = HC_128_IV_SIZE;

        break;
#endif
            
#ifdef BUILD_TLS_RSA_WITH_HC_128_CBC_SHA
        case TLS_RSA_WITH_HC_128_CBC_SHA :
            ssl->specs.bulk_cipher_algorithm = hc128;
            ssl->specs.cipher_type           = stream;
            ssl->specs.mac_algorithm         = sha_mac;
            ssl->specs.kea                   = rsa_kea;
            ssl->specs.hash_size             = SHA_DIGEST_SIZE;
            ssl->specs.pad_size              = PAD_SHA;
            ssl->specs.key_size              = HC_128_KEY_SIZE;
            ssl->specs.block_size            = 0;
            ssl->specs.iv_size               = HC_128_IV_SIZE;
            
            break;
#endif

#ifdef BUILD_TLS_RSA_WITH_RABBIT_CBC_SHA
    case TLS_RSA_WITH_RABBIT_CBC_SHA :
        ssl->specs.bulk_cipher_algorithm = rabbit;
        ssl->specs.cipher_type           = stream;
        ssl->specs.mac_algorithm         = sha_mac;
        ssl->specs.kea                   = rsa_kea;
        ssl->specs.hash_size             = SHA_DIGEST_SIZE;
        ssl->specs.pad_size              = PAD_SHA;
        ssl->specs.key_size              = RABBIT_KEY_SIZE;
        ssl->specs.block_size            = 0;
        ssl->specs.iv_size               = RABBIT_IV_SIZE;

        break;
#endif

    default:
        return UNSUPPORTED_SUITE;
    }

    /* set TLS if it hasn't been turned off */
    if (ssl->version.major == 3 && ssl->version.minor >= 1) {
#ifndef NO_TLS
        ssl->options.tls = 1;
        ssl->hmac = TLS_hmac;
        if (ssl->version.minor == 2)
            ssl->options.tls1_1 = 1;
#endif
    }

#ifdef CYASSL_DTLS
    if (ssl->options.dtls)
        ssl->hmac = TLS_hmac;
#endif

    return 0;
}


enum KeyStuff {
    MASTER_ROUNDS = 3,
    PREFIX        = 3,     /* up to three letters for master prefix */
    KEY_PREFIX    = 7      /* up to 7 prefix letters for key rounds */


};


/* true or false, zero for error */
static int SetPrefix(byte* sha_input, int index)
{
    switch (index) {
    case 0:
        memcpy(sha_input, "A", 1);
        break;
    case 1:
        memcpy(sha_input, "BB", 2);
        break;
    case 2:
        memcpy(sha_input, "CCC", 3);
        break;
    case 3:
        memcpy(sha_input, "DDDD", 4);
        break;
    case 4:
        memcpy(sha_input, "EEEEE", 5);
        break;
    case 5:
        memcpy(sha_input, "FFFFFF", 6);
        break;
    case 6:
        memcpy(sha_input, "GGGGGGG", 7);
        break;
    default:
        return 0; 
    }
    return 1;
}


static int SetKeys(Ciphers* encrypt, Ciphers* decrypt, Keys* keys,
                   CipherSpecs* specs, byte side)
{
#ifdef BUILD_ARC4
    word32 sz = specs->key_size;
    if (specs->bulk_cipher_algorithm == rc4) {
        if (side == CLIENT_END) {
            Arc4SetKey(&encrypt->arc4, keys->client_write_key, sz);
            Arc4SetKey(&decrypt->arc4, keys->server_write_key, sz);
        }
        else {
            Arc4SetKey(&encrypt->arc4, keys->server_write_key, sz);
            Arc4SetKey(&decrypt->arc4, keys->client_write_key, sz);
        }
    }
#endif
    
#ifdef BUILD_HC128
    if (specs->bulk_cipher_algorithm == hc128) {
        if (side == CLIENT_END) {
            Hc128_SetKey(&encrypt->hc128, keys->client_write_key,
                                          keys->client_write_IV);
            Hc128_SetKey(&decrypt->hc128, keys->server_write_key,
                                          keys->server_write_IV);
        }
        else {
            Hc128_SetKey(&encrypt->hc128, keys->server_write_key,
                                         keys->server_write_IV);
            Hc128_SetKey(&decrypt->hc128, keys->client_write_key,
                                         keys->client_write_IV);
        }
    }
#endif
    
#ifdef BUILD_RABBIT
    if (specs->bulk_cipher_algorithm == rabbit) {
        if (side == CLIENT_END) {
            RabbitSetKey(&encrypt->rabbit, keys->client_write_key,
                                           keys->client_write_IV);
            RabbitSetKey(&decrypt->rabbit, keys->server_write_key,
                                           keys->server_write_IV);
        }
        else {
            RabbitSetKey(&encrypt->rabbit, keys->server_write_key,
                                           keys->server_write_IV);
            RabbitSetKey(&decrypt->rabbit, keys->client_write_key,
                                           keys->client_write_IV);
        }
    }
#endif
    
#ifdef BUILD_DES3
    if (specs->bulk_cipher_algorithm == triple_des) {
        if (side == CLIENT_END) {
            Des3_SetKey(&encrypt->des3, keys->client_write_key,
                        keys->client_write_IV, DES_ENCRYPTION);
            Des3_SetKey(&decrypt->des3, keys->server_write_key,
                        keys->server_write_IV, DES_DECRYPTION);
        }
        else {
            Des3_SetKey(&encrypt->des3, keys->server_write_key,
                        keys->server_write_IV, DES_ENCRYPTION);
            Des3_SetKey(&decrypt->des3, keys->client_write_key,
                keys->client_write_IV, DES_DECRYPTION);
        }
    }
#endif

#ifdef BUILD_AES
    if (specs->bulk_cipher_algorithm == aes) {
        if (side == CLIENT_END) {
            AesSetKey(&encrypt->aes, keys->client_write_key,
                      specs->key_size, keys->client_write_IV,
                      AES_ENCRYPTION);
            AesSetKey(&decrypt->aes, keys->server_write_key,
                      specs->key_size, keys->server_write_IV,
                      AES_DECRYPTION);
        }
        else {
            AesSetKey(&encrypt->aes, keys->server_write_key,
                      specs->key_size, keys->server_write_IV,
                      AES_ENCRYPTION);
            AesSetKey(&decrypt->aes, keys->client_write_key,
                      specs->key_size, keys->client_write_IV,
                      AES_DECRYPTION);
        }
    }
#endif

    keys->sequence_number      = 0;
    keys->peer_sequence_number = 0;
    keys->encryptionOn         = 0;

    return 0;
}


/* TLS can call too */
int StoreKeys(SSL* ssl, const byte* keyData)
{
    int sz = ssl->specs.hash_size, i;

    memcpy(ssl->keys.client_write_MAC_secret, keyData, sz);
    i = sz;
    memcpy(ssl->keys.server_write_MAC_secret,&keyData[i], sz);
    i += sz;

    sz = ssl->specs.key_size;
    memcpy(ssl->keys.client_write_key, &keyData[i], sz);
    i += sz;
    memcpy(ssl->keys.server_write_key, &keyData[i], sz);
    i += sz;

    sz = ssl->specs.iv_size;
    memcpy(ssl->keys.client_write_IV, &keyData[i], sz);
    i += sz;
    memcpy(ssl->keys.server_write_IV, &keyData[i], sz);

    return SetKeys(&ssl->encrypt, &ssl->decrypt, &ssl->keys, &ssl->specs,
                   ssl->options.side);
}


int DeriveKeys(SSL* ssl)
{
    int length = 2 * ssl->specs.hash_size + 
                 2 * ssl->specs.key_size  +
                 2 * ssl->specs.iv_size;
    int rounds = (length + MD5_DIGEST_SIZE - 1 ) / MD5_DIGEST_SIZE, i;

    byte shaOutput[SHA_DIGEST_SIZE];
    byte md5Input[SECRET_LEN + SHA_DIGEST_SIZE];
    byte shaInput[KEY_PREFIX + SECRET_LEN + 2 * RAN_LEN];
  
    Md5 md5;
    Sha sha;

    byte keyData[KEY_PREFIX * MD5_DIGEST_SIZE];  /* max size */

    InitMd5(&md5);
    InitSha(&sha);

    memcpy(md5Input, ssl->arrays.masterSecret, SECRET_LEN);

    for (i = 0; i < rounds; ++i) {
        int j   = i + 1;
        int idx = j;

        if (!SetPrefix(shaInput, i)) {
            return PREFIX_ERROR;
        }

        memcpy(shaInput + idx, ssl->arrays.masterSecret, SECRET_LEN);
        idx += SECRET_LEN;
        memcpy(shaInput + idx, ssl->arrays.serverRandom, RAN_LEN);
        idx += RAN_LEN;
        memcpy(shaInput + idx, ssl->arrays.clientRandom, RAN_LEN);
        idx += RAN_LEN;

        ShaUpdate(&sha, shaInput, sizeof(shaInput) - KEY_PREFIX + j);
        ShaFinal(&sha, shaOutput);

        memcpy(&md5Input[SECRET_LEN], shaOutput, SHA_DIGEST_SIZE);
        Md5Update(&md5, md5Input, sizeof(md5Input));
        Md5Final(&md5, keyData + i * MD5_DIGEST_SIZE);
    }

    return StoreKeys(ssl, keyData);
}


void CleanPreMaster(SSL* ssl)
{
    int i, sz = ssl->arrays.preMasterSz;

    for (i = 0; i < sz; i++)
        ssl->arrays.preMasterSecret[i] = 0;

    RNG_GenerateBlock(&ssl->rng, ssl->arrays.preMasterSecret, sz);

    for (i = 0; i < sz; i++)
        ssl->arrays.preMasterSecret[i] = 0;

}


/* Create and store the master secret see page 32, 6.1 */
int MakeMasterSecret(SSL* ssl)
{
    byte   shaOutput[SHA_DIGEST_SIZE];
    byte   md5Input[ENCRYPT_LEN + SHA_DIGEST_SIZE];
    byte   shaInput[PREFIX + ENCRYPT_LEN + 2 * RAN_LEN];
    int    i;
    word32 idx;
    word32 pmsSz = ssl->arrays.preMasterSz;

    Md5 md5;
    Sha sha;

#ifdef SHOW_SECRETS
    {
        int j;
        printf("pre master secret: ");
        for (j = 0; j < pmsSz; j++)
            printf("%02x", ssl->arrays.preMasterSecret[j]);
        printf("\n");
    }
#endif

#ifndef NO_TLS
    if (ssl->options.tls) return MakeTlsMasterSecret(ssl);
#endif

    InitMd5(&md5);
    InitSha(&sha);

    memcpy(md5Input, ssl->arrays.preMasterSecret, pmsSz);

    for (i = 0; i < MASTER_ROUNDS; ++i) {
        byte prefix[PREFIX];
        if (!SetPrefix(prefix, i)) {
            return PREFIX_ERROR;
        }

        idx = 0;
        memcpy(shaInput, prefix, i + 1);
        idx += i + 1;

        memcpy(shaInput + idx, ssl->arrays.preMasterSecret, pmsSz);
        idx += pmsSz;
        memcpy(shaInput + idx, ssl->arrays.clientRandom, RAN_LEN);
        idx += RAN_LEN;
        memcpy(shaInput + idx, ssl->arrays.serverRandom, RAN_LEN);
        idx += RAN_LEN;
        ShaUpdate(&sha, shaInput, idx);
        ShaFinal(&sha, shaOutput);

        idx = pmsSz;  /* preSz */
        memcpy(md5Input + idx, shaOutput, SHA_DIGEST_SIZE);
        idx += SHA_DIGEST_SIZE;
        Md5Update(&md5, md5Input, idx);
        Md5Final(&md5, &ssl->arrays.masterSecret[i * MD5_DIGEST_SIZE]);
    }

#ifdef SHOW_SECRETS
    {
        int i;
        printf("master secret: ");
        for (i = 0; i < SECRET_LEN; i++)
            printf("%02x", ssl->arrays.masterSecret[i]);
        printf("\n");
    }
#endif

    DeriveKeys(ssl);
    CleanPreMaster(ssl);

    return 0;
}




