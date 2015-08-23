/* See COPYRIGHT for copyright information. */

#include <inc/asm.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>


void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else,
	// clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end-edata);

	// Initialize the console.
	// Can't call printf until after we do this!
	cons_init();

	printf("6828 decimal is %o octal!\n", 6828);

	// Lab 2 memory management initialization functions
	i386_detect_memory();
	i386_vm_init();
	page_init();
	page_check();

	// Lab 3 user environment initialization functions
	env_init();
	idt_init();

	// Lab 4 multitasking initialization functions
	pic_init();
	kclock_init();

	// ENV_CREATE(user_forktree);
	// Should always have an idle process as first one.
	ENV_CREATE(user_idle);

	// Start fs.
	ENV_CREATE(fs_fs);
	//ENV_CREATE(user_testfsipc);
	//ENV_CREATE(user_writemotd);
	/*ENV_CREATE(user_icode);*/

	// Start init
#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE2(TEST, TESTSIZE);
#else
	// Touch all you want.
	// ENV_CREATE(user_writemotd);
	// ENV_CREATE(user_testfsipc);
	// ENV_CREATE(user_testfdsharing);
    // ENV_CREATE(user_testpipe);
    // ENV_CREATE(user_testpiperace);
    // ENV_CREATE(user_testpiperace2);
    //ENV_CREATE(user_testptelibrary);
    ENV_CREATE(user_testshell);
	// ENV_CREATE(user_pipereadeof);
	// ENV_CREATE(user_pipewriteeof);
#endif
	kbd_intr();


	// Schedule and run the first user environment!
	sched_yield();


	// Drop into the kernel monitor.
	while (1)
		monitor(NULL);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
static const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	va_start(ap, fmt);
	printf("kernel panic at %s:%d: ", file, line);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	printf("kernel warning at %s:%d: ", file, line);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

