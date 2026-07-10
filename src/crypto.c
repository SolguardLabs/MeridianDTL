#include "meridian/crypto.h"

#include <stdio.h>
#include <string.h>

static void md_copy_label(char *dst, size_t dst_len, const char *src) {
    if (dst == 0 || dst_len == 0u) {
        return;
    }
    if (src == 0) {
        src = "";
    }
    snprintf(dst, dst_len, "%s", src);
}

MdIdentity md_identity_from_label(const char *label) {
    MdIdentity identity;
    char canonical[160];

    memset(&identity, 0, sizeof(identity));
    md_copy_label(identity.label, sizeof(identity.label), label);
    snprintf(canonical, sizeof(canonical), "identity:%s", identity.label);
    identity.public_key = md_digest_text("meridian-public-key", canonical);
    snprintf(canonical, sizeof(canonical), "verifier:%s:%s", identity.label, identity.public_key.hex);
    identity.verifier = md_digest_text("meridian-verifier", canonical);
    return identity;
}

MdSignature md_sign_payload(const MdIdentity *identity, const char *domain, const char *payload) {
    MdSignature signature;
    MdHash hash;

    memset(&signature, 0, sizeof(signature));
    if (identity == 0) {
        md_digest_to_hex(&signature.signer);
        md_digest_to_hex(&signature.authenticator);
        return signature;
    }
    signature.signer = identity->public_key;
    md_hash_init(&hash, domain);
    md_hash_update(&hash, identity->verifier.bytes, sizeof(identity->verifier.bytes));
    md_hash_update(&hash, "|", 1u);
    md_hash_update_cstr(&hash, payload);
    signature.authenticator = md_hash_finish(&hash);
    return signature;
}

int md_verify_payload(const MdIdentity *identity, const char *domain, const char *payload, const MdSignature *signature) {
    MdSignature expected;

    if (identity == 0 || signature == 0) {
        return 0;
    }
    if (!md_digest_equal(&identity->public_key, &signature->signer)) {
        return 0;
    }
    expected = md_sign_payload(identity, domain, payload);
    return md_digest_equal(&expected.authenticator, &signature->authenticator);
}

void md_signature_digest(const MdSignature *signature, MdDigest *out) {
    MdHash hash;

    if (out == 0) {
        return;
    }
    md_hash_init(&hash, "meridian-signature-digest");
    if (signature != 0) {
        md_hash_update(&hash, signature->signer.bytes, sizeof(signature->signer.bytes));
        md_hash_update(&hash, signature->authenticator.bytes, sizeof(signature->authenticator.bytes));
    }
    *out = md_hash_finish(&hash);
}
