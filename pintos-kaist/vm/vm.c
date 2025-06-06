/* vm.c: Generic interface for virtual memory objects. */
// vm.c: 가상 메모리 객체를 위한 일반적인 인터페이스
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h" 
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "string.h"


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
// 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다.
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}


/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
// 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 이후에 어떤 타입이 될지 알고 싶을 때 유용합니다.
// 이 함수는 현재 완전히 구현되어 있습니다.
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}


/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
// 초기화 함수를 가진 '지연 생성' 페이지 객체를 생성합니다.
// 페이지를 직접 생성하지 말고, 이 함수 또는 `vm_alloc_page`를 통해 생성하세요.
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	// 해당 가상 주소(upage)가 이미 점유되어 있는지 확인합니다.
	if (spt_find_page (spt, upage) == NULL) {

		struct page *_pages =(struct page*) malloc(sizeof(struct page));
		// struct page *_pages = palloc_get_page(PAL_USER);

		bool (*initializer) (struct page *page, enum vm_type type, void *kva);
		
		switch (VM_TYPE(type)) {  // VM_TYPE_MASK로 타입 추출
  		case VM_ANON:
    		initializer = anon_initializer;
    		break;

  		case VM_FILE:
    		initializer = file_backed_initializer;
    		break;

  		default:
    		initializer = NULL;
    		break;
        }
        
		uninit_new(_pages, upage, init, type, aux, initializer);
		_pages->writable = writable;
       
	   /*
	   "페이지를 생성하고, VM 타입에 따라 초기화 함수를 정한 뒤, 
	   uninit_new()를 호출해서 uninit 페이지 구조체를 만들어라.
	    그 후에 필요한 필드를 수정해라."*/
        // TODO: 페이지를 보조 페이지 테이블에 삽입합니다.
       
       //필요한 필드란 무슨 의미일까?
	   //writable 나중에봐 (2025-06-03 11:31 AM)

	   if(!spt_insert_page(spt, _pages)){
		    printf("spt_insert_page failed: va=%p\n", upage);
			free(_pages);
			goto err;
	   }
		return true;
	
	}

	
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// VA에 해당하는 페이지를 spt에서 찾아 반환합니다. 실패 시 NULL을 반환합니다.
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {

	if (spt == NULL || va == NULL) return NULL;

	struct page temp_page;
	temp_page.va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->spt_hash, &temp_page.hash_elem);


	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
// 검증 후 PAGE를 spt에 삽입합니다.
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {

	/* TODO: Fill this function. */
	// TODO: 이 함수를 구현하세요.

	int succ = false;

	struct hash_elem *elem = hash_insert(&spt->spt_hash, &page->hash_elem);

	if (elem == NULL)
		succ = true;
	
	return succ;
}


/* Get the struct frame, that will be evicted. */
// 제거할 프레임을 구합니다.
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	// TODO: 페이지 교체 정책은 여러분이 자유롭게 구현하세요.
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
// 하나의 페이지를 교체하고 해당 프레임을 반환합니다. 실패 시 NULL을 반환합니다.
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// TODO: victim을 swap out하고 해당 프레임을 반환하세요.
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
// palloc()을 호출하여 프레임을 얻습니다. 사용 가능한 페이지가 없으면 페이지를 교체(evict)하여 반환합니다.
// 항상 유효한 주소를 반환합니다. 즉, 사용자 메모리 풀이 가득 차도 프레임을 교체하여 메모리를 확보합니다.
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL)
		return NULL;
    
	void *new_kva = palloc_get_page(PAL_USER);
	if (new_kva == NULL) {
		free(frame);
		return NULL;
	}

	frame->page = NULL;
	frame->kva = new_kva;
	/* TODO: Fill this function. */
	// TODO: 이 함수를 구현하세요.

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
// 스택을 확장합니다.
static bool
vm_stack_growth (void *addr UNUSED) {
	void *stack_bottom = thread_current()->stack_bottom;
	void *new_addr = pg_round_down(addr);

	while(new_addr < stack_bottom){
		stack_bottom -= PGSIZE;
		if(!vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_0, stack_bottom, true, NULL, NULL)){
			return false;
		}
		if (!vm_claim_page(stack_bottom)) return false;

	}
	thread_current()->stack_bottom = stack_bottom;
	return true;

}

/* Handle the fault on write_protected page */
// 쓰기 보호된 페이지에서의 페이지 폴트를 처리합니다.
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// 성공 시 true를 반환합니다.
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt,addr);


	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// TODO: 페이지 폴트를 검증하고, 처리 코드를 작성하세요.
	/* 페이지가 NULL */
	/* stack에 공간이 부족하다 !*/

	if (page == NULL) {
		if ((addr < USER_STACK) && (addr >= USER_STACK - (1 << 20)) && (addr >= f->rsp - 8)) {
			if (vm_stack_growth(addr)) {
				return true;
			}
			return false;
		}
		return false;
	}



	/* write 접근했는데, 페이지가 writable 하지 않을 경우 */
	if (write && !page->writable){
		return false;
	}

	/* 권한 */
	if (!not_present){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
// 페이지를 해제합니다. 이 함수는 수정하지 마세요.
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// 주어진 VA에 해당하는 페이지를 점유(claim)합니다.
bool
vm_claim_page (void *va UNUSED) {
    va = pg_round_down(va);

    struct page *page = spt_find_page(&thread_current()->spt, va);
    if (page == NULL) {
        return false; 
    }
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
// 주어진 PAGE를 점유하고 MMU 설정을 완료합니다.
bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
  	if (frame == NULL){
		return false;
	}
	frame->page = page;
	page->frame = frame;
	

    // if (pml4_get_page(thread_current()->pml4, page->va) != NULL){

   	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)){
		return false;
	}
		

    return swap_in(page, frame->kva);
}
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED){
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED){
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);

	return a->va < b->va;
}


/* Initialize new supplemental page table */
// 새로운 보조 페이지 테이블을 초기화합니다.
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
	// lock_init(&spt->spt_lock);
}


bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// dst가 자식, src가 부모
	hash_init(dst, page_hash, page_less, NULL);	
	struct hash_iterator iterator;

	hash_first(&iterator, &src->spt_hash);

	while(hash_next(&iterator)){
		struct page *src_page = hash_entry(hash_cur(&iterator), struct page, hash_elem);

		if (page_get_type(src_page) == VM_UNINIT){
			struct uninit_page *src_un = &src_page->uninit;
			if(!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, NULL, NULL)) 
				return false;
		}

		else if(page_get_type(src_page) == VM_ANON){
			if(!vm_alloc_page_with_initializer(VM_ANON, src_page->va, src_page->writable, anon_initializer, NULL)) 
				return false;
			if(src_page->frame == NULL) continue;
			if(!vm_claim_page(src_page->va)) return false;
			
			struct page *dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}








/* Free the resource hold by the supplemental page table */
// 보조 페이지 테이블이 보유한 자원을 해제합니다.
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// TODO: 스레드가 보유한 모든 보조 페이지 테이블을 제거하고,
	// TODO: 수정된 내용을 스토리지에 기록하세요.

	hash_clear(&spt->spt_hash, spt_kill_destructor);
}

static void spt_kill_destructor (struct hash_elem *h, void *aux UNUSED) {
	struct page *page = hash_entry(h, struct page, hash_elem);

	destroy(page);
	free(page);
}
