#include <cstring>

#include <common/compiler_message.h>
#include <common/string_oprs.h>
#include <config/compiler_features.h>

#include <algorithm/crypto_cipher.h>
#include <std/static_assert.h>

#ifdef CRYPTO_CIPHER_ENABLED

#ifdef CRYPTO_USE_LIBSODIUM
#include <sodium.h>

#define LIBSODIUM_BLOCK_SIZE 64
typedef uint64_t libsodium_counter_t;
#define LIBSODIUM_COUNTER_SIZE sizeof(libsodium_counter_t)

static inline libsodium_counter_t libsodium_get_block(const unsigned char *p) {
    return ((uint32_t)(p)[0]) | (((uint32_t)(p)[1]) << 8) | (((uint32_t)p[2]) << 16) | (((uint32_t)(p)[3]) << 24);
}

static inline libsodium_counter_t libsodium_get_counter(const unsigned char *iv) {
    uint32_t low  = libsodium_get_block(iv);
    uint32_t high = libsodium_get_block(iv + 4);
    return (((libsodium_counter_t)(high)) << 32) | low;
}

#endif

/**
 * @note boringssl macros
 *       OPENSSL_IS_BORINGSSL
 *       BORINGSSL_API_VERSION      9
 *       OPENSSL_VERSION_NUMBER     0x1010007f
 *
 * @note libressl macros
 *       LIBRESSL_VERSION_NUMBER    0x2060500fL
 *       LIBRESSL_VERSION_TEXT      "LibreSSL 2.6.5"
 *       OPENSSL_VERSION_NUMBER     0x20000000L
 *       OPENSSL_VERSION_TEXT       LIBRESSL_VERSION_TEXT
 */

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)

#if defined(LIBRESSL_VERSION_NUMBER)

#if LIBRESSL_VERSION_NUMBER > 0x2040000fL
#define CRYPTO_USE_CHACHA20_WITH_CIPHER
#define CRYPTO_USE_CHACHA20_POLY1305_WITH_CIPHER
#endif

#elif defined(OPENSSL_IS_BORINGSSL) && defined(BORINGSSL_API_VERSION)

#if BORINGSSL_API_VERSION >= 4
#define CRYPTO_USE_CHACHA20_POLY1305_WITH_CIPHER
#endif

#elif defined(OPENSSL_VERSION_NUMBER)

#if OPENSSL_VERSION_NUMBER >= 0x1010002fL
#define CRYPTO_USE_CHACHA20_WITH_CIPHER
#define CRYPTO_USE_CHACHA20_POLY1305_WITH_CIPHER
#endif

#endif

// compact for old openssl and openssl compatible library(libressl, for example)

#ifndef EVP_CTRL_AEAD_SET_IVLEN
#define EVP_CTRL_AEAD_SET_IVLEN EVP_CTRL_GCM_SET_IVLEN
#endif

#ifndef EVP_CTRL_AEAD_GET_TAG
#define EVP_CTRL_AEAD_GET_TAG EVP_CTRL_GCM_GET_TAG
#endif

#ifndef EVP_CTRL_AEAD_SET_IVLEN
#define EVP_CTRL_AEAD_SET_IVLEN EVP_CTRL_GCM_SET_IVLEN
#endif

#ifndef EVP_CTRL_AEAD_SET_TAG
#define EVP_CTRL_AEAD_SET_TAG EVP_CTRL_GCM_SET_TAG
#endif

#endif

namespace util {
    namespace crypto {
        enum cipher_interface_method_t {
            EN_CIMT_INVALID = 0, // inner
            EN_CIMT_XXTEA   = 1, // inner
            EN_CIMT_INNER,       // inner bound
            EN_CIMT_CIPHER,      // using openssl/libressl/boringssl/mbedtls
            EN_CIMT_LIBSODIUM,   // using libsodium
            EN_CIMT_LIBSODIUM_CHACHA20,
            EN_CIMT_LIBSODIUM_CHACHA20_IETF,
            EN_CIMT_LIBSODIUM_XCHACHA20, // imported in libsodium 1.0.12 or upper
            EN_CIMT_LIBSODIUM_SALSA20,
            EN_CIMT_LIBSODIUM_XSALSA20, // imported in libsodium 1.0.12 or upper
            EN_CIMT_LIBSODIUM_CHACHA20_POLY1305,
            EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF,
            EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF, // imported in libsodium 1.0.12 or upper
        };

        enum cipher_interface_flags_t {
            EN_CIFT_NONE                   = 0,      // using inner algorithm
            EN_CIFT_NO_FINISH              = 0x0001, // should not call finish after update
            EN_CIFT_AEAD                   = 0x0010, // is aead cipher
            EN_CIFT_VARIABLE_IV_LEN        = 0x0020, // can be variable iv length
            EN_CIFT_AEAD_SET_LENGTH_BEFORE = 0x0040, // call update to set length before update
            EN_CIFT_DECRYPT_NO_PADDING     = 0x0100, // set no padding when decrypt
            EN_CIFT_ENCRYPT_NO_PADDING     = 0x0200, // set no padding when encrypt
        };

        struct cipher_interface_info_t {
            const char *              name;
            cipher_interface_method_t method;
            const char *              openssl_name;
            const char *              mbedtls_name;
            uint32_t                  flags;
        };

        namespace details {
            static inline cipher::error_code_t::type setup_errorno(cipher &ci, int64_t err, cipher::error_code_t::type ret) {
                ci.set_last_errno(err);
                return ret;
            }

            // openssl/libressl/boringssl   @see crypto/objects/obj_dat.h or crypto/obj/obj_dat.h
            // mbedtls                      @see library/cipher_wrap.c
            static const cipher_interface_info_t supported_ciphers[] = {
                {"xxtea", EN_CIMT_XXTEA, NULL, "xxtea", EN_CIFT_NONE},
                {"rc4", EN_CIMT_CIPHER, NULL, "ARC4-128", EN_CIFT_NONE},
                {"aes-128-cfb", EN_CIMT_CIPHER, NULL, "AES-128-CFB128", EN_CIFT_NONE},
                {"aes-192-cfb", EN_CIMT_CIPHER, NULL, "AES-192-CFB128", EN_CIFT_NONE},
                {"aes-256-cfb", EN_CIMT_CIPHER, NULL, "AES-256-CFB128", EN_CIFT_NONE},
                {"aes-128-ctr", EN_CIMT_CIPHER, NULL, "AES-128-CTR", EN_CIFT_NONE},
                {"aes-192-ctr", EN_CIMT_CIPHER, NULL, "AES-192-CTR", EN_CIFT_NONE},
                {"aes-256-ctr", EN_CIMT_CIPHER, NULL, "AES-256-CTR", EN_CIFT_NONE},
                {"aes-128-ecb", EN_CIMT_CIPHER, NULL, "AES-128-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"aes-192-ecb", EN_CIMT_CIPHER, NULL, "AES-192-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"aes-256-ecb", EN_CIMT_CIPHER, NULL, "AES-256-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"aes-128-cbc", EN_CIMT_CIPHER, NULL, "AES-128-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"aes-192-cbc", EN_CIMT_CIPHER, NULL, "AES-192-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"aes-256-cbc", EN_CIMT_CIPHER, NULL, "AES-256-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-ecb", EN_CIMT_CIPHER, NULL, "DES-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-cbc", EN_CIMT_CIPHER, NULL, "DES-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-ede", EN_CIMT_CIPHER, NULL, "DES-EDE-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-ede-cbc", EN_CIMT_CIPHER, NULL, "DES-EDE-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-ede3", EN_CIMT_CIPHER, NULL, "DES-EDE3-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"des-ede3-cbc", EN_CIMT_CIPHER, NULL, "DES-EDE3-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                // {"bf-ecb", EN_CIMT_CIPHER, "BLOWFISH-ECB", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                // this unit test of this cipher can not passed in all libraries
                {"bf-cbc", EN_CIMT_CIPHER, NULL, "BLOWFISH-CBC", EN_CIFT_ENCRYPT_NO_PADDING | EN_CIFT_DECRYPT_NO_PADDING},
                {"bf-cfb", EN_CIMT_CIPHER, NULL, "BLOWFISH-CFB64", EN_CIFT_NONE},
                {"camellia-128-cfb", EN_CIMT_CIPHER, NULL, "CAMELLIA-128-CFB128", EN_CIFT_NONE},
                {"camellia-192-cfb", EN_CIMT_CIPHER, NULL, "CAMELLIA-192-CFB128", EN_CIFT_NONE},
                {"camellia-256-cfb", EN_CIMT_CIPHER, NULL, "CAMELLIA-256-CFB128", EN_CIFT_NONE},

#if defined(CRYPTO_USE_CHACHA20_WITH_CIPHER)
                {"chacha20", // only available on openssl 1.1.0 and upper
                 EN_CIMT_CIPHER, NULL,
                 "CHACHA20", // only available on later mbedtls version, @see https://github.com/ARMmbed/mbedtls/pull/485
                 EN_CIFT_NONE},
#endif

#ifdef CRYPTO_USE_LIBSODIUM
                {"chacha20", EN_CIMT_LIBSODIUM_CHACHA20, NULL, "CHACHA20", EN_CIFT_NONE},
                {"chacha20-ietf", EN_CIMT_LIBSODIUM_CHACHA20_IETF, NULL, "CHACHA20-IETF", EN_CIFT_NONE},
#ifdef crypto_stream_xchacha20_KEYBYTES
                {"xchacha20", EN_CIMT_LIBSODIUM_XCHACHA20, NULL, "XCHACHA20", EN_CIFT_NONE},
#endif
                {"salsa20", EN_CIMT_LIBSODIUM_SALSA20, NULL, "SALSA20", EN_CIFT_NONE},
#ifdef crypto_stream_xsalsa20_KEYBYTES
                {"xsalsa20", EN_CIMT_LIBSODIUM_XSALSA20, NULL, "XSALSA20", EN_CIFT_NONE},
#endif

#endif

                {"aes-128-gcm", EN_CIMT_CIPHER, NULL, "AES-128-GCM", EN_CIFT_AEAD | EN_CIFT_VARIABLE_IV_LEN},
                {"aes-192-gcm", EN_CIMT_CIPHER, NULL, "AES-192-GCM", EN_CIFT_AEAD | EN_CIFT_VARIABLE_IV_LEN},
                {"aes-256-gcm", EN_CIMT_CIPHER, NULL, "AES-256-GCM", EN_CIFT_AEAD | EN_CIFT_VARIABLE_IV_LEN},

#if defined(CRYPTO_USE_CHACHA20_POLY1305_WITH_CIPHER)
                {"chacha20-poly1305-ietf", // only available on openssl 1.1.0 and upper or boringssl
                 EN_CIMT_CIPHER, "chacha20-poly1305",
                 "CHACHA20-POLY1305", // only available on later mbedtls version, @see https://github.com/ARMmbed/mbedtls/pull/485
                 EN_CIFT_AEAD | EN_CIFT_VARIABLE_IV_LEN},
#endif

#ifdef CRYPTO_USE_LIBSODIUM
                {"chacha20-poly1305", EN_CIMT_LIBSODIUM_CHACHA20_POLY1305, NULL, "CHACHA20-POLY1305", EN_CIFT_AEAD},
                {"chacha20-poly1305-ietf", EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF, NULL, "CHACHA20-POLY1305-IETF", EN_CIFT_AEAD},
#ifdef crypto_aead_xchacha20poly1305_ietf_KEYBYTES
                {"xchacha20-poly1305-ietf", EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF, NULL, "XCHACHA20-POLY1305-IETF", EN_CIFT_AEAD},
#endif

#endif

                {NULL, EN_CIMT_INVALID, NULL, NULL, false}, // end
            };

            static const cipher_interface_info_t *get_cipher_interface_by_name(const char *name) {
                if (NULL == name) {
                    return NULL;
                }

                for (size_t i = 0; NULL != details::supported_ciphers[i].name; ++i) {
                    if (0 == UTIL_STRFUNC_STRCASE_CMP(name, details::supported_ciphers[i].name)) {
                        return &details::supported_ciphers[i];
                    }
                }

                return NULL;
            }
        } // namespace details

        cipher::cipher() : interface_(NULL), last_errorno_(0), cipher_kt_(NULL) {}
        cipher::~cipher() { close(); }

        int cipher::init(const char *name, int mode) {
            if (NULL != interface_ && interface_->method != EN_CIMT_INVALID) {
                return details::setup_errorno(*this, -1, error_code_t::ALREADY_INITED);
            }

            if (NULL == name) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            const cipher_interface_info_t *interface = details::get_cipher_interface_by_name(name);
            if (NULL == interface) {
                return details::setup_errorno(*this, -1, error_code_t::CIPHER_NOT_SUPPORT);
            }

            int ret = error_code_t::OK;

            switch (interface->method) {
            case EN_CIMT_XXTEA:
                memset(xxtea_context_.key.data, 0, sizeof(xxtea_context_.key.data));
                break;
            case EN_CIMT_CIPHER:
                ret = init_with_cipher(interface, mode);
                break;
            case EN_CIMT_LIBSODIUM_CHACHA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20:
            case EN_CIMT_LIBSODIUM_SALSA20:
            case EN_CIMT_LIBSODIUM_XSALSA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF:
                memset(libsodium_context_.key, 0, sizeof(libsodium_context_.key));
                break;
            default:
                ret = details::setup_errorno(*this, -1, error_code_t::CIPHER_NOT_SUPPORT);
                break;
            }

            if (error_code_t::OK == ret) {
                interface_ = interface;
            }

            return ret;
        }

        int cipher::init_with_cipher(const cipher_interface_info_t *interface, int mode) {
            if (NULL == interface) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            if (interface->method != EN_CIMT_CIPHER) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            cipher_kt_ = get_cipher_by_name(interface->name);
            if (NULL == cipher_kt_) {
                return details::setup_errorno(*this, -1, error_code_t::CIPHER_NOT_SUPPORT);
            }

            int ret = error_code_t::OK;
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
            do {
                if (mode & mode_t::EN_CMODE_ENCRYPT) {
                    cipher_context_.enc = EVP_CIPHER_CTX_new();

                    if (NULL == cipher_context_.enc) {
                        ret = details::setup_errorno(*this, ERR_peek_error(), error_code_t::MALLOC);
                        break;
                    }
                    if (!(EVP_CipherInit_ex(cipher_context_.enc, cipher_kt_, NULL, NULL, NULL, 1))) {
                        ret = details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                        break;
                    }
                } else {
                    cipher_context_.enc = NULL;
                }

                if (mode & mode_t::EN_CMODE_DECRYPT) {
                    cipher_context_.dec = EVP_CIPHER_CTX_new();

                    if (NULL == cipher_context_.dec) {
                        ret = details::setup_errorno(*this, ERR_peek_error(), error_code_t::MALLOC);
                        break;
                    }

                    if (!(EVP_CipherInit_ex(cipher_context_.dec, cipher_kt_, NULL, NULL, NULL, 0))) {
                        ret = details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                        break;
                    }
                } else {
                    cipher_context_.dec = NULL;
                }

            } while (false);

            if (error_code_t::OK != ret) {
                if ((mode & mode_t::EN_CMODE_ENCRYPT) && NULL != cipher_context_.enc) {
                    EVP_CIPHER_CTX_free(cipher_context_.enc);
                    cipher_context_.enc = NULL;
                }

                if ((mode & mode_t::EN_CMODE_DECRYPT) && NULL != cipher_context_.dec) {
                    EVP_CIPHER_CTX_free(cipher_context_.dec);
                    cipher_context_.dec = NULL;
                }
            }

#elif defined(CRYPTO_USE_MBEDTLS)
            do {
                if (mode & mode_t::EN_CMODE_ENCRYPT) {
                    cipher_context_.enc = (cipher_evp_t *)malloc(sizeof(cipher_evp_t));

                    if (NULL == cipher_context_.enc) {
                        ret = details::setup_errorno(*this, -1, error_code_t::MALLOC);
                        break;
                    }

                    // memset(cipher_context_.enc, 0, sizeof(cipher_evp_t)); // call memset in mbedtls_cipher_init
                    mbedtls_cipher_init(cipher_context_.enc);
                    int res;
                    if ((res = mbedtls_cipher_setup(cipher_context_.enc, cipher_kt_)) != 0) {
                        ret = details::setup_errorno(*this, res, error_code_t::CIPHER_OPERATION);
                        break;
                    }
                } else {
                    cipher_context_.enc = NULL;
                }

                if (mode & mode_t::EN_CMODE_DECRYPT) {
                    cipher_context_.dec = (cipher_evp_t *)malloc(sizeof(cipher_evp_t));

                    if (NULL == cipher_context_.dec) {
                        ret = details::setup_errorno(*this, -1, error_code_t::MALLOC);
                        break;
                    }

                    // memset(cipher_context_.dec, 0, sizeof(cipher_evp_t)); // call memset in mbedtls_cipher_init
                    mbedtls_cipher_init(cipher_context_.dec);
                    int res;
                    if ((res = mbedtls_cipher_setup(cipher_context_.dec, cipher_kt_)) != 0) {
                        ret = details::setup_errorno(*this, res, error_code_t::CIPHER_OPERATION);
                        break;
                    }
                } else {
                    cipher_context_.dec = NULL;
                }

            } while (false);

            if (error_code_t::OK != ret) {
                if ((mode & mode_t::EN_CMODE_ENCRYPT) && NULL != cipher_context_.enc) {
                    mbedtls_cipher_free(cipher_context_.enc);
                    free(cipher_context_.enc);
                    cipher_context_.enc = NULL;
                }

                if ((mode & mode_t::EN_CMODE_DECRYPT) && NULL != cipher_context_.dec) {
                    mbedtls_cipher_free(cipher_context_.dec);
                    free(cipher_context_.dec);
                    cipher_context_.dec = NULL;
                }
            }
#else
            return details::setup_errorno(*this, -1, error_code_t::CIPHER_NOT_SUPPORT);
#endif
            return ret;
        }

        int cipher::close() {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            int ret = error_code_t::OK;
            switch (interface_->method) {
            case EN_CIMT_XXTEA:
                // just do nothing when using xxtea
                ret = details::setup_errorno(*this, 0, error_code_t::OK);
                break;

            case EN_CIMT_CIPHER:
                ret = close_with_cipher();
                break;

            case EN_CIMT_LIBSODIUM_CHACHA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20:
            case EN_CIMT_LIBSODIUM_SALSA20:
            case EN_CIMT_LIBSODIUM_XSALSA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF:
                // just do nothing when using xxtea
                ret = details::setup_errorno(*this, 0, error_code_t::OK);
                break;
            default:
                ret = details::setup_errorno(*this, 0, error_code_t::CIPHER_NOT_SUPPORT);
                break;
            }

            interface_ = NULL;
            return ret;
        }

        int cipher::close_with_cipher() {
            if (NULL == interface_) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            if (interface_->method != EN_CIMT_CIPHER) {
                return details::setup_errorno(*this, 0, error_code_t::INVALID_PARAM);
            }

            // cipher cleanup
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
            if (NULL != cipher_context_.enc) {
                EVP_CIPHER_CTX_free(cipher_context_.enc);
                cipher_context_.enc = NULL;
            }

            if (NULL != cipher_context_.dec) {
                EVP_CIPHER_CTX_free(cipher_context_.dec);
                cipher_context_.dec = NULL;
            }

#elif defined(CRYPTO_USE_MBEDTLS)
            if (NULL != cipher_context_.enc) {
                mbedtls_cipher_free(cipher_context_.enc);
                free(cipher_context_.enc);
                cipher_context_.enc = NULL;
            }

            if (NULL != cipher_context_.dec) {
                mbedtls_cipher_free(cipher_context_.dec);
                free(cipher_context_.dec);
                cipher_context_.dec = NULL;
            }
#endif

            return details::setup_errorno(*this, 0, error_code_t::OK);
        }

        bool cipher::is_aead() const {
            if (NULL == interface_) {
                return false;
            }

            return 0 != (interface_->flags & EN_CIFT_AEAD);
        }

        uint32_t cipher::get_iv_size() const {
            if (NULL == interface_) {
                return 0;
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
            case EN_CIMT_XXTEA:
                return 0;
            case EN_CIMT_CIPHER:
                if (NULL != cipher_context_.enc) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_iv_length(cipher_context_.enc));
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_iv_size(cipher_context_.enc));
#endif
                } else if (NULL != cipher_context_.dec) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_iv_length(cipher_context_.dec));
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_iv_size(cipher_context_.dec));
#endif
                } else {
                    return 0;
                }

#ifdef CRYPTO_USE_LIBSODIUM
            case EN_CIMT_LIBSODIUM_CHACHA20:
                return LIBSODIUM_COUNTER_SIZE + crypto_stream_chacha20_NONCEBYTES;
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
                return LIBSODIUM_COUNTER_SIZE + crypto_stream_chacha20_ietf_NONCEBYTES;

#ifdef crypto_stream_xchacha20_NONCEBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20:
                return LIBSODIUM_COUNTER_SIZE + crypto_stream_xchacha20_NONCEBYTES;
#endif

            case EN_CIMT_LIBSODIUM_SALSA20:
                return LIBSODIUM_COUNTER_SIZE + crypto_stream_salsa20_NONCEBYTES;
            case EN_CIMT_LIBSODIUM_XSALSA20:
                return LIBSODIUM_COUNTER_SIZE + crypto_stream_xsalsa20_NONCEBYTES;
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
                return crypto_aead_chacha20poly1305_NPUBBYTES;
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
                return crypto_aead_chacha20poly1305_IETF_NPUBBYTES;

#ifdef crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF:
                return crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
#endif

#endif

            default:
                return 0;
            }
        }

        uint32_t cipher::get_key_bits() const {
            if (NULL == interface_) {
                return 0;
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return 0;
            case EN_CIMT_XXTEA:
                return sizeof(::util::xxtea_key) * 8;
            case EN_CIMT_CIPHER:
                if (NULL != cipher_context_.enc) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_key_length(cipher_context_.enc) * 8);
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_key_bitlen(cipher_context_.enc));
#endif
                } else if (NULL != cipher_context_.dec) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_key_length(cipher_context_.dec) * 8);
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_key_bitlen(cipher_context_.dec));
#endif
                } else {
                    return 0;
                }

#ifdef CRYPTO_USE_LIBSODIUM
            case EN_CIMT_LIBSODIUM_CHACHA20:
                UTIL_CONFIG_STATIC_ASSERT(crypto_stream_chacha20_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_stream_chacha20_KEYBYTES * 8;
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
                UTIL_CONFIG_STATIC_ASSERT(crypto_stream_chacha20_ietf_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_stream_chacha20_ietf_KEYBYTES * 8;

#ifdef crypto_stream_xchacha20_KEYBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20:
                UTIL_CONFIG_STATIC_ASSERT(crypto_stream_xchacha20_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_stream_xchacha20_KEYBYTES * 8;
#endif

            case EN_CIMT_LIBSODIUM_SALSA20:
                UTIL_CONFIG_STATIC_ASSERT(crypto_stream_salsa20_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_stream_salsa20_KEYBYTES * 8;
            case EN_CIMT_LIBSODIUM_XSALSA20:
                UTIL_CONFIG_STATIC_ASSERT(crypto_stream_xsalsa20_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_stream_xsalsa20_KEYBYTES * 8;
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
                UTIL_CONFIG_STATIC_ASSERT(crypto_aead_chacha20poly1305_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_aead_chacha20poly1305_KEYBYTES * 8;
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
                UTIL_CONFIG_STATIC_ASSERT(crypto_aead_chacha20poly1305_IETF_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_aead_chacha20poly1305_IETF_KEYBYTES * 8;

#ifdef crypto_aead_xchacha20poly1305_ietf_KEYBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF:
                UTIL_CONFIG_STATIC_ASSERT(crypto_aead_xchacha20poly1305_ietf_KEYBYTES <= sizeof(libsodium_context_t));
                return crypto_aead_xchacha20poly1305_ietf_KEYBYTES * 8;
#endif

#endif

            default:
                return 0;
            }
        }

        uint32_t cipher::get_block_size() const {
            if (NULL == interface_) {
                return 0;
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return 0;
            case EN_CIMT_XXTEA:
                return 0x04;
            case EN_CIMT_CIPHER:
                if (NULL != cipher_context_.enc) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_block_size(cipher_context_.enc));
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_block_size(cipher_context_.enc));
#endif
                } else if (NULL != cipher_context_.dec) {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                    return static_cast<uint32_t>(EVP_CIPHER_CTX_block_size(cipher_context_.dec));
#elif defined(CRYPTO_USE_MBEDTLS)
                    return static_cast<uint32_t>(mbedtls_cipher_get_block_size(cipher_context_.dec));
#endif
                } else {
                    return 0;
                }

            case EN_CIMT_LIBSODIUM_CHACHA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20:
            case EN_CIMT_LIBSODIUM_SALSA20:
            case EN_CIMT_LIBSODIUM_XSALSA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF:
                return 1; // all block size of chacha20 and

            default:
                return 0;
            }
        }

        int cipher::set_key(const unsigned char *key, uint32_t key_bitlen) {
            if (NULL == interface_) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            case EN_CIMT_XXTEA: {
                unsigned char secret[4 * sizeof(uint32_t)] = {0};
                if (key_bitlen >= sizeof(secret) * 8) {
                    memcpy(secret, key, sizeof(secret));
                } else {
                    memcpy(secret, key, key_bitlen / 8);
                }
                util::xxtea_setup(&xxtea_context_.key, secret);
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }
            case EN_CIMT_CIPHER: {
                int res = 0;
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                if (get_key_bits() > key_bitlen) {
                    return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
                }

                if (NULL != cipher_context_.enc) {
                    if (!EVP_CipherInit_ex(cipher_context_.enc, NULL, NULL, key, NULL, -1)) {
                        res = (int)ERR_peek_error();
                    }
                }

                if (NULL != cipher_context_.dec) {
                    if (!EVP_CipherInit_ex(cipher_context_.dec, NULL, NULL, key, NULL, -1)) {
                        res = (int)ERR_peek_error();
                    }
                }

#elif defined(CRYPTO_USE_MBEDTLS)
                if (NULL != cipher_context_.enc) {
                    res = mbedtls_cipher_setkey(cipher_context_.enc, key, static_cast<int>(key_bitlen), MBEDTLS_ENCRYPT);
                }

                if (NULL != cipher_context_.dec) {
                    res = mbedtls_cipher_setkey(cipher_context_.dec, key, static_cast<int>(key_bitlen), MBEDTLS_DECRYPT);
                }
#endif
                if (res != 0) {
                    return details::setup_errorno(*this, res, error_code_t::CIPHER_OPERATION);
                }
                return details::setup_errorno(*this, res, error_code_t::OK);
            }

            case EN_CIMT_LIBSODIUM_CHACHA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20:
            case EN_CIMT_LIBSODIUM_SALSA20:
            case EN_CIMT_LIBSODIUM_XSALSA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF: {
                if (key_bitlen >= sizeof(libsodium_context_.key) * 8) {
                    memcpy(libsodium_context_.key, key, sizeof(libsodium_context_.key));
                } else {
                    memcpy(libsodium_context_.key, key, key_bitlen / 8);
                }
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }
            default:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            }
        }

        int cipher::set_iv(const unsigned char *iv, size_t iv_len) {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
            case EN_CIMT_XXTEA:
                return error_code_t::OK;

            case EN_CIMT_CIPHER: {
#if defined(CRYPTO_USE_MBEDTLS)
                if (iv_len > MBEDTLS_MAX_IV_LENGTH) {
                    return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
                }
#endif
                int res = 0;
                if (0 == (interface_->flags & EN_CIFT_VARIABLE_IV_LEN)) {
                    if (get_iv_size() != iv_len) {
                        return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
                    }
                }

                iv_.assign(iv, iv + iv_len);
                return details::setup_errorno(*this, res, error_code_t::OK);
            }

            case EN_CIMT_LIBSODIUM_CHACHA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20:
            case EN_CIMT_LIBSODIUM_SALSA20:
            case EN_CIMT_LIBSODIUM_XSALSA20:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305:
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF:
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF: {
                if (get_iv_size() != iv_len) {
                    return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
                }

                iv_.assign(iv, iv + iv_len);
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }

            default:
                return error_code_t::OK;
            }
        }

        void cipher::clear_iv() { iv_.clear(); }

        int cipher::encrypt(const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen) {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            if (is_aead()) {
                return error_code_t::MUST_CALL_AEAD_API;
            }

            if (input == NULL || ilen <= 0 || output == NULL || NULL == olen || *olen <= 0 || *olen < ilen + get_block_size()) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            if (interface_->method >= EN_CIMT_CIPHER && 0 == (interface_->flags & EN_CIFT_VARIABLE_IV_LEN) && iv_.size() < get_iv_size()) {
                if (0 != get_iv_size()) {
                    iv_.resize(get_iv_size(), 0);
                }
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            case EN_CIMT_XXTEA: {
                util::xxtea_encrypt(&xxtea_context_.key, reinterpret_cast<const void *>(input), ilen, reinterpret_cast<void *>(output),
                                    olen);
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }
            case EN_CIMT_CIPHER: {
                if (NULL == cipher_context_.enc) {
                    return details::setup_errorno(*this, 0, error_code_t::CIPHER_DISABLED);
                }

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                int outl, finish_olen;

                if (!iv_.empty()) {
                    if (!EVP_CipherInit_ex(cipher_context_.enc, NULL, NULL, NULL, &iv_[0], -1)) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_ENCRYPT_NO_PADDING)) {
                    EVP_CIPHER_CTX_set_padding(cipher_context_.enc, 0);
                }

                if (!(EVP_CipherUpdate(cipher_context_.enc, output, &outl, input, static_cast<int>(ilen)))) {
                    return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                }

                if (0 != (interface_->flags & EN_CIFT_NO_FINISH)) {
                    finish_olen = 0;
                } else {
                    if (!(EVP_CipherFinal_ex(cipher_context_.enc, output + outl, &finish_olen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                *olen = static_cast<size_t>(outl + finish_olen);
                return error_code_t::OK;

#elif defined(CRYPTO_USE_MBEDTLS)
                if (0 != (interface_->flags & EN_CIFT_ENCRYPT_NO_PADDING) && MBEDTLS_MODE_CBC == cipher_context_.enc->cipher_info->mode) {
                    if ((last_errorno_ = mbedtls_cipher_set_padding_mode(cipher_context_.enc, MBEDTLS_PADDING_NONE)) != 0) {
                        return error_code_t::CIPHER_OPERATION;
                    }
                }

                unsigned char empty_iv[MBEDTLS_MAX_IV_LENGTH] = {0};
                if ((last_errorno_ = mbedtls_cipher_crypt(cipher_context_.enc, iv_.empty() ? empty_iv : &iv_[0], iv_.size(), input, ilen,
                                                          output, olen)) != 0) {
                    return error_code_t::CIPHER_OPERATION;
                }
                return error_code_t::OK;
#endif
            }

#ifdef CRYPTO_USE_LIBSODIUM
            case EN_CIMT_LIBSODIUM_CHACHA20:
                if ((last_errorno_ = crypto_stream_chacha20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                   libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
                if ((last_errorno_ = crypto_stream_chacha20_ietf_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                        libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;

#ifdef crypto_stream_xchacha20_KEYBYTES
                if ((last_errorno_ = crypto_stream_xchacha20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                    libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
#endif

            case EN_CIMT_LIBSODIUM_SALSA20:
                if ((last_errorno_ = crypto_stream_salsa20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                  libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            case EN_CIMT_LIBSODIUM_XSALSA20:
                if ((last_errorno_ = crypto_stream_xsalsa20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                   libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
#endif
            default:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            }
        }

        int cipher::decrypt(const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen) {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            if (is_aead()) {
                return error_code_t::MUST_CALL_AEAD_API;
            }

            if (input == NULL || ilen <= 0 || output == NULL || NULL == olen || *olen <= 0 || *olen < ilen + get_block_size()) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            if (interface_->method >= EN_CIMT_CIPHER && 0 == (interface_->flags & EN_CIFT_VARIABLE_IV_LEN) && iv_.size() < get_iv_size()) {
                if (0 != get_iv_size()) {
                    iv_.resize(get_iv_size(), 0);
                }
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            case EN_CIMT_XXTEA: {
                util::xxtea_decrypt(&xxtea_context_.key, reinterpret_cast<const void *>(input), ilen, reinterpret_cast<void *>(output),
                                    olen);
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }
            case EN_CIMT_CIPHER: {
                if (NULL == cipher_context_.dec) {
                    return details::setup_errorno(*this, 0, error_code_t::CIPHER_DISABLED);
                }

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                int outl, finish_olen;

                if (!iv_.empty()) {
                    if (!EVP_CipherInit_ex(cipher_context_.dec, NULL, NULL, NULL, &iv_[0], -1)) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_DECRYPT_NO_PADDING)) {
                    EVP_CIPHER_CTX_set_padding(cipher_context_.dec, 0);
                }

                if (!(EVP_CipherUpdate(cipher_context_.dec, output, &outl, input, static_cast<int>(ilen)))) {
                    return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                }

                if (0 != (interface_->flags & EN_CIFT_NO_FINISH)) {
                    finish_olen = 0;
                } else {
                    if (!(EVP_CipherFinal_ex(cipher_context_.dec, output + outl, &finish_olen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                *olen = static_cast<size_t>(outl + finish_olen);

                return error_code_t::OK;
#elif defined(CRYPTO_USE_MBEDTLS)
                if (0 != (interface_->flags & EN_CIFT_DECRYPT_NO_PADDING) && MBEDTLS_MODE_CBC == cipher_context_.dec->cipher_info->mode) {
                    if ((last_errorno_ = mbedtls_cipher_set_padding_mode(cipher_context_.dec, MBEDTLS_PADDING_NONE)) != 0) {
                        return error_code_t::CIPHER_OPERATION;
                    }
                }

                unsigned char empty_iv[MBEDTLS_MAX_IV_LENGTH] = {0};
                if ((last_errorno_ = mbedtls_cipher_crypt(cipher_context_.dec, iv_.empty() ? empty_iv : &iv_[0], iv_.size(), input, ilen,
                                                          output, olen)) != 0) {
                    return error_code_t::CIPHER_OPERATION;
                }
                return error_code_t::OK;
#endif
            }

#ifdef CRYPTO_USE_LIBSODIUM
            case EN_CIMT_LIBSODIUM_CHACHA20:
                if ((last_errorno_ = crypto_stream_chacha20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                   libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            case EN_CIMT_LIBSODIUM_CHACHA20_IETF:
                if ((last_errorno_ = crypto_stream_chacha20_ietf_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                        libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;

#ifdef crypto_stream_xchacha20_KEYBYTES
                if ((last_errorno_ = crypto_stream_xchacha20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                    libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
#endif

            case EN_CIMT_LIBSODIUM_SALSA20:
                if ((last_errorno_ = crypto_stream_salsa20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                  libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            case EN_CIMT_LIBSODIUM_XSALSA20:
                if ((last_errorno_ = crypto_stream_xsalsa20_xor_ic(output, input, ilen, &iv_[LIBSODIUM_COUNTER_SIZE],
                                                                   libsodium_get_counter(&iv_[0]), libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
#endif

            default:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            }
        }

        int cipher::encrypt_aead(const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen, const unsigned char *ad,
                                 size_t ad_len, unsigned char *tag, size_t tag_len) {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            if (!is_aead()) {
                return error_code_t::MUST_NOT_CALL_AEAD_API;
            }

            if (input == NULL || ilen <= 0 || output == NULL || NULL == olen || *olen <= 0 || *olen < ilen + get_block_size()) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            if (interface_->method >= EN_CIMT_CIPHER && 0 == (interface_->flags & EN_CIFT_VARIABLE_IV_LEN) && iv_.size() < get_iv_size()) {
                if (0 != get_iv_size()) {
                    iv_.resize(get_iv_size(), 0);
                }
            }

            switch (interface_->method) {
            case EN_CIMT_CIPHER: {
                if (NULL == cipher_context_.enc) {
                    return details::setup_errorno(*this, 0, error_code_t::CIPHER_DISABLED);
                }

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                int outl, finish_olen;

                if (!iv_.empty()) {
                    if (0 != (interface_->flags & EN_CIFT_VARIABLE_IV_LEN)) {
                        if (!EVP_CIPHER_CTX_ctrl(cipher_context_.enc, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv_.size()), 0)) {
                            return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                        }
                    }

                    if (!EVP_CipherInit_ex(cipher_context_.enc, NULL, NULL, NULL, &iv_[0], -1)) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_AEAD_SET_LENGTH_BEFORE)) {
                    int tmplen;
                    if (!EVP_CipherUpdate(cipher_context_.enc, NULL, &tmplen, NULL, static_cast<int>(ilen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                int chunklen = 0;
                if (NULL != ad && ad_len > 0) {
                    if (!EVP_CipherUpdate(cipher_context_.enc, NULL, &chunklen, ad, static_cast<int>(ad_len))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_ENCRYPT_NO_PADDING)) {
                    EVP_CIPHER_CTX_set_padding(cipher_context_.enc, 0);
                }

                if (!(EVP_CipherUpdate(cipher_context_.enc, output, &outl, input, static_cast<int>(ilen)))) {
                    return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                }

                if (0 != (interface_->flags & EN_CIFT_NO_FINISH)) {
                    finish_olen = 0;
                } else {
                    if (!(EVP_CipherFinal_ex(cipher_context_.enc, output + outl, &finish_olen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                *olen = static_cast<size_t>(outl + finish_olen);

                if (NULL != tag && tag_len > 0) {
                    if (!EVP_CIPHER_CTX_ctrl(cipher_context_.enc, EVP_CTRL_AEAD_GET_TAG, static_cast<int>(tag_len), tag)) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                return error_code_t::OK;

#elif defined(CRYPTO_USE_MBEDTLS)
                // if (0 != (interface_->flags & EN_CIFT_ENCRYPT_NO_PADDING) && MBEDTLS_MODE_CBC == cipher_context_.enc->cipher_info->mode)
                // {
                //     if ((last_errorno_ = mbedtls_cipher_set_padding_mode(cipher_context_.enc, MBEDTLS_PADDING_NONE)) != 0) {
                //         return error_code_t::CIPHER_OPERATION;
                //     }
                // }

                unsigned char empty_iv[MBEDTLS_MAX_IV_LENGTH] = {0};
                if ((last_errorno_ = mbedtls_cipher_auth_encrypt(cipher_context_.enc, iv_.empty() ? empty_iv : &iv_[0], iv_.size(), ad,
                                                                 ad_len, input, ilen, output, olen, tag, tag_len)) != 0) {
                    return error_code_t::CIPHER_OPERATION;
                }
                return error_code_t::OK;
#endif
            }

#ifdef CRYPTO_USE_LIBSODIUM

            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305: {
                if (crypto_aead_chacha20poly1305_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                unsigned long long maclen = tag_len;
                if ((last_errorno_ = crypto_aead_chacha20poly1305_encrypt_detached(output, tag, &maclen, input, ilen, ad, ad_len, NULL,
                                                                                   &iv_[0], libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            }
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF: {
                if (crypto_aead_chacha20poly1305_IETF_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                unsigned long long maclen = tag_len;
                if ((last_errorno_ = crypto_aead_chacha20poly1305_ietf_encrypt_detached(output, tag, &maclen, input, ilen, ad, ad_len, NULL,
                                                                                        &iv_[0], libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            }

#ifdef crypto_aead_xchacha20poly1305_ietf_KEYBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF: {
                if (crypto_aead_xchacha20poly1305_ietf_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                unsigned long long maclen = tag_len;
                if ((last_errorno_ = crypto_aead_xchacha20poly1305_ietf_encrypt_detached(output, tag, &maclen, input, ilen, ad, ad_len,
                                                                                         NULL, &iv_[0], libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            }
#endif

#endif

            default:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            }
        }

        int cipher::decrypt_aead(const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen, const unsigned char *ad,
                                 size_t ad_len, const unsigned char *tag, size_t tag_len) {
            if (NULL == interface_ || interface_->method == EN_CIMT_INVALID) {
                return details::setup_errorno(*this, 0, error_code_t::NOT_INITED);
            }

            if (!is_aead()) {
                return error_code_t::MUST_NOT_CALL_AEAD_API;
            }

            if (input == NULL || ilen <= 0 || output == NULL || NULL == olen || *olen <= 0 || *olen < ilen + get_block_size()) {
                return details::setup_errorno(*this, -1, error_code_t::INVALID_PARAM);
            }

            if (interface_->method >= EN_CIMT_CIPHER && 0 == (interface_->flags & EN_CIFT_VARIABLE_IV_LEN) && iv_.size() < get_iv_size()) {
                if (0 != get_iv_size()) {
                    iv_.resize(get_iv_size(), 0);
                }
            }

            switch (interface_->method) {
            case EN_CIMT_INVALID:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            case EN_CIMT_XXTEA: {
                util::xxtea_decrypt(&xxtea_context_.key, reinterpret_cast<const void *>(input), ilen, reinterpret_cast<void *>(output),
                                    olen);
                return details::setup_errorno(*this, 0, error_code_t::OK);
            }
            case EN_CIMT_CIPHER: {
                if (NULL == cipher_context_.dec) {
                    return details::setup_errorno(*this, 0, error_code_t::CIPHER_DISABLED);
                }

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
                int outl, finish_olen;

                if (!iv_.empty()) {
                    if (0 != (interface_->flags & EN_CIFT_VARIABLE_IV_LEN)) {
                        if (!EVP_CIPHER_CTX_ctrl(cipher_context_.dec, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(iv_.size()), 0)) {
                            return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                        }
                    }

                    if (!EVP_CipherInit_ex(cipher_context_.dec, NULL, NULL, NULL, &iv_[0], -1)) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION_SET_IV);
                    }
                }

                if (NULL != tag && tag_len > 0) {
                    if (!(EVP_CIPHER_CTX_ctrl(cipher_context_.dec, EVP_CTRL_AEAD_SET_TAG, static_cast<int>(tag_len),
                                              const_cast<unsigned char *>(tag)))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_AEAD_SET_LENGTH_BEFORE)) {
                    int tmplen;
                    if (!EVP_CipherUpdate(cipher_context_.dec, NULL, &tmplen, NULL, static_cast<int>(ilen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                int chunklen = 0;
                if (NULL != ad && ad_len > 0) {
                    if (!EVP_CipherUpdate(cipher_context_.dec, NULL, &chunklen, ad, static_cast<int>(ad_len))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                if (0 != (interface_->flags & EN_CIFT_DECRYPT_NO_PADDING)) {
                    EVP_CIPHER_CTX_set_padding(cipher_context_.dec, 0);
                }

                if (!(EVP_CipherUpdate(cipher_context_.dec, output, &outl, input, static_cast<int>(ilen)))) {
                    return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                }

                if (0 != (interface_->flags & EN_CIFT_NO_FINISH)) {
                    finish_olen = 0;
                } else {
                    if (!(EVP_CipherFinal_ex(cipher_context_.dec, output + outl, &finish_olen))) {
                        return details::setup_errorno(*this, ERR_peek_error(), error_code_t::CIPHER_OPERATION);
                    }
                }

                *olen = static_cast<size_t>(outl + finish_olen);

                return error_code_t::OK;
#elif defined(CRYPTO_USE_MBEDTLS)
                // if (0 != (interface_->flags & EN_CIFT_DECRYPT_NO_PADDING) && MBEDTLS_MODE_CBC == cipher_context_.dec->cipher_info->mode)
                // {
                //     if ((last_errorno_ = mbedtls_cipher_set_padding_mode(cipher_context_.dec, MBEDTLS_PADDING_NONE)) != 0) {
                //         return error_code_t::CIPHER_OPERATION;
                //     }
                // }

                unsigned char empty_iv[MBEDTLS_MAX_IV_LENGTH] = {0};
                if ((last_errorno_ = mbedtls_cipher_auth_decrypt(cipher_context_.dec, iv_.empty() ? empty_iv : &iv_[0], iv_.size(), ad,
                                                                 ad_len, input, ilen, output, olen, tag, tag_len)) != 0) {
                    return error_code_t::CIPHER_OPERATION;
                }
                return error_code_t::OK;
#endif
            }

#ifdef CRYPTO_USE_LIBSODIUM

            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305: {
                if (crypto_aead_chacha20poly1305_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                if ((last_errorno_ = crypto_aead_chacha20poly1305_decrypt_detached(output, NULL, input, ilen, tag, ad, ad_len, &iv_[0],
                                                                                   libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            }
            case EN_CIMT_LIBSODIUM_CHACHA20_POLY1305_IETF: {
                if (crypto_aead_chacha20poly1305_IETF_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                if ((last_errorno_ = crypto_aead_chacha20poly1305_ietf_decrypt_detached(output, NULL, input, ilen, tag, ad, ad_len, &iv_[0],
                                                                                        libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }

                return error_code_t::OK;
            }

#ifdef crypto_aead_xchacha20poly1305_ietf_KEYBYTES
            case EN_CIMT_LIBSODIUM_XCHACHA20_POLY1305_IETF: {
                if (crypto_aead_xchacha20poly1305_ietf_ABYTES > tag_len) {
                    return error_code_t::LIBSODIUM_OPERATION_TAG_LEN;
                }

                if ((last_errorno_ = crypto_aead_xchacha20poly1305_ietf_decrypt_detached(output, NULL, input, ilen, tag, ad, ad_len,
                                                                                         &iv_[0], libsodium_context_.key)) != 0) {
                    return error_code_t::LIBSODIUM_OPERATION;
                }
                return error_code_t::OK;
            }
#endif

#endif
            default:
                return details::setup_errorno(*this, -1, error_code_t::NOT_INITED);
            }
        }

        const cipher::cipher_kt_t *cipher::get_cipher_by_name(const char *name) {
            const cipher_interface_info_t *interface = details::get_cipher_interface_by_name(name);

            if (NULL == interface) {
                return NULL;
            }

#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
            if (NULL == interface->openssl_name) {
                return EVP_get_cipherbyname(interface->name);
            } else {
                return EVP_get_cipherbyname(interface->openssl_name);
            }
#elif defined(CRYPTO_USE_MBEDTLS)
            return mbedtls_cipher_info_from_string(interface->mbedtls_name);
#endif
        }

        std::pair<const char *, const char *> cipher::ciphertok(const char *in) {
            std::pair<const char *, const char *> ret;
            ret.first  = NULL;
            ret.second = NULL;
            if (NULL == in) {
                return ret;
            }

            const char *b = in;
            const char *e;
            // skip \r\n\t and space
            while (0 != *b && (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n' || *b == ';' || *b == ',' || *b == ':')) {
                ++b;
            }

            for (e = b; 0 != *e; ++e) {
                if (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n' || *e == ';' || *e == ',' || *e == ':') {
                    break;
                }
            }

            if (e <= b) {
                return ret;
            }

            ret.first  = b;
            ret.second = e;
            return ret;
        }

        const std::vector<std::string> &cipher::get_all_cipher_names() {
            static std::vector<std::string> ret;
            if (ret.empty()) {
                for (size_t i = 0; NULL != details::supported_ciphers[i].name; ++i) {
                    if (details::supported_ciphers[i].method < EN_CIMT_INNER) {
                        ret.push_back(details::supported_ciphers[i].name);
                        continue;
                    }

                    if (details::supported_ciphers[i].method == EN_CIMT_CIPHER) {
                        if (NULL != get_cipher_by_name(details::supported_ciphers[i].name)) {
                            ret.push_back(details::supported_ciphers[i].name);
                        }
                        continue;
                    }

                    if (details::supported_ciphers[i].method > EN_CIMT_LIBSODIUM) {
                        ret.push_back(details::supported_ciphers[i].name);
                    }
                }
            }

            return ret;
        }

        int cipher::init_global_algorithm() {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
            OpenSSL_add_all_algorithms();
#endif
            return 0;
        }

        int cipher::cleanup_global_algorithm() {
#if defined(CRYPTO_USE_OPENSSL) || defined(CRYPTO_USE_LIBRESSL) || defined(CRYPTO_USE_BORINGSSL)
            EVP_cleanup();
#endif
            return 0;
        }
    } // namespace crypto
} // namespace util

#endif