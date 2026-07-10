#include "meridian/codec.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void md_writer_vprintf(MdWriter *writer, const char *fmt, va_list args) {
    int written;

    if (writer == 0 || writer->status != MD_OK || fmt == 0) {
        return;
    }
    if (writer->len >= writer->cap) {
        writer->status = MD_ERR_CAPACITY;
        return;
    }
    written = vsnprintf(writer->buf + writer->len, writer->cap - writer->len, fmt, args);
    if (written < 0 || (size_t)written >= writer->cap - writer->len) {
        writer->status = MD_ERR_CAPACITY;
        if (writer->cap > 0u) {
            writer->buf[writer->cap - 1u] = '\0';
        }
        return;
    }
    writer->len += (size_t)written;
}

static void md_writer_printf(MdWriter *writer, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    md_writer_vprintf(writer, fmt, args);
    va_end(args);
}

void md_writer_init(MdWriter *writer, char *buf, size_t cap) {
    if (writer == 0) {
        return;
    }
    writer->buf = buf;
    writer->len = 0u;
    writer->cap = cap;
    writer->status = (buf == 0 || cap == 0u) ? MD_ERR_ARGUMENT : MD_OK;
    if (buf != 0 && cap > 0u) {
        buf[0] = '\0';
    }
}

void md_writer_put(MdWriter *writer, const char *text) {
    if (text == 0) {
        text = "";
    }
    md_writer_printf(writer, "%s", text);
}

void md_writer_field(MdWriter *writer, const char *name, const char *value) {
    if (name == 0) {
        name = "";
    }
    if (value == 0) {
        value = "";
    }
    md_writer_printf(writer, "%s=%s;", name, value);
}

void md_writer_u64(MdWriter *writer, const char *name, uint64_t value) {
    if (name == 0) {
        name = "";
    }
    md_writer_printf(writer, "%s=%" PRIu64 ";", name, value);
}

void md_writer_u32(MdWriter *writer, const char *name, uint32_t value) {
    if (name == 0) {
        name = "";
    }
    md_writer_printf(writer, "%s=%" PRIu32 ";", name, value);
}

void md_writer_amount(MdWriter *writer, const char *name, MdAmount amount) {
    md_writer_u64(writer, name, amount);
}

void md_writer_digest(MdWriter *writer, const char *name, const MdDigest *digest) {
    if (digest == 0) {
        md_writer_field(writer, name, "");
        return;
    }
    md_writer_field(writer, name, digest->hex);
}

MdStatus md_writer_status(const MdWriter *writer) {
    if (writer == 0) {
        return MD_ERR_ARGUMENT;
    }
    return writer->status;
}

MdDigest md_digest_canonical(const char *domain, const char *payload) {
    MdHash hash;

    md_hash_init(&hash, domain);
    if (payload != 0) {
        md_hash_update(&hash, payload, strlen(payload));
    }
    return md_hash_finish(&hash);
}
