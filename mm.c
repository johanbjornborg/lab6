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

#define DEBUG



#include "mm.h"
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

#define WSIZE 4
#define DSIZE 8
#define OVERHEAD 8
#define EXPANDBLOCK (1<<10)

#define MAX( x, y ) ( (x) > (y) ? (x) : (y) )

#define PACK( size, alloc ) ( (size) | (alloc) )

#define GET( p )     ( *(size_t *)(p) )
#define PUT( p,val ) ( *(size_t *)(p) = (val) )

#define GET_ALLOC( p ) ( GET(p) & 0x1 )
#define GET_SIZE( p ) ( GET(p) & ~0x7 )

#define FTRP( bp ) ( (char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE )
#define HDRP( bp ) ( (char *)(bp) - WSIZE )

#define NEXT_BLKP( bp ) ( (char *)(bp) + GET_SIZE(((char *)(bp) -WSIZE)) )
#define PREV_BLKP( bp ) ( (char *)(bp) - GET_SIZE(((char *)(bp) -DSIZE)) )

static char * heap_ptr;

static void *expand_heap( size_t max_needed );
static void *coalesce( void *bp );
static void *find_space( size_t size );
static void place( void *bp, size_t size );

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if( (heap_ptr = mem_sbrk(4*WSIZE) ) == NULL )
	    return -1;
 
    PUT( heap_ptr, 0 );
    PUT( heap_ptr+WSIZE, PACK(OVERHEAD, 1) );
    PUT( heap_ptr+DSIZE, PACK(OVERHEAD, 1) );
    PUT( heap_ptr+WSIZE+DSIZE, PACK(0, 1) ); 
    heap_ptr += DSIZE;
    
    char *bp;
    bp = expand_heap(EXPANDBLOCK/WSIZE);
    if(bp == NULL)
	    return -1;
    
    return 0;
}

static void *expand_heap(size_t max_needed)
{
    char *bp;
    size_t size;
    
    size = (max_needed % 2) ? (max_needed+1) * WSIZE : max_needed * WSIZE;
    
    bp = mem_sbrk(size);
    if(bp == (void *)-1)
	    return NULL;
    
    PUT( HDRP(bp), PACK(size, 0) );
    PUT( FTRP(bp), PACK(size, 0) );
    PUT( HDRP(NEXT_BLKP(bp)), PACK(0, 1) );

    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit
    char *bp;      

    if (heap_ptr == 0)
		mm_init();

    // Ignore spurious requests
    if (size == 0)
		return NULL;

    // Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE)     
		asize = 2*DSIZE;    
    else
		asize = DSIZE * ( (size + (DSIZE) + (DSIZE-1)) / DSIZE ); 

    // Search the free list for a fit
    if ( (bp = find_space(asize)) != NULL ) 
    {  
		place(bp, asize);              
		return bp;
    }

    // No fit found. Get more memory and place the block
    extendsize = MAX(asize,EXPANDBLOCK); 
    if ( (bp = expand_heap(extendsize/WSIZE)) == NULL )  
		return NULL;                                  
    place(bp, asize);    
    
    return bp;
}

/*
 * Allocates a given block of memory in the heap with size var, and starting at
 * the location of pointer bp.
 */
static void place(void *bp, size_t size)
{
    size_t current_size = GET_SIZE(HDRP(bp));

    if( current_size >= DSIZE + OVERHEAD + size ) //If the size of free block >= needed size
    {
		PUT( HDRP(bp), PACK(size, 1) );//Allocate the block
		PUT( FTRP(bp), PACK(size, 1) );
		bp = NEXT_BLKP(bp);
		PUT( HDRP(bp), PACK(current_size-size, 0) );//Update the size of the remaining free block
		PUT( FTRP(bp), PACK(current_size-size, 0) );
    }
    else //If size of free block is less than needed size
    {	
		PUT( HDRP(bp), PACK(current_size, 1) );
		PUT( FTRP(bp), PACK(current_size, 1) );
    }
}

/*
 * Loops through all blocks in the heap both free and allocated and returns a pointer
 * to the first block that is both free and greater than or equal to the size of 
 * the requested space. If no such free block exists, null is returned.
 */
static void *find_space(size_t size)
{
    void *bp;
    
    for( bp = heap_ptr; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp) )
    {
		if( !GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp))) )
		{
	    	return bp;
		}
    }
    return NULL;
}


/*
 * mm_free - Freeing a block changes the alloc value in the header and footer
 * to 0. Everytime a block is freed, coalesce is called to combine any neighboring
 * blocks.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT( HDRP(bp), PACK(size,0) );
    PUT( FTRP(bp), PACK(size,0) );
    coalesce(bp);
}

/*
 *	Combines neighboring free blocks and places the combined size in a new
 * 	header and footer.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC( FTRP(PREV_BLKP(bp)) );//is the previous block free
    size_t next_alloc = GET_ALLOC( HDRP(NEXT_BLKP(bp)) );//is the next block free
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) //Case 1: Neither free
    {
		return bp;
	}

    else if(prev_alloc && !next_alloc) //Case 2: Only next is free
    {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));//Combines sizes
		PUT(HDRP(bp), PACK(size, 0));//Replaces header and footer
		PUT(FTRP(bp), PACK(size, 0));
		return(bp);
    }
    
    else if(!prev_alloc && next_alloc)//Case 3: Only previous is free
    {
		size += GET_SIZE( HDRP(PREV_BLKP(bp)) );
		PUT( FTRP(bp), PACK(size, 0) );
		PUT( HDRP( PREV_BLKP(bp)), PACK(size, 0) );
		return(PREV_BLKP(bp));
    }

    else //Case 4: Both are free
    {								
		size += GET_SIZE( HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)) );
		PUT(HDRP( PREV_BLKP(bp)), PACK(size, 0) );
		PUT(FTRP( NEXT_BLKP(bp)), PACK(size, 0) );
		return(PREV_BLKP(bp));
    }
}

/*
 * mm_realloc - Implemented for the following cases:
 * 		1) ptr is null: calls malloc for the given size
 * 		2) size is 0: calls free at the location of the given ptr
 * 		3) the new size is less than the old size:
 * 			truncates the currently allocated memory and places it back at 
 * 			the same place, then frees the leftover space
 * 		4) the new size is greater than the old size:
 * 			runs malloc on the new size and moves the currently allocated
 * 			to the new larger block, then frees the old block
 * 		5) the new size is equal to the old size:
 * 			returns the pointer and does nothing
 */
void *mm_realloc(void *ptr, size_t adj_size)
{
	void *oldptr = ptr;
	size_t copySize = GET_SIZE(((char *)oldptr - WSIZE));
	size_t size;
	void *newptr;
	
	if(ptr == NULL) //Case 1
	{				
		return mm_malloc(adj_size);
	}
	else if(adj_size == 0) //Case 2
	{			
		mm_free(ptr);
		return ptr;
	}	
	else
	{
		if(adj_size <= DSIZE)
		{		
			size = DSIZE + OVERHEAD; //Alligns the new size to a multiple of 8
		}
    	else
    	{
			size = DSIZE * ((adj_size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
		}
	
		if(copySize > size) //Case 3
		{		
			memmove(oldptr, oldptr, size);//replace with truncated version of block
			newptr = NEXT_BLKP(oldptr);	//pointer to the remaining memory to be freed
			mm_free(newptr);
			return oldptr;
		}
		else if(copySize < size) //Case 4
		{	
			newptr = mm_malloc(size);
			if(newptr == NULL) //If we cant allocate
			{		
				return NULL;
			}
    		memmove(newptr, oldptr, size);//move to newly allocated memory
    		mm_free(oldptr);
    		return newptr;
		}
		else //Case 5
			return oldptr;
	}
}




//Test if the footer pointer exists at the size from the base pointer
static inline int check_size(char* bp){
	return HDRP(bp) == FTRP(bp);
}

int check_heap(){ 
	int result = 1;
	
	//check the freelist
	int heapListSize = 0;
	char * cur;
	cur = heap_ptr;
	while(cur != 0)//end of list
	{
		++heapListSize;
		
		if(!check_size(cur))
		{
			printf("Error: footer not at expected location");
			result = 0;
		}
	}
	
	//check the continuous blocks
	cur = heap_ptr;
	int adjacentListSize = 0;
	//bool lastFree = 1;
	while( GET_SIZE(HDRP(cur)) != 0 && GET_ALLOC(HDRP(cur)) != 1 ) //end of adjacency list
	{
		++adjacentListSize;
		cur = NEXT_BLKP(cur);
	}
	
	return result; 
}
