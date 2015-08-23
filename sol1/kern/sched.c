
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Your code here to implement simple round-robin scheduling.
	// Search through envs array for a runnable environment,
	// in circular fashion starting from the previously running env,
	// and switch to the first such environment found.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.
	// Run the special idle environment when nothing else is runnable.
    int i, pre, stop;
    if(curenv == 0)
        pre = 0;
    else
        pre = ENVX(curenv->env_id);
    for(i = (pre + 1)%NENV; i != pre; i = (i+1)%NENV)
    {
        if(i == 0)
            continue;
        if(envs[i].env_status == ENV_RUNNABLE)
            env_run(&envs[i]);
    }
    if(i != 0 && envs[i].env_status == ENV_RUNNABLE)
        env_run(&envs[i]);
	assert(envs[0].env_status == ENV_RUNNABLE);
	env_run(&envs[0]);
}

