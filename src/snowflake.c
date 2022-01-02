#include <snowflake.h>

static uint64_t snowflake_next_timestamp(uint64_t last_timestamp);

uint64_t snowflake_id(snowflake_t *snf) {
    uint64_t cur = snowflake_timestamp();

    if (cur == snf->last_timestamp) {
        snf->seq = (snf->seq + 1) & SF_MAX_SEQ_NUM;
        if (snf->seq == 0) {
            cur = snowflake_next_timestamp(snf->last_timestamp);
        }
    } else {
        snf->seq = 0;
    }

    snf->last_timestamp = cur;

    return (cur - SF_START_TIMESTAMP) << SF_TIMESTAMP_LEFT_OFFSET
        | snf->worker << SF_WORKER_LEFT_OFFSET
        | snf->process << SF_PROCESS_LEFT_OFFSET
        | snf->seq << SF_SEQ_LEFT_OFFSET;
}

uint64_t snowflake_lock_id(snowflake_t *snf) {
    if (!snf->pmutex) return snowflake_id(snf);

    pthread_mutex_lock(snf->pmutex);
    uint64_t id = snowflake_id(snf);
    pthread_mutex_unlock(snf->pmutex);
    return id;
}

uint64_t snowflake_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static uint64_t snowflake_next_timestamp(uint64_t last_timestamp) {
    uint64_t cur;
    do {
        cur = snowflake_timestamp();
    } while (cur <= last_timestamp);
    return cur;
}
