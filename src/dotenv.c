#include <dotenv.h>

static char *env_val_parse(const char *val) {
    char  *parsed = malloc(2048);
    size_t len    = strlen(val);
    char  *cpy    = malloc(len + 1);
    strcpy(cpy, val);

    size_t i = 0;
    for (size_t j = 0; j < len; ++j) {
        if (j == 0 && val[j] == '"') {
            if (len > 1 && val[len - 1] == '"') {
                len -= 1;
                continue;
            } else {
                goto __evp_err;
            }
        } else if (val[j] == '\\' && j + 1 < len) {
            switch (val[j + 1]) {
                case 'n':
                    parsed[i++] = '\n';
                    j += 1;
                    break;
                case 't':
                    parsed[i++] = '\t';
                    j += 1;
                    break;
                case 'r':
                    parsed[i++] = '\r';
                    j += 1;
                    break;
                case '\\':
                    parsed[i++] = '\\';
                    j += 1;
                    break;
                default:
                    parsed[i++] = '\\';
            }
        } else if (val[j] == '$') {
            if (j + 1 < len) {
                if (val[j + 1] == '{') {
                    if (j + 3 >= len) goto __evp_err;
                    size_t t = j + 2;
                    while (t < len && (isalpha(val[t]) || val[t] == '_') &&
                           val[t] != '}')
                        t += 1;

                    if (val[t] != '}') goto __evp_err;

                    cpy[t]    = '\0';
                    char *rep = getenv(cpy + j + 2);
                    if (rep) {
                        size_t l = strlen(rep);
                        strcat(parsed + i, rep);
                        i += l;
                    }

                    j = t;
                } else {
                    size_t t = j + 1;
                    while (t < len && (isalpha(val[t]) || val[t] == '_'))
                        t += 1;

                    cpy[t]    = '\0';
                    char *rep = getenv(cpy + j + 1);
                    if (rep) {
                        size_t l = strlen(rep);
                        strcat(parsed + i, rep);
                        i += l;
                    }

                    j = t - 1;
                }
            } else {
                parsed[i++] = '$';
            }
        } else {
            parsed[i++] = val[j];
        }
    }

    goto __evp_done;

__evp_err:
    free(cpy);
    free(parsed);
    return NULL;
__evp_done:
    free(cpy);
    parsed[i] = '\0';
    return parsed;
}

void load_env() {
    FILE *fp = fopen("./.env", "r");
    if (!fp) return;

    char  line[1024];
    char *name, *val, *parsed;

    while (fgets(line, 1023, fp)) {
        name = strtok(line, "=");
        val  = strtok(NULL, "\r\n");
        if (!val) continue;
        parsed = env_val_parse(val);
        if (!parsed) continue;
        setenv(name, parsed, 0);
        free(parsed);
    }

    fclose(fp);
}
