#ifndef MERIDIAN_CRYPTO_H
#define MERIDIAN_CRYPTO_H

#include <stddef.h>

#include "meridian/codec.h"
#include "meridian/digest.h"
#include "meridian/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MD_LABEL_LENGTH 31u

typedef struct MdIdentity {
    char label[MD_LABEL_LENGTH + 1u];
    MdDigest public_key;
    MdDigest verifier;
} MdIdentity;

typedef struct MdSignature {
    MdDigest signer;
    MdDigest authenticator;
} MdSignature;

MdIdentity md_identity_from_label(const char *label);
MdSignature md_sign_payload(const MdIdentity *identity, const char *domain, const char *payload);
int md_verify_payload(const MdIdentity *identity, const char *domain, const char *payload, const MdSignature *signature);
void md_signature_digest(const MdSignature *signature, MdDigest *out);

#ifdef __cplusplus
}
#endif

#endif
