/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 8
#define NROPES 16
static int ropes_left = NROPES;
static int threadsLeft = 0;
static int threadsSleep = 0;

/* Data structures for rope mappings */
struct stakes{
    struct ropes *ropeConnected; //The rope that this stake is connected to
    struct lock *stakeLock; //Lock that locks the stake so only marigold or one of the flowerkillers can hold a stake at a time
    volatile bool isMapped; //Checks if this stake is still attached to a hook
    volatile bool isHeld; //Checks if someone is holding this stake
};


struct hooks{
    struct ropes *ropeConnected; //The rope that this hook is connected to
    volatile bool isMapped; //Checks if this hook is still attached to a stake
    
};

struct ropes{
    struct lock *ropeLock; //The lock that holds this rope
    volatile int hookIndex; //The index of the hook that connects this rope
    volatile int stakeIndex; //The index of the stake that connects this rope
    volatile int ropeIndex; //The index of this rope
    volatile bool isCut; //Check if the rope is cut
};

/* Implement this! */
//Created a dynamic array for each of the data structures. Marigold only has access to stakes same as Lord FlowerKiller, and Dandelion can only access hooks. Ropes are indirectly accessed.
struct stakes *stakeArr;
struct hooks *hookArr;
struct ropes *ropeArr;

/* Synchronization primitives */
struct lock *ropeCount; //Lock used to adjust the number of ropes
struct lock *threadLock; //Lock used to lock threads so threadsLeft variable can be adjusted. Also used to sleep threads.
struct lock *debugLock; //USED FOR DEBUGGING ONLY
struct cv *sleepCV; //CV used to sleep threads, and wake up upon all ropes being cut.


/* Implement this! */

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 * 
 * 
 */
 
 //All threads know that they're done when the ropes left are zero, since the goal of dandelion and marigold is to remove the threads, and Lord Flowerkiller just to delay them as long as
 //he can. My design is I have 3 data structures that keep track of the relationship/connections between stakes, ropes, and hooks. I use locks to assign stakes and ropes as resources, so that 
 //it is guarenteed that threads can complete their actions without race condition/deadlock. I assigned locks to stakes since Marigold and the many Lord FlowerKiller copies will try to 
 //access stakes at times when they shouldn't. All the threads are sleeping after they finish, so that the exit messages don't print before they are done. 
 //The main thread is yielding until every thread finishes, at which point it wakes every thread up to print their finish messages.

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Dandelion thread starting\n");

    thread_yield();
    
    //Increase the thread count
    lock_acquire(threadLock);
    threadsLeft++;
    lock_release(threadLock);   
       
	/* Implement this function */
	while(ropes_left > 0){
	    int nextRope = random() % NROPES;
	   
	    
	    
	    if (hookArr[nextRope].isMapped){
	    
	        //If the rope has already been cut, find another rope
	        if (hookArr[nextRope].ropeConnected->isCut){
	            continue;
	        }
	        
	        //Acquire the lock for the hook in question.
	        lock_acquire(hookArr[nextRope].ropeConnected->ropeLock);
	        
	        
	        //Incase the rope has been cut from the other end, release the lock that was acquired and just continue to find another rope
	        if (hookArr[nextRope].ropeConnected->isCut){
	            lock_release(hookArr[nextRope].ropeConnected->ropeLock);
	            continue;
	        }
	        
	        //Cuts the rope once lock has been acquired and we know the rope has not been cut
	        hookArr[nextRope].ropeConnected->isCut = true;
	        hookArr[nextRope].isMapped = false;
	        
	        //Print the severed rope message
	        kprintf("Dandelion severed rope %d\n",hookArr[nextRope].ropeConnected->ropeIndex);
	        
	        //Decrement the rope count
	        lock_acquire(ropeCount);
	        ropes_left--;
	        lock_release(ropeCount);
	        
	        
	        //Release the lock so that other threads may acquire it
	        lock_release(hookArr[nextRope].ropeConnected->ropeLock);
	        
	        
	        thread_yield();
	        
	    }
	}
	
    
	//Sleep the thread, decrement thread count and print Dandelion exit statement when it wakes up.
	lock_acquire(threadLock);
	threadsSleep++;
	cv_wait(sleepCV, threadLock);
	kprintf("Dandelion thread done\n");
	threadsLeft--;
	lock_release(threadLock);
	
	//End this thread
	thread_exit();
}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");
	
	thread_yield();
	
	//Increase the thread count
    lock_acquire(threadLock);
    threadsLeft++;
    lock_release(threadLock);

	/* Implement this function */
	while(ropes_left > 0){
	
	    //Generate a random rope to remove
	    int nextRope = random() % NROPES;
	    
	    
	    //If the stake is mapped and the stake is not currently held by others, acquire the stake lock
	    if (stakeArr[nextRope].isMapped && stakeArr[nextRope].isHeld == false){
	        
	        
	        //Acquire the lock to lock this stake
	        lock_acquire(stakeArr[nextRope].stakeLock);
	        stakeArr[nextRope].isHeld = true;
	        
	        //If the rope has already been cut, find another rope
	        if (stakeArr[nextRope].ropeConnected->isCut){
	            lock_release(stakeArr[nextRope].stakeLock);
	            stakeArr[nextRope].isHeld = false;
	            continue;
	        }
	        
	        //Acquire the lock to the rope in question
	        lock_acquire(stakeArr[nextRope].ropeConnected->ropeLock);
	        
	        
	        //Incase the rope has been cut from the other end, just continue to find another rope
	        if (stakeArr[nextRope].ropeConnected->isCut){
	            lock_release(stakeArr[nextRope].ropeConnected->ropeLock);
	            lock_release(stakeArr[nextRope].stakeLock);
	            stakeArr[nextRope].isHeld = false;
	            continue;
	        }
	        
	        //Cuts the rope
	        stakeArr[nextRope].ropeConnected->isCut = true;
	        stakeArr[nextRope].isMapped = false;
	        
	        //Prints severed rope message
	        kprintf("Marigold severed rope %d from stake %d\n",stakeArr[nextRope].ropeConnected->ropeIndex, stakeArr[nextRope].ropeConnected->stakeIndex);
	        
	        //Decrement the rope count
	        lock_acquire(ropeCount);
	        ropes_left--;
	        lock_release(ropeCount);
	        
	        //Release the rope lock, and then the stake lock
	        lock_release(stakeArr[nextRope].ropeConnected->ropeLock);
	        
	        lock_release(stakeArr[nextRope].stakeLock);
	        
	        //Set the variable of the stake that is being held to be false
	        stakeArr[nextRope].isHeld = false;
	        
	        thread_yield();
	        
	    }
	}
	
	
	//Sleep the thread, decrement thread count and print Marigold exit statement when it wakes up.
	lock_acquire(threadLock);
	threadsSleep++;
	cv_wait(sleepCV, threadLock);
	kprintf("Marigold thread done\n");
	threadsLeft--;
	lock_release(threadLock);
	
	
	//End this thread
	thread_exit();
}




static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Lord FlowerKiller thread starting\n");
	
	thread_yield();
	
	//Increase the thread count
    lock_acquire(threadLock);
    threadsLeft++;
    lock_release(threadLock);

    
	/* Implement this function */
	while(ropes_left > 1){
	    
	    //Select two random stakes
	    int nextRope1 = random() % NROPES;
	    int nextRope2 = random() % NROPES;
	    
	    //If stakes are mapped and it is not trying to swap itself
	    if (stakeArr[nextRope1].isMapped && stakeArr[nextRope2].isMapped && nextRope1 != nextRope2){
	        
	        //If the stake is not held
	        if (!stakeArr[nextRope1].isHeld){
	        
	            //Acquire the stake lock and set its status to held
	            lock_acquire(stakeArr[nextRope1].stakeLock);
	            stakeArr[nextRope1].isHeld = true;
	            
	            //If the second stake is being held, release the acquired lock and reset to the top of the loop
	            if (stakeArr[nextRope2].isHeld){      
	                lock_release(stakeArr[nextRope1].stakeLock);
	                stakeArr[nextRope2].isHeld = false;
	                continue;
	            }
	            else { //Otherwise, acquire the second stake lock
	                lock_acquire(stakeArr[nextRope2].stakeLock);
	                stakeArr[nextRope2].isHeld = true;
	            }
	            
	        }
	        else{ //If the first stake is being held, reset to the top of the loop
	            continue;
	        }
	        
	        //If either of the ropes have already been cut, release the locks and find another pair of ropes
	        if (stakeArr[nextRope1].ropeConnected->isCut || stakeArr[nextRope2].ropeConnected->isCut){
	            lock_release(stakeArr[nextRope1].stakeLock);
	            stakeArr[nextRope1].isHeld = false;
	            lock_release(stakeArr[nextRope2].stakeLock);
	            stakeArr[nextRope2].isHeld = false;
	            continue;
	        }
	        
	        //If all conditions are met, acquire the lock for the first rope
	        lock_acquire(stakeArr[nextRope1].ropeConnected->ropeLock);
	        
	        
	        //If the rope has already been cut or two ropes are the same, release all the locks and find another rope
	        if (stakeArr[nextRope1].ropeConnected->isCut || stakeArr[nextRope2].ropeConnected->isCut){
	            lock_release(stakeArr[nextRope1].ropeConnected->ropeLock);
	            
	            lock_release(stakeArr[nextRope1].stakeLock);
	            stakeArr[nextRope1].isHeld = false;
	            lock_release(stakeArr[nextRope2].stakeLock);
	            stakeArr[nextRope2].isHeld = false;
	            
	            continue;
	        }
	        
	        //If the ropes are not cut, then acquire the second lock
	        if (!stakeArr[nextRope1].ropeConnected->isCut && !stakeArr[nextRope2].ropeConnected->isCut){
	            lock_acquire(stakeArr[nextRope2].ropeConnected->ropeLock);
	            
	        }else{ //Otherwise, release all the locks and reset to the top of the loop
	            lock_release(stakeArr[nextRope1].ropeConnected->ropeLock);
	            
	            lock_release(stakeArr[nextRope1].stakeLock);
	            stakeArr[nextRope1].isHeld = false;
	            lock_release(stakeArr[nextRope2].stakeLock);
	            stakeArr[nextRope2].isHeld = false;
	             
	            continue;
	        }
	        
	        //Reassign the stake index for the ropes
	        int tempIndex = stakeArr[nextRope1].ropeConnected->stakeIndex;
	        stakeArr[nextRope1].ropeConnected->stakeIndex = stakeArr[nextRope2].ropeConnected->stakeIndex;
	        stakeArr[nextRope2].ropeConnected->stakeIndex = tempIndex;
	        
	        //Switch the stakes by switching the pointers to the ropes from the stake objects
	        struct ropes *tempRope = stakeArr[nextRope1].ropeConnected;
	        stakeArr[nextRope1].ropeConnected = stakeArr[nextRope2].ropeConnected;
	        stakeArr[nextRope2].ropeConnected = tempRope;
	        
	        
	        //Prints the switch rope statements
	        kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n",stakeArr[nextRope1].ropeConnected->ropeIndex, stakeArr[nextRope2].ropeConnected->stakeIndex, stakeArr[nextRope1].ropeConnected->stakeIndex);
	        kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n",stakeArr[nextRope2].ropeConnected->ropeIndex, stakeArr[nextRope1].ropeConnected->stakeIndex, stakeArr[nextRope2].ropeConnected->stakeIndex);
	           
	        //Release the locks held from both ropes 
	        lock_release(stakeArr[nextRope1].ropeConnected->ropeLock);
	        lock_release(stakeArr[nextRope2].ropeConnected->ropeLock);
	        
	        //Release the stake locks
	        lock_release(stakeArr[nextRope2].stakeLock);
	        stakeArr[nextRope2].isHeld = false;
	        lock_release(stakeArr[nextRope1].stakeLock);
	        stakeArr[nextRope1].isHeld = false;
	        
	        thread_yield();
	        
	    }
	    
	}
	
	
	//Sleep the Lord FlowerKiller as it either finishes or can no longer finish (less than 2 ropes where he can hold both locks) his tasks
	lock_acquire(threadLock);
	threadsSleep++;
	cv_wait(sleepCV, threadLock);
	kprintf("Lord FlowerKiller thread done\n");
	threadsLeft--;
	lock_release(threadLock);
	
	//End this thread
	thread_exit();
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");
	
	//Increase the thread count
    lock_acquire(threadLock);
    threadsLeft++;
    lock_release(threadLock);


	
	//Sleep until eventually all ropes are severed, then decrement thread count and print Balloon exit statement
	lock_acquire(threadLock);
	threadsSleep++;
	cv_wait(sleepCV, threadLock);
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	threadsLeft--;
	lock_release(threadLock);
	
	
	//End this thread
	thread_exit();
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{

	int err = 0, i;
	
	//Initiate variable totalSleepThreads, which keeps track of how many total threads should be asleep for them to all be woken up to print the finish statements
	int totalSleepThreads;
    totalSleepThreads = 3 + N_LORD_FLOWERKILLER; //Balloon thread, Marigold, Dandelion and all the Lord FlowerKiller threads

	(void)nargs;
	(void)args;
	
	
	//Add the first thread, this main thread, to the thread count.
	threadsLeft++;
	
	
	//Initialize all the locks needed plus the cv we will use to sleep threads
	ropeCount = lock_create("");
	threadLock = lock_create("");
	debugLock = lock_create("");
	sleepCV = cv_create("");
	
	//Initiate the ropeArr array, which contains NROPES ropes objects, where each ropes object represents a rope.
	ropeArr = kmalloc(sizeof(struct ropes) * NROPES); 
    for (int i = 0; i < NROPES; i++){
        ropeArr[i].ropeLock = lock_create("");
        ropeArr[i].hookIndex = i;
        ropeArr[i].stakeIndex = i;
        ropeArr[i].ropeIndex = i;
        ropeArr[i].isCut = false;
    
    }
    
    
    //Initiate the hookArr and stakeArr, which contains hook and stake objects
    hookArr = kmalloc(sizeof(struct hooks) * NROPES); 
    stakeArr = kmalloc(sizeof(struct stakes) * NROPES);
    for (int i = 0; i < NROPES; i++){
        
        hookArr[i].ropeConnected = &ropeArr[i];
        hookArr[i].isMapped = true;
        
        stakeArr[i].stakeLock = lock_create("");
        stakeArr[i].ropeConnected = &ropeArr[i];
        stakeArr[i].isMapped = true;
        stakeArr[i].isHeld = false;
    
    }   
    
    
	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:
    
    //Wait until all other threads are sleeping and there are no more ropes left. Otherwise, the main thread yields.
    while (threadsSleep < totalSleepThreads || ropes_left > 0){
        thread_yield();
    }
    
    //Wake up every thread so the finish statements can be printed
    lock_acquire(threadLock);
    cv_broadcast(sleepCV, threadLock);
    lock_release(threadLock);
    
    //Wait until all other threads finish
    while(threadsLeft > 1){
        thread_yield();
    }
    
    //Reset global variables so they can be called again
    ropes_left = NROPES;
    threadsLeft = 0;
    threadsSleep = 0;
    
    //Free up memory so we don't create memory leaks
    lock_destroy(ropeCount);
    lock_destroy(threadLock);
    lock_destroy(debugLock);
    cv_destroy(sleepCV);
    
    //Destroy all the allocated locks in ropes and stakes
    for (int i = 0; i < NROPES; i++){
        lock_destroy(ropeArr[i].ropeLock);
        lock_destroy(stakeArr[i].stakeLock);
    }
    
    //Free the allocated space for the arrays
    kfree(ropeArr);
    kfree(hookArr);
    kfree(stakeArr);
    
    
    //Print the exit statement for the main thread
    kprintf("Main thread done\n");
    
	return 0;
}
