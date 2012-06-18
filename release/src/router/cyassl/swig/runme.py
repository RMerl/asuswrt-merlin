# file: runme.py

import cyassl 

print ""
print "Trying to connect to the echo server..."

ctx    = cyassl.SSL_CTX_new(cyassl.TLSv1_client_method())
ret    = cyassl.SSL_CTX_load_verify_locations(ctx, "../certs/ca-cert.pem", None)
ssl    = cyassl.SSL_new(ctx)

ret    = cyassl.CyaSSL_connect(ssl, "localhost", 11111)

if ret != cyassl.SSL_SUCCESS:
    print "Couldn't do SSL connect"
    err    = cyassl.SSL_get_error(ssl, 0)
    print "error string = ", cyassl.CyaSSL_error_string(err)
    exit(-1)

print "...Connected"
written = cyassl.SSL_write(ssl, "hello from python\r\n", 19)

if written > 0:
    print "Wrote ", written, " bytes"

byteArray = cyassl.byteArray(100)
readBytes = cyassl.SSL_read(ssl, byteArray, 100)

print "server reply: ", cyassl.cdata(byteArray, readBytes) 

