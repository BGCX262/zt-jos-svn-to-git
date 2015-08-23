// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/pmap.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

struct Command {
	const char *name;
	const char *desc;
	void (*func)(int argc, char **argv);
};

static struct Command commands[] = {
	{"help",	"Display this list of commands", mon_help},
	{"kerninfo",	"Display information about the kernel", mon_kerninfo},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))



/***** Implementations of basic kernel monitor commands *****/

void
mon_help(int argc, char **argv)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		printf("%s - %s\n", commands[i].name, commands[i].desc);
}

void
mon_kerninfo(int argc, char **argv)
{
	extern char _start[], etext[], edata[], end[];

	printf("Special kernel symbols:\n");
	printf("  _start %08x (virt)  %08x (phys)\n", _start, _start-KERNBASE);
	printf("  etext  %08x (virt)  %08x (phys)\n", etext, etext-KERNBASE);
	printf("  edata  %08x (virt)  %08x (phys)\n", edata, edata-KERNBASE);
	printf("  end    %08x (virt)  %08x (phys)\n", end, end-KERNBASE);
	printf("Kernel executable memory footprint: %dKB\n",
		(end-_start+1023)/1024);
}

void
mon_backtrace(int argc, char **argv)
{
	// Your code here.
    extern void spin(void);
    u_long ebp, eip, up_ebp, retaddr;
    u_long arg0, arg1, arg2, arg3, arg4;
    ebp = read_ebp();
    eip = (u_long)mon_backtrace;
    while(1)
    {
        up_ebp = *(u_long *)ebp;
        retaddr = *(u_long *)(ebp + 4);
        arg0 = *(u_long *)(ebp + 8);
        arg1 = *(u_long *)(ebp + 12);
        arg2 = *(u_long *)(ebp + 16);
        arg3 = *(u_long *)(ebp + 20);
        arg4 = *(u_long *)(ebp + 24);
        printf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", 
                ebp, eip, arg0, arg1, arg2, arg3, arg4);
        ebp = up_ebp;
        eip = retaddr;
        if(retaddr == (u_long)spin)
            return; // stop
    }
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static void
runcmd(char *buf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			printf("Too many arguments (max %d)\n", MAXARGS);
			return;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			commands[i].func(argc, argv);
			return;
		}
	}
	printf("Unknown command '%s'\n", argv[0]);
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	printf("Welcome to the JOS kernel monitor!\n");
	printf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			runcmd(buf);
	}
}

