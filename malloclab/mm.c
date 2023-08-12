/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define DEBUG 0

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
        /* Team name */
        "Alpaca",
        /* First member's full name */
        "Liam Bao",
        /* First member's email address */
        "bao.zhiw@northeastern.edu",
        /* Second member's full name (leave blank if none) */
        "",
        /* Second member's email address (leave blank if none) */
        ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE           4       /* Word and header/footer size (bytes) */
#define DSIZE           8       /* Double word size (bytes) */
#define INITSIZE        16      /* Initialize heap with this size */
#define MINBLOCKSIZE    16      /* Minimum size for a free block */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))

/* The successor free block pointer and the predecessor free block pointer */
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(ptr + WSIZE))

#if DEBUG
#define CHECKHEAP(lineno) mm_checkheap(__LINE__);
#else
#define CHECKHEAP(lineno)
#endif

/* Data structure
 * Allocated Block          Free Block
 *  ---------               ---------
 * | HEADER  |             | HEADER  |
 *  ---------               ---------
 * |         |             |  NEXT   |
 * |         |              ---------
 * | PAYLOAD |             |  PREV   |
 * |         |              ---------
 * |         |             |         |
 *  ---------              |         |
 * | FOOTER  |              ---------
 *  ---------              | FOOTER  |
 *                          ---------
 *
 * Heap
 *  ____________                                                    _____________
 * |  PROLOGUE  |                8+ bytes or 2 ptrs                |   EPILOGUE  |
 * |------------|------------|-----------|------------|------------|-------------|
 * |   HEADER   |   HEADER   |        PAYLOAD         |   FOOTER   |    HEADER   |
 * |------------|------------|-----------|------------|------------|-------------|
 * ^            ^            ^
 * heap_listp   free_listp   bp
 */

static char *heap_listp;

static void *free_listp;

static void *extend_heap(size_t words);

static void *coalesce(void *ptr);

static void *find_fit(size_t size);

static void place(void *ptr, size_t size);

static void insert_block(void *ptr);

static void delete_block(void *ptr);

#if DEBUG
static void mm_checkheap(int lineno);
#endif

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(INITSIZE + MINBLOCKSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, PACK(MINBLOCKSIZE, 1));                 /* Prologue header */
    PUT(heap_listp + WSIZE, PACK(MINBLOCKSIZE, 0));         /* Free block header */
    PUT(heap_listp + (2 * WSIZE), PACK(0, 0));              /* Space for next pointer  */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 0));              /* Space for prev pointer */
    PUT(heap_listp + (4 * WSIZE), PACK(MINBLOCKSIZE, 0));   /* Free block footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));              /* Epilogue header */
    free_listp = heap_listp + WSIZE;                        /* Point free_list to the header of the first free block */
    CHECKHEAP(__LINE__);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    char *ptr;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    size = MAX(ALIGN(size) + DSIZE, MINBLOCKSIZE);

    if ((ptr = find_fit(size)) == NULL) {
        /* No fit found. Get more memory and place the block */
        if ((ptr = extend_heap(MAX(size, MINBLOCKSIZE) / WSIZE)) == NULL)
            return NULL;
    }

    place(ptr, size);
    return ptr;
}

/*
 * mm_free - Freeing a block and coalescing.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    size = MAX(ALIGN(size) + DSIZE, MINBLOCKSIZE);
    size_t current_size = GET_SIZE(HDRP(ptr));

    void *new_block;
    char *next = HDRP(NEXT_BLKP(ptr));
    size_t newsize = current_size + GET_SIZE(next);

    if (size == current_size) /* Case 1: Size is equal to the current payload size */
        return ptr;

    if (size <= current_size) { /* Case 2: Size is less than the current payload size */
        if (size > MINBLOCKSIZE && (current_size - size) > MINBLOCKSIZE) {
            PUT(HDRP(ptr), PACK(size, 1));
            PUT(FTRP(ptr), PACK(size, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(current_size - size, 1));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(current_size - size, 1));
            mm_free(NEXT_BLKP(ptr));
            return ptr;
        }

        /* allocate a new block of the requested size and release the current block */
        new_block = mm_malloc(size);
        memcpy(new_block, ptr, size);
        mm_free(ptr);
        return new_block;
    } else { /* Case 3: Requested size is greater than the current payload size */
        /* next block is unallocated and is large enough to complete the request
         * merge current block with next block up to the size needed and free the
         * remaining block. */
        if (!GET_ALLOC(next) && newsize >= size) {
            /* merge, split, and release */
            delete_block(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(size, 1));
            PUT(FTRP(ptr), PACK(size, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(newsize - size, 1));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(newsize - size, 1));
            mm_free(NEXT_BLKP(ptr));
            return ptr;
        }

        /* otherwise allocate a new block of the requested size and release the current block */
        new_block = mm_malloc(size);
        memcpy(new_block, ptr, current_size);
        mm_free(ptr);
        return new_block;
    }
}

/******************
 * Helper functions
 ******************/

static void *extend_heap(size_t words) {
    char *ptr;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((ptr = mem_sbrk(size)) == (void *) -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(ptr), PACK(size, 0));           /* Free block header */
    PUT(FTRP(ptr), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(ptr);
}

static void *coalesce(void *ptr) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr))) || PREV_BLKP(ptr) == ptr;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        delete_block(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        delete_block(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr))) + GET_SIZE(HDRP(PREV_BLKP(ptr)));
        delete_block(PREV_BLKP(ptr));
        delete_block(NEXT_BLKP(ptr));
        ptr = PREV_BLKP(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }
    insert_block(ptr);
    CHECKHEAP(__LINE__);
    return ptr;
}

static void *find_fit(size_t size) {
    void *ptr;

    /* Search the free list for a fit */
    for (ptr = free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = SUCC(ptr)) {
        if (size <= GET_SIZE(HDRP(ptr)))
            return ptr;
    }
    return NULL;    /* No fit */
}

static void place(void *ptr, size_t size) {
    size_t block_size = GET_SIZE(HDRP(ptr));
    size_t residual = block_size - size;

    if (residual >= MINBLOCKSIZE) {
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        delete_block(ptr);
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(residual, 0));
        PUT(FTRP(ptr), PACK(residual, 0));
        coalesce(ptr);
    } else {
        PUT(HDRP(ptr), PACK(block_size, 1));
        PUT(FTRP(ptr), PACK(block_size, 1));
        delete_block(ptr);
    }
    CHECKHEAP(__LINE__);
}

static void insert_block(void *ptr) {
    SUCC(ptr) = free_listp;
    PRED(free_listp) = ptr;
    PRED(ptr) = NULL;
    free_listp = ptr;
}

static void delete_block(void *ptr) {
    if (ptr == NULL) return;
    if (PRED(ptr))
        SUCC(PRED(ptr)) = SUCC(ptr);
    else
        free_listp = SUCC(ptr);
    if (SUCC(ptr) != NULL)
        PRED(SUCC(ptr)) = PRED(ptr);
}

#if DEBUG
/**************************
 * Heap Consistency Checker
 **************************/

void mm_checkheap(int lineno) {
    void *ptr;

    /* Is every block in the free list marked as free? */
    for (ptr = free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = SUCC(ptr)) {
        if (GET_ALLOC(HDRP(ptr))) {
            printf("Lineno %d: Block %p in the free list is not marked as free\n", lineno, ptr);
            return;
        }
    }

    /* Are there any contiguous free blocks that somehow escaped coalescing? */

    /* Is every free block actually in the free list? */

    /* Do the pointers in the free list point to valid free blocks? */
    for (ptr = free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = SUCC(ptr)) {
        if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
            printf("Lineno %d: Free block %p points to an invalid heap address\n", lineno, ptr);
            return;
        }
    }

    /* Do any allocated blocks overlap? */

    /* Do the pointers in a heap block point to valid heap addresses? */
}
#endif
