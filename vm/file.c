/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
/* Project 3 */
#include "threads/vaddr.h"
#include "userprog/process.h"

extern struct list frame_table;
/* Project 3 */

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

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	struct file *file = file_page->file;
	uint32_t read_bytes = file_page->page_read_bytes;
	uint32_t zero_bytes = file_page->page_zero_bytes;

	off_t ofs = file_page->ofs;

	file_seek (file, ofs);	/* Load this page. */
	if (file_read (file, kva, read_bytes) != (int) read_bytes) {
		palloc_free_page (kva);
		// list_remove(&page->frame->ft_elem);
		// free(page->frame);
		return false;
	}
	memset (kva + read_bytes, 0, zero_bytes);
	
	if(page->frame->cpy_cnt > 0){
		for(struct list_elem * e = list_begin(&frame_table); (e != list_end(&frame_table)); e = list_next(e)){
			struct frame *frame = list_entry(e, struct frame, ft_elem);
			if(file_get_inode(frame->page->file.file) == file_get_inode(page->file.file)){
				pml4_set_page(frame->page->pml4, frame->page->va, kva, false);
			}
		}
	}
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	/* Project 3 */
	if(page->frame->cpy_cnt > 0){
		// we don't need to write back
		void * kva = page->frame->kva;
		for(struct list_elem * e = list_begin(&frame_table); (e != list_end(&frame_table)); e = list_next(e)){
			struct frame *frame = list_entry(e, struct frame, ft_elem);
			if(frame->kva == kva){
				pml4_clear_page(frame->page->pml4, frame->page->va);
			}
		}
	}
	else{
		if(pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(file_page->file, page->frame->kva, file_page->page_read_bytes, file_page->ofs);
			pml4_set_dirty (thread_current()->pml4, page->va, 0);
		}
	}
	pml4_clear_page(thread_current()->pml4, page->va);
	page->frame = NULL;
	return true;
	/* Project 3 */
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	/* Project 3 */
	struct file_page *file_page UNUSED = &page->file;
	
	/* Project 3 */
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	/* Project 3 */
	file_seek(file, offset);
	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	void *upage = addr;
	int64_t mmap_id = timer_ticks();
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/* Project 3 */
		struct load_info * info = malloc(sizeof(struct load_info));
		info->file = file_reopen(file);
		info->ofs = offset;
		info->page_read_bytes = page_read_bytes;
		info->page_zero_bytes = page_zero_bytes;
		info->mmap_id = mmap_id;
		void *aux = info;
		/* Project 3 */
		if (!vm_alloc_page_with_initializer (VM_FILE, upage,
					writable, lazy_load_segment, aux)){
			free(aux);
			return NULL;
		}
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		offset += page_read_bytes;
	}
	return addr;
	/* Project 3 */
}

/* Do the munmap */
void
do_munmap (void *addr) {
	/* Project 3 */
	struct page* page = spt_find_page(&thread_current()->spt, addr);
	int64_t mmap_id = page->mmap_id;
	if(mmap_id == 0){
		return;
	}
	struct file* file = page->file.file;
	while(((page = spt_find_page(&thread_current()->spt, addr)) != NULL) && (page->mmap_id == mmap_id)){
		if(pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(page->file.file, page->frame->kva, page->file.page_read_bytes, page->file.ofs);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
		hash_delete(&thread_current()->spt.spt_hash_table, &page->spt_elem);
		file_close(page->file.file);
		destroy(page);
		addr += PGSIZE;
	}
	/* Project 3 */
}
