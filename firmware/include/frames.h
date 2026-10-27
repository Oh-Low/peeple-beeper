#pragma once

#include "crypto.h"
#include "hal.h"
#include "identity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_HELLO 0x01
#define FRAME_HELLO_ACK 0x02
#define FRAME_PAGE 0x03
#define FRAME_PAIR_LEAVE 0x04
#define FRAME_PAGE_ACK 0x05
#define FRAME_LOC_REQ 0x06
#define FRAME_LOC 0x07
#define FRAME_VERSION 1

#define LOC_PLAIN_LEN 11
#define LOC_FLAG_FIX 0x01
#define LOC_FLAG_DENY 0x02

#define EPH_LEN 32
#define HELLO_NONCE_LEN 8
#define PAGE_NONCE_LEN 8
#define PAGE_MSG_ID_LEN 8

#define HELLO_FIXED (1 + 1 + NODE_ID_LEN + EPH_LEN + HELLO_NONCE_LEN + 1)
#define HELLO_ACK_LEN (1 + 1 + NODE_ID_LEN + NODE_ID_LEN + CONFIRM_LEN)
#define PAGE_FIXED (1 + 1 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN + 1 + 1 + MAC_LEN)
#define PAGE_ACK_LEN (1 + 1 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN + 1 + MAC_LEN)
#define PAGE_HOP_TTL 3
#define PAGE_HOP_TTL_OFF (2 + NODE_ID_LEN + NODE_ID_LEN + PAGE_NONCE_LEN)
/* Plaintext is msg_id || beeple || text. */
#define PAGE_WIRE_BODY_MAX (PAGE_BODY_MAX + PAGE_MSG_ID_LEN + 1)

typedef struct {
  uint8_t node_id[NODE_ID_LEN];
  uint8_t eph[EPH_LEN];
  uint8_t nonce[HELLO_NONCE_LEN];
  char name[PAGE_FROM_MAX + 1];
  uint8_t beeple;
} hello_payload_t;

typedef struct {
  uint8_t src_id[NODE_ID_LEN];
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t confirm[CONFIRM_LEN];
} hello_ack_payload_t;

typedef hello_ack_payload_t pair_leave_payload_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t src_id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint8_t hop_ttl;
  uint8_t body[PAGE_WIRE_BODY_MAX];
  uint8_t body_len;
  uint8_t mac[MAC_LEN];
} page_payload_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t src_id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint8_t hop_ttl;
  uint8_t mac[MAC_LEN];
} page_ack_payload_t;

typedef page_ack_payload_t loc_req_payload_t;

typedef struct {
  uint8_t dest_id[NODE_ID_LEN];
  uint8_t src_id[NODE_ID_LEN];
  uint8_t nonce[PAGE_NONCE_LEN];
  uint8_t hop_ttl;
  uint8_t body[LOC_PLAIN_LEN];
  uint8_t body_len;
  uint8_t mac[MAC_LEN];
} loc_payload_t;

int frame_type(const uint8_t *bytes, size_t len);

size_t frame_encode_hello(const hello_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_hello(const uint8_t *bytes, size_t len, hello_payload_t *out);

size_t frame_encode_hello_ack(const hello_ack_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_hello_ack(const uint8_t *bytes, size_t len, hello_ack_payload_t *out);

size_t frame_encode_pair_leave(const pair_leave_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_pair_leave(const uint8_t *bytes, size_t len, pair_leave_payload_t *out);

size_t frame_encode_page(const page_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_page(const uint8_t *bytes, size_t len, page_payload_t *out);

size_t frame_encode_page_ack(const page_ack_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_page_ack(const uint8_t *bytes, size_t len, page_ack_payload_t *out);

size_t frame_encode_loc_req(const loc_req_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_loc_req(const uint8_t *bytes, size_t len, loc_req_payload_t *out);

size_t frame_encode_loc(const loc_payload_t *p, uint8_t *out, size_t max);
bool frame_decode_loc(const uint8_t *bytes, size_t len, loc_payload_t *out);

bool frame_set_hop_ttl(uint8_t *bytes, size_t len, uint8_t ttl);
