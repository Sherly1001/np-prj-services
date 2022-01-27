#ifndef __JWT_H__
#define __JWT_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <json-c/json.h>

#include <bool.h>

void jwt_sha256(const char *in, const char *key, char *out);
void jwt_hmac256(const char *in, const char *key, char *out, int *outlen);

int jwt_b64url_encode(
    char *coded_dst, const char *plain_src, int len_plain_src);
int jwt_b64url_decode(char *plain_dst, const char *coded_src);

char *jwt_encode(uint64_t user_id, const char *key);
bool  jwt_decode(const char *token, const char *key, uint64_t *user_id);

#endif
