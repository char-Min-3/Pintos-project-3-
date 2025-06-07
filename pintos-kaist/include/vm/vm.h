#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "lib/kernel/hash.h"


enum vm_type {
	/* 초기화되지 않은 페이지 */
	VM_UNINIT = 0,
	/* 파일과 관련 없는 페이지, 즉 익명 페이지 */
	VM_ANON = 1,
	/* 파일과 관련된 페이지 */
	VM_FILE = 2,
	/* 페이지 캐시를 보유하는 페이지 (project 4 용) */
	VM_PAGE_CACHE = 3,

	/* 상태 저장을 위한 비트 플래그 */

	/* 보조 정보를 저장하기 위한 플래그. int 범위 내에서 원하는 만큼 추가 가능 */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* 이 값을 넘지 말 것 */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;
struct frame;

#define VM_TYPE(type) ((type) & 7)

/* "page"의 표현 구조체
 * 이 구조체는 부모 클래스와 같으며, 4개의 자식 클래스(uninit_page, file_page, anon_page, page cache)가 존재함
 * 이 구조체의 사전 정의된 멤버는 제거/수정 금지 */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */
	struct hash_elem hash_elem; /*spt의 hash를 위한 hash_elem*/
	
    //쓰기 읽기 멤버가 필요한가? 
	
	/* 여기에 구현 내용 추가 가능 */

    /* 실제로 해당 페이지를 사용하는 프로세스에 대한 멤버도 필요할 것 같다. */
    bool writable;
	/* 타입별 데이터를 union으로 묶음
	 * 각 함수는 현재 타입을 자동으로 감지함 */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* "frame"의 표현 구조체 */
struct frame {
	void *kva;           /* 커널 가상 주소 */
	struct page *page;   /* 해당 프레임에 연결된 페이지 */
	struct hash_elem hash_elem;
};

/* 페이지 동작을 위한 함수 테이블
 * 이는 C에서 "인터페이스"를 구현하는 방식 중 하나임
 * 구조체 멤버에 함수 테이블을 두고, 필요할 때 호출함 */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* 현재 프로세스의 메모리 공간을 나타내는 구조체
 * 이 구조체는 자유롭게 설계 가능 */
struct supplemental_page_table {
	struct hash spt_hash;		/*spt hash 테이블 */
	// struct lock spt_lock;		/*spt lock (임계 구역 관리)*/
};

struct frame_table{
	struct hash ft;
	struct lock ft_lock;
};

extern struct frame_table frame_table;

unsigned frame_hash (const struct hash_elem *e, void *aux);
bool	frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

/* 페이지 할당 매크로: 초기화자 없이 페이지 할당 */
#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)

/* 초기화자와 함께 페이지를 할당 */
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);

/* 페이지 해제 */
void vm_dealloc_page (struct page *page);
/* 주소에 해당하는 페이지 점유 */
bool vm_do_claim_page(struct page *page);
bool vm_claim_page (void *va);
/* 페이지의 타입 반환 */
enum vm_type page_get_type (struct page *page);

/*hash*/
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

static void spt_kill_destructor (struct hash_elem *h, void *aux UNUSED);
#endif  /* VM_VM_H */

