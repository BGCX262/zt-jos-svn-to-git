// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, vpd, and vpt.

#include <inc/lib.h>
extern void umain(int argc, char **argv);

volatile struct Env *env;
char *binaryname = "(PROGRAM NAME UNKNOWN)";

void
libmain(int argc, char **argv)
{
	// set env to point at our env structure in envs[].
	// LAB 3: Your code here.
  	envid_t eid = sys_getenvid();
	//cprintf("eid == %08x\n",eid);
	env = &envs[ENVX(eid)];
	//cprintf("eid:%08x,env:%08x\n",eid,env);
	//env = &envs[00000000];
	// save the name of the program so that panic() can use it
	if (argc > 0)
	{
		binaryname = argv[0];
		//cprintf("%s\n",binaryname);
	}

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

