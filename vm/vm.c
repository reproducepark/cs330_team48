/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Project 3 */
#include "threads/vaddr.h"
#include "threads/mmu.h"

struct list frame_table;

/* Project 3 */

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* Project 3 */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	/* Project 3 */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
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

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

// /* Create the pending page object with initializer. If you want to create a
//  * page, do not create it directly and make it through this function or
//  * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* Project 3 */
		struct page * new_page = (struct page *)calloc(sizeof(struct page), 1);
		if(new_page == NULL){
			goto err;
		}
		if(VM_TYPE(type) == VM_FILE){
			uninit_new(new_page, upage, init, type, aux, file_backed_initializer);
		}
		else if(VM_TYPE(type) == VM_ANON){
			uninit_new(new_page, upage, init, type, aux, anon_initializer);
		}
		new_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
		/* Project 3 */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	/* TODO: Fill this function. */
	/* Project 3 */
	struct page page_to_find;
	page_to_find.va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->spt_hash_table, &page_to_find.spt_elem);
	if (e == NULL) {
		return NULL;
	}
	return hash_entry(e, struct page, spt_elem);
	/* Project 3 */
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	/* Project 3 */
	struct hash_elem *e = hash_insert(&spt->spt_hash_table, &page->spt_elem);
	if (e == NULL) {
		return true;
	}
	return false;
	/* Project 3 */
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	/* Project 3 */
	// struct hash_elem *e = hash_delete(&spt->spt_hash_table, &page->spt_elem);
	vm_dealloc_page (page);
	// if (e == NULL) {
		// return true;
	// }
	// return false;
	/* Project 3 */
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	
	/* TODO: The policy for eviction is up to you. */
	/* Project 3 */
	static int i = 1;
	struct frame *victim = NULL;
	struct thread *cur = thread_current();
	if(i > 0){
		for(struct list_elem * e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
			victim = list_entry(e, struct frame, ft_elem);
			if(!pml4_is_accessed(&cur->pml4, victim->page->va)){
				i *= -1;
				return victim;
			}
		}
	}
	else{
		for(struct list_elem * e = list_end(&frame_table); e != list_begin(&frame_table); e = list_prev(e)){
			victim = list_entry(e, struct frame, ft_elem);
			if(!pml4_is_accessed(&cur->pml4, victim->page->va)){
				i *= -1;
				return victim;
			}
		}
	}
	
	for(struct list_elem * e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
		victim = list_entry(e, struct frame, ft_elem);
		pml4_set_accessed(&cur->pml4, victim->page->va, false);
	}
	/* Project 3 */
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// if(){
		swap_out(victim->page);
	// }
	// else if() {
	// 	anon_swap_out(victim->page);
	// }
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)calloc(sizeof (struct frame), 1);
	/* TODO: Fill this function. */
	/* Project 3 */
	if((frame->kva = palloc_get_page(PAL_USER)) == NULL){
		free(frame);
		frame = vm_evict_frame();
		if(frame == NULL){
			return NULL;
		}
		frame->page = NULL;
	}
	else{
		list_push_back(&frame_table, &frame->ft_elem);
	}
	/* Project 3 */
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Project 3 */
	struct page *page = spt_find_page(spt, addr);
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// if(page->frame != NULL){
	// 	return swap_in(page, page->frame->kva);
	// }
	/* Project 3 */
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	/* Project 3 */
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if(page == NULL){
		return false;
	}
	/* TODO: Fill this function */
	return vm_do_claim_page (page);
	/* Project 3 */
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) {
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* Project 3 */
	if(pml4_get_page (&thread_current()->pml4, page->va) != NULL){
		return false;
	}
	if(pml4_set_page(&thread_current()->pml4, page->va, frame->kva, page->writable) == false){
		return false;
	}
	return swap_in (page, frame->kva);
	/* Project 3 */
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->spt_hash_table, spt_hash_func, spt_hash_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	/* Project 3 */
	struct hash_iterator i;
	hash_first (&i, &src->spt_hash_table);
	while (hash_next (&i)){
		struct page *page = hash_entry (hash_cur (&i), struct page, spt_elem);
		switch(VM_TYPE(page->operations->type)){
			case VM_UNINIT:
				{
				struct load_info * aux = NULL;
				if(VM_TYPE(page->uninit.type) == VM_FILE){
					aux = (struct load_info *)malloc(sizeof(struct load_info *));
					memcpy(aux, (struct load_info *)page->uninit.aux, sizeof(struct load_info));
					aux->file = file_reopen(aux->file);
				}
				vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable, page->uninit.init, aux);
				// 이렇게 할까? 깃북에서 하라는데...
				vm_claim_page(page->va);
				break;
				}
			case VM_FILE:
				{
				struct load_info * info = malloc(sizeof(struct load_info));
				struct file * file = file_reopen(page->file.file);

				file_seek(file, info->ofs);
				if(!vm_alloc_page(VM_FILE, page->va, page->writable))
					return false;
				struct page * dst_page = spt_find_page(dst, page->va);
				dst_page->file.file = file;
				dst_page->file.ofs = info->ofs;
				dst_page->file.page_read_bytes = info->page_read_bytes;
				dst_page->file.page_zero_bytes = info->page_zero_bytes;
				vm_do_claim_page(dst_page);
				break;
				}
			case VM_ANON:
				{
				if(!vm_alloc_page(VM_ANON, page->va, page->writable))
					return false;
				struct page * dst_page = spt_find_page(dst, page->va);
				vm_do_claim_page(dst_page);
				memcpy(dst_page->frame, page->frame, sizeof(struct frame));
				break;
				}

		}
	}
	return true;
	/* Project 3 */
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	 /* Project 3 */
	struct hash_iterator i;
	hash_first(&i, &thread_current()->spt.spt_hash_table);
	while (hash_next(&i)){
		struct page * page = hash_entry (hash_cur(&i), struct page, spt_elem);
		if((page->operations->type == VM_FILE) && pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(page->file.file, page->frame->kva, page->file.page_read_bytes, page->file.ofs);
		}
		free(page->frame);
		vm_dealloc_page(page);
	}
	 /* Project 3 */
}

/* Project 3 */
// For hash table
uint64_t spt_hash_func (const struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry (e, struct page, spt_elem);
	return hash_bytes (&page->va, sizeof (page->va));
}

bool spt_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct page *page_a = hash_entry (a, struct page, spt_elem);
	struct page *page_b = hash_entry (b, struct page, spt_elem);
	return page_a->va < page_b->va;
}

/* Project 3 */


