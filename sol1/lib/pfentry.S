#include <inc/mmu.h>
#include <inc/pmap.h>


// Page fault handler entrypoint -
// this is where we ask the kernel
// (by calling sys_set_pgfault_handler in pgfault.c)
// to redirect us to whenever we cause a page fault in user space.
//
// When a page fault actually occurs, 
// the kernel switches our ESP to point to the user exception stack
// if we're not already on the user exception stack,
// and then it pushes the following minimal trap frame
// onto our user exception stack:
//
//	[ 5 spare words ]
//	trap-time eip
//	trap-time eflags
//	trap-time esp
//	trap-time errcode
//	trap-time va		<-- %esp
//
// We then have to save additional callee-saved registers
// and call up to the appropriate page fault handler in C code,
// pointed to by the global variable '_pgfault_handler' declared above.

.text
.globl _pgfault_entry
_pgfault_entry:
	// Save the caller-save registers
	//	(your code here)
    push %eax
    push %ecx
    push %edx
    push %ebx
    push %esi
    push %edi
    sub $0x14, %esp

	// Call the C page fault handler
	movl _pgfault_handler, %eax
	call *%eax

	// Push trap-time eip and eflags onto trap-time stack.
	// In the case of a recursive fault on the exception stack,
	// note that the two words we're pushing now
	// overlap with the exception frame we're currently using!
	//	(your code here)
    add $0x8, %esp 
    pop %eax  // old esp
    pop %edx  // eflags
    pop %ecx // eip
    
    /*
    // cmpare if this stack is within UXSTACKTOP-BY2PG
    mov UXSTACKTOP, %ebx
    cmp %ebx, %eax
    jb 1f
    mov UXSTACKTOP-BY2PG, %ebx
    cmp %ebx, %eax
    ja 1f
    // user exception stack, no needs for stack pointer switch
    pop %edi
    pop %esi
    pop %ebx
    pop %edx
    pop %ecx
    pop %eax
    add $0x8, %esp
    popfl
    ret */

    // normal stack 
//1: 
    mov %ecx, -4(%eax) // push eip
    mov %edx, -8(%eax) // push eflags
    
	// Restore the caller-save registers.
	//	(your code here)
    pop %edi
    pop %esi
    pop %ebx
    pop %edx
    pop %ecx
    pop %eax

	// Switch back to the trap-time stack.
	//	(your code here)
    mov -36(%esp), %esp 
    sub $0x8, %sp // reserve two words place for eflags and eip

	// Restore eflags and eip from the stack.
	//	(your code here)
    popfl
    ret
