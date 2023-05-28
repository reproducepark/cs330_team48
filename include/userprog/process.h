#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* Project 2 */
struct ptr{
    struct thread *parent;
    struct intr_frame *parent_if;
};

#define FDT_SIZE 512
/* Project 2 */

/* Project 3 */
#ifdef VM
bool lazy_load_segment (struct page *page, void *aux);
#endif
/* Project 3 */

#endif /* userprog/process.h */
