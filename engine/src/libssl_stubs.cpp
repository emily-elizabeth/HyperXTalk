/* libssl_stubs.cpp
 *
 * Stub implementations of OpenSSL libssl API for dbpostgresql.
 *
 * libssl.lib was compiled with a newer MSVC toolchain (LTCG format) and
 * cannot be linked by the VS2019 linker (LNK1127 "library is corrupt").
 * These stubs allow dbpostgresql.dll to link.  PostgreSQL SSL connections
 * will fail gracefully; non-SSL (plaintext) connections are unaffected.
 *
 * To restore real SSL support: rebuild OpenSSL 3 with VS2019 by running
 * build-win-x86_64\build-openssl3.bat in a VS2019 Developer Command Prompt,
 * then remove this file from dbpostgresql.vcxproj and add libssl.lib back.
 */

#include <stdint.h>

/* Opaque SSL types — all we need is the pointer identity. */
typedef struct ssl_st         SSL;
typedef struct ssl_ctx_st     SSL_CTX;
typedef struct ssl_method_st  SSL_METHOD;
typedef struct x509_store_st  X509_STORE;
typedef void                  OPENSSL_INIT_SETTINGS;

extern "C" {

/* --- SSL_CTX functions --- */
SSL_CTX *SSL_CTX_new(const SSL_METHOD *method)                          { return nullptr; }
void     SSL_CTX_free(SSL_CTX *ctx)                                     {}
long     SSL_CTX_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)    { return 0; }
int      SSL_CTX_set_options(SSL_CTX *ctx, long options)                { return 0; }
X509_STORE *SSL_CTX_get_cert_store(const SSL_CTX *ctx)                 { return nullptr; }
int      SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file) { return 0; }
int      SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile, const char *CApath) { return 0; }
void     SSL_CTX_set_verify(SSL_CTX *ctx, int mode, void *cb)          {}

/* --- SSL (per-connection) functions --- */
SSL     *SSL_new(SSL_CTX *ctx)                                          { return nullptr; }
void     SSL_free(SSL *ssl)                                             {}
int      SSL_set_fd(SSL *s, int fd)                                     { return 0; }
long     SSL_set_options(SSL *ssl, long options)                        { return 0; }
void     SSL_set_verify(SSL *s, int mode, void *callback)               {}
int      SSL_set_ex_data(SSL *ssl, int idx, void *data)                 { return 0; }
int      SSL_use_PrivateKey(SSL *ssl, void *pkey)                       { return 0; }
int      SSL_use_PrivateKey_file(SSL *ssl, const char *file, int type) { return 0; }
int      SSL_use_certificate_file(SSL *ssl, const char *file, int type){ return 0; }
int      SSL_check_private_key(const SSL *ssl)                          { return 0; }
void    *SSL_get_peer_certificate(const SSL *s)                         { return nullptr; }
int      SSL_connect(SSL *ssl)                                          { return -1; }
int      SSL_read(SSL *ssl, void *buf, int num)                         { return -1; }
int      SSL_write(SSL *ssl, const void *buf, int num)                  { return -1; }
int      SSL_pending(const SSL *s)                                      { return 0; }
int      SSL_get_error(const SSL *ssl, int ret)                         { return 0; }
int      SSL_shutdown(SSL *s)                                           { return 0; }

/* --- SSL method --- */
const SSL_METHOD *TLS_method(void)                                      { return nullptr; }

/* --- OPENSSL init --- */
int OPENSSL_init_ssl(uint64_t opts, const OPENSSL_INIT_SETTINGS *settings) { return 1; }

} /* extern "C" */
