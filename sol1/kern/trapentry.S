/* See COPYRIGHT for copyright information. */

#include <inc/asm.h>
#include <inc/mmu.h>
#include <inc/pmap.h>
#include <inc/trap.h>
#include <kern/picirq.h>



###################################################################
# exceptions/interrupts
###################################################################

/* For certain traps the CPU automatically pushes an error code, for 
 * all other traps the IDTFUNC_NOEC() pushes a 0 in place of the error code,
 * so the trap frame has the same format.
 */
#define IDTFNC(name,num)      ENTRY(name)           pushl $(num); jmp _alltraps
#define IDTFNC_NOEC(name,num) ENTRY(name) pushl $0; pushl $(num); jmp _alltraps 


.text
_alltraps:
    cli        // disable interrupts
    pushl %ds
    pushl %es
    pushal
    movl $(GD_KD), %eax
    movl %eax, %ds
    movl %eax, %es
    pushl %esp
    call trap
    popl %esp
    popal
    popl %es
    pop  %ds
    addl 0x8, %esp
    sti
    iret

    IDTFNC(divide, T_DIVIDE);
    IDTFNC(debug, T_DEBUG);
    IDTFNC(nmi, T_NMI);
    IDTFNC(brkpt, T_BRKPT);
    IDTFNC(oflow, T_OFLOW);
    IDTFNC(bound, T_BOUND);
    IDTFNC(illop, T_ILLOP);
    IDTFNC(device, T_DEVICE);
    IDTFNC(reserved_9, 9);
    IDTFNC(tss, T_TSS);
    IDTFNC(segnp, T_SEGNP);
    IDTFNC(stack, T_STACK);
    IDTFNC(gpflt, T_GPFLT);
    IDTFNC(pgflt, T_PGFLT);
    IDTFNC(reserved_15, 15);
    IDTFNC(fperr, T_FPERR);
    IDTFNC(align, T_ALIGN);
    IDTFNC(mchk, T_MCHK);
    IDTFNC_NOEC(sys_call, T_SYSCALL);
    IDTFNC_NOEC(tick_action, IRQ_OFFSET);
    IDTFNC_NOEC(kbd_action, IRQ_KBD);
