// User-level page fault handler support.
// We use an assembly language wrapper around a C function.
// The assembly wrapper is in pfentry.S.

#include <inc/lib.h>


// Assembly language pgfault entrypoint defined in lib/pgfaultentry.S.
extern void _pgfault_entry(void);

// Pointer to currently installed C-language pgfault handler.
void (*_pgfault_handler)(u_int, u_int);

//
// Set the page fault handler function.
// If there isn't one yet, _pgfault_handler will be 0.
// The first time we register a handler, we need to 
// allocate an exception stack and tell the kernel to
// call _asm_pgfault_handler on it.
//
void
set_pgfault_handler(void (*fn)(u_int va, u_int err))
{
	int r;

	if (_pgfault_handler == 0) {
		// Your code here:
		// map one page of exception stack with top at UXSTACKTOP
		// register assembly pgfault entrypoint with JOS kernel
        r = sys_mem_alloc(0, UXSTACKTOP-BY2PG, PTE_P|PTE_U|PTE_W);
        if(r < 0)
            panic("sys_mem_alloc error\n");
        r = sys_set_pgfault_entry(0, (u_int)_pgfault_entry);
        if(r < 0)
            panic("sys_set_pgfault_entry error\n");
		//panic("set_pgfault_handler not implemented");
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = fn;
    
}

