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

	//_idt_entry, _irqhandler and trap_syscall are defined trapentry.S	
	extern uint32_t _idt_entry[];	
	extern uint32_t _irqhandler[];
	extern uint32_t trap_syscall;        
	uint32_t i,istrap,dpl;
	
	for(i = 0; i <= T_SIMDERR; i++)
	{
		istrap = 1;
		dpl = 0;
		// trap 9 and trap 15 are reserved but no need to use
		// But we have reserved their slot in our idt
		if(i == 9 || i == 15)
			continue;
		if(i == T_NMI)
			istrap = 0;
		if(i == T_BRKPT)
			dpl = 3;
	      	SETGATE(idt[i],istrap,GD_KT,_idt_entry[i],dpl);
	}

	
		
	for(i = 0; i< MAX_IRQS; i++)
	{
		istrap = 0;
		dpl = 3;
		if(i == 0)
		{
			istrap = 1;
			dpl = 0;
		}

		SETGATE(idt[IRQ_OFFSET + i], istrap, GD_KT, _irqhandler[i],dpl);
	}


	SETGATE(idt[T_SYSCALL],0,GD_KT, &trap_syscall,3);

       
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS. We need the ss0 and esp0 of TSS to switch to kernel stack 
	// when exception or interruption occur
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
 	switch(tf->tf_trapno)
 	{
		case T_DEBUG:
			cprintf("single step INT1\n");

			return;
 		case T_PGFLT:
 			page_fault_handler(tf);
 			return;
 		case T_BRKPT:			
 			monitor(tf);
 			return;
 		case T_SYSCALL:
 			tf->tf_regs.reg_eax = 
 				syscall(tf->tf_regs.reg_eax,
 					tf->tf_regs.reg_edx,
 					tf->tf_regs.reg_ecx,
 					tf->tf_regs.reg_ebx,
 					tf->tf_regs.reg_edi,
 					tf->tf_regs.reg_esi);
 			return;
 		//case IRQ_OFFSET:
 		//	sched_yield();
 		//	return;
 	}	
	
	// Handle clock and serial interrupts.
	// LAB 4: Your code here.
 	if(tf->tf_trapno == IRQ_OFFSET)
 	{
 		if(tf->tf_cs == GD_KT)
 			return;
 		else
 		{
 			sched_yield();
 			return;
		}
 	}
	// Handle keyboard interrupts.
	// LAB 5: Your code here.
	if(tf->tf_trapno == IRQ_OFFSET+IRQ_KBD)
	{
		kbd_intr();
		return;
	}
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
	//cprintf("Incoming TRAP frame at %p\n", tf);
	//cprintf("tf-> tf_trapno: %08x\n",tf->tf_trapno);
	//cprintf("tf->tf_cs :%08x,GD_KT:%08x\n",tf->tf_cs ,GD_KT);
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
	{
		env_run(curenv);
	}
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
	if ((tf->tf_cs & 3) == 0) {
		// trapped from kernel mode
		// and we are in trouble...
		cprintf("kernel fault va %08x ip %08x\n",
				fault_va, tf->tf_eip);
		panic("page fault happend in kernel mode");
		return;
	}

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
	unsigned int orig_esp;
	struct UTrapframe utf;
	//cprintf("pgfault_upcall:%08x\n",curenv->env_pgfault_upcall);
	// Destroy the environment that caused the fault,
	// if 'env_pgfault_upcall' is null
	if (!curenv->env_pgfault_upcall) {
		cprintf("not page fault handler installed.\n");
		cprintf("[%08x] user fault va %08x ip %08x\n",
				curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
		return;
	}
	//cprintf("check %08x whether the user exception stack is accessible\n",curenv->env_id);
	//user_mem_assert(curenv, (void *)(UXSTACKTOP-4), 4,
	//				PTE_P | PTE_W | PTE_U);
	// added according to 'faultbadhandler' to check whether
	//cprintf("check %08x the page fault installed is accessible to the user\n",curenv->env_id);
	//user_mem_assert(curenv, (void *)(curenv->env_pgfault_upcall), 4,
	//				PTE_P | PTE_U);
	// initialize the utf according to tf
	utf.utf_fault_va = fault_va;
	//cprintf("fault va:%08x\n",fault_va);
	utf.utf_err = tf->tf_err;
	utf.utf_regs = tf->tf_regs;
	utf.utf_eip = tf->tf_eip;
	utf.utf_eflags = tf->tf_eflags;
	utf.utf_esp = tf->tf_esp;

	// tf->tf_esp is already on the user exception stack
	if (tf->tf_esp >= UXSTACKTOP-PGSIZE && tf->tf_esp < UXSTACKTOP)
	{
		cprintf("push an empty 32-bit word,tf->tf_esp:%08x\n",tf->tf_esp);
		tf->tf_esp -= 4;
	}
	else
		tf->tf_esp = UXSTACKTOP;
	// push user trap frame
	tf->tf_esp -= sizeof(struct UTrapframe);
	//cprintf("tf->tf_esp:%08x\n",tf->tf_esp);

	if (tf->tf_esp < UXSTACKTOP-PGSIZE) {
		cprintf("user exception stack overflowed.\n");
		cprintf("[%08x] user fault va %08x ip %08x\n",
				curenv->env_id, fault_va, tf->tf_eip);
		print_trapframe(tf);
		env_destroy(curenv);
		return;
	}
	*(struct UTrapframe *)(tf->tf_esp) = utf;

	tf->tf_eip = (unsigned int)curenv->env_pgfault_upcall;
	env_run(curenv);
}

