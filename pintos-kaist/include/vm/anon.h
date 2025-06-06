#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"
#include "include/threads/vaddr.h"
#include "include/lib/kernel/bitmap.h"
#include "threads/synch.h"

struct page;
enum vm_type;

struct anon_page {
    int swap_slot_idx;
};

struct swap_table {
    struct bitmap *swap_bitmap;
    struct lock swap_lock;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif