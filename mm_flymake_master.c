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

#include "mm_flymake.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Systematic Burnout",
    /* First member's full name */
    "Thomas Kline",
    /* First member's email address */
    "Tombo890@gmail.com",
    /* Second member's full name (leave blank if none) */
    "John Wells",
    /* Second member's email address (leave blank if none) */
    "JohnCWells86@gmail.com"
};
/* Define several constants and inline methods */
#define WSIZE 4
#define CHUNKSIZE ( 1<<12 )

#define MAX( x, y ) ( (x) > (y)? (x) : (y) )  

#define PACK( size, alloc ) ( (size) | (alloc) )
#define PUT( p, val ) ( *(size_t *)(p) = (val) )
#define GET( p ) ( *(size_t *)(p) )
#define GET_SIZE( p ) ( GET(p) & ~0x7 )
#define GET_ALLOC( p ) ( GET(p) & 0x1 )

#define HDRP( bp ) ( (char *)(bp) + GET_SIZE( ((char *)(bp) - WSIZE)) )
#define FTRP( bp ) ( (char *)(bp) + GET_SIZE( ((char *)(bp) - ALIGNMENT)) )
#define NEXT_BLKP( bp ) ( (char *)(bp) + GET_SIZE(((char *)(bp) -WSIZE)) )
#define PREV_BLKP( bp ) ( (char *)(bp) - GET_SIZE(((char *)(bp) -ALIGNMENT)) )

#define bool int
#define true 1
#define false 0

static char* heapPtr;
static bool firstFitDone = false;

static void *expand_heap( size_t words );
static void *coalesce( void *blkP );
static void *find_fit( size_t aSize );
static void place( void *blkP, size_t aSize );

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	// Create empty heap
	if( (heapPtr = mem_sbrk(4*WSIZE)) == NULL )
	return -1;
	
	PUT( heapPtr, 0 ); //Padding
	PUT( heapPtr + WSIZE, PACK(ALIGNMENT, 1) ); //Prologue Header
	PUT( heapPtr + ALIGNMENT, PACK(ALIGNMENT, 1) ); //Prologue Footer
	PUT( heapPtr + WSIZE + ALIGNMENT, PACK(0, 1) ); //Epilogue Header
	heapPtr += ALIGNMENT;
	
	if( expand_heap(CHUNKSIZE/WSIZE) == NULL )
		return -1;
	
    return 0;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *expand_heap(size_t words)
{
	char *blkP;
	size_t size;
	
	//Allocate an even number of words to maintain alignment
	size = ( words % 2 ) ? ( words + 1 ) * WSIZE : words * WSIZE;
	
	blkP = mem_sbrk(size);
	if(blkP == (void *)-1 )
		return NULL;
		
	PUT( HDRP(blkP), PACK(size, 0) ); //Initialize Header
	PUT( FTRP(blkP), PACK(size, 0) ); //Initialize Footer
	PUT( HDRP(NEXT_BLKP(blkP)), PACK(0, 1) ); //New Epilogue Header
	
	//Coalesce if the previous block was free
	return coalesce(blkP);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *blkP) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(blkP)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(blkP)));
    size_t size = GET_SIZE(HDRP(blkP));

    if (prev_alloc && next_alloc) {            //Case 1
	return blkP;
    }

    else if (prev_alloc && !next_alloc) {      //Case 2
	size += GET_SIZE(HDRP(NEXT_BLKP(blkP)));
	PUT(HDRP(blkP), PACK(size, 0));
	PUT(FTRP(blkP), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      //Case 3
	size += GET_SIZE(HDRP(PREV_BLKP(blkP)));
	PUT(FTRP(blkP), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(blkP)), PACK(size, 0));
	blkP = PREV_BLKP(blkP);
    }

    else {                                     //Case 4
	size += GET_SIZE(HDRP(PREV_BLKP(blkP))) + 
	    GET_SIZE(FTRP(NEXT_BLKP(blkP)));
	PUT(HDRP(PREV_BLKP(blkP)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(blkP)), PACK(size, 0));
	blkP = PREV_BLKP(blkP);
    }
    
    // Make sure the heapPtr isn't pointing into the free block
    // that we just coalesced
    if ((heapPtr > (char *)blkP) && (heapPtr < NEXT_BLKP(blkP))) 
	heapPtr = blkP;
	
    return blkP;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t aSize;      //Adjusted block size
    size_t extendSize; //Amount to extend heap if it doesn't fit
    char *blkP;      

    if (heapPtr == 0){
	mm_init();
    }
    
    //Ignore empty requests
    if (size == 0)
	return NULL;

    //Adjust block size to include overhead and alignment reqs
    if (size <= ALIGNMENT) 
	aSize = 2*ALIGNMENT; 
	
    else
	aSize = ALIGNMENT * ( (size + (ALIGNMENT) + (ALIGNMENT-1)) / ALIGNMENT );

    //Search the free list for a fit
    if ((blkP = find_fit(aSize)) != NULL) {
	place(blkP, aSize);
	return blkP;
    }

    //No fit found. Get more memory and place the block
    extendSize = MAX(aSize,CHUNKSIZE);                 
    if ((blkP = expand_heap(extendSize/WSIZE)) == NULL)  
	return NULL;                                 
    place(blkP, aSize);
    return blkP;
}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t aSize)
{
	if(!firstFitDone)
	{
		firstFitDone = true;
		//First fit search
    	void *blkP;

    	for (blkP = heapPtr; GET_SIZE(HDRP(blkP)) > 0; blkP = NEXT_BLKP(blkP)) 
			if (!GET_ALLOC(HDRP(blkP)) && (aSize <= GET_SIZE(HDRP(blkP)))) 
	    		return blkP;
	    		
    	return NULL; //No fit
    }
    
    else
    {
    	//Next fit search
    	char *prevHeapPtr = heapPtr;

    	//Search from the heapPtr to the end of list
    	for ( ; GET_SIZE(HDRP(heapPtr)) > 0; heapPtr = NEXT_BLKP(heapPtr))
			if (!GET_ALLOC(HDRP(heapPtr)) && (aSize <= GET_SIZE(HDRP(heapPtr))))
	    		return heapPtr;

    	//Search from start of list to old rover
    	for (heapPtr = heapPtr; heapPtr < prevHeapPtr; heapPtr = NEXT_BLKP(heapPtr))
			if (!GET_ALLOC(HDRP(heapPtr)) && (aSize <= GET_SIZE(HDRP(heapPtr))))
	    		return heapPtr;
	}
    return NULL;  //no fit found

    
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *blkP, size_t aSize)
{
    size_t cSize = GET_SIZE(HDRP(blkP));   

    if ((cSize - aSize) >= (2*ALIGNMENT)) 
    { 
		PUT(HDRP(blkP), PACK(aSize, 1));
		PUT(FTRP(blkP), PACK(aSize, 1));
		blkP = NEXT_BLKP(blkP);
		PUT(HDRP(blkP), PACK(cSize-aSize, 0));
		PUT(FTRP(blkP), PACK(cSize-aSize, 0));
    }
    else 
    { 
		PUT(HDRP(blkP), PACK(cSize, 1));
		PUT(FTRP(blkP), PACK(cSize, 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}













