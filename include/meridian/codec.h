#ifndef MERIDIAN_CODEC_H
#define MERIDIAN_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "meridian/amount.h"
#include "meridian/digest.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MdWriter {
    char *buf;
    size_t len;
    size_t cap;
    MdStatus status;
} MdWriter;

void md_writer_init(MdWriter *writer, char *buf, size_t cap);
void md_writer_put(MdWriter *writer, const char *text);
void md_writer_field(MdWriter *writer, const char *name, const char *value);
void md_writer_u64(MdWriter *writer, const char *name, uint64_t value);
void md_writer_u32(MdWriter *writer, const char *name, uint32_t value);
void md_writer_amount(MdWriter *writer, const char *name, MdAmount amount);
void md_writer_digest(MdWriter *writer, const char *name, const MdDigest *digest);
MdStatus md_writer_status(const MdWriter *writer);
MdDigest md_digest_canonical(const char *domain, const char *payload);

#ifdef __cplusplus
}
#endif

#endif
