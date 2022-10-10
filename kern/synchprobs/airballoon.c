/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 1
#define NROPES 16
static int ropes_left = NROPES;

/* Data structures for rope-hook-stake mappings */
struct rope
{
	struct lock *rope_lock;
	int hook;
	volatile int stake;
	volatile bool rope_severed;
};
// array of ropes that is indexed via stake (ie. array index = stake, array element = rope)
struct rope *stakes;
// eg.
// rope:  3, 1, 4, 2
// stake: 1, 2, 3, 4

// array of ropes that is indexed via hooks (ie. array index = hook, array element = rope). does not get shuffled.
struct rope *hooks;
// eg.
// rope: 1, 2, 3, 4
// hook: 1, 2, 3, 4

/* Synchronization primitives */
// lock for counting ropes
struct lock *ropes_counter_lock;

// lock and cv for threads
struct lock *thread_lock;
struct cv *thread_cv;

// main lock and cv
struct lock *main_lock;
struct cv *main_cv;

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */

// Main cv is used to synchronize the main thread airballoon() and the intermediary thread balloon(). Thread cv is used to synchronize balloon() and the other worker threads (dandelion, marigold, flowerkiller) through dandelion. The main lock and thread lock protect the main cv and thread cv, to ensure that a wake signal for the threads cannot be missed.
// The main thread runs first and sets up the data structures (2 rope arrays) using init(). It starts the worker threads and is put to sleep while balloon() runs. balloon() runs and waits for the worker threads to run. A global counter ropes_left is used to keep track of the number of unsevered ropes left. Each of the worker threads know when to exit based on this count, and will complete when there are no ropes left.
// Each worker thread is synchronized through rope locks. This ensures that each time a rope is mutated on either rope array (switching stakes or getting severed), no other thread can mutate the same rope and cause problems. For the dandelion and marigold threads, each time a rope is severed or unstaked, it is marked in both rope arrays and the ropes_left counter is decremented. The decrementation is protected by a rope_counter_lock.
// When the dandelion thread detects that there are no ropes left, it signals to the balloon() thread, which wakes it up and subsequently signals the main thread to stop the whole process.
// The main thread runs terminate() to deallocate all memory before exiting.

// Function to set up ropes array and required synchronization primitives
static void
init()
{
	// create the 2 rope arrays
	stakes = kmalloc(sizeof(struct rope) * NROPES);
	hooks = kmalloc(sizeof(struct rope) * NROPES);
	for (int i = 0; i < NROPES; i++)
	{
		stakes[i].rope_lock = lock_create("rope lock");
		stakes[i].hook = i;
		stakes[i].stake = i;
		stakes[i].rope_severed = false;

		hooks[i].rope_lock = lock_create("rope lock");
		hooks[i].hook = i;
		hooks[i].stake = i;
		hooks[i].rope_severed = false;
	}

	// create locks and cvs
	ropes_counter_lock = lock_create("ropes counter lock");
	thread_lock = lock_create("thread lock");
	thread_cv = cv_create("thread cv");
	main_lock = lock_create("main lock");
	main_cv = cv_create("main cv");
	big_lock = lock_create("big_lock");
};

// Function to free all allocated memory after main thread is done
static void
terminate()
{

	for (int i = 0; i < NROPES; i++)
	{
		lock_destroy(stakes[i].rope_lock);
		lock_destroy(hooks[i].rope_lock);
	}
	kfree(stakes);
	kfree(hooks);

	// destroy locks and cvs
	lock_destroy(ropes_counter_lock);
	lock_destroy(thread_lock);
	cv_destroy(thread_cv);
	lock_destroy(main_lock);
	cv_destroy(main_cv);
};

static void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	// use dandelion to signal balloon
	lock_acquire(thread_lock);
	kprintf("Dandelion thread starting\n");

	// keep running until no ropes left
	while (ropes_left > 0) {
		int hook = random() % NROPES;
		int stake = hooks[hook].stake;

		if (hooks[hook].rope_severed == false) {
			// make sure no other rope is selected by hook or stake
			lock_acquire(hooks[hook].rope_lock);
			lock_acquire(stakes[stake].rope_lock);

			// indicate rope severed on both arrays
			hooks[hook].rope_severed = true;
			stakes[stake].rope_severed = true;
			kprintf("Dandelion severed rope %d\n", hook);

			// decrement ropes left
			lock_acquire(ropes_counter_lock);
			ropes_left--;
			lock_release(ropes_counter_lock);

			lock_release(hooks[hook].rope_lock);
			lock_release(stakes[stake].rope_lock);
		}

		// switch to another thread
		thread_yield();
	}

	kprintf("Dandelion thread done\n");
	// Signal to balloon that all ropes have been severed
    cv_signal(thread_cv, thread_lock);
    lock_release(thread_lock);
}

static void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");

	// keep running until no ropes left
	while (ropes_left > 0) {
		int stake = random() % NROPES;
		int hook = stakes[stake].hook;

		if (stakes[stake].rope_severed == false) {
			// make sure no other rope is selected by hook or stake
			lock_acquire(stakes[stake].rope_lock);
			lock_acquire(hooks[hook].rope_lock);

			// indicate rope severed on both arrays
			hooks[hook].rope_severed = true;
			stakes[stake].rope_severed = true;
			kprintf("Marigold severed rope %d from stake %d\n", hook, stake);

			// decrement ropes left
			lock_acquire(ropes_counter_lock);
			ropes_left--;
			lock_release(ropes_counter_lock);

			lock_release(hooks[hook].rope_lock);
			lock_release(stakes[stake].rope_lock);
		}

		// switch to another thread
		thread_yield();
	}

    kprintf("Marigold thread done\n");
}

static void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;


	kprintf("Lord FlowerKiller thread starting\n");

	while (ropes_left > 0)
	{

		int old_stake = random() % NROPES;
		int new_stake = random() % NROPES;
		
		lock_acquire(stakes[old_stake].rope_lock);
		lock_acquire(stakes[new_stake].rope_lock);
		

		if (old_stake != new_stake &&
			!stakes[old_stake].rope_severed &&
			!stakes[new_stake].rope_severed)
		{
			
			lock_acquire(hooks[stakes[old_stake].hook].rope_lock);
			lock_acquire(hooks[stakes[new_stake].hook].rope_lock);
			
			int old_hook = stakes[old_stake].hook;
			int new_hook = stakes[new_stake].hook;

			if (!hooks[old_hook].rope_severed &&
				!hooks[new_hook].rope_severed) 
			{
				// update hooks on rope-stakes
				stakes[old_stake].hook = new_hook;
				stakes[new_stake].hook = old_hook;

				// update stakes on rope-hooks
				hooks[old_hook].stake = new_stake;
				hooks[new_hook].stake = old_stake;
			}
			lock_release(hooks[old_hook].rope_lock);
			lock_release(hooks[new_hook].rope_lock);

			kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", old_hook, old_stake, new_stake);
			
		}
		lock_release(stakes[new_stake].rope_lock);
		lock_release(stakes[old_stake].rope_lock);
		
		// switch to another thread
		thread_yield();
	
	}
	kprintf("Lord FlowerKiller thread done\n");
}

static void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	// acquire thread lock
	lock_acquire(thread_lock);

	// acquire main lock
	lock_acquire(main_lock);

	// wait for threads to finish running
	while (ropes_left > 0)
	{
		cv_wait(thread_cv, thread_lock);
	}

	// dandelion thread complete
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	lock_release(thread_lock);

	// signal to wake main thread
	cv_signal(main_cv, main_lock);
	lock_release(main_lock);
}

// Change this function as necessary
int airballoon(int nargs, char **args)
{

	int err = 0;
	int i;

	(void)nargs;
	(void)args;
	(void)ropes_left;

	// reset ropes_left for each run
	ropes_left = NROPES;

	// run init
	init();

	// acquire main lock
	lock_acquire(main_lock);

	// driver code
	err = thread_fork("Marigold Thread",
					  NULL, marigold, NULL, 0);
	if (err)
		goto panic;

	err = thread_fork("Dandelion Thread",
					  NULL, dandelion, NULL, 0);
	if (err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++)
	{
		err = thread_fork("Lord FlowerKiller Thread",
						  NULL, flowerkiller, NULL, 0);
		if (err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
					  NULL, balloon, NULL, 0);
	if (err)
		goto panic;

	// main thread sleeps while waiting for other threads to finish running
	while (ropes_left > 0)
	{
		cv_wait(main_cv, main_lock);
	}
	// signal received from balloon, main thread wakes
	lock_release(main_lock);

	goto done;

panic:
	panic("airballoon: thread_fork failed: %s)\n",
		  strerror(err));

done:
	terminate();
	kprintf("Main thread done\n");
	return 0;
}
