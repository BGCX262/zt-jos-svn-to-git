/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */
//SETDATA(name) is used to automatically generate the table we need
//e.g _idt_entry and _irqhandler. It is just like an array
//pay attention to .data and .text
//These two directives are used to switch between data and code in at&t asm
//I hate AT&T assembly!!!!!!!!!!

#define SETDATA(name) .data;.long name; 
#define TRAPHANDLER(name, num)						\
  	SETDATA(name);                                                      \
        .text;                                                          \
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
        SETDATA(name);                                                     \
        .text;                                                          \
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

//This place defines the _idt_entry
.data
	.globl _idt_entry
_idt_entry:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

    TRAPHANDLER_NOEC(trap_divide, T_DIVIDE);
    TRAPHANDLER_NOEC(trap_debug, T_DEBUG);
    TRAPHANDLER_NOEC(trap_nmi, T_NMI);
    TRAPHANDLER_NOEC(trap_brkpt, T_BRKPT);
    TRAPHANDLER_NOEC(trap_oflow, T_OFLOW);
    TRAPHANDLER_NOEC(trap_bound, T_BOUND);
    TRAPHANDLER_NOEC(trap_illop, T_ILLOP);
    TRAPHANDLER_NOEC(trap_device, T_DEVICE);
    TRAPHANDLER(trap_dblflt, T_DBLFLT);
    TRAPHANDLER_NOEC(trap_coproc, T_COPROC);
    TRAPHANDLER(trap_tss, T_TSS);
    TRAPHANDLER(trap_segnp, T_SEGNP);
    TRAPHANDLER(trap_stack, T_STACK);
    TRAPHANDLER(trap_gpflt, T_GPFLT);
    TRAPHANDLER(trap_pgflt, T_PGFLT);
    TRAPHANDLER_NOEC(trap_res, T_RES);
    TRAPHANDLER_NOEC(trap_fperr, T_FPERR);
    TRAPHANDLER_NOEC(trap_align, T_ALIGN);
    TRAPHANDLER_NOEC(trap_mchk, T_MCHK);
    TRAPHANDLER_NOEC(trap_simderr, T_SIMDERR);
    TRAPHANDLER_NOEC(trap_syscall, T_SYSCALL);

//this place defines _irqhandler
.data
	.globl _irqhandler
_irqhandler:

.text

    TRAPHANDLER_NOEC(irq0, IRQ_OFFSET);
    TRAPHANDLER_NOEC(irq1, IRQ_OFFSET + 1);
    TRAPHANDLER_NOEC(irq2, IRQ_OFFSET + 2);
    TRAPHANDLER_NOEC(irq3, IRQ_OFFSET + 3);
    TRAPHANDLER_NOEC(irq4, IRQ_OFFSET + 4);
    TRAPHANDLER_NOEC(irq5, IRQ_OFFSET + 5);
    TRAPHANDLER_NOEC(irq6, IRQ_OFFSET + 6);
    TRAPHANDLER_NOEC(irq7, IRQ_OFFSET + 7);
    TRAPHANDLER_NOEC(irq8, IRQ_OFFSET + 8);
    TRAPHANDLER_NOEC(irq9, IRQ_OFFSET + 9);
    TRAPHANDLER_NOEC(irq10, IRQ_OFFSET + 10);
    TRAPHANDLER_NOEC(irq11, IRQ_OFFSET + 11);
    TRAPHANDLER_NOEC(irq12, IRQ_OFFSET + 12);
    TRAPHANDLER_NOEC(irq13, IRQ_OFFSET + 13);
    TRAPHANDLER_NOEC(irq14, IRQ_OFFSET + 14);
    TRAPHANDLER_NOEC(irq15, IRQ_OFFSET + 15);



/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    //We should push into the stack to finish the trapframe
    //Some fields have already been set by x86 hardware
    //We just need to manually add these fields left
    //And don't forget to pop what we pushed after the call
    //iret will automatically pop the other fields left 
    pushl %ds;
    pushl %es;
    pushal;
    cli;
    /*cli's function is like below
     *pushfl;
     *popl %eax;
     *movl $FL_IF,%ebx;
     *notl %ebx;
     *andl %ebx, %eax;
     *pushl %eax;
     *popfl;      	
     */
    movw $GD_KD, %ax;
    movw %ax, %ds;
    movw %ax, %es;
    //esp is the argument to the trap. it points to the trapframe we pushed into the stack
    pushl %esp;
    call trap;
    popal;
    popl %es;
    popl %ds;
    //iret will recover the state of the env which cause the interrupt or exception
    iret;
	


