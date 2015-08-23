// Ping-pong a counter between two processes.
// Only need to start one of these -- splits into two with fork.

#include <inc/lib.h>

void
umain(void)
{
	envid_t who;

	if ((who = fork()) != 0) {
		// get the ball rolling
		cprintf("send 0 from %x to %x\n", sys_getenvid(), who);
		ipc_send(who, 0, 0, 0);
		//cprintf("%x send complete\n",sys_getenvid());

	}

	while (1) {
		//cprintf("set %x recv\n",sys_getenvid());
		uint32_t i = ipc_recv(&who, 0, 0);
		cprintf("%x got %d from %x\n", sys_getenvid(), i, who);
		if (i == 10)
			return;
		i++;
		//cprintf("send %d from %x to %x\n", i,sys_getenvid(), who);
		ipc_send(who, i, 0, 0);
		if (i == 10)
			return;
	}
		
}

