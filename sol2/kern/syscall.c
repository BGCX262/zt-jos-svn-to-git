/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>


//every env's tickets related to switch of status should be changed
//system call is the only part which will deal with the env's status change  	

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
	
	// LAB 3: Your code here.
	//cprintf("curenv env id:%08x,s:%08x,len:%08x\n",curenv->env_id,s,len);
	user_mem_assert(curenv, s, len, 0);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console.
// Returns the character.
static int
sys_cgetc(void)
{
	int c;

	// The cons_getc() primitive doesn't wait for a character,
	// but the sys_cgetc() system call does.
	while ((c = cons_getc()) == 0)
		/* do nothing */;

	return c;
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
	
	// LAB 4: Your code here.
	struct Env * child ;
	if(env_alloc(&child, curenv->env_id) < 0)
		return -E_NO_FREE_ENV;
	else
	{
		//cprintf("curenv id:%08x\n",curenv->env_id);
		if(child->env_status == ENV_RUNNABLE)
		{
			global_tickets -= child->tickets;
			child->tickets = 0;
		}
		child->env_status = ENV_NOT_RUNNABLE;


		child->env_tf = curenv->env_tf;
		child->env_pgfault_upcall = curenv->env_pgfault_upcall;
		//	cprintf("curenv->env_tf:%08x\n",curenv->env_tf);
		child->env_tf.tf_regs.reg_eax = 0;


		return child->env_id;
	}
		       	

	//panic("sys_exofork not implemented");
}


// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
  	// Hint: Use the 'envid2env' function from kern/env.c to translate an
  	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.
	
	// LAB 4: Your code here.

	struct Env * env; 
	if(envid2env(envid, &env, 1) == -E_BAD_ENV)
		return -E_BAD_ENV;
	if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
	{
		//cprintf("status:%08x\n",status);
		return -E_INVAL;
	}
	else
	{


		if(status == ENV_RUNNABLE && env->env_status != ENV_RUNNABLE)
		{
			env->tickets = INIT_TICKET;
			global_tickets += env->tickets;


		}
		else if(status != ENV_RUNNABLE && env->env_status == ENV_RUNNABLE)
		{
			global_tickets -= env->tickets;
			env->tickets = 0;
		}
		
		env -> env_status = status;
		return 0;
	}
	//panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 4: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *env;
	int r;
	if((r = envid2env(envid, &env,1)) < 0)
		return -E_BAD_ENV;
	tf->tf_eflags |= FL_IF;

	env->env_tf = *tf;
	return 0;



	//panic("sys_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *env;
	if(envid2env(envid,&env,1) == -E_BAD_ENV)
		return -E_BAD_ENV;
	env -> env_pgfault_upcall = func;
	//cprintf("env_pgfault_upcall:%08x\n",env->env_pgfault_upcall);
	user_mem_assert(env, (void *)(env->env_pgfault_upcall), 4,0);
	//cprintf("set pgfault up call successfully\n");
	return 0;
	//panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	struct Env *env;

	if(envid2env(envid, &env, 1) == -E_BAD_ENV)
	{
		//cprintf("bad environment envid in sys page alloc:%08x\n",envid);
		return -E_BAD_ENV;
	}
	else if((uintptr_t)va >= UTOP || (uint32_t)va % PGSIZE)
	       return -E_INVAL;
	else if((perm & PTE_U) && (perm & PTE_P))
	{
		if(perm & ((~(PTE_U|PTE_P|PTE_W|PTE_AVAIL) & 0xfff)))
			return -E_INVAL;		
	}
	if((vpd[PDX(va)] & PTE_P) && (vpt[VPN(va)] & PTE_P))
		page_remove(env->env_pgdir,va);

	//cprintf("env id:%08x\n",env->env_id);	
	struct Page * page;
	if(page_alloc(&page) == -E_NO_MEM)
		return -E_NO_MEM;
	//cprintf("page alloc kva:%08x\n",page2kva(page));
	// At this time, we use the page table of the kernel
	// so we clear the phsical page according to the kernel virtual address 
	memset(page2kva(page),0x0,PGSIZE);
	//cprintf("page insert,env_id:%08x,env_pgdir:%08x,page:%08x,va:%08x\n",
	//		env->env_id,env->env_pgdir,page,va);
	if(page_insert(env -> env_pgdir, page, va, perm) != 0)
	{
		page_free(page);
		return -E_NO_MEM;
	}
	//cprintf("page insert success\n");
	
	return 0;
	//panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	struct Env *srcenv,*dstenv;
	pte_t *pte;
	struct Page *page;
	if((envid2env(srcenvid, &srcenv, 1) == -E_BAD_ENV) ||
			(envid2env( dstenvid, &dstenv,1) == -E_BAD_ENV))
	{
		//cprintf("return bad environment\n");
		return -E_BAD_ENV;
	}
	if((uint32_t)srcva >= UTOP || (uint32_t)srcva % PGSIZE ||
		(uint32_t)dstva >= UTOP || (uint32_t)dstva % PGSIZE)
		return -E_INVAL;
	if((page = page_lookup(srcenv->env_pgdir,srcva,&pte)) == NULL)
		return  -E_INVAL;
	if((perm & PTE_U) && (perm & PTE_P))
	{
		if(perm & ((~(PTE_U|PTE_P|PTE_W|PTE_AVAIL)) & 0xfff))
			return -E_INVAL;		
	}
	if((perm & PTE_W) && !(PTE_W & *pte))
		return -E_INVAL;
	if(page_insert(dstenv -> env_pgdir, page, dstva,perm) == -E_NO_MEM)
	        return -E_NO_MEM;
	//tlb_invalidate(dstenv->env_pgdir,dstva);
	return 0;
	
	//panic("sys_page_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
	
	// LAB 4: Your code here.
	struct Env *env;
	if(envid2env(envid, &env, 1) == -E_BAD_ENV)
		return -E_BAD_ENV;
	else if((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE)
		return -E_INVAL;
	else
	{
		page_remove(env->env_pgdir,va);
		return 0;
	}
	//panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If va != 0, then also send page currently mapped at 'va',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target has not requested IPC with sys_ipc_recv.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused ipc_recv system call.
//
// If the sender sends a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc doesn't happen unless no errors occur.
//
// Returns 0 on success where no page mapping occurs,
// 1 on success where a page mapping occurs, and < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	
	//cprintf("envid in try send:%08x\n",envid);
	struct Env *env,dstenv;
	int32_t ret;
	struct Page *page;
	pte_t *pte;
	if(envid2env(envid,&env,0) != 0)
		return -E_BAD_ENV;
	if(env->env_ipc_recving == 0)
		return -E_IPC_NOT_RECV;
	else
	{
		//cprintf("try send set ipc:%8x not recv\n",envid);
		env->env_ipc_recving = 0;
		env->env_ipc_from = curenv->env_id;
		env->env_ipc_value = value;
		if(srcva == 0 ||env->env_ipc_dstva == 0)
		{
			env->env_ipc_perm = 0;
			if(env->env_status != ENV_RUNNABLE)
			{
				env->tickets = INIT_TICKET;
				global_tickets += env->tickets;
			}
			env->env_status = ENV_RUNNABLE;
			return 0;
		}
		else if((uint32_t)srcva < UTOP)
		{
			env->env_ipc_perm = perm;
			if((page = page_lookup(curenv->env_pgdir,srcva,&pte)) == NULL)
				return -E_INVAL;
			if((ret = page_insert(env->env_pgdir, page, env->env_ipc_dstva, perm)) < 0)
	        		return ret;
			if(ret == 0)
			{
				if(env->env_status != ENV_RUNNABLE)
				{
					env->tickets = INIT_TICKET;
					global_tickets += env->tickets;
				}
				env->env_status = ENV_RUNNABLE;				
				return 1;
			}
		}
		return 0;
	}	
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	struct Env *curenv;
	if(envid2env(0, &curenv, 0) != 0)
		panic("ipc recv:no such envid");
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = 0;
	if((uint32_t)dstva < UTOP)
	{
		if(((uint32_t)dstva % PGSIZE) == 0)
			curenv->env_ipc_dstva = dstva;
		else
			return -E_INVAL;
	}
	if(curenv-> env_status == ENV_RUNNABLE)
	{
		global_tickets -= curenv->tickets;
		curenv->tickets = 0;
	}
	curenv->env_status = ENV_NOT_RUNNABLE;
	return 0;
}


//new added sys_for_fork to 
//augment the system call interface 
//so that it is possible to send a batch of system calls at once
//because switching into the kernel has non-trivial cost!!!
static int
sys_for_fork(envid_t envid, void * func, int status)
{
	int r;
	int perm = PTE_W|PTE_P|PTE_U;
	void * va = (void*)(UXSTACKTOP - PGSIZE);

	if((r = sys_page_alloc(envid, va, perm)) < 0)
			return r;
	if ((r = sys_page_map(envid, va, curenv->env_id, UTEMP, perm)) < 0)
			panic("sys_page_map: %e", r);
	memmove(UTEMP, va, PGSIZE);
	if ((r = sys_page_unmap(curenv->env_id, UTEMP)) < 0)
			panic("sys_page_unmap: %e", r);
	if ((r = sys_env_set_pgfault_upcall(envid, func)) < 0)
		return r;
	if ((r = sys_env_set_status(envid, status)) < 0)
		return r;
	return 0;
}

static int
sys_set_shforkid(envid_t envid)
{
	shforkid = envid;
	//cprintf("sys set shell fork id %08x\n",shforkid);
	return 0;

}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	uint32_t result = -E_INVAL;
	//cprintf("syscallno:%08x\n",syscallno);
	switch(syscallno)
	{
		case SYS_cputs:
			sys_cputs((char *)a1,(size_t)a2);
			result = SYS_cputs;
			break;
		case SYS_getenvid:
			result = sys_getenvid();
			break;
		case SYS_cgetc:
			result = sys_cgetc();
			break;
		case SYS_env_destroy:
			result = sys_env_destroy((envid_t)a1);
			break;
		case SYS_env_set_trapframe:
			result = sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
			break;
		case SYS_yield:
			sys_yield();
			result = SYS_yield;
			break;
		case SYS_exofork:
			result = sys_exofork();
			break;
		case SYS_env_set_status:
			result = sys_env_set_status((envid_t)a1,(int)a2);
			break;
		case SYS_page_alloc:
			result = sys_page_alloc((envid_t)a1,(void *)a2,(int)a3);
			break;
		case SYS_page_map:
			result = sys_page_map((envid_t)a1,(void *)a2,(envid_t)a3,(void *)a4,(int)a5);
			break;
		case SYS_page_unmap:
			result = sys_page_unmap((envid_t)a1,(void *)a2);
			break;	
		case SYS_env_set_pgfault_upcall:
			result = sys_env_set_pgfault_upcall((envid_t)a1,(void*)a2);
			break;	
		case SYS_ipc_recv:
			result = sys_ipc_recv((void*)a1);
			break;
		case SYS_ipc_try_send:
			result = sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
			break;
		case SYS_for_fork:
			result = sys_for_fork((envid_t)a1, (void *)a2, (int)a3);
			break;
		case SYS_set_shforkid:
			result = sys_set_shforkid((envid_t)a1);
			break;	
			
		default:
			result = -E_INVAL;
	}
	return result;
}

