#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#ifndef DISABLE_NETWORK

#include <vector>
#include <string>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "../core/IStream.hpp"
#include "../Diagnostic.h"
#include "NetworkKey.h"

#define KEY_TYPE EVP_PKEY_RSA

constexpr sint32 KEY_LENGTH_BITS = 2048;

NetworkKey::NetworkKey()
{
    _ctx = EVP_PKEY_CTX_new_id(KEY_TYPE, nullptr);
    if (_ctx == nullptr)
    {
        log_error("Failed to create OpenSSL context");
    }
}

NetworkKey::~NetworkKey()
{
    Unload();
    if (_ctx != nullptr)
    {
        EVP_PKEY_CTX_free(_ctx);
        _ctx = nullptr;
    }
}

void NetworkKey::Unload()
{
    if (_key != nullptr)
    {
        EVP_PKEY_free(_key);
        _key = nullptr;
    }
}

bool NetworkKey::Generate()
{
    if (_ctx == nullptr)
    {
        log_error("Invalid OpenSSL context");
        return false;
    }
#if KEY_TYPE == EVP_PKEY_RSA
    if (!EVP_PKEY_CTX_set_rsa_keygen_bits(_ctx, KEY_LENGTH_BITS))
    {
        log_error("Failed to set keygen params");
        return false;
    }
#else
    #error Only RSA is supported!
#endif
    if (EVP_PKEY_keygen_init(_ctx) <= 0)
    {
        log_error("Failed to initialise keygen algorithm");
        return false;
    }
    if (EVP_PKEY_keygen(_ctx, &_key) <= 0)
    {
        log_error("Failed to generate new key!");
        return false;
    }
    else
    {
        log_verbose("Key successfully generated");
    }
    log_verbose("New key of type %d, length %d generated successfully.", KEY_TYPE, KEY_LENGTH_BITS);
    return true;
}

bool NetworkKey::LoadPrivate(IStream *stream)
{
    Guard::ArgumentNotNull(stream);
    long ssize = stream->GetLength();
    if (ssize < 0) {
        log_error("unknown size, refusing to load key");
        return false;
    }
    if (ssize > 4 * 1024 * 1024) {
        log_error("Key file suspiciously large, refusing to load it");
        return false;
    }
    std::vector<unsigned char> buf(static_cast<size_t>(ssize));
    stream->Read(buf.data(), ssize);
    BIO *bio = BIO_new_mem_buf(buf.data(), static_cast<int>(ssize));
    if (!bio) {
        log_error("Failed to create BIO");
        return false;
    }
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    if (!pkey) {
        BIO_reset(bio);
        pkey = d2i_PrivateKey_bio(bio, nullptr);
    }
    BIO_free(bio);
    if (!pkey) {
        unsigned long err = ERR_get_error();
        char msg[256];
        ERR_error_string_n(err, msg, sizeof(msg));
        log_error("OpenSSL failed to parse private key: %s", msg);
        return false;
    }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!pctx || EVP_PKEY_check(pctx) <= 0) {
        log_error("Invalid private key");
        if (pctx) EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    EVP_PKEY_CTX_free(pctx);
#else
    if (EVP_PKEY_id(pkey) == EVP_PKEY_RSA) {
        RSA *rsa = EVP_PKEY_get1_RSA(pkey);
        if (!rsa || RSA_check_key(rsa) != 1) {
            log_error("Invalid private RSA key");
            EVP_PKEY_free(pkey);
            return false;
        }
    }
#endif
    if (_key)
        EVP_PKEY_free(_key);
    _key = pkey;
    return true;
}


bool NetworkKey::LoadPublic(IStream *stream)
{
    Guard::ArgumentNotNull(stream);
    long ssize = stream->GetLength();
    if (ssize < 0)
    {
        log_error("unknown size, refusing to load key");
        return false;
    }
    if (ssize > 4 * 1024 * 1024)
    {
        log_error("Key file suspiciously large, refusing to load it");
        return false;
    }
    std::vector<unsigned char> buf(static_cast<size_t>(ssize));
    stream->Read(buf.data(), ssize);
    BIO *bio = BIO_new_mem_buf(buf.data(), static_cast<int>(ssize));
    if (!bio)
    {
        log_error("Failed to create BIO");
        return false;
    }
    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    if (!pkey)
    {
        BIO_reset(bio);
        pkey = d2i_PUBKEY_bio(bio, nullptr);
    }
    BIO_free(bio);
    if (!pkey)
    {
        unsigned long err = ERR_get_error();
        char msg[256];
        ERR_error_string_n(err, msg, sizeof(msg));
        log_error("OpenSSL failed to parse public key: %s", msg);
        return false;
    }
    if (_key)
        EVP_PKEY_free(_key);
    _key = pkey;
    return true;
}

bool NetworkKey::SavePrivate(IStream *stream)
{
    if (!_key)
    {
        log_error("No key loaded");
        return false;
    }
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
    {
        log_error("Failed to create BIO");
        return false;
    }
    if (PEM_write_bio_PrivateKey(bio, _key, nullptr, nullptr, 0, nullptr, nullptr) != 1)
    {
        log_error("failed to write private key!");
        BIO_free_all(bio);
        return false;
    }
    int len = BIO_pending(bio);
    std::vector<char> data(len);
    BIO_read(bio, data.data(), len);
    BIO_free_all(bio);
    stream->Write(data.data(), len);
    return true;
}

bool NetworkKey::SavePublic(IStream *stream)
{
    if (!_key)
    {
        log_error("No key loaded");
        return false;
    }
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
    {
        log_error("Failed to create BIO");
        return false;
    }
    if (PEM_write_bio_PUBKEY(bio, _key) != 1)
    {
        log_error("failed to write public key!");
        BIO_free_all(bio);
        return false;
    }
    int len = BIO_pending(bio);
    std::vector<char> data(len);
    BIO_read(bio, data.data(), len);
    BIO_free_all(bio);
    stream->Write(data.data(), len);
    return true;
}

std::string NetworkKey::PublicKeyString()
{
    if (!_key)
    {
        log_error("No key loaded");
        return {};
    }
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
    {
        log_error("Failed to create BIO");
        return {};
    }
    if (PEM_write_bio_PUBKEY(bio, _key) != 1)
    {
        log_error("failed to write public key!");
        BIO_free_all(bio);
        return {};
    }
    int len = BIO_pending(bio);
    std::string s(len, '\0');
    BIO_read(bio, &s[0], len);
    BIO_free_all(bio);
    return s;
}


/**
 * @brief NetworkKey::PublicKeyHash
 * Computes a short, human-readable (e.g. asciif-ied hex) hash for a given
 * public key. Serves a purpose of easy identification keys in multiplayer
 * overview, multiplayer settings.
 *
 * In particular, any of digest functions applied to a standardised key
 * representation, like PEM, will be sufficient.
 *
 * @return returns a string containing key hash.
 */
std::string NetworkKey::PublicKeyHash()
{
    std::string key = PublicKeyString();
    if (key.empty())
    {
        log_error("No key found");
        return nullptr;
    }
    EVP_MD_CTX * ctx = EVP_MD_CTX_create();
    if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) <= 0)
    {
        log_error("Failed to initialise digest context");
        EVP_MD_CTX_destroy(ctx);
        return nullptr;
    }
    if (EVP_DigestUpdate(ctx, key.c_str(), key.size()) <= 0)
    {
        log_error("Failed to update digset");
        EVP_MD_CTX_destroy(ctx);
        return nullptr;
    }
    uint32 digest_size = EVP_MAX_MD_SIZE;
    std::vector<uint8> digest(EVP_MAX_MD_SIZE);
    // Cleans up `ctx` automatically.
    EVP_DigestFinal(ctx, digest.data(), &digest_size);
    std::string digest_out;
    digest_out.reserve(EVP_MAX_MD_SIZE * 2 + 1);
    for (uint32 i = 0; i < digest_size; i++)
    {
        char buf[3];
        snprintf(buf, 3, "%02x", digest[i]);
        digest_out.append(buf);
    }
    return digest_out;
}

bool NetworkKey::Sign(const uint8 * md, const size_t len, char ** signature, size_t * out_size)
{
    EVP_MD_CTX * mdctx = nullptr;

    *signature = nullptr;

    /* Create the Message Digest Context */
    if ((mdctx = EVP_MD_CTX_create()) == nullptr)
    {
        log_error("Failed to create MD context");
        return false;
    }
    /* Initialise the DigestSign operation - SHA-256 has been selected as the message digest function in this example */
    if (1 != EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, _key))
    {
        log_error("Failed to init digest sign");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    /* Call update with the message */
    if (1 != EVP_DigestSignUpdate(mdctx, md, len))
    {
        log_error("Failed to goto update digest");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }

    /* Finalise the DigestSign operation */
    /* First call EVP_DigestSignFinal with a nullptr sig parameter to obtain the length of the
     * signature. Length is returned in slen */
    if (1 != EVP_DigestSignFinal(mdctx, nullptr, out_size))
    {
        log_error("failed to finalise signature");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }

    uint8 * sig;
    /* Allocate memory for the signature based on size in slen */
    if ((sig = (unsigned char*)malloc((sint32)(sizeof(unsigned char) * (*out_size)))) == nullptr)
    {
        log_error("Failed to crypto-allocate space for signature");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    /* Obtain the signature */
    if (1 != EVP_DigestSignFinal(mdctx, sig, out_size)) {
        log_error("Failed to finalise signature");
        EVP_MD_CTX_destroy(mdctx);
        free(sig);
        return false;
    }
    *signature = new char[*out_size];
    memcpy(*signature, sig, *out_size);
    free(sig);
    EVP_MD_CTX_destroy(mdctx);

    return true;
}

bool NetworkKey::Verify(const uint8 * md, const size_t len, const char * sig, const size_t siglen)
{
    EVP_MD_CTX * mdctx = nullptr;

    /* Create the Message Digest Context */
    if ((mdctx = EVP_MD_CTX_create()) == nullptr)
    {
        log_error("Failed to create MD context");
        return false;
    }

    if (1 != EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, _key))
    {
        log_error("Failed to initialise verification routine");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }

    /* Initialize `key` with a public key */
    if (1 != EVP_DigestVerifyUpdate(mdctx, md, len))
    {
        log_error("Failed to update verification");
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }

    if (1 == EVP_DigestVerifyFinal(mdctx, (uint8 *)sig, siglen))
    {
        EVP_MD_CTX_destroy(mdctx);
        log_verbose("Successfully verified signature");
        return true;
    }
    else
    {
        EVP_MD_CTX_destroy(mdctx);
        log_error("Signature is invalid");
        return false;
    }
}

#endif // DISABLE_NETWORK
