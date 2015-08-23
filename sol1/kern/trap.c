
#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/env.h>
#include <kern/console.h>
#include <kern/syscall.h>
#include <kern/monitor.h>
#include <kern/picirq.h>

u_int page_fault_mode = PFM_NONE;
static struct Taskstate ts;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { {0}, };
struct Pseudodesc idt_pd =
{
	0, sizeof(idt) - 1, (unsigned long) idt,
};


#define EXTERNFUNC(func, num) extern void func(void)
EXTERNFUNC(divide, T_DIVIDE);
EXTERNFUNC(debug, T_DEBUG);
EXTERNFUNC(nmi, T_NMI);
EXTERNFUNC(brkpt, T_BRKPT);
EXTERNFUNC(oflow, T_OFLOW);
EXTERNFUNC(bound, T_BOUND);
EXTERNFUNC(illop, T_ILLOP);
EXTERNFUNC(device, T_DEVICE);
EXTERNFUNC(tss, T_TSS);
EXTERNFUNC(segnp, T_SEGNP);
EXTERNFUNC(stack, T_STACK);
EXTERNFUNC(gpflt, T_GPFLT);
EXTERNFUNC(pgflt, T_PGFLT);
EXTERNFUNC(fperr, T_FPERR);
EXTERNFUNC(align, T_ALIGN);
EXTERNFUNC(mchk, T_MCHK);

#define SETTRAPGATE(func, num) do {SETGATE(idt[num], 1, GD_KT, func, 0)}while(0)

static void idt_install(void)
{
    extern void sys_call(void);
    extern void tick_action(void);
    extern void kbd_action(void);
    SETTRAPGATE(divide, T_DIVIDE);
    SETTRAPGATE(debug, T_DEBUG);
    SETTRAPGATE(nmi, T_NMI);
    //SETTRAPGATE(brkpt, T_BRKPT);
    SETGATE(idt[T_BRKPT], 1, GD_KT, brkpt, 3);
    SETTRAPGATE(oflow, T_OFLOW);
    SETTRAPGATE(bound, T_BOUND);
    SETTRAPGATE(illop, T_ILLOP);
    SETTRAPGATE(device, T_DEVICE);
    SETTRAPGATE(tss, T_TSS);
    SETTRAPGATE(segnp, T_SEGNP);
    SETTRAPGATE(stack, T_STACK);
    SETTRAPGATE(gpflt, T_GPFLT);
    SETTRAPGATE(pgflt, T_PGFLT);
    SETTRAPGATE(fperr, T_FPERR);
    SETTRAPGATE(align, T_ALIGN);
    SETTRAPGATE(mchk, T_MCHK);
    SETGATE(idt[T_SYSCALL], 0, GD_KT, sys_call, 3);
    SETGATE(idt[IRQ_OFFSET], 0, GD_KT, tick_action, 3);
    SETGATE(idt[IRQ_KBD], 0, GD_KT, kbd_action, 3);
}

static const char *trapname(int trapno)
{
	static const char *excnames[] = {
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

	return "(unknown trap)";
}


void
idt_init(void)
{
	extern struct Segdesc gdt[];

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Love to put this code in the initialization of gdt,
	// but the compiler generates an error incorrectly.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (u_long) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);
    idt_install();
	// Load the IDT
	asm volatile("lidt idt_pd+2");
}


void
print_trapframe(struct Trapframe *tf)
{
	printf("TRAP frame at %p\n", tf);
	printf("  edi  0x%08x\n", tf->tf_edi);
	printf("  esi  0x%08x\n", tf->tf_esi);
	printf("  ebp  0x%08x\n", tf->tf_ebp);
	printf("  oesp 0x%08x\n", tf->tf_oesp);
	printf("  ebx  0x%08x\n", tf->tf_ebx);
	printf("  edx  0x%08x\n", tf->tf_edx);
	printf("  ecx  0x%08x\n", tf->tf_ecx);
	printf("  eax  0x%08x\n", tf->tf_eax);
	printf("  es   0x----%04x\n", tf->tf_es);
	printf("  ds   0x----%04x\n", tf->tf_ds);
	printf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	printf("  err  0x%08x\n", tf->tf_err);
	printf("  eip  0x%08x\n", tf->tf_eip);
	printf("  cs   0x----%04x\n", tf->tf_cs);
	printf("  flag 0x%08x\n", tf->tf_eflags);
	printf("  esp  0x%08x\n", tf->tf_esp);
	printf("  ss   0x----%04x\n", tf->tf_ss);
}

void
trap(struct Trapframe *tf)
{
    int ret;

	// Handle processor exceptions
	// Your code here.
    switch(tf->tf_trapno)
    {
        case T_BRKPT:
            while(1)
                monitor(NULL);
            break;
        case T_PGFLT:
            page_fault_handler(tf);
            tf->tf_eflags |= FL_IF;
            //env_pop_tf(tf);
            memcpy(&curenv->env_tf, tf, sizeof(*tf));
            env_run(curenv);
            return;
        case T_SYSCALL:
            ret = syscall(tf->tf_eax, tf->tf_edx, tf->tf_ecx, tf->tf_ebx,
                    tf->tf_edi, tf->tf_esi);
            tf->tf_eax = ret;
            tf->tf_eflags |= FL_IF;
            // env_pop_tf(tf);
            memcpy(&curenv->env_tf, tf, sizeof(*tf));
            env_run(curenv);
        case IRQ_OFFSET:
            sched_yield();
            break;
        case IRQ_KBD:
            kbd_intr();
            memcpy(&curenv->env_tf, tf, sizeof(*tf));
            env_run(curenv);
            break;
        default:
            break;
    }

	// the user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

struct user_stack_frame
{
    u_int fault_va;
    u_int tf_err;
    u_int esp;
    u_int eflags;
    u_int eip;
    u_int empty0;
    u_int empty1;
    u_int empty2;
    u_int empty3;
    u_int empty4;
    u_int empty5;
};

static u_long backtrace_intrap(u_long ebp)
{
    printf("[frame] %08x\n", *(u_long *)(ebp + 4));
    return *(u_long *)ebp;
}

void
page_fault_handler(struct Trapframe *tf)
{
	u_int fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();
    
    if(!(tf->tf_cs & 0x3))
    {
        // This happens in kernel mode
        if(page_fault_mode == PFM_NONE) {
            int i;
            u_long ebp = tf->tf_ebp;
            for(i = 0; i < 3; i++) {
                ebp = backtrace_intrap(ebp);
            }
            panic("Aiee, page fault in kernel mode va %08x ip %08x\n", fault_va, tf->tf_eip);
        }
        else
        {
            Pte *pte;
            u_long va = fault_va;
            printf("[%08x] PFM_KILL va %08x ip %08x\n",
                    curenv->env_id, fault_va, tf->tf_eip);
            printf("curenv->env_pgdir[PDX(va)] = %08x\n", curenv->env_pgdir[PDX(va)]);
            pte = KADDR(PTE_ADDR(curenv->env_pgdir[PDX(va)]));
            printf("pte[PTX(va)] = %08x\n", pte[PTX(va)]);
            page_fault_mode = PFM_NONE;
            env_destroy(curenv);
        }
        return;
    }

    if(curenv->env_pgfault_entry)
    {
        u_int newsp;
        struct user_stack_frame *us;
        if(tf->tf_esp > UXSTACKTOP-BY2PG && 
                tf->tf_esp < UXSTACKTOP)
        {
            //Page fault within UXSTACKTOP
            newsp = tf->tf_esp - 8; // Reserve 2 words for eip and eflags
        }
        else
        {
            newsp = UXSTACKTOP;
        }
        if(newsp < UXSTACKTOP-BY2PG)
            goto fail;
        //printf("Trap env[%08x] va %08x ip %08x\n", curenv->env_id, fault_va, tf->tf_eip);
        //print_trapframe(tf);
        //printf("newsp = %08x, tf->tf_esp = %08x\n", newsp, tf->tf_esp);
        us = (struct user_stack_frame *)(newsp - sizeof(struct user_stack_frame));
        us->fault_va = fault_va;
        us->tf_err = tf->tf_err;
        us->esp = tf->tf_esp;
        us->eip = tf->tf_eip;
        us->eflags = tf->tf_eflags;
        tf->tf_esp = newsp;
        tf->tf_eip = curenv->env_pgfault_entry;
        return;
    }
fail:
	// User-mode exception - destroy the environment.
	printf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}


