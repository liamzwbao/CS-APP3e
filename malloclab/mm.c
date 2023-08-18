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
#define INITCHUNKSIZE   (1<<6)  /* Initialize heap with this size */
#define CHUNKSIZE       (1<<12) /* Extend heap by this amount (bytes) */
#define LISTSIZE        16      /* Number of free lists */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

/* Set a pointer */
#define SET_PTR(p, ptr) (*(size_t *)(p) = (size_t)(ptr))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))

/* The successor free block pointer's pointer and the predecessor free block pointer's pointer in the free list */
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

/* The successor free block pointer and the predecessor free block pointer */
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

#if DEBUG
#define CHECKHEAP(lineno) mm_checkheap(__LINE__);
#else
#define CHECKHEAP(lineno)
#endif

/* Data structure
Allocated block:
                            31  30  29  ... 5   4   3   2   1   0
                            +-------------------------+-------+--+
    Header:                 |   Size of the block     |       | A|
        block pointer +-->  +-------------------------+-------+--+
                            |                                    |
                            |   Payload                          |
                            |                                    |
                            +------------------------------------+
                            |   Padding(optional)                |
                            +-------------------------+-------+--+
    Footer:                 |   Size of the block     |       | A|
                            +-------------------------+-------+--+

Free block:
                            31  30  29  ... 5   4   3   2   1   0
                            +-------------------------+-------+--+
    Header:                 |   size of the block     |       | A|
        block pointer +-->  +-------------------------+-------+--+
                            |   pointer to its predecessor       |
                            +------------------------------------+
                            |   pointer to its successor         |
                            +------------------------------------+
                            |                                    |
                            |   Payload                          |
                            |                                    |
                            +------------------------------------+
                            |   Padding(optional)                |
                            +-------------------------+-------+--+
    Footer:                 |   size of the block     |       | A|
                            +-------------------------+-------+--+

Heap:
                            31  30  29  ... 5   4   3   2   1   0
    Start of heap:          +-------------------------+-------+--+
                            |   0(Padding)            |       |  |
                            +-------------------------+-------+--+  <--+
                            |   8                     |       | A|     |
static char *heap_listp +-> +-------------------------+-------+--+     +--  Prologue block
                            |   8                     |       | A|     |
                            +-------------------------+-------+--+  <--+
                            |                                    |
                            |                                    |
                            |   Blocks                           |
                            |                                    |
                            |                                    |
                            +-------------------------+-------+--+  <--+
    Footer:                 |   0                     |       | A|     +--  Epilogue block
                            +-------------------------+-------+--+  <--+

 Segregated free lists:

                            +---+---+---+---+---+---+---+---+---+
                            |   |   |   |   |   |   |   |   |   |
                            +-+-+---+---+---+---+---+---+---+---+
                              |       |           |
                              v       v           v
                            +---+   +---+       +---+
                            |   |   |   |       |   |
                            +---+   +---+       +---+
                              |
                              v
                            +---+
                            |   |
                            +---+
 */

static char *heap_listp;

static void *segregated_free_lists[LISTSIZE]; /* Segregated free lists */

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
    /* Initialize the segregated free lists */
    for (int i = 0; i < LISTSIZE; i++) {
        segregated_free_lists[i] = NULL;
    }

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* Epilogue header */
    CHECKHEAP(__LINE__);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(INITCHUNKSIZE) == NULL)
        return -1;
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
    size = size <= DSIZE ? 2 * DSIZE : ALIGN(size + DSIZE);

    if ((ptr = find_fit(size)) == NULL) {
        /* No fit found. Get more memory and place the block */
        if ((ptr = extend_heap(MAX(size, CHUNKSIZE))) == NULL)
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
    insert_block(ptr);
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
    size = size <= DSIZE ? 2 * DSIZE : ALIGN(size + DSIZE);

    /* 1. If target size is smaller than or equal to current size, return the block pointer directly */
    if (size <= GET_SIZE(HDRP(ptr)))
        return ptr;

    /* 2. Target size is bigger than current size */

    /* 2.1. If the next block is the epilogue block, extend the heap directly */
    if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0) {
        /* Extend the heap */
        size_t extend_size = MAX((size - GET_SIZE(HDRP(ptr))), CHUNKSIZE);
        if (extend_heap(extend_size) == NULL)
            return NULL;

        /* Reset current block */
        size_t new_size = GET_SIZE(HDRP(ptr)) + extend_size;
        delete_block(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(new_size, 1));
        PUT(FTRP(ptr), PACK(new_size, 1));
        return ptr;
    }

    /* 2.2. If the next block is a free block, and the size if enough to resize the new block */
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) {
        size_t new_size = GET_SIZE(HDRP(NEXT_BLKP(ptr))) + GET_SIZE(HDRP(ptr));
        if (new_size >= size) {
            delete_block(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(new_size, 1));
            PUT(FTRP(ptr), PACK(new_size, 1));
            return ptr;
        }
    }

    /* 2.3. Allocate a new block with the target size */
    void *new_block = mm_malloc(size);
    memcpy(new_block, ptr, GET_SIZE(HDRP(ptr)));
    mm_free(ptr);
    return new_block;
}

/******************
 * Helper functions
 ******************/

static void *extend_heap(size_t words) {
    char *ptr;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = ALIGN(words);
    if ((ptr = mem_sbrk(size)) == (void *) -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(ptr), PACK(size, 0));           /* Free block header */
    PUT(FTRP(ptr), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));   /* New epilogue header */

    insert_block(ptr);
    /* Coalesce if the previous block was free */
    return coalesce(ptr);
}

static void *coalesce(void *ptr) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc) {
        return ptr;
    } else if (prev_alloc && !next_alloc) {
        delete_block(ptr);
        delete_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        delete_block(PREV_BLKP(ptr));
        delete_block(ptr);
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    } else {
        delete_block(PREV_BLKP(ptr));
        delete_block(ptr);
        delete_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr))) + GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    insert_block(ptr);
    CHECKHEAP(__LINE__);
    return ptr;
}

static void *find_fit(size_t size) {
    void *ptr = NULL;
    size_t search_size = size;

    /* Search the free list for a fit */
    for (int i = 0; i < LISTSIZE; i++) {
        /* Find free list */
        if (((search_size <= 1) && (segregated_free_lists[i] != NULL))) {
            ptr = segregated_free_lists[i];
            /* Find free block */
            while ((ptr != NULL) && ((GET_SIZE(HDRP(ptr)) < size))) {
                ptr = SUCC(ptr);
            }
            if (ptr != NULL)
                break;
        }
        search_size >>= 1;
    }
    return ptr;
}

static void place(void *ptr, size_t size) {
    size_t block_size = GET_SIZE(HDRP(ptr));
    size_t residual = block_size - size;

    delete_block(ptr);
    if (residual >= 2 * DSIZE) {
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(residual, 0));
        PUT(FTRP(ptr), PACK(residual, 0));
        insert_block(ptr);
    } else {
        PUT(HDRP(ptr), PACK(block_size, 1));
        PUT(FTRP(ptr), PACK(block_size, 1));
    }
    CHECKHEAP(__LINE__);
}

static void insert_block(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
    int list_number = 0;
    void *succ_ptr = NULL;
    void *pred_ptr = NULL;

    /* Find the corresponding free list for current block */
    while ((list_number < LISTSIZE - 1) && (size > 1)) {
        size >>= 1;
        list_number++;
    }

    /* Find the insert position for block, keep free list in ascending order */
    succ_ptr = segregated_free_lists[list_number];
    while ((succ_ptr != NULL) && (GET_SIZE(HDRP(succ_ptr)) < size)) {
        pred_ptr = succ_ptr;
        succ_ptr = SUCC(succ_ptr);
    }

    if (succ_ptr != NULL) {
        /* 1. The insert position is neither the beginning nor the end of the list */
        if (pred_ptr != NULL) {
            /* Set current block's predecessor pointer */
            SET_PTR(PRED_PTR(ptr), pred_ptr);
            /* Set current block's successor pointer */
            SET_PTR(SUCC_PTR(ptr), succ_ptr);
            /* Set predecessor block's successor pointer */
            SET_PTR(PRED_PTR(succ_ptr), ptr);
            /* Set successor block's predecessor pointer */
            SET_PTR(SUCC_PTR(pred_ptr), ptr);
        }
            /* 2. The insert position is the beginning of the list */
        else {
            /* Set current block's predecessor pointer */
            SET_PTR(PRED_PTR(ptr), NULL);
            /* Set current block's successor pointer */
            SET_PTR(SUCC_PTR(ptr), succ_ptr);
            /* Set successor block's predecessor pointer */
            SET_PTR(PRED_PTR(succ_ptr), ptr);
            /* Set the beginning pointer of the free list */
            segregated_free_lists[list_number] = ptr;
        }
    } else {
        /* 3. The insert position is the end of the list */
        if (pred_ptr != NULL) {
            /* Set current block's predecessor pointer */
            SET_PTR(PRED_PTR(ptr), pred_ptr);
            /* Set current block's successor pointer */
            SET_PTR(SUCC_PTR(ptr), NULL);
            /* Set successor block's predecessor pointer */
            SET_PTR(SUCC_PTR(pred_ptr), ptr);
        }
            /* 4. The list is empty */
        else {
            /* Set current block's predecessor pointer */
            SET_PTR(SUCC_PTR(ptr), NULL);
            /* Set current block's successor pointer */
            SET_PTR(PRED_PTR(ptr), NULL);
            /* Set the beginning pointer of the free list */
            segregated_free_lists[list_number] = ptr;
        }
    }
}

static void delete_block(void *ptr) {
    int list_number = 0;
    size_t size = GET_SIZE(HDRP(ptr));

    /* Find the corresponding free list for current block */
    while ((list_number < LISTSIZE - 1) && (size > 1)) {
        size >>= 1;
        list_number++;
    }

    if (SUCC(ptr) != NULL) {
        /* 1. The current block is neither the beginning nor the end of the list */
        if (PRED(ptr) != NULL) {
            /* Set predecessor block's successor */
            SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            /* Set successor block's predecessor */
            SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        }
            /* 2. The current block is the beginning of the list */
        else {
            /* Set successor block's predecessor */
            SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
            /* Set the beginning pointer of the free list */
            segregated_free_lists[list_number] = SUCC(ptr);
        }
    } else {
        /* 3. The current block is the end of the list */
        if (PRED(ptr) != NULL) {
            /* Set predecessor block's successor */
            SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
        }
            /* 4. The free list has only one block, the current block */
        else {
            /* Set the beginning pointer of the free list */
            segregated_free_lists[list_number] = NULL;
        }
    }
}

#if DEBUG
/**************************
 * Heap Consistency Checker
 **************************/

void mm_checkheap(int lineno) {
    void *ptr;
    int list_number;

    /* Is every block in the free list marked as free? */
    for (list_number = 0; list_number < LISTSIZE -1; list_number++) {
        for (ptr = segregated_free_lists[list_number]; ptr != NULL; ptr = SUCC(ptr)) {
            if (GET_ALLOC(HDRP(ptr))) {
                printf("Lineno %d: Block %p in the free list is not marked as free\n", lineno, ptr);
                return;
            }
        }
    }

    /* Are there any contiguous free blocks that somehow escaped coalescing? */
    for (list_number = 0; list_number < LISTSIZE -1; list_number++) {
        for (ptr = segregated_free_lists[list_number]; ptr != NULL; ptr = SUCC(ptr)) {
            if (!GET_ALLOC(HDRP(ptr)) && !GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) {
                printf("Lineno %d: Contiguous free blocks escaped coalescing\n", lineno);
                return;
            }
        }
    }

    /* Is every free block actually in the free list? */

    /* Do the pointers in the free list point to valid free blocks? */
    for (list_number = 0; list_number < LISTSIZE -1; list_number++) {
        for (ptr = segregated_free_lists[list_number]; ptr != NULL; ptr = SUCC(ptr)) {
            if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
                printf("Lineno %d: Free block %p points to an invalid heap address\n", lineno, ptr);
                return;
            }
        }
    }

    /* Do any allocated blocks overlap? */

    /* Do the pointers in a heap block point to valid heap addresses? */
}
#endif
