#ifndef MERIDIAN_DIGEST_H
#define MERIDIAN_DIGEST_H

#include <stddef.h>
#include <stdint.h>

#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_DIGEST_BYTES 32u
#define MD_DIGEST_HEX_LENGTH 64u

typedef struct MdDigest {
    unsigned char bytes[MD_DIGEST_BYTES];
    char hex[MD_DIGEST_HEX_LENGTH + 1u];
} MdDigest;

typedef struct MdHash {
    uint64_t lane[4];
    uint64_t length;
} MdHash;

void md_hash_init(MdHash *hash, const char *domain);
void md_hash_update(MdHash *hash, const void *data, size_t len);
void md_hash_update_cstr(MdHash *hash, const char *value);
MdDigest md_hash_finish(MdHash *hash);
MdDigest md_digest_text(const char *domain, const char *value);
MdDigest md_digest_join(const char *domain, const MdDigest *left, const MdDigest *right);
void md_digest_to_hex(MdDigest *digest);
int md_digest_equal(const MdDigest *left, const MdDigest *right);
void md_digest_copy(MdDigest *dst, const MdDigest *src);

#ifdef __cplusplus
}
#endif

#endif
