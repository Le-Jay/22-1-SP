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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20170624",
    /* Your full name*/
    "Jehyun Lee",
    /* Your email address */
    "jlesl97@naver.com",
};



#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MINSIZE 16
/* single word (4) or double word (8) alignment */
//#define ALIGNMENT 8 //DSIZE

#define LISTS     20      /* Number of segregated lists */
#define BUFFER  (1<<7)    /* Reallocation buffer */

//for size와 할당 여부 하나로 합치는 용도.
#define PACK(size,alloc) ((size)|(alloc))

/* rounds up to the nearest multiple of ALIGNMENT */
//#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


//#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y)? (x) : (y))  
#define MIN(x, y) ((x) < (y)? (x) : (y))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Header, Footer의 pointer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp

/* Address of free block's predecessor and successor entries. */
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

/* Address of free block's predecessor and successor on the segregated list */
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

/* p가 가리키는 memory에 ptr을 입력. */
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

void *free_lists[LISTS];

static void *extend_heap(size_t words);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
//static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);




int mm_init(void)
{
    int list;
    char *heap_startp; //heap의 시작부분의 pointer
    
    //초기화
    for(list = 0; list < LISTS ; list++) {
        free_lists[list] = NULL;
    }
    if((long)(heap_startp = mem_sbrk(4 * WSIZE)) == -1) return -1;
    
    PUT(heap_startp, 0);
    PUT(heap_startp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_startp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_startp + (3 * WSIZE), PACK(0, 1));
    heap_startp += (2 * WSIZE);
    
    if(extend_heap(CHUNKSIZE) == NULL) return -1;

    return 0;
}

static void *extend_heap(size_t size)
{
    void *ptr;                   /* Pointer to newly allocated memory */
    size_t words = size / WSIZE; /* Size of extension in words */    // ! //
    size_t asize;                /* Adjusted size */
    
    asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(ptr = mem_sbrk(asize)) == -1)
        return NULL;
    
    /* Set headers and footer */
    PUT(HDRP(ptr), PACK(asize, 0));   /*header */
    PUT(FTRP(ptr), PACK(asize, 0));   /*footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); /*header */

    insert_node(ptr, asize);
    
    return coalesce(ptr);
}

static void insert_node(void *ptr, size_t size)
{
    int idx = 0;
    void *search_ptr = ptr;
    void *insert_ptr = NULL; //node가 삽입되는곳.

    while ((idx < LISTS - 1) && (size > 1)) {
        size >>= 1;
        idx++;
    }
    search_ptr = free_lists[idx];
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr)))) {
        insert_ptr = search_ptr;
        search_ptr = PRED(search_ptr);
    }
    if (search_ptr != NULL) {
        if (insert_ptr != NULL) {
        SET_PTR(PRED_PTR(ptr), search_ptr); 
        SET_PTR(SUCC_PTR(search_ptr), ptr);
        SET_PTR(SUCC_PTR(ptr), insert_ptr);
        SET_PTR(PRED_PTR(insert_ptr), ptr);
        }
        else {
        SET_PTR(PRED_PTR(ptr), search_ptr); 
        SET_PTR(SUCC_PTR(search_ptr), ptr);
        SET_PTR(SUCC_PTR(ptr), NULL);
        free_lists[idx] = ptr; /* Add block to appropriate list */
        }
    } else {
        if (insert_ptr != NULL) {
        SET_PTR(PRED_PTR(ptr), NULL);
        SET_PTR(SUCC_PTR(ptr), insert_ptr);
        SET_PTR(PRED_PTR(insert_ptr), ptr);
        } 
        else {
        SET_PTR(PRED_PTR(ptr), NULL);
        SET_PTR(SUCC_PTR(ptr), NULL);
        free_lists[idx] = ptr; /* Add block to appropriate list */
        }
    }
    return;
}

static void delete_node(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    int idx = 0;

    while((size > 1) && (idx < LISTS - 1)) {
        size >>= 1;
        idx++;
    }
    if(PRED(ptr) != NULL) {
        //prev, succ 모두 allocated 일때
        if(SUCC(ptr) != NULL) {
            SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        }
        else {
            SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
            free_lists[idx] = PRED(ptr);
        }
    }
    else {
        //prev가 allocated, succ가 free일때
        if(SUCC(ptr) != NULL)
            SET_PTR(PRED_PTR(SUCC(ptr)), NULL); //prev의 뒤 ptr을 null로.
        else
            free_lists[idx] = NULL;
    }
    return;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   

    delete_node(bp); //!!!!!

    if ((csize - asize) >= (2 * DSIZE)) { 
	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));

	PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
    insert_node(NEXT_BLKP(bp), (csize - asize));   // ! //
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}



static void *coalesce(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if(prev_alloc && next_alloc)  //모두 free x-> 합칠 필요x
        return bp;
    if(!prev_alloc && !next_alloc) { //앞뒤블록 모두 free
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else if(!prev_alloc && next_alloc) { //앞 블록만 free
        delete_node(bp);
        delete_node(PREV_BLKP(bp));  
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else if(prev_alloc && !next_alloc) { //뒤 블록 free
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }

    //합쳐진경우 -> free block의 위치를 추가.
    insert_node(bp, size);
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *ptr = NULL;
    int list = 0;
    if (size == 0) return NULL;

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }


    size = asize;
    while (list < LISTS) {
        if ((list == LISTS - 1) || ((size <= 1) && (free_lists[list] != NULL))) {
        ptr = free_lists[list];
        // Ignore blocks that are too small
        while ((ptr != NULL) && ((asize > GET_SIZE(HDRP(ptr)))))
            ptr = PRED(ptr);

        if (ptr != NULL) break;
        }
        size >>= 1;
        list++;
    }

        /* extend the heap if no free blocks of sufficient size are found */
    if (ptr == NULL) {
        extendsize = MAX(asize, CHUNKSIZE);
        if ((ptr = extend_heap(extendsize)) == NULL) return NULL;
    }
    place(ptr, asize);
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    /* boundary tags 내에서 allocation status를 조정. */
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    
    insert_node(bp,size);

    /* free된 block들을 합침. */
    coalesce(bp);
    return;
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}