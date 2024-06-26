/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* Projcet 3 */
#include <bitmap.h>
struct bitmap *swap_table;
/* Project 3 */

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

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
	// 8 sectors allocated for each page
	swap_table = bitmap_create(disk_size(swap_disk) / 8);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	for(int i = 0; i < 8; i++){
		disk_read(swap_disk, page->anon.idx * 8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);
	}
	bitmap_flip(swap_table, page->anon.idx);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	page->anon.idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	if(page->anon.idx == BITMAP_ERROR){
		return false;
	}
	for(int i = 0; i < 8; i++){
		disk_write(swap_disk, page->anon.idx * 8 + i, page->va + DISK_SECTOR_SIZE * i);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
