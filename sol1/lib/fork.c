// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>


#define debug 0

#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//

static void
pgfault(u_int va, u_int err)
{
	int r;
	u_char *tmp;

	// Your code here.
    // printf("pgfault va %08x, err %08x\n", va, err);
    if(va > UTOP)
        panic("Invalid va %08x\n", va);
    if(!(err&0x2))
        panic("This handler only handle cow fault\n");
    if(!(err&0x1))
        panic("This page of va %08x is not even present\n");
    va = ROUNDDOWN(va, BY2PG);
    // printf("Before [%08x]vpt[%08x>>PGSHIFT] = %08x\n", sys_getenvid(), va, vpt[va>>PGSHIFT]);
    // First map this phyical page to a tmp address
    tmp = (u_char *)(UTEXT - BY2PG);
    if((r = sys_mem_map(0, va, 0, (u_int)tmp, PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_mem_map: %e\n", r);
    // then ummap the original page
    if((r = sys_mem_unmap(0, va)) < 0)
        panic("sys_mem_unmap: %e\n", r);
    // And then alloc a new page 
    if((r = sys_mem_alloc(0, va, PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_mem_alloc: %e", r);
    // Copy the content of the page back
    memcpy((void *)va, tmp, BY2PG);
    // Unmap the tmp page
    if((r = sys_mem_unmap(0, (u_int)tmp)) < 0)
        panic("sys_mem_unmap: %e", r);
    // printf("After  [%08x]vpt[%08x>>PGSHIFT] = %08x\n", sys_getenvid(), va, vpt[va>>PGSHIFT]);
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*BY2PG) into the target envid
// at the same virtual address.  if the page is writable or copy-on-write,
// the new mapping must be created copy on write and then our mapping must be
// marked copy on write as well.  (Exercise: why mark ours copy-on-write again if
// it was already copy-on-write?)
// 
static void
duppage(u_int envid, u_int pn)
{
	int r;
	u_int addr;

	// Your code here.
    addr = pn << PGSHIFT;
    if(vpt[pn]&PTE_LIBRARY)
    {
        u_int pte_flags = vpt[pn]&PTE_USER;
        if((r = sys_mem_map(0, addr, envid, addr, pte_flags)) < 0)
            panic("sys_mem_map: %e\n", r);
        return;
    }
    if((r = sys_mem_map(0, addr, envid, addr, PTE_COW|PTE_U)) < 0)
        panic("sys_mem_map: %e\n", r);
    if((r = sys_mem_map(0, addr, 0, addr, PTE_COW|PTE_U)) < 0)
        panic("sys_mem_map: %e\n", r);
}

//
// User-level fork.  Create a child and then copy our address space
// and page fault handler setup to the child.
//
// Hint: use vpd, vpt, and duppage.
// Hint: remember to fix "env" in the child process!
// 
int
fork(void)
{
	// Your code here.
	int envid, r;
    u_int i, j, pn;
    extern void _pgfault_entry(void);

    set_pgfault_handler(pgfault);
    envid = sys_env_alloc();
    if(envid < 0)
        panic("sys_env_fork: %e", envid);
    if(envid == 0)
    {
        env = &envs[ENVX(sys_getenvid())];
        return 0;
    }
    for(i = 0; i < (UTOP>>PDSHIFT); i++)
    {
        if(!(vpd[i]&PTE_P))
            continue;
        // This page table contain some pages
        for(j = 0; j < (BY2PG>>2); j++)
        {
            pn = (i<<(PDSHIFT - PGSHIFT)) + j;
            if(pn == ((UXSTACKTOP - BY2PG) >> PGSHIFT))
                continue; // leave the UXSTACKTOP alone
            if(!(vpt[pn]&PTE_P))
                continue;
            duppage(envid, pn);
        }
    }
    if ((r = sys_mem_alloc(envid, UXSTACKTOP-BY2PG, PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_mem_alloc: %e", r);
    if ((r = sys_set_pgfault_entry(envid, (u_int)_pgfault_entry)) < 0)
        panic("sys_set_pgfault_entry : %e", r);
	if ((r=sys_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_set_status: %e", r);

	return envid;

	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
