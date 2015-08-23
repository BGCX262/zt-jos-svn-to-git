// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	/*cprintf("[DEBUG] Error code %08x addr %08x\n", err, addr);*/
	if (!(err & FEC_WR))
		panic("pgfault: FEC_WR check error");
	if (!(vpt[VPN(addr)] & PTE_COW))
		panic("pgfault: PTE_COW check error");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	
	// LAB 4: Your code here.
	
	//panic("pgfault not implemented");
	if ((r=sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P)) < 0)
		panic("pgfault: page alloc %e", r);

	memmove(PFTEMP, (void *)ROUNDDOWN(addr, PGSIZE), PGSIZE);

	if((r=sys_page_map(0, PFTEMP, 0, (void *)ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_W|PTE_U)) < 0)
		panic("pgfault: page map %e", r);
	if ((r=sys_page_unmap(0, PFTEMP)) < 0)
		panic("pgfault: page umap %e", r);
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

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	pte = vpt[pn];
	addr = (void *) (pn << PGSHIFT);

	if ((pte & PTE_W) || (pte & PTE_COW)) {
		if ((r=sys_page_map(0, addr, envid, addr, PTE_U|PTE_P|PTE_COW)) < 0)
			return r;
		if ((r=sys_page_map(0, addr, 0, addr, PTE_U|PTE_P|PTE_COW)) < 0)
			return r;
	} else {
		//////////////////////////////////////////////////////////
		//				Why reach here
		cprintf("[DEBUG] Reach here %08x\n", pte);
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
	//panic("fork not implemented");
	envid_t curenvid, newenvid;
	uint32_t pdex, ptex, pn;
	extern unsigned char end[];
	int r;
	extern void _pgfault_upcall(void);
	
	set_pgfault_handler(pgfault);

	if ((newenvid = sys_exofork()) < 0) 
		panic("fork: sys_exofork %e", newenvid);

	if (newenvid == 0) {
		env = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	
	for (pdex = PDX(UTEXT); pdex <= PDX(USTACKTOP); pdex++) {
		if (vpd[pdex] & (PTE_P)) {
			for (ptex = 0; ptex < NPTENTRIES; ptex++) {
				pn = (pdex<<10) + ptex;
				if((pn<VPN(UXSTACKTOP-PGSIZE))&&(vpt[pn]&PTE_P)) {
						duppage(newenvid, pn);
				}
			}
		}
	}

	/*for (pn = PPN(UTEXT); pn < PPN((uintptr_t)end); pn++) */
		/*if ((vpd[pn>>10]&PTE_P) && (vpt[pn]&PTE_P))*/
			/*duppage(newenvid, pn);*/

	/*pn = PPN(USTACKTOP-PGSIZE);*/
	/*while ((vpd[pn>>10]&PTE_P) && (vpt[pn]&PTE_P)) {*/
		/*duppage(newenvid, pn);*/
		/*--pn;*/
	/*}*/


	if ((r=sys_page_alloc(newenvid, (void *) (UXSTACKTOP-PGSIZE), PTE_P|PTE_W|PTE_U)) < 0)
		return r;
	if ((r=sys_page_map(newenvid, (void *) (UXSTACKTOP-PGSIZE), 
					0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		return r;
	memmove(UTEMP, (void *) (UXSTACKTOP-PGSIZE), PGSIZE);
	if ((r=sys_page_unmap(0, UTEMP)) < 0) 
		return r;
	if ((r=sys_env_set_pgfault_upcall(newenvid, _pgfault_upcall)) < 0)
		return r;
	if ((r=sys_env_set_status(newenvid, ENV_RUNNABLE)) < 0)
		return r;

	return newenvid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
