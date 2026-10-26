#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SESSION_KEY_LEN 16
#define HASH_LEN 32
#define MAC_LEN 8
#define CONFIRM_LEN 16

void sha256(const uint8_t *data, size_t len, uint8_t out[HASH_LEN]);

void pair_derive_session_key(const uint8_t *id_a, const uint8_t *id_b, const uint8_t *eph_a,
                             const uint8_t *eph_b, uint8_t out[SESSION_KEY_LEN]);

void pair_confirm_hash(const uint8_t *hello_a, size_t na, const uint8_t *hello_b, size_t nb,
                       uint8_t out[CONFIRM_LEN]);

void page_seal(const uint8_t *session_key, const uint8_t *nonce, const uint8_t *plaintext,
               size_t n, uint8_t *cipher, uint8_t mac[MAC_LEN]);

/* Returns true and writes n bytes to plaintext if MAC matches. */
bool page_open(const uint8_t *session_key, const uint8_t *nonce, const uint8_t *cipher, size_t n,
               const uint8_t mac[MAC_LEN], uint8_t *plaintext);

void page_ack_mac(const uint8_t *session_key, const uint8_t *nonce, uint8_t mac[MAC_LEN]);
bool page_ack_check(const uint8_t *session_key, const uint8_t *nonce, const uint8_t mac[MAC_LEN]);

void loc_req_mac(const uint8_t *session_key, const uint8_t *nonce, uint8_t mac[MAC_LEN]);
bool loc_req_check(const uint8_t *session_key, const uint8_t *nonce, const uint8_t mac[MAC_LEN]);
