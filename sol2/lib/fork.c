// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	//cprintf("pgfault\n");
	void *addr = (void *) utf->utf_fault_va;
	//cprintf("addr:%08x\n",addr);
	uint32_t err = utf->utf_err;
	int r;
	uint32_t pn;
	envid_t envid = sys_getenvid();

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err & FEC_WR))
		panic("not a write operation");
	pn = VPN(addr);
	if(!(vpt[pn] & PTE_COW))
		panic("vpt[pn]:%08x,not copy-on-write page",vpt[pn]);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	
	// LAB 4: Your code here.
	if((r = sys_page_alloc(envid,PFTEMP,PTE_W|PTE_U|PTE_P)) < 0)
		panic("pgfault page alloc%e",r);
	memmove((void*)PFTEMP, (void*)ROUNDDOWN(addr,PGSIZE),PGSIZE);
	if((r = sys_page_map(envid,(void*)PFTEMP,envid,(void*)ROUNDDOWN(addr,PGSIZE),PTE_P|PTE_U|PTE_W)) < 0)
		panic("map failed:%e",r);
	if((r = sys_page_unmap(envid,(void*)PFTEMP)) < 0)
		panic("unmap failed:%e",r);	
	
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why mark ours copy-on-write again
// if it was already copy-on-write?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr;
	pte_t pte;
	uint32_t perm = 0;
	addr = (void*)(pn*PGSIZE);
	struct Env *env;
	// LAB 4: Your code here.
	pte = vpt[pn];
	
	if(pte & PTE_SHARE)
	{
		pte = pte & PTE_USER;

		if((r = sys_page_map(0,addr,envid,addr,pte)) < 0)
			return r;
		
	}
	else if((pte & PTE_W) || (pte & PTE_COW))
	{
		perm = PTE_U|PTE_P|PTE_COW;
		if((r = sys_page_map(0,addr,envid,addr,perm)) < 0)
			return r;
		if((r = sys_page_map(0,addr,0,addr,perm)) < 0)
			return r;
	}
	else
	{
		pte = pte & PTE_USER;
		if((r = sys_page_map(0,addr,envid,addr,pte)) < 0)
			return r;


	}	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" and the user exception stack in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid,curenvid;
	uint8_t * addr;
	uint32_t pdeno,pteno;
	int r;
	uint32_t pn = 0;
	extern unsigned char end[];
	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	//cprintf("envid:%08x,end:%08x,UTOP:%08x\n",envid,end,UTOP);
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'env'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		//cprintf("child\n");		
		env = &envs[ENVX(sys_getenvid())];
		//cprintf("child pgfault upcall:%08x\n",env->env_pgfault_upcall);
		return 0;
	}
	//cprintf("max pdeno:%08x,end :%08x\n",PDX(UTOP));
	for(pdeno = PDX(0);pdeno < PDX(UTOP);pdeno++)
	{
		if(!(vpd[pdeno] & (PTE_P)))
			continue;
		else
		{
			for(pteno = 0;pteno < NPTENTRIES;pteno++)
			{
				pn = (pdeno<<10) + pteno;
				//cprintf("pn:%08x\n",pn);				
				if(pn < VPN(UXSTACKTOP - PGSIZE))
				{
					if(vpt[pn] & (PTE_P))
					{
						//cprintf("duppage,envid:%08x\n",envid);
						duppage(envid,pn);
					}
				}
				else
					break;
			}
		}
	}
	
	//use sys_for_fork to substitue a batch of syscalls
	//e.g sys_page_alloc sys_env_set_pgfault_upcall and sys_env_set_status
	//alloc page for UXSTACKTOP and set the pgfault_upcall
	//Though I think that set _pgfault_upcall is not necessary
	
	
   	if((r = sys_for_fork(envid, _pgfault_upcall, ENV_RUNNABLE) < 0))
		panic("sys_for_fork error: %e", r);
	return envid;

}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
