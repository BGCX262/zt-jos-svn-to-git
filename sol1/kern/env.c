/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

struct Env *envs = NULL;		// All environments
struct Env *curenv = NULL;	        // The current env

static struct Env_list env_free_list;	// Free list

//
// Calculates the envid for env e.  
//
static u_int
mkenvid(struct Env *e)
{
	static u_long next_env_id = 0;
	// lower bits of envid hold e's position in the envs array
	u_int idx = e - envs;
	// high bits of envid hold an increasing number
	return(++next_env_id << (1 + LOG2NENV)) | idx;
}

//
// Converts an envid to an env pointer.
//
// RETURNS
//   env pointer -- on success and sets *error = 0
//   NULL -- on failure, and sets *error = the error number
//
int
envid2env(u_int envid, struct Env **penv, int checkperm)
{
	struct Env *e;

	// If envid is specified as zero,
	// just assume the current environment by default.
	if (envid == 0) {
		*penv = curenv;
		return 0;
	}

	// Look up the Env structure via the index part of the envid,
	// then check the env_id field in that struct Env
	// to ensure that the envid is not stale
	// (i.e., does not refer to a _previous_ environment
	// that used the same slot in the envs[] array).
	e = &envs[ENVX(envid)];
	if (e->env_status == ENV_FREE || e->env_id != envid) {
		*penv = 0;
		return -E_BAD_ENV;
	}

	// Check that the calling environment has legitimate permission
	// to manipulate the specified environment.
	// If checkperm is set, the specified environment
	// must be either the current environment
	// or an immediate child of the current environment.
	if (checkperm) {
		// Your code here in Lab 4
        if((e->env_parent_id != curenv->env_id)
                &&(e->env_id != curenv->env_id)) 
            return -E_BAD_ENV;
	}
	*penv = e;
	return 0;
}

//
// Marks all environments in 'envs' as free and inserts them into 
// the env_free_list.  Insert in reverse order, so that
// the first call to env_alloc() returns envs[0].
//
void
env_init(void)
{
    int i;
    LIST_INIT(&env_free_list);
    for(i = NENV-1; i >= 0; i--)
    {
        envs[i].env_status = ENV_FREE;
        LIST_INSERT_HEAD(&env_free_list, &envs[i], env_link);
    }
}

//
// Initializes the kernel virtual memory layout for environment e.
// Allocates a page directory and initializes
// the kernel portion of the new environment's address space.
// Also sets e->env_cr3 and e->env_pgdir accordingly.
// We do NOT (yet) map anything into the user portion
// of the environment's virtual address space.
//
// RETURNS
//   0 -- on sucess
//   <0 -- otherwise 
//
static int
env_setup_vm(struct Env *e)
{
	int i, r;
	struct Page *p = NULL;

	// Allocate a page for the page directory
	if ((r = page_alloc(&p)) < 0)
		return r;

	// Hint:
	//    - The VA space of all envs is identical above UTOP
	//      (except at VPT and UVPT).
	//    - Use boot_pgdir as a template.
	//    - You do not need to make any more calls to page_alloc.
	//    - Note: pp_ref is not maintained
	//	for physical pages mapped above UTOP.
    e->env_pgdir = (u_long)KADDR(page2pa(p));
    e->env_cr3 = PADDR(e->env_pgdir);
    memcpy(e->env_pgdir, boot_pgdir, BY2PG);


	// ...except at VPT and UVPT.  These map the env's own page table
	e->env_pgdir[PDX(VPT)]   = e->env_cr3 | PTE_P | PTE_W;
	e->env_pgdir[PDX(UVPT)]  = e->env_cr3 | PTE_P | PTE_U;

	return 0;
}

//
// Allocates and initializes a new env.
//
// RETURNS
//   0 -- on success, sets *new to point at the new env 
//   <0 -- on failure
//
int
env_alloc(struct Env **new, u_int parent_id)
{
	int r;
	struct Env *e;

	if (!(e = LIST_FIRST(&env_free_list)))
		return -E_NO_FREE_ENV;

	// Allocate and set up the page directory for this environment.
	if ((r = env_setup_vm(e)) < 0)
		return r;

	// Generate an env_id for this environment,
	// and set the basic status variables.
	e->env_id = mkenvid(e);
	e->env_parent_id = parent_id;
	e->env_status = ENV_RUNNABLE;

	// Clear out all the saved register state,
	// to prevent the register values
	// of a prior environment inhabiting this Env structure
	// from "leaking" into our new environment.
	memset(&e->env_tf, 0, sizeof(e->env_tf));

	// Set up appropriate initial values for the segment registers.
	// GD_UD is the user data segment selector in the GDT, and 
	// GD_UT is the user text segment selector (see inc/pmap.h).
	// The low 2 bits of each segment register
	// contains the Requestor Privilege Level (RPL);
	// 3 means user mode.
	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_esp = USTACKTOP;
	e->env_tf.tf_cs = GD_UT | 3;


	// You also need to set tf_eip to the correct value at some point.
	// Hint: see load_icode

	// Clear the page fault handler until user installs one.
	e->env_pgfault_entry = 0;

	// Also clear the IPC receiving flag.
	e->env_ipc_recving = 0;


	// commit the allocation
	LIST_REMOVE(e, env_link);
	*new = e;

    if(e == &envs[1])
    {
        e->env_tf.tf_eflags |= FL_IOPL0|FL_IOPL1;
    }

	//printf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
	return 0;
}

// Allocate and map all required pages into an env's address space
// to cover virtual addresses va through va+len-1 inclusive.
// Does not zero or otherwise initialize the mapped pages in any way.
// Panic if any allocation attempt fails.
//
// Warning: Neither va nor len are necessarily page-aligned!
// You may assume, however, that nothing is already mapped
// in the pages touched by the specified virtual address range.
static void
map_segment(struct Env *e, u_int va, u_int len)
{
	// Your code here.
    struct Page *p;
    u_int i;
    u_int end = va + len;
    va = ROUNDDOWN(va, BY2PG);
    /*len = ROUND(len, BY2PG);*/
    end = ROUND(end, BY2PG);
    for(i = va; i <= end; i += BY2PG)
    {
        if(page_alloc(&p) < 0)
            panic("Alloc error: not enough memory!\n");
        if(page_insert(e->env_pgdir, p, i, PTE_W|PTE_U))
            panic("page_insert\n");
    }
}

//
// Set up the the initial stack and program binary for a user process.
// This function is ONLY called during kernel initialization,
// before running the first user-mode environment.
//
// This function loads all loadable segments from the ELF binary image
// into the environment's user memory, starting at the appropriate
// virtual addresses indicated in the ELF program header.
// At the same time it clears to zero any portions of these segments
// that are marked in the program header as being mapped
// but not actually present in the ELF file - i.e., the program's bss section.
//
// Finally, this function maps one page for the program's initial stack
// at virtual address USTACKTOP - BY2PG.
//
static void
load_icode(struct Env *e, u_char *binary, u_int size)
{
	// Hint: 
	//  Use map_segment() to map memory for each program segment.
	//  Only use segments with ph->p_type == ELF_PROG_LOAD.
	//  Each segment's virtual address can be found in ph->p_va
	//  and its size in memory can be found in ph->p_memsz.
	//  For each segment, load the first ph->p_filesz bytes from the ELF
	//  binary and clear any remaining bytes in the segment to zero.
	//
	// Hint:
	//  Loading the segments is much simpler if you set things up
	//  so that you can access them via the user's virtual addresses!
    struct Elf *elf;
    struct Proghdr *ph;
    u_int i;
    elf = (struct Elf *)binary;
    if(elf->e_magic != 0x464C457F)
        panic("Invalid elf header");
    e->env_tf.tf_eip = elf->e_entry;
    e->env_tf.tf_esp = USTACKTOP;
    map_segment(e, USTACKTOP - BY2PG, BY2PG);
    ph = (struct Proghdr *)(binary + elf->e_phoff);
    for(i = 0; i < elf->e_phnum; i++, ph++)
        if(ph->p_type == ELF_PROG_LOAD)
        {
            map_segment(e, ph->p_va, ph->p_memsz);
        }
    lcr3(e->env_cr3);
    ph = (struct Proghdr *)(binary + elf->e_phoff);
    for(i = 0; i < elf->e_phnum; i++, ph++)
        if(ph->p_type == ELF_PROG_LOAD)
        {
            memcpy(ph->p_va, binary+ph->p_offset, ph->p_filesz);
            if(ph->p_memsz-ph->p_filesz > 0)
                memset(ph->p_va, 0, ph->p_memsz-ph->p_filesz);
        }
    lcr3(boot_cr3);
}

//
// Allocates a new env and loads the elf binary into it.
// This function is ONLY called during kernel initialization,
// before running the first user-mode environment.
// The new env's parent env id is set to 0.
void
env_create(u_char *binary, int size)
{
    struct Env *e;
    if(env_alloc(&e, 0) < 0)
    {
        panic("alloc env error!\n");
    }
    load_icode(e, binary, size);
}

//
// Frees env e and all memory it uses.
// 
void
env_free(struct Env *e)
{
	Pte *pt;
	u_int pdeno, pteno, pa;

	// Note the environment's demise.
	//printf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

	// Flush all mapped pages in the user portion of the address space
	static_assert(UTOP%PDMAP == 0);
	for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {

		// only look at mapped page tables
		if (!(e->env_pgdir[pdeno] & PTE_P))
			continue;

		// find the pa and va of the page table
		pa = PTE_ADDR(e->env_pgdir[pdeno]);
		pt = (Pte*)KADDR(pa);

		// unmap all PTEs in this page table
		for (pteno = 0; pteno <= PTX(~0); pteno++) {
			if (pt[pteno] & PTE_P)
				page_remove(e->env_pgdir,
					(pdeno << PDSHIFT) |
					(pteno << PGSHIFT));
		}

		// free the page table itself
		e->env_pgdir[pdeno] = 0;
		page_decref(pa2page(pa));
	}

	// free the page directory
	pa = e->env_cr3;
	e->env_pgdir = 0;
	e->env_cr3 = 0;
	page_decref(pa2page(pa));

	// return the environment to the free list
	e->env_status = ENV_FREE;
	LIST_INSERT_HEAD(&env_free_list, e, env_link);
}

//
// Frees env e.  And schedules a new env
// if e was the current env.
//
void
env_destroy(struct Env *e) 
{
	env_free(e);

	if (curenv == e) {
		curenv = NULL;
		sched_yield();
	}
}


//
// Restores the register values in the Trapframe
//  (does not return)
//
void
env_pop_tf(struct Trapframe *tf)
{
#if 0
	printf(" --> %d 0x%x\n", ENVX(curenv->env_id), tf->tf_eip);
#endif

	asm volatile("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tiret"
		:: "g" (tf) : "memory");
	panic("iret failed");  /* mostly to placate the compiler */
}

//
// Context switch from curenv to env e.
// Note: if this is the first call to env_run, curenv is NULL.
//  (This function does not return.)
//
void
env_run(struct Env *e)
{
	// save the register state of the previously executing environment
	if (curenv) {
		// Your code here.
		// Hint: this can be done in a single line of code.
        memcpy((void *)&curenv->env_tf, (void *)UTF, sizeof(struct Trapframe));
	//	panic("need to save previous env's register state!");
	}

    e->env_runs++;
	// step 1: set curenv to the new environment to be run.
    curenv = e;
	// step 2: use lcr3 to switch to the new environment's address space.
    lcr3(e->env_cr3);
	// step 3: use env_pop_tf() to restore the new environment's registers
    env_pop_tf(&e->env_tf);
	//	and drop into user mode in the new environment.
}


