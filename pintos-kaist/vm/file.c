#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	
	// // aux->file = page->uninit.;
	// // aux->offset;
	// // aux->read_bytes;
	// // aux->zero_bytes;
	// memset(&page->uninit, 0, sizeof(struct uninit_page));
	page->operations = &file_ops;
	
	struct file_page *file_page = &page->file;
	struct file_info *aux = page->uninit.aux;
	file_page->file = aux->file;
	file_page->offset = aux->offset;
	file_page->page_read = aux->page_read;
	file_page->page_zero = aux->page_zero;

	
    // free(aux);
	page->uninit.aux = NULL;
	
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	off_t bytes_read = file_read_at(file_page->file,kva,file_page->page_read,file_page->offset);
	/* bytes_read : 실제로 읽은 바이트 // page_read : 읽어야할 바이트*/
	// 같아야함 !
	if (bytes_read != file_page->page_read){
		return false;
	}
	//나머지를 0으로 채워준다. page_zero 만큼.
	memset(kva+file_page->page_read,0, file_page->page_zero);
	//페이지 매핑해주기
	// pml4_set_page(thread_current()->pml4, page->va,kva, page->writable);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct frame *frame = page->frame;
	
	if(frame == NULL) return false;
	if(pml4_is_dirty(thread_current()->pml4,page->va)){
		file_write_at(file_page->file, frame->kva, file_page->page_read, file_page->offset);
	}

	//pml4에서 페이지 제거함.
	pml4_clear_page(thread_current()->pml4, page->va);
	//frame 페이지 제거함.
	// palloc_free_page(frame->kva);
	//hash에서도 삭제하기
	lock_acquire(&frame_table.ft_lock);
	hash_delete(&frame_table.ft, &frame->hash_elem);
	lock_release(&frame_table.ft_lock);
	
	// free(page->frame);
	page->frame = NULL;
	return true;

}


/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if(page->frame && pml4_is_dirty(thread_current()->pml4,page->va)){
		file_write_at(file_page->file, page->frame->kva, file_page->page_read, file_page->offset);
	}

}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

    //얼마나 읽고 0으로 채울지 추측
    //load segment 와 유사한 동작
	void * file_addr = addr;

    size_t read_bytes = length > file_length(file) ? file_length(file) : length;
    size_t zero_bytes = (PGSIZE - read_bytes % PGSIZE);
	// size_t zero_bytes = (PGSIZE - (read_bytes % PGSIZE)) % PGSIZE;
		
	while (read_bytes > 0 || zero_bytes > 0) {

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_info *aux = malloc(sizeof(struct file_info));
		if (aux == NULL) return false;	
		aux->file = file;
		aux->page_read = page_read_bytes;
		aux->page_zero = page_zero_bytes;
		aux->offset = offset;
		
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, aux)){
			free(aux);
			return NULL;
		}
			

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}	
	return file_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
   
   	while(true){
		struct page *page = spt_find_page(&thread_current()->spt, addr);

		if(page == NULL) return;
		
		struct file_page *aux = &page->file;

		if(aux == NULL) return;

		//사용중인 페이지를 감지? 반복문에 가둬야하나?
		if(pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(aux->file, page->frame->kva,aux->page_read, aux->offset);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
	
		pml4_clear_page(thread_current()->pml4, page->va);
		destroy(page);

		if (page->frame != NULL) {
            struct frame *f = page->frame;	
			//frmae_table에서 제거
			lock_acquire(&frame_table.ft_lock);
        	hash_delete(&frame_table.ft, &page->frame->hash_elem);
        	lock_release(&frame_table.ft_lock);

			//frame도 제거
			palloc_free_page(page->frame->kva);
        	free(page->frame);
			page->frame = NULL;
		}

		//spt에서 제거
		spt_delete_page(&thread_current()->spt, page);
		free(page);
		addr += PGSIZE;
	}
 
}
