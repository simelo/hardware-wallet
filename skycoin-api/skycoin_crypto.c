/*
 * This file is part of the Skycoin project, https://skycoin.net/
 *
 * Copyright (C) 2018-2019 Skycoin Project
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "skycoin_crypto.h"

#include <stdio.h> //sprintf
#include <string.h>

#include "skycoin_constants.h"
#include "skycoin_signature.h"
#include "base58.h"
#include "secp256k1.h"
#include "curves.h"
#include "ecdsa.h"
#include "ripemd160.h"
#include "sha2.h"

extern void bn_print(const bignum256* a);

void tohex(char* str, const uint8_t* buffer, int buffer_length)
{
    int i;
    for (i = 0; i < buffer_length; ++i) {
        sprintf(&str[2 * i], "%02x", buffer[i]);
    }
}

void tobuff(const char* str, uint8_t* buf, size_t buffer_length)
{
    for (size_t i = 0; i < buffer_length; i++) {
        uint8_t c = 0;
        if (str[i * 2] >= '0' && str[i * 2] <= '9') c += (str[i * 2] - '0') << 4;
        if ((str[i * 2] & ~0x20) >= 'A' && (str[i * 2] & ~0x20) <= 'F') c += (10 + (str[i * 2] & ~0x20) - 'A') << 4;
        if (str[i * 2 + 1] >= '0' && str[i * 2 + 1] <= '9') c += (str[i * 2 + 1] - '0');
        if ((str[i * 2 + 1] & ~0x20) >= 'A' && (str[i * 2 + 1] & ~0x20) <= 'F') c += (10 + (str[i * 2 + 1] & ~0x20) - 'A');
        buf[i] = c;
    }
}

void writebuf_fromhexstr(const char* str, uint8_t* buf)
{
    size_t len = strlen(str) / 2;
    if (len > 32) len = 32;
    tobuff(str, buf, len);
}

void skycoin_pubkey_from_seckey(const uint8_t* seckey, uint8_t* pubkey)
{
    /*
    SKYCOIN CIPHER AUDIT
    Compare to function: secp256k1.SkycoinPubkeyFromSeckey
    */
    const curve_info* curve = get_curve_by_name(SECP256K1_NAME);
    ecdsa_get_public_key33(curve->params, seckey, pubkey);
}

void deterministic_key_pair_iterator_step(const uint8_t* seed, const size_t seed_length, uint8_t* seckey, uint8_t* pubkey)
{
    /*
    SKYCOIN CIPHER AUDIT
    Compare to function: secp256k1.GenerateDeterministicKeyPair
    Note: Does not conform to secp256k1.GenerateDeterministicKeyPair
        - Needs to check secret key for validity
        - Needs to have retry logic to brute force a valid secret and public key
    */
    sha256sum(seed, seckey, seed_length);
    skycoin_pubkey_from_seckey(seckey, pubkey);
}

/*
secret_key: 32 bytes
remote_public_key: SKYCOIN_PUBKEY_LEN bytes (compressed public key)
ecdh_key: SKYCOIN_PUBKEY_LEN bytes (compressed public key)

Caller should verify that the ecdh_key is a valid pubkey
*/
void ecdh(const uint8_t* pub_key, const uint8_t* sec_key, uint8_t* ecdh_key)
{
    uint8_t long_pub_key[65] = {0};
    const curve_info* curve = get_curve_by_name(SECP256K1_NAME);
    ecdh_multiply(curve->params, sec_key, pub_key, long_pub_key);
    compress_pubkey(long_pub_key, ecdh_key);
}

void secp256k1sum(const uint8_t* seed, const size_t seed_length, uint8_t* digest)
{
    /*
    SKYCOIN CIPHER AUDIT
    Compare to function: secp256k1.Secp256k1Hash
    */
    uint8_t seckey[SKYCOIN_SECKEY_LEN] = {0};                           // generateKey(sha256(seed))
    uint8_t dummy_seckey[SKYCOIN_SECKEY_LEN] = {0};
    uint8_t pubkey[SKYCOIN_PUBKEY_LEN] = {0};                           // generateKey(sha256(sha256(seed))
    uint8_t hash[SHA256_DIGEST_LENGTH] = {0};                           // sha256(seed)
    uint8_t hash2[SHA256_DIGEST_LENGTH] = {0};                          // sha256(sha256(seed))
    uint8_t ecdh_key[SKYCOIN_PUBKEY_LEN] = {0};                         // ecdh(pubkey, seckey)
    uint8_t hash_ecdh[SHA256_DIGEST_LENGTH + SKYCOIN_PUBKEY_LEN] = {0}; // sha256(sha256(seed)+ecdh)

    // hash = sha256(seed)
    sha256sum(seed, hash, seed_length);

    // seckey = deriveSecKey(hash)
    // TODO: AUDIT: This should be deterministicKeyPairIteratorStep(),
    // which performs sha256() in a loop,
    // each time checking that the resulting secret key is valid.
    // This code is missing that.
    deterministic_key_pair_iterator_step(hash, SHA256_DIGEST_LENGTH, seckey, pubkey);

    // pubkey = derivePubKey(sha256(hash))
    sha256sum(hash, hash2, sizeof(hash));
    deterministic_key_pair_iterator_step(hash2, SHA256_DIGEST_LENGTH, dummy_seckey, pubkey);

    // ecdh_key = ECDH(pubkey, seckey)
    ecdh(pubkey, seckey, ecdh_key);

    // sha256(hash + ecdh_key)
    memcpy(hash_ecdh, hash, sizeof(hash));
    memcpy(&hash_ecdh[SHA256_DIGEST_LENGTH], ecdh_key, sizeof(ecdh_key));
    sha256sum(hash_ecdh, digest, sizeof(hash_ecdh));
}

#define DEBUG_DETERMINISTIC_KEY_PAIR_ITERATOR 1

bool deterministic_key_pair_iterator(const uint8_t* seed, const size_t seed_length, uint8_t* next_seed, uint8_t* seckey, uint8_t* pubkey)
{
    /*
    SKYCOIN CIPHER AUDIT
    Compare to function: secp254k1.DeterministicKeyPairIterator
    */
    uint8_t seed1[SHA256_DIGEST_LENGTH] = {0};
    uint8_t seed2[SHA256_DIGEST_LENGTH] = {0};

    // TODO: AUDIT: Why 256 here? seed can be any length in the skycoin cipher code.
    // If there are length restrictions imposed here, they must be enforced with a check
    uint8_t keypair_seed[256] = {0};
    if (seed_length > sizeof(keypair_seed)) {
        return false;
    }

    secp256k1sum(seed, seed_length, seed1);

    #if DEBUG_DETERMINISTIC_KEY_PAIR_ITERATOR
    char buf[256];
    tohex(buf, seed, seed_length);
    printf("seedIn: %s\n", buf);
    tohex(buf, seed1, SHA256_DIGEST_LENGTH);
    printf("seed1: %s\n", buf);
    #endif

    // TODO: AUDIT: buffer overflow if seed_length > 256 - SHA256_DIGEST_LENGTH
    memcpy(keypair_seed, seed, seed_length);
    memcpy(&keypair_seed[seed_length], seed1, SHA256_DIGEST_LENGTH);
    memcpy(next_seed, seed1, SHA256_DIGEST_LENGTH);

    sha256sum(keypair_seed, seed2, seed_length + sizeof(seed1));

    #if DEBUG_DETERMINISTIC_KEY_PAIR_ITERATOR
    tohex(buf, seed2, SHA256_DIGEST_LENGTH);
    printf("seed2: %s\n", buf);
    #endif

    deterministic_key_pair_iterator_step(seed2, SHA256_DIGEST_LENGTH, seckey, pubkey);

    #if DEBUG_DETERMINISTIC_KEY_PAIR_ITERATOR
    tohex(buf, seckey, SKYCOIN_SECKEY_LEN);
    printf("seckey: %s\n", buf);
    tohex(buf, pubkey, SKYCOIN_PUBKEY_LEN);
    printf("pubkey: %s\n", buf);
    #endif
    return true;
}

// priv_key 32 bytes private key
// digest 32 bytes sha256 hash
// sig 65 bytes compact recoverable signature
int skycoin_ecdsa_sign_digest(const uint8_t* priv_key, const uint8_t* digest, uint8_t* sig)
{
    int ret;
    const curve_info* curve = get_curve_by_name(SECP256K1_NAME);
    uint8_t recid = 0;
    ret = ecdsa_sign_digest(curve->params, priv_key, digest, sig, &recid, NULL);
    if (recid > 4) {
        // This should never happen; we can abort() here, as a sanity check
        return -3;
    }
    sig[64] = recid;
    return ret;
}

/**
 * @brief sha256sum hash over buffer
 * @param buffer in data
 * @param buffer_len in data len
 * @param out_digest out sha256 data
 */
void sha256sum(const uint8_t* data, uint8_t* out_digest /*size SHA256_DIGEST_LENGTH*/, size_t data_length)
{
    SHA256_CTX ctx;
    sha256_Init(&ctx);
    sha256_Update(&ctx, data, data_length);
    sha256_Final(&ctx, out_digest);
}

/**
 * @brief add_sha256 make the sum of msg2 and to msg1
 * @param msg1 buffer content
 * @param msg1_len buffer conttn len
 * @param msg2 buffer content
 * @param msg2_len buffer content len
 * @param out_digest sum_sha256 of msg1 appened to mag2
 */
void add_sha256(const uint8_t* msg1, size_t msg1_len, const uint8_t* msg2, size_t msg2_len, uint8_t* out_digest)
{
    SHA256_CTX ctx;
    sha256_Init(&ctx);
    sha256_Update(&ctx, msg1, msg1_len);
    sha256_Update(&ctx, msg2, msg2_len);
    sha256_Final(&ctx, out_digest);
}

/*
pubkey is the SKYCOIN_PUBKEY_LEN byte compressed pubkey

address_size is the size of the allocated address buffer, it will be overwritten by the computed address size
The address_size must be at least 36 bytes and the address buffer must be at least that large.
*/
void skycoin_address_from_pubkey(const uint8_t* pubkey, char* b58address, size_t* size_b58address)
{
    /*
    SKYCOIN CIPHER AUDIT
    https://github.com/skycoin/skycoin/wiki/Technical-background-of-version-0-Skycoin-addresses

    address = ripemd160(sha256(sha256(pubkey))
    checksum = sha256(address+version)
    */
    uint8_t address[RIPEMD160_DIGEST_LENGTH + 1 + 4] = {0};
    uint8_t r1[SHA256_DIGEST_LENGTH] = {0};
    uint8_t r2[SHA256_DIGEST_LENGTH] = {0};

    // ripemd160(sha256(sha256(pubkey))
    sha256sum(pubkey, r1, SKYCOIN_PUBKEY_LEN);
    sha256sum(r1, r2, sizeof(r1));
    ripemd160(r2, SHA256_DIGEST_LENGTH, address);

    // compute base58 address
    uint8_t digest[SHA256_DIGEST_LENGTH] = {0};
    address[RIPEMD160_DIGEST_LENGTH] = 0; // version byte

    // checksum
    sha256sum(address, digest, RIPEMD160_DIGEST_LENGTH + 1);
    memcpy(&address[RIPEMD160_DIGEST_LENGTH + 1], digest, SKYCOIN_ADDRESS_CHECKSUM_LENGTH);

    b58enc(b58address, size_b58address, address, sizeof(address));
}

void transaction_initZeroTransaction(Transaction* self)
{
    self->nbIn = 0;
    self->nbOut = 0;
    self->has_innerHash = 0;
}

void transaction_addInput(Transaction* self, uint8_t* address)
{
    memcpy(&self->inAddress[self->nbIn], address, 32);
    self->nbIn++;
};

void transaction_addOutput(Transaction* self, uint32_t coin, uint32_t hour, char* address)
{
    self->outAddress[self->nbOut].coin = coin;
    self->outAddress[self->nbOut].hour = hour;
    size_t len = 36;
    uint8_t b58string[36];
    b58tobin(b58string, &len, address);
    memcpy(self->outAddress[self->nbOut].address, &b58string[36 - len], len);
    self->nbOut++;
}

void transaction_innerHash(Transaction* self)
{
    uint8_t ctx[sizeof(Transaction)];
    memset(ctx, 0, sizeof(Transaction));
    uint64_t bitcount = 0;
    // serialized in
    uint8_t nbIn = self->nbIn;
    memcpy(&ctx[bitcount], &nbIn, 1);
    memset(&ctx[bitcount + 1], 0, 3);
    bitcount += 4;
    for (uint8_t i = 0; i < self->nbIn; ++i) {
        memcpy(&ctx[bitcount], (uint8_t*)&self->inAddress[i], 32);
        bitcount += 32;
    }

    // serialized out
    uint8_t nbOut = self->nbOut;
    memcpy(&ctx[bitcount], &nbOut, 1);
    memset(&ctx[bitcount + 1], 0, 3);
    bitcount += 4;
    for (uint8_t i = 0; i < self->nbOut; ++i) {
        ctx[bitcount] = 0;
        bitcount += 1;
        memcpy(&ctx[bitcount], &self->outAddress[i].address, 20);
        bitcount += 20;
        memcpy(&ctx[bitcount], (uint8_t*)&self->outAddress[i].coin, 4);
        bitcount += 4;
        memset(&ctx[bitcount], 0, 4);
        bitcount += 4;
        memcpy(&ctx[bitcount], (uint8_t*)&self->outAddress[i].hour, 4);
        bitcount += 4;
        memset(&ctx[bitcount], 0, 4);
        bitcount += 4;
    }

    SHA256_CTX sha256ctx;
    sha256_Init(&sha256ctx);
    sha256_Update(&sha256ctx, ctx, bitcount);
    sha256_Final(&sha256ctx, self->innerHash);
    self->has_innerHash = 1;
}

void transaction_msgToSign(Transaction* self, uint8_t index, uint8_t* msg_digest)
{
    if (index >= self->nbIn) {
        return;
    }
    // concat innerHash and transaction hash
    uint8_t shaInput[64];
    if (!self->has_innerHash) {
        transaction_innerHash(self);
    }
    memcpy(shaInput, self->innerHash, 32);
    memcpy(&shaInput[32], (uint8_t*)&self->inAddress[index], 32);
#ifdef EMULATOR
#if EMULATOR
    char str[128];
    tohex(str, shaInput, 64);
    printf("InnerHash computation on %s\n", str);
#endif
#endif
    // compute hash
    SHA256_CTX sha256ctx;
    sha256_Init(&sha256ctx);
    sha256_Update(&sha256ctx, shaInput, 64);
    sha256_Final(&sha256ctx, msg_digest);
}
