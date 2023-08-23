#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdbool.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Number of blocks in the cache */
#define MAX_BLOCK_NUM 10

typedef struct {
    char val[MAX_OBJECT_SIZE];  /* Value of object in the block */
    char key[MAXLINE];          /* Key of object in the block */
    bool valid;                 /* If the data in the block is valid */
    int counter;                /* Counter for the LRU cache strategy */
    int read_cnt;               /* Number of readers of the block */
    sem_t mutex;                /* Access control of the read_cnt */
    sem_t w;                    /* Access control of the block */
} block_t;

typedef struct {
    block_t blocks[MAX_BLOCK_NUM];  /* Cache consists an array of blocks */
} cache_t;

void cache_init(cache_t *cache);

block_t *cache_read(cache_t *cache, char *key);

void cache_write(cache_t *cache, char *key, char *val);

void lock(block_t *block);

void unlock(block_t *block);

#endif /* __CACHE_H__ */
