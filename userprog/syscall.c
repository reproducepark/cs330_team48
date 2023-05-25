#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* Project 2 */
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "devices/input.h"
struct semaphore sys_sema;
/* Project 2 */

/* Project 3 */
#include "vm/vm.h"
/* Project 3 */

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* Project 2 */
void halt (void);
void exit (int status);
pid_t fork (const char *thread_name, struct intr_frame *if_ );
int exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

bool check_fd(int fd);
bool check_addr(void *addr);
/* Project 2 */

/* Project 3 */
void * mmap(void * addr, size_t length, int writable, int fd, off_t offset);
void munmap(void * addr);
/* Project 3 */

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	
	/* Project 2 */
	sema_init(&sys_sema,1);
	/* Project 2 */
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	/* Project 2 */
	thread_current()->ursp = f->rsp;
	switch(f->R.rax){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			exec(f->R.rdi);
			break;
		case SYS_WAIT:	
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		case SYS_DUP2:
			f->R.rax = dup2(f->R.rdi, f->R.rsi);
			break;
		/* Project 3 */
		case SYS_MMAP:
			f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
			break;
		case SYS_MUNMAP:
			munmap(f->R.rdi);
			break;
		/* Project 3 */
		default:
			exit(-1);
			break;
	}
	/* Project 2 */
}
/* Project 2 */

void halt (void) {
	power_off();
}

void exit (int status) {
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_current()->exit_status = status;
	thread_exit();
}

pid_t fork (const char *thread_name, struct intr_frame *if_ ) {
	if(check_addr(thread_name) == false){
		exit(-1);
	}
	sema_down(&sys_sema);
	tid_t tid = process_fork(thread_name, if_);
	sema_up(&sys_sema);
	return tid;
}

int exec (const char *cmd_line) {
	if(check_addr(cmd_line) == false){
		exit(-1);
	}
	char *fn_copy = palloc_get_page (0);
	if (fn_copy == NULL){
		exit(-1);
	}
	strlcpy (fn_copy, cmd_line, PGSIZE);
	if(process_exec(fn_copy) == -1){
		exit(-1);
	}
	NOT_REACHED();
}

int wait (pid_t pid) {
	
	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
	//we need to check the address
	if(check_addr(file) == false){
		exit(-1);
	}
	sema_down(&sys_sema);
	bool success = filesys_create(file, initial_size);
	sema_up(&sys_sema);

	return success;
}

bool remove (const char *file) {
	//we need to check the address
	if(check_addr(file) == false){
		exit(-1);
	}
	return filesys_remove(file);			// Unix-like semantics for filesys_remove() are implemented.
}

int open (const char *file) {
	//we need to check the address
	if(check_addr(file) == false){
		exit(-1);
	}
	struct file *f;
	sema_down(&sys_sema);
	if((f = filesys_open(file)) == NULL){
		sema_up(&sys_sema);
		return -1;
	}
	sema_up(&sys_sema);
	for(int i = 0; i < FDT_SIZE; i++){
		if(thread_current()->fdt[i] == NULL){
			thread_current()->fdt[i] = f;
			return i;
		}
	}
	file_close(f);
	return -1;
}

int filesize (int fd) {
	if(check_fd(fd) == false){
		return -1;
	}
	struct file *f = thread_current()->fdt[fd];
	if(f == STDIN_FP || f == STDOUT_FP){
		return -1;
	}

	return file_length(thread_current()->fdt[fd]);
}

int read (int fd, void *buffer, unsigned size) {
	//we need to check the address
	if(check_addr(buffer) == false){
		exit(-1);
	}
	if(check_fd(fd) == false){
		return -1;
	}

	void * buf = buffer;
	while(buf < buffer + size){
		struct page * bufpage =spt_find_page(&thread_current()->spt, buf);
		if(bufpage->writable == false){
			exit(-1);
		}
		buf += PGSIZE;
	}
	

	struct file *f = thread_current()->fdt[fd];

	if(f == STDIN_FP){
		for(unsigned i = 0; i < size; i++){
			*(char *)(buffer + i) = input_getc();
			if(*(char *)(buffer + i) == '\0'){
				return i;
			} 
		}
		return size;
	}
	else if(f == STDOUT_FP){
		return -1;
	}
	else{
		sema_down(&sys_sema);
		int byte = file_read(thread_current()->fdt[fd], buffer, size);
		sema_up(&sys_sema);
		return byte;
	}
}

int write (int fd, const void *buffer, unsigned size) {
	//we need to check the address
	if(check_addr(buffer) == false){
		exit(-1);
	}
	if(check_fd(fd) == false){
		return -1;
	}

	struct file *f = thread_current()->fdt[fd];

	if(f == STDOUT_FP){
		putbuf(buffer, size);
		return size;
	}
	else if (f == STDIN_FP){
		return -1;
	}
	else{
		sema_down(&sys_sema);
		int byte = file_write(thread_current()->fdt[fd], buffer, size);
		sema_up(&sys_sema);
		return byte;
	}
}

void seek (int fd, unsigned position) {
	if(check_fd(fd) == false){
		return;
	}
	struct file *f = thread_current()->fdt[fd];
	if(f == STDIN_FP || f == STDOUT_FP){
		return -1;
	}
	file_seek(thread_current()->fdt[fd], position);
}

unsigned tell (int fd) {
	if(check_fd(fd) == false){
		return -1;
	}
	struct file *f = thread_current()->fdt[fd];
	if(f == STDIN_FP || f == STDOUT_FP){
		return -1;
	}
	return file_tell(thread_current()->fdt[fd]);
}

void close (int fd) {
	if(check_fd(fd) == false){
		return;
	}
	struct file *f = thread_current()->fdt[fd];
	if(f != STDOUT_FP && f != STDIN_FP && file_dec_cnt(f) == 0){
		file_close(f);
	}
	thread_current()->fdt[fd] = NULL;
}

bool check_fd (int fd){
	if((fd < 0) || (fd >= FDT_SIZE)){
		return false;
	}
	else if(thread_current()->fdt[fd] == NULL){
		return false;
	}
	return true;
}

bool check_addr (void* addr){
	if(addr == NULL){
		return false;
	}
	else if(is_user_vaddr(addr) == false){
		return false;
	}
	else if(pml4_get_page(thread_current()->pml4, addr) == NULL){
		#ifdef VM
			if(spt_find_page(&thread_current()->spt, addr) != NULL){
				return true;
			}
		#endif
		return false;
	}
	return true;
}

int dup2(int oldfd, int newfd){
	if(check_fd(oldfd) == false){
		return -1;
	}
	if(newfd == oldfd){
		return newfd;
	}

	struct file *f = thread_current()->fdt[oldfd];
	struct file *f_old_0 = thread_current()->fdt[0];
	struct file *f_old_1 = thread_current()->fdt[1];
	close(newfd);
	if(f != STDOUT_FP && f != STDIN_FP){
		file_inc_cnt(f);
	}
	
	thread_current()->fdt[newfd] = f;
	return newfd;
}

/* Project 2 */

/* Project 3 */
void * mmap(void * addr, size_t length, int writable, int fd, off_t offset){
	if((addr == NULL) || (is_user_vaddr(addr) == false) || (addr != pg_round_down(addr))){
		return NULL;
	}
	if(((size_t)addr / PGSIZE + length / PGSIZE) > (KERN_BASE / PGSIZE)){
		return NULL;
	}
	if(length == 0 || (length < offset)){
		return NULL;
	}
	if(check_fd(fd) == false){
		return NULL;
	}
	struct file *f = thread_current()->fdt[fd];
	if(f == STDIN_FP || f == STDOUT_FP){
		return NULL;
	}

	if(spt_find_page(&thread_current()->spt, addr) != NULL){
		return NULL;
	}
	
	return do_mmap(addr, length, writable, f, offset);
}

void munmap(void * addr){
	if((addr == NULL) || (is_user_vaddr(addr) == false) || (addr != pg_round_down(addr))){
		return NULL;
	}
	struct page * page = spt_find_page(&thread_current()->spt, addr);
	if(page_get_type(page) == VM_FILE){
		do_munmap(addr);
	}
}
/* Project 3 */