#ifndef __SNOWFLAKE_H__
#define __SNOWFLAKE_H__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#define SF_START_TIMESTAMP 1577811600000

#define SF_WORKER_BIT  5
#define SF_PROCESS_BIT 5
#define SF_SEQ_BIT     12

#define SF_MAX_WORKER_NUM  ((1 << SF_WORKER_BIT) - 1)
#define SF_MAX_PROCESS_NUM ((1 << SF_PROCESS_BIT) - 1)
#define SF_MAX_SEQ_NUM     ((1 << SF_SEQ_BIT) - 1)

#define SF_SEQ_LEFT_OFFSET       0
#define SF_PROCESS_LEFT_OFFSET   (SF_SEQ_LEFT_OFFSET + SF_SEQ_BIT)
#define SF_WORKER_LEFT_OFFSET    (SF_PROCESS_LEFT_OFFSET + SF_PROCESS_BIT)
#define SF_TIMESTAMP_LEFT_OFFSET (SF_WORKER_LEFT_OFFSET + SF_WORKER_BIT)

typedef struct {
    int      worker;
    int      process;
    int      seq;
    uint64_t last_timestamp;

    pthread_mutex_t *pmutex;
} snowflake_t;

uint64_t snowflake_id(snowflake_t *);
uint64_t snowflake_lock_id(snowflake_t *);
uint64_t snowflake_timestamp();

// return miliseconds
uint64_t snowflake_id_to_msec(uint64_t id);

#endif
