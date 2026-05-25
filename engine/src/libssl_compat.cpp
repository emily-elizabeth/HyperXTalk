/* libssl_compat.cpp
 *
 * OpenSSL 1.x -> OpenSSL 3 compatibility shims for prebuilt libpq.lib.
 *
 * In OpenSSL 3, SSL_get_peer_certificate() was renamed to
 * SSL_get1_peer_certificate().  The old name is only a preprocessor
 * macro in openssl/ssl.h, so libpq.lib (compiled against OpenSSL 1.x
 * headers) has an unresolved external for the original symbol.
 * This shim provides the old name as a real exported function that
 * forwards to the new one from our statically-linked libssl.lib.
 */

typedef struct ssl_st  SSL;
typedef struct x509_st X509;

extern "C" {

/* The real OpenSSL 3 function (in libssl.lib). */
X509 *SSL_get1_peer_certificate(const SSL *s);

/* Compatibility entry-point referenced by libpq.lib. */
X509 *SSL_get_peer_certificate(const SSL *s)
{
    return SSL_get1_peer_certificate(s);
}

} /* extern "C" */
