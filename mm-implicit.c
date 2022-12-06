#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "team31",
    /* First member's full name */
    "Taewook Jeong",
    /* First member's email address */
    "gotlou8317@gmail.com",
    /* Second member's full name (leave blank if none) */
    "Hoyeon Kbak",
    /* Second member's email address (leave blank if none) */
    "h0y301v@gmail.com"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // In this case, this is 8.

/* Basic constancs and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes), 1<<12 == 4096(4KB) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))                  // read a word
#define PUT(p, value) (*(unsigned int *)(p) = (value)) // write a word

/* Read the size and allocated fields from address p(header or footer) */
#define GET_SIZE(p) (GET(p) & ~0x7) // read the size of block
#define GET_ALLOC(p) (GET(p) & 0x1) // read the allocated block(if p is alocated, 1 else 0)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)                      // pointer to header of block
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // pointer to footer of block

/* Given block ptr bp, compute address of next and previous blocks */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // pointer to previous block
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) // pointer to next block

static char *heap_listp; // prologue block bp(None)

static void *extend_heap(size_t);
static void *coalesce(void *);
static void place(size_t *bp, size_t size);
static void *find_fit(size_t size);
static void *bp_before; // For next-fit


/*
mm init: Before calling mm malloc mm realloc or mm free, the application program
(i.e., the trace-driven driver program that you will use to evaluate your implementation)
calls mm init to perform any necessary initializations, such as allocating the initial heap area.
The return value should be -1 if there was a problem in performing the initialization, 0 otherwise.
*/
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    /* An initial free block(4 * WSIZE) */
    PUT(heap_listp, 0);                            // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);                     // Move pointer to prologue block bp(None)

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) // bp: points to last byte of heap(before extended, old mem_brk)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    /* Coalesce if the previous block was fress */
    return coalesce(bp);
}

/*
The mm malloc routine returns a pointer to an allocated block payload of at least size bytes.
The entire allocated block should lie within the heap region and should not overlap with any other allocated chunk.
We will comparing your implementation to the version of malloc supplied in the standard C library (libc).
Since the libc malloc always returns payload pointers that are aligned to 8 bytes,
your malloc implementation should do likewise and always return 8-byte aligned pointers.
*/
void *mm_malloc(size_t size)
{
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    size_t asize = ALIGN(size) + 2 * WSIZE;    // Adjusted block size to include overhead(hdr, ftr) and alignment reqs.
    size_t extendsize = MAX(asize, CHUNKSIZE); // Amount to extend heap if no fit
    char *bp;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void place(size_t *bp, size_t asize)
{
    size_t fsize = GET_SIZE(HDRP(bp)) - asize; // Size of splitted free block(remainder)
    PUT(HDRP(bp), PACK(asize, 1));             // Place header and footer of new allocated block
    PUT(FTRP(bp), PACK(asize, 1));

    if (0 < fsize) // There is free remaining block.
    {
        PUT(HDRP(NEXT_BLKP(bp)), PACK(fsize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(fsize, 0));
    }
}

static void *find_fit(size_t asize)
{
    /* First-Fit */
    // void *bp = heap_listp + 2 * WSIZE; // First block pointer in free list(next block of prologue block)

    // while (GET((HDRP(bp))) != 0x1)     // Checking block(bp) is in heap(if word in bp block header is 0x1, it means epilogue header.)
    // {
    //     if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp))) // If bp(block) is free and fitting, return bp.
    //         return bp;
    //     bp = NEXT_BLKP(bp);
    // }

    // return NULL; // If there isn't fitting free block.

    /* Next-Fit */
    void *bp_start = heap_listp + 2 * WSIZE;

    if (bp_before == NULL) // Initialization of static variable bp_before
        bp_before = bp_start;

    void *bp = bp_before;

    do // Travering all the block in heap to find fitting block from right before allocatied block
    {
        if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp)))
        {
            bp_before = bp;
            return bp;
        }

        if (GET(HDRP(NEXT_BLKP(bp))) == 0x1)
            bp = bp_start;
        else // If next block isn't(header is epilogue block), traverse from initial block in heap to bp_before(last allocated block)
            bp = NEXT_BLKP(bp);

    } while (bp != bp_before);

    return NULL;

    /* Best-Fit */
    // void *bp = heap_listp + 2 * WSIZE;
    // void *bp_best = bp;
    // size_t size_best = 20 * (1 << 20); // MAX_HEAP(20MB)

    // while (GET((HDRP(bp))) != 0x1)
    // {
    //     if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp)))
    //         if (GET_SIZE(HDRP(bp)) < size_best)
    //         {
    //             bp_best = bp;
    //             size_best = GET_SIZE(HDRP(bp));
    //         }
    //     bp = NEXT_BLKP(bp);
    // }

    // if (size_best == 20 * (1 << 20))
    //     return NULL;
    // else
    //     return bp_best;

    /* Worst-Fit */
    // void *bp = heap_listp + 2 * WSIZE;
    // void *bp_worst = bp;
    // size_t size_worst = 0;

    // while (GET((HDRP(bp))) != 0x1)
    // {
    //     if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp)))
    //         if (GET_SIZE(HDRP(bp)) > size_worst)
    //         {
    //             bp_worst = bp;
    //             size_worst = GET_SIZE(HDRP(bp));
    //         }
    //     bp = NEXT_BLKP(bp);
    // }

    // if (size_worst == 0)
    //     return NULL;
    // else
    //     return bp_worst;
}

/*
The mm free routine frees the block pointed to by ptr. It returns nothing.
This routine is only guaranteed to work when the passed pointer (ptr) was returned
by an earlier call to mm malloc or mm realloc and has not yet been freed.
*/
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    bp = coalesce(bp); // If previous(left) or next(right) block is free, coalesce with them
    if (bp < bp_before && bp_before < NEXT_BLKP(bp)) // For next-fit
        bp_before = bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) /* Case 1 */
    {
        return bp;
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
    return bp;
}

/*
mm realloc: The mm realloc routine returns a pointer to an allocated region of at least size bytes with the following constraints.
– if ptr is NULL, the call is equivalent to mm malloc(size);
– if size is equal to zero, the call is equivalent to mm free(ptr);
– if ptr is not NULL, it must have been returned by an earlier call to mm malloc or mm realloc.

The call to mm realloc changes the size of the memory block pointed to by ptr (the old block) to size bytes and returns the address of the new block. 
Notice that the address of the new block might be the same as the old block, or it might be different, depending on your implementation,
the amount of internal fragmentation in the old block, and the size of the realloc request.

The contents of the new block are the same as those of the old ptr block, up to the minimum of the old and new sizes. Everything else is uninitialized.
For example, if the old block is 8 bytes and the new block is 12 bytes, then the first 8 bytes of the new block are identical to the first 8
*/
void *mm_realloc(void *oldptr, size_t newsize)
{
    if (oldptr == NULL)
        return mm_malloc(newsize);

    if (newsize == 0)
    {
        mm_free(oldptr);
        return NULL;
    }

    newsize = ALIGN(newsize);                    // Size of payload(aligned)
    size_t newsize_blk = newsize + 2 * WSIZE;    // Size of block(already-aligned)

    size_t oldsize_blk = GET_SIZE(HDRP(oldptr)); // Size of block(already-aligned)
    size_t oldsize = oldsize_blk - 2 * WSIZE;    // Size of payload(already-aligned)

    if (newsize == oldsize) // Case 1
        return oldptr;
    else if (newsize < oldsize) // Case 2
    {
        place(oldptr, newsize_blk);
        coalesce(NEXT_BLKP(oldptr)); // If next to the remaining block is free, two blocks should be coalesced.
        return oldptr;
    }
    else if (GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) == 0) // Case 3. When next block is free and old + next block size is larger than
    {
        size_t nextsize_blk = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        if (newsize_blk <= oldsize_blk + nextsize_blk)
        {
            PUT(HDRP(oldptr), PACK(oldsize_blk + nextsize_blk, 0)); // For using new(old+next) block size at place function
            place(oldptr, newsize_blk);
            return oldptr;
        }
    }

    /* When unable to find free space at block itself and next block, just use mm_malloc and mm_free function */
    void *newptr = mm_malloc(newsize);
    if (newptr == NULL)
        return NULL;
    mm_free(oldptr);
    memcpy(newptr, oldptr, oldsize);
    return newptr;
}