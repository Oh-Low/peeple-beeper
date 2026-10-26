#include "frames.h"

#include "util.h"

#include <string.h>

int frame_type(const uint8_t *bytes, size_t len) {
  if (!bytes || len == 0) return -1;
  return (int)bytes[0];
}

size_t frame_encode_hello(const hello_payload_t *p, uint8_t *out, size_t max) {
  if (!p || !out) return 0;
  size_t name_len = str_ascii_len(p->name);
  if (name_len > PAGE_FROM_MAX) name_len = PAGE_FROM_MAX;
  size_t total = HELLO_FIXED + name_len + 1;
  if (max < total) return 0;
  size_t o = 0;
  out[o++] = FRAME_HELLO;
  out[o++] = FRAME_VERSION;
  memcpy(out + o, p->node_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->eph, EPH_LEN);
  o += EPH_LEN;
  memcpy(out + o, p->nonce, HELLO_NONCE_LEN);
  o += HELLO_NONCE_LEN;
  out[o++] = (uint8_t)name_len;
  if (name_len) memcpy(out + o, p->name, name_len);
  o += name_len;
  out[o++] = p->beeple;
  return total;
}

bool frame_decode_hello(const uint8_t *bytes, size_t len, hello_payload_t *out) {
  if (!bytes || !out || len < HELLO_FIXED) return false;
  if (bytes[0] != FRAME_HELLO || bytes[1] != FRAME_VERSION) return false;
  size_t o = 2;
  memcpy(out->node_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->eph, bytes + o, EPH_LEN);
  o += EPH_LEN;
  memcpy(out->nonce, bytes + o, HELLO_NONCE_LEN);
  o += HELLO_NONCE_LEN;
  uint8_t name_len = bytes[o++];
  if (name_len > PAGE_FROM_MAX) return false;
  if (len != HELLO_FIXED + name_len + 1) return false;
  memset(out->name, 0, sizeof out->name);
  if (name_len) memcpy(out->name, bytes + o, name_len);
  out->name[name_len] = 0;
  out->beeple = bytes[o + name_len];
  return true;
}

static size_t encode_ack_like(uint8_t type, const hello_ack_payload_t *p, uint8_t *out, size_t max) {
  if (!p || !out || max < HELLO_ACK_LEN) return 0;
  size_t o = 0;
  out[o++] = type;
  out[o++] = FRAME_VERSION;
  memcpy(out + o, p->src_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->dest_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->confirm, CONFIRM_LEN);
  return HELLO_ACK_LEN;
}

static bool decode_ack_like(uint8_t type, const uint8_t *bytes, size_t len, hello_ack_payload_t *out) {
  if (!bytes || !out || len != HELLO_ACK_LEN) return false;
  if (bytes[0] != type || bytes[1] != FRAME_VERSION) return false;
  size_t o = 2;
  memcpy(out->src_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->dest_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->confirm, bytes + o, CONFIRM_LEN);
  return true;
}

size_t frame_encode_hello_ack(const hello_ack_payload_t *p, uint8_t *out, size_t max) {
  return encode_ack_like(FRAME_HELLO_ACK, p, out, max);
}

bool frame_decode_hello_ack(const uint8_t *bytes, size_t len, hello_ack_payload_t *out) {
  return decode_ack_like(FRAME_HELLO_ACK, bytes, len, out);
}

size_t frame_encode_pair_leave(const pair_leave_payload_t *p, uint8_t *out, size_t max) {
  return encode_ack_like(FRAME_PAIR_LEAVE, p, out, max);
}

bool frame_decode_pair_leave(const uint8_t *bytes, size_t len, pair_leave_payload_t *out) {
  return decode_ack_like(FRAME_PAIR_LEAVE, bytes, len, out);
}

size_t frame_encode_page(const page_payload_t *p, uint8_t *out, size_t max) {
  if (!p || !out) return 0;
  if (p->body_len > PAGE_WIRE_BODY_MAX) return 0;
  size_t total = PAGE_FIXED + p->body_len;
  if (max < total) return 0;
  size_t o = 0;
  out[o++] = FRAME_PAGE;
  out[o++] = FRAME_VERSION;
  memcpy(out + o, p->dest_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->src_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->nonce, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out[o++] = p->hop_ttl;
  out[o++] = p->body_len;
  if (p->body_len) memcpy(out + o, p->body, p->body_len);
  o += p->body_len;
  memcpy(out + o, p->mac, MAC_LEN);
  return total;
}

bool frame_decode_page(const uint8_t *bytes, size_t len, page_payload_t *out) {
  size_t min = PAGE_FIXED - MAC_LEN;
  if (!bytes || !out || len < min) return false;
  if (bytes[0] != FRAME_PAGE || bytes[1] != FRAME_VERSION) return false;
  uint8_t body_len = bytes[PAGE_HOP_TTL_OFF + 1];
  if (body_len > PAGE_WIRE_BODY_MAX) return false;
  if (len != PAGE_FIXED + body_len) return false;
  size_t o = 2;
  memcpy(out->dest_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->src_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->nonce, bytes + o, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out->hop_ttl = bytes[o++];
  o += 1;
  out->body_len = body_len;
  memset(out->body, 0, sizeof out->body);
  if (body_len) memcpy(out->body, bytes + o, body_len);
  o += body_len;
  memcpy(out->mac, bytes + o, MAC_LEN);
  return true;
}

static size_t encode_page_ack_like(uint8_t type, const page_ack_payload_t *p, uint8_t *out, size_t max) {
  if (!p || !out || max < PAGE_ACK_LEN) return 0;
  size_t o = 0;
  out[o++] = type;
  out[o++] = FRAME_VERSION;
  memcpy(out + o, p->dest_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->src_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->nonce, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out[o++] = p->hop_ttl;
  memcpy(out + o, p->mac, MAC_LEN);
  return PAGE_ACK_LEN;
}

static bool decode_page_ack_like(uint8_t type, const uint8_t *bytes, size_t len, page_ack_payload_t *out) {
  if (!bytes || !out || len != PAGE_ACK_LEN) return false;
  if (bytes[0] != type || bytes[1] != FRAME_VERSION) return false;
  size_t o = 2;
  memcpy(out->dest_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->src_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->nonce, bytes + o, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out->hop_ttl = bytes[o++];
  memcpy(out->mac, bytes + o, MAC_LEN);
  return true;
}

size_t frame_encode_page_ack(const page_ack_payload_t *p, uint8_t *out, size_t max) {
  return encode_page_ack_like(FRAME_PAGE_ACK, p, out, max);
}

bool frame_decode_page_ack(const uint8_t *bytes, size_t len, page_ack_payload_t *out) {
  return decode_page_ack_like(FRAME_PAGE_ACK, bytes, len, out);
}

size_t frame_encode_loc_req(const loc_req_payload_t *p, uint8_t *out, size_t max) {
  return encode_page_ack_like(FRAME_LOC_REQ, p, out, max);
}

bool frame_decode_loc_req(const uint8_t *bytes, size_t len, loc_req_payload_t *out) {
  return decode_page_ack_like(FRAME_LOC_REQ, bytes, len, out);
}

size_t frame_encode_loc(const loc_payload_t *p, uint8_t *out, size_t max) {
  if (!p || !out) return 0;
  if (p->body_len != LOC_PLAIN_LEN) return 0;
  size_t total = PAGE_FIXED + LOC_PLAIN_LEN;
  if (max < total) return 0;
  size_t o = 0;
  out[o++] = FRAME_LOC;
  out[o++] = FRAME_VERSION;
  memcpy(out + o, p->dest_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->src_id, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out + o, p->nonce, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out[o++] = p->hop_ttl;
  out[o++] = LOC_PLAIN_LEN;
  memcpy(out + o, p->body, LOC_PLAIN_LEN);
  o += LOC_PLAIN_LEN;
  memcpy(out + o, p->mac, MAC_LEN);
  return total;
}

bool frame_decode_loc(const uint8_t *bytes, size_t len, loc_payload_t *out) {
  if (!bytes || !out || len != PAGE_FIXED + LOC_PLAIN_LEN) return false;
  if (bytes[0] != FRAME_LOC || bytes[1] != FRAME_VERSION) return false;
  if (bytes[PAGE_HOP_TTL_OFF + 1] != LOC_PLAIN_LEN) return false;
  size_t o = 2;
  memcpy(out->dest_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->src_id, bytes + o, NODE_ID_LEN);
  o += NODE_ID_LEN;
  memcpy(out->nonce, bytes + o, PAGE_NONCE_LEN);
  o += PAGE_NONCE_LEN;
  out->hop_ttl = bytes[o++];
  o += 1;
  out->body_len = LOC_PLAIN_LEN;
  memcpy(out->body, bytes + o, LOC_PLAIN_LEN);
  o += LOC_PLAIN_LEN;
  memcpy(out->mac, bytes + o, MAC_LEN);
  return true;
}

bool frame_set_hop_ttl(uint8_t *bytes, size_t len, uint8_t ttl) {
  if (!bytes || len == 0) return false;
  uint8_t kind = bytes[0];
  if (kind != FRAME_PAGE && kind != FRAME_PAGE_ACK && kind != FRAME_LOC && kind != FRAME_LOC_REQ) {
    return false;
  }
  if (bytes[1] != FRAME_VERSION) return false;
  if (kind == FRAME_PAGE_ACK || kind == FRAME_LOC_REQ) {
    if (len != PAGE_ACK_LEN) return false;
  } else if (kind == FRAME_LOC) {
    if (len != PAGE_FIXED + LOC_PLAIN_LEN) return false;
    if (bytes[PAGE_HOP_TTL_OFF + 1] != LOC_PLAIN_LEN) return false;
  } else {
    if (len < PAGE_FIXED) return false;
    uint8_t body_len = bytes[PAGE_HOP_TTL_OFF + 1];
    if (len != PAGE_FIXED + body_len) return false;
  }
  bytes[PAGE_HOP_TTL_OFF] = ttl;
  return true;
}
