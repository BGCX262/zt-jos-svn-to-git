#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>

static struct Taskstate ts;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Falt",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
idt_init(void)
{
	extern struct Segdesc gdt[];
	
	// LAB 3: Your code here.

	// trap handler funtions defined in trapentry.S
	extern void trap_divide();
	extern void trap_debug();
	extern void trap_nmi();
	extern void trap_brkpt();
	extern void trap_oflow();
	extern void trap_bound();
	extern void trap_illop();
	extern void trap_device();
	extern void trap_dblflt();
	//extern void trap_coproc();
	extern void trap_tss();
	extern void trap_segnp();
	extern void trap_stack();
	extern void trap_gpflt();
	extern void trap_pgflt();
	//extern void trap_res();
	extern void trap_fperr();
	extern void trap_align();
	extern void trap_mchk();
	extern void trap_simderr();
	extern void trap_syscall();

	extern void irq0_handler();
	extern void irq1_handler();
	extern void irq2_handler();
	extern void irq3_handler();
	extern void irq4_handler();
	extern void irq5_handler();
	extern void irq6_handler();
	extern void irq7_handler();
	extern void irq8_handler();
	extern void irq9_handler();
	extern void irq10_handler();
	extern void irq11_handler();
	extern void irq12_handler();
	extern void irq13_handler();
	extern void irq14_handler();
	extern void irq15_handler();

	SETGATE(idt[T_DIVIDE], 1, GD_KT, trap_divide, 0);
	SETGATE(idt[T_DEBUG], 1, GD_KT, trap_debug, 0);
	SETGATE(idt[T_NMI], 0, GD_KT, trap_nmi, 0);
	SETGATE(idt[T_BRKPT], 1, GD_KT, trap_brkpt, 3);
	SETGATE(idt[T_OFLOW], 1, GD_KT, trap_oflow, 0);
	SETGATE(idt[T_BOUND], 1, GD_KT, trap_bound, 0);
	SETGATE(idt[T_ILLOP], 1, GD_KT, trap_illop, 0);
	SETGATE(idt[T_DEVICE], 1, GD_KT, trap_device, 0);
	SETGATE(idt[T_DBLFLT], 1, GD_KT, trap_dblflt, 0);
	//SETGATE(idt[T_COPROC], 0, GD_KT, trap_coproc, 0);
	SETGATE(idt[T_TSS], 1, GD_KT, trap_tss, 0);
	SETGATE(idt[T_SEGNP], 1, GD_KT, trap_segnp, 0);
	SETGATE(idt[T_STACK], 1, GD_KT, trap_stack, 0);
	SETGATE(idt[T_GPFLT], 1, GD_KT, trap_gpflt, 0);
	SETGATE(idt[T_PGFLT], 1, GD_KT, trap_pgflt, 0);
	//SETGATE(idt[T_RES], 0, GD_KT, trap_res, 0);
	SETGATE(idt[T_FPERR], 1, GD_KT, trap_fperr, 0);
	SETGATE(idt[T_ALIGN], 1, GD_KT, trap_align, 0);
	SETGATE(idt[T_MCHK], 1, GD_KT, trap_mchk, 0);
	SETGATE(idt[T_SIMDERR], 1, GD_KT, trap_simderr, 0);

	//Initial system call entry
	SETGATE(idt[T_SYSCALL], 1, GD_KT, trap_syscall, 3);

	//Initial IRQ handlers
	SETGATE(idt[IRQ_OFFSET], 0, GD_KT, irq0_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 1], 0, GD_KT, irq1_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 2], 0, GD_KT, irq2_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 3], 0, GD_KT, irq3_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 4], 0, GD_KT, irq4_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 5], 0, GD_KT, irq5_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 6], 0, GD_KT, irq6_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 7], 0, GD_KT, irq7_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 8], 0, GD_KT, irq8_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 9], 0, GD_KT, irq9_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 10], 0, GD_KT, irq10_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 11], 0, GD_KT, irq11_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 12], 0, GD_KT, irq12_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 13], 0, GD_KT, irq13_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 14], 0, GD_KT, irq14_handler, 0);
	SETGATE(idt[IRQ_OFFSET + 15], 0, GD_KT, irq15_handler, 0);

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);

	// Load the IDT
	asm volatile("lidt idt_pd");
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
 	switch (tf->tf_trapno) {
 		case T_PGFLT:
 			page_fault_handler(tf);
 			return;
 		case T_BRKPT:
 			monitor(tf);
 			return;
 		case T_SYSCALL:
 			tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax, 
 															tf->tf_regs.reg_edx, 
 															tf->tf_regs.reg_ecx,
 															tf->tf_regs.reg_ebx,
 															tf->tf_regs.reg_edi,
 															tf->tf_regs.reg_esi);
			return;
	}

	// Handle clock and serial interrupts.
	// LAB 4: Your code here.
	if (tf->tf_trapno == IRQ_OFFSET+IRQ_TIMER) {
		if(tf->tf_cs == GD_KT) {
			return;
		}
		else {
			sched_yield();
			return;
		}
	}

	// Handle keyboard interrupts.
	// LAB 5: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		assert(curenv);
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
	
	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNABLE)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	
	// LAB 3: Your code here.
	if (tf->tf_cs == 0x8) 
		panic("page_fault_handler: page fault in kernel mode va %08x ip %08x", fault_va,  tf->tf_eip);

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack, or the exception stack overflows,
	// then destroy the environment that caused the fault.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').
	
	// LAB 4: Your code here.
	if (curenv->env_pgfault_upcall) {
		user_mem_assert(curenv, (void *)(UXSTACKTOP-4), 4, 0);
		struct UTrapframe utf;

		utf.utf_fault_va = fault_va;
		utf.utf_err = tf->tf_err;
		utf.utf_regs = tf->tf_regs;
		utf.utf_eflags = tf->tf_eflags;
		utf.utf_eip = tf->tf_eip;
		utf.utf_esp = tf->tf_esp;

		//If in a recursive pgfault call
		if (tf->tf_esp >= UXSTACKTOP-PGSIZE && tf->tf_esp < UXSTACKTOP)
			tf->tf_esp -= 4;
		else
			tf->tf_esp = UXSTACKTOP;
		
		tf->tf_esp -= sizeof(struct UTrapframe);

		//Stack overflow
		if (tf->tf_esp < UXSTACKTOP-PGSIZE) {
			cprintf("[%08x] user exception stack overflowed: va %08x ip %08x esp %08x\n",	curenv->env_id, fault_va, tf->tf_eip, tf->tf_esp);
			print_trapframe(tf);
			env_destroy(curenv);
			return;
		}

		*(struct UTrapframe *) tf->tf_esp = utf;

		tf->tf_eip = (unsigned int) curenv->env_pgfault_upcall;
		env_run(curenv);
		cprintf("[DEBUG] should not reach here!\n");
	}

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

