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
    "team06",
    /* First member's full name */
    "Park Chan",
    /* First member's email address */
    "cks537@gmail.com",
    /* Second member's full name (leave blank if none) */
    "Woong Jae",
    /* Second member's email address (leave blank if none) */
    "wjcheon@gmail.com"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) // 사이즈만 떼옴
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))             //

#define DSIZE 8                                                       /*더블워드 사이즈 */
#define WSIZE 4                                                       /*워드 사이즈 */
#define CHUNKSIZE (1 << 9)                                            /* 청크사이즈 : 기본 이닛 힙 사이즈 */
#define MAX(x, y) ((x) > (y) ? (x) : (y))                             /* 맥스함수 정의 */
#define PACK(size, alloc) ((size) | (alloc))                          /* 사이즈랑 얼록 합치기 */
#define GET(p) (*(unsigned int *)(p))                                 /* p주소에 있는 값 받음 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))                    /* p주소에 있는 값을 변경함 */
#define GET_SIZE(p) (GET(p) & ~0x7)                                   /* 사이즈를 받음 = 얼록을 제외해서 */
#define GET_ALLOC(p) (GET(p) & 0x1)                                   /* 얼록만 받음 */
#define HDRP(bp) ((char *)(bp)-WSIZE)                                 /* 헤더위치 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)          /* 푸터위치 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) /* 다음 블록의 주소 = 다음 블록의 헤더 다음 위치 */
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   /* 이전 블록의 주소 = 이전 블록의 헤더 다음 위치 */
static char *heap_listp;
char *next_bp;

static void *find_fit(size_t asize) // first-fit search
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) //
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) // 얼로케이티드가 안 되있고 사이즈가 들어가기에 충분히 크다면
        {
            return bp; // bp 반환
        }
    }
    return NULL;
}

static void *next_fit(size_t asize) // next-fit search
{
    void *bp;

    for (bp = next_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // csize에 bp가 있는 블록의 크기를 넣음

    if ((csize - asize) >= (2 * DSIZE)) // 16 바이트이상의 여유가 있으면
    {
        PUT(HDRP(bp), PACK(asize, 1)); // bp의 헤더에 asize를 넣음
        PUT(FTRP(bp), PACK(asize, 1)); // bp의 푸터에 asize를 넣음
        bp = NEXT_BLKP(bp);            // 다음 블록으로 포인터 이동
        next_bp = bp;
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 나머지를 할당되지 않은 공간으로 만듬
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else // 16바이트이상의 여유가 없으면 현재 블록 크기만큼 할당한다.
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        next_bp = bp;
    }
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록이 얼로케이티드 되있는지
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록이 얼로케이티드 되있는지
    size_t size = GET_SIZE(HDRP(bp));                   // 현재 블록 사이즈를 받음

    if (prev_alloc && next_alloc)
    { // Case 1 : 둘다 얼로케이티드
        next_bp = bp;
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {                                          // Case 2 : 다음 블록만 프리 되있는 상태
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록 사이즈까지 더함
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    {                                          // Case 3 : 전 블록만 프리 되있는 상태
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 전 블록 사이즈까지 더함
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // bp를 전 블록으로 옮김
    }

    else
    {                                                                          // Case 4 : 전 블록 다음 블록 둘다 프리 되있는 상태
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 전 블록 사이즈 다음 블록 사이즈 둘다 더함 사이즈에
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // bp를 전 블록으로 옮김
    }
    next_bp = bp;
    return bp;
}

static void *extend_heap(size_t words) // 힙 사이즈 늘리기
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 짝수면 4를 곱하면 8의 배수가 된다, 홀수면 1을 더하고 4를 곱하면 8의 배수가 된다
    if ((long)(bp = mem_sbrk(size)) == -1)                    // bp를 늘리기 전의 brk위치로 한다. 오류시 널을 반환
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         // 옛 brk에 사이즈만큼의 블록에서의 헤더배정
    PUT(FTRP(bp), PACK(size, 0));         // 옛 brk에 사이즈만큼의 블록에서의 푸터배정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 에필로그 해더위치 배정

    return coalesce(bp); // 두개의 가용 블록을 통합하기 위해 coalesce 함수를 호출 통합된 블록의 블록 포인터를 리턴
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) /* 힙 리스트를 4워드만큼 늘리고 에러가 나면 -1 을 리턴  */
        return -1;
    PUT(heap_listp, 0);                            /* 힙 리스트의 첫번째 값을 0으로 unused */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* 첫번째 워드를 8/1 배정 프롤로그 블록(헤더) */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* 두번째 워드를 8/1 배정 프롤로그 블록(푸터) */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* 세번째 워드를 0/1 배정 에필로그 블록 */
    heap_listp += (2 * WSIZE);                     /* 힙리스트 포인터 위치를 2워드만큼 앞으로 프롤로그의 헤더 다음값 */

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 힙을 chunksize/4 만큼 늘려준다
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE); // 최소사이즈 8
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if ((bp = next_fit(newsize)) != NULL)
    {
        place(bp, newsize); //헤더 푸터를 플레이스함
        return bp;
    }

    extendsize = MAX(newsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    return bp;
}

void *mm_malloc_find_fit(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE); // 최소사이즈 8
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if ((bp = find_fit(newsize)) != NULL)
    {
        place(bp, newsize); //헤더 푸터를 플레이스함
        return bp;
    }

    extendsize = MAX(newsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp)); // bp의 사이즈를 사이즈에 넣는다

    PUT(HDRP(bp), PACK(size, 0)); // 헤드에 얼로케이티드를 해제시킴
    PUT(FTRP(bp), PACK(size, 0)); // 푸터역시 얼로케이티드를 해제시킴
    coalesce(bp);                 // 이을거 있으면 이음
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    copySize = GET_SIZE(HDRP(ptr));
    if (size == copySize)
        return oldptr;
    // if (size < copySize)
    // {
    //     mm_free(oldptr);
    //     char *new_bp = mm_malloc(size);
    //     memcpy(new_bp, oldptr, size);
    //     return oldptr;
    // }
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}