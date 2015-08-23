#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

//initialization of seed
uint32_t seed = 1;


//Park-Miller psudo random number generator
uint32_t
fast_random(uint32_t seed)
{
	return ((long unsigned)seed * 279470273UL) % 4294967291UL;
}

// Choose a user environment to run and run it.
// Using lottery scheduling
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
	//cprintf("begin sched yield,curenv:%08x,&envs[0]:%08x\n",curenv,&envs[0]);
	
	uint32_t i;
	int winning = 1 ;
	if(global_tickets != 0)
	{
		//generate the random winning number
		//always remember to change the seed
		//the quality of the random winning 
		//is the most critical part of lottery scheduling
		while(winning < INIT_TICKET)
		{

			winning = fast_random(seed)% global_tickets;
			//cprintf("winning %08x,seed:%08x\n",winning,seed);
			seed++;
			if(seed > 10000)
				seed = 0;

		}
	}
	else
		cprintf("global ticket = 0\n");
	if(envs[0].env_status == ENV_RUNNABLE)
		winning -= envs[0].tickets;
	for( i = 1; i < NENV; i++)
	{
		//only runnable env has tickets
		if(envs[i].env_status == ENV_RUNNABLE)
		{
			winning -= envs[i].tickets;
			if(winning < 0)
			{
				//adjust the tickets dynamically according to
				//the running of the envs
				if(envs[i].tickets != 1)
				{
					envs[i].tickets /= 2;
					global_tickets -= envs[i].tickets;
				}
				else
				{
					envs[i].tickets = INIT_TICKET;
					global_tickets += (INIT_TICKET - 1);
				}
				
				env_run(&envs[i]);
				return;
			}
		}

	}
	
	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
