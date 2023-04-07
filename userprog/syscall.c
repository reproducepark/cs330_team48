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
struct lock sys_lock;
/* Project 2 */

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
	lock_init(&sys_lock);
	/* Project 2 */
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	/* Project 2 */
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
	return process_fork(thread_name, if_);
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
	return filesys_create(file, initial_size);
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
	if((f = filesys_open(file)) == NULL){
		return -1;
	}
	for(int i = 2; i < 128; i++){
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
	return file_length(thread_current()->fdt[fd]);
}

int read (int fd, void *buffer, unsigned size) {
	//we need to check the address
	if(check_addr(buffer) == false){
		exit(-1);
	}
	if(fd == 0){
		for(unsigned i = 0; i < size; i++){
			*(char *)(buffer + i) = input_getc();
			if(*(char *)(buffer + i) == '\0'){
				return i;
			} 
		}
		return size;
	}
	else if (check_fd(fd) == false){
		return -1;
	}
	else{
		lock_acquire(&sys_lock);
		int byte = file_read(thread_current()->fdt[fd], buffer, size);
		lock_release(&sys_lock);
		return byte;
	}
}

int write (int fd, const void *buffer, unsigned size) {
	//we need to check the address
	if(check_addr(buffer) == false){
		exit(-1);
	}
	if(fd == 1){
		putbuf(buffer, size);
		return size;
	}
	else if (check_fd(fd) == false){
		return 0;
	}
	else{
		lock_acquire(&sys_lock);
		int byte = file_write(thread_current()->fdt[fd], buffer, size);
		lock_release(&sys_lock);
		return byte;
	}
}

void seek (int fd, unsigned position) {
	if(check_fd(fd) == false){
		return;
	}
	file_seek(thread_current()->fdt[fd], position);
}

unsigned tell (int fd) {
	if(check_fd(fd) == false){
		return -1;
	}
	return file_tell(thread_current()->fdt[fd]);
}

void close (int fd) {
	if(check_fd(fd) == false){
		return;
	}
	file_close(thread_current()->fdt[fd]);
	thread_current()->fdt[fd] = NULL;
}

bool check_fd (int fd){
	if((fd < 2) || (fd >= 128)){
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
		return false;
	}
	return true;
}



/* Project 2 */

