#!/usr/bin/python
# Copyright 2012-2013, The Tor Project, Inc
# See LICENSE for licensing information

"""
ntor_ref.py


This module is a reference implementation for the "ntor" protocol
s proposed by Goldberg, Stebila, and Ustaoglu and as instantiated in
Tor Proposal 216.

It's meant to be used to validate Tor's ntor implementation.  It
requirs the curve25519 python module from the curve25519-donna
package.

                *** DO NOT USE THIS IN PRODUCTION. ***

commands:

   gen_kdf_vectors: Print out some test vectors for the RFC5869 KDF.
   timing: Print a little timing information about this implementation's
      handshake.
   self-test: Try handshaking with ourself; make sure we can.
   test-tor: Handshake with tor's ntor implementation via the program
      src/test/test-ntor-cl; make sure we can.

"""

import binascii
try:
    import curve25519
    curve25519mod = curve25519.keys
except ImportError:
    curve25519 = None
    import slownacl_curve25519
    curve25519mod = slownacl_curve25519

import hashlib
import hmac
import subprocess
import sys

# **********************************************************************
# Helpers and constants

def HMAC(key,msg):
    "Return the HMAC-SHA256 of 'msg' using the key 'key'."
    H = hmac.new(key, b"", hashlib.sha256)
    H.update(msg)
    return H.digest()

def H(msg,tweak):
    """Return the hash of 'msg' using tweak 'tweak'.  (In this version of ntor,
       the tweaked hash is just HMAC with the tweak as the key.)"""
    return HMAC(key=tweak,
                msg=msg)

def keyid(k):
    """Return the 32-byte key ID of a public key 'k'. (Since we're
       using curve25519, we let k be its own keyid.)
    """
    return k.serialize()

NODE_ID_LENGTH = 20
KEYID_LENGTH = 32
G_LENGTH = 32
H_LENGTH = 32

PROTOID = b"ntor-curve25519-sha256-1"
M_EXPAND = PROTOID + b":key_expand"
T_MAC    = PROTOID + b":mac"
T_KEY    = PROTOID + b":key_extract"
T_VERIFY = PROTOID + b":verify"

def H_mac(msg): return H(msg, tweak=T_MAC)
def H_verify(msg): return H(msg, tweak=T_VERIFY)

class PrivateKey(curve25519mod.Private):
    """As curve25519mod.Private, but doesn't regenerate its public key
       every time you ask for it.
    """
    def __init__(self):
        curve25519mod.Private.__init__(self)
        self._memo_public = None

    def get_public(self):
        if self._memo_public is None:
            self._memo_public = curve25519mod.Private.get_public(self)

        return self._memo_public

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if sys.version < '3':
   def int2byte(i):
      return chr(i)
else:
   def int2byte(i):
      return bytes([i])

def  kdf_rfc5869(key, salt, info, n):

    prk = HMAC(key=salt, msg=key)

    out = b""
    last = b""
    i = 1
    while len(out) < n:
        m = last + info + int2byte(i)
        last = h = HMAC(key=prk, msg=m)
        out += h
        i = i + 1
    return out[:n]

def kdf_ntor(key, n):
    return kdf_rfc5869(key, T_KEY, M_EXPAND, n)

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

def client_part1(node_id, pubkey_B):
    """Initial handshake, client side.

       From the specification:

         <<To send a create cell, the client generates a keypair x,X =
           KEYGEN(), and sends a CREATE cell with contents:

           NODEID:     ID             -- ID_LENGTH bytes
           KEYID:      KEYID(B)       -- H_LENGTH bytes
           CLIENT_PK:  X              -- G_LENGTH bytes
         >>

       Takes node_id -- a digest of the server's identity key,
             pubkey_B -- a public key for the server.
       Returns a tuple of (client secret key x, client->server message)"""

    assert len(node_id) == NODE_ID_LENGTH

    key_id = keyid(pubkey_B)
    seckey_x = PrivateKey()
    pubkey_X = seckey_x.get_public().serialize()

    message = node_id + key_id + pubkey_X

    assert len(message) == NODE_ID_LENGTH + H_LENGTH + H_LENGTH
    return seckey_x , message

def hash_nil(x):
    """Identity function: if we don't pass a hash function that does nothing,
       the curve25519 python lib will try to sha256 it for us."""
    return x

def bad_result(r):
    """Helper: given a result of multiplying a public key by a private key,
       return True iff one of the inputs was broken"""
    assert len(r) == 32
    return r == '\x00'*32

def server(seckey_b, my_node_id, message, keyBytes=72):
    """Handshake step 2, server side.

       From the spec:

       <<
         The server generates a keypair of y,Y = KEYGEN(), and computes

           secret_input = EXP(X,y) | EXP(X,b) | ID | B | X | Y | PROTOID
           KEY_SEED = H(secret_input, t_key)
           verify = H(secret_input, t_verify)
           auth_input = verify | ID | B | Y | X | PROTOID | "Server"

         The server sends a CREATED cell containing:

           SERVER_PK:  Y                     -- G_LENGTH bytes
           AUTH:       H(auth_input, t_mac)  -- H_LENGTH byets
        >>

       Takes seckey_b -- the server's secret key
             my_node_id -- the servers's public key digest,
             message -- a message from a client
             keybytes -- amount of key material to generate

       Returns a tuple of (key material, sever->client reply), or None on
       error.
    """

    assert len(message) == NODE_ID_LENGTH + H_LENGTH + H_LENGTH

    if my_node_id != message[:NODE_ID_LENGTH]:
        return None

    badness = (keyid(seckey_b.get_public()) !=
               message[NODE_ID_LENGTH:NODE_ID_LENGTH+H_LENGTH])

    pubkey_X = curve25519mod.Public(message[NODE_ID_LENGTH+H_LENGTH:])
    seckey_y = PrivateKey()
    pubkey_Y = seckey_y.get_public()
    pubkey_B = seckey_b.get_public()
    xy = seckey_y.get_shared_key(pubkey_X, hash_nil)
    xb = seckey_b.get_shared_key(pubkey_X, hash_nil)

    # secret_input = EXP(X,y) | EXP(X,b) | ID | B | X | Y | PROTOID
    secret_input = (xy + xb + my_node_id +
                    pubkey_B.serialize() +
                    pubkey_X.serialize() +
                    pubkey_Y.serialize() +
                    PROTOID)

    verify = H_verify(secret_input)

    # auth_input = verify | ID | B | Y | X | PROTOID | "Server"
    auth_input = (verify +
                  my_node_id +
                  pubkey_B.serialize() +
                  pubkey_Y.serialize() +
                  pubkey_X.serialize() +
                  PROTOID +
                  b"Server")

    msg = pubkey_Y.serialize() + H_mac(auth_input)

    badness += bad_result(xb)
    badness += bad_result(xy)

    if badness:
        return None

    keys = kdf_ntor(secret_input, keyBytes)

    return keys, msg

def client_part2(seckey_x, msg, node_id, pubkey_B, keyBytes=72):
    """Handshake step 3: client side again.

       From the spec:

       <<
         The client then checks Y is in G^* [see NOTE below], and computes

         secret_input = EXP(Y,x) | EXP(B,x) | ID | B | X | Y | PROTOID
         KEY_SEED = H(secret_input, t_key)
         verify = H(secret_input, t_verify)
         auth_input = verify | ID | B | Y | X | PROTOID | "Server"

         The client verifies that AUTH == H(auth_input, t_mac).
       >>

       Takes seckey_x -- the secret key we generated in step 1.
             msg -- the message from the server.
             node_id -- the node_id we used in step 1.
             server_key -- the same public key we used in step 1.
             keyBytes -- the number of bytes we want to generate
       Returns key material, or None on error

    """
    assert len(msg) == G_LENGTH + H_LENGTH

    pubkey_Y = curve25519mod.Public(msg[:G_LENGTH])
    their_auth = msg[G_LENGTH:]

    pubkey_X = seckey_x.get_public()

    yx = seckey_x.get_shared_key(pubkey_Y, hash_nil)
    bx = seckey_x.get_shared_key(pubkey_B, hash_nil)


    # secret_input = EXP(Y,x) | EXP(B,x) | ID | B | X | Y | PROTOID
    secret_input = (yx + bx + node_id +
                    pubkey_B.serialize() +
                    pubkey_X.serialize() +
                    pubkey_Y.serialize() + PROTOID)

    verify = H_verify(secret_input)

    # auth_input = verify | ID | B | Y | X | PROTOID | "Server"
    auth_input = (verify + node_id +
                  pubkey_B.serialize() +
                  pubkey_Y.serialize() +
                  pubkey_X.serialize() + PROTOID +
                  b"Server")

    my_auth = H_mac(auth_input)

    badness = my_auth != their_auth
    badness = bad_result(yx) + bad_result(bx)

    if badness:
        return None

    return kdf_ntor(secret_input, keyBytes)

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

def demo(node_id=b"iToldYouAboutStairs.", server_key=PrivateKey()):
    """
       Try to handshake with ourself.
    """
    x, create = client_part1(node_id, server_key.get_public())
    skeys, created = server(server_key, node_id, create)
    ckeys = client_part2(x, created, node_id, server_key.get_public())
    assert len(skeys) == 72
    assert len(ckeys) == 72
    assert skeys == ckeys
    print("OK")

# ======================================================================
def timing():
    """
       Use Python's timeit module to see how fast this nonsense is
    """
    import timeit
    t = timeit.Timer(stmt="ntor_ref.demo(N,SK)",
       setup="import ntor_ref,curve25519;N='ABCD'*5;SK=ntor_ref.PrivateKey()")
    print(t.timeit(number=1000))

# ======================================================================

def kdf_vectors():
    """
       Generate some vectors to check our KDF.
    """
    import binascii
    def kdf_vec(inp):
        k = kdf(inp, T_KEY, M_EXPAND, 100)
        print(repr(inp), "\n\""+ binascii.b2a_hex(k)+ "\"")
    kdf_vec("")
    kdf_vec("Tor")
    kdf_vec("AN ALARMING ITEM TO FIND ON YOUR CREDIT-RATING STATEMENT")

# ======================================================================


def test_tor():
    """
       Call the test-ntor-cl command-line program to make sure we can
       interoperate with Tor's ntor program
    """
    enhex=lambda s: binascii.b2a_hex(s)
    dehex=lambda s: binascii.a2b_hex(s.strip())

    PROG = b"./src/test/test-ntor-cl"
    def tor_client1(node_id, pubkey_B):
        " returns (msg, state) "
        p = subprocess.Popen([PROG, b"client1", enhex(node_id),
                              enhex(pubkey_B.serialize())],
                             stdout=subprocess.PIPE)
        return map(dehex, p.stdout.readlines())
    def tor_server1(seckey_b, node_id, msg, n):
        " returns (msg, keys) "
        p = subprocess.Popen([PROG, "server1", enhex(seckey_b.serialize()),
                              enhex(node_id), enhex(msg), str(n)],
                             stdout=subprocess.PIPE)
        return map(dehex, p.stdout.readlines())
    def tor_client2(state, msg, n):
        " returns (keys,) "
        p = subprocess.Popen([PROG, "client2", enhex(state),
                              enhex(msg), str(n)],
                             stdout=subprocess.PIPE)
        return map(dehex, p.stdout.readlines())


    node_id = b"thisisatornodeid$#%^"
    seckey_b = PrivateKey()
    pubkey_B = seckey_b.get_public()

    # Do a pure-Tor handshake
    c2s_msg, c_state = tor_client1(node_id, pubkey_B)
    s2c_msg, s_keys = tor_server1(seckey_b, node_id, c2s_msg, 90)
    c_keys, = tor_client2(c_state, s2c_msg, 90)
    assert c_keys == s_keys
    assert len(c_keys) == 90

    # Try a mixed handshake with Tor as the client
    c2s_msg, c_state = tor_client1(node_id, pubkey_B)
    s_keys, s2c_msg = server(seckey_b, node_id, c2s_msg, 90)
    c_keys, = tor_client2(c_state, s2c_msg, 90)
    assert c_keys == s_keys
    assert len(c_keys) == 90

    # Now do a mixed handshake with Tor as the server
    c_x, c2s_msg = client_part1(node_id, pubkey_B)
    s2c_msg, s_keys = tor_server1(seckey_b, node_id, c2s_msg, 90)
    c_keys = client_part2(c_x, s2c_msg, node_id, pubkey_B, 90)
    assert c_keys == s_keys
    assert len(c_keys) == 90

    print("OK")

# ======================================================================

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
    elif sys.argv[1] == 'gen_kdf_vectors':
        kdf_vectors()
    elif sys.argv[1] == 'timing':
        timing()
    elif sys.argv[1] == 'self-test':
        demo()
    elif sys.argv[1] == 'test-tor':
        test_tor()

    else:
        print(__doc__)
