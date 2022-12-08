#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "team31",
    "Taewook Jeong",
    "gotlou8317@gmail.com",
    "Hoyeon Kbak",
    "h0y301v@gmail.com"};

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, value) (*(unsigned int *)(p) = (value))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((void *)(bp) - WSIZE)
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE((void *)(bp) - WSIZE))

/* Given (free)block ptr bp, compute address of its predecessor and successor */
#define PRDP(bp) ((void *)(bp))
#define SUCP(bp) ((void *)(bp) + WSIZE)

/* Given (free)block ptr bp, compute address of next and previous free blocks */
#define PREV_FREE_BLKP(bp) (GET(PRDP(bp)))
#define NEXT_FREE_BLKP(bp) (GET(SUCP(bp)))

static void *heap_listp; // prologue block bp(Root)
static void *extend_heap(size_t);
static void *coalesce(void *);
static void place(void *, size_t);
static void *find_fit(size_t);

static void insert_free(void *);
static void unlink_free(void *);

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                                      // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1));       // Prologue header
    PUT(heap_listp + (2 * WSIZE), heap_listp + (2 * WSIZE)); // Predecessor(Root)
    PUT(heap_listp + (3 * WSIZE), heap_listp + (2 * WSIZE)); // Successor(Root)
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1));       // Prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));               // Epilogue header
    heap_listp += (2 * WSIZE);                               // Move pointer to prologue block bp

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc)
        unlink_free(PREV_BLKP(bp));
    
    if (!next_alloc)
        unlink_free(NEXT_BLKP(bp));

    if (prev_alloc && next_alloc) /* Case 1 */
    {
    }
    else if (prev_alloc && !next_alloc) /* Case 2 */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) /* Case 3 */
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else /* Case 4 */
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_free(bp);
    return bp;
}

void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    size_t asize = ALIGN(size) + 2 * WSIZE;
    size_t extendsize = MAX(asize, CHUNKSIZE);
    void *bp;

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{
    /* First-Fit */
    for (void *bp = NEXT_FREE_BLKP(heap_listp); bp != heap_listp; bp = NEXT_FREE_BLKP(bp))
        if (asize <= GET_SIZE(HDRP(bp)))
            return bp;

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    unlink_free(bp);
    if (csize-asize >= 2 * DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        insert_free(NEXT_BLKP(bp));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

void *mm_realloc(void *oldptr, size_t newsize)
{
    if (oldptr == NULL)
        return mm_malloc(newsize);

    if (newsize == 0)
    {
        mm_free(oldptr);
        return NULL;
    }

    void *newptr = mm_malloc(newsize);
    if (newptr == NULL)
      return NULL;

    size_t oldsize = GET_SIZE(HDRP(oldptr)) - 2 * WSIZE;
    if (newsize < oldsize)
        oldsize = newsize;
    memcpy(newptr, oldptr, oldsize);
    mm_free(oldptr);
    return newptr;
}

/* LIFO ordered explicit free list */
static void insert_free(void *bp)
{
    /* When new free(first) block(bp) is inserted between prologue block ans last first-free block */
    PUT(PRDP(bp), heap_listp);                 // Predecessor of new free block is prologue block.
    PUT(SUCP(bp), NEXT_FREE_BLKP(heap_listp)); // Successor of new free block is last first-free block.
    PUT(PRDP(NEXT_FREE_BLKP(heap_listp)), bp); // Predecessor of last first-free block is new(first) free block.
    PUT(SUCP(heap_listp), bp);                 // Succecssor of prologue block is new(first) free block.
}

static void unlink_free(void *bp)
{
    /* When free block(bp) is unlinked, predecessor(previous free block) and successor(next free block) are linked  */
    PUT(SUCP(PREV_FREE_BLKP(bp)), NEXT_FREE_BLKP(bp));
    PUT(PRDP(NEXT_FREE_BLKP(bp)), PREV_FREE_BLKP(bp));
}