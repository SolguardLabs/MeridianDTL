#include "meridian/digest.h"

#include <string.h>

static uint64_t md_rotl64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t md_mix64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static void md_store_u64_be(unsigned char *out, uint64_t value) {
    for (size_t i = 0u; i < 8u; i++) {
        out[i] = (unsigned char)((value >> (56u - (i * 8u))) & 0xffu);
    }
}

void md_digest_to_hex(MdDigest *digest) {
    static const char table[] = "0123456789abcdef";

    if (digest == 0) {
        return;
    }
    for (size_t i = 0u; i < MD_DIGEST_BYTES; i++) {
        digest->hex[i * 2u] = table[(digest->bytes[i] >> 4u) & 0x0fu];
        digest->hex[(i * 2u) + 1u] = table[digest->bytes[i] & 0x0fu];
    }
    digest->hex[MD_DIGEST_HEX_LENGTH] = '\0';
}

void md_hash_init(MdHash *hash, const char *domain) {
    if (hash == 0) {
        return;
    }
    hash->lane[0] = UINT64_C(0x243f6a8885a308d3);
    hash->lane[1] = UINT64_C(0x13198a2e03707344);
    hash->lane[2] = UINT64_C(0xa4093822299f31d0);
    hash->lane[3] = UINT64_C(0x082efa98ec4e6c89);
    hash->length = 0u;
    if (domain != 0) {
        md_hash_update_cstr(hash, domain);
        md_hash_update(hash, "\x1f", 1u);
    }
}

void md_hash_update(MdHash *hash, const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;

    if (hash == 0 || data == 0 || len == 0u) {
        return;
    }
    for (size_t i = 0u; i < len; i++) {
        uint64_t b = (uint64_t)bytes[i] + UINT64_C(0x9e3779b97f4a7c15) + hash->length;
        size_t lane = (size_t)(hash->length & 3u);
        hash->lane[lane] ^= b;
        hash->lane[lane] *= UINT64_C(0x100000001b3);
        hash->lane[lane] = md_rotl64(hash->lane[lane], (unsigned int)(11u + (lane * 7u)));
        hash->lane[(lane + 1u) & 3u] ^= md_mix64(hash->lane[lane] + b);
        hash->length++;
    }
}

void md_hash_update_cstr(MdHash *hash, const char *value) {
    if (value == 0) {
        md_hash_update(hash, "<null>", 6u);
        return;
    }
    md_hash_update(hash, value, strlen(value));
}

MdDigest md_hash_finish(MdHash *hash) {
    MdDigest digest;
    uint64_t lane0;
    uint64_t lane1;
    uint64_t lane2;
    uint64_t lane3;

    memset(&digest, 0, sizeof(digest));
    if (hash == 0) {
        md_digest_to_hex(&digest);
        return digest;
    }

    lane0 = md_mix64(hash->lane[0] ^ hash->length ^ hash->lane[2]);
    lane1 = md_mix64(hash->lane[1] + hash->length + hash->lane[3]);
    lane2 = md_mix64(hash->lane[2] ^ md_rotl64(lane0, 17u));
    lane3 = md_mix64(hash->lane[3] + md_rotl64(lane1, 29u));

    md_store_u64_be(&digest.bytes[0], lane0);
    md_store_u64_be(&digest.bytes[8], lane1);
    md_store_u64_be(&digest.bytes[16], lane2);
    md_store_u64_be(&digest.bytes[24], lane3);
    md_digest_to_hex(&digest);
    return digest;
}

MdDigest md_digest_text(const char *domain, const char *value) {
    MdHash hash;

    md_hash_init(&hash, domain);
    md_hash_update_cstr(&hash, value);
    return md_hash_finish(&hash);
}

MdDigest md_digest_join(const char *domain, const MdDigest *left, const MdDigest *right) {
    MdHash hash;

    md_hash_init(&hash, domain);
    if (left != 0) {
        md_hash_update(&hash, left->bytes, sizeof(left->bytes));
    }
    md_hash_update(&hash, "|", 1u);
    if (right != 0) {
        md_hash_update(&hash, right->bytes, sizeof(right->bytes));
    }
    return md_hash_finish(&hash);
}

int md_digest_equal(const MdDigest *left, const MdDigest *right) {
    unsigned char diff = 0u;

    if (left == 0 || right == 0) {
        return 0;
    }
    for (size_t i = 0u; i < MD_DIGEST_BYTES; i++) {
        diff |= (unsigned char)(left->bytes[i] ^ right->bytes[i]);
    }
    return diff == 0u;
}

void md_digest_copy(MdDigest *dst, const MdDigest *src) {
    if (dst == 0 || src == 0) {
        return;
    }
    memcpy(dst, src, sizeof(*dst));
}
