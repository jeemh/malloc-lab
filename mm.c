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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "fsaf",
    /* Second member's email address (leave blank if none) */
    "asdfasd"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 // word 크기(byte 단위)
#define DSIZE 8 // double word 크기(byte 단위)
#define CHUNKSIZE (1<<12) // 2^10 * 2^2 (byte 단위) -> 4KB

#define MAX(x,y) ((x)>(y)) ? (x) : (y) // x,y 중 큰 값

#define PACK(size,alloc) ((size) | (alloc)) // size 와 할당여부

#define GET(p) (*(unsigned int *)p) // p가 가리키는 값
#define PUT(p,val) (*(unsigned int *)(p) = (val)) // p가 가리키는 값을 val로 설정

#define GET_SIZE(p) (GET(p) & ~0x7) // ~0x7 -> ...11111000 뒤 세자리 비트 외 넷째자리 비트부터 size 판독
#define GET_ALLOC(p) (GET(p) & 0x1) // 뒤 세자리 비트가 001인지 000인지 판독

#define HDRP(bp) ((char*)(bp) - WSIZE) // bp보다 WSIZE 앞 -> 헤더 부분
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // bp의 현재블럭 크기 - 헤더 크기(WSIZE, bp위치가 헤더 시작부분이 아니라 헤더 끝 부분이기 때문) - 푸터 크기(WSIZE) -> 푸터 부분

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //bp - WSIZE -> bp의 헤더, bp의 현재 블럭 크기를 더해서 다음 블럭으로 이동
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //bp - DSIZE -> bp의 이전 블럭의 푸터, bp의 이전 블럭 크기를 빼서 이전 블럭으로 이동

/* 
 * mm_init - initialize the malloc package.
 */
void *heap_listp;

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) {
        return bp;
    } else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    } else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE; //DSIZE 단위로 설정
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

int mm_init(void)
{
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp + 1*WSIZE, PACK(DSIZE,1));
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE,1));
    PUT(heap_listp + 3*WSIZE, PACK(0,1));
    heap_listp += 2*WSIZE;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *find_fit(size_t asize){
    char *bp = NEXT_BLKP(heap_listp);
    while(GET_SIZE(HDRP(bp)) > 0){
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
            return bp;
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if(csize - asize >= DSIZE + WSIZE*2){
        PUT(HDRP(bp), PACK(asize,1));
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize,0));
        PUT(FTRP(bp), PACK(csize - asize,0));
    }else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *ptr;

    /* 의미 없는 요청 처리 안함 */
    if (size == 0)
    {
        return NULL;
    }
    //만약 payload에 넣으려고하는 데이터가 2byte라면 header(4byte) + footer(4byte) + payload(2byte) = 10byte 이지만,
    //더블워드 정렬 조건을 충족시키기 위해서 패딩 6byte를 추가해야한다. 따라서 총 16byte의 블록이 만들어지는데 이 과정을 계산하는 식이 아래 식이다.
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    // 가용 블록을 가용리스트에서 검색하고 할당기는 요청한 블록을 배치한다.
    if ((ptr = find_fit(asize)) != NULL)
    {
        place(ptr, asize);
        return ptr;
    }

    /* 리스트에 들어갈 수 있는 free 리스트가 없을 경우, 메모리를 확장하고 블록을 배치한다 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(ptr, asize);

    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 
- 만약 ptr == NULL 이면, `mm_malloc(size)`과 동일한 동작을 수행한다.
- 만약 size가 0 이면, `mm_free(ptr)`와 동일한 동작을 수행한다.
- 만약 ptr != NULL 이면, 이전에 `mm_malloc()` 또는 `mm_realloc()`을 이용해 반환값이 존재하는 상태라고 볼 수 있다.
  이때 `mm_realloc()`을 호출하면, ptr이 가르키는 메모리 블록(이전 블록)의 메모리 크기가 바이트 단위로 변경된다. 이후 새 블록의 주소를 return 한다.
  새블록의 크기는 이전 ptr 블록의 크기와 최소한의 크기까지는 동일하고, 그 이외의 범위는 초기화되지 않는다. 예를 들어 이전 블록이 8바이트이고, 새 블록이 12바이트인 경우 첫 8바이트는 이전 블록과 동일하고 이후 4바이트는 초기화되지 않은 상태임을 의미한다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    } 

    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    size_t csize = GET_SIZE(HDRP(ptr));
    if (size < csize) { // 재할당 요청에 들어온 크기보다, 기존 블록의 크기가 크다면
        csize = size; // 기존 블록의 크기를 요청에 들어온 크기 만큼으로 줄인다.
    }
    memcpy(new_ptr, ptr, csize); // ptr 위치에서 csize만큼의 크기를 new_ptr의 위치에 복사함
    mm_free(ptr); // 기존 ptr의 메모리는 할당 해제해줌
    return new_ptr;
}















