#include <jwt.h>

void jwt_sha256(const char *in, const char *key, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, in, strlen(in));
    if (key) SHA256_Update(&sha256, key, strlen(key));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    out[SHA256_DIGEST_LENGTH * 2] = '\0';
}

void jwt_hmac256(const char *in, const char *key, char *out, int *outlen) {
    unsigned char *res = NULL;
    unsigned int   len = -1;

    res = HMAC(EVP_sha256(), key, strlen(key), (const unsigned char *)in,
        strlen(in), res, &len);

    memcpy(out, res, len);
    *outlen = len;
}

/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] = {
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 64, 64, 64, 64, 64, 64, 64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64};

int jwt_b64url_decode(char *bufplain, const char *bufcoded) {
    int                           nbytesdecoded;
    register const unsigned char *bufin;
    register unsigned char       *bufout;
    register int                  nprbytes;

    bufin = (const unsigned char *)bufcoded;
    while (pr2six[*(bufin++)] <= 63)
        ;
    nprbytes      = (bufin - (const unsigned char *)bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    bufout = (unsigned char *)bufplain;
    bufin  = (const unsigned char *)bufcoded;

    while (nprbytes > 4) {
        *(bufout++) =
            (unsigned char)(pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char)(pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) = (unsigned char)(pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ingore that case */
    if (nprbytes > 1) {
        *(bufout++) =
            (unsigned char)(pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
        *(bufout++) =
            (unsigned char)(pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }
    if (nprbytes > 3) {
        *(bufout++) = (unsigned char)(pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    }

    *(bufout++) = '\0';
    nbytesdecoded -= (4 - nprbytes) & 3;
    return nbytesdecoded;
}

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
// "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int jwt_b64url_encode(char *encoded, const char *string, int len) {
    int   i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int)(string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int)(string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            // *p++ = '=';
        } else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int)(string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        // *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

static char jwt_header[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";

char *jwt_encode(uint64_t user_id, const char *key) {
    const char *payload;

    char payload_enc[256];
    char sign[65], sign_enc[256];
    int  sign_len;

    char *token = malloc(2048);

    // exp 30 days
    time_t exp = time(NULL) + 2592000;

    struct json_object *claim = json_object_new_object();
    json_object_object_add(claim, "exp", json_object_new_uint64(exp));
    json_object_object_add(claim, "user_id", json_object_new_uint64(user_id));
    payload = json_object_to_json_string_ext(claim, JSON_C_TO_STRING_PLAIN);

    jwt_b64url_encode(payload_enc, payload, strlen(payload));
    sprintf(token, "%s.%s", jwt_header, payload_enc);

    jwt_hmac256(token, key, sign, &sign_len);
    jwt_b64url_encode(sign_enc, sign, sign_len);

    sprintf(token, "%s.%s.%s", jwt_header, payload_enc, sign_enc);

    json_object_put(claim);

    return token;
}

bool jwt_decode(const char *token, const char *key, uint64_t *user_id) {
    char *token_cp = malloc(strlen(token) + 1);
    strcpy(token_cp, token);

    char *jwt_header = strtok(token_cp, ".");
    char *payload    = strtok(NULL, ".");
    char *sign       = strtok(NULL, ".");

    if (!jwt_header || !payload || !sign) {
        free(token_cp);
        return false;
    }

    char tmp[2048], tmp_sign[64], tmp_sign_enc[256];
    int  tmp_sign_len;
    sprintf(tmp, "%s.%s", jwt_header, payload);
    jwt_hmac256(tmp, key, tmp_sign, &tmp_sign_len);
    jwt_b64url_encode(tmp_sign_enc, tmp_sign, tmp_sign_len);

    if (strcmp(sign, tmp_sign_enc) != 0) {
        free(token_cp);
        return false;
    }

    jwt_b64url_decode(tmp, payload);
    struct json_object *claim = json_tokener_parse(tmp);
    struct json_object *uid, *exp;

    free(token_cp);

    if (!claim) {
        return false;
    }

    json_object_object_get_ex(claim, "exp", &exp);
    json_object_object_get_ex(claim, "user_id", &uid);

    if (!exp || !uid) {
        return false;
    }

    if (json_object_get_type(exp) != json_type_int ||
        json_object_get_type(uid) != json_type_int) {
        return false;
    }

    if (json_object_get_uint64(exp) <= (uint64_t)time(NULL)) {
        return false;
    }

    *user_id = json_object_get_uint64(uid);

    return true;
}
