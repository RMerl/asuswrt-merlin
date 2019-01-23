Long: tlsv1.3
Help: Use TLSv1.3
Protocols: TLS
Added: 7.52.0
---
Forces curl to use TLS version 1.3 when connecting to a remote TLS server.

Note that TLS 1.3 is only supported by a subset of TLS backends. At the time
of writing this, those are BoringSSL and NSS only.
