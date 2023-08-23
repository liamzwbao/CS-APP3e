#include "csapp.h"
#include "cache.h"

/*
 * cache_init - initialize the cache
 */
void cache_init(cache_t *cache) {
    block_t *block;
    int i;
    for (i = 0; i < MAX_BLOCK_NUM; i++) {
        block = &cache->blocks[i];
        block->counter = i;
        block->valid = false;
        block->read_cnt = 0;
        Sem_init(&block->mutex, 0, 1);
        Sem_init(&block->w, 0, 1);
    }
}

/*
 * cache_update - update the counter of cache
 */
void cache_update(cache_t *cache, block_t *block_hit) {
    block_t *block;
    int i;
    for (i = 0; i < MAX_BLOCK_NUM; i++) {
        block = &cache->blocks[i];
        lock(block);
        if (block->counter > block_hit->counter)
            block->counter--;
        unlock(block);
    }
    block_hit->counter = MAX_BLOCK_NUM - 1;
}

/*
 * cache_read - read the key from cache, return -1 if not found
 */
block_t *cache_read(cache_t *cache, char *key) {
    block_t *block;
    int i;
    for (i = 0; i < MAX_BLOCK_NUM; i++) {
        block = &cache->blocks[i];
        lock(block);
        if (block->valid && !strcmp(key, block->key)) {
            cache_update(cache, block);
            unlock(block);
            return block;
        }
        unlock(block);
    }
    return NULL;
}

/*
 * cache_write - write the key and value into the cache
 */
void cache_write(cache_t *cache, char *key, char *val) {
    block_t *block;
    int i;
    for (i = 0; i < MAX_BLOCK_NUM; i++) {
        block = &cache->blocks[i];
        lock(block);
        if (block->counter == 0) {
            break;
        }
        unlock(block);
    }
    strcpy(block->key, key);
    strcpy(block->val, val);
    block->valid = true;
    cache_update(cache, block);
    unlock(block);
}

/*
 * lock - lock the block
 */
void lock(block_t *block) {
    P(&block->mutex);
    block->read_cnt++;
    if (block->read_cnt == 1)
        P(&block->w);
    V(&block->mutex);
}

/*
 * unlock - unlock the block
 */
void unlock(block_t *block) {
    P(&block->mutex);
    block->read_cnt--;
    if (block->read_cnt == 0)
        V(&block->w);
    V(&block->mutex);
}
