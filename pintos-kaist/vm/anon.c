/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "vm/anon.h"
#include <stdlib.h>
#include "lib/kernel/bitmap.h"
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct swap_table swap_table;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

#define SECTOR_PER_SLOT (PGSIZE / DISK_SECTOR_SIZE) // 총 스왑 슬롯 개수

#define SECTOR_PER_SLOT (PGSIZE / DISK_SECTOR_SIZE) // 총 스왑 슬롯 개수

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	disk_sector_t sector_cnt = disk_size(swap_disk); // 스왑 디스크의 섹터 개수
	size_t swap_slot_cnt = sector_cnt / SECTOR_PER_SLOT; // 총 스왑 슬롯 개수

	swap_table.swap_bitmap = bitmap_create(swap_slot_cnt);
	lock_init(&swap_table.swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	//필요한 모든 연결을 해줘야함.//
	page->operations = &anon_ops;
	// page->frame->kva = kva;
	// page->frame->page = page;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_slot_idx = -1;
	
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	int slot_idx = anon_page->swap_slot_idx;
	int sector_start = (slot_idx)*SECTOR_PER_SLOT;
	
	lock_acquire(&swap_table.swap_lock);
    
	for(int i =0; i<SECTOR_PER_SLOT; i++ ){
		disk_read(swap_disk, sector_start + i, (uint8_t *)kva + DISK_SECTOR_SIZE*i);
	}
	/*비트맵 0으로 만들기.*/
	
	bitmap_reset(swap_table.swap_bitmap,slot_idx);
	
	anon_page->swap_slot_idx = -1;

	lock_release(&swap_table.swap_lock);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	void *kva = page->frame->kva;
	lock_acquire(&swap_table.swap_lock);
	size_t slot_idx = bitmap_scan(swap_table.swap_bitmap, 0, 1, false);

	/* 없는 경우 체크 */
	if(slot_idx == BITMAP_ERROR){
		lock_release(&swap_table.swap_lock);
		return false;
	}

	bitmap_flip(swap_table.swap_bitmap, slot_idx);
	anon_page->swap_slot_idx = slot_idx;

	/* slot 번호를 섹터 오프셋으로 변환 */
    size_t sector_offset = slot_idx * SECTOR_PER_SLOT;

  	/* 512바이트씩 나눠서 기록 */
    for (size_t i = 0; i < SECTOR_PER_SLOT; i++) {
        disk_write (swap_disk, sector_offset + i, (uint8_t *)kva + i * DISK_SECTOR_SIZE);
    }
	lock_release(&swap_table.swap_lock);

	lock_acquire(&frame_table.ft_lock);
	hash_delete(&frame_table.ft, &page->frame->hash_elem);
	lock_release(&frame_table.ft_lock);
	
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	if (anon_page->swap_slot_idx != -1){
		lock_acquire(&swap_table.swap_lock);
        bitmap_reset(swap_table.swap_bitmap, anon_page->swap_slot_idx);
        lock_release(&swap_table.swap_lock);
	}

	// if (page->frame){
	// 	palloc_free_page(page->frame->kva);
	// 	free(page->frame);
	// 	page->frame = NULL;
	// }
}
