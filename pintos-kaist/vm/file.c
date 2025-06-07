

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
	struct file_info *aux = malloc(sizeof(struct file_info));
	file_page->file = aux->file;
	file_page->offset = aux->offset;
	file_page->page_read = aux->page_read;
	file_page->page_zero = aux->page_zero;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if(pml4_is_dirty(thread_current()->pml4,page->va)){
		file_write_at(file_page->file, page->va, file_page->page_read, file_page->offset);
	}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
    struct file *files = file_reopen(file);
    //얼마나 읽고 0으로 채울지 추측
    //load segment 와 유사한 동작
	void * file_addr = addr;

    size_t read_bytes = length > file_length(files) ? file_length(files) : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
		
	while (read_bytes > 0 || zero_bytes > 0) {

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_info *aux = malloc(sizeof(struct file_info));
		if (aux == NULL) return false;	
		aux->file = files;
		aux->page_read = page_read_bytes;
		aux->page_zero = page_zero_bytes;
		aux->offset = offset;
		
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, aux))
			return NULL;

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
   struct file_info *aux = page->uninit.aux;
    if(aux == NULL) return;
    //사용중인 페이지를 감지? 반복문에 가둬야하나?
 if(pml4_is_dirty(thread_current()->pml4, page->va)){
       file_write_at(aux->file, addr,aux->page_read, aux->offset);
    pml4_set_dirty(thread_current()->pml4, page->va, 0);
 }
   
 pml4_clear_page(thread_current()->pml4, page->va);
 addr +=PGSIZE;
   }
 


}
