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

#define WSIZE           4                               // 1개의 word 사이즈
#define DSIZE           8                               // 2배의 word 사이즈. double word
#define CHUNKSIZE       (1 << 12)               // heap을 한번 늘릴때 필요한 size. 4kb로 설정.

#define MAX(x, y) ((x) > (y) ? (x) : (y))       // 크기 비교

// size를packing하고, 개별 word 안에 bit를 할당.(size와 alloc을 비트 연산.) 헤더에서 사용
#define PACK(size, alloc) ((size) | (alloc))    // 블록 헤더에 사이즈와 할당 여부를 넣어서 체크

#define GET(p)                  (*(unsigned int *)(p))                  // 블록에 담긴 값을 읽어옴.
#define PUT(p, val)             (*(unsigned int *)(p) = (val))  // 특정 위치에 값을 저장.

#define GET_SIZE(p)             (GET(p) & ~0x7)                                 // 해당 블록의 size를 읽어옴.
#define GET_ALLOC(p)    (GET(p) & 0x1)                                  // 해당 블록의 할당 여부를 받아옴.

#define HDRP(bp)                ((char *)(bp) - WSIZE)                  // 헤더위치를 읽어옴.
#define FTRP(bp)                ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     //푸터 위치를 읽어옴.

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))       // 현재 블록 다음 블록의 위치로 이동.
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))       //현재 블록의 이전 블록의 위치로 이동.

static char *last_bp;
static char *heap_listp;        //처음에 쓸 큰 가용 블록 힙을 생성.
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "bitmasking",
    /* First member's full name */
    "wjcheon96@gmail.com",
    /* First member's email address */
    "cheon woongjae",
    /* Second member's full name (leave blank if none) */
    "park chan",
    /* Second member's email address (leave blank if none) */
    "a@a"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */

static void *coalesce(void *bp)								// 블록간의 연결
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc)							// 이전과 다음 블록 모두 할당된 경우. 할당에서 가용으로 변경.
	{
		last_bp = bp;
		return bp;
	}
	else if (prev_alloc && !next_alloc)						//이전 블록은 할당상태. 다음 블록 가용 상태. 현재 블록을 다음 블록과 통합.
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	else if (!prev_alloc && next_alloc)						// 이전 블록은 가용상태. 다음 블록 할당상태. 이전 블록을 현재 블록과 통합.
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	else													// 블록 모두 가용상태. 이전, 현재, 다음 블록 모두 가용 블록 통합.
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	last_bp = bp;
	return (bp);
}

//힙 확장.
static void	*extend_heap(size_t words)
{
	char	*bp;
	size_t	size;

	size = (words%2) ? (words+1) * WSIZE : words * WSIZE;	//2의 배수로 size를 증가시킨다. 홀수면 1을 증가시키고 word 만큼, 짝수면 그냥 word만큼.
	if ((long)(bp = mem_sbrk(size)) == -1)		//sbrk로 size를 증가시킨다. 
		return (NULL);							// size 증가시 old_brk위치는 과거의 mem_brk위치로 이동하게됨.

	PUT(HDRP(bp), PACK(size, 0));				//free block header 생성.
	PUT(FTRP(bp), PACK(size, 0));				//free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));		//epilogue header의 위치를 조정해줌.

	return (coalesce(bp));						//coalesce를 진행.
}

/* 맨 처음에 힙을 생성. 0부터 시작한다. */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) //초기 brk에서 4만큼 늘려 mem brk를 늘려준다.
		return (-1);
	PUT(heap_listp, 0);		//블록 생성시 padding을 한 워드만큼 생성. heap_listp의 위치는 맨 처음에 있다.
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));	//prologue header. DSIZE(double word size)만큼 할당을 진행한다.
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));	//prologue footer.
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));		//epilogue block header. 뒤로 밀리게 된다.
	heap_listp += (2 * WSIZE);						//포인터 위치 변경. header와 footer 사이로 가며, header 뒤에 위치하게 된다.
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)		//시작시 heap을 한번 늘려준다.
		return (-1);
	last_bp = (char *)heap_listp;					//늘려준 heap_list의 주소를 last_bp에 넣어줌. 
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

/*
static void	*find_fit(size_t asize)
{
	void *bp;
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
			return bp;
	}
	return NULL;
}
*/
static void	*next_fit(size_t asize)
{
	char *bp = last_bp;

	for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp))
	{
		if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
		{
			last_bp = bp;
			return bp;
		}
	}
	
	bp = heap_listp;
	while (bp < last_bp)
	{
		bp = NEXT_BLKP(bp);
		if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize)
		{
			last_bp = bp;
			return bp;
		}
	}
	return NULL;
}

static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	if ((csize - asize) >= 2*DSIZE)
	{
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
	}
	else
	{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

void *mm_malloc(size_t size)
{
	size_t	asize;
	size_t	extendsize;
	char	*bp;

	if (size == 0)
		return NULL;
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
	//if ((bp = find_fit(asize)) != NULL)
	if ((bp = next_fit(asize)) != NULL)
	{
		place(bp, asize);
		return bp;
	}

	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	//last_bp = bp;
	return (bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	if (size <= 0)
	{
		mm_free(ptr);
		return 0;
	}
	if (ptr == NULL)
		return mm_malloc(size);

	void *newptr = mm_malloc(size);
	if (newptr == NULL)
		return 0;
	size_t oldsize = GET_SIZE(HDRP(ptr));
	if (size < oldsize)
		oldsize = size;
	memcpy(newptr, ptr, oldsize);
    mm_free(ptr);
    return (newptr);
}
