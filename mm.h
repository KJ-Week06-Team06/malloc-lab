#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */

#define WSIZE		4				// 1개의 word 사이즈
#define DSIZE		8				// 2배의 word 사이즈. double word
#define CHUNKSIZE	(1 << 12)		// heap을 한번 늘릴때 필요한 size. 4kb로 설정.

#define MAX(x, y) ((x) > (y) ? (x) : (y))	// 크기 비교

// size를packing하고, 개별 word 안에 bit를 할당.(size와 alloc을 비트 연산.) 헤더에서 사용
#define PACK(size, alloc) ((size) | (alloc))	// 블록 헤더에 사이즈와 할당 여부를 넣어서 체크

#define GET(p)			(*(unsigned int *)(p))			// 블록에 담긴 값을 읽어옴.
#define PUT(p, val)		(*(unsigned int *)(p) = (val))	// 특정 위치에 값을 저장.

#define GET_SIZE(p)		(GET(p) & -0x7)					// 해당 블록의 size를 읽어옴.
#define GET_ALLOC(p) 	(GET(p) & 0x1)					// 해당 블록의 할당 여부를 받아옴.

#define HDRP(bp)		((char *)(bp) - WSIZE)			// 헤더위치를 읽어옴.
#define FTRP(bp)		((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)	//푸터 위치를 읽어옴.

#define NEXT_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - WSIZE)))	// 현재 블록 다음 블록의 위치로 이동.
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))	//현재 블록의 이전 블록의 위치로 이동.

typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

static char *heap_listp;	//처음에 쓸 큰 가용 블록 힙을 생성.
