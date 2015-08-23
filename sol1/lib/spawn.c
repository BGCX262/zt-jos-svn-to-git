
#include <inc/lib.h>
#include <inc/elf.h>

#define TMPPAGE		(BY2PG)
#define TMPPAGETOP	(TMPPAGE+BY2PG)

// Set up the initial stack page for the new child process with envid 'child',
// using the arguments array pointed to by 'argv',
// which is a null-terminated array of pointers to null-terminated strings.
//
// On success, returns 0 and sets *init_esp
// to the initial stack pointer with which the child should start.
// Returns < 0 on failure.
//
static int
init_stack(u_int child, char **argv, u_int *init_esp)
{
	int argc, i, r, tot;
	char *strings, *p;
	u_int *args;
    u_int tmp;


	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (tot)
	tot = 0;
	for (argc=0; argv[argc]; argc++)
		tot += strlen(argv[argc])+1;

	// Make sure everything will fit in the initial stack page
	if (ROUND(tot, 4)+4*(argc+3) > BY2PG)
		return -E_NO_MEM;

	// Determine where to place the strings and the args array
	strings = (char*)TMPPAGETOP - tot;
	args = (u_int*)(TMPPAGETOP - ROUND(tot, 4) - 4*(argc+1));

	if ((r = sys_mem_alloc(0, TMPPAGE, PTE_P|PTE_U|PTE_W)) < 0)
		return r;

	// Replace this with your code to:
	//
	//	- copy the argument strings into the stack page at 'strings'
    for(i = 0, p = strings; i < argc; i++)
    {
        strcpy(p, argv[i]);
        p += strlen(argv[i]) + 1;
    }
        
	//	- initialize args[0..argc-1] to be pointers to these strings
	//	  that will be valid addresses for the child environment
	//	  (for whom this page will be at USTACKTOP-BY2PG!).
	for(i = 0; i < argc; i++)
    {
        args[i] = USTACKTOP - (TMPPAGETOP - (u_int)strings);
        strings += strlen(argv[i]) + 1;
    }
	//	- set args[argc] to 0 to null-terminate the args array.
	args[argc] = 0;
	//	- push two more words onto the child's stack below 'args',
	//	  containing the argc and argv parameters to be passed
	//	  to the child's umain() function.
	tmp = (u_int)args; 
    args = (u_int *)(tmp - 8);
    args[0] = argc;
    args[1] = USTACKTOP - (TMPPAGETOP - tmp);
	//	- set *init_esp to the initial stack pointer for the child
	//
	//*init_esp = USTACKTOP;	// Change this!
    *init_esp = USTACKTOP - (TMPPAGETOP - (u_int)args);

	if ((r = sys_mem_map(0, TMPPAGE, child, USTACKTOP-BY2PG, PTE_P|PTE_U|PTE_W)) < 0)
		goto error;
	if ((r = sys_mem_unmap(0, TMPPAGE)) < 0)
		goto error;

	return 0;

error:
	sys_mem_unmap(0, TMPPAGE);
	return r;
}

// map one page from va of child's address space to TMPPAGE
static int 
map_tmp_page(int child, u_int va, u_int perm, int *isnew)
{
    int r;
    if((r = sys_mem_map(child, va, 0, TMPPAGE, PTE_P|PTE_U|PTE_W)) < 0)
    {
        // that page hasn't been mapped to child's address space
        // so alloc one
        if((r = sys_mem_alloc(child, va, perm)) < 0)
            return r;
        if(isnew)
            *isnew = 1;
        // and try map again
        // if sys_mem_map fails, return this time
        if((r = sys_mem_map(child, va, 0, TMPPAGE, PTE_P|PTE_U|PTE_W)) < 0)
            return r;
    }
    return 0;
}


int
map_segment(int child, u_int va, u_int memsz, 
	int fd, u_int filesz, u_int fileoffset, u_int perm)
{
    int i, r;
    u_int va_rdown, pageoff, foff, ncopy;
    void *blk;
    int n, isnew;
    va_rdown = ROUNDDOWN(va, BY2PG);
    pageoff = va - va_rdown;
    foff = fileoffset;
    i = 0;
    /*printf("map_segment [%08x] va %08x memsz %08x filesz %08x fileoffset %08x\n",
            child, va, memsz, filesz, fileoffset);*/
    while(i < filesz)
    {
        // Firstly, try to map child's page into my TMPPAGE
        if((r = map_tmp_page(child, va_rdown, perm, &isnew)) < 0)
        {
            sys_mem_unmap(0, TMPPAGE);
            return r;
        }
        ncopy = BY2PG - pageoff; // maybe I will copy a little bit more above filesz
                                 // but I will clear it to zero next
        if((r = read_map(fd, foff, &blk)) < 0)
        {
            sys_mem_unmap(0, TMPPAGE);
            return r;
        }
        memcpy((void *)(TMPPAGE + pageoff), blk, ncopy);
        i += ncopy;
        foff += ncopy;
        va_rdown += pageoff+ncopy;
        pageoff = va_rdown - ROUNDDOWN(va_rdown, BY2PG); //must be 0 except the first time 
        if((r = sys_mem_unmap(0, TMPPAGE)) < 0)
            return r;
    }
    i = filesz;
    va_rdown = ROUNDDOWN(va+filesz, BY2PG);
    pageoff = va + filesz - va_rdown;
    while(i < memsz)
    {
        if((r = map_tmp_page(child, va_rdown, perm, &isnew)) < 0)
        {
            sys_mem_unmap(0, TMPPAGE);
            return r;
        }
        ncopy = BY2PG - pageoff; 
        memset((void *)(TMPPAGE + pageoff), 0, ncopy);
        i += ncopy;
        va_rdown += pageoff+ncopy;
        pageoff = va_rdown - ROUNDDOWN(va_rdown, BY2PG);
        if((r = sys_mem_unmap(0, TMPPAGE)) < 0)
            return r;
    }
    return 0;
    /*u_int i;
    int r;
    void *blk;
    va = ROUNDDOWN(va);
    fileoffset = ROUNDDOWN(fileoff);
    memsz = ROUND(memsz);
    for(i = 0; i < memsz; i += BY2PG)
    {
        if((r = read_map(fd, fileoff, &blk)) < 0)
            return r;
        if((r = sys_mem_map(0, blk, child, va, perm)) < 0)
            return r;
        va += BY2PG;
        fileoff += BY2PG;
    }
    return 0;*/
}


// Spawn a child process from a program image loaded from the file system.
// prog: the pathname of the program to run.
// argv: pointer to null-terminated array of pointers to strings,
// 	 which will be passed to the child as its command-line arguments.
// Returns child envid on success, < 0 on failure.
int
spawn(char *prog, char **argv)
{
	// Insert your code, following approximately this procedure:
	//
	//   - Open the program file and read the elf header (see inc/elf.h).
    struct Elf elf;
    struct Proghdr ph; 
    int fd, child;
    int i, n, j, ret = 0;
    u_int init_sp;
    struct Trapframe tf;
    if((fd = open(prog, O_RDONLY)) < 0)
        return -E_NOT_FOUND;
    if((n = read(fd, &elf, sizeof(elf))) < 0)
    {
        ret = -E_INVAL;
        printf("SPAWN: read %s error %e", prog, n);
        goto out_spawn;
    }
    if(elf.e_magic != 0x464C457F)
    {
        ret = -E_INVAL;
        printf("elf.e_magic = %08x\n", elf.e_magic);
        goto out_spawn;
    }
    /*printf("elf.e_entry = %08x e_phnum = %08x e_phoff = %08x\n", elf.e_entry,
            elf.e_phnum, elf.e_phoff);*/
    
	//   - Use sys_env_alloc() to create a new environment.
	if((ret = sys_env_alloc()) < 0)
        goto out_spawn;
    child = ret;
	//   - Call the init_stack() function above to set up
	//     the initial stack page for the child environment.
	if((ret = init_stack(child, argv, &init_sp)) < 0)
        goto out_spawn;
	//   - Map the program's segments that are of p_type
	//     ELF_PROG_LOAD.  Use read_map() and map the pages it returns
	//     directly into the child so that multiple instances of the
	//     same program will share the same copy of the program text.
	//     Be sure to map the program text read-only in the child.
	//     Read_map is like read but returns a pointer to the data in
	//     *blk rather than copying the data into another buffer.
    if((ret = seek(fd, elf.e_phoff)) < 0)
        goto out_spawn;
	for(i = 0; i < elf.e_phnum; i++)
    {
        u_int perm = PTE_U|PTE_P;
        if((n = read(fd, &ph, sizeof(ph))) < 0)
        {
            ret = -E_INVAL;
            goto out_spawn;
        }
        if(ph.p_type != ELF_PROG_LOAD)
           continue;
        if(!(ph.p_flags&ELF_PROG_FLAG_EXEC))
            perm |= PTE_W;
        if((ret = map_segment(child, ph.p_va, ph.p_memsz, fd, 
                        ph.p_filesz, ph.p_offset, perm)) < 0)
            goto out_spawn;
    }
	//   - Set up the child process's data segment.  For each page,
	//     allocate a page in the parent temporarily at TMPPAGE,
	//     read() the appropriate block of the file into that page,
	//     and then insert that page mapping into the child.
	//     Look at init_stack() for inspiration.
	//     Be sure you understand why you can't use read_map() here.
	//
	//   - Set up the child process's bss segment.
	//     All you need to do here is sys_mem_alloc() the pages
	//     directly into the child's address space, because
	//     sys_mem_alloc() automatically zeroes the pages it allocates.
	//
	//     The bss will start page aligned (since it picks up where the
	//     data segment left off), but it's length may not be a multiple
	//     of the page size, so it may not end on a page boundary.
	//     Be sure to map the last page.  (It's okay to map the whole last page
	//     even though the program will only need part of it.)
	//
	//     The bss is not read from the binary file.  It is simply 
	//     allocated as zeroed memory.
	//
	//     XXX delete this whole section? XXX
	//     The exact location of the bss is a bit confusing, because
	//     the linker lies to the loader about where it is.  
	//     For example, in the copy of user/init that we have (yours
	//     will depend on the size of your implementation of open and close),
	//     i386-jos-elf-nm claims that the bss starts at 0x8067c0
	//     and ends at 0x807f40 (file offsets 0x67c0 to 0x7f40).
	//     However, since this is not page aligned,
	//     it lies to the loader, inserting some extra zeros at the end
	//     of the data section to page-align the end, and then claims
	//     that the data (which starts at 0x2000) is 0x5000 long, ending
	//     at 0x7000, and that the bss is 0xf40 long, making it run from
	//     0x7000 to 0x7f40.  This has the same effect as far as the
	//     loading of the program.  Offsets 0x8067c0 to 0x807f40 
	//     end up being filled with zeros, but they come from different
	//     places -- the ones in the 0x806 page come from the binary file
	//     as part of the data segment, but the ones in the 0x807 page
	//     are just fresh zeroed pages not read from anywhere.
	//
	//   - Use the new sys_set_trapframe() call to set up the
	//     correct initial eip and esp register values in the child.
	//     You can use envs[ENVX(child)].env_tf as a template trapframe
	//     in order to get the initial segment registers and such.
    // printf("map_segment ok, init_sp %08x\n", init_sp);
    memcpy(&tf, &envs[ENVX(child)].env_tf, sizeof(tf));
    tf.tf_eip = elf.e_entry;
    tf.tf_esp = init_sp;
    if((ret = sys_set_trapframe(child, &tf)) < 0)
        goto  out_spawn;
    for(i = 0; i < (UTOP>>PDSHIFT); i++)
    {
        if(!(vpd[i]&PTE_P))
            continue;
        for(j = 0; j < (BY2PG>>2); j++)
        {
            u_int pn, addr;
            pn = (i<<(PDSHIFT - PGSHIFT)) + j;
            if(!(vpt[pn]&PTE_P))
                continue;
            if(!(vpt[pn]&PTE_LIBRARY))
                continue;
            addr = pn << PGSHIFT;
            if((ret = sys_mem_map(0, addr, child, addr, vpt[pn]&PTE_USER)) < 0)
                return ret;
        }
    }

	//   - Start the child process running with sys_set_status().
    if((ret = sys_set_status(child, ENV_RUNNABLE)) < 0)
        goto out_spawn;
    return child;
	//
out_spawn:
    close(fd);
    return ret;
	//panic("spawn unimplemented!");
}

// Spawn, taking command-line arguments array directly on the stack.
int
spawnl(char *prog, char *args, ...)
{
	return spawn(prog, &args);
}

